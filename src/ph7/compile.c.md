# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5249/6568 lines (79.92%)

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
|       - |    97 | `/* Forward declaration */` |
|       - |    98 | `static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));` |
|       - |    99 | `/*` |
|       - |   100 | ` * Local utility routines used in the code generation phase.` |
|       - |   101 | ` */` |
|       - |   102 | `/*` |
|       - |   103 | ` * Check if the given name refer to a valid label.` |
|       - |   104 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|       - |   105 | ` * Any other return value indicates no such label.` |
|       - |   106 | ` */` |
|     148 |   107 | `static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)` |
|       5 |   108 |  |
|       - |   109 | `	Label *aLabel;` |
|       - |   110 | `	sxu32 n;` |
|       - |   111 | `	/* Perform a linear scan on the label table */` |
|     153 |   112 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|     333 |   113 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     277 |   114 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|       - |   115 | `			/* Jump destination found */` |
|      96 |   116 | `			aLabel[n].bRef = TRUE;` |
|      96 |   117 | `			if( ppOut ){` |
|      96 |   118 | `				*ppOut = &aLabel[n];` |
|      46 |   119 | `			}` |
|      96 |   120 | `			return SXRET_OK;` |
|       - |   121 | `		}` |
|      93 |   122 | `	}` |
|       - |   123 | `	/* No such destination */` |
|      60 |   124 | `	return SXERR_NOTFOUND;` |
|      79 |   125 |  |
|       - |   126 | `/*` |
|       - |   127 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|       - |   128 | ` * compiled blocks.` |
|       - |   129 | ` * Return a pointer to that block on success. NULL otherwise.` |
|       - |   130 | ` */` |
|    3426 |   131 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   132 |  |
|    3431 |   133 | `	GenBlock *pBlock = pCurrent;` |
|    9695 |   134 | `	for(;;){` |
|   19395 |   135 | `		if( pBlock->iFlags & iBlockType ){` |
|    3323 |   136 | `			iCount--; /* Decrement nesting level */` |
|    3323 |   137 | `			if( iCount < 1 ){` |
|       - |   138 | `				/* Block meet with the desired criteria */` |
|    3297 |   139 | `				return pBlock;` |
|       - |   140 | `			}` |
|      13 |   141 | `		}` |
|       - |   142 | `		/* Point to the upper block */` |
|   16103 |   143 | `		pBlock = pBlock->pParent;` |
|   16103 |   144 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   145 | `			/* Forbidden */` |
|      72 |   146 | `			break;` |
|       - |   147 | `		}` |
|       5 |   148 | `	}` |
|       - |   149 | `	/* No such block */` |
|     139 |   150 | `	return 0;` |
|    1718 |   151 |  |
|       - |   152 | `/*` |
|       - |   153 | ` * Initialize a freshly allocated block instance.` |
|       - |   154 | ` */` |
|  743130 |   155 | `static void GenStateInitBlock(` |
|       - |   156 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   157 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   158 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   159 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   160 | `	void *pUserData      /* Upper layer private data */` |
|       - |   161 | `	)` |
|       5 |   162 |  |
|       - |   163 | `	/* Initialize block fields */` |
|  743135 |   164 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  743135 |   165 | `	pBlock->pUserData   = pUserData;` |
|  743135 |   166 | `	pBlock->pGen        = pGen;` |
|  743135 |   167 | `	pBlock->iFlags      = iType;` |
|  743135 |   168 | `	pBlock->pParent     = 0;` |
|  743135 |   169 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  743135 |   170 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  743135 |   171 |  |
|       - |   172 | `/*` |
|       - |   173 | ` * Allocate a new block instance.` |
|       - |   174 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   175 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   176 | ` * processing on failure.` |
|       - |   177 | ` */` |
|  739982 |   178 | `static sxi32 GenStateEnterBlock(` |
|       - |   179 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   180 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   181 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   182 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   183 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   184 | `	)` |
|       5 |   185 |  |
|       - |   186 | `	GenBlock *pBlock;` |
|       - |   187 | `	/* Allocate a new block instance */` |
|  739987 |   188 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  739987 |   189 | `	if( pBlock == 0 ){` |
|       - |   190 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   191 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   192 | `		 */` |
|     ! 0 |   193 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   194 | `		/* Abort processing immediately */` |
|     ! 0 |   195 | `		return SXERR_ABORT;` |
|       - |   196 | `	}` |
|       - |   197 | `	/* Zero the structure */` |
|  739987 |   198 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  739987 |   199 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   200 | `	/* Link to the parent block */` |
|  739987 |   201 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   202 | `	/* Mark as the current block */` |
|  739987 |   203 | `	pGen->pCurrent = pBlock;` |
|  739987 |   204 | `	if( ppBlock ){` |
|       - |   205 | `		/* Write a pointer to the new instance */` |
|  359425 |   206 | `		*ppBlock = pBlock;` |
|  179710 |   207 | `	}` |
|  739987 |   208 | `	return SXRET_OK;` |
|  369996 |   209 |  |
|       - |   210 | `/*` |
|       - |   211 | ` * Release block fields without freeing the whole instance.` |
|       - |   212 | ` */` |
|  739974 |   213 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   214 |  |
|  739979 |   215 | `	SySetRelease(&pBlock->aPostContFix);` |
|  739979 |   216 | `	SySetRelease(&pBlock->aJumpFix);` |
|  739979 |   217 |  |
|       - |   218 | `/*` |
|       - |   219 | ` * Release a block.` |
|       - |   220 | ` */` |
|  739974 |   221 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   222 |  |
|  739979 |   223 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  739979 |   224 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   225 | `	/* Free the instance */` |
|  739979 |   226 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  739979 |   227 |  |
|       - |   228 | `/*` |
|       - |   229 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   230 | ` */` |
|  739974 |   231 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   232 |  |
|  739979 |   233 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  739979 |   234 | `	if( pBlock == 0 ){` |
|       - |   235 | `		/* No more block to pop */` |
|     ! 0 |   236 | `		return SXERR_EMPTY;` |
|       - |   237 | `	}` |
|       - |   238 | `	/* Point to the upper block */` |
|  739979 |   239 | `	pGen->pCurrent = pBlock->pParent;` |
|  739979 |   240 | `	if( ppBlock ){` |
|       - |   241 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   242 | `		*ppBlock = pBlock;` |
|     ! 0 |   243 | `	}else{` |
|       - |   244 | `		/* Safely release the block */` |
|  739979 |   245 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   246 | `	}` |
|  739979 |   247 | `	return SXRET_OK;` |
|  369992 |   248 |  |
|       - |   249 | `/*` |
|       - |   250 | ` * Emit a forward jump.` |
|       - |   251 | ` * Notes on forward jumps` |
|       - |   252 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|       - |   253 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |   254 | ` *  generation of forward jumps.` |
|       - |   255 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|       - |   256 | ` *  are emitted, we record each forward jump in an instance of the following` |
|       - |   257 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|       - |   258 | ` */` |
|  210122 |   259 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   260 |  |
|       - |   261 | `	JumpFixup sJumpFix;` |
|       - |   262 | `	sxi32 rc;` |
|       - |   263 | `	/* Init the JumpFixup structure */` |
|  210127 |   264 | `	sJumpFix.nJumpType = nJumpType;` |
|  210127 |   265 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   266 | `	/* Insert in the jump fixup table */` |
|  210127 |   267 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  210127 |   268 | `	return rc;` |
|       5 |   269 |  |
|       - |   270 | `/*` |
|       - |   271 | ` * Fix a forward jump now the jump destination is resolved.` |
|       - |   272 | ` * Return the total number of fixed jumps.` |
|       - |   273 | ` * Notes on forward jumps:` |
|       - |   274 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|       - |   275 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |   276 | ` *  generation of forward jumps.` |
|       - |   277 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|       - |   278 | ` *  are emitted, we record each forward jump in an instance of the following` |
|       - |   279 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|       - |   280 | ` */` |
|  517622 |   281 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   282 |  |
|       - |   283 | `	JumpFixup *aFix;` |
|       - |   284 | `	VmInstr *pInstr;` |
|       - |   285 | `	sxu32 nFixed;` |
|       - |   286 | `	sxu32 n;` |
|       - |   287 | `	/* Point to the jump fixup table */` |
|  517627 |   288 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   289 | `	/* Fix the desired jumps */` |
|  931661 |   290 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  414039 |   291 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   292 | `			/* Already fixed */` |
|  165335 |   293 | `			continue;` |
|       - |   294 | `		}` |
|  248709 |   295 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   296 | `			/* Not of our interest */` |
|   38589 |   297 | `			continue;` |
|       - |   298 | `		}` |
|       - |   299 | `		/* Point to the instruction to fix */` |
|  210125 |   300 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  210125 |   301 | `		if( pInstr ){` |
|  210125 |   302 | `			pInstr->iP2 = nJumpDest;` |
|  210125 |   303 | `			nFixed++;` |
|       - |   304 | `			/* Mark as fixed */` |
|  210125 |   305 | `			aFix[n].nJumpType = -1;` |
|  105060 |   306 | `		}` |
|  105065 |   307 | `	}` |
|       - |   308 | `	/* Total number of fixed jumps */` |
|  517627 |   309 | `	return nFixed;` |
|       5 |   310 |  |
|       - |   311 | `/*` |
|       - |   312 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   313 | ` * The goto statement can be used to jump to another section` |
|       - |   314 | ` * in the program.` |
|       - |   315 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   316 | ` * statement for more information.` |
|       - |   317 | ` */` |
|  210358 |   318 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   319 |  |
|       - |   320 | `	JumpFixup *pJump,*aJumps;` |
|       - |   321 | `	Label *pLabel,*aLabel;` |
|       - |   322 | `	VmInstr *pInstr;` |
|       - |   323 | `	sxi32 rc;` |
|       - |   324 | `	sxu32 n;` |
|       - |   325 | `	/* Point to the goto table */` |
|  210363 |   326 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   327 | `	/* Fix */` |
|  210509 |   328 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|     153 |   329 | `		pJump = &aJumps[n];` |
|       - |   330 | `		/* Extract the target label */` |
|     153 |   331 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|     153 |   332 | `		if( rc != SXRET_OK ){` |
|       - |   333 | `			/* No such label */` |
|      60 |   334 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|      60 |   335 | `			if( rc == SXERR_ABORT ){` |
|       3 |   336 | `				return SXERR_ABORT;` |
|       - |   337 | `			}` |
|      58 |   338 | `			continue;` |
|       - |   339 | `		}` |
|       - |   340 | `		/* Make sure the target label is reachable */` |
|      96 |   341 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|      10 |   342 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|      10 |   343 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   344 | `				return SXERR_ABORT;` |
|       - |   345 | `			}` |
|       4 |   346 | `		}` |
|       - |   347 | `		/* Fix the jump now the destination is resolved */` |
|      96 |   348 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      96 |   349 | `		if( pInstr ){` |
|      96 |   350 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   351 | `		}` |
|      50 |   352 | `	}` |
|  210361 |   353 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  210493 |   354 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   355 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   356 | `			/* Emit a warning */` |
|      40 |   357 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   358 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   359 | `		}` |
|      71 |   360 | `	}` |
|  210361 |   361 | `	return SXRET_OK;` |
|  105184 |   362 |  |
|       - |   363 | `/*` |
|       - |   364 | ` * Check if a given token value is installed in the literal table.` |
|       - |   365 | ` */` |
|  664974 |   366 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   367 |  |
|       - |   368 | `	SyHashEntry *pEntry;` |
|  664979 |   369 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  664979 |   370 | `	if( pEntry == 0 ){` |
|  288943 |   371 | `		return SXERR_NOTFOUND;` |
|       - |   372 | `	}` |
|  376041 |   373 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  376041 |   374 | `	return SXRET_OK;` |
|  332492 |   375 |  |
|       - |   376 | `/*` |
|       - |   377 | ` * Install a given constant index in the literal table.` |
|       - |   378 | ` * In order to be installed, the ph7_value must be of type string.` |
|       - |   379 | ` *` |
|       - |   380 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|       - |   381 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|       - |   382 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|       - |   383 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|       - |   384 | ` * many "" literals appear in user code.` |
|       - |   385 | ` */` |
|  288938 |   386 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   387 |  |
|  288943 |   388 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  288943 |   389 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  144469 |   390 | `	}` |
|  288943 |   391 | `	return SXRET_OK;` |
|       5 |   392 |  |
|       - |   393 | `/*` |
|       - |   394 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   395 | ` * in the constant table.` |
|       - |   396 | ` */` |
|  110902 |   397 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   398 |  |
|       - |   399 | `	ph7_value *pObj;` |
|  110907 |   400 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   401 | `	/* Reserve a new constant */` |
|  110907 |   402 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  110907 |   403 | `	if( pObj == 0 ){` |
|     ! 0 |   404 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   405 | `		return 0;` |
|       - |   406 | `	}` |
|  110907 |   407 | `	*pIdx = nIdx;` |
|       - |   408 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   409 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   410 | `	 */` |
|  110907 |   411 | `	return pObj;` |
|   55456 |   412 |  |
|       - |   413 | `/*` |
|       - |   414 | ` * Implementation of the PHP language constructs.` |
|       - |   415 | ` */` |
|       - |   416 | `/*` |
|       - |   417 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|       - |   418 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|       - |   419 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|       - |   420 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|       - |   421 | ` *` |
|       - |   422 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|       - |   423 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|       - |   424 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|       - |   425 | ` * surrounding callsites' zero-check fallback pattern.` |
|       - |   426 | ` */` |
|  397718 |   427 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   428 |  |
|       - |   429 | `	VmCallArgMap *pMap;` |
|  397723 |   430 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   431 | `	if( p3 == 0 ){` |
|      31 |   432 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   433 | `		if( pMap == 0 ) return 0;` |
|      31 |   434 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   435 | `		p3 = (void *)pMap;` |
|      14 |   436 | `	}` |
|      33 |   437 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   438 | `	return p3;` |
|  198864 |   439 |  |
|       - |   440 | `/* Forward declaration */` |
|       - |   441 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |   442 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);` |
|       - |   443 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn);` |
|       - |   444 | `/* Forward decl: union type parser is defined later in this file. */` |
|       - |   445 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |   446 | `	ph7_gen_state *pGen,` |
|       - |   447 | `	sxu32 *pnType,` |
|       - |   448 | `	SyString *pClass,` |
|       - |   449 | `	SySet *pAlts,` |
|       - |   450 | `	sxi32 *piTypeFlags,` |
|       - |   451 | `	SyString *pTypeText,` |
|       - |   452 | `	int iNullableFlag,` |
|       - |   453 | `	int iUnionFlag,` |
|       - |   454 | `	int bAllowVoid,` |
|       - |   455 | `	sxu32 nLine` |
|       - |   456 | `);` |
|       - |   457 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|       - |   458 | `static const char * TokenTypeName(sxu32 nType);` |
|       - |   459 | `/*` |
|       - |   460 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|       - |   461 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|       - |   462 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|       - |   463 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|       - |   464 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|       - |   465 | ` * for anything larger, so correctness is preserved even for pathological` |
|       - |   466 | ` * inputs like a thousand-digit number.` |
|       - |   467 | ` */` |
|       - |   468 | `#define GEN_NUM_SCRATCH 128` |
|       - |   469 | `/*` |
|       - |   470 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|       - |   471 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|       - |   472 | ` *   base  2 => 0 or 1` |
|       - |   473 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|       - |   474 | ` *              decimal scan in the lexer)` |
|       - |   475 | ` */` |
|    1076 |   476 | `static int GenStateIsBaseDigit(int c, int base)` |
|       5 |   477 |  |
|    1081 |   478 | `	if( base == 16 ){ return SyisHex(c); }` |
|     982 |   479 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     703 |   480 | `	return SyisDigit(c);` |
|     543 |   481 |  |
|       - |   482 | `/*` |
|       - |   483 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|       - |   484 | ` * underscore separator so the caller can report the malformed portion with` |
|       - |   485 | ` * the exact wording PHP uses:` |
|       - |   486 | ` *` |
|       - |   487 | ` *   syntax error, unexpected identifier "X"` |
|       - |   488 | ` *` |
|       - |   489 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|       - |   490 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|       - |   491 | ` * absorbed by the lexer specifically to let this validator see and report` |
|       - |   492 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|       - |   493 | ` * no forward rescan needed.` |
|       - |   494 | ` *` |
|       - |   495 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|       - |   496 | ` * returns 0 when it is well-formed.` |
|       - |   497 | ` */` |
|  111492 |   498 | `static int GenStateFindBadNumericSeparator(` |
|       - |   499 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   500 |  |
|  111497 |   501 | `	const char *z = pRaw->zString;` |
|  111497 |   502 | `	sxu32 n = pRaw->nByte;` |
|  111497 |   503 | `	int base = 10;` |
|       - |   504 | `	sxu32 i, start;` |
|  111497 |   505 | `	if( n < 2 ) return 0;` |
|    9387 |   506 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   507 | `		base = 16;` |
|    9352 |   508 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   509 | `		base = 2;` |
|     139 |   510 | `	}` |
|   34301 |   511 | `	for( i = 0; i < n; ++i ){` |
|   24933 |   512 | `		if( z[i] != '_' ) continue;` |
|     814 |   513 | `		if( i > 0 && i + 1 < n` |
|     543 |   514 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|     543 |   515 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|     533 |   516 | `			continue; /* well-placed separator */` |
|       - |   517 | `		}` |
|       - |   518 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|       - |   519 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|      18 |   520 | `		start = i;` |
|      23 |   521 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|      12 |   522 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|       6 |   523 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|       2 |   524 | `		}` |
|      18 |   525 | `		*pBadStart = &z[start];` |
|      18 |   526 | `		*pBadLen = n - start;` |
|      18 |   527 | `		return 1;` |
|     ! 0 |   528 | `	}` |
|    9373 |   529 | `	return 0;` |
|   55751 |   530 |  |
|       - |   531 | `/*` |
|       - |   532 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   533 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   534 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   535 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   536 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   537 | ` * so callers can bail from the current construct).` |
|       - |   538 | ` */` |
|  111492 |   539 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   540 |  |
|  111497 |   541 | `	const char *zBad = 0;` |
|  111497 |   542 | `	sxu32 nBad = 0;` |
|       - |   543 | `	SyString sBad;` |
|       - |   544 | `	sxi32 rc;` |
|  111497 |   545 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  111483 |   546 | `		return SXRET_OK;` |
|       - |   547 | `	}` |
|      18 |   548 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   549 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   550 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   551 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   552 | `		return SXERR_ABORT;` |
|       - |   553 | `	}` |
|      18 |   554 | `	return SXERR_SYNTAX;` |
|   55751 |   555 |  |
|       - |   556 | `/*` |
|       - |   557 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|       - |   558 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|       - |   559 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|       - |   560 | ` *` |
|       - |   561 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|       - |   562 | ` * and *pzAlloc is set to NULL.` |
|       - |   563 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|       - |   564 | ` * and *pzAlloc is set to NULL.` |
|       - |   565 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|       - |   566 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|       - |   567 | ` * caller with SyMemBackendFree once the converter is done.` |
|       - |   568 | ` *` |
|       - |   569 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|       - |   570 | ` * case *pOut is left untouched and the caller must not read it).` |
|       - |   571 | ` */` |
|  111478 |   572 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   573 | `	SyMemBackend *pAlloc,` |
|       - |   574 | `	const SyString *pToken,` |
|       - |   575 | `	char *zScratch, sxu32 nScratch,` |
|       - |   576 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   577 |  |
|       - |   578 | `	sxu32 i, j;` |
|  111483 |   579 | `	int hasUnderscore = 0;` |
|       - |   580 | `	char *zBuf;` |
|  111483 |   581 | `	*pzAlloc = 0;` |
|  236441 |   582 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  125215 |   583 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   62484 |   584 | `	}` |
|  111483 |   585 | `	if( !hasUnderscore ){` |
|  111231 |   586 | `		SyStringDupPtr(pOut, pToken);` |
|  111231 |   587 | `		return SXRET_OK;` |
|       - |   588 | `	}` |
|     253 |   589 | `	if( pToken->nByte <= nScratch ){` |
|     251 |   590 | `		zBuf = zScratch;` |
|     126 |   591 | `	}else{` |
|       3 |   592 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|       3 |   593 | `		if( zBuf == 0 ){` |
|     ! 0 |   594 | `			return SXERR_ABORT;` |
|       - |   595 | `		}` |
|       3 |   596 | `		*pzAlloc = zBuf;` |
|       - |   597 | `	}` |
|     253 |   598 | `	j = 0;` |
|    2895 |   599 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|    2643 |   600 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|    1322 |   601 | `	}` |
|     253 |   602 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|     253 |   603 | `	return SXRET_OK;` |
|   55744 |   604 |  |
|       - |   605 | `/*` |
|       - |   606 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |   607 | ` * Notes on the integer type.` |
|       - |   608 | ` *  According to the PHP language reference manual` |
|       - |   609 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |   610 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |   611 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |   612 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |   613 | ` * Symisc eXtension to the integer type.` |
|       - |   614 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |   615 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |   616 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |   617 | ` *  [i.e: either 32bit or 64bit].` |
|       - |   618 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |   619 | ` *  documentation.` |
|       - |   620 | ` */` |
|  111464 |   621 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   622 |  |
|  111469 |   623 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  111469 |   624 | `	sxu32 nIdx = 0;` |
|       - |   625 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  111469 |   626 | `	char *zAlloc = 0;` |
|       - |   627 | `	SyString sNum;` |
|       - |   628 | `	sxi32 rc;` |
|   55732 |   629 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  111469 |   630 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  111469 |   631 | `	if( rc != SXRET_OK ){` |
|      14 |   632 | `		return rc;` |
|       - |   633 | `	}` |
|  167186 |   634 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   55727 |   635 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  111459 |   636 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   637 | `		return SXERR_ABORT;` |
|       - |   638 | `	}` |
|  111459 |   639 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   640 | `		ph7_value *pObj;` |
|       - |   641 | `		sxi64 iValue;` |
|  110907 |   642 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  110907 |   643 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  110907 |   644 | `		if( pObj == 0 ){` |
|     ! 0 |   645 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   646 | `			return SXERR_ABORT;` |
|       - |   647 | `		}` |
|  110907 |   648 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   55456 |   649 | `	}else{` |
|       - |   650 | `		/* Real number */` |
|       - |   651 | `		ph7_value *pObj;` |
|       - |   652 | `		/* Reserve a new constant */` |
|     557 |   653 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     557 |   654 | `		if( pObj == 0 ){` |
|     ! 0 |   655 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   656 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   657 | `			return SXERR_ABORT;` |
|       - |   658 | `		}` |
|     557 |   659 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     557 |   660 | `		PH7_MemObjToReal(pObj);` |
|       - |   661 | `	}` |
|  111459 |   662 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   663 | `	/* Emit the load constant instruction */` |
|  111459 |   664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   665 | `	/* Node successfully compiled */` |
|  111459 |   666 | `	return SXRET_OK;` |
|   55737 |   667 |  |
|       - |   668 | `/*` |
|       - |   669 | ` * Compile a single quoted string.` |
|       - |   670 | ` * According to the PHP language reference manual:` |
|       - |   671 | ` *` |
|       - |   672 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |   673 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |   674 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |   675 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |   676 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |   677 | ` *` |
|       - |   678 | ` */` |
|   80182 |   679 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   680 |  |
|   80187 |   681 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   682 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   683 | `	ph7_value *pObj;` |
|       - |   684 | `	sxu32 nIdx;` |
|   80187 |   685 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   686 | `	/* Delimit the string */` |
|   80187 |   687 | `	zIn  = pStr->zString;` |
|   80187 |   688 | `	zEnd = &zIn[pStr->nByte];` |
|   80187 |   689 | `	if( zIn >= zEnd ){` |
|       - |   690 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   691 | `		 * rather than reserving a new object each time. */` |
|    6453 |   692 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    6453 |   693 | `		return SXRET_OK;` |
|       - |   694 | `	}` |
|   73739 |   695 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   696 | `		/* Already processed,emit the load constant instruction` |
|       - |   697 | `		 * and return.` |
|       - |   698 | `		 */` |
|   29401 |   699 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   29401 |   700 | `		return SXRET_OK;` |
|       - |   701 | `	}` |
|       - |   702 | `	/* Reserve a new constant */` |
|   44343 |   703 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   44343 |   704 | `	if( pObj == 0 ){` |
|     ! 0 |   705 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   706 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   707 | `		return SXERR_ABORT;` |
|       - |   708 | `	}` |
|   44343 |   709 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   710 | `	/* Compile the node */` |
|   44390 |   711 | `	for(;;){` |
|   88785 |   712 | `		if( zIn >= zEnd ){` |
|       - |   713 | `			/* End of input */` |
|   44343 |   714 | `			break;` |
|       - |   715 | `		}` |
|   44447 |   716 | `		zCur = zIn;` |
|  699389 |   717 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  654947 |   718 | `			zIn++;` |
|       5 |   719 | `		}` |
|   44447 |   720 | `		if( zIn > zCur ){` |
|       - |   721 | `			/* Append raw contents*/` |
|   44423 |   722 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   22209 |   723 | `		}` |
|   44447 |   724 | `		zIn++;` |
|   44447 |   725 | `		if( zIn < zEnd ){` |
|     126 |   726 | `			if( zIn[0] == '\\' ){` |
|       - |   727 | `				/* A literal backslash */` |
|      23 |   728 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     115 |   729 | `			}else if( zIn[0] == '\'' ){` |
|       - |   730 | `				/* A single quote */` |
|      11 |   731 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   732 | `			}else{` |
|       - |   733 | `				/* verbatim copy */` |
|      94 |   734 | `				zIn--;` |
|      94 |   735 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      94 |   736 | `				zIn++;` |
|       - |   737 | `			}` |
|      62 |   738 | `		}` |
|       - |   739 | `		/* Advance the stream cursor */` |
|   44447 |   740 | `		zIn++;` |
|       5 |   741 | `	}` |
|       - |   742 | `	/* Emit the load constant instruction */` |
|   44343 |   743 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   44343 |   744 | `	if( pStr->nByte < 1024 ){` |
|       - |   745 | `		/* Install in the literal table */` |
|   44343 |   746 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   22169 |   747 | `	}` |
|       - |   748 | `	/* Node successfully compiled */` |
|   44343 |   749 | `	return SXRET_OK;` |
|   40096 |   750 |  |
|       - |   751 | `/*` |
|       - |   752 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |   753 | ` *` |
|       - |   754 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |   755 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |   756 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |   757 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |   758 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |   759 | ` *` |
|       - |   760 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |   761 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |   762 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |   763 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |   764 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |   765 | ` *     whitespace.` |
|       - |   766 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |   767 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |   768 | ` */` |
|     108 |   769 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       5 |   770 |  |
|     113 |   771 | `	SyString *pIn = &pGen->pIn->sData;` |
|     113 |   772 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   773 | `	const char *zPrefix;` |
|       - |   774 | `	const char *z, *zEnd;` |
|       - |   775 | `	char *zBuf, *zDst;` |
|     113 |   776 | `	if( nIndent == 0 ){` |
|       - |   777 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      67 |   778 | `		*pOut = *pIn;` |
|      67 |   779 | `		return SXRET_OK;` |
|       - |   780 | `	}` |
|       - |   781 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |   782 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |   783 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |   784 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |   785 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |   786 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      49 |   787 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      49 |   788 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   789 | `		zPrefix += 2;` |
|     ! 0 |   790 | `	}else{` |
|      49 |   791 | `		zPrefix += 1;` |
|       - |   792 | `	}` |
|       - |   793 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      49 |   794 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      49 |   795 | `	if( zBuf == 0 ){` |
|     ! 0 |   796 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   797 | `		return SXERR_ABORT;` |
|       - |   798 | `	}` |
|      49 |   799 | `	zDst = zBuf;` |
|      49 |   800 | `	z = pIn->zString;` |
|      49 |   801 | `	zEnd = z + pIn->nByte;` |
|     131 |   802 | `	while( z < zEnd ){` |
|      73 |   803 | `		const char *zLine = z;` |
|       - |   804 | `		sxu32 nLine;` |
|       - |   805 | `		int bEmpty;` |
|     801 |   806 | `		while( z < zEnd && z[0] != '\n' ){` |
|     733 |   807 | `			z++;` |
|       5 |   808 | `		}` |
|      73 |   809 | `		nLine = (sxu32)(z - zLine);` |
|      73 |   810 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      73 |   811 | `		if( !bEmpty ){` |
|       - |   812 | `			sxu32 i;` |
|      69 |   813 | `			if( nLine < nIndent ){` |
|     ! 0 |   814 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   815 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   816 | `					nIndent);` |
|     ! 0 |   817 | `				return SXERR_ABORT;` |
|       - |   818 | `			}` |
|     271 |   819 | `			for( i = 0; i < nIndent; i++ ){` |
|     215 |   820 | `				if( zLine[i] != zPrefix[i] ){` |
|      12 |   821 | `					unsigned char c = (unsigned char)zLine[i];` |
|      12 |   822 | `					if( c == ' ' \|\| c == '\t' ){` |
|       6 |   823 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   824 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       4 |   825 | `					}else{` |
|       8 |   826 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   827 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   828 | `							nIndent);` |
|       - |   829 | `					}` |
|      12 |   830 | `					return SXERR_ABORT;` |
|       - |   831 | `				}` |
|     104 |   832 | `			}` |
|      57 |   833 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      57 |   834 | `			zDst += nLine - nIndent;` |
|      33 |   835 | `		}else if( nLine == 1 ){` |
|       - |   836 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |   837 | `			*zDst++ = '\r';` |
|     ! 0 |   838 | `		}` |
|      61 |   839 | `		if( z < zEnd ){` |
|      25 |   840 | `			*zDst++ = '\n';` |
|      25 |   841 | `			z++;` |
|      12 |   842 | `		}` |
|       1 |   843 | `	}` |
|      37 |   844 | `	pOut->zString = zBuf;` |
|      37 |   845 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      37 |   846 | `	return SXRET_OK;` |
|      59 |   847 |  |
|       - |   848 | `/*` |
|       - |   849 | ` * Compile a nowdoc string.` |
|       - |   850 | ` * According to the PHP language reference manual:` |
|       - |   851 | ` *` |
|       - |   852 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |   853 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |   854 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |   855 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |   856 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |   857 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |   858 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |   859 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |   860 | ` *  of the closing identifier.` |
|       - |   861 | ` */` |
|      44 |   862 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |   863 |  |
|       - |   864 | `	SyString sStripped;` |
|       - |   865 | `	SyString *pStr;` |
|       - |   866 | `	ph7_value *pObj;` |
|       - |   867 | `	sxu32 nIdx;` |
|       - |   868 | `	sxi32 rc;` |
|      48 |   869 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      48 |   870 | `	if( rc != SXRET_OK ){` |
|       6 |   871 | `		return rc;` |
|       - |   872 | `	}` |
|      42 |   873 | `	pStr = &sStripped;` |
|      42 |   874 | `	nIdx = 0; /* Prevent compiler warning */` |
|      42 |   875 | `	if( pStr->nByte <= 0 ){` |
|       - |   876 | `		/* Empty string,load NULL */` |
|       7 |   877 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |   878 | `		return SXRET_OK;` |
|       - |   879 | `	}` |
|       - |   880 | `	/* Reserve a new constant */` |
|      36 |   881 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      36 |   882 | `	if( pObj == 0 ){` |
|     ! 0 |   883 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   884 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   885 | `		return SXERR_ABORT;` |
|       - |   886 | `	}` |
|       - |   887 | `	/* No processing is done here, simply a memcpy() operation */` |
|      36 |   888 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |   889 | `	/* Emit the load constant instruction */` |
|      36 |   890 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   891 | `	/* Node successfully compiled */` |
|      36 |   892 | `	return SXRET_OK;` |
|      26 |   893 |  |
|       - |   894 | `/*` |
|       - |   895 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |   896 | ` * According to the PHP language reference manual` |
|       - |   897 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |   898 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |   899 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |   900 | ` *  property in a string with a minimum of effort.` |
|       - |   901 | ` *  Simple syntax` |
|       - |   902 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |   903 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |   904 | ` *   the end of the name.` |
|       - |   905 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |   906 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |   907 | ` *   as to simple variables.` |
|       - |   908 | ` *  Complex (curly) syntax` |
|       - |   909 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |   910 | ` *   of complex expressions.` |
|       - |   911 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |   912 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |   913 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |   914 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |   915 | ` */` |
|    2058 |   916 | `static sxi32 GenStateProcessStringExpression(` |
|       - |   917 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   918 | `	sxu32 nLine,         /* Line number */` |
|       - |   919 | `	const char *zIn,     /* Raw expression */` |
|       - |   920 | `	const char *zEnd     /* End of the expression */` |
|       - |   921 | `	)` |
|       5 |   922 |  |
|       - |   923 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |   924 | `	SySet sToken;` |
|       - |   925 | `	sxi32 rc;` |
|       - |   926 | `	/* Initialize the token set */` |
|    2063 |   927 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   928 | `	/* Preallocate some slots */` |
|    2063 |   929 | `	SySetAlloc(&sToken,0x08);` |
|       - |   930 | `	/* Tokenize the text */` |
|    2063 |   931 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   932 | `	/* Swap delimiter */` |
|    2063 |   933 | `	pTmpIn  = pGen->pIn;` |
|    2063 |   934 | `	pTmpEnd = pGen->pEnd;` |
|    2063 |   935 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2063 |   936 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   937 | `	/* Compile the expression */` |
|    2063 |   938 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   939 | `	/* Restore token stream */` |
|    2063 |   940 | `	pGen->pIn  = pTmpIn;` |
|    2063 |   941 | `	pGen->pEnd = pTmpEnd;` |
|       - |   942 | `	/* Release the token set */` |
|    2063 |   943 | `	SySetRelease(&sToken);` |
|       - |   944 | `	/* Compilation result */` |
|    2063 |   945 | `	return rc;` |
|       5 |   946 |  |
|       - |   947 | `/*` |
|       - |   948 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   949 | ` */` |
|   22108 |   950 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   951 |  |
|       - |   952 | `	ph7_value *pConstObj;` |
|   22113 |   953 | `	sxu32 nIdx = 0;` |
|       - |   954 | `	/* Reserve a new constant */` |
|   22113 |   955 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   22113 |   956 | `	if( pConstObj == 0 ){` |
|     ! 0 |   957 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   958 | `		return 0;` |
|       - |   959 | `	}` |
|   22113 |   960 | `	(*pCount)++;` |
|   22113 |   961 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   962 | `	/* Emit the load constant instruction */` |
|   22113 |   963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   22113 |   964 | `	return pConstObj;` |
|   11059 |   965 |  |
|       - |   966 | `/*` |
|       - |   967 | ` * Compile a double quoted/heredoc string.` |
|       - |   968 | ` * According to the PHP language reference manual` |
|       - |   969 | ` * Heredoc` |
|       - |   970 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |   971 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |   972 | ` *  to close the quotation.` |
|       - |   973 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |   974 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |   975 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |   976 | ` *  Warning` |
|       - |   977 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |   978 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |   979 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |   980 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |   981 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |   982 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |   983 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |   984 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |   985 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |   986 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |   987 | ` * Double quoted` |
|       - |   988 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |   989 | ` *  Escaped characters Sequence 	Meaning` |
|       - |   990 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |   991 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |   992 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |   993 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |   994 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |   995 | ` *  \\ backslash` |
|       - |   996 | ` *  \$ dollar sign` |
|       - |   997 | ` *  \" double-quote` |
|       - |   998 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |   999 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  1000 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  1001 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  1002 | ` * See string parsing for details.` |
|       - |  1003 | ` */` |
|   20662 |  1004 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1005 |  |
|   20667 |  1006 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1007 | `	const char *zIn,*zCur,*zEnd;` |
|   20667 |  1008 | `	ph7_value *pObj = 0;` |
|       - |  1009 | `	sxi32 iCons;` |
|       - |  1010 | `	sxi32 rc;` |
|       - |  1011 | `	/* Delimit the string */` |
|   20667 |  1012 | `	zIn  = pStr->zString;` |
|   20667 |  1013 | `	zEnd = &zIn[pStr->nByte];` |
|   20667 |  1014 | `	if( zIn >= zEnd ){` |
|       - |  1015 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1016 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1017 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1018 | `		 */` |
|     277 |  1019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     277 |  1020 | `		return SXRET_OK;` |
|       - |  1021 | `	}` |
|   20395 |  1022 | `	zCur = 0;` |
|       - |  1023 | `	/* Compile the node */` |
|   20395 |  1024 | `	iCons = 0;` |
|   11224 |  1025 | `	for(;;){` |
|   33813 |  1026 | `		zCur = zIn;` |
|  165381 |  1027 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  133631 |  1028 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1029 | `				break;` |
|  133505 |  1030 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1934 |  1031 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     970 |  1032 | `					break;` |
|       - |  1033 | `			}` |
|  131573 |  1034 | `			zIn++;` |
|       5 |  1035 | `		}` |
|   33813 |  1036 | `		if( zIn > zCur ){` |
|   15607 |  1037 | `			if( pObj == 0 ){` |
|   15227 |  1038 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   15227 |  1039 | `				if( pObj == 0 ){` |
|     ! 0 |  1040 | `					return SXERR_ABORT;` |
|       - |  1041 | `				}` |
|    7611 |  1042 | `			}` |
|   15607 |  1043 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    7801 |  1044 | `		}` |
|   33813 |  1045 | `		if( zIn >= zEnd ){` |
|   20395 |  1046 | `			break;` |
|       - |  1047 | `		}` |
|   13423 |  1048 | `		if( zIn[0] == '\\' ){` |
|   11365 |  1049 | `			const char *zPtr = 0;` |
|       - |  1050 | `			sxu32 n;` |
|   11365 |  1051 | `			zIn++;` |
|   11365 |  1052 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1053 | `				break;` |
|       - |  1054 | `			}` |
|   11365 |  1055 | `			if( pObj == 0 ){` |
|    6891 |  1056 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    6891 |  1057 | `				if( pObj == 0 ){` |
|     ! 0 |  1058 | `					return SXERR_ABORT;` |
|       - |  1059 | `				}` |
|    3443 |  1060 | `			}` |
|   11365 |  1061 | `			n = sizeof(char); /* size of conversion */` |
|   11365 |  1062 | `			switch( zIn[0] ){` |
|       3 |  1063 | `			case '$':` |
|       - |  1064 | `				/* Dollar sign */` |
|       7 |  1065 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 |  1066 | `				break;` |
|      44 |  1067 | `			case '\\':` |
|       - |  1068 | `				/* A literal backslash */` |
|      92 |  1069 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      92 |  1070 | `				break;` |
|       2 |  1071 | `			case 'a':` |
|       - |  1072 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 |  1073 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 |  1074 | `				break;` |
|       2 |  1075 | `			case 'b':` |
|       - |  1076 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 |  1077 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 |  1078 | `				break;` |
|       4 |  1079 | `			case 'f':` |
|       - |  1080 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  1081 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  1082 | `				break;` |
|    5254 |  1083 | `			case 'n':` |
|       - |  1084 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   10513 |  1085 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   10513 |  1086 | `				break;` |
|      19 |  1087 | `			case 'r':` |
|       - |  1088 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      43 |  1089 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      43 |  1090 | `				break;` |
|      24 |  1091 | `			case 't':` |
|       - |  1092 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      53 |  1093 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      53 |  1094 | `				break;` |
|       3 |  1095 | `			case 'v':` |
|       - |  1096 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  1097 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  1098 | `				break;` |
|       1 |  1099 | `			case '\'':` |
|       - |  1100 | `				/* Single quote */` |
|       3 |  1101 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 |  1102 | `				break;` |
|      66 |  1103 | `			case '"':` |
|       - |  1104 | `				/* Double quote */` |
|     137 |  1105 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     137 |  1106 | `				break;` |
|       6 |  1107 | `			case '0':` |
|       - |  1108 | `				/* NUL byte */` |
|      13 |  1109 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      13 |  1110 | `				break;` |
|     226 |  1111 | `			case 'x':` |
|     453 |  1112 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  1113 | `					int c;` |
|       - |  1114 | `					/* Hex digit */` |
|     439 |  1115 | `					c = SyHexToint(zIn[1]) << 4;` |
|     439 |  1116 | `					if( &zIn[2] < zEnd ){` |
|     439 |  1117 | `						c +=  SyHexToint(zIn[2]);` |
|     219 |  1118 | `					}` |
|       - |  1119 | `					/* Output char */` |
|     439 |  1120 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     439 |  1121 | `					n += sizeof(char) * 2;` |
|     220 |  1122 | `				}else{` |
|       - |  1123 | `					/* Output literal character  */` |
|      15 |  1124 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  1125 | `				}` |
|     453 |  1126 | `				break;` |
|      15 |  1127 | `			case 'o':` |
|      31 |  1128 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - |  1129 | `					/* Octal digit stream */` |
|       - |  1130 | `					int c;` |
|      21 |  1131 | `					c = 0;` |
|      21 |  1132 | `					zIn++;` |
|      61 |  1133 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 |  1134 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 |  1135 | `							break;` |
|       - |  1136 | `						}` |
|      41 |  1137 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 |  1138 | `					}` |
|      21 |  1139 | `					if ( c > 0 ){` |
|      15 |  1140 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 |  1141 | `					}` |
|      21 |  1142 | `					n = (sxu32)(zPtr-zIn);` |
|      11 |  1143 | `				}else{` |
|       - |  1144 | `					/* Output literal character  */` |
|      11 |  1145 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - |  1146 | `				}` |
|      31 |  1147 | `				break;` |
|      11 |  1148 | `			default:` |
|       - |  1149 | `				/* Output without a slash */` |
|      23 |  1150 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 |  1151 | `				break;` |
|       - |  1152 | `			}` |
|       - |  1153 | `			/* Advance the stream cursor */` |
|   11365 |  1154 | `			zIn += n;` |
|   11365 |  1155 | `			continue;` |
|       - |  1156 | `		}` |
|    2063 |  1157 | `		if( zIn[0] == '{' ){` |
|       - |  1158 | `			/* Curly syntax */` |
|       - |  1159 | `			const char *zExpr;` |
|     131 |  1160 | `			sxi32 iNest = 1;` |
|     131 |  1161 | `			zIn++;` |
|     131 |  1162 | `			zExpr = zIn;` |
|       - |  1163 | `			/* Synchronize with the next closing curly braces */` |
|    1359 |  1164 | `			while( zIn < zEnd ){` |
|    1359 |  1165 | `				if( zIn[0] == '{' ){` |
|       - |  1166 | `					/* Increment nesting level */` |
|       9 |  1167 | `					iNest++;` |
|    1355 |  1168 | `				}else if(zIn[0] == '}' ){` |
|       - |  1169 | `					/* Decrement nesting level */` |
|     139 |  1170 | `					iNest--;` |
|     139 |  1171 | `					if( iNest <= 0 ){` |
|     131 |  1172 | `						break;` |
|       - |  1173 | `					}` |
|       4 |  1174 | `				}` |
|    1231 |  1175 | `				zIn++;` |
|       3 |  1176 | `			}` |
|       - |  1177 | `			/* Process the expression */` |
|     131 |  1178 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     131 |  1179 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1180 | `				return SXERR_ABORT;` |
|       - |  1181 | `			}` |
|     131 |  1182 | `			if( rc != SXERR_EMPTY ){` |
|     131 |  1183 | `				++iCons;` |
|      64 |  1184 | `			}` |
|     131 |  1185 | `			if( zIn < zEnd ){` |
|       - |  1186 | `				/* Jump the trailing curly */` |
|     131 |  1187 | `				zIn++;` |
|      64 |  1188 | `			}` |
|      67 |  1189 | `		}else{` |
|       - |  1190 | `			/* Simple syntax */` |
|    1935 |  1191 | `			const char *zExpr = zIn;` |
|       - |  1192 | `			/* Assemble variable name */` |
|     971 |  1193 | `			for(;;){` |
|       - |  1194 | `				/* Jump leading dollars */` |
|    3877 |  1195 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1935 |  1196 | `					zIn++;` |
|       5 |  1197 | `				}` |
|     971 |  1198 | `				for(;;){` |
|   11188 |  1199 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8275 |  1200 | `						zIn++;` |
|       5 |  1201 | `					}` |
|    1947 |  1202 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1203 | `						/* UTF-8 stream */` |
|     ! 0 |  1204 | `						zIn++;` |
|     ! 0 |  1205 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1206 | `							zIn++;` |
|     ! 0 |  1207 | `						}` |
|     ! 0 |  1208 | `						continue;` |
|       - |  1209 | `					}` |
|    1947 |  1210 | `					break;` |
|     ! 0 |  1211 | `				}` |
|    1947 |  1212 | `				if( zIn >= zEnd ){` |
|     126 |  1213 | `					break;` |
|       - |  1214 | `				}` |
|    1825 |  1215 | `				if( zIn[0] == '[' ){` |
|      12 |  1216 | `					sxi32 iSquare = 1;` |
|      12 |  1217 | `					zIn++;` |
|      28 |  1218 | `					while( zIn < zEnd ){` |
|      28 |  1219 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  1220 | `							iSquare++;` |
|      28 |  1221 | `						}else if (zIn[0] == ']' ){` |
|      12 |  1222 | `							iSquare--;` |
|      12 |  1223 | `							if( iSquare <= 0 ){` |
|      12 |  1224 | `								break;` |
|       - |  1225 | `							}` |
|     ! 0 |  1226 | `						}` |
|      18 |  1227 | `						zIn++;` |
|       2 |  1228 | `					}` |
|      12 |  1229 | `					if( zIn < zEnd ){` |
|      12 |  1230 | `						zIn++;` |
|       5 |  1231 | `					}` |
|      12 |  1232 | `					break;` |
|    1815 |  1233 | `				}else if(zIn[0] == '{' ){` |
|       6 |  1234 | `					sxi32 iCurly = 1;` |
|       6 |  1235 | `					zIn++;` |
|      18 |  1236 | `					while( zIn < zEnd ){` |
|      16 |  1237 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  1238 | `							iCurly++;` |
|      16 |  1239 | `						}else if (zIn[0] == '}' ){` |
|       3 |  1240 | `							iCurly--;` |
|       3 |  1241 | `							if( iCurly <= 0 ){` |
|       3 |  1242 | `								break;` |
|       - |  1243 | `							}` |
|     ! 0 |  1244 | `						}` |
|      14 |  1245 | `						zIn++;` |
|       2 |  1246 | `					}` |
|       6 |  1247 | `					if( zIn < zEnd ){` |
|       3 |  1248 | `						zIn++;` |
|       1 |  1249 | `					}` |
|       6 |  1250 | `					break;` |
|    1811 |  1251 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1252 | `					/* Member access operator '->' */` |
|      14 |  1253 | `					zIn += 2;` |
|    1805 |  1254 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1255 | `					/* Static member access operator '::' */` |
|     ! 0 |  1256 | `					zIn += 2;` |
|     ! 0 |  1257 | `				}else{` |
|     902 |  1258 | `					break;` |
|       - |  1259 | `				}` |
|       2 |  1260 | `			}` |
|       - |  1261 | `			/* Process the expression */` |
|    1935 |  1262 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1935 |  1263 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1264 | `				return SXERR_ABORT;` |
|       - |  1265 | `			}` |
|    1935 |  1266 | `			if( rc != SXERR_EMPTY ){` |
|    1933 |  1267 | `				++iCons;` |
|     964 |  1268 | `			}` |
|       - |  1269 | `		}` |
|       - |  1270 | `		/* Invalidate the previously used constant */` |
|    2063 |  1271 | `		pObj = 0;` |
|       5 |  1272 | `	}/*for(;;)*/` |
|   20395 |  1273 | `	if( iCons > 1 ){` |
|       - |  1274 | `		/* Concatenate all compiled constants */` |
|    1535 |  1275 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     765 |  1276 | `	}` |
|       - |  1277 | `	/* Node successfully compiled */` |
|   20395 |  1278 | `	return SXRET_OK;` |
|   10336 |  1279 |  |
|       - |  1280 | `/*` |
|       - |  1281 | ` * Compile a double quoted string.` |
|       - |  1282 | ` *  See the block-comment above for more information.` |
|       - |  1283 | ` */` |
|   20602 |  1284 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1285 |  |
|       - |  1286 | `	sxi32 rc;` |
|   20607 |  1287 | `	rc = GenStateCompileString(&(*pGen));` |
|   10301 |  1288 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1289 | `	/* Compilation result */` |
|   20607 |  1290 | `	return rc;` |
|       5 |  1291 |  |
|       - |  1292 | `/*` |
|       - |  1293 | ` * Compile a Heredoc string.` |
|       - |  1294 | ` *  See the block-comment above for more information.` |
|       - |  1295 | ` */` |
|      64 |  1296 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  1297 |  |
|       - |  1298 | `	SyString sOrig, sStripped;` |
|       - |  1299 | `	sxi32 rc;` |
|      68 |  1300 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      68 |  1301 | `	if( rc != SXRET_OK ){` |
|       6 |  1302 | `		return rc;` |
|       - |  1303 | `	}` |
|       - |  1304 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - |  1305 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - |  1306 | `	 * Restore before returning so downstream code that references pIn is` |
|       - |  1307 | `	 * unaffected, including on the error path. */` |
|      63 |  1308 | `	sOrig = pGen->pIn->sData;` |
|      63 |  1309 | `	pGen->pIn->sData = sStripped;` |
|      63 |  1310 | `	rc = GenStateCompileString(&(*pGen));` |
|      63 |  1311 | `	pGen->pIn->sData = sOrig;` |
|      30 |  1312 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      63 |  1313 | `	return rc;` |
|      36 |  1314 |  |
|       - |  1315 | `/*` |
|       - |  1316 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  1317 | ` *  Notes on array entries.` |
|       - |  1318 | ` *  According to the PHP language reference manual` |
|       - |  1319 | ` *  An array can be created by the array() language construct.` |
|       - |  1320 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  1321 | ` *  array(  key =>  value` |
|       - |  1322 | ` *    , ...` |
|       - |  1323 | ` *    )` |
|       - |  1324 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - |  1325 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - |  1326 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - |  1327 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - |  1328 | ` *  contain integer and string indices.` |
|       - |  1329 | ` *  A value can be any PHP type.` |
|       - |  1330 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - |  1331 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - |  1332 | ` *  is specified, that value will be overwritten.` |
|       - |  1333 | ` */` |
|   19088 |  1334 | `static sxi32 GenStateCompileArrayEntry(` |
|       - |  1335 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1336 | `	SyToken *pIn,        /* Token stream */` |
|       - |  1337 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - |  1338 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - |  1339 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - |  1340 | `	)` |
|       5 |  1341 |  |
|       - |  1342 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1343 | `	sxi32 rc;` |
|       - |  1344 | `	/* Swap token stream */` |
|   19093 |  1345 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1346 | `	/* Compile the expression*/` |
|   19093 |  1347 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1348 | `	/* Restore token stream */` |
|   19093 |  1349 | `	RE_SWAP_DELIMITER(pGen);` |
|   19093 |  1350 | `	return rc;` |
|       5 |  1351 |  |
|       - |  1352 | `/*` |
|       - |  1353 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - |  1354 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1355 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1356 | ` * error message.` |
|       - |  1357 | ` * See the routine responible of compiling the array language construct` |
|       - |  1358 | ` * for more inforation.` |
|       - |  1359 | ` */` |
|      36 |  1360 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1361 |  |
|      40 |  1362 | `	sxi32 rc = SXRET_OK;` |
|      40 |  1363 | `	if( pRoot->pOp ){` |
|      19 |  1364 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1365 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      16 |  1366 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1367 | `			/* Unexpected expression */` |
|      13 |  1368 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1369 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      13 |  1370 | `			if( rc != SXERR_ABORT ){` |
|      13 |  1371 | `				rc = SXERR_INVALID;` |
|       5 |  1372 | `			}` |
|       9 |  1373 | `		}` |
|      31 |  1374 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1375 | `		/* Unexpected expression */` |
|       3 |  1376 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1377 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1378 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1379 | `			rc = SXERR_INVALID;` |
|       1 |  1380 | `		}` |
|       1 |  1381 | `	}` |
|      40 |  1382 | `	return rc;` |
|       4 |  1383 |  |
|       - |  1384 | `/*` |
|       - |  1385 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1386 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1387 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1388 | ` */` |
|   27724 |  1389 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1390 |  |
|       - |  1391 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1392 | `	SyToken *pKey,*pCur;` |
|   27729 |  1393 | `	sxi32 iEmitRef = 0;` |
|   27729 |  1394 | `	sxi32 iSpread = 0;` |
|   27729 |  1395 | `	sxi32 nPair = 0;` |
|       - |  1396 | `	sxi32 iNest;` |
|       - |  1397 | `	sxi32 rc;` |
|   27729 |  1398 | `	xValidator = 0;` |
|   22617 |  1399 | `	for(;;){` |
|       - |  1400 | `		/* Jump leading commas */` |
|   51187 |  1401 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5953 |  1402 | `			pGen->pIn++;` |
|       5 |  1403 | `		}` |
|   45239 |  1404 | `		pCur = pGen->pIn;` |
|   45239 |  1405 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1406 | `			/* No more entry to process */` |
|   27713 |  1407 | `			break;` |
|       - |  1408 | `		}` |
|   17531 |  1409 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1410 | `			continue;` |
|       - |  1411 | `		}` |
|       - |  1412 | `		/* Compile the key if available */` |
|   17531 |  1413 | `		pKey = pCur;` |
|   17531 |  1414 | `		iNest = 0;` |
|   49007 |  1415 | `		while( pCur < pGen->pIn ){` |
|   32945 |  1416 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1465 |  1417 | `				break;` |
|       - |  1418 | `			}` |
|       - |  1419 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1420 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1421 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1422 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1423 | `			 */` |
|   31485 |  1424 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      84 |  1425 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      84 |  1426 | `				SyToken *pFn = pCur;` |
|      82 |  1427 | `				if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pGen->pIn` |
|     ! 0 |  1428 | `					&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       2 |  1429 | `					&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1430 | `					pFn = &pCur[1];` |
|     ! 0 |  1431 | `					nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1432 | `				}` |
|      84 |  1433 | `				if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1434 | `					pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1435 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1436 | `						pCur++;` |
|     ! 0 |  1437 | `					}` |
|       5 |  1438 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1439 | `						pCur++;` |
|       5 |  1440 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1441 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1442 | `						if( pCur < pGen->pIn ){` |
|       5 |  1443 | `							pCur++;` |
|       2 |  1444 | `						}` |
|       2 |  1445 | `					}` |
|       5 |  1446 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1447 | `						pCur++;` |
|     ! 0 |  1448 | `						if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1449 | `							&& pCur->sData.nByte == 1` |
|     ! 0 |  1450 | `							&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1451 | `							pCur++;` |
|     ! 0 |  1452 | `						}` |
|     ! 0 |  1453 | `						if( pCur < pGen->pIn` |
|     ! 0 |  1454 | `							&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1455 | `							pCur++;` |
|     ! 0 |  1456 | `						}` |
|     ! 0 |  1457 | `					}` |
|       - |  1458 | `					/* The rest of the entry is the arrow function body — no` |
|       - |  1459 | `					 * outer key to extract. Stop the scan here. */` |
|       5 |  1460 | `					pCur = pGen->pIn;` |
|       5 |  1461 | `					break;` |
|       - |  1462 | `				}` |
|       - |  1463 | `				/* Match expression (PHP 8.0): 'match (subject) { ... }'.` |
|       - |  1464 | `				 * The '=>' inside match arms is not an array key/value separator —` |
|       - |  1465 | `				 * it introduces each arm's result expression. Skip past the full` |
|       - |  1466 | `				 * match span so the outer scan sees no false '=>'. */` |
|      80 |  1467 | `				if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1468 | `					pCur++; /* past 'match' */` |
|       3 |  1469 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1470 | `						pCur++;` |
|       3 |  1471 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1472 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1473 | `						if( pCur < pGen->pIn ){` |
|       3 |  1474 | `							pCur++;` |
|       1 |  1475 | `						}` |
|       1 |  1476 | `					}` |
|       3 |  1477 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1478 | `						pCur++;` |
|       3 |  1479 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1480 | `							PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1481 | `						if( pCur < pGen->pIn ){` |
|       3 |  1482 | `							pCur++;` |
|       1 |  1483 | `						}` |
|       1 |  1484 | `					}` |
|       3 |  1485 | `					continue;` |
|       - |  1486 | `				}` |
|      38 |  1487 | `			}` |
|   31479 |  1488 | `			if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     265 |  1489 | `				iNest++;` |
|   31348 |  1490 | `			}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1491 | `				/* Don't worry about mismatched brackets here,the expression` |
|       - |  1492 | `				 * parser will shortly detect any syntax error.` |
|       - |  1493 | `				 */` |
|     265 |  1494 | `				iNest--;` |
|     131 |  1495 | `			}` |
|   31479 |  1496 | `			pCur++;` |
|       5 |  1497 | `		}` |
|   17531 |  1498 | `		rc = SXERR_EMPTY;` |
|   17531 |  1499 | `		if( pCur < pGen->pIn ){` |
|    1465 |  1500 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1501 | `				/* Missing value */` |
|      13 |  1502 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1503 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1504 | `					return SXERR_ABORT;` |
|       - |  1505 | `				}` |
|      13 |  1506 | `				return SXRET_OK;` |
|       - |  1507 | `			}` |
|       - |  1508 | `			/* Compile the expression holding the key */` |
|    1455 |  1509 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1510 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1455 |  1511 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1512 | `				return SXERR_ABORT;` |
|       - |  1513 | `			}` |
|    1455 |  1514 | `			pCur++; /* Jump the '=>' operator */` |
|   16796 |  1515 | `		}else if( pKey == pCur ){` |
|       - |  1516 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1517 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1518 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1519 | `		}else{` |
|       - |  1520 | `			/* Reset back the cursor and point to the entry value */` |
|   16071 |  1521 | `			pCur = pKey;` |
|       - |  1522 | `		}` |
|   17521 |  1523 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1524 | `			/* No available key,load NULL */` |
|   16073 |  1525 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    8034 |  1526 | `		}` |
|   17521 |  1527 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1528 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      44 |  1529 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      44 |  1530 | `			iEmitRef = 1;` |
|      44 |  1531 | `			pCur++; /* Jump the '&' token */` |
|      44 |  1532 | `			if( pCur >= pGen->pIn ){` |
|       - |  1533 | `				/* Missing value */` |
|       3 |  1534 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1535 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1536 | `					return SXERR_ABORT;` |
|       - |  1537 | `				}` |
|       3 |  1538 | `				return SXRET_OK;` |
|       - |  1539 | `			}` |
|      19 |  1540 | `		}` |
|       - |  1541 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - |  1542 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - |  1543 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - |  1544 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - |  1545 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   17519 |  1546 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   17519 |  1547 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - |  1548 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - |  1549 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - |  1550 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - |  1551 | `			 * output is engine-portable. */` |
|       6 |  1552 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - |  1553 | `				"syntax error, unexpected token \"...\"");` |
|       6 |  1554 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1555 | `				return SXERR_ABORT;` |
|       - |  1556 | `			}` |
|       6 |  1557 | `			return SXRET_OK;` |
|       - |  1558 | `		}` |
|       - |  1559 | `		/* Compile indice value */` |
|   17515 |  1560 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   17515 |  1561 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1562 | `			return SXERR_ABORT;` |
|       - |  1563 | `		}` |
|   17515 |  1564 | `		if( iSpread ){` |
|       - |  1565 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1566 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   17484 |  1567 | `		}else if( iEmitRef ){` |
|       - |  1568 | `			/* Emit the load reference instruction */` |
|      40 |  1569 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1570 | `		}` |
|   17515 |  1571 | `		xValidator = 0;` |
|   17515 |  1572 | `		iEmitRef = 0;` |
|   17515 |  1573 | `		iSpread = 0;` |
|   17515 |  1574 | `		nPair++;` |
|       5 |  1575 | `	}` |
|       - |  1576 | `	/* Emit the load map instruction */` |
|   27713 |  1577 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1578 | `	/* Node successfully compiled */` |
|   27713 |  1579 | `	return SXRET_OK;` |
|   13867 |  1580 |  |
|       - |  1581 | `/*` |
|       - |  1582 | ` * Compile the 'array' language construct.` |
|       - |  1583 | ` *	 According to the PHP language reference manual` |
|       - |  1584 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1585 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1586 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1587 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1588 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1589 | ` */` |
|   26988 |  1590 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1591 |  |
|       - |  1592 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   26993 |  1593 | `	pGen->pIn += 2;` |
|   26993 |  1594 | `	pGen->pEnd--;` |
|   13494 |  1595 | `	SXUNUSED(iCompileFlag);` |
|   26993 |  1596 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1597 |  |
|       - |  1598 | `/*` |
|       - |  1599 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1600 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1601 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1602 | ` */` |
|     736 |  1603 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1604 |  |
|       - |  1605 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     741 |  1606 | `	pGen->pIn++;` |
|     741 |  1607 | `	pGen->pEnd--;` |
|     368 |  1608 | `	SXUNUSED(iCompileFlag);` |
|     741 |  1609 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1610 |  |
|       - |  1611 | `/*` |
|       - |  1612 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1613 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1614 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1615 | ` * error message.` |
|       - |  1616 | ` * See the routine responible of compiling the list language construct` |
|       - |  1617 | ` * for more inforation.` |
|       - |  1618 | ` */` |
|     128 |  1619 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1620 |  |
|     132 |  1621 | `	sxi32 rc = SXRET_OK;` |
|     132 |  1622 | `	if( pRoot->pOp ){` |
|     ! 0 |  1623 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 |  1624 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1625 | `				/* Unexpected expression */` |
|     ! 0 |  1626 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1627 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1628 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1629 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1630 | `				}` |
|     ! 0 |  1631 | `		}` |
|     132 |  1632 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1633 | `		/* Unexpected expression */` |
|       6 |  1634 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1635 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1636 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1637 | `			rc = SXERR_INVALID;` |
|       2 |  1638 | `		}` |
|       2 |  1639 | `	}` |
|     132 |  1640 | `	return rc;` |
|       4 |  1641 |  |
|       - |  1642 | `/*` |
|       - |  1643 | ` * Compile the 'list' language construct.` |
|       - |  1644 | ` *  According to the PHP language reference` |
|       - |  1645 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1646 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1647 | ` *  Description` |
|       - |  1648 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1649 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1650 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1651 | ` *  Parameters` |
|       - |  1652 | ` *   $varname: A variable.` |
|       - |  1653 | ` *  Return Values` |
|       - |  1654 | ` *   The assigned array.` |
|       - |  1655 | ` */` |
|       - |  1656 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1657 | `struct NestedListEntry {` |
|       - |  1658 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1659 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1660 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1661 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1662 | `};` |
|       - |  1663 | `/*` |
|       - |  1664 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1665 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1666 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1667 | ` */` |
|      74 |  1668 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       4 |  1669 |  |
|       - |  1670 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1671 | `	SyToken *pNext;` |
|       - |  1672 | `	sxi32 nExpr;` |
|       - |  1673 | `	sxi32 rc;` |
|      78 |  1674 | `	nExpr = 0;` |
|      78 |  1675 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     232 |  1676 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     158 |  1677 | `		if( pGen->pIn < pNext ){` |
|       - |  1678 | `			/* Check for nested list() */` |
|     146 |  1679 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1680 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1681 | `				/* Record this nested list for post-processing */` |
|       3 |  1682 | `				SyToken *pListEnd = 0;` |
|       3 |  1683 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1684 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1685 | `				}` |
|       3 |  1686 | `				if( pListEnd ){` |
|       - |  1687 | `					struct NestedListEntry sEntry;` |
|       3 |  1688 | `					sEntry.nIndex = nExpr;` |
|       3 |  1689 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1690 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1691 | `					sEntry.isShort = 0;` |
|       3 |  1692 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1693 | `				}` |
|       - |  1694 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1695 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     145 |  1696 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1697 | `				/* Nested short destructuring [...] */` |
|      13 |  1698 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1699 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1700 | `				if( pBracketEnd ){` |
|       - |  1701 | `					struct NestedListEntry sEntry;` |
|      13 |  1702 | `					sEntry.nIndex = nExpr;` |
|      13 |  1703 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1704 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1705 | `					sEntry.isShort = 1;` |
|      13 |  1706 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1707 | `				}` |
|       - |  1708 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1709 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1710 | `			}else{` |
|       - |  1711 | `				/* Compile the expression holding the variable */` |
|     132 |  1712 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     132 |  1713 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1714 | `					SySetRelease(&sNested);` |
|     ! 0 |  1715 | `					return SXRET_OK;` |
|       - |  1716 | `				}` |
|       - |  1717 | `			}` |
|      75 |  1718 | `		}else{` |
|       - |  1719 | `			/* Empty entry,load NULL */` |
|      13 |  1720 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1721 | `		}` |
|     158 |  1722 | `		nExpr++;` |
|       - |  1723 | `		/* Advance the stream cursor */` |
|     158 |  1724 | `		pGen->pIn = &pNext[1];` |
|       4 |  1725 | `	}` |
|       - |  1726 | `	/* Emit the LOAD_LIST instruction */` |
|      78 |  1727 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1728 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1729 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1730 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1731 | `	 */` |
|      78 |  1732 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1733 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1734 | `		sxu32 i;` |
|      27 |  1735 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1736 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1737 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1738 | `			ph7_value *pIdx;` |
|       - |  1739 | `			sxu32 nConstIdx;` |
|       - |  1740 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1741 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1742 | `			/* Push the integer index for this nested entry */` |
|      15 |  1743 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1744 | `			if( pIdx == 0 ){` |
|     ! 0 |  1745 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1746 | `				SySetRelease(&sNested);` |
|     ! 0 |  1747 | `				return SXERR_ABORT;` |
|       - |  1748 | `			}` |
|      15 |  1749 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1750 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1751 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1752 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1753 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1754 | `			 */` |
|      15 |  1755 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1756 | `			/* Recursively compile the inner list */` |
|      15 |  1757 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1758 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1759 | `			if( apNested[i].isShort ){` |
|      13 |  1760 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1761 | `			}else{` |
|       3 |  1762 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1763 | `			}` |
|      15 |  1764 | `			pGen->pIn = pSavedIn;` |
|      15 |  1765 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1766 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1767 | `				SySetRelease(&sNested);` |
|     ! 0 |  1768 | `				return SXERR_ABORT;` |
|       - |  1769 | `			}` |
|       - |  1770 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1771 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1772 | `		}` |
|       6 |  1773 | `	}` |
|      78 |  1774 | `	SySetRelease(&sNested);` |
|       - |  1775 | `	/* Node successfully compiled */` |
|      78 |  1776 | `	return SXRET_OK;` |
|      41 |  1777 |  |
|      32 |  1778 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1779 |  |
|       - |  1780 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 |  1781 | `	pGen->pIn += 2;` |
|      34 |  1782 | `	pGen->pEnd--;` |
|      16 |  1783 | `	SXUNUSED(iCompileFlag);` |
|      34 |  1784 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1785 |  |
|      42 |  1786 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       3 |  1787 |  |
|       - |  1788 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      45 |  1789 | `	pGen->pIn++;` |
|      45 |  1790 | `	pGen->pEnd--;` |
|      21 |  1791 | `	SXUNUSED(iCompileFlag);` |
|      45 |  1792 | `	return GenStateCompileListBody(pGen);` |
|       3 |  1793 |  |
|       - |  1794 | `/* Forward declarations */` |
|       - |  1795 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  1796 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  1797 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1798 | `/*` |
|       - |  1799 | ` * Compile an annoynmous function or a closure.` |
|       - |  1800 | ` * According to the PHP language reference` |
|       - |  1801 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1802 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1803 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1804 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1805 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1806 | ` *  Example Anonymous function variable assignment example` |
|       - |  1807 | ` * <?php` |
|       - |  1808 | ` * $greet = function($name)` |
|       - |  1809 | ` * {` |
|       - |  1810 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1811 | ` * };` |
|       - |  1812 | ` * $greet('World');` |
|       - |  1813 | ` * $greet('PHP');` |
|       - |  1814 | ` * ?>` |
|       - |  1815 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1816 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1817 | ` */` |
|     246 |  1818 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1819 |  |
|       - |  1820 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1821 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1822 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1823 | `							  * one thread is allowed to compile the script.` |
|       - |  1824 | `						      */` |
|       - |  1825 | `	ph7_value *pObj;` |
|       - |  1826 | `	SyString sName;` |
|       - |  1827 | `	sxu32 nIdx;` |
|       - |  1828 | `	sxu32 nLen;` |
|       - |  1829 | `	sxi32 rc;` |
|       - |  1830 |  |
|     251 |  1831 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     251 |  1832 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1833 | `		pGen->pIn++;` |
|     ! 0 |  1834 | `	}` |
|       - |  1835 | `	/* Reserve a constant for the lambda */` |
|     251 |  1836 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     251 |  1837 | `	if( pObj == 0 ){` |
|     ! 0 |  1838 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1839 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1840 | `		return SXERR_ABORT;` |
|       - |  1841 | `	}` |
|       - |  1842 | `	/* Generate a unique name */` |
|     251 |  1843 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1844 | `	/* Make sure the generated name is unique */` |
|     251 |  1845 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1846 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1847 | `	}` |
|     251 |  1848 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     251 |  1849 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - |  1850 | `	/* Compile the lambda body */` |
|     251 |  1851 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     251 |  1852 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1853 | `		return SXERR_ABORT;` |
|       - |  1854 | `	}` |
|     251 |  1855 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - |  1856 | `		/* Emit the load closure instruction */` |
|      21 |  1857 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|      13 |  1858 | `	}else{` |
|       - |  1859 | `		/* Emit the load constant instruction */` |
|     235 |  1860 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1861 | `	}` |
|       - |  1862 | `	/* Node successfully compiled */` |
|     251 |  1863 | `	return SXRET_OK;` |
|     128 |  1864 |  |
|       - |  1865 | `/*` |
|       - |  1866 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1867 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1868 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1869 | ` */` |
|     150 |  1870 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  1871 | `	ph7_gen_state *pGen,` |
|       - |  1872 | `	ph7_vm_func *pFunc,` |
|       - |  1873 | `	const char *zName,` |
|       - |  1874 | `	sxu32 nByte,` |
|       - |  1875 | `	SyString *aShadow,` |
|       - |  1876 | `	sxu32 nShadow)` |
|       2 |  1877 |  |
|       - |  1878 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  1879 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  1880 | `	sxu32 n, nEnv;` |
|       - |  1881 | `	char *zDup;` |
|     152 |  1882 | `	if( nByte == 0 ){` |
|     ! 0 |  1883 | `		return SXRET_OK;` |
|       - |  1884 | `	}` |
|     150 |  1885 | `	if( nByte == sizeof("this")-1` |
|      81 |  1886 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  1887 | `		return SXRET_OK;` |
|       - |  1888 | `	}` |
|     182 |  1889 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     128 |  1890 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     125 |  1891 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      98 |  1892 | `			return SXRET_OK;` |
|       - |  1893 | `		}` |
|      17 |  1894 | `	}` |
|      53 |  1895 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  1896 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  1897 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  1898 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  1899 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  1900 | `			return SXRET_OK;` |
|       - |  1901 | `		}` |
|      15 |  1902 | `	}` |
|      53 |  1903 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  1904 | `	if( zDup == 0 ){` |
|     ! 0 |  1905 | `		return SXERR_ABORT;` |
|       - |  1906 | `	}` |
|      53 |  1907 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  1908 | `	sEnv.iFlags = 0;` |
|      53 |  1909 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  1910 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  1911 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  1912 | `	return SXRET_OK;` |
|      77 |  1913 |  |
|       - |  1914 | `/*` |
|       - |  1915 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  1916 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  1917 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  1918 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  1919 | ` */` |
|      14 |  1920 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  1921 | `	ph7_gen_state *pGen,` |
|       - |  1922 | `	ph7_vm_func *pFunc,` |
|       - |  1923 | `	const char *zIn,` |
|       - |  1924 | `	const char *zEnd,` |
|       - |  1925 | `	SyString *aShadow,` |
|       - |  1926 | `	sxu32 nShadow)` |
|       1 |  1927 |  |
|       - |  1928 | `	sxi32 rc;` |
|     159 |  1929 | `	while( zIn < zEnd ){` |
|     145 |  1930 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  1931 | `			zIn++;` |
|     ! 0 |  1932 | `			if( zIn < zEnd ){` |
|     ! 0 |  1933 | `				zIn++;` |
|     ! 0 |  1934 | `			}` |
|     ! 0 |  1935 | `			continue;` |
|       - |  1936 | `		}` |
|     144 |  1937 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  1938 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  1939 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  1940 | `			const char *zName;` |
|      13 |  1941 | `			zIn++; /* skip '$' */` |
|      13 |  1942 | `			zName = zIn;` |
|      39 |  1943 | `			while( zIn < zEnd ){` |
|      35 |  1944 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  1945 | `				if( c >= 0xc0 ){` |
|     ! 0 |  1946 | `					zIn++;` |
|     ! 0 |  1947 | `					while( zIn < zEnd` |
|     ! 0 |  1948 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1949 | `						zIn++;` |
|     ! 0 |  1950 | `					}` |
|     ! 0 |  1951 | `					continue;` |
|       - |  1952 | `				}` |
|      35 |  1953 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  1954 | `					break;` |
|       - |  1955 | `				}` |
|      27 |  1956 | `				zIn++;` |
|       1 |  1957 | `			}` |
|      13 |  1958 | `			if( zIn > zName ){` |
|      19 |  1959 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  1960 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  1961 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1962 | `					return SXERR_ABORT;` |
|       - |  1963 | `				}` |
|       6 |  1964 | `			}` |
|      13 |  1965 | `			continue;` |
|       - |  1966 | `		}` |
|     133 |  1967 | `		zIn++;` |
|       1 |  1968 | `	}` |
|      15 |  1969 | `	return SXRET_OK;` |
|       8 |  1970 |  |
|       - |  1971 | `/*` |
|       - |  1972 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  1973 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  1974 | ` *   - plain $<id> pairs` |
|       - |  1975 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  1976 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  1977 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  1978 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  1979 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  1980 | ` *     are never mistakenly captured.` |
|       - |  1981 | ` */` |
|     136 |  1982 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  1983 | `	ph7_gen_state *pGen,` |
|       - |  1984 | `	ph7_vm_func *pFunc,` |
|       - |  1985 | `	SyToken *pStart,` |
|       - |  1986 | `	SyToken *pEnd,` |
|       - |  1987 | `	SyString *aShadow,` |
|       - |  1988 | `	sxu32 nShadow)` |
|       2 |  1989 |  |
|     138 |  1990 | `	SyToken *pScan = pStart;` |
|       - |  1991 | `	sxi32 rc;` |
|     512 |  1992 | `	while( pScan < pEnd ){` |
|     376 |  1993 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 |  1994 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 |  1995 | `				pScan->sData.zString,` |
|      14 |  1996 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 |  1997 | `				aShadow,nShadow);` |
|      15 |  1998 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1999 | `				return SXERR_ABORT;` |
|       - |  2000 | `			}` |
|      15 |  2001 | `			pScan++;` |
|      15 |  2002 | `			continue;` |
|       - |  2003 | `		}` |
|     362 |  2004 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  2005 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  2006 | `			SyToken *pFnKw = pScan;` |
|      20 |  2007 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2008 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  2009 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2010 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2011 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2012 | `			}` |
|      21 |  2013 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2014 | `				SyToken *pInnerSigStart;` |
|       - |  2015 | `				SyToken *pInnerSigEnd;` |
|       - |  2016 | `				SyToken *pInnerBodyEnd;` |
|       - |  2017 | `				SyString *aInnerShadow;` |
|       - |  2018 | `				sxu32 nInnerShadow;` |
|       - |  2019 | `				sxu32 nInnerParamMax;` |
|       - |  2020 | `				SyToken *p;` |
|       - |  2021 | `				int iNestInner;` |
|      19 |  2022 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2023 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2024 | `					pScan++;` |
|     ! 0 |  2025 | `				}` |
|      19 |  2026 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2027 | `					pScan++;` |
|     ! 0 |  2028 | `					continue;` |
|       - |  2029 | `				}` |
|      19 |  2030 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2031 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2032 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2033 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2034 | `					pScan = pEnd;` |
|     ! 0 |  2035 | `					continue;` |
|       - |  2036 | `				}` |
|       - |  2037 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2038 | `				nInnerParamMax = 0;` |
|      57 |  2039 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2040 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2041 | `						nInnerParamMax++;` |
|       6 |  2042 | `					}` |
|      20 |  2043 | `				}` |
|      19 |  2044 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2045 | `					&pGen->pVm->sAllocator,` |
|      18 |  2046 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2047 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2048 | `					return SXERR_ABORT;` |
|       - |  2049 | `				}` |
|      19 |  2050 | `				nInnerShadow = 0;` |
|      25 |  2051 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2052 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2053 | `				}` |
|      57 |  2054 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2055 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2056 | `						continue;` |
|       - |  2057 | `					}` |
|      13 |  2058 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2059 | `						break;` |
|       - |  2060 | `					}` |
|      13 |  2061 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2062 | `						continue;` |
|       - |  2063 | `					}` |
|      13 |  2064 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2065 | `				}` |
|      19 |  2066 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2067 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2068 | `					pScan++;` |
|     ! 0 |  2069 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2070 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2071 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2072 | `						pScan++;` |
|     ! 0 |  2073 | `					}` |
|     ! 0 |  2074 | `					if( pScan < pEnd` |
|     ! 0 |  2075 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2076 | `						pScan++;` |
|     ! 0 |  2077 | `					}` |
|     ! 0 |  2078 | `				}` |
|      19 |  2079 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2080 | `					pScan++; /* past '=>' */` |
|       9 |  2081 | `				}` |
|      19 |  2082 | `				pInnerBodyEnd = pScan;` |
|      19 |  2083 | `				iNestInner = 0;` |
|     131 |  2084 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2085 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2086 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2087 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2088 | `						break;` |
|       - |  2089 | `					}` |
|     113 |  2090 | `					if( pInnerBodyEnd->nType &` |
|       - |  2091 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2092 | `						iNestInner++;` |
|     112 |  2093 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2094 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2095 | `						iNestInner--;` |
|       1 |  2096 | `					}` |
|     113 |  2097 | `					pInnerBodyEnd++;` |
|       1 |  2098 | `				}` |
|       - |  2099 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2100 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2101 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2102 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2103 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2104 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2105 | `				 *` |
|       - |  2106 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2107 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2108 | `				 * range after the '=' sign. */` |
|       - |  2109 | `				{` |
|      19 |  2110 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2111 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2112 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2113 | `						SyToken *pEq = 0;` |
|      13 |  2114 | `						int iNestArg = 0;` |
|      49 |  2115 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2116 | `							if( iNestArg == 0` |
|      39 |  2117 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2118 | `								break;` |
|       - |  2119 | `							}` |
|      37 |  2120 | `							if( pArgEnd->nType &` |
|       - |  2121 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2122 | `								iNestArg++;` |
|      37 |  2123 | `							}else if( pArgEnd->nType &` |
|       - |  2124 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2125 | `								iNestArg--;` |
|     ! 0 |  2126 | `							}` |
|      36 |  2127 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2128 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2129 | `								pEq = pArgEnd;` |
|       3 |  2130 | `							}` |
|      37 |  2131 | `							pArgEnd++;` |
|       1 |  2132 | `						}` |
|      13 |  2133 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2134 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2135 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2136 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2137 | `								return SXERR_ABORT;` |
|       - |  2138 | `							}` |
|       3 |  2139 | `						}` |
|      13 |  2140 | `						pArgStart = pArgEnd;` |
|      12 |  2141 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2142 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2143 | `							pArgStart++;` |
|       1 |  2144 | `						}` |
|       1 |  2145 | `					}` |
|       - |  2146 | `				}` |
|      28 |  2147 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2148 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2149 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2150 | `					return SXERR_ABORT;` |
|       - |  2151 | `				}` |
|      19 |  2152 | `				pScan = pInnerBodyEnd;` |
|      19 |  2153 | `				continue;` |
|       - |  2154 | `			}` |
|       1 |  2155 | `		}` |
|     344 |  2156 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     206 |  2157 | `			pScan++;` |
|     206 |  2158 | `			continue;` |
|       - |  2159 | `		}` |
|       - |  2160 | `		{` |
|       - |  2161 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     140 |  2162 | `			SyToken *pDollar = pScan;` |
|     207 |  2163 | `			while( &pDollar[1] < pEnd` |
|     140 |  2164 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2165 | `				pDollar++;` |
|     ! 0 |  2166 | `			}` |
|     140 |  2167 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2168 | `				break;` |
|       - |  2169 | `			}` |
|     140 |  2170 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2171 | `				pScan = pDollar + 1;` |
|     ! 0 |  2172 | `				continue;` |
|       - |  2173 | `			}` |
|     209 |  2174 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     138 |  2175 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      69 |  2176 | `				aShadow,nShadow);` |
|     140 |  2177 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2178 | `				return SXERR_ABORT;` |
|       - |  2179 | `			}` |
|     140 |  2180 | `			pScan = pDollar + 2;` |
|       - |  2181 | `		}` |
|       2 |  2182 | `	}` |
|     138 |  2183 | `	return SXRET_OK;` |
|      70 |  2184 |  |
|       - |  2185 | `/*` |
|       - |  2186 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2187 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2188 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2189 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2190 | ` * $this is also made available.` |
|       - |  2191 | ` */` |
|     118 |  2192 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2193 |  |
|       - |  2194 | `	ph7_vm_func *pFunc;` |
|       - |  2195 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2196 | `	GenBlock *pBlock;` |
|       - |  2197 | `	SySet *pInstrContainer;` |
|       - |  2198 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2199 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2200 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2201 | `	SyToken *pSavedEnd;` |
|       - |  2202 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2203 | `	char zName[512];` |
|       - |  2204 | `	static int iCnt = 1;` |
|       - |  2205 | `	char *zDup;` |
|       - |  2206 | `	sxu32 nLen;` |
|       - |  2207 | `	sxu32 nLine;` |
|     122 |  2208 | `	sxi32 iFlags = 0;` |
|     122 |  2209 | `	int bStatic = 0;` |
|       - |  2210 | `	sxi32 rc;` |
|       - |  2211 | `	sxu32 n;` |
|      59 |  2212 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2213 |  |
|     122 |  2214 | `	nLine = pGen->pIn->nLine;` |
|       - |  2215 | `	/* Optional 'static' prefix */` |
|     118 |  2216 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     122 |  2217 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2218 | `		bStatic = 1;` |
|       3 |  2219 | `		pGen->pIn++;` |
|       1 |  2220 | `	}` |
|       - |  2221 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     118 |  2222 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     122 |  2223 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2224 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2225 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2226 | `		return SXERR_SYNTAX;` |
|       - |  2227 | `	}` |
|     122 |  2228 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2229 | `	/* Optional '&' — return by reference */` |
|     122 |  2230 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2231 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2232 | `		pGen->pIn++;` |
|     ! 0 |  2233 | `	}` |
|       - |  2234 | `	/* Expect '(' */` |
|     122 |  2235 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2236 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2237 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2238 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2239 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2240 | `		}else{` |
|     ! 0 |  2241 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2242 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2243 | `		}` |
|       3 |  2244 | `		return SXERR_SYNTAX;` |
|       - |  2245 | `	}` |
|     119 |  2246 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2247 | `	/* Delimit the parameter list */` |
|     119 |  2248 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     119 |  2249 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2250 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2251 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2252 | `		return SXERR_SYNTAX;` |
|       - |  2253 | `	}` |
|       - |  2254 | `	/* Allocate the function state */` |
|     117 |  2255 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     117 |  2256 | `	if( pFunc == 0 ){` |
|     ! 0 |  2257 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2258 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2259 | `		return SXERR_ABORT;` |
|       - |  2260 | `	}` |
|       - |  2261 | `	/* Generate a unique lambda name */` |
|     117 |  2262 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     215 |  2263 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     100 |  2264 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2265 | `	}` |
|     117 |  2266 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     117 |  2267 | `	if( zDup == 0 ){` |
|     ! 0 |  2268 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2269 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2270 | `		return SXERR_ABORT;` |
|       - |  2271 | `	}` |
|     117 |  2272 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2273 | `	/* Collect function arguments */` |
|     117 |  2274 | `	if( pGen->pIn < pSigEnd ){` |
|      87 |  2275 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      87 |  2276 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2277 | `			return SXERR_ABORT;` |
|       - |  2278 | `		}` |
|      42 |  2279 | `	}` |
|       - |  2280 | `	/* Point past ')' and parse optional return type */` |
|     117 |  2281 | `	pGen->pIn = &pSigEnd[1];` |
|     117 |  2282 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     117 |  2283 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2284 | `		return SXERR_ABORT;` |
|     117 |  2285 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2286 | `		return SXERR_SYNTAX;` |
|       - |  2287 | `	}` |
|       - |  2288 | `	/* Expect '=>' */` |
|     117 |  2289 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2290 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2291 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2292 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2293 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2294 | `		}else{` |
|     ! 0 |  2295 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2296 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2297 | `		}` |
|       3 |  2298 | `		return SXERR_SYNTAX;` |
|       - |  2299 | `	}` |
|     114 |  2300 | `	pGen->pIn++; /* Jump '=>' */` |
|     114 |  2301 | `	pBodyStart = pGen->pIn;` |
|     114 |  2302 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2303 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2304 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2305 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2306 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     114 |  2307 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2308 | `	{` |
|     114 |  2309 | `		SyString *aShadow = 0;` |
|     114 |  2310 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     114 |  2311 | `		if( nShadow > 0 ){` |
|      84 |  2312 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      82 |  2313 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      84 |  2314 | `			if( aShadow == 0 ){` |
|     ! 0 |  2315 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2316 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2317 | `				return SXERR_ABORT;` |
|       - |  2318 | `			}` |
|     184 |  2319 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     102 |  2320 | `				aShadow[n] = aArgs[n].sName;` |
|      52 |  2321 | `			}` |
|      41 |  2322 | `		}` |
|     170 |  2323 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      56 |  2324 | `			aShadow,nShadow);` |
|     114 |  2325 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2326 | `			return SXERR_ABORT;` |
|       - |  2327 | `		}` |
|       - |  2328 | `	}` |
|       - |  2329 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2330 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2331 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2332 | `	 * $this. */` |
|     114 |  2333 | `	if( !bStatic ){` |
|       - |  2334 | `		char *zThisDup;` |
|     112 |  2335 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     112 |  2336 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2337 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2338 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2339 | `			return SXERR_ABORT;` |
|       - |  2340 | `		}` |
|     112 |  2341 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     112 |  2342 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     112 |  2343 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     112 |  2344 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     112 |  2345 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      55 |  2346 | `	}` |
|       - |  2347 | `	/* Arrow functions are always closures */` |
|     114 |  2348 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2349 | `	/* Compile the body expression as an implicit return */` |
|     170 |  2350 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      56 |  2351 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     114 |  2352 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2353 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2354 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2355 | `		return SXERR_ABORT;` |
|       - |  2356 | `	}` |
|     114 |  2357 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     114 |  2358 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     114 |  2359 | `	pSavedEnd = pGen->pEnd;` |
|     114 |  2360 | `	pGen->pIn = pBodyStart;` |
|     114 |  2361 | `	pGen->pEnd = pBodyEnd;` |
|     114 |  2362 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     114 |  2363 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2364 | `		return SXERR_ABORT;` |
|       - |  2365 | `	}` |
|       - |  2366 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2367 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2368 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2369 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     114 |  2370 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     114 |  2371 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     114 |  2372 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     114 |  2373 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     114 |  2374 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2375 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     114 |  2376 | `	pGen->pIn = pBodyEnd;` |
|     114 |  2377 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2378 | `	/* Emit the load-closure instruction */` |
|     114 |  2379 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     114 |  2380 | `	return SXRET_OK;` |
|      63 |  2381 |  |
|       - |  2382 | `/*` |
|       - |  2383 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2384 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2385 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2386 | ` * expression's value.` |
|       - |  2387 | ` */` |
|     346 |  2388 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2389 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2390 |  |
|       - |  2391 | `	SySet *pInstrContainer;` |
|       - |  2392 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2393 | `	GenBlock *pArmBlock;` |
|       - |  2394 | `	sxi32 rc;` |
|     349 |  2395 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2396 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2397 | `	pGen->pIn  = pStart;` |
|     349 |  2398 | `	pGen->pEnd = pStop;` |
|     349 |  2399 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2400 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2401 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2402 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2403 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2404 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2405 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2406 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2407 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2408 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2409 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2410 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2411 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2412 | `		return SXERR_ABORT;` |
|       - |  2413 | `	}` |
|     349 |  2414 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2415 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2416 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2417 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2418 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2419 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2420 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2421 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2422 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2423 | `		return SXERR_ABORT;` |
|       - |  2424 | `	}` |
|     349 |  2425 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2426 | `		return SXERR_EMPTY;` |
|       - |  2427 | `	}` |
|     349 |  2428 | `	return SXRET_OK;` |
|     176 |  2429 |  |
|       - |  2430 | `/*` |
|       - |  2431 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2432 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2433 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2434 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2435 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2436 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2437 | ` */` |
|       - |  2438 | `/*` |
|       - |  2439 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2440 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2441 | ` * caller can bail out of the current expression.` |
|       - |  2442 | ` */` |
|       2 |  2443 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2444 |  |
|       - |  2445 | `	va_list ap;` |
|       - |  2446 | `	sxi32 rc;` |
|       - |  2447 | `	SyBlob sMsg;` |
|       3 |  2448 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2449 | `	va_start(ap,zFmt);` |
|       3 |  2450 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2451 | `	va_end(ap);` |
|       3 |  2452 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2453 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2454 | `	SyBlobRelease(&sMsg);` |
|       3 |  2455 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2456 | `		return SXERR_ABORT;` |
|       - |  2457 | `	}` |
|       3 |  2458 | `	return SXERR_SYNTAX;` |
|       2 |  2459 |  |
|       - |  2460 | `/*` |
|       - |  2461 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2462 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2463 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2464 | ` */` |
|     348 |  2465 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2466 |  |
|     352 |  2467 | `	SyToken *pCur = pStart;` |
|     352 |  2468 | `	int iNest = 0;` |
|     814 |  2469 | `	while( pCur < pEnd ){` |
|     780 |  2470 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2471 | `			iNest++;` |
|     774 |  2472 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2473 | `			iNest--;` |
|     762 |  2474 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2475 | `			return pCur;` |
|       - |  2476 | `		}` |
|     466 |  2477 | `		pCur++;` |
|       4 |  2478 | `	}` |
|      37 |  2479 | `	return pEnd;` |
|     178 |  2480 |  |
|      70 |  2481 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2482 |  |
|       - |  2483 | `	ph7_match *pMatch;` |
|       - |  2484 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2485 | `	int bHasDefault = 0;` |
|       - |  2486 | `	sxu32 nLine;` |
|       - |  2487 | `	sxi32 rc;` |
|      35 |  2488 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2489 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2490 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2491 | `	/* Expect '(' */` |
|      75 |  2492 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2493 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2494 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2495 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2496 | `	}` |
|      75 |  2497 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2498 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2499 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2500 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2501 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2502 | `	}` |
|      75 |  2503 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2504 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2505 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2506 | `	}` |
|       - |  2507 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2508 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2509 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2510 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2511 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2512 | `		return SXERR_ABORT;` |
|       - |  2513 | `	}` |
|      75 |  2514 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2515 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2516 | `	/* Expect '{' */` |
|      75 |  2517 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2518 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2519 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2520 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2521 | `	}` |
|      75 |  2522 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2523 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2524 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2525 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2526 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2527 | `	}` |
|       - |  2528 | `	/* Allocate ph7_match container */` |
|      75 |  2529 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2530 | `	if( pMatch == 0 ){` |
|     ! 0 |  2531 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2532 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2533 | `		return SXERR_ABORT;` |
|       - |  2534 | `	}` |
|      75 |  2535 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2536 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2537 | `	/* Iterate arms */` |
|     253 |  2538 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2539 | `		ph7_match_arm sArm;` |
|       - |  2540 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2541 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2542 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2543 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2544 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2545 | `		/* 'default' arm? */` |
|     182 |  2546 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2547 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2548 | `			if( bHasDefault ){` |
|       3 |  2549 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2550 | `					"Match expressions may only contain one default arm");` |
|       4 |  2551 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2552 | `			}` |
|      20 |  2553 | `			sArm.bDefault = 1;` |
|      20 |  2554 | `			bHasDefault = 1;` |
|      20 |  2555 | `			pGen->pIn++;` |
|      20 |  2556 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2557 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2558 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2559 | `			}` |
|      20 |  2560 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2561 | `		}else{` |
|       - |  2562 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2563 | `			pCondStart = pGen->pIn;` |
|     166 |  2564 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2565 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2566 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2567 | `				SySet sCondBc;` |
|       9 |  2568 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2569 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2570 | `						"syntax error, empty match condition expression");` |
|       - |  2571 | `				}` |
|       9 |  2572 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2573 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2574 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2575 | `					return SXERR_ABORT;` |
|       - |  2576 | `				}` |
|       9 |  2577 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2578 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2579 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2580 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2581 | `			}` |
|     166 |  2582 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2583 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2584 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2585 | `			}` |
|     163 |  2586 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2587 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2588 | `					"syntax error, empty match condition expression");` |
|       - |  2589 | `			}` |
|       - |  2590 | `			{` |
|       - |  2591 | `				SySet sCondBc;` |
|     163 |  2592 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2593 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2594 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2595 | `					return SXERR_ABORT;` |
|       - |  2596 | `				}` |
|     163 |  2597 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2598 | `			}` |
|     163 |  2599 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2600 | `		}` |
|       - |  2601 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2602 | `		pResStart = pGen->pIn;` |
|     181 |  2603 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2604 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2605 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2606 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2607 | `		}` |
|     181 |  2608 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2609 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2610 | `			return SXERR_ABORT;` |
|       - |  2611 | `		}` |
|     181 |  2612 | `		pGen->pIn = pResEnd;` |
|     181 |  2613 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2614 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2615 | `		}` |
|     181 |  2616 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2617 | `	}` |
|      69 |  2618 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2619 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2620 | `	return SXRET_OK;` |
|      40 |  2621 |  |
|       - |  2622 | `/*` |
|       - |  2623 | ` * Compile a backtick quoted string.` |
|       - |  2624 | ` */` |
|       4 |  2625 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2626 |  |
|       - |  2627 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2628 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2629 | `	 */` |
|       8 |  2630 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2631 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2632 | `		ph7_lib_version()` |
|       - |  2633 | `		);` |
|       - |  2634 | `	/* Load NULL */` |
|       6 |  2635 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2636 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2637 | `	/* Node successfully compiled */` |
|       6 |  2638 | `	return SXRET_OK;` |
|       2 |  2639 |  |
|       - |  2640 | `/*` |
|       - |  2641 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2642 | ` * construct.` |
|       - |  2643 | ` */` |
|      80 |  2644 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2645 |  |
|       - |  2646 | `	SyString *pName;` |
|       - |  2647 | `	sxu32 nKeyID;` |
|       - |  2648 | `	sxi32 rc;` |
|       - |  2649 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      85 |  2650 | `	pName = &pGen->pIn->sData;` |
|      85 |  2651 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  2652 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      85 |  2653 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2654 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2655 | `		/* Compile arguments one after one */` |
|       9 |  2656 | `		pTmp = pGen->pEnd;` |
|       - |  2657 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2658 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2659 | `		 *  mean that the following expression is valid:` |
|       - |  2660 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2661 | `		 */` |
|       9 |  2662 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2663 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2664 | `			if( pGen->pIn < pNext ){` |
|       9 |  2665 | `				pGen->pEnd = pNext;` |
|       9 |  2666 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2667 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2668 | `					return SXERR_ABORT;` |
|       - |  2669 | `				}` |
|       9 |  2670 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2671 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2672 | `					 * without the overhead of a function call.` |
|       - |  2673 | `					 * This is a very powerful optimization that improve` |
|       - |  2674 | `					 * performance greatly.` |
|       - |  2675 | `					 */` |
|       9 |  2676 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2677 | `				}` |
|       4 |  2678 | `			}` |
|       - |  2679 | `			/* Jump trailing commas */` |
|       9 |  2680 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2681 | `				pNext++;` |
|     ! 0 |  2682 | `			}` |
|       9 |  2683 | `			pGen->pIn = pNext;` |
|       1 |  2684 | `		}` |
|       - |  2685 | `		/* Restore token stream */` |
|       9 |  2686 | `		pGen->pEnd = pTmp;` |
|       5 |  2687 | `	}else{` |
|      77 |  2688 | `		sxi32 nArg = 0;` |
|      77 |  2689 | `		sxu32 nIdx = 0;` |
|      77 |  2690 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      77 |  2691 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2692 | `			return SXERR_ABORT;` |
|      77 |  2693 | `		}else if(rc != SXERR_EMPTY ){` |
|      77 |  2694 | `			nArg = 1;` |
|      36 |  2695 | `		}` |
|      77 |  2696 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2697 | `			ph7_value *pObj;` |
|       - |  2698 | `			/* Emit the call instruction */` |
|      29 |  2699 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  2700 | `			if( pObj == 0 ){` |
|     ! 0 |  2701 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2702 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2703 | `				return SXERR_ABORT;` |
|       - |  2704 | `			}` |
|      29 |  2705 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2706 | `			/* Install in the literal table */` |
|      29 |  2707 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      12 |  2708 | `		}` |
|       - |  2709 | `		/* Emit the call instruction */` |
|      77 |  2710 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      77 |  2711 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2712 | `	}` |
|       - |  2713 | `	/* Node successfully compiled */` |
|      85 |  2714 | `	return SXRET_OK;` |
|      45 |  2715 |  |
|       - |  2716 | `/*` |
|       - |  2717 | ` * Compile a node holding a variable declaration.` |
|       - |  2718 | ` * According to the PHP language reference` |
|       - |  2719 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2720 | ` *  The variable name is case-sensitive.` |
|       - |  2721 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2722 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2723 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2724 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2725 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2726 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2727 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2728 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2729 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2730 | ` *  the chapter on Expressions.` |
|       - |  2731 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2732 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2733 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2734 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2735 | ` *  is being assigned (the source variable).` |
|       - |  2736 | ` */` |
|  987102 |  2737 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2738 |  |
|  987107 |  2739 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2740 | `	sxi32 iVv;` |
|       - |  2741 | `	sxi32 iP1;` |
|       - |  2742 | `	void *p3;` |
|       - |  2743 | `	sxi32 rc;` |
|  987107 |  2744 | `	iVv = -1; /* Variable variable counter */` |
| 1974221 |  2745 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  987119 |  2746 | `		pGen->pIn++;` |
|  987119 |  2747 | `		iVv++;` |
|       5 |  2748 | `	}` |
|  987107 |  2749 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2750 | `		/* Invalid variable name */` |
|     ! 0 |  2751 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2752 | `		if( rc == SXERR_ABORT ){` |
|       - |  2753 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2754 | `			return SXERR_ABORT;` |
|       - |  2755 | `		}` |
|     ! 0 |  2756 | `		return SXRET_OK;` |
|       - |  2757 | `	}` |
|  987107 |  2758 | `	p3  = 0;` |
|  987107 |  2759 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2760 | `		/* Dynamic variable creation */` |
|      19 |  2761 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  2762 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  2763 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2764 | `			/* Empty expression */` |
|       3 |  2765 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2766 | `			return SXRET_OK;` |
|       - |  2767 | `		}` |
|       - |  2768 | `		/* Compile the expression holding the variable name */` |
|      16 |  2769 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2770 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2771 | `			return SXERR_ABORT;` |
|      16 |  2772 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2773 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2774 | `			return SXRET_OK;` |
|       - |  2775 | `		}` |
|       7 |  2776 | `	}else{` |
|       - |  2777 | `		SyHashEntry *pEntry;` |
|       - |  2778 | `		SyString *pName;` |
|  987091 |  2779 | `		char *zName = 0;` |
|       - |  2780 | `		/* Extract variable name */` |
|  987091 |  2781 | `		pName = &pGen->pIn->sData;` |
|       - |  2782 | `		/* Advance the stream cursor */` |
|  987091 |  2783 | `		pGen->pIn++;` |
|  987091 |  2784 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  987091 |  2785 | `		if( pEntry == 0 ){` |
|       - |  2786 | `			/* Duplicate name */` |
|  132417 |  2787 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  132417 |  2788 | `			if( zName == 0 ){` |
|     ! 0 |  2789 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2790 | `				return SXERR_ABORT;` |
|       - |  2791 | `			}` |
|       - |  2792 | `			/* Install in the hashtable */` |
|  132417 |  2793 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   66211 |  2794 | `		}else{` |
|       - |  2795 | `			/* Name already available */` |
|  854679 |  2796 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2797 | `		}` |
|  987091 |  2798 | `		p3 = (void *)zName;` |
|       - |  2799 | `	}` |
|  987103 |  2800 | `	iP1 = 0;` |
|  987103 |  2801 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  359731 |  2802 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2803 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  359713 |  2804 | `			iP1 = 1;` |
|  179854 |  2805 | `		}` |
|  179863 |  2806 | `	}` |
|       - |  2807 | `	/* Emit the load instruction */` |
|  987103 |  2808 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  987115 |  2809 | `	while( iVv > 0 ){` |
|      13 |  2810 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2811 | `		iVv--;` |
|       1 |  2812 | `	}` |
|       - |  2813 | `	/* Node successfully compiled */` |
|  987103 |  2814 | `	return SXRET_OK;` |
|  493556 |  2815 |  |
|       - |  2816 | `/*` |
|       - |  2817 | ` * Load a literal.` |
|       - |  2818 | ` */` |
|  693904 |  2819 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2820 |  |
|  693909 |  2821 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2822 | `	ph7_value *pObj;` |
|       - |  2823 | `	SyString *pStr;` |
|       - |  2824 | `	sxu32 nIdx;` |
|       - |  2825 | `	/* Extract token value */` |
|  693909 |  2826 | `	pStr = &pToken->sData;` |
|       - |  2827 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  693909 |  2828 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  147087 |  2829 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2830 | `			/* NULL constant are always indexed at 0 */` |
|   54197 |  2831 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   54197 |  2832 | `			return SXRET_OK;` |
|   92895 |  2833 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2834 | `			/* TRUE constant are always indexed at 1 */` |
|     595 |  2835 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     595 |  2836 | `			return SXRET_OK;` |
|       5 |  2837 | `		}` |
|  647967 |  2838 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  109980 |  2839 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2840 | `			/* FALSE constant are always indexed at 2 */` |
|   41561 |  2841 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   41561 |  2842 | `			return SXRET_OK;` |
|  554579 |  2843 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   98616 |  2844 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2845 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    9455 |  2846 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    9455 |  2847 | `			if( pObj == 0 ){` |
|     ! 0 |  2848 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2849 | `				return SXERR_ABORT;` |
|       - |  2850 | `			}` |
|    9455 |  2851 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2852 | `			/* Emit the load constant instruction */` |
|    9455 |  2853 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    9455 |  2854 | `			return SXRET_OK;` |
|  511752 |  2855 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   31862 |  2856 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2857 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       8 |  2858 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       8 |  2859 | `			if( pObj == 0 ){` |
|     ! 0 |  2860 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2861 | `				return SXERR_ABORT;` |
|       - |  2862 | `			}` |
|       8 |  2863 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2864 | `				SyString sNs;` |
|       8 |  2865 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  2866 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       5 |  2867 | `			}else{` |
|     ! 0 |  2868 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2869 | `			}` |
|       8 |  2870 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       8 |  2871 | `			return SXRET_OK;` |
|  510884 |  2872 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   13347 |  2873 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  504206 |  2874 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   16806 |  2875 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  2876 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  2877 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  2878 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  2879 | `				/* Point to the upper block */` |
|      11 |  2880 | `				pBlock = pBlock->pParent;` |
|       1 |  2881 | `			}` |
|      11 |  2882 | `			if( pBlock == 0 ){` |
|       - |  2883 | `				/* Called in the global scope,load NULL */` |
|       5 |  2884 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  2885 | `			}else{` |
|       - |  2886 | `				/* Extract the target function/method */` |
|       7 |  2887 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  2888 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  2889 | `					/* Not a class method,Load null */` |
|       3 |  2890 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2891 | `				}else{` |
|       5 |  2892 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  2893 | `					if( pObj == 0 ){` |
|     ! 0 |  2894 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2895 | `						return SXERR_ABORT;` |
|       - |  2896 | `					}` |
|       5 |  2897 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  2898 | `					/* Emit the load constant instruction */` |
|       5 |  2899 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  2900 | `				}` |
|       - |  2901 | `			}` |
|      11 |  2902 | `			return SXRET_OK;` |
|       - |  2903 | `	}` |
|       - |  2904 | `	/* Query literal table */` |
|  588105 |  2905 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2906 | `		ph7_value *pLitObj;` |
|       - |  2907 | `		/* Unknown literal,install it in the literal table */` |
|  244137 |  2908 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  244137 |  2909 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2910 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2911 | `			return SXERR_ABORT;` |
|       - |  2912 | `		}` |
|  244137 |  2913 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  244137 |  2914 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  122066 |  2915 | `	}` |
|       - |  2916 | `	/* Emit the load constant instruction */` |
|  588105 |  2917 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  588105 |  2918 | `	return SXRET_OK;` |
|  346957 |  2919 |  |
|       - |  2920 | `/*` |
|       - |  2921 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2922 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2923 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2924 | ` * Otherwise, load the simple literal directly.` |
|       - |  2925 | ` */` |
|  693940 |  2926 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  2927 |  |
|       - |  2928 | `	sxi32 rc;` |
|  693945 |  2929 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2930 | `		return SXRET_OK;` |
|       - |  2931 | `	}` |
|       - |  2932 | `	/* Check if this is a multi-token namespace path */` |
|  693945 |  2933 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  2934 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      40 |  2935 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      40 |  2936 | `		int isAbsolute = 0;` |
|      40 |  2937 | `		SyBlobReset(pWorker);` |
|       - |  2938 | `		/* Check for leading backslash (absolute path) */` |
|      40 |  2939 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      38 |  2940 | `			isAbsolute = 1;` |
|      38 |  2941 | `			pGen->pIn++; /* Skip leading backslash */` |
|      17 |  2942 | `		}` |
|       - |  2943 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      40 |  2944 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  2945 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  2946 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  2947 | `		}` |
|       - |  2948 | `		/* Collect all path components */` |
|     136 |  2949 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     136 |  2950 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      52 |  2951 | `				SyBlobAppend(pWorker,"\\",1);` |
|      28 |  2952 | `			}else{` |
|      88 |  2953 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  2954 | `			}` |
|     136 |  2955 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      40 |  2956 | `				pGen->pIn++;` |
|      40 |  2957 | `				break;` |
|       - |  2958 | `			}` |
|     100 |  2959 | `			pGen->pIn++;` |
|       4 |  2960 | `		}` |
|      40 |  2961 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  2962 | `			ph7_value *pObj;` |
|       - |  2963 | `			SyString sPath;` |
|       - |  2964 | `			sxu32 nIdx;` |
|      40 |  2965 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  2966 | `			/* Install in the literal table */` |
|      40 |  2967 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      20 |  2968 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 |  2969 | `				if( pObj == 0 ){` |
|     ! 0 |  2970 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2971 | `					return SXERR_ABORT;` |
|       - |  2972 | `				}` |
|      20 |  2973 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      20 |  2974 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 |  2975 | `			}` |
|       - |  2976 | `			/* Emit the load constant instruction.` |
|       - |  2977 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  2978 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      58 |  2979 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      18 |  2980 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      18 |  2981 | `				nIdx,0,0);` |
|      40 |  2982 | `			return SXRET_OK;` |
|       - |  2983 | `		}` |
|     ! 0 |  2984 | `	}` |
|       - |  2985 | `	/* Single-token literal: load directly */` |
|  693909 |  2986 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  693909 |  2987 | `	return rc;` |
|  346975 |  2988 |  |
|       - |  2989 | `/*` |
|       - |  2990 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2991 | ` */` |
|  693940 |  2992 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2993 |  |
|       - |  2994 | `	sxi32 rc;` |
|  693945 |  2995 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  693945 |  2996 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2997 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2998 | `		return rc;` |
|       - |  2999 | `	}` |
|       - |  3000 | `	/* Node successfully compiled */` |
|  693945 |  3001 | `	return SXRET_OK;` |
|  346975 |  3002 |  |
|       - |  3003 | `/*` |
|       - |  3004 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3005 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3006 | ` */` |
|       8 |  3007 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3008 |  |
|       - |  3009 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3010 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3011 | `		pGen->pIn++;` |
|       1 |  3012 | `	}` |
|       9 |  3013 | `	return SXRET_OK;` |
|       1 |  3014 |  |
|       - |  3015 | `/*` |
|       - |  3016 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3017 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3018 | ` */` |
|      58 |  3019 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3020 |  |
|      63 |  3021 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      28 |  3022 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3023 | `			return TRUE;` |
|      26 |  3024 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 |  3025 | `			return TRUE;` |
|       3 |  3026 | `		}` |
|      48 |  3027 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3028 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3029 | `			return TRUE;` |
|       - |  3030 | `		}` |
|     ! 0 |  3031 | `	}` |
|       - |  3032 | `	/* Not a reserved constant */` |
|      55 |  3033 | `	return FALSE;` |
|      34 |  3034 |  |
|       - |  3035 | `/*` |
|       - |  3036 | ` * Compile the 'const' statement.` |
|       - |  3037 | ` * According to the PHP language reference` |
|       - |  3038 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3039 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3040 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3041 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3042 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3043 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3044 | ` *  Syntax` |
|       - |  3045 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3046 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3047 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3048 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3049 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3050 | ` *  to get a list of all defined constants.` |
|       - |  3051 | ` *` |
|       - |  3052 | ` * Symisc eXtension.` |
|       - |  3053 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3054 | ` *  would allow only simple scalar value.` |
|       - |  3055 | ` *  Example` |
|       - |  3056 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3057 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3058 | ` */` |
|      32 |  3059 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3060 |  |
|       - |  3061 | `	SySet *pConsCode,*pInstrContainer;` |
|      37 |  3062 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3063 | `	SyString *pName;` |
|       - |  3064 | `	sxi32 rc;` |
|      37 |  3065 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      37 |  3066 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3067 | `		/* Invalid constant name */` |
|       7 |  3068 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 |  3069 | `		if( rc == SXERR_ABORT ){` |
|       - |  3070 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3071 | `			return SXERR_ABORT;` |
|       - |  3072 | `		}` |
|       7 |  3073 | `		goto Synchronize;` |
|       - |  3074 | `	}` |
|       - |  3075 | `	/* Peek constant name */` |
|      30 |  3076 | `	pName = &pGen->pIn->sData;` |
|       - |  3077 | `	/* Make sure the constant name isn't reserved */` |
|      30 |  3078 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3079 | `		/* Reserved constant */` |
|       9 |  3080 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 |  3081 | `		if( rc == SXERR_ABORT ){` |
|       - |  3082 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3083 | `			return SXERR_ABORT;` |
|       - |  3084 | `		}` |
|       9 |  3085 | `		goto Synchronize;` |
|       - |  3086 | `	}` |
|      21 |  3087 | `	pGen->pIn++;` |
|      21 |  3088 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3089 | `		/* Invalid statement*/` |
|       6 |  3090 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3091 | `		if( rc == SXERR_ABORT ){` |
|       - |  3092 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3093 | `			return SXERR_ABORT;` |
|       - |  3094 | `		}` |
|       6 |  3095 | `		goto Synchronize;` |
|       - |  3096 | `	}` |
|      15 |  3097 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3098 | `	/* Allocate a new constant value container */` |
|      15 |  3099 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3100 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3101 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3102 | `		return SXERR_ABORT;` |
|       - |  3103 | `	}` |
|      15 |  3104 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3105 | `	/* Swap bytecode container */` |
|      15 |  3106 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3107 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3108 | `	/* Compile constant value */` |
|      15 |  3109 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3110 | `	/* Emit the done instruction */` |
|      15 |  3111 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3112 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3113 | `	if( rc == SXERR_ABORT ){` |
|       - |  3114 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3115 | `		return SXERR_ABORT;` |
|       - |  3116 | `	}` |
|      15 |  3117 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3118 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3119 | `	{` |
|       - |  3120 | `		SyBlob sFQN;` |
|       - |  3121 | `		SyString sFQNStr;` |
|      15 |  3122 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3123 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3124 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3125 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3126 | `		SyBlobRelease(&sFQN);` |
|       - |  3127 | `	}` |
|      15 |  3128 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3129 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3130 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3131 | `	}` |
|      15 |  3132 | `	return SXRET_OK;` |
|       9 |  3133 | `Synchronize:` |
|       - |  3134 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3135 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      40 |  3136 | `		pGen->pIn++;` |
|       2 |  3137 | `	}` |
|      22 |  3138 | `	return SXRET_OK;` |
|      21 |  3139 |  |
|       - |  3140 | `/*` |
|       - |  3141 | ` * Compile the 'continue' statement.` |
|       - |  3142 | ` * According to the PHP language reference` |
|       - |  3143 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3144 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3145 | ` *  iteration.` |
|       - |  3146 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3147 | ` *  the purposes of continue.` |
|       - |  3148 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3149 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3150 | ` *  Note:` |
|       - |  3151 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3152 | ` */` |
|       - |  3153 | `/*` |
|       - |  3154 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3155 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3156 | ` * break/continue crosses a try boundary.` |
|       - |  3157 | ` *` |
|       - |  3158 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3159 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3160 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3161 | ` */` |
|    3288 |  3162 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3163 |  |
|    3293 |  3164 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   19241 |  3165 | `	while( pBlock && pBlock != pTarget ){` |
|   15953 |  3166 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3167 | `			if( pBlock->pUserData ){` |
|       - |  3168 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3169 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3170 | `			}else{` |
|       - |  3171 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3172 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3173 | `				 * exception context from a sub-execution.` |
|       - |  3174 | `				 */` |
|     ! 0 |  3175 | `				break;` |
|       - |  3176 | `			}` |
|       1 |  3177 | `		}` |
|   15953 |  3178 | `		pBlock = pBlock->pParent;` |
|       5 |  3179 | `	}` |
|    3293 |  3180 |  |
|    3192 |  3181 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3182 |  |
|       - |  3183 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3184 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3185 | `	sxu32 nLineLocal;` |
|       - |  3186 | `	sxi32 rc;` |
|    3197 |  3187 | `	nLineLocal = pGen->pIn->nLine;` |
|    3197 |  3188 | `	iLevel = 0;` |
|       - |  3189 | `	/* Jump the 'continue' keyword */` |
|    3197 |  3190 | `	pGen->pIn++;` |
|    3197 |  3191 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3192 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3193 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3194 | `		 */` |
|       - |  3195 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3196 | `		char *zAlloc = 0;` |
|       - |  3197 | `		SyString sNum;` |
|      17 |  3198 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3199 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3200 | `			return SXERR_ABORT;` |
|       - |  3201 | `		}` |
|      17 |  3202 | `		if( rc == SXRET_OK ){` |
|      20 |  3203 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3204 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3205 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3206 | `				return SXERR_ABORT;` |
|       - |  3207 | `			}` |
|      14 |  3208 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3209 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3210 | `		}` |
|      17 |  3211 | `		if( iLevel < 2 ){` |
|       3 |  3212 | `			iLevel = 0;` |
|       1 |  3213 | `		}` |
|      17 |  3214 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3215 | `	}` |
|       - |  3216 | `	/* Point to the target loop */` |
|    3197 |  3217 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3197 |  3218 | `	if( pLoop == 0 ){` |
|       - |  3219 | `		/* Illegal continue */` |
|      13 |  3220 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      13 |  3221 | `		if( rc == SXERR_ABORT ){` |
|       - |  3222 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3223 | `			return SXERR_ABORT;` |
|       - |  3224 | `		}` |
|       8 |  3225 | `	}else{` |
|    3187 |  3226 | `		sxu32 nInstrIdx = 0;` |
|       - |  3227 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3187 |  3228 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3187 |  3229 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3230 | `			/* According to the PHP language reference manual` |
|       - |  3231 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3232 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3233 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3234 | `			 */` |
|       5 |  3235 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3236 | `			if( rc == SXRET_OK ){` |
|       5 |  3237 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3238 | `			}` |
|       3 |  3239 | `		}else{` |
|       - |  3240 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3183 |  3241 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3183 |  3242 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3243 | `				JumpFixup sJumpFix;` |
|       - |  3244 | `				/* Post-continue */` |
|      14 |  3245 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3246 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3247 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3248 | `			}` |
|       - |  3249 | `		}` |
|       - |  3250 | `	}` |
|    3197 |  3251 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3252 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3253 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3254 | `	}` |
|       - |  3255 | `	/* Statement successfully compiled */` |
|    3197 |  3256 | `	return SXRET_OK;` |
|    1601 |  3257 |  |
|       - |  3258 | `/*` |
|       - |  3259 | ` * Compile the 'break' statement.` |
|       - |  3260 | ` * According to the PHP language reference` |
|       - |  3261 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3262 | ` *  structure.` |
|       - |  3263 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3264 | ` *  enclosing structures are to be broken out of.` |
|       - |  3265 | ` */` |
|     122 |  3266 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3267 |  |
|       - |  3268 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3269 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3270 | `	sxi32 rc;` |
|     127 |  3271 | `	iLevel = 0;` |
|       - |  3272 | `	/* Jump the 'break' keyword */` |
|     127 |  3273 | `	pGen->pIn++;` |
|     127 |  3274 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3275 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3276 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3277 | `		 */` |
|       - |  3278 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      18 |  3279 | `		char *zAlloc = 0;` |
|       - |  3280 | `		SyString sNum;` |
|      18 |  3281 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      18 |  3282 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3283 | `			return SXERR_ABORT;` |
|       - |  3284 | `		}` |
|      18 |  3285 | `		if( rc == SXRET_OK ){` |
|      21 |  3286 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3287 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3288 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3289 | `				return SXERR_ABORT;` |
|       - |  3290 | `			}` |
|      15 |  3291 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3292 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3293 | `		}` |
|      18 |  3294 | `		if( iLevel < 2 ){` |
|       3 |  3295 | `			iLevel = 0;` |
|       1 |  3296 | `		}` |
|      18 |  3297 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3298 | `	}` |
|       - |  3299 | `	/* Extract the target loop */` |
|     127 |  3300 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     127 |  3301 | `	if( pLoop == 0 ){` |
|       - |  3302 | `		/* Illegal break */` |
|      18 |  3303 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      18 |  3304 | `		if( rc == SXERR_ABORT ){` |
|       - |  3305 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3306 | `			return SXERR_ABORT;` |
|       - |  3307 | `		}` |
|      10 |  3308 | `	}else{` |
|       - |  3309 | `		sxu32 nInstrIdx;` |
|       - |  3310 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|     111 |  3311 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|     111 |  3312 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|     111 |  3313 | `		if( rc == SXRET_OK ){` |
|       - |  3314 | `			/* Fix the jump later when the jump destination is resolved */` |
|     111 |  3315 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      53 |  3316 | `		}` |
|       - |  3317 | `	}` |
|     127 |  3318 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3319 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3320 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3321 | `	}` |
|       - |  3322 | `	/* Statement successfully compiled */` |
|     127 |  3323 | `	return SXRET_OK;` |
|      66 |  3324 |  |
|       - |  3325 | `/*` |
|       - |  3326 | ` * Compile or record a label.` |
|       - |  3327 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3328 | ` * Example` |
|       - |  3329 | ` *  goto LABEL;` |
|       - |  3330 | ` *   echo 'Foo';` |
|       - |  3331 | ` *  LABEL:` |
|       - |  3332 | ` *   echo 'Bar';` |
|       - |  3333 | ` */` |
|     112 |  3334 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3335 |  |
|       - |  3336 | `	GenBlock *pBlock;` |
|       - |  3337 | `	Label sLabel;` |
|       - |  3338 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3339 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3340 | `	if( pBlock ){` |
|       - |  3341 | `		sxi32 rc;` |
|       8 |  3342 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3343 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3344 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3345 | `			return SXERR_ABORT;` |
|       - |  3346 | `		}` |
|       4 |  3347 | `	}else{` |
|     113 |  3348 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3349 | `		char *zDup;` |
|       - |  3350 | `		/* Initialize label fields */` |
|     113 |  3351 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3352 | `		/* Duplicate label name */` |
|     113 |  3353 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3354 | `		if( zDup == 0 ){` |
|     ! 0 |  3355 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3356 | `			return SXERR_ABORT;` |
|       - |  3357 | `		}` |
|     113 |  3358 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3359 | `		sLabel.bRef  = FALSE;` |
|     113 |  3360 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3361 | `		pBlock = pGen->pCurrent;` |
|     221 |  3362 | `		while( pBlock ){` |
|     133 |  3363 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      24 |  3364 | `				break;` |
|       - |  3365 | `			}` |
|       - |  3366 | `			/* Point to the upper block */` |
|     113 |  3367 | `			pBlock = pBlock->pParent;` |
|       5 |  3368 | `		}` |
|     113 |  3369 | `		if( pBlock ){` |
|      24 |  3370 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      14 |  3371 | `		}else{` |
|      93 |  3372 | `			sLabel.pFunc = 0;` |
|       - |  3373 | `		}` |
|       - |  3374 | `		/* Insert in label set */` |
|     113 |  3375 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3376 | `	}` |
|     117 |  3377 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3378 | `	return SXRET_OK;` |
|      61 |  3379 |  |
|       - |  3380 | `/*` |
|       - |  3381 | ` * Compile the so hated 'goto' statement.` |
|       - |  3382 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3383 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3384 | ` * a compiler it has to do this.` |
|       - |  3385 | ` * According to the PHP language reference manual` |
|       - |  3386 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3387 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3388 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3389 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3390 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3391 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3392 | ` *   of a multi-level break` |
|       - |  3393 | ` */` |
|     152 |  3394 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3395 |  |
|       - |  3396 | `	JumpFixup sJump;` |
|       - |  3397 | `	sxi32 rc;` |
|     157 |  3398 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3399 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3400 | `		/* Missing label */` |
|     ! 0 |  3401 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3402 | `		if( rc == SXERR_ABORT ){` |
|       - |  3403 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3404 | `			return SXERR_ABORT;` |
|       - |  3405 | `		}` |
|     ! 0 |  3406 | `		return SXRET_OK;` |
|       - |  3407 | `	}` |
|     157 |  3408 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  3409 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3410 | `		if( rc == SXERR_ABORT ){` |
|       - |  3411 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3412 | `			return SXERR_ABORT;` |
|       - |  3413 | `		}` |
|       3 |  3414 | `	}else{` |
|     153 |  3415 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3416 | `		GenBlock *pBlock;` |
|       - |  3417 | `		char *zDup;` |
|       - |  3418 | `		/* Prepare the jump destination */` |
|     153 |  3419 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3420 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3421 | `		/* Duplicate label name */` |
|     153 |  3422 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3423 | `		if( zDup == 0 ){` |
|     ! 0 |  3424 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3425 | `			return SXERR_ABORT;` |
|       - |  3426 | `		}` |
|     153 |  3427 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3428 | `		pBlock = pGen->pCurrent;` |
|     315 |  3429 | `		while( pBlock ){` |
|     199 |  3430 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      36 |  3431 | `				break;` |
|       - |  3432 | `			}` |
|       - |  3433 | `			/* Point to the upper block */` |
|     167 |  3434 | `			pBlock = pBlock->pParent;` |
|       5 |  3435 | `		}` |
|     153 |  3436 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       9 |  3437 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       9 |  3438 | `			if( rc == SXERR_ABORT ){` |
|       - |  3439 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3440 | `				return SXERR_ABORT;` |
|       - |  3441 | `			}` |
|       3 |  3442 | `		}` |
|     153 |  3443 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      29 |  3444 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      16 |  3445 | `		}else{` |
|     127 |  3446 | `			sJump.pFunc = 0;` |
|       - |  3447 | `		}` |
|       - |  3448 | `		/* Emit the unconditional jump */` |
|     153 |  3449 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3450 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3451 | `		}` |
|       - |  3452 | `	}` |
|     157 |  3453 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3454 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3455 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3456 | `	}` |
|       - |  3457 | `	/* Statement successfully compiled */` |
|     157 |  3458 | `	return SXRET_OK;` |
|      81 |  3459 |  |
|       - |  3460 | `/*` |
|       - |  3461 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3462 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3463 | ` * failure.` |
|       - |  3464 | ` */` |
|      20 |  3465 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  3466 |  |
|       - |  3467 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3468 | `	sxu32 nRawObj;` |
|      10 |  3469 | `	sxu32 nObjIdx;` |
|       - |  3470 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3471 | `	 * a PHP block.` |
|       - |  3472 | `	 */` |
|      10 |  3473 | `Consume:` |
|      22 |  3474 | `	nRawObj = nObjIdx = 0;` |
|      22 |  3475 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3476 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3477 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3478 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3479 | `			return SXERR_ABORT;` |
|       - |  3480 | `		}` |
|       - |  3481 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3482 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3483 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3484 | `		++nRawObj;` |
|     ! 0 |  3485 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3486 | `	}` |
|      22 |  3487 | `	if( nRawObj > 0 ){` |
|       - |  3488 | `		/* Emit the consume instruction */` |
|     ! 0 |  3489 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3490 | `	}` |
|      22 |  3491 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3492 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3493 | `		/* Reset the token set */` |
|     ! 0 |  3494 | `		SySetReset(pTokenSet);` |
|       - |  3495 | `		/* Tokenize input */` |
|     ! 0 |  3496 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3497 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3498 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3499 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3500 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3501 | `		/* Advance the stream cursor */` |
|     ! 0 |  3502 | `		pGen->pRawIn++;` |
|       - |  3503 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3504 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3505 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3506 | `			sxi32 rc;` |
|       - |  3507 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3508 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3509 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3510 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3511 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3512 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3513 | `				return SXERR_ABORT;` |
|     ! 0 |  3514 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3515 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3516 | `			}` |
|     ! 0 |  3517 | `			goto Consume;` |
|       - |  3518 | `		}` |
|     ! 0 |  3519 | `	}else{` |
|       - |  3520 | `		/* No more chunks to process */` |
|      22 |  3521 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  3522 | `		return SXERR_EOF;` |
|       - |  3523 | `	}` |
|     ! 0 |  3524 | `	return SXRET_OK;` |
|      12 |  3525 |  |
|       - |  3526 | `/*` |
|       - |  3527 | ` * Compile a PHP block.` |
|       - |  3528 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3529 | ` * optionally delimited by braces {}.` |
|       - |  3530 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3531 | ` * and this function takes care of generating the appropriate error` |
|       - |  3532 | ` * message.` |
|       - |  3533 | ` */` |
|  382048 |  3534 | `static sxi32 PH7_CompileBlock(` |
|       - |  3535 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3536 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3537 | `	)` |
|       5 |  3538 |  |
|       - |  3539 | `	sxi32 rc;` |
|       - |  3540 | `	sxu32 nLine;` |
|  382053 |  3541 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  380567 |  3542 | `		nLine = pGen->pIn->nLine;` |
|  380567 |  3543 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  380567 |  3544 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3545 | `			return SXERR_ABORT;` |
|       - |  3546 | `		}` |
|  380567 |  3547 | `		pGen->pIn++;` |
|       - |  3548 | `		/* Compile until we hit the closing braces '}' */` |
|  519787 |  3549 | `		for(;;){` |
| 1039579 |  3550 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  3551 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  3552 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3553 | `			 	   return SXERR_ABORT;` |
|       - |  3554 | `				}` |
|      22 |  3555 | `				if( rc == SXERR_EOF ){` |
|       - |  3556 | `					/* No more token to process. Missing closing braces */` |
|      22 |  3557 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      22 |  3558 | `					break;` |
|       - |  3559 | `				}` |
|     ! 0 |  3560 | `			}` |
| 1039559 |  3561 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3562 | `				/* Closing braces found,break immediately*/` |
|  380547 |  3563 | `				pGen->pIn++;` |
|  380547 |  3564 | `				break;` |
|       - |  3565 | `			}` |
|       - |  3566 | `			/* Compile a single statement */` |
|  659017 |  3567 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  659017 |  3568 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3569 | `				return SXERR_ABORT;` |
|       - |  3570 | `			}` |
|       5 |  3571 | `		}` |
|  380567 |  3572 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  191772 |  3573 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3574 | `		pGen->pIn++;` |
|     ! 0 |  3575 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3576 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3577 | `			return SXERR_ABORT;` |
|       - |  3578 | `		}` |
|       - |  3579 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3580 | `		for(;;){` |
|     ! 0 |  3581 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3582 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3583 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3584 | `			 	   return SXERR_ABORT;` |
|       - |  3585 | `				}` |
|     ! 0 |  3586 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3587 | `					/* No more token to process */` |
|     ! 0 |  3588 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3589 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3590 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3591 | `					}` |
|     ! 0 |  3592 | `					break;` |
|       - |  3593 | `				}` |
|     ! 0 |  3594 | `			}` |
|     ! 0 |  3595 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3596 | `				sxi32 nKwrd;` |
|       - |  3597 | `				/* Keyword found */` |
|     ! 0 |  3598 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3599 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3600 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3601 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3602 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3603 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3604 | `						}` |
|     ! 0 |  3605 | `						break;` |
|       - |  3606 | `				}` |
|     ! 0 |  3607 | `			}` |
|       - |  3608 | `			/* Compile a single statement */` |
|     ! 0 |  3609 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3610 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3611 | `				return SXERR_ABORT;` |
|       - |  3612 | `			}` |
|     ! 0 |  3613 | `		}` |
|     ! 0 |  3614 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3615 | `	}else{` |
|       - |  3616 | `		/* Compile a single statement */` |
|    1491 |  3617 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1491 |  3618 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3619 | `			return SXERR_ABORT;` |
|       - |  3620 | `		}` |
|       - |  3621 | `	}` |
|       - |  3622 | `	/* Jump trailing semi-colons ';' */` |
|  382053 |  3623 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3624 | `		pGen->pIn++;` |
|     ! 0 |  3625 | `	}` |
|  382053 |  3626 | `	return SXRET_OK;` |
|  191029 |  3627 |  |
|       - |  3628 | `/*` |
|       - |  3629 | ` * Compile the gentle 'while' statement.` |
|       - |  3630 | ` * According to the PHP language reference` |
|       - |  3631 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3632 | ` *  The basic form of a while statement is:` |
|       - |  3633 | ` *  while (expr)` |
|       - |  3634 | ` *   statement` |
|       - |  3635 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3636 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3637 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3638 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3639 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3640 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3641 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3642 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3643 | ` *  while (expr):` |
|       - |  3644 | ` *    statement` |
|       - |  3645 | ` *   endwhile;` |
|       - |  3646 | ` */` |
|   12700 |  3647 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3648 |  |
|   12705 |  3649 | `	GenBlock *pWhileBlock = 0;` |
|   12705 |  3650 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3651 | `	sxu32 nFalseJump;` |
|       - |  3652 | `	sxu32 nLine;` |
|       - |  3653 | `	sxi32 rc;` |
|   12705 |  3654 | `	nLine = pGen->pIn->nLine;` |
|       - |  3655 | `	/* Jump the 'while' keyword */` |
|   12705 |  3656 | `	pGen->pIn++;` |
|   12705 |  3657 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3658 | `		/* Syntax error */` |
|     ! 0 |  3659 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3660 | `		if( rc == SXERR_ABORT ){` |
|       - |  3661 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3662 | `			return SXERR_ABORT;` |
|       - |  3663 | `		}` |
|     ! 0 |  3664 | `		goto Synchronize;` |
|       - |  3665 | `	}` |
|       - |  3666 | `	/* Jump the left parenthesis '(' */` |
|   12705 |  3667 | `	pGen->pIn++;` |
|       - |  3668 | `	/* Create the loop block */` |
|   12705 |  3669 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   12705 |  3670 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3671 | `		return SXERR_ABORT;` |
|       - |  3672 | `	}` |
|       - |  3673 | `	/* Delimit the condition */` |
|   12705 |  3674 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   12705 |  3675 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3676 | `		/* Empty expression */` |
|       3 |  3677 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3678 | `		if( rc == SXERR_ABORT ){` |
|       - |  3679 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3680 | `			return SXERR_ABORT;` |
|       - |  3681 | `		}` |
|       1 |  3682 | `	}` |
|       - |  3683 | `	/* Swap token streams */` |
|   12705 |  3684 | `	pTmp = pGen->pEnd;` |
|   12705 |  3685 | `	pGen->pEnd = pEnd;` |
|       - |  3686 | `	/* Compile the expression */` |
|   12705 |  3687 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12705 |  3688 | `	if( rc == SXERR_ABORT ){` |
|       - |  3689 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3690 | `		return SXERR_ABORT;` |
|       - |  3691 | `	}` |
|       - |  3692 | `	/* Update token stream */` |
|   12705 |  3693 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3694 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3695 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3696 | `			return SXERR_ABORT;` |
|       - |  3697 | `		}` |
|     ! 0 |  3698 | `		pGen->pIn++;` |
|     ! 0 |  3699 | `	}` |
|       - |  3700 | `	/* Synchronize pointers */` |
|   12705 |  3701 | `	pGen->pIn  = &pEnd[1];` |
|   12705 |  3702 | `	pGen->pEnd = pTmp;` |
|       - |  3703 | `	/* Emit the false jump */` |
|   12705 |  3704 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3705 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   12705 |  3706 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3707 | `	/* Compile the loop body */` |
|   12705 |  3708 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   12705 |  3709 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3710 | `		return SXERR_ABORT;` |
|       - |  3711 | `	}` |
|       - |  3712 | `	/* Emit the unconditional jump to the start of the loop */` |
|   12705 |  3713 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3714 | `	/* Fix all jumps now the destination is resolved */` |
|   12705 |  3715 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3716 | `	/* Release the loop block */` |
|   12705 |  3717 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3718 | `	/* Statement successfully compiled */` |
|   12705 |  3719 | `	return SXRET_OK;` |
|     ! 0 |  3720 | `Synchronize:` |
|       - |  3721 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3722 | `	 * compiling this erroneous block.` |
|       - |  3723 | `	 */` |
|     ! 0 |  3724 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3725 | `		pGen->pIn++;` |
|     ! 0 |  3726 | `	}` |
|     ! 0 |  3727 | `	return SXRET_OK;` |
|    6355 |  3728 |  |
|       - |  3729 | `/*` |
|       - |  3730 | ` * Compile the ugly do..while() statement.` |
|       - |  3731 | ` * According to the PHP language reference` |
|       - |  3732 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3733 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3734 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3735 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3736 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3737 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3738 | ` *  would end immediately).` |
|       - |  3739 | ` *  There is just one syntax for do-while loops:` |
|       - |  3740 | ` *  <?php` |
|       - |  3741 | ` *  $i = 0;` |
|       - |  3742 | ` *  do {` |
|       - |  3743 | ` *   echo $i;` |
|       - |  3744 | ` *  } while ($i > 0);` |
|       - |  3745 | ` * ?>` |
|       - |  3746 | ` */` |
|       2 |  3747 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3748 |  |
|       3 |  3749 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3750 | `	GenBlock *pDoBlock = 0;` |
|       - |  3751 | `	sxu32 nLine;` |
|       - |  3752 | `	sxi32 rc;` |
|       3 |  3753 | `	nLine = pGen->pIn->nLine;` |
|       - |  3754 | `	/* Jump the 'do' keyword */` |
|       3 |  3755 | `	pGen->pIn++;` |
|       - |  3756 | `	/* Create the loop block */` |
|       3 |  3757 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3758 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3759 | `		return SXERR_ABORT;` |
|       - |  3760 | `	}` |
|       - |  3761 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3762 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3763 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3764 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3765 | `		return SXERR_ABORT;` |
|       - |  3766 | `	}` |
|       3 |  3767 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3768 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3769 | `	}` |
|       3 |  3770 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3771 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3772 | `			/* Missing 'while' statement */` |
|       3 |  3773 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3774 | `			if( rc == SXERR_ABORT ){` |
|       - |  3775 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3776 | `				return SXERR_ABORT;` |
|       - |  3777 | `			}` |
|       3 |  3778 | `			goto Synchronize;` |
|       - |  3779 | `	}` |
|       - |  3780 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3781 | `	pGen->pIn++;` |
|     ! 0 |  3782 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3783 | `		/* Syntax error */` |
|     ! 0 |  3784 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3785 | `		if( rc == SXERR_ABORT ){` |
|       - |  3786 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3787 | `			return SXERR_ABORT;` |
|       - |  3788 | `		}` |
|     ! 0 |  3789 | `		goto Synchronize;` |
|       - |  3790 | `	}` |
|       - |  3791 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3792 | `	pGen->pIn++;` |
|       - |  3793 | `	/* Delimit the condition */` |
|     ! 0 |  3794 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3795 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3796 | `		/* Empty expression */` |
|     ! 0 |  3797 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3798 | `		if( rc == SXERR_ABORT ){` |
|       - |  3799 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3800 | `			return SXERR_ABORT;` |
|       - |  3801 | `		}` |
|     ! 0 |  3802 | `		goto Synchronize;` |
|       - |  3803 | `	}` |
|       - |  3804 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3805 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3806 | `		JumpFixup *aPost;` |
|       - |  3807 | `		VmInstr *pInstr;` |
|       - |  3808 | `		sxu32 nJumpDest;` |
|       - |  3809 | `		sxu32 n;` |
|     ! 0 |  3810 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3811 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3812 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3813 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3814 | `			if( pInstr ){` |
|       - |  3815 | `				/* Fix */` |
|     ! 0 |  3816 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3817 | `			}` |
|     ! 0 |  3818 | `		}` |
|     ! 0 |  3819 | `	}` |
|       - |  3820 | `	/* Swap token streams */` |
|     ! 0 |  3821 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3822 | `	pGen->pEnd = pEnd;` |
|       - |  3823 | `	/* Compile the expression */` |
|     ! 0 |  3824 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3825 | `	if( rc == SXERR_ABORT ){` |
|       - |  3826 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3827 | `		return SXERR_ABORT;` |
|       - |  3828 | `	}` |
|       - |  3829 | `	/* Update token stream */` |
|     ! 0 |  3830 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3831 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3832 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3833 | `			return SXERR_ABORT;` |
|       - |  3834 | `		}` |
|     ! 0 |  3835 | `		pGen->pIn++;` |
|     ! 0 |  3836 | `	}` |
|     ! 0 |  3837 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3838 | `	pGen->pEnd = pTmp;` |
|       - |  3839 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3840 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3841 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3842 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3843 | `	/* Release the loop block */` |
|     ! 0 |  3844 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3845 | `	/* Statement successfully compiled */` |
|     ! 0 |  3846 | `	return SXRET_OK;` |
|       1 |  3847 | `Synchronize:` |
|       - |  3848 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3849 | `	 * compiling this erroneous block.` |
|       - |  3850 | `	 */` |
|       3 |  3851 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3852 | `		pGen->pIn++;` |
|     ! 0 |  3853 | `	}` |
|       3 |  3854 | `	return SXRET_OK;` |
|       2 |  3855 |  |
|       - |  3856 | `/*` |
|       - |  3857 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3858 | ` * According to the PHP language reference` |
|       - |  3859 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3860 | ` *  The syntax of a for loop is:` |
|       - |  3861 | ` *  for (expr1; expr2; expr3)` |
|       - |  3862 | ` *   statement` |
|       - |  3863 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3864 | ` *  the beginning of the loop.` |
|       - |  3865 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  3866 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  3867 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  3868 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  3869 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  3870 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  3871 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  3872 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  3873 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  3874 | ` *  of using the for truth expression.` |
|       - |  3875 | ` */` |
|   12712 |  3876 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  3877 |  |
|   12717 |  3878 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   12717 |  3879 | `	GenBlock *pForBlock = 0;` |
|       - |  3880 | `	sxu32 nFalseJump;` |
|       - |  3881 | `	sxu32 nLine;` |
|       - |  3882 | `	sxi32 rc;` |
|   12717 |  3883 | `	nLine = pGen->pIn->nLine;` |
|       - |  3884 | `	/* Jump the 'for' keyword */` |
|   12717 |  3885 | `	pGen->pIn++;` |
|   12717 |  3886 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3887 | `		/* Syntax error */` |
|     ! 0 |  3888 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  3889 | `		if( rc == SXERR_ABORT ){` |
|       - |  3890 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3891 | `			return SXERR_ABORT;` |
|       - |  3892 | `		}` |
|     ! 0 |  3893 | `		return SXRET_OK;` |
|       - |  3894 | `	}` |
|       - |  3895 | `	/* Jump the left parenthesis '(' */` |
|   12717 |  3896 | `	pGen->pIn++;` |
|       - |  3897 | `	/* Delimit the init-expr;condition;post-expr */` |
|   12717 |  3898 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   12717 |  3899 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3900 | `		/* Empty expression */` |
|     ! 0 |  3901 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  3902 | `		if( rc == SXERR_ABORT ){` |
|       - |  3903 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3904 | `			return SXERR_ABORT;` |
|       - |  3905 | `		}` |
|       - |  3906 | `		/* Synchronize */` |
|     ! 0 |  3907 | `		pGen->pIn = pEnd;` |
|     ! 0 |  3908 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3909 | `			pGen->pIn++;` |
|     ! 0 |  3910 | `		}` |
|     ! 0 |  3911 | `		return SXRET_OK;` |
|       - |  3912 | `	}` |
|       - |  3913 | `	/* Swap token streams */` |
|   12717 |  3914 | `	pTmp = pGen->pEnd;` |
|   12717 |  3915 | `	pGen->pEnd = pEnd;` |
|       - |  3916 | `	/* Compile initialization expressions if available */` |
|   12717 |  3917 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3918 | `	/* Pop operand lvalues */` |
|   12717 |  3919 | `	if( rc == SXERR_ABORT ){` |
|       - |  3920 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3921 | `		return SXERR_ABORT;` |
|   12717 |  3922 | `	}else if( rc != SXERR_EMPTY ){` |
|   12715 |  3923 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6355 |  3924 | `	}` |
|   12717 |  3925 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3926 | `		/* Syntax error */` |
|     ! 0 |  3927 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3928 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  3929 | `		if( rc == SXERR_ABORT ){` |
|       - |  3930 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3931 | `			return SXERR_ABORT;` |
|       - |  3932 | `		}` |
|     ! 0 |  3933 | `		return SXRET_OK;` |
|       - |  3934 | `	}` |
|       - |  3935 | `	/* Jump the trailing ';' */` |
|   12717 |  3936 | `	pGen->pIn++;` |
|       - |  3937 | `	/* Create the loop block */` |
|   12717 |  3938 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   12717 |  3939 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3940 | `		return SXERR_ABORT;` |
|       - |  3941 | `	}` |
|       - |  3942 | `	/* Deffer continue jumps */` |
|   12717 |  3943 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  3944 | `	/* Compile the condition */` |
|   12717 |  3945 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12717 |  3946 | `	if( rc == SXERR_ABORT ){` |
|       - |  3947 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3948 | `		return SXERR_ABORT;` |
|   12717 |  3949 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  3950 | `		/* Emit the false jump */` |
|   12715 |  3951 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3952 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   12715 |  3953 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    6355 |  3954 | `	}` |
|   12717 |  3955 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3956 | `		/* Syntax error */` |
|       6 |  3957 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3958 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  3959 | `		if( rc == SXERR_ABORT ){` |
|       - |  3960 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3961 | `			return SXERR_ABORT;` |
|       - |  3962 | `		}` |
|       6 |  3963 | `		return SXRET_OK;` |
|       - |  3964 | `	}` |
|       - |  3965 | `	/* Jump the trailing ';' */` |
|   12713 |  3966 | `	pGen->pIn++;` |
|       - |  3967 | `	/* Save the post condition stream */` |
|   12713 |  3968 | `	pPostStart = pGen->pIn;` |
|       - |  3969 | `	/* Compile the loop body */` |
|   12713 |  3970 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   12713 |  3971 | `	pGen->pEnd = pTmp;` |
|   12713 |  3972 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   12713 |  3973 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3974 | `		return SXERR_ABORT;` |
|       - |  3975 | `	}` |
|       - |  3976 | `	/* Fix post-continue jumps */` |
|   12713 |  3977 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  3978 | `		JumpFixup *aPost;` |
|       - |  3979 | `		VmInstr *pInstr;` |
|       - |  3980 | `		sxu32 nJumpDest;` |
|       - |  3981 | `		sxu32 n;` |
|      14 |  3982 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  3983 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  3984 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  3985 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  3986 | `			if( pInstr ){` |
|       - |  3987 | `				/* Fix jump */` |
|      14 |  3988 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  3989 | `			}` |
|       8 |  3990 | `		}` |
|       6 |  3991 | `	}` |
|       - |  3992 | `	/* compile the post-expressions if available */` |
|   12713 |  3993 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3994 | `		pPostStart++;` |
|     ! 0 |  3995 | `	}` |
|   12713 |  3996 | `	if( pPostStart < pEnd ){` |
|       - |  3997 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   12713 |  3998 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   12713 |  3999 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12713 |  4000 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4001 | `			/* Syntax error */` |
|     ! 0 |  4002 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4003 | `			if( rc == SXERR_ABORT ){` |
|       - |  4004 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4005 | `				return SXERR_ABORT;` |
|       - |  4006 | `			}` |
|     ! 0 |  4007 | `			return SXRET_OK;` |
|       - |  4008 | `		}` |
|   12713 |  4009 | `		RE_SWAP_DELIMITER(pGen);` |
|   12713 |  4010 | `		if( rc == SXERR_ABORT ){` |
|       - |  4011 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4012 | `			return SXERR_ABORT;` |
|   12713 |  4013 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4014 | `			/* Pop operand lvalue */` |
|   12713 |  4015 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6354 |  4016 | `		}` |
|    6354 |  4017 | `	}` |
|       - |  4018 | `	/* Emit the unconditional jump to the start of the loop */` |
|   12713 |  4019 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4020 | `	/* Fix all jumps now the destination is resolved */` |
|   12713 |  4021 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4022 | `	/* Release the loop block */` |
|   12713 |  4023 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4024 | `	/* Statement successfully compiled */` |
|   12713 |  4025 | `	return SXRET_OK;` |
|    6361 |  4026 |  |
|       - |  4027 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4028 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4029 | ` * are allowed.` |
|       - |  4030 | ` */` |
|    6798 |  4031 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4032 |  |
|    6803 |  4033 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    6803 |  4034 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4035 | `		/* Unexpected expression */` |
|     ! 0 |  4036 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4037 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4038 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4039 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4040 | `		}` |
|     ! 0 |  4041 | `	}` |
|    6803 |  4042 | `	return rc;` |
|       5 |  4043 |  |
|       - |  4044 | `/*` |
|       - |  4045 | ` * Compile the 'foreach' statement.` |
|       - |  4046 | ` * According to the PHP language reference` |
|       - |  4047 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4048 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4049 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4050 | ` *  is a minor but useful extension of the first:` |
|       - |  4051 | ` *  foreach (array_expression as $value)` |
|       - |  4052 | ` *    statement` |
|       - |  4053 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4054 | ` *   statement` |
|       - |  4055 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4056 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4057 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4058 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4059 | ` *  to the variable $key on each loop.` |
|       - |  4060 | ` *  Note:` |
|       - |  4061 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4062 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4063 | ` *  Note:` |
|       - |  4064 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4065 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4066 | ` *  or after the foreach without resetting it.` |
|       - |  4067 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4068 | ` *  of copying the value.` |
|       - |  4069 | ` */` |
|    3464 |  4070 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4071 |  |
|    3469 |  4072 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3469 |  4073 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3469 |  4074 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4075 | `	ph7_foreach_info *pInfo;` |
|       - |  4076 | `	sxu32 nFalseJump;` |
|       - |  4077 | `	VmInstr *pInstr;` |
|       - |  4078 | `	sxu32 nLine;` |
|       - |  4079 | `	sxi32 rc;` |
|    3469 |  4080 | `	nLine = pGen->pIn->nLine;` |
|       - |  4081 | `	/* Jump the 'foreach' keyword */` |
|    3469 |  4082 | `	pGen->pIn++;` |
|    3469 |  4083 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4084 | `		/* Syntax error */` |
|     ! 0 |  4085 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4086 | `		if( rc == SXERR_ABORT ){` |
|       - |  4087 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4088 | `			return SXERR_ABORT;` |
|       - |  4089 | `		}` |
|     ! 0 |  4090 | `		goto Synchronize;` |
|       - |  4091 | `	}` |
|       - |  4092 | `	/* Jump the left parenthesis '(' */` |
|    3469 |  4093 | `	pGen->pIn++;` |
|       - |  4094 | `	/* Create the loop block */` |
|    3469 |  4095 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3469 |  4096 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4097 | `		return SXERR_ABORT;` |
|       - |  4098 | `	}` |
|       - |  4099 | `	/* Delimit the expression */` |
|    3469 |  4100 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3469 |  4101 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4102 | `		/* Empty expression */` |
|     ! 0 |  4103 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4104 | `		if( rc == SXERR_ABORT ){` |
|       - |  4105 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4106 | `			return SXERR_ABORT;` |
|       - |  4107 | `		}` |
|       - |  4108 | `		/* Synchronize */` |
|     ! 0 |  4109 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4110 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4111 | `			pGen->pIn++;` |
|     ! 0 |  4112 | `		}` |
|     ! 0 |  4113 | `		return SXRET_OK;` |
|       - |  4114 | `	}` |
|       - |  4115 | `	/* Compile the array expression */` |
|    3469 |  4116 | `	pCur = pGen->pIn;` |
|   23219 |  4117 | `	while( pCur < pEnd ){` |
|   23219 |  4118 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3483 |  4119 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3483 |  4120 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4121 | `				/* Break with the first 'as' found */` |
|    3469 |  4122 | `				break;` |
|       - |  4123 | `			}` |
|       7 |  4124 | `		}` |
|       - |  4125 | `		/* Advance the stream cursor */` |
|   19755 |  4126 | `		pCur++;` |
|       5 |  4127 | `	}` |
|    3469 |  4128 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4129 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4130 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4131 | `		if( rc == SXERR_ABORT ){` |
|       - |  4132 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4133 | `			return SXERR_ABORT;` |
|       - |  4134 | `		}` |
|     ! 0 |  4135 | `		goto Synchronize;` |
|       - |  4136 | `	}` |
|       - |  4137 | `	/* Swap token streams */` |
|    3469 |  4138 | `	pTmp = pGen->pEnd;` |
|    3469 |  4139 | `	pGen->pEnd = pCur;` |
|    3469 |  4140 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3469 |  4141 | `	if( rc == SXERR_ABORT ){` |
|       - |  4142 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4143 | `		return SXERR_ABORT;` |
|       - |  4144 | `	}` |
|       - |  4145 | `	/* Update token stream */` |
|    3469 |  4146 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4147 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4148 | `		if( rc == SXERR_ABORT ){` |
|       - |  4149 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4150 | `			return SXERR_ABORT;` |
|       - |  4151 | `		}` |
|     ! 0 |  4152 | `		pGen->pIn++;` |
|     ! 0 |  4153 | `	}` |
|    3469 |  4154 | `	pCur++; /* Jump the 'as' keyword */` |
|    3469 |  4155 | `	pGen->pIn = pCur;` |
|    3469 |  4156 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4157 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4158 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4159 | `			return SXERR_ABORT;` |
|       - |  4160 | `		}` |
|     ! 0 |  4161 | `	}` |
|       - |  4162 | `	/* Create the foreach context */` |
|    3469 |  4163 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3469 |  4164 | `	if( pInfo == 0 ){` |
|     ! 0 |  4165 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4166 | `		return SXERR_ABORT;` |
|       - |  4167 | `	}` |
|       - |  4168 | `	/* Zero the structure */` |
|    3469 |  4169 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4170 | `	/* Initialize structure fields */` |
|    3469 |  4171 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4172 | `	/* Check if we have a key field */` |
|   10447 |  4173 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6983 |  4174 | `		pCur++;` |
|       5 |  4175 | `	}` |
|    3469 |  4176 | `	if( pCur < pEnd ){` |
|       - |  4177 | `		/* Compile the expression holding the key name */` |
|    3349 |  4178 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4179 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4180 | `			if( rc == SXERR_ABORT ){` |
|       - |  4181 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4182 | `				return SXERR_ABORT;` |
|       - |  4183 | `			}` |
|     ! 0 |  4184 | `		}else{` |
|    3349 |  4185 | `			pGen->pEnd = pCur;` |
|    3349 |  4186 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3349 |  4187 | `			if( rc == SXERR_ABORT ){` |
|       - |  4188 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4189 | `				return SXERR_ABORT;` |
|       - |  4190 | `			}` |
|    3349 |  4191 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3349 |  4192 | `			if( pInstr->p3 ){` |
|       - |  4193 | `				/* Record key name */` |
|    3349 |  4194 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1672 |  4195 | `			}` |
|    3349 |  4196 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4197 | `		}` |
|    3349 |  4198 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1672 |  4199 | `	}` |
|    3469 |  4200 | `	pGen->pEnd = pEnd;` |
|    3469 |  4201 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4202 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4203 | `		if( rc == SXERR_ABORT ){` |
|       - |  4204 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4205 | `			return SXERR_ABORT;` |
|       - |  4206 | `		}` |
|     ! 0 |  4207 | `		goto Synchronize;` |
|       - |  4208 | `	}` |
|    3469 |  4209 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4210 | `		pGen->pIn++;` |
|       - |  4211 | `		/* Pass by reference  */` |
|      11 |  4212 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4213 | `	}` |
|       - |  4214 | `	/* Check if the value target is list() */` |
|    3469 |  4215 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4216 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4217 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4218 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4219 | `		 */` |
|       - |  4220 | `		static int iForeachListCnt = 0;` |
|       - |  4221 | `		char zTmp[128];` |
|       - |  4222 | `		sxu32 nLen;` |
|       - |  4223 | `		char *zDup;` |
|      10 |  4224 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4225 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4226 | `		if( zDup == 0 ){` |
|     ! 0 |  4227 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4228 | `			return SXERR_ABORT;` |
|       - |  4229 | `		}` |
|      10 |  4230 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4231 | `		/* Save list() token boundaries */` |
|      10 |  4232 | `		pListStart = pGen->pIn;` |
|       - |  4233 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4234 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4235 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4236 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4237 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4238 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4239 | `				return SXERR_ABORT;` |
|       - |  4240 | `			}` |
|       3 |  4241 | `			goto Synchronize;` |
|       - |  4242 | `		}` |
|       7 |  4243 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4244 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4245 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4246 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4247 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4248 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4249 | `				return SXERR_ABORT;` |
|       - |  4250 | `			}` |
|     ! 0 |  4251 | `			goto Synchronize;` |
|       - |  4252 | `		}` |
|       7 |  4253 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4254 | `		pListEnd = pGen->pIn;` |
|       7 |  4255 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3464 |  4256 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4257 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4258 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4259 | `		 */` |
|       - |  4260 | `		static int iForeachShortListCnt = 0;` |
|       - |  4261 | `		char zTmp[128];` |
|       - |  4262 | `		sxu32 nLen;` |
|       - |  4263 | `		char *zDup;` |
|       3 |  4264 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 |  4265 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 |  4266 | `		if( zDup == 0 ){` |
|     ! 0 |  4267 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4268 | `			return SXERR_ABORT;` |
|       - |  4269 | `		}` |
|       3 |  4270 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4271 | `		/* Save [...] token boundaries */` |
|       3 |  4272 | `		pListStart = pGen->pIn;` |
|       - |  4273 | `		/* Advance past [...] */` |
|       3 |  4274 | `		pGen->pIn++; /* Jump '[' */` |
|       3 |  4275 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 |  4276 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4277 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4278 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4279 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4280 | `				return SXERR_ABORT;` |
|       - |  4281 | `			}` |
|     ! 0 |  4282 | `			goto Synchronize;` |
|       - |  4283 | `		}` |
|       3 |  4284 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 |  4285 | `		pListEnd = pGen->pIn;` |
|       3 |  4286 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 |  4287 | `	}else{` |
|       - |  4288 | `		/* Compile the expression holding the value name */` |
|    3459 |  4289 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3459 |  4290 | `		if( rc == SXERR_ABORT ){` |
|       - |  4291 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4292 | `			return SXERR_ABORT;` |
|       - |  4293 | `		}` |
|    3459 |  4294 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3459 |  4295 | `		if( pInstr->p3 ){` |
|       - |  4296 | `			/* Record value name */` |
|    3459 |  4297 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1727 |  4298 | `		}` |
|       - |  4299 | `	}` |
|       - |  4300 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3467 |  4301 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4302 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3467 |  4303 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4304 | `	/* Record the first instruction to execute */` |
|    3467 |  4305 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4306 | `	/* Emit the FOREACH_STEP instruction */` |
|    3467 |  4307 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4308 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3467 |  4309 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4310 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3467 |  4311 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4312 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4313 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4314 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4315 | `		 */` |
|       9 |  4316 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4317 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4318 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4319 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4320 | `		 */` |
|       9 |  4321 | `		pSavedIn = pGen->pIn;` |
|       9 |  4322 | `		pSavedEnd = pGen->pEnd;` |
|       9 |  4323 | `		pGen->pIn = pListStart;` |
|       9 |  4324 | `		pGen->pEnd = pListEnd;` |
|       9 |  4325 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 |  4326 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 |  4327 | `		}else{` |
|       7 |  4328 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4329 | `		}` |
|       9 |  4330 | `		pGen->pIn = pSavedIn;` |
|       9 |  4331 | `		pGen->pEnd = pSavedEnd;` |
|       9 |  4332 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4333 | `			return SXERR_ABORT;` |
|       - |  4334 | `		}` |
|       - |  4335 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 |  4336 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 |  4337 | `	}` |
|       - |  4338 | `	/* Compile the loop body */` |
|    3467 |  4339 | `	pGen->pIn = &pEnd[1];` |
|    3467 |  4340 | `	pGen->pEnd = pTmp;` |
|    3467 |  4341 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3467 |  4342 | `	if( rc == SXERR_ABORT ){` |
|       - |  4343 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4344 | `		return SXERR_ABORT;` |
|       - |  4345 | `	}` |
|       - |  4346 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3467 |  4347 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4348 | `	/* Fix all jumps now the destination is resolved */` |
|    3467 |  4349 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4350 | `	/* Release the loop block */` |
|    3467 |  4351 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4352 | `	/* Statement successfully compiled */` |
|    3467 |  4353 | `	return SXRET_OK;` |
|       1 |  4354 | `Synchronize:` |
|       - |  4355 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4356 | `	 * compiling this erroneous block.` |
|       - |  4357 | `	 */` |
|       3 |  4358 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4359 | `		pGen->pIn++;` |
|     ! 0 |  4360 | `	}` |
|       3 |  4361 | `	return SXRET_OK;` |
|    1737 |  4362 |  |
|       - |  4363 | `/*` |
|       - |  4364 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4365 | ` * According to the PHP language reference` |
|       - |  4366 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4367 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4368 | ` *  that is similar to that of C:` |
|       - |  4369 | ` *  if (expr)` |
|       - |  4370 | ` *   statement` |
|       - |  4371 | ` *  else construct:` |
|       - |  4372 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4373 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4374 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4375 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4376 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4377 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4378 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4379 | ` *  elseif` |
|       - |  4380 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4381 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4382 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4383 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4384 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4385 | ` *   <?php` |
|       - |  4386 | ` *    if ($a > $b) {` |
|       - |  4387 | ` *     echo "a is bigger than b";` |
|       - |  4388 | ` *    } elseif ($a == $b) {` |
|       - |  4389 | ` *     echo "a is equal to b";` |
|       - |  4390 | ` *    } else {` |
|       - |  4391 | ` *     echo "a is smaller than b";` |
|       - |  4392 | ` *    }` |
|       - |  4393 | ` *    ?>` |
|       - |  4394 | ` */` |
|  132252 |  4395 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4396 |  |
|  132257 |  4397 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  132257 |  4398 | `	GenBlock *pCondBlock = 0;` |
|       - |  4399 | `	sxu32 nJumpIdx;` |
|       - |  4400 | `	sxu32 nKeyID;` |
|       - |  4401 | `	sxi32 rc;` |
|       - |  4402 | `	/* Jump the 'if' keyword */` |
|  132257 |  4403 | `	pGen->pIn++;` |
|  132257 |  4404 | `	pToken = pGen->pIn;` |
|       - |  4405 | `	/* Create the conditional block */` |
|  132257 |  4406 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  132257 |  4407 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4408 | `		return SXERR_ABORT;` |
|       - |  4409 | `	}` |
|       - |  4410 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   72435 |  4411 | `	for(;;){` |
|  144875 |  4412 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4413 | `			/* Syntax error */` |
|     ! 0 |  4414 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4415 | `				pToken--;` |
|     ! 0 |  4416 | `			}` |
|     ! 0 |  4417 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4418 | `			if( rc == SXERR_ABORT ){` |
|       - |  4419 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4420 | `				return SXERR_ABORT;` |
|       - |  4421 | `			}` |
|     ! 0 |  4422 | `			goto Synchronize;` |
|       - |  4423 | `		}` |
|       - |  4424 | `		/* Jump the left parenthesis '(' */` |
|  144875 |  4425 | `		pToken++;` |
|       - |  4426 | `		/* Delimit the condition */` |
|  144875 |  4427 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  144875 |  4428 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4429 | `			/* Syntax error */` |
|     ! 0 |  4430 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4431 | `				pToken--;` |
|     ! 0 |  4432 | `			}` |
|     ! 0 |  4433 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4434 | `			if( rc == SXERR_ABORT ){` |
|       - |  4435 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4436 | `				return SXERR_ABORT;` |
|       - |  4437 | `			}` |
|     ! 0 |  4438 | `			goto Synchronize;` |
|       - |  4439 | `		}` |
|       - |  4440 | `		/* Swap token streams */` |
|  144875 |  4441 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4442 | `		/* Compile the condition */` |
|  144875 |  4443 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4444 | `		/* Update token stream */` |
|  144875 |  4445 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4446 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4447 | `			pGen->pIn++;` |
|     ! 0 |  4448 | `		}` |
|  144875 |  4449 | `		pGen->pIn  = &pEnd[1];` |
|  144875 |  4450 | `		pGen->pEnd = pTmp;` |
|  144875 |  4451 | `		if( rc == SXERR_ABORT ){` |
|       - |  4452 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4453 | `			return SXERR_ABORT;` |
|       - |  4454 | `		}` |
|       - |  4455 | `		/* Emit the false jump */` |
|  144875 |  4456 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4457 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  144875 |  4458 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4459 | `		/* Compile the body */` |
|  144875 |  4460 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  144875 |  4461 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4462 | `			return SXERR_ABORT;` |
|       - |  4463 | `		}` |
|  144875 |  4464 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   40425 |  4465 | `			break;` |
|       - |  4466 | `		}` |
|       - |  4467 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   64035 |  4468 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   64035 |  4469 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   41231 |  4470 | `			break;` |
|       - |  4471 | `		}` |
|       - |  4472 | `		/* Emit the unconditional jump */` |
|   22809 |  4473 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4474 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   22809 |  4475 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   22809 |  4476 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   16487 |  4477 | `			pToken = &pGen->pIn[1];` |
|   16487 |  4478 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    6326 |  4479 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5098 |  4480 | `					break;` |
|       - |  4481 | `			}` |
|    6301 |  4482 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3148 |  4483 | `		}` |
|   12623 |  4484 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4485 | `		/* Synchronize cursors */` |
|   12623 |  4486 | `		pToken = pGen->pIn;` |
|       - |  4487 | `		/* Fix the false jump */` |
|   12623 |  4488 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4489 | `	} /* For(;;) */` |
|       - |  4490 | `	/* Fix the false jump */` |
|  132257 |  4491 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  132257 |  4492 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   51412 |  4493 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4494 | `			/* Compile the else block */` |
|   10191 |  4495 | `			pGen->pIn++;` |
|   10191 |  4496 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   10191 |  4497 | `			if( rc == SXERR_ABORT ){` |
|       - |  4498 |  |
|     ! 0 |  4499 | `				return SXERR_ABORT;` |
|       - |  4500 | `			}` |
|    5093 |  4501 | `	}` |
|  132257 |  4502 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4503 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  132257 |  4504 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4505 | `	/* Release the conditional block */` |
|  132257 |  4506 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4507 | `	/* Statement successfully compiled */` |
|  132257 |  4508 | `	return SXRET_OK;` |
|     ! 0 |  4509 | `Synchronize:` |
|       - |  4510 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4511 | `	 */` |
|     ! 0 |  4512 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4513 | `		pGen->pIn++;` |
|     ! 0 |  4514 | `	}` |
|     ! 0 |  4515 | `	return SXRET_OK;` |
|   66131 |  4516 |  |
|       - |  4517 | `/*` |
|       - |  4518 | ` * Compile the global construct.` |
|       - |  4519 | ` * According to the PHP language reference` |
|       - |  4520 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4521 | ` *  to be used in that function.` |
|       - |  4522 | ` *  Example #1 Using global` |
|       - |  4523 | ` *  <?php` |
|       - |  4524 | ` *   $a = 1;` |
|       - |  4525 | ` *   $b = 2;` |
|       - |  4526 | ` *   function Sum()` |
|       - |  4527 | ` *   {` |
|       - |  4528 | ` *    global $a, $b;` |
|       - |  4529 | ` *    $b = $a + $b;` |
|       - |  4530 | ` *   }` |
|       - |  4531 | ` *   Sum();` |
|       - |  4532 | ` *   echo $b;` |
|       - |  4533 | ` *  ?>` |
|       - |  4534 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4535 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4536 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4537 | ` */` |
|      36 |  4538 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4539 |  |
|      41 |  4540 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4541 | `	sxi32 nExpr;` |
|       - |  4542 | `	sxi32 rc;` |
|       - |  4543 | `	/* Jump the 'global' keyword */` |
|      41 |  4544 | `	pGen->pIn++;` |
|      41 |  4545 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4546 | `		/* Nothing to process */` |
|     ! 0 |  4547 | `		return SXRET_OK;` |
|       - |  4548 | `	}` |
|      41 |  4549 | `	pTmp = pGen->pEnd;` |
|      41 |  4550 | `	nExpr = 0;` |
|      87 |  4551 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4552 | `		if( pGen->pIn < pNext ){` |
|      51 |  4553 | `			pGen->pEnd = pNext;` |
|      51 |  4554 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4555 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4556 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4557 | `					return SXERR_ABORT;` |
|       - |  4558 | `				}` |
|     ! 0 |  4559 | `			}else{` |
|      51 |  4560 | `				pGen->pIn++;` |
|      51 |  4561 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4562 | `					/* Emit a warning */` |
|     ! 0 |  4563 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4564 | `				}else{` |
|      51 |  4565 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4566 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4567 | `						return SXERR_ABORT;` |
|      51 |  4568 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4569 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4570 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4571 | `							/* Variable name, not a constant */` |
|      51 |  4572 | `							pLast->iP1 = 0;` |
|      23 |  4573 | `						}` |
|      51 |  4574 | `						nExpr++;` |
|      23 |  4575 | `					}` |
|       - |  4576 | `				}` |
|       - |  4577 | `			}` |
|      23 |  4578 | `		}` |
|       - |  4579 | `		/* Next expression in the stream */` |
|      51 |  4580 | `		pGen->pIn = pNext;` |
|       - |  4581 | `		/* Jump trailing commas */` |
|      61 |  4582 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4583 | `			pGen->pIn++;` |
|       5 |  4584 | `		}` |
|       5 |  4585 | `	}` |
|       - |  4586 | `	/* Restore token stream */` |
|      41 |  4587 | `	pGen->pEnd = pTmp;` |
|      41 |  4588 | `	if( nExpr > 0 ){` |
|       - |  4589 | `		/* Emit the uplink instruction */` |
|      41 |  4590 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4591 | `	}` |
|      41 |  4592 | `	return SXRET_OK;` |
|      23 |  4593 |  |
|       - |  4594 | `/*` |
|       - |  4595 | ` * Compile the return statement.` |
|       - |  4596 | ` * According to the PHP language reference` |
|       - |  4597 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4598 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4599 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4600 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4601 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4602 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4603 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4604 | ` *  from within the main script file, then script execution end.` |
|       - |  4605 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4606 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4607 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4608 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4609 | ` */` |
|  208826 |  4610 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4611 |  |
|  208831 |  4612 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4613 | `	sxi32 rc;` |
|       - |  4614 | `	/* Jump the 'return' keyword */` |
|  208831 |  4615 | `	pGen->pIn++;` |
|  208831 |  4616 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4617 | `		/* Compile the expression */` |
|  208807 |  4618 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  208807 |  4619 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4620 | `			return SXERR_ABORT;` |
|  208807 |  4621 | `		}else if(rc != SXERR_EMPTY ){` |
|  208807 |  4622 | `			nRet = 1;` |
|  104401 |  4623 | `		}` |
|  104401 |  4624 | `	}` |
|       - |  4625 | `	/* Emit the done instruction */` |
|  208831 |  4626 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  208831 |  4627 | `	return SXRET_OK;` |
|  104418 |  4628 |  |
|       - |  4629 | `/*` |
|       - |  4630 | ` * Compile a yield expression.` |
|       - |  4631 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4632 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4633 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4634 | ` */` |
|      72 |  4635 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4636 |  |
|       - |  4637 | `	SyToken *pTmp, *pSplit;` |
|      77 |  4638 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      77 |  4639 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4640 | `	sxi32 rc;` |
|      36 |  4641 | `	(void)iCompileFlag;` |
|       - |  4642 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      77 |  4643 | `	pGen->pIn++;` |
|       - |  4644 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4645 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      77 |  4646 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4647 | `		/* Bare yield — no value */` |
|     ! 0 |  4648 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4649 | `		return SXRET_OK;` |
|       - |  4650 | `	}` |
|       - |  4651 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      77 |  4652 | `	pSplit = 0;` |
|       - |  4653 | `	{` |
|      77 |  4654 | `		SyToken *pCur = pGen->pIn;` |
|      77 |  4655 | `		sxi32 nNest = 0;` |
|     163 |  4656 | `		while( pCur < pGen->pEnd ){` |
|     105 |  4657 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4658 | `				nNest++;` |
|     105 |  4659 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4660 | `				nNest--;` |
|     105 |  4661 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4662 | `				pSplit = pCur;` |
|      16 |  4663 | `				break;` |
|       - |  4664 | `			}` |
|      91 |  4665 | `			pCur++;` |
|       5 |  4666 | `		}` |
|       - |  4667 | `	}` |
|      77 |  4668 | `	pTmp = pGen->pEnd;` |
|      77 |  4669 | `	if( pSplit ){` |
|       - |  4670 | `		/* yield $key => $value */` |
|      16 |  4671 | `		pGen->pEnd = pSplit;` |
|      16 |  4672 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4673 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4674 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4675 | `		pGen->pEnd = pTmp;` |
|      16 |  4676 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4677 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4678 | `		iP1 = 1;` |
|      16 |  4679 | `		iP2 = 1;` |
|       9 |  4680 | `	}else{` |
|       - |  4681 | `		/* yield $value */` |
|      63 |  4682 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      63 |  4683 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      63 |  4684 | `		if( rc != SXERR_EMPTY ){` |
|      63 |  4685 | `			iP1 = 1;` |
|      29 |  4686 | `		}` |
|       - |  4687 | `	}` |
|      77 |  4688 | `	pGen->pEnd = pTmp;` |
|      77 |  4689 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      77 |  4690 | `	return SXRET_OK;` |
|      41 |  4691 |  |
|       - |  4692 | `/*` |
|       - |  4693 | ` * Compile the die/exit language construct.` |
|       - |  4694 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4695 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4696 | ` */` |
|     120 |  4697 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4698 |  |
|     125 |  4699 | `	sxi32 nExpr = 0;` |
|       - |  4700 | `	sxi32 rc;` |
|       - |  4701 | `	/* Jump the die/exit keyword */` |
|     125 |  4702 | `	pGen->pIn++;` |
|     125 |  4703 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4704 | `		/* Compile the expression */` |
|     125 |  4705 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4706 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4707 | `			return SXERR_ABORT;` |
|     125 |  4708 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4709 | `			nExpr = 1;` |
|      60 |  4710 | `		}` |
|      60 |  4711 | `	}` |
|       - |  4712 | `	/* Emit the HALT instruction */` |
|     125 |  4713 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4714 | `	return SXRET_OK;` |
|      65 |  4715 |  |
|       - |  4716 | `/*` |
|       - |  4717 | ` * Compile the 'echo' language construct.` |
|       - |  4718 | ` */` |
|   13340 |  4719 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4720 |  |
|   13345 |  4721 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4722 | `	sxi32 rc;` |
|       - |  4723 | `	/* Jump the 'echo' keyword */` |
|   13345 |  4724 | `	pGen->pIn++;` |
|       - |  4725 | `	/* Compile arguments one after one */` |
|   13345 |  4726 | `	pTmp = pGen->pEnd;` |
|   28853 |  4727 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   15513 |  4728 | `		if( pGen->pIn < pNext ){` |
|   15513 |  4729 | `			pGen->pEnd = pNext;` |
|   15513 |  4730 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   15513 |  4731 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4732 | `				return SXERR_ABORT;` |
|   15513 |  4733 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4734 | `				/* Emit the consume instruction */` |
|   15489 |  4735 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    7742 |  4736 | `			}` |
|    7754 |  4737 | `		}` |
|       - |  4738 | `		/* Jump trailing commas */` |
|   17681 |  4739 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    2173 |  4740 | `			pNext++;` |
|       5 |  4741 | `		}` |
|   15513 |  4742 | `		pGen->pIn = pNext;` |
|       5 |  4743 | `	}` |
|       - |  4744 | `	/* Restore token stream */` |
|   13345 |  4745 | `	pGen->pEnd = pTmp;` |
|   13345 |  4746 | `	return SXRET_OK;` |
|    6675 |  4747 |  |
|       - |  4748 | `/*` |
|       - |  4749 | ` * Compile the static statement.` |
|       - |  4750 | ` * According to the PHP language reference` |
|       - |  4751 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4752 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4753 | ` *  when program execution leaves this scope.` |
|       - |  4754 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4755 | ` * Symisc eXtension.` |
|       - |  4756 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4757 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4758 | ` *  Example` |
|       - |  4759 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4760 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4761 | ` */` |
|       6 |  4762 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       2 |  4763 |  |
|       - |  4764 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4765 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4766 | `	GenBlock *pBlock;` |
|       - |  4767 | `	SyString *pName;` |
|       - |  4768 | `	char *zDup;` |
|       - |  4769 | `	sxu32 nLine;` |
|       - |  4770 | `	sxi32 rc;` |
|       - |  4771 | `	/* Jump the static keyword */` |
|       8 |  4772 | `	nLine = pGen->pIn->nLine;` |
|       8 |  4773 | `	pGen->pIn++;` |
|       - |  4774 | `	/* Extract the enclosing function if any */` |
|       8 |  4775 | `	pBlock = pGen->pCurrent;` |
|      14 |  4776 | `	while( pBlock ){` |
|      14 |  4777 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       8 |  4778 | `			break;` |
|       - |  4779 | `		}` |
|       - |  4780 | `		/* Point to the upper block */` |
|       8 |  4781 | `		pBlock = pBlock->pParent;` |
|       2 |  4782 | `	}` |
|       8 |  4783 | `	if( pBlock == 0 ){` |
|       - |  4784 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4785 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4786 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4787 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4788 | `				return SXERR_ABORT;` |
|       - |  4789 | `			}` |
|     ! 0 |  4790 | `			goto Synchronize;` |
|       - |  4791 | `		}` |
|       - |  4792 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4793 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4794 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4795 | `			return SXERR_ABORT;` |
|     ! 0 |  4796 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4797 | `			/* Emit the POP instruction */` |
|     ! 0 |  4798 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4799 | `		}` |
|     ! 0 |  4800 | `		return SXRET_OK;` |
|       - |  4801 | `	}` |
|       8 |  4802 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4803 | `	/* Make sure we are dealing with a valid statement */` |
|       8 |  4804 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       4 |  4805 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4806 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4807 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4808 | `				return SXERR_ABORT;` |
|       - |  4809 | `			}` |
|       3 |  4810 | `			goto Synchronize;` |
|       - |  4811 | `	}` |
|       5 |  4812 | `	pGen->pIn++;` |
|       - |  4813 | `	/* Extract variable name */` |
|       5 |  4814 | `	pName = &pGen->pIn->sData;` |
|       5 |  4815 | `	pGen->pIn++; /* Jump the var name */` |
|       5 |  4816 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4817 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4818 | `		goto Synchronize;` |
|       - |  4819 | `	}` |
|       - |  4820 | `	/* Initialize the structure describing the static variable */` |
|       5 |  4821 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       5 |  4822 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4823 | `	/* Duplicate variable name */` |
|       5 |  4824 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       5 |  4825 | `	if( zDup == 0 ){` |
|     ! 0 |  4826 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4827 | `		return SXERR_ABORT;` |
|       - |  4828 | `	}` |
|       5 |  4829 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4830 | `	/* Check if we have an expression to compile */` |
|       5 |  4831 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4832 | `		SySet *pInstrContainer;` |
|       - |  4833 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4834 | `		 * Static variable can take any complex expression including function` |
|       - |  4835 | `		 * call as their initialization value.` |
|       - |  4836 | `		 * Example:` |
|       - |  4837 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4838 | `		 */` |
|       5 |  4839 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4840 | `		/* Swap bytecode container */` |
|       5 |  4841 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       5 |  4842 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4843 | `		/* Compile the expression */` |
|       5 |  4844 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4845 | `		/* Emit the done instruction */` |
|       5 |  4846 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4847 | `		/* Restore default bytecode container */` |
|       5 |  4848 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       2 |  4849 | `	}` |
|       - |  4850 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       5 |  4851 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       5 |  4852 | `	return SXRET_OK;` |
|       1 |  4853 | `Synchronize:` |
|       - |  4854 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  4855 | `	 * statement.` |
|       - |  4856 | `	 */` |
|       5 |  4857 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  4858 | `		pGen->pIn++;` |
|       1 |  4859 | `	}` |
|       3 |  4860 | `	return SXRET_OK;` |
|       5 |  4861 |  |
|       - |  4862 | `/*` |
|       - |  4863 | ` * Compile the var statement.` |
|       - |  4864 | ` * Symisc Extension:` |
|       - |  4865 | ` *      var statement can be used outside of a class definition.` |
|       - |  4866 | ` */` |
|       4 |  4867 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  4868 |  |
|       - |  4869 | `	sxu32 nLine;` |
|       - |  4870 | `	sxi32 rc;` |
|       5 |  4871 | `	nLine = pGen->pIn->nLine;` |
|       - |  4872 | `	/* Jump the 'var' keyword */` |
|       5 |  4873 | `	pGen->pIn++;` |
|       5 |  4874 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  4875 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  4876 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  4877 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  4878 | `			pGen->pIn++;` |
|     ! 0 |  4879 | `		}` |
|     ! 0 |  4880 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4881 | `			return SXERR_ABORT;` |
|       - |  4882 | `		}` |
|     ! 0 |  4883 | `	}else{` |
|       - |  4884 | `		/* Compile the expression */` |
|       5 |  4885 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  4886 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4887 | `			return SXERR_ABORT;` |
|       5 |  4888 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  4889 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  4890 | `		}` |
|       - |  4891 | `	}` |
|       5 |  4892 | `	return SXRET_OK;` |
|       3 |  4893 |  |
|       - |  4894 | `/*` |
|       - |  4895 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  4896 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  4897 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  4898 | ` */` |
|       - |  4899 | `/*` |
|       - |  4900 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  4901 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  4902 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  4903 | ` * qualified name and updates the instruction's operand index.` |
|       - |  4904 | ` *` |
|       - |  4905 | ` * Resolution order:` |
|       - |  4906 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  4907 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  4908 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  4909 | ` *` |
|       - |  4910 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  4911 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  4912 | ` * Returns the (possibly new) literal index.` |
|       - |  4913 | ` */` |
|  389986 |  4914 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  4915 |  |
|       - |  4916 | `	ph7_value *pLit;` |
|       - |  4917 | `	const char *zLit;` |
|       - |  4918 | `	SyString sQualified;` |
|       - |  4919 | `	sxu32 nLit;` |
|       - |  4920 | `	sxu32 k;` |
|       - |  4921 | `	sxu32 nNewIdx;` |
|       - |  4922 | `	int hasNsSep;` |
|       - |  4923 | `	SyHashEntry *pImport;` |
|       - |  4924 | `	ph7_value *pNew;` |
|  389991 |  4925 | `	if( pFromImport ){` |
|  372891 |  4926 | `		*pFromImport = 0;` |
|  186443 |  4927 | `	}` |
|  389991 |  4928 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  389991 |  4929 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4930 | `		return nOrigIdx;` |
|       - |  4931 | `	}` |
|  389991 |  4932 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  389991 |  4933 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4934 | `	/* Skip if already qualified (contains backslash) */` |
|  389991 |  4935 | `	hasNsSep = 0;` |
| 4219351 |  4936 | `	for( k = 0; k < nLit; k++ ){` |
| 3829373 |  4937 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1914685 |  4938 | `	}` |
|  389991 |  4939 | `	if( hasNsSep ){` |
|      11 |  4940 | `		return nOrigIdx;` |
|       - |  4941 | `	}` |
|       - |  4942 | `	/* Check use imports first (works even outside namespaces) */` |
|  389983 |  4943 | `	SyBlobReset(&pGen->sWorker);` |
|  389983 |  4944 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  389983 |  4945 | `	if( pImport ){` |
|      41 |  4946 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  4947 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  4948 | `		if( pFromImport ){` |
|      18 |  4949 | `			*pFromImport = 1;` |
|       8 |  4950 | `		}` |
|      23 |  4951 | `	}else{` |
|  389947 |  4952 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  389857 |  4953 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  4954 | `		}` |
|       - |  4955 | `		/* Prepend current namespace */` |
|      95 |  4956 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  4957 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  4958 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  4959 | `	}` |
|       - |  4960 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  4961 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  4962 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  4963 | `		return nNewIdx; /* Already interned */` |
|       - |  4964 | `	}` |
|      79 |  4965 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  4966 | `	if( pNew == 0 ){` |
|     ! 0 |  4967 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  4968 | `	}` |
|      79 |  4969 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  4970 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  4971 | `	return nNewIdx;` |
|  194998 |  4972 |  |
|       - |  4973 | `/*` |
|       - |  4974 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4975 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4976 | ` */` |
|   85692 |  4977 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  4978 |  |
|       - |  4979 | `	SyHashEntry *pImport;` |
|       - |  4980 | `	/* Check use imports first */` |
|   85697 |  4981 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   85697 |  4982 | `	if( pImport ){` |
|      15 |  4983 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 |  4984 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      15 |  4985 | `		return;` |
|       - |  4986 | `	}` |
|       - |  4987 | `	/* Prepend current namespace if active */` |
|   85685 |  4988 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4989 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4990 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4991 | `	}` |
|   85685 |  4992 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   42851 |  4993 |  |
|       - |  4994 | `/*` |
|       - |  4995 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  4996 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  4997 | ` * The caller must release pOut when done.` |
|       - |  4998 | ` */` |
|  120688 |  4999 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5000 |  |
|  120693 |  5001 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5002 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5003 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5004 | `	}` |
|  120693 |  5005 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  120693 |  5006 |  |
|       - |  5007 | `/*` |
|       - |  5008 | ` * Compile a namespace statement` |
|       - |  5009 | ` * According to the PHP language reference manual` |
|       - |  5010 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5011 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5012 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5013 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5014 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5015 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5016 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5017 | ` *  programming world.` |
|       - |  5018 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5019 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5020 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5021 | ` *  classes/functions/constants.` |
|       - |  5022 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5023 | ` *  readability of source code.` |
|       - |  5024 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5025 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5026 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5027 | ` *       class MyClass {}` |
|       - |  5028 | ` *       function myfunction() {}` |
|       - |  5029 | ` *       const MYCONST = 1;` |
|       - |  5030 | ` *       $a = new MyClass;` |
|       - |  5031 | ` *       $c = new \my\name\MyClass;` |
|       - |  5032 | ` *       $a = strlen('hi');` |
|       - |  5033 | ` *       $d = namespace\MYCONST;` |
|       - |  5034 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5035 | ` *       echo constant($d);` |
|       - |  5036 | ` * NOTE` |
|       - |  5037 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5038 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5039 | ` */` |
|       - |  5040 | `/*` |
|       - |  5041 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5042 | ` */` |
|      14 |  5043 | `static const char * TokenTypeName(sxu32 nType)` |
|       4 |  5044 |  |
|      18 |  5045 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 |  5046 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 |  5047 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 |  5048 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 |  5049 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 |  5050 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5051 | `	return "token";` |
|      11 |  5052 |  |
|     106 |  5053 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5054 |  |
|       - |  5055 | `	sxu32 nLine;` |
|       - |  5056 | `	sxi32 rc;` |
|     111 |  5057 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5058 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5059 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5060 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5061 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5062 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5063 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5064 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5065 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5066 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5067 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5068 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5069 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5070 | `		return SXRET_OK;` |
|       - |  5071 | `	}` |
|     111 |  5072 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5073 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5074 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5075 | `		return SXRET_OK;` |
|       - |  5076 | `	}` |
|     111 |  5077 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5078 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5079 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5080 | `		return SXRET_OK;` |
|       - |  5081 | `	}` |
|       - |  5082 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5083 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5084 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5085 | `			/* Append backslash separator */` |
|      27 |  5086 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5087 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5088 | `			}` |
|      16 |  5089 | `		}else{` |
|       - |  5090 | `			/* Append identifier */` |
|     131 |  5091 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5092 | `		}` |
|     153 |  5093 | `		pGen->pIn++;` |
|       5 |  5094 | `	}` |
|       - |  5095 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5096 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5097 | `	{` |
|     111 |  5098 | `		char *zNsDup = 0;` |
|     111 |  5099 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5100 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5101 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5102 | `		}` |
|     111 |  5103 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5104 | `	}` |
|     111 |  5105 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5106 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5107 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5108 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5109 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5110 | `			return SXERR_ABORT;` |
|       - |  5111 | `		}` |
|       2 |  5112 | `	}` |
|     111 |  5113 | `	return SXRET_OK;` |
|      58 |  5114 |  |
|       - |  5115 | `/*` |
|       - |  5116 | ` * Compile the 'use' statement` |
|       - |  5117 | ` * According to the PHP language reference manual` |
|       - |  5118 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5119 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5120 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5121 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5122 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5123 | ` *  a function or constant is not supported.` |
|       - |  5124 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5125 | ` * NOTE` |
|       - |  5126 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5127 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5128 | ` */` |
|      68 |  5129 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5130 |  |
|       - |  5131 | `	sxu32 nLine;` |
|       - |  5132 | `	sxi32 rc;` |
|       - |  5133 | `	SyBlob sPath;` |
|       - |  5134 | `	SyString sAlias;` |
|       - |  5135 | `	SyToken *pLast;` |
|       - |  5136 | `	char *zDup;` |
|       - |  5137 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5138 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5139 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5140 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5141 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5142 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5143 | `	iUseType = 0;` |
|      73 |  5144 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5145 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5146 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5147 | `			iUseType = 1;` |
|      16 |  5148 | `			pGen->pIn++;` |
|      23 |  5149 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5150 | `			iUseType = 2;` |
|      16 |  5151 | `			pGen->pIn++;` |
|       7 |  5152 | `		}` |
|      14 |  5153 | `	}` |
|       - |  5154 | `	/* Select target hash tables based on import type */` |
|      73 |  5155 | `	switch( iUseType ){` |
|       7 |  5156 | `		case 1:` |
|      16 |  5157 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5158 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5159 | `			break;` |
|       7 |  5160 | `		case 2:` |
|      16 |  5161 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5162 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5163 | `			break;` |
|      20 |  5164 | `		default:` |
|      45 |  5165 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5166 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5167 | `			break;` |
|       - |  5168 | `	}` |
|      73 |  5169 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5170 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5171 | `	for(;;){` |
|      75 |  5172 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5173 | `			break;` |
|       - |  5174 | `		}` |
|      75 |  5175 | `		SyBlobReset(&sPath);` |
|      75 |  5176 | `		pLast = 0;` |
|       - |  5177 | `		/* Collect the full namespace path */` |
|     261 |  5178 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5179 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5180 | `				pLast = pGen->pIn;` |
|     131 |  5181 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5182 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5183 | `				}` |
|     131 |  5184 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5185 | `			}` |
|     191 |  5186 | `			pGen->pIn++;` |
|       5 |  5187 | `		}` |
|      75 |  5188 | `		if( pLast == 0 ){` |
|       - |  5189 | `			/* Empty path */` |
|       6 |  5190 | `			break;` |
|       - |  5191 | `		}` |
|       - |  5192 | `		/* Default alias is the last component of the path */` |
|      71 |  5193 | `		sAlias = pLast->sData;` |
|       - |  5194 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5195 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5196 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      19 |  5197 | `			pGen->pIn++; /* Jump 'as' */` |
|      19 |  5198 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      19 |  5199 | `				sAlias = pGen->pIn->sData;` |
|      19 |  5200 | `				pGen->pIn++;` |
|       8 |  5201 | `			}` |
|       8 |  5202 | `		}` |
|       - |  5203 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5204 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5205 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5206 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5207 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5208 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5209 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5210 | `				return SXERR_ABORT;` |
|       - |  5211 | `			}` |
|       2 |  5212 | `		}` |
|       - |  5213 | `		/* Register the import: alias -> FQN.` |
|       - |  5214 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5215 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5216 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5217 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5218 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5219 | `		if( zDup ){` |
|      71 |  5220 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5221 | `			if( pVmHash ){` |
|       - |  5222 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5223 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5224 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5225 | `				if( zAliasDup ){` |
|      43 |  5226 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5227 | `				}` |
|      19 |  5228 | `			}` |
|      71 |  5229 | `			if( iUseType == 2 ){` |
|       - |  5230 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5231 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5232 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5233 | `				if( zAliasDup ){` |
|       - |  5234 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5235 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5236 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5237 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5238 | `					if( azPair ){` |
|      16 |  5239 | `						azPair[0] = zAliasDup;` |
|      16 |  5240 | `						azPair[1] = zDup;` |
|      16 |  5241 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5242 | `					}` |
|       7 |  5243 | `				}` |
|       7 |  5244 | `			}` |
|      33 |  5245 | `		}` |
|       - |  5246 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5247 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5248 | `			pGen->pIn++;` |
|       2 |  5249 | `		}else{` |
|      37 |  5250 | `			break;` |
|       - |  5251 | `		}` |
|       1 |  5252 | `	}` |
|      73 |  5253 | `	SyBlobRelease(&sPath);` |
|      73 |  5254 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5255 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5256 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5257 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5258 | `			return SXERR_ABORT;` |
|       - |  5259 | `		}` |
|       1 |  5260 | `	}` |
|      73 |  5261 | `	return SXRET_OK;` |
|      39 |  5262 |  |
|       - |  5263 | `/*` |
|       - |  5264 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5265 | ` *` |
|       - |  5266 | ` * According to the PHP language reference manual.` |
|       - |  5267 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5268 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5269 | ` *  declare (directive)` |
|       - |  5270 | ` *   statement` |
|       - |  5271 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5272 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5273 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5274 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5275 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5276 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5277 | ` * <?php` |
|       - |  5278 | ` * // these are the same:` |
|       - |  5279 | ` * // you can use this:` |
|       - |  5280 | ` * declare(ticks=1) {` |
|       - |  5281 | ` *   // entire script here` |
|       - |  5282 | ` * }` |
|       - |  5283 | ` * // or you can use this:` |
|       - |  5284 | ` * declare(ticks=1);` |
|       - |  5285 | ` * // entire script here` |
|       - |  5286 | ` * ?>` |
|       - |  5287 | ` *` |
|       - |  5288 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5289 | ` */` |
|       - |  5290 | `/*` |
|       - |  5291 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5292 | ` */` |
|      68 |  5293 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5294 |  |
|     103 |  5295 | `	return SyStringLength(pName) == nWant` |
|      68 |  5296 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5297 |  |
|       - |  5298 |  |
|      40 |  5299 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5300 |  |
|      45 |  5301 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5302 | `	SyToken *pBodyEnd = 0;` |
|       - |  5303 | `	SyToken *pBodyStart;` |
|       - |  5304 | `	SyToken *pCursor;` |
|       - |  5305 | `	int bHasStrictTypes;` |
|       - |  5306 | `	int bBlockForm;` |
|       - |  5307 | `	int bPlacementOk;` |
|       - |  5308 | `	sxi32 rc;` |
|      45 |  5309 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5310 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5311 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5312 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5313 | `			return SXERR_ABORT;` |
|       - |  5314 | `		}` |
|       6 |  5315 | `		goto Synchro;` |
|       - |  5316 | `	}` |
|      41 |  5317 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5318 | `	pBodyStart = pGen->pIn;` |
|       - |  5319 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5320 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5321 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5322 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5323 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5324 | `			return SXERR_ABORT;` |
|       - |  5325 | `		}` |
|     ! 0 |  5326 | `		return SXRET_OK;` |
|       - |  5327 | `	}` |
|       - |  5328 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5329 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5330 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5331 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5332 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5333 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5334 | `			return SXERR_ABORT;` |
|       - |  5335 | `		}` |
|     ! 0 |  5336 | `	}` |
|      41 |  5337 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5338 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5339 | `	bHasStrictTypes = 0;` |
|       - |  5340 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5341 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5342 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5343 | `	pCursor = pBodyStart;` |
|      53 |  5344 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5345 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5346 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5347 | `				bHasStrictTypes = 1;` |
|      37 |  5348 | `				break;` |
|       - |  5349 | `			}` |
|       2 |  5350 | `		}` |
|      14 |  5351 | `		pCursor++;` |
|       2 |  5352 | `	}` |
|      41 |  5353 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5354 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5355 | `			"strict_types declaration must not use block mode");` |
|       3 |  5356 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5357 | `		return SXRET_OK;` |
|       - |  5358 | `	}` |
|      39 |  5359 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5360 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5361 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5362 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5363 | `		return SXRET_OK;` |
|       - |  5364 | `	}` |
|       - |  5365 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5366 | `	pCursor = pBodyStart;` |
|      65 |  5367 | `	while( pCursor < pBodyEnd ){` |
|       - |  5368 | `		SyToken *pNameTok;` |
|       - |  5369 | `		SyToken *pEqTok;` |
|       - |  5370 | `		SyToken *pValTok;` |
|       - |  5371 | `		SyString *pDirName;` |
|       - |  5372 | `		int bIsStrict;` |
|       - |  5373 | `		int iStrictValue;` |
|      37 |  5374 | `		pNameTok = pCursor;` |
|      37 |  5375 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5376 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5377 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5378 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5379 | `			return SXRET_OK;` |
|       - |  5380 | `		}` |
|      37 |  5381 | `		pEqTok = pNameTok + 1;` |
|      37 |  5382 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5383 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5384 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5385 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5386 | `			return SXRET_OK;` |
|       - |  5387 | `		}` |
|      37 |  5388 | `		pValTok = pEqTok + 1;` |
|      37 |  5389 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5390 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5391 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5392 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5393 | `			return SXRET_OK;` |
|       - |  5394 | `		}` |
|      37 |  5395 | `		pDirName = &pNameTok->sData;` |
|      37 |  5396 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5397 | `		if( bIsStrict ){` |
|       - |  5398 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5399 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5400 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5401 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5402 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5403 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5404 | `				return SXRET_OK;` |
|       - |  5405 | `			}` |
|      33 |  5406 | `			iStrictValue = -1;` |
|      33 |  5407 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5408 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5409 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5410 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5411 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5412 | `			}` |
|      33 |  5413 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5414 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5415 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5416 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5417 | `				return SXRET_OK;` |
|       - |  5418 | `			}` |
|      30 |  5419 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5420 | `		}else{` |
|       - |  5421 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5422 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5423 | `			 * behavior don't regress. */` |
|       8 |  5424 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5425 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5426 | `				ph7_lib_version()` |
|       - |  5427 | `				);` |
|       - |  5428 | `		}` |
|      35 |  5429 | `		pCursor = pValTok + 1;` |
|       - |  5430 | `		/* Consume separating comma (or end). */` |
|      35 |  5431 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5432 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5433 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5434 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5435 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5436 | `				return SXRET_OK;` |
|       - |  5437 | `			}` |
|       3 |  5438 | `			pCursor++;` |
|       1 |  5439 | `		}` |
|       5 |  5440 | `	}` |
|       - |  5441 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5442 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5443 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5444 | `	return SXRET_OK;` |
|       2 |  5445 | `Synchro:` |
|       - |  5446 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5447 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5448 | `		pGen->pIn++;` |
|       2 |  5449 | `	}` |
|       6 |  5450 | `	return SXRET_OK;` |
|      25 |  5451 |  |
|       - |  5452 | `/*` |
|       - |  5453 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5454 | ` * as follows:` |
|       - |  5455 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5456 | ` * {` |
|       - |  5457 | ` *   return "Making a cup of $type.\n";` |
|       - |  5458 | ` * }` |
|       - |  5459 | ` * Symisc eXtension.` |
|       - |  5460 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5461 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5462 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5463 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5464 | ` *      {` |
|       - |  5465 | ` *       var_dump($a);` |
|       - |  5466 | ` *      }` |
|       - |  5467 | ` *     //call test without args` |
|       - |  5468 | ` *      test();` |
|       - |  5469 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5470 | ` *      Example:` |
|       - |  5471 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5472 | ` * 3 -) Function overloading!!` |
|       - |  5473 | ` *      Example:` |
|       - |  5474 | ` *      function foo($a) {` |
|       - |  5475 | ` *   	  return $a.PHP_EOL;` |
|       - |  5476 | ` *	    }` |
|       - |  5477 | ` *	    function foo($a, $b) {` |
|       - |  5478 | ` *   	  return $a + $b;` |
|       - |  5479 | ` *	    }` |
|       - |  5480 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5481 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5482 | ` *      // Same arg` |
|       - |  5483 | ` *	   function foo(string $a)` |
|       - |  5484 | ` *	   {` |
|       - |  5485 | ` *	     echo "a is a string\n";` |
|       - |  5486 | ` *	     var_dump($a);` |
|       - |  5487 | ` *	   }` |
|       - |  5488 | ` *	  function foo(int $a)` |
|       - |  5489 | ` *	  {` |
|       - |  5490 | ` *	    echo "a is integer\n";` |
|       - |  5491 | ` *	    var_dump($a);` |
|       - |  5492 | ` *	  }` |
|       - |  5493 | ` *	  function foo(array $a)` |
|       - |  5494 | ` *	  {` |
|       - |  5495 | ` * 	    echo "a is an array\n";` |
|       - |  5496 | ` * 	    var_dump($a);` |
|       - |  5497 | ` *	  }` |
|       - |  5498 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5499 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5500 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5501 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5502 | ` * introduced by the PH7 engine.` |
|       - |  5503 | ` */` |
|   59846 |  5504 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5505 |  |
|       - |  5506 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5507 | `	SySet *pInstrContainer;` |
|       - |  5508 | `	sxi32 rc;` |
|       - |  5509 | `	/* Swap token stream */` |
|   59851 |  5510 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   59851 |  5511 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   59851 |  5512 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5513 | `	/* Compile the expression holding the argument value */` |
|   59851 |  5514 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5515 | `	/* Emit the done instruction */` |
|   59851 |  5516 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   59851 |  5517 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   59851 |  5518 | `	RE_SWAP_DELIMITER(pGen);` |
|   59851 |  5519 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5520 | `		return SXERR_ABORT;` |
|       - |  5521 | `	}` |
|   59851 |  5522 | `	return SXRET_OK;` |
|   29928 |  5523 |  |
|       - |  5524 | `/*` |
|       - |  5525 | ` * Collect function arguments one after one.` |
|       - |  5526 | ` * According to the PHP language reference manual.` |
|       - |  5527 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5528 | ` * list of expressions.` |
|       - |  5529 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5530 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5531 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5532 | ` * for more information.` |
|       - |  5533 | ` * Example #1 Passing arrays to functions` |
|       - |  5534 | ` * <?php` |
|       - |  5535 | ` * function takes_array($input)` |
|       - |  5536 | ` * {` |
|       - |  5537 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5538 | ` * }` |
|       - |  5539 | ` * ?>` |
|       - |  5540 | ` * Making arguments be passed by reference` |
|       - |  5541 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5542 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5543 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5544 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5545 | ` * to the argument name in the function definition:` |
|       - |  5546 | ` * Example #2 Passing function parameters by reference` |
|       - |  5547 | ` * <?php` |
|       - |  5548 | ` * function add_some_extra(&$string)` |
|       - |  5549 | ` * {` |
|       - |  5550 | ` *   $string .= 'and something extra.';` |
|       - |  5551 | ` * }` |
|       - |  5552 | ` * $str = 'This is a string, ';` |
|       - |  5553 | ` * add_some_extra($str);` |
|       - |  5554 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5555 | ` * ?>` |
|       - |  5556 | ` *` |
|       - |  5557 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5558 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5559 | ` * on these extension.` |
|       - |  5560 | ` */` |
|   83002 |  5561 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5562 |  |
|       - |  5563 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5564 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5565 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5566 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5567 | `	sxi32 rc;` |
|       - |  5568 |  |
|   83007 |  5569 | `	pIn = pGen->pIn;` |
|   83007 |  5570 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5571 | `	/* Process arguments one after one */` |
|  103814 |  5572 | `	for(;;){` |
|  207633 |  5573 | `		if( pIn >= pEnd ){` |
|       - |  5574 | `			/* No more arguments to process */` |
|   82995 |  5575 | `			break;` |
|       - |  5576 | `		}` |
|  124643 |  5577 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  124643 |  5578 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  124643 |  5579 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  124643 |  5580 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5581 | `		/* Parse optional visibility modifier (constructor property promotion, PHP 8.0+) */` |
|  124643 |  5582 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   56827 |  5583 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   56827 |  5584 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      47 |  5585 | `				if( !bCtorCtx ){` |
|       6 |  5586 | `					if( bAbstractCtx ){` |
|       3 |  5587 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5588 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5589 | `					}else{` |
|       3 |  5590 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5591 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5592 | `					}` |
|       6 |  5593 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5594 | `						return SXERR_ABORT;` |
|       - |  5595 | `					}` |
|       6 |  5596 | `					return SXERR_SYNTAX;` |
|       - |  5597 | `				}` |
|      43 |  5598 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      43 |  5599 | `				if( nKw == PH7_TKWRD_PRIVATE ){` |
|       3 |  5600 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PRIVATE;` |
|      42 |  5601 | `				}else if( nKw == PH7_TKWRD_PROTECTED ){` |
|       3 |  5602 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PROTECTED;` |
|       2 |  5603 | `				}else{` |
|      39 |  5604 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PUBLIC;` |
|       - |  5605 | `				}` |
|      43 |  5606 | `				pIn++;` |
|      19 |  5607 | `			}` |
|   28409 |  5608 | `		}` |
|       - |  5609 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  157800 |  5610 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   97070 |  5611 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   67924 |  5612 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   66315 |  5613 | `			sxu32 nLineLocal = pIn->nLine;` |
|   66315 |  5614 | `			sxi32 iTFlags = 0;` |
|   66315 |  5615 | `			pGen->pIn = pIn;` |
|   66315 |  5616 | `			rc = GenStateParseUnionTypeDecl(` |
|   33155 |  5617 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   33155 |  5618 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5619 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5620 | `				/* bAllowVoid */ 0,` |
|   33155 |  5621 | `						nLineLocal);` |
|   66315 |  5622 | `			pIn = pGen->pIn;` |
|   66315 |  5623 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5624 | `				return SXERR_ABORT;` |
|   66315 |  5625 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5626 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5627 | `				return SXERR_SYNTAX;` |
|   66313 |  5628 | `			}else if( rc == SXERR_SYNTAX ){` |
|       6 |  5629 | `				if( pIn < pEnd ){` |
|       8 |  5630 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5631 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5632 | `						&pIn->sData);` |
|       4 |  5633 | `				}else{` |
|     ! 0 |  5634 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5635 | `						"syntax error, unexpected end of file");` |
|       - |  5636 | `				}` |
|       6 |  5637 | `				return SXERR_SYNTAX;` |
|       - |  5638 | `			}` |
|   66309 |  5639 | `			sArg.iFlags \|= iTFlags;` |
|   33152 |  5640 | `		}` |
|  124633 |  5641 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5642 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5643 | `			return rc;` |
|       - |  5644 | `		}` |
|  124633 |  5645 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5646 | `			/* Pass by reference,record that */` |
|    3181 |  5647 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3181 |  5648 | `			pIn++;` |
|    1588 |  5649 | `		}` |
|  124633 |  5650 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5651 | `			/* Variadic parameter: ...$args */` |
|      47 |  5652 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      47 |  5653 | `			pIn++;` |
|      21 |  5654 | `		}` |
|  124633 |  5655 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5656 | `			/* Invalid argument */` |
|     ! 0 |  5657 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5658 | `			return rc;` |
|       - |  5659 | `		}` |
|  124633 |  5660 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5661 | `		/* Copy argument name */` |
|  124633 |  5662 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  124633 |  5663 | `		if( zDup == 0 ){` |
|     ! 0 |  5664 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5665 | `			return SXERR_ABORT;` |
|       - |  5666 | `		}` |
|  124633 |  5667 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  124633 |  5668 | `		pIn++;` |
|  124633 |  5669 | `		if( pIn < pEnd ){` |
|   70005 |  5670 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5671 | `				SyToken *pDefend;` |
|   59853 |  5672 | `				sxi32 iNest = 0;` |
|   59853 |  5673 | `				pIn++; /* Jump the equal sign */` |
|   59853 |  5674 | `				pDefend = pIn;` |
|       - |  5675 | `				/* Process the default value associated with this argument */` |
|  125995 |  5676 | `				while( pDefend < pEnd ){` |
|   97635 |  5677 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   31493 |  5678 | `						break;` |
|       - |  5679 | `					}` |
|   66147 |  5680 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5681 | `						/* Increment nesting level */` |
|    3153 |  5682 | `						iNest++;` |
|   64573 |  5683 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5684 | `						/* Decrement nesting level */` |
|    3153 |  5685 | `						iNest--;` |
|    1574 |  5686 | `					}` |
|   66147 |  5687 | `					pDefend++;` |
|       5 |  5688 | `				}` |
|   59853 |  5689 | `				if( pIn >= pDefend ){` |
|       3 |  5690 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5691 | `					return rc;` |
|       - |  5692 | `				}` |
|       - |  5693 | `				/* Process default value */` |
|   59851 |  5694 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   59851 |  5695 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5696 | `					return rc;` |
|       - |  5697 | `				}` |
|       - |  5698 | `				/* Point beyond the default value */` |
|   59851 |  5699 | `				pIn = pDefend;` |
|   29923 |  5700 | `			}` |
|   70003 |  5701 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5702 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5703 | `				return rc;` |
|       - |  5704 | `			}` |
|   70003 |  5705 | `			pIn++; /* Jump the trailing comma */` |
|   34999 |  5706 | `		}` |
|       - |  5707 | `		/* Append argument signature */` |
|  124631 |  5708 | `		if( sArg.nType > 0 ){` |
|   66265 |  5709 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5710 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    9471 |  5711 | `				int marker = 'o';` |
|    9471 |  5712 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    9471 |  5713 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    4738 |  5714 | `			}else{` |
|       - |  5715 | `				int c;` |
|   56799 |  5716 | `				c = 'n'; /* cc warning */` |
|       - |  5717 | `				/* Type leading character */` |
|   56799 |  5718 | `				switch(sArg.nType){` |
|     ! 0 |  5719 | `				case MEMOBJ_HASHMAP:` |
|       - |  5720 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5721 | `					c = 'h';` |
|     ! 0 |  5722 | `					break;` |
|    7906 |  5723 | `				case MEMOBJ_INT:` |
|       - |  5724 | `					/* Integer */` |
|   15817 |  5725 | `					c = 'i';` |
|   15817 |  5726 | `					break;` |
|       1 |  5727 | `				case MEMOBJ_BOOL:` |
|       - |  5728 | `					/* Bool */` |
|       3 |  5729 | `					c = 'b';` |
|       3 |  5730 | `					break;` |
|       1 |  5731 | `				case MEMOBJ_REAL:` |
|       - |  5732 | `					/* Float */` |
|       3 |  5733 | `					c = 'f';` |
|       3 |  5734 | `					break;` |
|   20481 |  5735 | `				case MEMOBJ_STRING:` |
|       - |  5736 | `					/* String */` |
|   40967 |  5737 | `					c = 's';` |
|   40967 |  5738 | `					break;` |
|       7 |  5739 | `				case MEMOBJ_OBJ:` |
|       - |  5740 | `					/* Object */` |
|      16 |  5741 | `					c = 'o';` |
|      14 |  5742 | `					break;` |
|       1 |  5743 | `				default:` |
|       2 |  5744 | `					break;` |
|       - |  5745 | `				}` |
|   56799 |  5746 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5747 | `			}` |
|   33135 |  5748 | `		}else{` |
|       - |  5749 | `			/* No type is associated with this parameter which mean` |
|       - |  5750 | `			 * that this function is not condidate for overloading.` |
|       - |  5751 | `			 */` |
|   58371 |  5752 | `			SyBlobRelease(&sSig);` |
|       - |  5753 | `		}` |
|       - |  5754 | `		/* Save in the argument set */` |
|  124631 |  5755 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5756 | `	}` |
|   82995 |  5757 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5758 | `		/* Save function signature */` |
|   41059 |  5759 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   20527 |  5760 | `	}` |
|   82995 |  5761 | `	return SXRET_OK;` |
|   41506 |  5762 |  |
|       - |  5763 | `/*` |
|       - |  5764 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5765 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5766 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5767 | ` */` |
|  197018 |  5768 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5769 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5770 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5771 | `	)` |
|       5 |  5772 |  |
|       - |  5773 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5774 | `	GenBlock *pBlock;` |
|       - |  5775 | `	sxu32 nGotoOfft;` |
|       - |  5776 | `	sxi32 rc;` |
|       - |  5777 | `	/* Attach the new function */` |
|  197023 |  5778 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  197023 |  5779 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5780 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5781 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5782 | `		return SXERR_ABORT;` |
|       - |  5783 | `	}` |
|  197023 |  5784 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5785 | `	/* Swap bytecode containers */` |
|  197023 |  5786 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  197023 |  5787 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5788 | `	/* Emit constructor property promotion prologue:` |
|       - |  5789 | `	 *   $this->NAME = $NAME;` |
|       - |  5790 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5791 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5792 | `	{` |
|  197023 |  5793 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5794 | `		sxu32 i;` |
|  296343 |  5795 | `		for( i = 0; i < nArg; i++ ){` |
|   99325 |  5796 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5797 | `			char *zSrc;` |
|       - |  5798 | `			sxu32 nSrc,nName;` |
|       - |  5799 | `			SySet sToken;` |
|       - |  5800 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5801 | `			sxi32 rcPromote;` |
|   99325 |  5802 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   99295 |  5803 | `				continue;` |
|       - |  5804 | `			}` |
|       - |  5805 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5806 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5807 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5808 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5809 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      33 |  5810 | `			nName = SyStringLength(&pArg->sName);` |
|      33 |  5811 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      33 |  5812 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      33 |  5813 | `			if( zSrc == 0 ){` |
|     ! 0 |  5814 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5815 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5816 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5817 | `				return SXERR_ABORT;` |
|       - |  5818 | `			}` |
|       - |  5819 | `			{` |
|      33 |  5820 | `				char *z = zSrc;` |
|      33 |  5821 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      33 |  5822 | `				z += sizeof("$this->")-1;` |
|      33 |  5823 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      33 |  5824 | `				z += nName;` |
|      33 |  5825 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      33 |  5826 | `				z += sizeof(" = $")-1;` |
|      33 |  5827 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      33 |  5828 | `				z += nName;` |
|      33 |  5829 | `				*z = 0;` |
|       - |  5830 | `			}` |
|      33 |  5831 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      33 |  5832 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      33 |  5833 | `			pTmpIn = pGen->pIn;` |
|      33 |  5834 | `			pTmpEnd = pGen->pEnd;` |
|      33 |  5835 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      33 |  5836 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      33 |  5837 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 |  5838 | `			pGen->pIn = pTmpIn;` |
|      33 |  5839 | `			pGen->pEnd = pTmpEnd;` |
|      33 |  5840 | `			SySetRelease(&sToken);` |
|      33 |  5841 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  5842 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5843 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5844 | `				return SXERR_ABORT;` |
|       - |  5845 | `			}` |
|       - |  5846 | `			/* Discard the assignment result — this is a statement expression. */` |
|      33 |  5847 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      18 |  5848 | `		}` |
|       - |  5849 | `	}` |
|       - |  5850 | `	/* Compile the body */` |
|  197023 |  5851 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5852 | `	/* Fix exception jumps now the destination is resolved */` |
|  197023 |  5853 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5854 | `	/* Emit the final return if not yet done */` |
|  197023 |  5855 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5856 | `	/* Fix gotos jumps now the destination is resolved */` |
|  197023 |  5857 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5858 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5859 | `	}` |
|  197023 |  5860 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5861 | `	/* Restore the default container */` |
|  197023 |  5862 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5863 | `	/* Leave function block */` |
|  197023 |  5864 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  197023 |  5865 | `	if( rc == SXERR_ABORT ){` |
|       - |  5866 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5867 | `		return SXERR_ABORT;` |
|       - |  5868 | `	}` |
|       - |  5869 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5870 | `	{` |
|  197023 |  5871 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5872 | `		sxu32 i;` |
| 3848081 |  5873 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 3651099 |  5874 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      41 |  5875 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      41 |  5876 | `				break;` |
|       - |  5877 | `			}` |
| 1825534 |  5878 | `		}` |
|       - |  5879 | `	}` |
|       - |  5880 | `	/* All done, function body compiled */` |
|  197023 |  5881 | `	return SXRET_OK;` |
|   98514 |  5882 |  |
|       - |  5883 | `/*` |
|       - |  5884 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5885 | ` * According to the PHP language reference manual.` |
|       - |  5886 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5887 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5888 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5889 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5890 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5891 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5892 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5893 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5894 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5895 | ` *` |
|       - |  5896 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5897 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5898 | ` * on these extension.` |
|       - |  5899 | ` */` |
|       - |  5900 | `/*` |
|       - |  5901 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5902 | ` */` |
|     320 |  5903 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  5904 |  |
|       - |  5905 | `	sxu32 i;` |
|     893 |  5906 | `	for( i = 0; i < n; i++ ){` |
|     765 |  5907 | `		int a = zA[i], b = zB[i];` |
|     765 |  5908 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     765 |  5909 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     765 |  5910 | `		if( a != b ) return a - b;` |
|     289 |  5911 | `	}` |
|     133 |  5912 | `	return 0;` |
|     165 |  5913 |  |
|       - |  5914 | `/*` |
|       - |  5915 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5916 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5917 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5918 | ` */` |
|       - |  5919 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5920 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5921 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5922 |  |
|       - |  5923 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5924 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5925 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5926 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5927 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5928 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5929 |  |
|       - |  5930 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5931 | `struct PhlTypeAtom {` |
|       - |  5932 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5933 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5934 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5935 | `	sxu32 nCanon;` |
|       - |  5936 | `};` |
|       - |  5937 |  |
|       - |  5938 | `/*` |
|       - |  5939 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5940 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5941 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5942 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5943 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5944 | ` * already be consumed by the caller.` |
|       - |  5945 | ` */` |
|   66876 |  5946 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  5947 |  |
|   66881 |  5948 | `	SyToken *pIn = pGen->pIn;` |
|   66881 |  5949 | `	SyZero(pOut, sizeof(*pOut));` |
|   66881 |  5950 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   66881 |  5951 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5952 | `		return SXERR_SYNTAX;` |
|       - |  5953 | `	}` |
|       - |  5954 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   66881 |  5955 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5956 | `		pIn++;` |
|       8 |  5957 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5958 | `			return SXERR_SYNTAX;` |
|       - |  5959 | `		}` |
|       3 |  5960 | `	}` |
|   66881 |  5961 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5962 | `		return SXERR_SYNTAX;` |
|       - |  5963 | `	}` |
|   66881 |  5964 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   57197 |  5965 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   57197 |  5966 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      18 |  5967 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   57190 |  5968 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      59 |  5969 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   57156 |  5970 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   15989 |  5971 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   49137 |  5972 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   41091 |  5973 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   20602 |  5974 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      28 |  5975 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      46 |  5976 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  5977 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      21 |  5978 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       5 |  5979 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  5980 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  5981 | `			pOut->sClass = pIn->sData;` |
|       4 |  5982 | `		}else{` |
|       3 |  5983 | `			return SXERR_SYNTAX;` |
|       - |  5984 | `		}` |
|   57195 |  5985 | `		pIn++;` |
|   28600 |  5986 | `	}else{` |
|       - |  5987 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  5988 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    9689 |  5989 | `		SyString *pT = &pIn->sData;` |
|    9689 |  5990 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      18 |  5991 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      18 |  5992 | `			pIn++;` |
|    9681 |  5993 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     111 |  5994 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     111 |  5995 | `			pIn++;` |
|    9620 |  5996 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  5997 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  5998 | `			pIn++;` |
|       2 |  5999 | `		}else{` |
|       - |  6000 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    9565 |  6001 | `			SyToken *pFirst = pIn;` |
|    9565 |  6002 | `			SyToken *pLast = pIn;` |
|    9565 |  6003 | `			pOut->nType = SXU32_HIGH;` |
|    9565 |  6004 | `			pOut->sClass = pIn->sData;` |
|    9565 |  6005 | `			pIn++;` |
|   14343 |  6006 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    9568 |  6007 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6008 | `				pLast = &pIn[1];` |
|       3 |  6009 | `				pIn += 2;` |
|       1 |  6010 | `			}` |
|    9565 |  6011 | `			if( pLast != pFirst ){` |
|       3 |  6012 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6013 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6014 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6015 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6016 | `			}` |
|       - |  6017 | `		}` |
|       - |  6018 | `	}` |
|   66879 |  6019 | `	pGen->pIn = pIn;` |
|   66879 |  6020 | `	return SXRET_OK;` |
|   33443 |  6021 |  |
|       - |  6022 |  |
|       - |  6023 | `/*` |
|       - |  6024 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6025 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6026 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6027 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6028 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6029 | ` */` |
|   66776 |  6030 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6031 |  |
|       - |  6032 | `	int i;` |
|   66781 |  6033 | `	int nNonNull = 0;` |
|  133641 |  6034 | `	for( i = 0; i < nAtoms; i++ ){` |
|   66865 |  6035 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   66849 |  6036 | `			nNonNull++;` |
|   33422 |  6037 | `		}` |
|   33435 |  6038 | `	}` |
|   66781 |  6039 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6040 | `		/* Shorthand: ?T */` |
|      60 |  6041 | `		for( i = 0; i < nAtoms; i++ ){` |
|      60 |  6042 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      60 |  6043 | `			SyBlobAppend(pBlob, "?", 1);` |
|      60 |  6044 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      15 |  6045 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       9 |  6046 | `			}else{` |
|      47 |  6047 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6048 | `			}` |
|      60 |  6049 | `			return;` |
|     ! 0 |  6050 | `		}` |
|     ! 0 |  6051 | `	}` |
|       - |  6052 | `	{` |
|   66725 |  6053 | `		int bFirst = 1;` |
|       - |  6054 | `		/* 1) Classes in declaration order */` |
|  133523 |  6055 | `		for( i = 0; i < nAtoms; i++ ){` |
|   66803 |  6056 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    9557 |  6057 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    9557 |  6058 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    9557 |  6059 | `				bFirst = 0;` |
|    4776 |  6060 | `			}` |
|   33404 |  6061 | `		}` |
|       - |  6062 | `		/* 2) Built-ins in canonical order */` |
|       - |  6063 | `		{` |
|       - |  6064 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6065 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6066 | `			int k;` |
|  467045 |  6067 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  743889 |  6068 | `				for( i = 0; i < nAtoms; i++ ){` |
|  400701 |  6069 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   57137 |  6070 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   57137 |  6071 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   57137 |  6072 | `						bFirst = 0;` |
|   57137 |  6073 | `						break;` |
|       - |  6074 | `					}` |
|  171787 |  6075 | `				}` |
|  200165 |  6076 | `			}` |
|       - |  6077 | `		}` |
|       - |  6078 | `		/* 3) null suffix */` |
|   66725 |  6079 | `		if( bNullable ){` |
|      12 |  6080 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      12 |  6081 | `			SyBlobAppend(pBlob, "null", 4);` |
|       5 |  6082 | `		}` |
|       - |  6083 | `	}` |
|   33393 |  6084 |  |
|       - |  6085 |  |
|       - |  6086 | `/*` |
|       - |  6087 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6088 | ` *` |
|       - |  6089 | ` * Outputs:` |
|       - |  6090 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6091 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6092 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6093 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6094 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6095 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6096 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6097 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6098 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6099 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6100 | ` *` |
|       - |  6101 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6102 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6103 | ` */` |
|   66786 |  6104 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6105 | `	ph7_gen_state *pGen,` |
|       - |  6106 | `	sxu32 *pnType,` |
|       - |  6107 | `	SyString *pClass,` |
|       - |  6108 | `	SySet *pAlts,` |
|       - |  6109 | `	sxi32 *piTypeFlags,` |
|       - |  6110 | `	SyString *pTypeText,` |
|       - |  6111 | `	int iNullableFlag,` |
|       - |  6112 | `	int iUnionFlag,` |
|       - |  6113 | `	int bAllowVoid,` |
|       - |  6114 | `	sxu32 nLine` |
|       5 |  6115 | `){` |
|       - |  6116 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   66791 |  6117 | `	int nAtoms = 0;` |
|   66791 |  6118 | `	int bShortNullable = 0;` |
|   66791 |  6119 | `	int bExplicitNull = 0;` |
|       - |  6120 | `	sxi32 rc;` |
|   66791 |  6121 | `	*pnType = 0;` |
|   66791 |  6122 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   66791 |  6123 | `	*piTypeFlags = 0;` |
|   66791 |  6124 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6125 |  |
|   66791 |  6126 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6127 | `		return SXRET_OK;` |
|       - |  6128 | `	}` |
|       - |  6129 | ``	/* Optional `?` shorthand prefix */`` |
|   66786 |  6130 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      57 |  6131 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      56 |  6132 | `		bShortNullable = 1;` |
|      56 |  6133 | `		pGen->pIn++;` |
|      56 |  6134 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6135 | `			return SXERR_SYNTAX;` |
|       - |  6136 | `		}` |
|      26 |  6137 | `	}` |
|       - |  6138 | `	/* First atom is mandatory */` |
|   66791 |  6139 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   66791 |  6140 | `	if( rc != SXRET_OK ){` |
|       3 |  6141 | `		return rc;` |
|       - |  6142 | `	}` |
|   66789 |  6143 | `	nAtoms = 1;` |
|       - |  6144 | ``	/* Subsequent atoms separated by `\|` */`` |
|  100313 |  6145 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   66926 |  6146 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      97 |  6147 | `		if( bShortNullable ){` |
|       - |  6148 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6149 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6150 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  6151 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6152 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6153 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6154 | `		}` |
|      95 |  6155 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6156 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6157 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6158 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6159 | `		}` |
|      95 |  6160 | ``		pGen->pIn++; /* skip `\|` */`` |
|      95 |  6161 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      95 |  6162 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  6163 | `			return rc;` |
|       - |  6164 | `		}` |
|      95 |  6165 | `		nAtoms++;` |
|       5 |  6166 | `	}` |
|       - |  6167 | `	/* Validation pass.` |
|       - |  6168 | `	 *` |
|       - |  6169 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6170 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6171 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6172 | `	 */` |
|       - |  6173 | `	{` |
|       - |  6174 | `		int i, j;` |
|   66787 |  6175 | `		int bHasNonNull = 0;` |
|  133653 |  6176 | `		for( i = 0; i < nAtoms; i++ ){` |
|   66877 |  6177 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     111 |  6178 | `				if( nAtoms > 1 ){` |
|       3 |  6179 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6180 | `						"Void can only be used as a standalone type");` |
|       3 |  6181 | `					return SXERR_SYNTAX;` |
|       - |  6182 | `				}` |
|     109 |  6183 | `				if( !bAllowVoid ){` |
|     ! 0 |  6184 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6185 | `						"void cannot be used here");` |
|     ! 0 |  6186 | `					return SXERR_SYNTAX;` |
|       - |  6187 | `				}` |
|     109 |  6188 | `				if( bShortNullable ){` |
|     ! 0 |  6189 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6190 | `						"Void type cannot be nullable");` |
|     ! 0 |  6191 | `					return SXERR_SYNTAX;` |
|       - |  6192 | `				}` |
|      52 |  6193 | `			}` |
|   66875 |  6194 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6195 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6196 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6197 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6198 | `				 * does not return), and folding them would mislead any` |
|       - |  6199 | `				 * future return-enforcement work. */` |
|       3 |  6200 | `				if( nAtoms > 1 ){` |
|       3 |  6201 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6202 | `						"never can only be used as a standalone type");` |
|       3 |  6203 | `					return SXERR_SYNTAX;` |
|       - |  6204 | `				}` |
|     ! 0 |  6205 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6206 | `					"never type is not yet implemented");` |
|     ! 0 |  6207 | `				return SXERR_SYNTAX;` |
|       - |  6208 | `			}` |
|   66873 |  6209 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      18 |  6210 | `				bExplicitNull = 1;` |
|      10 |  6211 | `			}else{` |
|   66857 |  6212 | `				bHasNonNull = 1;` |
|       - |  6213 | `			}` |
|       - |  6214 | `			/* Duplicate detection */` |
|   66993 |  6215 | `			for( j = 0; j < i; j++ ){` |
|     127 |  6216 | `				int bDup = 0;` |
|     127 |  6217 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      17 |  6218 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6219 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      14 |  6220 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6221 | `								aAtoms[j].sClass.zString,` |
|      12 |  6222 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6223 | `							bDup = 1;` |
|     ! 0 |  6224 | `						}` |
|       8 |  6225 | `					}else{` |
|       3 |  6226 | `						bDup = 1;` |
|       - |  6227 | `					}` |
|       7 |  6228 | `				}` |
|     127 |  6229 | `				if( bDup ){` |
|       - |  6230 | `					const char *zName;` |
|       - |  6231 | `					sxu32 nName;` |
|       3 |  6232 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6233 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6234 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6235 | `					}else{` |
|       3 |  6236 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6237 | `						nName = aAtoms[i].nCanon;` |
|       - |  6238 | `					}` |
|       4 |  6239 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6240 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6241 | `					return SXERR_SYNTAX;` |
|       - |  6242 | `				}` |
|      65 |  6243 | `			}` |
|   33438 |  6244 | `		}` |
|   66781 |  6245 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6246 | `			if( bShortNullable ){` |
|       - |  6247 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6248 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6249 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6250 | `				return SXERR_SYNTAX;` |
|       - |  6251 | `			}` |
|       - |  6252 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6253 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6254 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6255 | `			 * atom, so set it here. */` |
|       7 |  6256 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6257 | `		}` |
|       - |  6258 | `	}` |
|       - |  6259 | `	/* Compute nullability flag */` |
|   66781 |  6260 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      70 |  6261 | `		*piTypeFlags \|= iNullableFlag;` |
|      33 |  6262 | `	}` |
|       - |  6263 | `	/* Build canonical type text */` |
|   66781 |  6264 | `	if( pTypeText ){` |
|       - |  6265 | `		SyBlob sBlob;` |
|   66781 |  6266 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  100144 |  6267 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   33388 |  6268 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   66781 |  6269 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  100013 |  6270 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   66672 |  6271 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   66677 |  6272 | `			if( zDup ){` |
|   66677 |  6273 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   33336 |  6274 | `			}` |
|   33336 |  6275 | `		}` |
|   66781 |  6276 | `		SyBlobRelease(&sBlob);` |
|   33388 |  6277 | `	}` |
|       - |  6278 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6279 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6280 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6281 | `	{` |
|   66781 |  6282 | `		int nNonNull = 0;` |
|   66781 |  6283 | `		int iNonNullIdx = -1;` |
|       - |  6284 | `		int i;` |
|  133641 |  6285 | `		for( i = 0; i < nAtoms; i++ ){` |
|   66865 |  6286 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   66849 |  6287 | `				nNonNull++;` |
|   66849 |  6288 | `				iNonNullIdx = i;` |
|   33422 |  6289 | `			}` |
|   33435 |  6290 | `		}` |
|   66781 |  6291 | `		if( nNonNull <= 1 ){` |
|       - |  6292 | `			/* Fast path: store as single type. */` |
|   66723 |  6293 | `			if( iNonNullIdx >= 0 ){` |
|   66717 |  6294 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   66717 |  6295 | `				if( pA->nType == SXU32_HIGH ){` |
|   14309 |  6296 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    4768 |  6297 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    9541 |  6298 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    9541 |  6299 | `					*pnType = SXU32_HIGH;` |
|    9541 |  6300 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   61949 |  6301 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     109 |  6302 | `					*pnType = MEMOBJ_VOID;` |
|      57 |  6303 | `				}else{` |
|       - |  6304 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6305 | `					 * pass above rejects it as not-yet-implemented. */` |
|   57077 |  6306 | `					*pnType = pA->nType;` |
|       - |  6307 | `				}` |
|   33356 |  6308 | `			}` |
|   33364 |  6309 | `		}else{` |
|       - |  6310 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      63 |  6311 | `			*piTypeFlags \|= iUnionFlag;` |
|     199 |  6312 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6313 | `				ph7_type_alt sAlt;` |
|     141 |  6314 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     137 |  6315 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     137 |  6316 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      44 |  6317 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      14 |  6318 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      30 |  6319 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      30 |  6320 | `					sAlt.nType = SXU32_HIGH;` |
|      30 |  6321 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      16 |  6322 | `				}else{` |
|     109 |  6323 | `					sAlt.nType = aAtoms[i].nType;` |
|     109 |  6324 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6325 | `				}` |
|     137 |  6326 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      71 |  6327 | `			}` |
|       - |  6328 | `		}` |
|       - |  6329 | `	}` |
|   66781 |  6330 | `	return SXRET_OK;` |
|   33398 |  6331 |  |
|       - |  6332 |  |
|       - |  6333 | `/*` |
|       - |  6334 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6335 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6336 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6337 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6338 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6339 | `` *          and union types `: T\|U`.`` |
|       - |  6340 | ` */` |
|  279044 |  6341 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6342 |  |
|  279049 |  6343 | `	sxi32 iFlags = 0;` |
|       - |  6344 | `	sxi32 rc;` |
|       - |  6345 | `	sxu32 nLine;` |
|  279049 |  6346 | `	pFunc->nReturnType = 0;` |
|  279049 |  6347 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  279049 |  6348 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  279049 |  6349 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  278709 |  6350 | `		return SXRET_OK;` |
|       - |  6351 | `	}` |
|     345 |  6352 | `	pGen->pIn++; /* Skip ':' */` |
|     345 |  6353 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6354 | `		return SXRET_OK;` |
|       - |  6355 | `	}` |
|     345 |  6356 | `	nLine = pGen->pIn->nLine;` |
|     345 |  6357 | `	rc = GenStateParseUnionTypeDecl(` |
|     170 |  6358 | `		pGen,` |
|     170 |  6359 | `		&pFunc->nReturnType,` |
|     170 |  6360 | `		&pFunc->sReturnClass,` |
|     170 |  6361 | `		&pFunc->aReturnUnion,` |
|       - |  6362 | `		&iFlags,` |
|     170 |  6363 | `		&pFunc->sReturnTypeName,` |
|       - |  6364 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6365 | `		/* iUnionFlag */ 0,` |
|       - |  6366 | `		/* bAllowVoid */ 1,` |
|     170 |  6367 | `		nLine);` |
|     170 |  6368 | `	(void)iFlags;` |
|     345 |  6369 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6370 | `		return SXERR_ABORT;` |
|       - |  6371 | `	}` |
|     345 |  6372 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6373 | `		/* Error already reported */` |
|     ! 0 |  6374 | `		return SXERR_SYNTAX;` |
|       - |  6375 | `	}` |
|     345 |  6376 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6377 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6378 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6379 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6380 | `				&pGen->pIn->sData);` |
|       3 |  6381 | `		}else{` |
|     ! 0 |  6382 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6383 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6384 | `		}` |
|       5 |  6385 | `		return SXERR_SYNTAX;` |
|       - |  6386 | `	}` |
|     341 |  6387 | `	return SXRET_OK;` |
|  139527 |  6388 |  |
|       - |  6389 |  |
|   41968 |  6390 | `static sxi32 GenStateCompileFunc(` |
|       - |  6391 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6392 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6393 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6394 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6395 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6396 | `	)` |
|       5 |  6397 |  |
|       - |  6398 | `	ph7_vm_func *pFunc;` |
|       - |  6399 | `	SyToken *pEnd;` |
|       - |  6400 | `	sxu32 nLine;` |
|       - |  6401 | `	char *zName;` |
|       - |  6402 | `	sxi32 rc;` |
|       - |  6403 | `	/* Extract line number */` |
|   41973 |  6404 | `	nLine = pGen->pIn->nLine;` |
|       - |  6405 | `	/* Jump the left parenthesis '(' */` |
|   41973 |  6406 | `	pGen->pIn++;` |
|       - |  6407 | `	/* Delimit the function signature */` |
|   41973 |  6408 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   41973 |  6409 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6410 | `		/* Syntax error */` |
|       9 |  6411 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       9 |  6412 | `		if( rc == SXERR_ABORT ){` |
|       - |  6413 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6414 | `			return SXERR_ABORT;` |
|       - |  6415 | `		}` |
|       9 |  6416 | `		pGen->pIn = pGen->pEnd;` |
|       9 |  6417 | `		return SXRET_OK;` |
|       - |  6418 | `	}` |
|       - |  6419 | `	/* Create the function state */` |
|   41967 |  6420 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   41967 |  6421 | `	if( pFunc == 0 ){` |
|     ! 0 |  6422 | `		goto OutOfMem;` |
|       - |  6423 | `	}` |
|       - |  6424 | `	/* Build the function name, prepending namespace if active */` |
|   41974 |  6425 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6426 | `		SyBlob sFQN;` |
|       - |  6427 | `		sxu32 nLen;` |
|      16 |  6428 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6429 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6430 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6431 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6432 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6433 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6434 | `		SyBlobRelease(&sFQN);` |
|      16 |  6435 | `		if( zName == 0 ){` |
|     ! 0 |  6436 | `			goto OutOfMem;` |
|       - |  6437 | `		}` |
|      16 |  6438 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6439 | `	}else{` |
|   41953 |  6440 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   41953 |  6441 | `		if( zName == 0 ){` |
|     ! 0 |  6442 | `			goto OutOfMem;` |
|       - |  6443 | `		}` |
|   41953 |  6444 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6445 | `	}` |
|   41967 |  6446 | `	if( pGen->pIn < pEnd ){` |
|       - |  6447 | `		/* Collect function arguments */` |
|   29071 |  6448 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   29071 |  6449 | `		if( rc == SXERR_ABORT ){` |
|       - |  6450 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6451 | `			return SXERR_ABORT;` |
|       - |  6452 | `		}` |
|   14533 |  6453 | `	}` |
|       - |  6454 | `	/* Point past ')' and parse optional return type ': type' */` |
|   41967 |  6455 | `	pGen->pIn = &pEnd[1];` |
|       - |  6456 | `	{` |
|   41967 |  6457 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   41967 |  6458 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6459 | `			return SXERR_ABORT;` |
|   41967 |  6460 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6461 | `			return SXERR_SYNTAX;` |
|       - |  6462 | `		}` |
|       - |  6463 | `	}` |
|   41963 |  6464 | `	if( bHandleClosure ){` |
|       - |  6465 | `		ph7_vm_func_closure_env sEnv;` |
|     251 |  6466 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     246 |  6467 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     136 |  6468 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      21 |  6469 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6470 | `				/* Closure,record environment variable */` |
|      21 |  6471 | `				pGen->pIn++;` |
|      21 |  6472 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6473 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6474 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6475 | `						return SXERR_ABORT;` |
|       - |  6476 | `					}` |
|     ! 0 |  6477 | `				}` |
|      21 |  6478 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6479 | `				/* Compile until we hit the first closing parenthesis */` |
|      41 |  6480 | `				while( pGen->pIn < pGen->pEnd ){` |
|      41 |  6481 | `					int iFlagsLocal = 0;` |
|      41 |  6482 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      21 |  6483 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      21 |  6484 | `						break;` |
|       - |  6485 | `					}` |
|      25 |  6486 | `					nLineLocal = pGen->pIn->nLine;` |
|      25 |  6487 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6488 | `						/* Pass by reference,record that */` |
|     ! 0 |  6489 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6490 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6491 | `							);` |
|     ! 0 |  6492 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6493 | `						pGen->pIn++;` |
|     ! 0 |  6494 | `					}` |
|      20 |  6495 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      25 |  6496 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6497 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6498 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6499 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6500 | `								return SXERR_ABORT;` |
|       - |  6501 | `							}` |
|       - |  6502 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6503 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6504 | `								pGen->pIn++;` |
|     ! 0 |  6505 | `							}` |
|     ! 0 |  6506 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6507 | `								pGen->pIn++;` |
|     ! 0 |  6508 | `							}` |
|     ! 0 |  6509 | `							break;` |
|       - |  6510 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6511 | `					}else{` |
|       - |  6512 | `						SyString *pNameLocal;` |
|       - |  6513 | `						char *zDup;` |
|       - |  6514 | `						/* Duplicate variable name */` |
|      25 |  6515 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      25 |  6516 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      25 |  6517 | `						if( zDup ){` |
|       - |  6518 | `							/* Zero the structure */` |
|      25 |  6519 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      25 |  6520 | `							sEnv.iFlags = iFlagsLocal;` |
|      25 |  6521 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      25 |  6522 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      25 |  6523 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6524 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6525 | `									got_this = 1;` |
|     ! 0 |  6526 | `							}` |
|       - |  6527 | `							/* Save imported variable */` |
|      25 |  6528 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      15 |  6529 | `						}else{` |
|     ! 0 |  6530 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6531 | `							 return SXERR_ABORT;` |
|       - |  6532 | `						}` |
|       - |  6533 | `					}` |
|      25 |  6534 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      31 |  6535 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6536 | `						/* Ignore trailing commas */` |
|       7 |  6537 | `						pGen->pIn++;` |
|       1 |  6538 | `					}` |
|       5 |  6539 | `				}` |
|      21 |  6540 | `				if( !got_this ){` |
|       - |  6541 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6542 | `					 * available to the closure environment.` |
|       - |  6543 | `					 */` |
|      21 |  6544 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      21 |  6545 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      21 |  6546 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      21 |  6547 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      21 |  6548 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       8 |  6549 | `				}` |
|      21 |  6550 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6551 | `					/* Mark as closure */` |
|      21 |  6552 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       8 |  6553 | `				}` |
|       8 |  6554 | `		}` |
|     123 |  6555 | `	}` |
|       - |  6556 | `	/* Compile the body */` |
|   41963 |  6557 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   41963 |  6558 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6559 | `		return SXERR_ABORT;` |
|       - |  6560 | `	}` |
|   41963 |  6561 | `	if( ppFunc ){` |
|     251 |  6562 | `		*ppFunc = pFunc;` |
|     123 |  6563 | `	}` |
|   41963 |  6564 | `	rc = SXRET_OK;` |
|   41963 |  6565 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6566 | `		/* Finally register the function */` |
|   41947 |  6567 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   20971 |  6568 | `	}` |
|   41963 |  6569 | `	if( rc == SXRET_OK ){` |
|   41963 |  6570 | `		return SXRET_OK;` |
|       - |  6571 | `	}` |
|       - |  6572 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6573 | `OutOfMem:` |
|       - |  6574 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6575 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6576 | `	 */` |
|     ! 0 |  6577 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6578 | `	return SXERR_ABORT;` |
|   20989 |  6579 |  |
|       - |  6580 | `/*` |
|       - |  6581 | ` * Compile a standard PHP function.` |
|       - |  6582 | ` *  Refer to the block-comment above for more information.` |
|       - |  6583 | ` */` |
|   41728 |  6584 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6585 |  |
|       - |  6586 | `	SyString *pName;` |
|       - |  6587 | `	sxi32 iFlags;` |
|       - |  6588 | `	sxu32 nLine;` |
|       - |  6589 | `	sxi32 rc;` |
|       - |  6590 |  |
|   41733 |  6591 | `	nLine = pGen->pIn->nLine;` |
|   41733 |  6592 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   41733 |  6593 | `	iFlags = 0;` |
|   41733 |  6594 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6595 | `		/* Return by reference,remember that */` |
|       7 |  6596 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6597 | `		/* Jump the '&' token */` |
|       7 |  6598 | `		pGen->pIn++;` |
|       3 |  6599 | `	}` |
|   41733 |  6600 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6601 | `		/* Invalid function name */` |
|       6 |  6602 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       6 |  6603 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6604 | `			return SXERR_ABORT;` |
|       - |  6605 | `		}` |
|       - |  6606 | `		/* Sychronize with the next semi-colon or braces*/` |
|      18 |  6607 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      14 |  6608 | `			pGen->pIn++;` |
|       2 |  6609 | `		}` |
|       6 |  6610 | `		return SXRET_OK;` |
|       - |  6611 | `	}` |
|   41729 |  6612 | `	pName = &pGen->pIn->sData;` |
|   41729 |  6613 | `	nLine = pGen->pIn->nLine;` |
|       - |  6614 | `	/* Jump the function name */` |
|   41729 |  6615 | `	pGen->pIn++;` |
|   41729 |  6616 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6617 | `		/* Syntax error */` |
|       3 |  6618 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6619 | `		if( rc == SXERR_ABORT ){` |
|       - |  6620 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6621 | `			return SXERR_ABORT;` |
|       - |  6622 | `		}` |
|       - |  6623 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6624 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6625 | `			pGen->pIn++;` |
|     ! 0 |  6626 | `		}` |
|       3 |  6627 | `		return SXRET_OK;` |
|       - |  6628 | `	}` |
|       - |  6629 | `	/* Compile function body */` |
|   41727 |  6630 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   41727 |  6631 | `	return rc;` |
|   20869 |  6632 |  |
|       - |  6633 | `/*` |
|       - |  6634 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6635 | ` * According to the PHP language reference manual` |
|       - |  6636 | ` *  Visibility:` |
|       - |  6637 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6638 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6639 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6640 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6641 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6642 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6643 | ` */` |
|  297324 |  6644 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  6645 |  |
|  297329 |  6646 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    9533 |  6647 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  287801 |  6648 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   40973 |  6649 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6650 | `	}` |
|       - |  6651 | `	/* Assume public by default */` |
|  246833 |  6652 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  148667 |  6653 |  |
|       - |  6654 | `/*` |
|       - |  6655 | ` * Compile a class constant.` |
|       - |  6656 | ` * According to the PHP language reference manual` |
|       - |  6657 | ` *  Class Constants` |
|       - |  6658 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6659 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6660 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6661 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6662 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6663 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6664 | ` * Symisc eXtension.` |
|       - |  6665 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6666 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6667 | ` *  Example:` |
|       - |  6668 | ` *   class Test{` |
|       - |  6669 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6670 | ` *   };` |
|       - |  6671 | ` *   var_dump(TEST::MyConst);` |
|       - |  6672 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6673 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6674 | ` */` |
|      32 |  6675 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  6676 |  |
|      37 |  6677 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6678 | `	SySet *pInstrContainer;` |
|       - |  6679 | `	ph7_class_attr *pCons;` |
|       - |  6680 | `	SyString *pName;` |
|       - |  6681 | `	sxi32 rc;` |
|       - |  6682 | `	/* Extract visibility level */` |
|      37 |  6683 | `	iProtection = GetProtectionLevel(iProtection);` |
|      37 |  6684 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      16 |  6685 | `loop:` |
|       - |  6686 | `	/* Mark as constant */` |
|      37 |  6687 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      37 |  6688 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6689 | `		/* Invalid constant name */` |
|     ! 0 |  6690 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6691 | `		if( rc == SXERR_ABORT ){` |
|       - |  6692 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6693 | `			return SXERR_ABORT;` |
|       - |  6694 | `		}` |
|     ! 0 |  6695 | `		goto Synchronize;` |
|       - |  6696 | `	}` |
|       - |  6697 | `	/* Peek constant name */` |
|      37 |  6698 | `	pName = &pGen->pIn->sData;` |
|       - |  6699 | `	/* Make sure the constant name isn't reserved */` |
|      37 |  6700 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6701 | `		/* Reserved constant name */` |
|     ! 0 |  6702 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6703 | `		if( rc == SXERR_ABORT ){` |
|       - |  6704 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6705 | `			return SXERR_ABORT;` |
|       - |  6706 | `		}` |
|     ! 0 |  6707 | `		goto Synchronize;` |
|       - |  6708 | `	}` |
|       - |  6709 | `	/* Advance the stream cursor */` |
|      37 |  6710 | `	pGen->pIn++;` |
|      37 |  6711 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6712 | `		/* Invalid declaration */` |
|     ! 0 |  6713 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6714 | `		if( rc == SXERR_ABORT ){` |
|       - |  6715 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6716 | `			return SXERR_ABORT;` |
|       - |  6717 | `		}` |
|     ! 0 |  6718 | `		goto Synchronize;` |
|       - |  6719 | `	}` |
|      37 |  6720 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6721 | `	/* Allocate a new class attribute */` |
|      37 |  6722 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      37 |  6723 | `	if( pCons == 0 ){` |
|     ! 0 |  6724 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6725 | `		return SXERR_ABORT;` |
|       - |  6726 | `	}` |
|       - |  6727 | `	/* Swap bytecode container */` |
|      37 |  6728 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      37 |  6729 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6730 | `	/* Compile constant value.` |
|       - |  6731 | `	 */` |
|      37 |  6732 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      37 |  6733 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6734 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6735 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6736 | `			return SXERR_ABORT;` |
|       - |  6737 | `		}` |
|       1 |  6738 | `	}` |
|       - |  6739 | `	/* Emit the done instruction */` |
|      37 |  6740 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      37 |  6741 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      37 |  6742 | `	if( rc == SXERR_ABORT ){` |
|       - |  6743 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6744 | `		return SXERR_ABORT;` |
|       - |  6745 | `	}` |
|       - |  6746 | `	/* All done,install the constant */` |
|      37 |  6747 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      37 |  6748 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6749 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6750 | `		return SXERR_ABORT;` |
|       - |  6751 | `	}` |
|      37 |  6752 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6753 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 |  6754 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 |  6755 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6756 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6757 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6758 | `				pTok--;` |
|     ! 0 |  6759 | `			}` |
|     ! 0 |  6760 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6761 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6762 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6763 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6764 | `				return SXERR_ABORT;` |
|       - |  6765 | `			}` |
|     ! 0 |  6766 | `		}else{` |
|     ! 0 |  6767 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 |  6768 | `				goto loop;` |
|       - |  6769 | `			}` |
|       - |  6770 | `		}` |
|     ! 0 |  6771 | `	}` |
|      37 |  6772 | `	return SXRET_OK;` |
|     ! 0 |  6773 | `Synchronize:` |
|       - |  6774 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  6775 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 |  6776 | `		pGen->pIn++;` |
|     ! 0 |  6777 | `	}` |
|     ! 0 |  6778 | `	return SXERR_CORRUPT;` |
|      21 |  6779 |  |
|       - |  6780 | `/*` |
|       - |  6781 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6782 | ` * According to the PHP language reference manual` |
|       - |  6783 | ` *  Properties` |
|       - |  6784 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6785 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6786 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6787 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6788 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6789 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6790 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6791 | ` * Symisc eXtension.` |
|       - |  6792 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6793 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6794 | ` *  Example:` |
|       - |  6795 | ` *   class Test{` |
|       - |  6796 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6797 | ` *   };` |
|       - |  6798 | ` *   var_dump(TEST::myVar);` |
|       - |  6799 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6800 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6801 | ` */` |
|       - |  6802 | `/*` |
|       - |  6803 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6804 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6805 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6806 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6807 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6808 | ` */` |
|  155172 |  6809 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  6810 |  |
|  155177 |  6811 | `	SyToken *p = pStart;` |
|  155177 |  6812 | `	if( p >= pEnd ) return 0;` |
|  155177 |  6813 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 |  6814 | `		p++;` |
|      16 |  6815 | `		if( p >= pEnd ) return 0;` |
|       7 |  6816 | `	}` |
|  155177 |  6817 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6818 | `		p++;` |
|       3 |  6819 | `		if( p >= pEnd ) return 0;` |
|       1 |  6820 | `	}` |
|  155177 |  6821 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6822 | `		return 0;` |
|       - |  6823 | `	}` |
|       - |  6824 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6825 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6826 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6827 | `	 * dispatch site. */` |
|  155177 |  6828 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  155155 |  6829 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  155207 |  6830 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3320 |  6831 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  155041 |  6832 | `			return 0;` |
|       - |  6833 | `		}` |
|      57 |  6834 | `	}` |
|     141 |  6835 | `	p++;` |
|       - |  6836 | `	/* Consume optional namespace path */` |
|     143 |  6837 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6838 | `		p += 2;` |
|       1 |  6839 | `	}` |
|       - |  6840 | ``	/* Consume any `\| Type` union alternatives */`` |
|     222 |  6841 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      91 |  6842 | `		&& p->sData.zString[0] == '\|' ){` |
|      16 |  6843 | `		p++;` |
|      16 |  6844 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      16 |  6845 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      16 |  6846 | `		p++;` |
|      16 |  6847 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6848 | `			p += 2;` |
|     ! 0 |  6849 | `		}` |
|       4 |  6850 | `	}` |
|     141 |  6851 | `	if( p >= pEnd ) return 0;` |
|     141 |  6852 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   77591 |  6853 |  |
|       - |  6854 |  |
|       - |  6855 | `/*` |
|       - |  6856 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6857 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6858 | ` * if not). Recognized forms:` |
|       - |  6859 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6860 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6861 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6862 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6863 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6864 | ` * on unrecoverable error.` |
|       - |  6865 | ` *` |
|       - |  6866 | ` * When a type is parsed:` |
|       - |  6867 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  6868 | ` *   *pClass is set to the class name (for class types)` |
|       - |  6869 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  6870 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  6871 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  6872 | ` */` |
|     136 |  6873 | `static sxi32 GenStateParsePropertyType(` |
|       - |  6874 | `	ph7_gen_state *pGen,` |
|       - |  6875 | `	sxu32 *pnType,` |
|       - |  6876 | `	SyString *pClass,` |
|       - |  6877 | `	sxi32 *piTypeFlags,` |
|       - |  6878 | `	SyString *pTypeText,` |
|       - |  6879 | `	SySet *pAlts` |
|       5 |  6880 | `){` |
|     141 |  6881 | `	sxi32 iFlags = 0;` |
|       - |  6882 | `	sxi32 rc;` |
|     141 |  6883 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6884 | `		return SXRET_OK;` |
|       - |  6885 | `	}` |
|       - |  6886 | `	/* If the first token is '$', there's no type */` |
|     141 |  6887 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  6888 | `		return SXRET_OK;` |
|       - |  6889 | `	}` |
|     141 |  6890 | `	rc = GenStateParseUnionTypeDecl(` |
|      68 |  6891 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  6892 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  6893 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  6894 | `		/* bAllowVoid */ 0,` |
|     136 |  6895 | `		pGen->pIn->nLine);` |
|     141 |  6896 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6897 | `		return rc;` |
|       - |  6898 | `	}` |
|       - |  6899 | `	/* Verify next token is '$' (start of property name) */` |
|     141 |  6900 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6901 | `		return SXERR_SYNTAX;` |
|       - |  6902 | `	}` |
|     141 |  6903 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     141 |  6904 | `	return SXRET_OK;` |
|      73 |  6905 |  |
|       - |  6906 |  |
|       - |  6907 | `/*` |
|       - |  6908 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  6909 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  6910 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  6911 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  6912 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  6913 | ` * by the type parser itself before reaching here.` |
|       - |  6914 | ` *` |
|       - |  6915 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  6916 | ` * use in the error message.` |
|       - |  6917 | ` */` |
|     202 |  6918 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  6919 | `	sxu32 nType,` |
|       - |  6920 | `	const SyString *pClass,` |
|       - |  6921 | `	const char **pzName,` |
|       - |  6922 | `	sxu32 *pnName)` |
|       5 |  6923 |  |
|       - |  6924 | `	const char *z;` |
|       - |  6925 | `	sxu32 n;` |
|     207 |  6926 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     173 |  6927 | `		return 0;` |
|       - |  6928 | `	}` |
|      38 |  6929 | `	z = pClass->zString;` |
|      38 |  6930 | `	n = pClass->nByte;` |
|      38 |  6931 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       6 |  6932 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  6933 | `	}` |
|       - |  6934 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  6935 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  6936 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      33 |  6937 | `	return 0;` |
|     106 |  6938 |  |
|       - |  6939 |  |
|       - |  6940 | `/*` |
|       - |  6941 | ` * Validate a parsed property type (main atom + any union alternatives)` |
|       - |  6942 | ` * against the disallowed-pseudo-types list. Emits a PHP-compatible` |
|       - |  6943 | ` * "Property C::$x cannot have type T" error on rejection, where T is` |
|       - |  6944 | ` * the full canonical type text (matching PHP's error wording for` |
|       - |  6945 | `` * unions like `callable\|int`).`` |
|       - |  6946 | ` *` |
|       - |  6947 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  6948 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  6949 | ` */` |
|     174 |  6950 | `static sxi32 GenStateValidatePropertyType(` |
|       - |  6951 | `	ph7_gen_state *pGen,` |
|       - |  6952 | `	ph7_class *pClass,` |
|       - |  6953 | `	const SyString *pPropName,` |
|       - |  6954 | `	sxu32 nType,` |
|       - |  6955 | `	const SyString *pTypeClass,` |
|       - |  6956 | `	const SyString *pTypeText,` |
|       - |  6957 | `	SySet *pUnionAlts,` |
|       - |  6958 | `	sxu32 nLine)` |
|       5 |  6959 |  |
|     179 |  6960 | `	const char *zBad = 0;` |
|     179 |  6961 | `	sxu32 nBad = 0;` |
|       - |  6962 | `	SyString sFallback;` |
|       - |  6963 | `	const SyString *pBad;` |
|       - |  6964 | `	sxi32 rc;` |
|     179 |  6965 | `	int bDisallowed = 0;` |
|     179 |  6966 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       3 |  6967 | `		bDisallowed = 1;` |
|     178 |  6968 | `	}else if( pUnionAlts ){` |
|       - |  6969 | `		sxu32 i;` |
|      44 |  6970 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      32 |  6971 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      32 |  6972 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  6973 | `				bDisallowed = 1;` |
|       3 |  6974 | `				break;` |
|       - |  6975 | `			}` |
|      17 |  6976 | `		}` |
|       7 |  6977 | `	}` |
|     179 |  6978 | `	if( !bDisallowed ){` |
|     175 |  6979 | `		return SXRET_OK;` |
|       - |  6980 | `	}` |
|       - |  6981 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  6982 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  6983 | `	 * canonical spelling if the type text is unavailable. */` |
|       6 |  6984 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       6 |  6985 | `		pBad = pTypeText;` |
|       4 |  6986 | `	}else{` |
|     ! 0 |  6987 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  6988 | `		pBad = &sFallback;` |
|       - |  6989 | `	}` |
|       8 |  6990 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6991 | `		"Property %z::$%z cannot have type %z",` |
|       2 |  6992 | `		&pClass->sName,pPropName,pBad);` |
|       6 |  6993 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6994 | `		return SXERR_ABORT;` |
|       - |  6995 | `	}` |
|       6 |  6996 | `	return SXERR_SYNTAX;` |
|      92 |  6997 |  |
|       - |  6998 |  |
|   60322 |  6999 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7000 |  |
|   60327 |  7001 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7002 | `	ph7_class_attr *pAttr;` |
|       - |  7003 | `	SyString *pName;` |
|       - |  7004 | `	sxi32 rc;` |
|   60327 |  7005 | `	sxu32 nType = 0;` |
|       - |  7006 | `	SyString sTypeClass;` |
|       - |  7007 | `	SyString sTypeText;` |
|       - |  7008 | `	SySet aUnionAlts;` |
|   60327 |  7009 | `	sxi32 iTypeFlags = 0;` |
|   60327 |  7010 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   60327 |  7011 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   60327 |  7012 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7013 | `	/* Extract visibility level */` |
|   60327 |  7014 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7015 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   60395 |  7016 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     141 |  7017 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     141 |  7018 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7019 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7020 | `			goto Synchronize;` |
|     141 |  7021 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7022 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7023 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7024 | `				&pGen->pIn->sData);` |
|     ! 0 |  7025 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7026 | `				return SXERR_ABORT;` |
|       - |  7027 | `			}` |
|     ! 0 |  7028 | `			goto Synchronize;` |
|     141 |  7029 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7030 | `			return SXERR_ABORT;` |
|       - |  7031 | `		}` |
|      68 |  7032 | `	}` |
|     ! 0 |  7033 | `loop:` |
|   60331 |  7034 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7035 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7036 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7037 | `			return SXERR_ABORT;` |
|       - |  7038 | `		}` |
|     ! 0 |  7039 | `		goto Synchronize;` |
|       - |  7040 | `	}` |
|   60331 |  7041 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   60331 |  7042 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7043 | `		/* Invalid attribute name */` |
|     ! 0 |  7044 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7045 | `		if( rc == SXERR_ABORT ){` |
|       - |  7046 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7047 | `			return SXERR_ABORT;` |
|       - |  7048 | `		}` |
|     ! 0 |  7049 | `		goto Synchronize;` |
|       - |  7050 | `	}` |
|       - |  7051 | `	/* Peek attribute name */` |
|   60331 |  7052 | `	pName = &pGen->pIn->sData;` |
|       - |  7053 | `	/* Advance the stream cursor */` |
|   60331 |  7054 | `	pGen->pIn++;` |
|   60331 |  7055 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7056 | `		/* Invalid declaration */` |
|       3 |  7057 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7058 | `		if( rc == SXERR_ABORT ){` |
|       - |  7059 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7060 | `			return SXERR_ABORT;` |
|       - |  7061 | `		}` |
|       3 |  7062 | `		goto Synchronize;` |
|       - |  7063 | `	}` |
|       - |  7064 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7065 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7066 | `	 * by the type parser. */` |
|   60329 |  7067 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     215 |  7068 | `		rc = GenStateValidatePropertyType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7069 | `			&sTypeText,` |
|     140 |  7070 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,nLine);` |
|     145 |  7071 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7072 | `			return SXERR_ABORT;` |
|     145 |  7073 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7074 | `			goto Synchronize;` |
|       - |  7075 | `		}` |
|      70 |  7076 | `	}` |
|       - |  7077 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   60329 |  7078 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7079 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7080 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7081 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7082 | `			return SXERR_ABORT;` |
|       - |  7083 | `		}` |
|       3 |  7084 | `		goto Synchronize;` |
|       - |  7085 | `	}` |
|       - |  7086 | `	/* Allocate a new class attribute */` |
|   60327 |  7087 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   60327 |  7088 | `	if( pAttr == 0 ){` |
|     ! 0 |  7089 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7090 | `		return SXERR_ABORT;` |
|       - |  7091 | `	}` |
|   60327 |  7092 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     143 |  7093 | `		pAttr->nType = nType;` |
|     143 |  7094 | `		pAttr->sClass = sTypeClass;` |
|     143 |  7095 | `		pAttr->sTypeName = sTypeText;` |
|     143 |  7096 | `		if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7097 | `			/* Copy the parsed alternatives into the attribute. The class-name` |
|       - |  7098 | `			 * SyStrings inside each ph7_type_alt point to memory owned by the` |
|       - |  7099 | `			 * VM allocator (SyMemBackendStrDup'd in GenStateParseUnionTypeDecl),` |
|       - |  7100 | `			 * so it's safe for multiple attrs in a multi-decl chain to share` |
|       - |  7101 | `			 * the same backing strings — they outlive the temporary set. */` |
|       - |  7102 | `			sxu32 i;` |
|      34 |  7103 | `			for( i = 0; i < SySetUsed(&aUnionAlts); i++ ){` |
|      24 |  7104 | `				ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&aUnionAlts, i);` |
|      24 |  7105 | `				SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      14 |  7106 | `			}` |
|       5 |  7107 | `		}` |
|      69 |  7108 | `	}` |
|   60327 |  7109 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7110 | `		SySet *pInstrContainer;` |
|   19285 |  7111 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7112 | `		/* Swap bytecode container */` |
|   19285 |  7113 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   19285 |  7114 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7115 | `		/* Compile attribute value.` |
|       - |  7116 | `		 */` |
|   19285 |  7117 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   19285 |  7118 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7119 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7120 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7121 | `				return SXERR_ABORT;` |
|       - |  7122 | `			}` |
|     ! 0 |  7123 | `		}` |
|       - |  7124 | `		/* Emit the done instruction */` |
|   19285 |  7125 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   19285 |  7126 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    9640 |  7127 | `	}` |
|       - |  7128 | `	/* All done,install the attribute */` |
|   60327 |  7129 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   60327 |  7130 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7131 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7132 | `		return SXERR_ABORT;` |
|       - |  7133 | `	}` |
|   60327 |  7134 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7135 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7136 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7137 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7138 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7139 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7140 | `				pTok--;` |
|     ! 0 |  7141 | `			}` |
|     ! 0 |  7142 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7143 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7144 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7145 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7146 | `				return SXERR_ABORT;` |
|       - |  7147 | `			}` |
|     ! 0 |  7148 | `		}else{` |
|       5 |  7149 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7150 | `				goto loop;` |
|       - |  7151 | `			}` |
|       - |  7152 | `		}` |
|     ! 0 |  7153 | `	}` |
|   60323 |  7154 | `	SySetRelease(&aUnionAlts);` |
|   60323 |  7155 | `	return SXRET_OK;` |
|       2 |  7156 | `Synchronize:` |
|       - |  7157 | `	/* Synchronize with the first semi-colon */` |
|      12 |  7158 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       8 |  7159 | `		pGen->pIn++;` |
|       2 |  7160 | `	}` |
|       6 |  7161 | `	SySetRelease(&aUnionAlts);` |
|       6 |  7162 | `	return SXERR_CORRUPT;` |
|   30166 |  7163 |  |
|       - |  7164 | `/*` |
|       - |  7165 | ` * Compile a class method.` |
|       - |  7166 | ` *` |
|       - |  7167 | ` * Refer to the official documentation for more information` |
|       - |  7168 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7169 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7170 | ` * overloading and many more.` |
|       - |  7171 | ` */` |
|  236970 |  7172 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7173 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7174 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7175 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7176 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7177 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7178 | `	)` |
|       5 |  7179 |  |
|  236975 |  7180 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7181 | `	ph7_class_method *pMeth;` |
|       - |  7182 | `	sxi32 iFuncFlags;` |
|       - |  7183 | `	SyString *pName;` |
|       - |  7184 | `	SyToken *pEnd;` |
|       - |  7185 | `	sxi32 rc;` |
|       - |  7186 | `	/* Extract visibility level */` |
|  236975 |  7187 | `	iProtection = GetProtectionLevel(iProtection);` |
|  236975 |  7188 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  236975 |  7189 | `	iFuncFlags = 0;` |
|  236975 |  7190 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7191 | `		/* Invalid method name */` |
|     ! 0 |  7192 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7193 | `		if( rc == SXERR_ABORT ){` |
|       - |  7194 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7195 | `			return SXERR_ABORT;` |
|       - |  7196 | `		}` |
|     ! 0 |  7197 | `		goto Synchronize;` |
|       - |  7198 | `	}` |
|  236975 |  7199 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7200 | `		/* Return by reference,remember that */` |
|     ! 0 |  7201 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7202 | `		/* Jump the '&' token */` |
|     ! 0 |  7203 | `		pGen->pIn++;` |
|     ! 0 |  7204 | `	}` |
|  236975 |  7205 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7206 | `		/* Invalid method name */` |
|     ! 0 |  7207 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7208 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7209 | `			return SXERR_ABORT;` |
|       - |  7210 | `		}` |
|     ! 0 |  7211 | `		goto Synchronize;` |
|       - |  7212 | `	}` |
|       - |  7213 | `	/* Peek method name */` |
|  236975 |  7214 | `	pName = &pGen->pIn->sData;` |
|  236975 |  7215 | `	nLine = pGen->pIn->nLine;` |
|       - |  7216 | `	/* Jump the method name */` |
|  236975 |  7217 | `	pGen->pIn++;` |
|  236975 |  7218 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7219 | `		/* Abstract method */` |
|   81905 |  7220 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7221 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7222 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7223 | `				&pClass->sName,pName);` |
|     ! 0 |  7224 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7225 | `				return SXERR_ABORT;` |
|       - |  7226 | `			}` |
|     ! 0 |  7227 | `		}` |
|       - |  7228 | `		/* Assemble method signature only */` |
|   81905 |  7229 | `		doBody = FALSE;` |
|   40950 |  7230 | `	}` |
|  236975 |  7231 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7232 | `		/* Syntax error */` |
|     ! 0 |  7233 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7234 | `		if( rc == SXERR_ABORT ){` |
|       - |  7235 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7236 | `			return SXERR_ABORT;` |
|       - |  7237 | `		}` |
|     ! 0 |  7238 | `		goto Synchronize;` |
|       - |  7239 | `	}` |
|       - |  7240 | `	/* Allocate a new class_method instance */` |
|  236975 |  7241 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  236975 |  7242 | `	if( pMeth == 0 ){` |
|     ! 0 |  7243 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7244 | `		return SXERR_ABORT;` |
|       - |  7245 | `	}` |
|       - |  7246 | `	/* Jump the left parenthesis '(' */` |
|  236975 |  7247 | `	pGen->pIn++;` |
|  236975 |  7248 | `	pEnd = 0; /* cc warning */` |
|       - |  7249 | `	/* Delimit the method signature */` |
|  236975 |  7250 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  236975 |  7251 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7252 | `		/* Syntax error */` |
|       3 |  7253 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7254 | `		if( rc == SXERR_ABORT ){` |
|       - |  7255 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7256 | `			return SXERR_ABORT;` |
|       - |  7257 | `		}` |
|       3 |  7258 | `		goto Synchronize;` |
|       - |  7259 | `	}` |
|       - |  7260 | `	{` |
|  236973 |  7261 | `		int bIsCtor = 0;` |
|  236973 |  7262 | `		int bAbstractCtor = 0;` |
|  345955 |  7263 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  140607 |  7264 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  227476 |  7265 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   18999 |  7266 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7267 | `				bAbstractCtor = 1;` |
|       2 |  7268 | `			}else{` |
|   18997 |  7269 | `				bIsCtor = 1;` |
|       - |  7270 | `			}` |
|    9497 |  7271 | `		}` |
|  236973 |  7272 | `		if( pGen->pIn < pEnd ){` |
|       - |  7273 | `			/* Collect method arguments */` |
|   53857 |  7274 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   53857 |  7275 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7276 | `				return SXERR_ABORT;` |
|       - |  7277 | `			}` |
|   26926 |  7278 | `		}` |
|       - |  7279 | `	}` |
|       - |  7280 | `	/* Point past ')' and parse optional return type ': type' */` |
|  236973 |  7281 | `	pGen->pIn = &pEnd[1];` |
|       - |  7282 | `	{` |
|  236973 |  7283 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  236973 |  7284 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7285 | `			return SXERR_ABORT;` |
|  236973 |  7286 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7287 | `			goto Synchronize;` |
|       - |  7288 | `		}` |
|       - |  7289 | `	}` |
|       - |  7290 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7291 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7292 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7293 | `	{` |
|  236973 |  7294 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7295 | `		sxu32 i;` |
|  322395 |  7296 | `		for( i = 0; i < nArg; i++ ){` |
|   85435 |  7297 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7298 | `			ph7_class_attr *pAttr;` |
|   85435 |  7299 | `			sxi32 iAttrFlags = 0;` |
|   85435 |  7300 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   85397 |  7301 | `				continue;` |
|       - |  7302 | `			}` |
|      43 |  7303 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7304 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7305 | `					"Cannot declare variadic promoted property");` |
|       3 |  7306 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7307 | `					return SXERR_ABORT;` |
|       - |  7308 | `				}` |
|       3 |  7309 | `				goto Synchronize;` |
|       - |  7310 | `			}` |
|       - |  7311 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7312 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7313 | `			 * appear as an alternative of a union type. */` |
|      36 |  7314 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      11 |  7315 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      56 |  7316 | `				rc = GenStateValidatePropertyType(pGen,pClass,&pArg->sName,` |
|      34 |  7317 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      34 |  7318 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      17 |  7319 | `					nLine);` |
|      39 |  7320 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7321 | `					return SXERR_ABORT;` |
|      39 |  7322 | `				}else if( rc != SXRET_OK ){` |
|       6 |  7323 | `					goto Synchronize;` |
|       - |  7324 | `				}` |
|      15 |  7325 | `			}` |
|       - |  7326 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      36 |  7327 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7328 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7329 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7330 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7331 | `					return SXERR_ABORT;` |
|       - |  7332 | `				}` |
|       3 |  7333 | `				goto Synchronize;` |
|       - |  7334 | `			}` |
|      33 |  7335 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      29 |  7336 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      13 |  7337 | `			}` |
|      33 |  7338 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7339 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7340 | `			}` |
|      33 |  7341 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7342 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7343 | `			}` |
|      33 |  7344 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      33 |  7345 | `			if( pAttr == 0 ){` |
|     ! 0 |  7346 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7347 | `				return SXERR_ABORT;` |
|       - |  7348 | `			}` |
|      33 |  7349 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      29 |  7350 | `				pAttr->nType = pArg->nType;` |
|      29 |  7351 | `				pAttr->sClass = pArg->sClass;` |
|      29 |  7352 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      29 |  7353 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7354 | `					sxu32 k;` |
|     ! 0 |  7355 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7356 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7357 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7358 | `					}` |
|     ! 0 |  7359 | `				}` |
|      13 |  7360 | `			}` |
|      33 |  7361 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      33 |  7362 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7363 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7364 | `				return SXERR_ABORT;` |
|       - |  7365 | `			}` |
|      18 |  7366 | `		}` |
|       - |  7367 | `	}` |
|  236965 |  7368 | `	if( doBody ){` |
|       - |  7369 | `		/* Compile method body */` |
|  155065 |  7370 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  155065 |  7371 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7372 | `			return SXERR_ABORT;` |
|       - |  7373 | `		}` |
|   77535 |  7374 | `	}else{` |
|       - |  7375 | `		/* Only method signature is allowed */` |
|   81905 |  7376 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7377 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7378 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7379 | `				if( rc == SXERR_ABORT ){` |
|       - |  7380 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7381 | `					return SXERR_ABORT;` |
|       - |  7382 | `				}` |
|     ! 0 |  7383 | `				return SXERR_CORRUPT;` |
|       - |  7384 | `			}` |
|       - |  7385 | `	}` |
|       - |  7386 | `	/* All done,install the method */` |
|  236965 |  7387 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  236965 |  7388 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7389 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7390 | `		return SXERR_ABORT;` |
|       - |  7391 | `	}` |
|  236965 |  7392 | `	return SXRET_OK;` |
|       5 |  7393 | `Synchronize:` |
|       - |  7394 | `	/* Synchronize with the first semi-colon */` |
|      34 |  7395 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      24 |  7396 | `		pGen->pIn++;` |
|       4 |  7397 | `	}` |
|      14 |  7398 | `	return SXERR_CORRUPT;` |
|  118490 |  7399 |  |
|       - |  7400 | `/*` |
|       - |  7401 | ` * Compile an object interface.` |
|       - |  7402 | ` *  According to the PHP language reference manual` |
|       - |  7403 | ` *   Object Interfaces:` |
|       - |  7404 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7405 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7406 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7407 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7408 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7409 | ` */` |
|   34672 |  7410 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  7411 |  |
|   34677 |  7412 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7413 | `	ph7_class *pClass,*pBase;` |
|       - |  7414 | `	SyToken *pEnd,*pTmp;` |
|       - |  7415 | `	SyString *pName;` |
|       - |  7416 | `	sxi32 nKwrd;` |
|       - |  7417 | `	sxi32 rc;` |
|       - |  7418 | `	/* Jump the 'interface' keyword */` |
|   34677 |  7419 | `	pGen->pIn++;` |
|       - |  7420 | `	/* Extract interface name */` |
|   34677 |  7421 | `	pName = &pGen->pIn->sData;` |
|       - |  7422 | `	/* Advance the stream cursor */` |
|   34677 |  7423 | `	pGen->pIn++;` |
|       - |  7424 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7425 | `		SyBlob sFQN;` |
|       - |  7426 | `		SyString sFQNStr;` |
|   34677 |  7427 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   34677 |  7428 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   34677 |  7429 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   34677 |  7430 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   34677 |  7431 | `		SyBlobRelease(&sFQN);` |
|       - |  7432 | `	}` |
|   34677 |  7433 | `	if( pClass == 0 ){` |
|     ! 0 |  7434 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7435 | `		return SXERR_ABORT;` |
|       - |  7436 | `	}` |
|       - |  7437 | `	/* Mark as an interface */` |
|   34677 |  7438 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7439 | `	/* Assume no base class is given */` |
|   34677 |  7440 | `	pBase = 0;` |
|   34677 |  7441 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    9457 |  7442 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    9457 |  7443 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7444 | `			SyBlob sResolved;` |
|       - |  7445 | `			SyString sBaseName;` |
|       - |  7446 | `			sxu32 nRefLine;` |
|       - |  7447 | `			/* Extract base interface */` |
|    9457 |  7448 | `			pGen->pIn++;` |
|    9457 |  7449 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    9457 |  7450 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    9457 |  7451 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7452 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7453 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7454 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7455 | `					pName);` |
|     ! 0 |  7456 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7457 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7458 | `					return SXERR_ABORT;` |
|       - |  7459 | `				}` |
|     ! 0 |  7460 | `				return SXRET_OK;` |
|       - |  7461 | `			}` |
|   14183 |  7462 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    9452 |  7463 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    9457 |  7464 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7465 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7466 | `			/* Only interfaces is allowed */` |
|    9457 |  7467 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7468 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7469 | `			}` |
|    9457 |  7470 | `			if( pBase == 0 ){` |
|     ! 0 |  7471 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7472 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  7473 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7474 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7475 | `					return SXERR_ABORT;` |
|       - |  7476 | `				}` |
|     ! 0 |  7477 | `			}` |
|    9457 |  7478 | `			SyBlobRelease(&sResolved);` |
|    4726 |  7479 | `		}` |
|    4726 |  7480 | `	}` |
|   34677 |  7481 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7482 | `		/* Syntax error */` |
|     ! 0 |  7483 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7484 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7485 | `		if( rc == SXERR_ABORT ){` |
|       - |  7486 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7487 | `			return SXERR_ABORT;` |
|       - |  7488 | `		}` |
|     ! 0 |  7489 | `		return SXRET_OK;` |
|       - |  7490 | `	}` |
|   34677 |  7491 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   34677 |  7492 | `	pEnd = 0; /* cc warning */` |
|       - |  7493 | `	/* Delimit the interface body */` |
|   34677 |  7494 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   34677 |  7495 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7496 | `		/* Syntax error */` |
|     ! 0 |  7497 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7498 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7499 | `		if( rc == SXERR_ABORT ){` |
|       - |  7500 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7501 | `			return SXERR_ABORT;` |
|       - |  7502 | `		}` |
|     ! 0 |  7503 | `		return SXRET_OK;` |
|       - |  7504 | `	}` |
|       - |  7505 | `	/* Swap token stream */` |
|   34677 |  7506 | `	pTmp = pGen->pEnd;` |
|   34677 |  7507 | `	pGen->pEnd = pEnd;` |
|       - |  7508 | `	/* Start the parse process` |
|       - |  7509 | `	 * Note (According to the PHP reference manual):` |
|       - |  7510 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7511 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7512 | `	 */` |
|   58280 |  7513 | `	for(;;){` |
|       - |  7514 | `		/* Jump leading/trailing semi-colons */` |
|  198453 |  7515 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   81893 |  7516 | `			pGen->pIn++;` |
|       5 |  7517 | `		}` |
|  116565 |  7518 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7519 | `			/* End of interface body */` |
|   34675 |  7520 | `			break;` |
|       - |  7521 | `		}` |
|   81895 |  7522 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7523 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7524 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7525 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7526 | `			if( rc == SXERR_ABORT ){` |
|       - |  7527 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7528 | `				return SXERR_ABORT;` |
|       - |  7529 | `			}` |
|     ! 0 |  7530 | `			goto done;` |
|       - |  7531 | `		}` |
|       - |  7532 | `		/* Extract the current keyword */` |
|   81895 |  7533 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   81895 |  7534 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7535 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7536 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7537 | `			const char *zKind = "member";` |
|       3 |  7538 | `			SyString *pMemberName = 0;` |
|       3 |  7539 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7540 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7541 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7542 | `					zKind = "constant";` |
|       3 |  7543 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7544 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7545 | `					}` |
|       1 |  7546 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7547 | `					zKind = "method";` |
|     ! 0 |  7548 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7549 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7550 | `					}` |
|     ! 0 |  7551 | `				}` |
|       1 |  7552 | `			}` |
|       3 |  7553 | `			if( pMemberName ){` |
|       4 |  7554 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7555 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7556 | `			}else{` |
|     ! 0 |  7557 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7558 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7559 | `			}` |
|       3 |  7560 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7561 | `				return SXERR_ABORT;` |
|       - |  7562 | `			}` |
|       3 |  7563 | `			goto done;` |
|       - |  7564 | `		}` |
|   81893 |  7565 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7566 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7567 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7568 | `			if( rc == SXERR_ABORT ){` |
|       - |  7569 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7570 | `				return SXERR_ABORT;` |
|       - |  7571 | `			}` |
|     ! 0 |  7572 | `			goto done;` |
|       - |  7573 | `		}` |
|   81893 |  7574 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7575 | `			/* Advance the stream cursor */` |
|   81889 |  7576 | `			pGen->pIn++;` |
|   81889 |  7577 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7578 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7579 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7580 | `				if( rc == SXERR_ABORT ){` |
|       - |  7581 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7582 | `					return SXERR_ABORT;` |
|       - |  7583 | `				}` |
|     ! 0 |  7584 | `				goto done;` |
|       - |  7585 | `			}` |
|   81889 |  7586 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   81889 |  7587 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7588 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7589 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7590 | `				if( rc == SXERR_ABORT ){` |
|       - |  7591 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7592 | `					return SXERR_ABORT;` |
|       - |  7593 | `				}` |
|     ! 0 |  7594 | `				goto done;` |
|       - |  7595 | `			}` |
|   40942 |  7596 | `		}` |
|   81893 |  7597 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7598 | `			/* Parse constant */` |
|       3 |  7599 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 |  7600 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7601 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7602 | `					return SXERR_ABORT;` |
|       - |  7603 | `				}` |
|     ! 0 |  7604 | `				goto done;` |
|       - |  7605 | `			}` |
|       2 |  7606 | `		}else{` |
|   81891 |  7607 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   81891 |  7608 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7609 | `				/* Static method,record that */` |
|    9449 |  7610 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7611 | `				/* Advance the stream cursor */` |
|    9449 |  7612 | `				pGen->pIn++;` |
|    9444 |  7613 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    9449 |  7614 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7615 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7616 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7617 | `						if( rc == SXERR_ABORT ){` |
|       - |  7618 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7619 | `							return SXERR_ABORT;` |
|       - |  7620 | `						}` |
|     ! 0 |  7621 | `						goto done;` |
|       - |  7622 | `				}` |
|    4722 |  7623 | `			}` |
|       - |  7624 | `			/* Process method signature (no body for interface methods) */` |
|   81891 |  7625 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   81891 |  7626 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7627 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7628 | `					return SXERR_ABORT;` |
|       - |  7629 | `				}` |
|     ! 0 |  7630 | `				goto done;` |
|       - |  7631 | `			}` |
|       - |  7632 | `		}` |
|       5 |  7633 | `	}` |
|       - |  7634 | `	/* Install the interface */` |
|   34675 |  7635 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   34675 |  7636 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7637 | `		/* Inherit from the base interface */` |
|    9457 |  7638 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    4726 |  7639 | `	}` |
|   34675 |  7640 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7641 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7642 | `		return SXERR_ABORT;` |
|       - |  7643 | `	}` |
|   17335 |  7644 | `done:` |
|       - |  7645 | `	/* Point beyond the interface body */` |
|   34677 |  7646 | `	pGen->pIn  = &pEnd[1];` |
|   34677 |  7647 | `	pGen->pEnd = pTmp;` |
|   34677 |  7648 | `	return PH7_OK;` |
|   17341 |  7649 |  |
|       - |  7650 | `/*` |
|       - |  7651 | ` * Compile a user-defined class.` |
|       - |  7652 | ` * According to the PHP language reference manual` |
|       - |  7653 | ` *  class` |
|       - |  7654 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7655 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7656 | ` *  of the properties and methods belonging to the class.` |
|       - |  7657 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7658 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7659 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7660 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7661 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7662 | ` *  (called "methods").` |
|       - |  7663 | ` */` |
|       - |  7664 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7665 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7666 | `struct TraitUseEntry {` |
|       - |  7667 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7668 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7669 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7670 | `};` |
|       - |  7671 | `/*` |
|       - |  7672 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7673 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7674 | ` */` |
|   85930 |  7675 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  7676 |  |
|       - |  7677 | `	ph7_class **apIface;` |
|       - |  7678 | `	sxu32 nIface,i;` |
|       - |  7679 | `	sxi32 rc;` |
|   85935 |  7680 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7681 | `		return SXRET_OK;` |
|       - |  7682 | `	}` |
|   85935 |  7683 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   85935 |  7684 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  171123 |  7685 | `	for(i = 0; i < nIface; i++){` |
|   85193 |  7686 | `		ph7_class *pIface = apIface[i];` |
|       - |  7687 | `		SyHashEntry *pEntry;` |
|   85193 |  7688 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  227255 |  7689 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  142067 |  7690 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7691 | `			ph7_class_method *pImplMeth;` |
|  142067 |  7692 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7693 | `			/* Find the implementing method in the class */` |
|  142067 |  7694 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  142067 |  7695 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  7696 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  7697 | `			}` |
|       - |  7698 | `			/* Check visibility: interface methods must be implemented as public */` |
|  142053 |  7699 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  7700 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7701 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  7702 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  7703 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7704 | `					return SXERR_ABORT;` |
|       - |  7705 | `				}` |
|       1 |  7706 | `			}` |
|       - |  7707 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7708 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7709 | `			 */` |
|       - |  7710 | `			{` |
|  142053 |  7711 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  142053 |  7712 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  142053 |  7713 | `				int sigError = 0;` |
|  142053 |  7714 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7715 | `					sigError = 1;` |
|  142052 |  7716 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7717 | `					/* Extra parameters must all have default values */` |
|       6 |  7718 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7719 | `					sxu32 k;` |
|       8 |  7720 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  7721 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7722 | `							sigError = 1;` |
|       3 |  7723 | `							break;` |
|       - |  7724 | `						}` |
|       2 |  7725 | `					}` |
|       2 |  7726 | `				}` |
|  142053 |  7727 | `				if( sigError ){` |
|       - |  7728 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7729 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7730 | `					sxu32 j;` |
|       6 |  7731 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  7732 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7733 | `					/* Build implementing method signature */` |
|       6 |  7734 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  7735 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  7736 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  7737 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  7738 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  7739 | `					}` |
|       - |  7740 | `					/* Build interface method signature */` |
|       6 |  7741 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  7742 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  7743 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  7744 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  7745 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  7746 | `					}` |
|       8 |  7747 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7748 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7749 | `						&pClass->sName,pMName,` |
|       4 |  7750 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7751 | `						&pIface->sName,pMName,` |
|       4 |  7752 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  7753 | `					SyBlobRelease(&sImplSig);` |
|       6 |  7754 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  7755 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7756 | `						return SXERR_ABORT;` |
|       - |  7757 | `					}` |
|       2 |  7758 | `				}` |
|       - |  7759 | `			}` |
|       5 |  7760 | `		}` |
|   42599 |  7761 | `	}` |
|   85935 |  7762 | `	return SXRET_OK;` |
|   42970 |  7763 |  |
|       - |  7764 | `/*` |
|       - |  7765 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7766 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7767 | ` */` |
|   85930 |  7768 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  7769 |  |
|       - |  7770 | `	ph7_class_method *pMeth;` |
|       - |  7771 | `	SyHashEntry *pEntry;` |
|       - |  7772 | `	sxu32 nAbstract;` |
|       - |  7773 | `	SyBlob sMsg;` |
|       - |  7774 | `	sxi32 rc;` |
|       - |  7775 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   85935 |  7776 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      27 |  7777 | `		return SXRET_OK;` |
|       - |  7778 | `	}` |
|       - |  7779 | `	/* Count abstract methods */` |
|   85913 |  7780 | `	nAbstract = 0;` |
|   85913 |  7781 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  833373 |  7782 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  747465 |  7783 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  747465 |  7784 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  7785 | `			nAbstract++;` |
|       8 |  7786 | `		}` |
|       5 |  7787 | `	}` |
|   85913 |  7788 | `	if( nAbstract == 0 ){` |
|   85899 |  7789 | `		return SXRET_OK;` |
|       - |  7790 | `	}` |
|       - |  7791 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  7792 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  7793 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7794 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7795 | `		&pClass->sName,nAbstract,` |
|       7 |  7796 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7797 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7798 | `	/* Second pass: list methods with origins */` |
|       - |  7799 | `	{` |
|      18 |  7800 | `		sxu32 nListed = 0;` |
|      18 |  7801 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  7802 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  7803 | `			ph7_class *pOrigin = 0;` |
|       - |  7804 | `			SyString *pMName;` |
|      22 |  7805 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  7806 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7807 | `				continue;` |
|       - |  7808 | `			}` |
|      20 |  7809 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  7810 | `			if( nListed > 0 ){` |
|       3 |  7811 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7812 | `			}` |
|       - |  7813 | `			/* Find the origin of this abstract method.` |
|       - |  7814 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7815 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7816 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7817 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7818 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7819 | `			 * class's namespace.` |
|       - |  7820 | `			 */` |
|       - |  7821 | `			{` |
|       - |  7822 | `				ph7_class **apIface;` |
|       - |  7823 | `				ph7_class **apTrait;` |
|       - |  7824 | `				ph7_class *pWalk;` |
|       - |  7825 | `				sxu32 i;` |
|       - |  7826 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7827 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  7828 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  7829 | `				 */` |
|      20 |  7830 | `				if( pClass->pBase ){` |
|      11 |  7831 | `					pWalk = pClass->pBase;` |
|      19 |  7832 | `					while( pWalk ){` |
|       - |  7833 | `						ph7_class_method *pParentMeth;` |
|      13 |  7834 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  7835 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  7836 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  7837 | `							 * in this class's ancestor chain.` |
|       - |  7838 | `							 */` |
|      13 |  7839 | `							int fromIface = 0;` |
|      13 |  7840 | `							ph7_class *pAnc = pWalk;` |
|      17 |  7841 | `							while( pAnc ){` |
|       - |  7842 | `								ph7_class **apPI;` |
|       - |  7843 | `								sxu32 j;` |
|      15 |  7844 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  7845 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  7846 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  7847 | `										fromIface = 1;` |
|      10 |  7848 | `										break;` |
|       - |  7849 | `									}` |
|     ! 0 |  7850 | `								}` |
|      15 |  7851 | `								if( fromIface ) break;` |
|       6 |  7852 | `								pAnc = pAnc->pBase;` |
|       2 |  7853 | `							}` |
|      13 |  7854 | `							if( !fromIface ){` |
|       3 |  7855 | `								pOrigin = pWalk;` |
|       3 |  7856 | `								break;` |
|       - |  7857 | `							}` |
|       4 |  7858 | `						}` |
|      10 |  7859 | `						pWalk = pWalk->pBase;` |
|       2 |  7860 | `					}` |
|       4 |  7861 | `				}` |
|       - |  7862 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  7863 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  7864 | `				 */` |
|      20 |  7865 | `				if( !pOrigin ){` |
|      18 |  7866 | `					pWalk = pClass;` |
|      40 |  7867 | `					while( pWalk && !pOrigin ){` |
|      26 |  7868 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  7869 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  7870 | `							ph7_class *pIface = apIface[i];` |
|      16 |  7871 | `							ph7_class *pDeepest = 0;` |
|      28 |  7872 | `							while( pIface ){` |
|      16 |  7873 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  7874 | `									pDeepest = pIface;` |
|       6 |  7875 | `								}` |
|      16 |  7876 | `								pIface = pIface->pBase;` |
|       4 |  7877 | `							}` |
|      16 |  7878 | `							if( pDeepest ){` |
|      16 |  7879 | `								pOrigin = pDeepest;` |
|      16 |  7880 | `								break;` |
|       - |  7881 | `							}` |
|     ! 0 |  7882 | `						}` |
|      26 |  7883 | `						pWalk = pWalk->pBase;` |
|       4 |  7884 | `					}` |
|       7 |  7885 | `				}` |
|       - |  7886 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  7887 | `				if( !pOrigin ){` |
|       3 |  7888 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  7889 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  7890 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  7891 | `							pOrigin = pClass;` |
|       3 |  7892 | `							break;` |
|       - |  7893 | `						}` |
|     ! 0 |  7894 | `					}` |
|       1 |  7895 | `				}` |
|       - |  7896 | `			}` |
|      20 |  7897 | `			if( pOrigin ){` |
|      20 |  7898 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  7899 | `			}else{` |
|       - |  7900 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  7901 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  7902 | `			}` |
|      20 |  7903 | `			nListed++;` |
|       4 |  7904 | `		}` |
|       - |  7905 | `	}` |
|      18 |  7906 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  7907 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  7908 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  7909 | `	SyBlobRelease(&sMsg);` |
|      18 |  7910 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7911 | `		return SXERR_ABORT;` |
|       - |  7912 | `	}` |
|      18 |  7913 | `	return SXRET_OK;` |
|   42970 |  7914 |  |
|       - |  7915 | `/*` |
|       - |  7916 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  7917 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  7918 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  7919 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  7920 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  7921 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  7922 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  7923 | ` */` |
|   85670 |  7924 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  7925 |  |
|   85675 |  7926 | `	int isAbsolute = 0;` |
|   85675 |  7927 | `	SyToken *pStart = pGen->pIn;` |
|       - |  7928 | `	SyBlob sName;` |
|   85675 |  7929 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      33 |  7930 | `		isAbsolute = 1;` |
|      33 |  7931 | `		pGen->pIn++;` |
|      15 |  7932 | `	}` |
|   85675 |  7933 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       9 |  7934 | `		pGen->pIn = pStart;` |
|       9 |  7935 | `		return SXERR_INVALID;` |
|       - |  7936 | `	}` |
|   85669 |  7937 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   85669 |  7938 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   85669 |  7939 | `	pGen->pIn++;` |
|  128514 |  7940 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   42855 |  7941 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  7942 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  7943 | `		pGen->pIn++;` |
|      13 |  7944 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  7945 | `		pGen->pIn++;` |
|       1 |  7946 | `	}` |
|   85669 |  7947 | `	if( isAbsolute ){` |
|      30 |  7948 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      16 |  7949 | `	}else{` |
|       - |  7950 | `		SyString sRaw;` |
|   85641 |  7951 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   85641 |  7952 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  7953 | `	}` |
|   85669 |  7954 | `	SyBlobRelease(&sName);` |
|   85669 |  7955 | `	return SXRET_OK;` |
|   42840 |  7956 |  |
|       - |  7957 | `/*` |
|       - |  7958 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  7959 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  7960 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  7961 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  7962 | ` * either direction cannot run unbounded.` |
|       - |  7963 | ` */` |
|       - |  7964 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    9562 |  7965 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  7966 |  |
|       - |  7967 | `	ph7_class **apParent;` |
|       - |  7968 | `	sxu32 n;` |
|   16003 |  7969 | `	while( pInterface ){` |
|   12747 |  7970 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  7971 | `			return FALSE;` |
|       - |  7972 | `		}` |
|   15907 |  7973 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    6320 |  7974 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    6311 |  7975 | `			return TRUE;` |
|       - |  7976 | `		}` |
|    6441 |  7977 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    6441 |  7978 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  7979 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  7980 | `				return TRUE;` |
|       - |  7981 | `			}` |
|     ! 0 |  7982 | `		}` |
|    6441 |  7983 | `		pInterface = pInterface->pBase;` |
|    6441 |  7984 | `		iDepth++;` |
|       5 |  7985 | `	}` |
|    3261 |  7986 | `	return FALSE;` |
|    4786 |  7987 |  |
|    9562 |  7988 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  7989 |  |
|    9567 |  7990 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  7991 |  |
|       - |  7992 | `/*` |
|       - |  7993 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  7994 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  7995 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  7996 | ` */` |
|    6306 |  7997 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  7998 |  |
|    6315 |  7999 | `	while( pBase ){` |
|      10 |  8000 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8001 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8002 | `			return TRUE;` |
|       - |  8003 | `		}` |
|      10 |  8004 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8005 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8006 | `			return TRUE;` |
|       - |  8007 | `		}` |
|       5 |  8008 | `		pBase = pBase->pBase;` |
|       1 |  8009 | `	}` |
|    6307 |  8010 | `	return FALSE;` |
|    3158 |  8011 |  |
|   85946 |  8012 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  8013 |  |
|   85951 |  8014 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8015 | `	ph7_class *pClass,*pBase;` |
|       - |  8016 | `	SyToken *pEnd,*pTmp;` |
|       - |  8017 | `	sxi32 iProtection;` |
|       - |  8018 | `	SySet aInterfaces;` |
|       - |  8019 | `	SySet aUseEntries;` |
|       - |  8020 | `	sxi32 iAttrflags;` |
|       - |  8021 | `	SyString *pName;` |
|       - |  8022 | `	sxi32 nKwrd;` |
|       - |  8023 | `	sxi32 rc;` |
|       - |  8024 | `	/* Jump the 'class' keyword */` |
|   85951 |  8025 | `	pGen->pIn++;` |
|   85951 |  8026 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8027 | `		/* Syntax error */` |
|     ! 0 |  8028 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8029 | `		if( rc == SXERR_ABORT ){` |
|       - |  8030 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8031 | `			return SXERR_ABORT;` |
|       - |  8032 | `		}` |
|       - |  8033 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8034 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8035 | `			pGen->pIn++;` |
|     ! 0 |  8036 | `		}` |
|     ! 0 |  8037 | `		return SXRET_OK;` |
|       - |  8038 | `	}` |
|       - |  8039 | `	/* Extract class name */` |
|   85951 |  8040 | `	pName = &pGen->pIn->sData;` |
|       - |  8041 | `	/* Advance the stream cursor */` |
|   85951 |  8042 | `	pGen->pIn++;` |
|       - |  8043 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8044 | `		SyBlob sFQN;` |
|       - |  8045 | `		SyString sFQNStr;` |
|   85951 |  8046 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   85951 |  8047 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   85951 |  8048 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   85951 |  8049 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   85951 |  8050 | `		SyBlobRelease(&sFQN);` |
|       - |  8051 | `	}` |
|   85951 |  8052 | `	if( pClass == 0 ){` |
|     ! 0 |  8053 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8054 | `		return SXERR_ABORT;` |
|       - |  8055 | `	}` |
|       - |  8056 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   85951 |  8057 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   85951 |  8058 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8059 | `	/* Assume a standalone class */` |
|   85951 |  8060 | `	pBase = 0;` |
|   85951 |  8061 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   75813 |  8062 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   75813 |  8063 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8064 | `			SyBlob sResolved;` |
|       - |  8065 | `			SyString sBaseName;` |
|       - |  8066 | `			sxu32 nRefLine;` |
|   66257 |  8067 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   66257 |  8068 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   66257 |  8069 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   66257 |  8070 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8071 | `				SyBlobRelease(&sResolved);` |
|       4 |  8072 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8073 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8074 | `					pName);` |
|       3 |  8075 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8076 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8077 | `					return SXERR_ABORT;` |
|       - |  8078 | `				}` |
|       3 |  8079 | `				return SXRET_OK;` |
|       - |  8080 | `			}` |
|   99380 |  8081 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   66250 |  8082 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   66255 |  8083 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8084 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8085 | `			/* Interfaces are not allowed */` |
|   66255 |  8086 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8087 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8088 | `			}` |
|   66255 |  8089 | `			if( pBase == 0 ){` |
|     ! 0 |  8090 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8091 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8092 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8093 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8094 | `					return SXERR_ABORT;` |
|       - |  8095 | `				}` |
|     ! 0 |  8096 | `			}else{` |
|   66255 |  8097 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8098 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8099 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8100 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8101 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8102 | `						return SXERR_ABORT;` |
|       - |  8103 | `					}` |
|     ! 0 |  8104 | `				}` |
|       - |  8105 | `			}` |
|   66255 |  8106 | `			SyBlobRelease(&sResolved);` |
|   33125 |  8107 | `		}` |
|   75811 |  8108 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8109 | `			ph7_class *pInterface;` |
|       - |  8110 | `			/* Interface implementation */` |
|    9567 |  8111 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    4781 |  8112 | `			for(;;){` |
|       - |  8113 | `				SyBlob sResolved;` |
|       - |  8114 | `				SyString sIntName;` |
|       - |  8115 | `				sxu32 nRefLine;` |
|    9567 |  8116 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    9567 |  8117 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    9567 |  8118 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8119 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8120 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8121 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8122 | `						pName);` |
|     ! 0 |  8123 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8124 | `						return SXERR_ABORT;` |
|       - |  8125 | `					}` |
|     ! 0 |  8126 | `					break;` |
|       - |  8127 | `				}` |
|   19129 |  8128 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    9562 |  8129 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    9567 |  8130 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8131 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8132 | `				/* Only interfaces are allowed */` |
|    9567 |  8133 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8134 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8135 | `				}` |
|    9567 |  8136 | `				if( pInterface == 0 ){` |
|     ! 0 |  8137 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8138 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8139 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8140 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8141 | `						return SXERR_ABORT;` |
|       - |  8142 | `					}` |
|     ! 0 |  8143 | `				}else{` |
|       - |  8144 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8145 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8146 | `					 * unless they already extend Exception or Error.` |
|       - |  8147 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8148 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8149 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    9567 |  8150 | `					SyString *pFqn = &pClass->sName;` |
|    9567 |  8151 | `					int bIsExceptionOrError =` |
|    7931 |  8152 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   15919 |  8153 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    7993 |  8154 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3158 |  8155 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   15866 |  8156 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    9462 |  8157 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3151 |  8158 | `						!bIsExceptionOrError ){` |
|      12 |  8159 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8160 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8161 | `							&pClass->sName);` |
|       9 |  8162 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8163 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8164 | `							return SXERR_ABORT;` |
|       - |  8165 | `						}` |
|       - |  8166 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8167 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8168 | `					}else{` |
|    9561 |  8169 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8170 | `					}` |
|       - |  8171 | `				}` |
|    9567 |  8172 | `				SyBlobRelease(&sResolved);` |
|    9567 |  8173 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    4786 |  8174 | `					break;` |
|       - |  8175 | `				}` |
|     ! 0 |  8176 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  8177 | `			}` |
|    4781 |  8178 | `		}` |
|   37903 |  8179 | `	}` |
|   85949 |  8180 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8181 | `		/* Syntax error */` |
|     ! 0 |  8182 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8183 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8184 | `		if( rc == SXERR_ABORT ){` |
|       - |  8185 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8186 | `			return SXERR_ABORT;` |
|       - |  8187 | `		}` |
|     ! 0 |  8188 | `		return SXRET_OK;` |
|       - |  8189 | `	}` |
|   85949 |  8190 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   85949 |  8191 | `	pEnd = 0; /* cc warning */` |
|       - |  8192 | `	/* Delimit the class body */` |
|   85949 |  8193 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   85949 |  8194 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8195 | `		/* Syntax error */` |
|     ! 0 |  8196 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8197 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8198 | `		if( rc == SXERR_ABORT ){` |
|       - |  8199 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8200 | `			return SXERR_ABORT;` |
|       - |  8201 | `		}` |
|     ! 0 |  8202 | `		return SXRET_OK;` |
|       - |  8203 | `	}` |
|       - |  8204 | `	/* Swap token stream */` |
|   85949 |  8205 | `	pTmp = pGen->pEnd;` |
|   85949 |  8206 | `	pGen->pEnd = pEnd;` |
|       - |  8207 | `	/* Set the inherited flags */` |
|   85949 |  8208 | `	pClass->iFlags = iFlags;` |
|       - |  8209 | `	/* Start the parse process */` |
|  120499 |  8210 | `	for(;;){` |
|       - |  8211 | `		/* Jump leading/trailing semi-colons */` |
|  361721 |  8212 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   60383 |  8213 | `			pGen->pIn++;` |
|       5 |  8214 | `		}` |
|  301343 |  8215 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8216 | `			/* End of class body */` |
|   85935 |  8217 | `			break;` |
|       - |  8218 | `		}` |
|  215413 |  8219 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8220 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8221 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8222 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8223 | `			if( rc == SXERR_ABORT ){` |
|       - |  8224 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8225 | `				return SXERR_ABORT;` |
|       - |  8226 | `			}` |
|     ! 0 |  8227 | `			goto done;` |
|       - |  8228 | `		}` |
|       - |  8229 | `		/* Assume public visibility */` |
|  215413 |  8230 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  215413 |  8231 | `		iAttrflags = 0;` |
|  215413 |  8232 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  8233 | `			/* Extract the current keyword */` |
|  215413 |  8234 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  215413 |  8235 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8236 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8237 | `				TraitUseEntry sUse;` |
|      49 |  8238 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      49 |  8239 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      49 |  8240 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      30 |  8241 | `				for(;;){` |
|       - |  8242 | `					ph7_class *pTrait;` |
|       - |  8243 | `					SyString *pTraitName;` |
|      57 |  8244 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8245 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8246 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8247 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8248 | `							return SXERR_ABORT;` |
|       - |  8249 | `						}` |
|     ! 0 |  8250 | `						break;` |
|       - |  8251 | `					}` |
|      57 |  8252 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8253 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8254 | `						SyBlob sResolved;` |
|      57 |  8255 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      57 |  8256 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     109 |  8257 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      52 |  8258 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      57 |  8259 | `						SyBlobRelease(&sResolved);` |
|       - |  8260 | `					}` |
|       - |  8261 | `					/* Only traits are allowed */` |
|      57 |  8262 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8263 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8264 | `					}` |
|      57 |  8265 | `					if( pTrait == 0 ){` |
|     ! 0 |  8266 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8267 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8268 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8269 | `							return SXERR_ABORT;` |
|       - |  8270 | `						}` |
|     ! 0 |  8271 | `					}else{` |
|      57 |  8272 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8273 | `					}` |
|      57 |  8274 | `					pGen->pIn++; /* Advance past trait name */` |
|      57 |  8275 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      27 |  8276 | `						break;` |
|       - |  8277 | `					}` |
|      10 |  8278 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  8279 | `				}` |
|       - |  8280 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      49 |  8281 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8282 | `					SyToken *pBlock;` |
|      10 |  8283 | `					pGen->pIn++; /* Jump '{' */` |
|      10 |  8284 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      10 |  8285 | `					sUse.pResolvStart = pGen->pIn;` |
|      10 |  8286 | `					sUse.pResolvEnd = pBlock;` |
|      10 |  8287 | `					if( pBlock < pGen->pEnd ){` |
|      10 |  8288 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       6 |  8289 | `					}else{` |
|     ! 0 |  8290 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8291 | `					}` |
|       4 |  8292 | `				}` |
|      49 |  8293 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8294 | `				/* The semicolon will be consumed by the outer loop */` |
|      49 |  8295 | `				continue;` |
|       - |  8296 | `			}` |
|  215369 |  8297 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  212097 |  8298 | `				iProtection = nKwrd;` |
|  212097 |  8299 | `				pGen->pIn++; /* Jump the visibility token */` |
|  212092 |  8300 | `				if( pGen->pIn >= pGen->pEnd` |
|  212097 |  8301 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8302 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8303 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8304 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8305 | `					if( rc == SXERR_ABORT ){` |
|       - |  8306 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8307 | `						return SXERR_ABORT;` |
|       - |  8308 | `					}` |
|     ! 0 |  8309 | `					goto done;` |
|       - |  8310 | `				}` |
|  212097 |  8311 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8312 | `					/* Attribute declaration (untyped) */` |
|   60169 |  8313 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   60169 |  8314 | `					if( rc != SXRET_OK ){` |
|       3 |  8315 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8316 | `							return SXERR_ABORT;` |
|       - |  8317 | `						}` |
|       3 |  8318 | `						goto done;` |
|       - |  8319 | `					}` |
|   60167 |  8320 | `					continue;` |
|       - |  8321 | `				}` |
|  151933 |  8322 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8323 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     127 |  8324 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     127 |  8325 | `					if( rc != SXRET_OK ){` |
|       3 |  8326 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8327 | `							return SXERR_ABORT;` |
|       - |  8328 | `						}` |
|       3 |  8329 | `						goto done;` |
|       - |  8330 | `					}` |
|     125 |  8331 | `					continue;` |
|       - |  8332 | `				}` |
|       - |  8333 | `				/* Extract the keyword */` |
|  151811 |  8334 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   75903 |  8335 | `			}` |
|  155083 |  8336 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8337 | `				/* Process constant declaration */` |
|      35 |  8338 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      35 |  8339 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8340 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8341 | `						return SXERR_ABORT;` |
|       - |  8342 | `					}` |
|     ! 0 |  8343 | `					goto done;` |
|       - |  8344 | `				}` |
|      20 |  8345 | `			}else{` |
|  155053 |  8346 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8347 | `					/* Static method or attribute,record that */` |
|    3193 |  8348 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    3193 |  8349 | `					pGen->pIn++; /* Jump the static keyword */` |
|    3193 |  8350 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8351 | `						/* Extract the keyword */` |
|    3187 |  8352 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    3187 |  8353 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8354 | `							iProtection = nKwrd;` |
|     ! 0 |  8355 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8356 | `						}` |
|    1591 |  8357 | `					}` |
|    3188 |  8358 | `					if( pGen->pIn >= pGen->pEnd` |
|    3193 |  8359 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8360 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8361 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8362 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8363 | `						if( rc == SXERR_ABORT ){` |
|       - |  8364 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8365 | `							return SXERR_ABORT;` |
|       - |  8366 | `						}` |
|     ! 0 |  8367 | `						goto done;` |
|       - |  8368 | `					}` |
|    3193 |  8369 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8370 | `						/* Attribute declaration */` |
|       5 |  8371 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8372 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8373 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8374 | `								return SXERR_ABORT;` |
|       - |  8375 | `							}` |
|     ! 0 |  8376 | `							goto done;` |
|       - |  8377 | `						}` |
|       5 |  8378 | `						continue;` |
|       - |  8379 | `					}` |
|    3189 |  8380 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8381 | `						/* Typed static attribute declaration */` |
|      13 |  8382 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      13 |  8383 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8384 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8385 | `								return SXERR_ABORT;` |
|       - |  8386 | `							}` |
|     ! 0 |  8387 | `							goto done;` |
|       - |  8388 | `						}` |
|      13 |  8389 | `						continue;` |
|       - |  8390 | `					}` |
|       - |  8391 | `					/* Extract the keyword */` |
|    3179 |  8392 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  153452 |  8393 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8394 | `					/* Abstract method,record that */` |
|      12 |  8395 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8396 | `					/* Mark the whole class as abstract */` |
|      12 |  8397 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8398 | `					/* Advance the stream cursor */` |
|      12 |  8399 | `					pGen->pIn++;` |
|      12 |  8400 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8401 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8402 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8403 | `							iProtection = nKwrd;` |
|      10 |  8404 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8405 | `						}` |
|       5 |  8406 | `					}` |
|      12 |  8407 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8408 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8409 | `							/* Static method */` |
|     ! 0 |  8410 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8411 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8412 | `					}` |
|      12 |  8413 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8414 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8415 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8416 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8417 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8418 | `							if( rc == SXERR_ABORT ){` |
|       - |  8419 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8420 | `								return SXERR_ABORT;` |
|       - |  8421 | `							}` |
|     ! 0 |  8422 | `							goto done;` |
|       - |  8423 | `					}` |
|      12 |  8424 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  151860 |  8425 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8426 | `					/* final method ,record that */` |
|       6 |  8427 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       6 |  8428 | `					pGen->pIn++; /* Jump the final keyword */` |
|       6 |  8429 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8430 | `						/* Extract the keyword */` |
|       6 |  8431 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  8432 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  8433 | `							iProtection = nKwrd;` |
|       6 |  8434 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 |  8435 | `						}` |
|       2 |  8436 | `					}` |
|       6 |  8437 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8438 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8439 | `							/* Static method */` |
|     ! 0 |  8440 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8441 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8442 | `					}` |
|       6 |  8443 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8444 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8445 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8446 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8447 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8448 | `							if( rc == SXERR_ABORT ){` |
|       - |  8449 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8450 | `								return SXERR_ABORT;` |
|       - |  8451 | `							}` |
|     ! 0 |  8452 | `							goto done;` |
|       - |  8453 | `					}` |
|       6 |  8454 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8455 | `				}` |
|  155039 |  8456 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8457 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8458 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8459 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8460 | `						if( rc == SXERR_ABORT ){` |
|       - |  8461 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8462 | `							return SXERR_ABORT;` |
|       - |  8463 | `						}` |
|     ! 0 |  8464 | `						goto done;` |
|       - |  8465 | `				}` |
|  155039 |  8466 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8467 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8468 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8469 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8470 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8471 | `						if( rc == SXERR_ABORT ){` |
|       - |  8472 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8473 | `							return SXERR_ABORT;` |
|       - |  8474 | `						}` |
|     ! 0 |  8475 | `						goto done;` |
|       - |  8476 | `					}` |
|       - |  8477 | `					/* Attribute declaration */` |
|       7 |  8478 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8479 | `				}else{` |
|       - |  8480 | `					/* Process method declaration */` |
|  155033 |  8481 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8482 | `				}` |
|  155039 |  8483 | `				if( rc != SXRET_OK ){` |
|      14 |  8484 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8485 | `						return SXERR_ABORT;` |
|       - |  8486 | `					}` |
|      14 |  8487 | `					goto done;` |
|       - |  8488 | `				}` |
|       - |  8489 | `			}` |
|   77532 |  8490 | `		}else{` |
|       - |  8491 | `			/* Attribute declaration */` |
|     ! 0 |  8492 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8493 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8494 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8495 | `					return SXERR_ABORT;` |
|       - |  8496 | `				}` |
|     ! 0 |  8497 | `				goto done;` |
|       - |  8498 | `			}` |
|       - |  8499 | `		}` |
|       5 |  8500 | `	}` |
|       - |  8501 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8502 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8503 | `	 */` |
|       - |  8504 | `	{` |
|       - |  8505 | `		TraitUseEntry *apUse;` |
|       - |  8506 | `		sxu32 nU;` |
|   85935 |  8507 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   85979 |  8508 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      49 |  8509 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      49 |  8510 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      49 |  8511 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      49 |  8512 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8513 | `			sxu32 nT;` |
|      49 |  8514 | `			if( !hasResolution ){` |
|       - |  8515 | `				/* No conflict resolution block: use standard trait application */` |
|      83 |  8516 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      47 |  8517 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      47 |  8518 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8519 | `						break;` |
|       - |  8520 | `					}` |
|      26 |  8521 | `				}` |
|      23 |  8522 | `			}else{` |
|       - |  8523 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8524 | `				 * then use the block to resolve method conflicts.` |
|       - |  8525 | `				 */` |
|       - |  8526 | `				SyToken *pR;` |
|      20 |  8527 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      12 |  8528 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8529 | `					ph7_class_attr *pAR;` |
|       - |  8530 | `					SyHashEntry *pER;` |
|       - |  8531 | `					SyString *pNR;` |
|      12 |  8532 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      17 |  8533 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8534 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8535 | `						pNR = &pAR->sName;` |
|     ! 0 |  8536 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8537 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8538 | `						}` |
|     ! 0 |  8539 | `					}` |
|      12 |  8540 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       7 |  8541 | `				}` |
|       - |  8542 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      10 |  8543 | `				pR = pUse->pResolvStart;` |
|      22 |  8544 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8545 | `					SyString sTrait,sMethod;` |
|       - |  8546 | `					ph7_class *pSrcTrait;` |
|       - |  8547 | `					ph7_class_method *pMeth;` |
|       - |  8548 | `					sxi32 nRKwrd;` |
|      34 |  8549 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      22 |  8550 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      14 |  8551 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      14 |  8552 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      14 |  8553 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      14 |  8554 | `					sMethod = pR->sData;` |
|      14 |  8555 | `					pR++;` |
|      14 |  8556 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8557 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8558 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8559 | `							sTrait = sMethod;` |
|       7 |  8560 | `							pR++;` |
|       7 |  8561 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8562 | `							sMethod = pR->sData;` |
|       7 |  8563 | `							pR++;` |
|       3 |  8564 | `						}` |
|       3 |  8565 | `					}` |
|      14 |  8566 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8567 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8568 | `						continue;` |
|       - |  8569 | `					}` |
|      14 |  8570 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      14 |  8571 | `					pR++;` |
|      14 |  8572 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8573 | `						pSrcTrait = 0;` |
|       7 |  8574 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8575 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8576 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8577 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8578 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8579 | `								break;` |
|       - |  8580 | `							}` |
|       2 |  8581 | `						}` |
|       5 |  8582 | `						if( pSrcTrait ){` |
|       5 |  8583 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8584 | `							if( pMeth ){` |
|       5 |  8585 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8586 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8587 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8588 | `								}` |
|       2 |  8589 | `							}` |
|       2 |  8590 | `						}` |
|       2 |  8591 | `					}` |
|      30 |  8592 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       2 |  8593 | `				}` |
|       - |  8594 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      20 |  8595 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8596 | `					ph7_class_method *pMR;` |
|       - |  8597 | `					SyHashEntry *pER;` |
|       - |  8598 | `					SyString *pNR;` |
|      12 |  8599 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      35 |  8600 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      20 |  8601 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      20 |  8602 | `						pNR = &pMR->sFunc.sName;` |
|      20 |  8603 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8604 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8605 | `						}` |
|       2 |  8606 | `					}` |
|       7 |  8607 | `				}` |
|       - |  8608 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      10 |  8609 | `				pR = pUse->pResolvStart;` |
|      22 |  8610 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8611 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8612 | `					ph7_class *pSrcTrait;` |
|       - |  8613 | `					ph7_class_method *pMeth;` |
|      22 |  8614 | `					int hasQual = 0;` |
|       - |  8615 | `					sxi32 nRKwrd;` |
|      34 |  8616 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      22 |  8617 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      14 |  8618 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      14 |  8619 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      14 |  8620 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      14 |  8621 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      14 |  8622 | `					sMethod = pR->sData;` |
|      14 |  8623 | `					pR++;` |
|      14 |  8624 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8625 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8626 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8627 | `							sTrait = sMethod;` |
|       7 |  8628 | `							hasQual = 1;` |
|       7 |  8629 | `							pR++;` |
|       7 |  8630 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8631 | `							sMethod = pR->sData;` |
|       7 |  8632 | `							pR++;` |
|       3 |  8633 | `						}` |
|       3 |  8634 | `					}` |
|      14 |  8635 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8636 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8637 | `						continue;` |
|       - |  8638 | `					}` |
|      14 |  8639 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      14 |  8640 | `					pR++;` |
|      14 |  8641 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      10 |  8642 | `						sxi32 iNewVis = -1;` |
|      10 |  8643 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  8644 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  8645 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  8646 | `								iNewVis = nAK;` |
|       7 |  8647 | `								pR++;` |
|       3 |  8648 | `							}` |
|       3 |  8649 | `						}` |
|      10 |  8650 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       8 |  8651 | `							sAlias = pR->sData;` |
|       8 |  8652 | `							pR++;` |
|       3 |  8653 | `						}` |
|      10 |  8654 | `						pMeth = 0;` |
|      10 |  8655 | `						if( hasQual ){` |
|       3 |  8656 | `							pSrcTrait = 0;` |
|       5 |  8657 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  8658 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  8659 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  8660 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  8661 | `									pSrcTrait = apTrait[nT];` |
|       3 |  8662 | `									break;` |
|       - |  8663 | `								}` |
|       2 |  8664 | `							}` |
|       3 |  8665 | `							if( pSrcTrait ){` |
|       3 |  8666 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  8667 | `							}` |
|       2 |  8668 | `						}else{` |
|       7 |  8669 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  8670 | `						}` |
|      10 |  8671 | `						if( pMeth ){` |
|      10 |  8672 | `							if( sAlias.nByte > 0 ){` |
|       - |  8673 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  8674 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  8675 | `								 */` |
|       - |  8676 | `								ph7_class_method *pAlias;` |
|       - |  8677 | `								char *zAliasDup;` |
|       8 |  8678 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       8 |  8679 | `								if( pAlias ){` |
|       8 |  8680 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       8 |  8681 | `									if( iNewVis >= 0 ){` |
|       5 |  8682 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8683 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8684 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  8685 | `									}` |
|       8 |  8686 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       8 |  8687 | `									if( zAliasDup ){` |
|       8 |  8688 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  8689 | `									}` |
|       5 |  8690 | `								}` |
|       6 |  8691 | `							}else if( iNewVis >= 0 ){` |
|       - |  8692 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  8693 | `								ph7_class_method *pCopy;` |
|       3 |  8694 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  8695 | `								if( pCopy ){` |
|       3 |  8696 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  8697 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  8698 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8699 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8700 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  8701 | `									/* Replace the method in the class hash */` |
|       3 |  8702 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  8703 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  8704 | `								}` |
|       1 |  8705 | `							}` |
|       4 |  8706 | `						}` |
|       4 |  8707 | `						SXUNUSED(hasQual);` |
|       4 |  8708 | `					}` |
|      18 |  8709 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       2 |  8710 | `				}` |
|       - |  8711 | `			}` |
|      49 |  8712 | `			SySetRelease(&pUse->aTraits);` |
|      27 |  8713 | `		}` |
|       - |  8714 | `	}` |
|       - |  8715 | `	/* Install the class */` |
|   85935 |  8716 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   85935 |  8717 | `	if( rc == SXRET_OK ){` |
|       - |  8718 | `		ph7_class **apInterface;` |
|       - |  8719 | `		sxu32 n;` |
|   85935 |  8720 | `		if( pBase ){` |
|       - |  8721 | `			/* Inherit from base class and mark as a subclass */` |
|   66255 |  8722 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   33125 |  8723 | `		}` |
|   85935 |  8724 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   95491 |  8725 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  8726 | `			/* Implements one or more interface */` |
|    9561 |  8727 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    9561 |  8728 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8729 | `				break;` |
|       - |  8730 | `			}` |
|    4783 |  8731 | `		}` |
|       - |  8732 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  8733 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  128895 |  8734 | `		if( rc == SXRET_OK` |
|   85930 |  8735 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   85935 |  8736 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   75639 |  8737 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  8738 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   75639 |  8739 | `			if( pStringable ){` |
|   75639 |  8740 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   75639 |  8741 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  8742 | `				sxu32 i;` |
|   75639 |  8743 | `				int bAlready = 0;` |
|   81939 |  8744 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    6307 |  8745 | `					if( apImpl[i] == pStringable ){` |
|       3 |  8746 | `						bAlready = 1;` |
|       3 |  8747 | `						break;` |
|       - |  8748 | `					}` |
|    3155 |  8749 | `				}` |
|   75639 |  8750 | `				if( !bAlready ){` |
|   75637 |  8751 | `					PH7_ClassImplement(pClass,pStringable);` |
|   37816 |  8752 | `				}` |
|   37817 |  8753 | `			}` |
|   37817 |  8754 | `		}` |
|       - |  8755 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   85935 |  8756 | `		if( rc == SXRET_OK ){` |
|   85935 |  8757 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   85935 |  8758 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8759 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8760 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8761 | `				return SXERR_ABORT;` |
|       - |  8762 | `			}` |
|   42965 |  8763 | `		}` |
|       - |  8764 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   85935 |  8765 | `		if( rc == SXRET_OK ){` |
|   85935 |  8766 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   85935 |  8767 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8768 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8769 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8770 | `				return SXERR_ABORT;` |
|       - |  8771 | `			}` |
|   42965 |  8772 | `		}` |
|   42965 |  8773 | `	}` |
|   85935 |  8774 | `	SySetRelease(&aUseEntries);` |
|   85935 |  8775 | `	SySetRelease(&aInterfaces);` |
|   85935 |  8776 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8777 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8778 | `		return SXERR_ABORT;` |
|       - |  8779 | `	}` |
|   42965 |  8780 | `done:` |
|       - |  8781 | `	/* Point beyond the class body */` |
|   85949 |  8782 | `	pGen->pIn = &pEnd[1];` |
|   85949 |  8783 | `	pGen->pEnd = pTmp;` |
|   85949 |  8784 | `	return PH7_OK;` |
|   42978 |  8785 |  |
|       - |  8786 | `/*` |
|       - |  8787 | ` * Compile a user-defined abstract class.` |
|       - |  8788 | ` *  According to the PHP language reference manual` |
|       - |  8789 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  8790 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  8791 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  8792 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  8793 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  8794 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  8795 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  8796 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  8797 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  8798 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  8799 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  8800 | ` *   could differ.` |
|       - |  8801 | ` */` |
|      20 |  8802 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       5 |  8803 |  |
|       - |  8804 | `	sxi32 rc;` |
|      25 |  8805 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      25 |  8806 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      25 |  8807 | `	return rc;` |
|       5 |  8808 |  |
|       - |  8809 | `/*` |
|       - |  8810 | ` * Compile a user-defined final class.` |
|       - |  8811 | ` *  According to the PHP language reference manual` |
|       - |  8812 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - |  8813 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - |  8814 | ` *    final then it cannot be extended.` |
|       - |  8815 | ` */` |
|       2 |  8816 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 |  8817 |  |
|       - |  8818 | `	sxi32 rc;` |
|       3 |  8819 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 |  8820 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 |  8821 | `	return rc;` |
|       1 |  8822 |  |
|       - |  8823 | `/*` |
|       - |  8824 | ` * Compile a user-defined trait.` |
|       - |  8825 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  8826 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  8827 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  8828 | ` */` |
|      56 |  8829 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  8830 |  |
|      61 |  8831 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8832 | `	ph7_class *pClass;` |
|       - |  8833 | `	SyToken *pEnd,*pTmp;` |
|       - |  8834 | `	sxi32 iProtection;` |
|       - |  8835 | `	sxi32 iAttrflags;` |
|       - |  8836 | `	SyString *pName;` |
|       - |  8837 | `	sxi32 nKwrd;` |
|       - |  8838 | `	sxi32 rc;` |
|       - |  8839 | `	/* Jump the 'trait' keyword */` |
|      61 |  8840 | `	pGen->pIn++;` |
|      61 |  8841 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8842 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  8843 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8844 | `			return SXERR_ABORT;` |
|       - |  8845 | `		}` |
|     ! 0 |  8846 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  8847 | `			pGen->pIn++;` |
|     ! 0 |  8848 | `		}` |
|     ! 0 |  8849 | `		return SXRET_OK;` |
|       - |  8850 | `	}` |
|       - |  8851 | `	/* Extract trait name */` |
|      61 |  8852 | `	pName = &pGen->pIn->sData;` |
|      61 |  8853 | `	pGen->pIn++;` |
|       - |  8854 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8855 | `		SyBlob sFQN;` |
|       - |  8856 | `		SyString sFQNStr;` |
|      61 |  8857 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      61 |  8858 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      61 |  8859 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      61 |  8860 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      61 |  8861 | `		SyBlobRelease(&sFQN);` |
|       - |  8862 | `	}` |
|      61 |  8863 | `	if( pClass == 0 ){` |
|     ! 0 |  8864 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8865 | `		return SXERR_ABORT;` |
|       - |  8866 | `	}` |
|       - |  8867 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      61 |  8868 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  8869 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  8870 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8871 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8872 | `			return SXERR_ABORT;` |
|       - |  8873 | `		}` |
|     ! 0 |  8874 | `		return SXRET_OK;` |
|       - |  8875 | `	}` |
|      61 |  8876 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      61 |  8877 | `	pEnd = 0;` |
|      61 |  8878 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      61 |  8879 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  8880 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  8881 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8882 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8883 | `			return SXERR_ABORT;` |
|       - |  8884 | `		}` |
|     ! 0 |  8885 | `		return SXRET_OK;` |
|       - |  8886 | `	}` |
|       - |  8887 | `	/* Swap token stream */` |
|      61 |  8888 | `	pTmp = pGen->pEnd;` |
|      61 |  8889 | `	pGen->pEnd = pEnd;` |
|       - |  8890 | `	/* Mark as trait */` |
|      61 |  8891 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  8892 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      56 |  8893 | `	for(;;){` |
|     161 |  8894 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  8895 | `			pGen->pIn++;` |
|       4 |  8896 | `		}` |
|     137 |  8897 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      61 |  8898 | `			break;` |
|       - |  8899 | `		}` |
|      81 |  8900 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8901 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8902 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8903 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8904 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8905 | `				return SXERR_ABORT;` |
|       - |  8906 | `			}` |
|     ! 0 |  8907 | `			goto done;` |
|       - |  8908 | `		}` |
|      81 |  8909 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      81 |  8910 | `		iAttrflags = 0;` |
|      81 |  8911 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      81 |  8912 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      81 |  8913 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8914 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  8915 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  8916 | `				for(;;){` |
|       - |  8917 | `					ph7_class *pUsedTrait;` |
|       - |  8918 | `					SyString *pUsedName;` |
|       5 |  8919 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8920 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8921 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  8922 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8923 | `							return SXERR_ABORT;` |
|       - |  8924 | `						}` |
|     ! 0 |  8925 | `						break;` |
|       - |  8926 | `					}` |
|       5 |  8927 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  8928 | `					{` |
|       - |  8929 | `						SyBlob sResolved;` |
|       5 |  8930 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  8931 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  8932 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  8933 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  8934 | `						SyBlobRelease(&sResolved);` |
|       - |  8935 | `					}` |
|       5 |  8936 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8937 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  8938 | `					}` |
|       5 |  8939 | `					if( pUsedTrait == 0 ){` |
|       4 |  8940 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  8941 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  8942 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8943 | `							return SXERR_ABORT;` |
|       - |  8944 | `						}` |
|       2 |  8945 | `					}else{` |
|       3 |  8946 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  8947 | `					}` |
|       5 |  8948 | `					pGen->pIn++;` |
|       5 |  8949 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  8950 | `						break;` |
|       - |  8951 | `					}` |
|     ! 0 |  8952 | `					pGen->pIn++;` |
|     ! 0 |  8953 | `				}` |
|       5 |  8954 | `				continue;` |
|       - |  8955 | `			}` |
|      77 |  8956 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  8957 | `				iProtection = nKwrd;` |
|      73 |  8958 | `				pGen->pIn++;` |
|      68 |  8959 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  8960 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8961 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8962 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8963 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8964 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8965 | `						return SXERR_ABORT;` |
|       - |  8966 | `					}` |
|     ! 0 |  8967 | `					goto done;` |
|       - |  8968 | `				}` |
|      73 |  8969 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  8970 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  8971 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8972 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8973 | `							return SXERR_ABORT;` |
|       - |  8974 | `						}` |
|     ! 0 |  8975 | `						goto done;` |
|       - |  8976 | `					}` |
|      12 |  8977 | `					continue;` |
|       - |  8978 | `				}` |
|      63 |  8979 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  8980 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8981 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8982 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8983 | `							return SXERR_ABORT;` |
|       - |  8984 | `						}` |
|     ! 0 |  8985 | `						goto done;` |
|       - |  8986 | `					}` |
|       5 |  8987 | `					continue;` |
|       - |  8988 | `				}` |
|      58 |  8989 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  8990 | `			}` |
|      62 |  8991 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  8992 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8993 | `					"Traits cannot have constants");` |
|     ! 0 |  8994 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8995 | `					return SXERR_ABORT;` |
|       - |  8996 | `				}` |
|     ! 0 |  8997 | `				goto done;` |
|     ! 0 |  8998 | `			}else{` |
|      62 |  8999 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9000 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9001 | `					pGen->pIn++;` |
|       5 |  9002 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9003 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9004 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9005 | `							iProtection = nKwrd;` |
|     ! 0 |  9006 | `							pGen->pIn++;` |
|     ! 0 |  9007 | `						}` |
|       1 |  9008 | `					}` |
|       4 |  9009 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9010 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  9011 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9012 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9013 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9014 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9015 | `							return SXERR_ABORT;` |
|       - |  9016 | `						}` |
|     ! 0 |  9017 | `						goto done;` |
|       - |  9018 | `					}` |
|       5 |  9019 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9020 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9021 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9022 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9023 | `								return SXERR_ABORT;` |
|       - |  9024 | `							}` |
|     ! 0 |  9025 | `							goto done;` |
|       - |  9026 | `						}` |
|       3 |  9027 | `						continue;` |
|       - |  9028 | `					}` |
|       3 |  9029 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9030 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9031 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9032 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9033 | `								return SXERR_ABORT;` |
|       - |  9034 | `							}` |
|     ! 0 |  9035 | `							goto done;` |
|       - |  9036 | `						}` |
|     ! 0 |  9037 | `						continue;` |
|       - |  9038 | `					}` |
|       3 |  9039 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      59 |  9040 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 |  9041 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 |  9042 | `					pGen->pIn++;` |
|       6 |  9043 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 |  9044 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  9045 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  9046 | `							iProtection = nKwrd;` |
|       6 |  9047 | `							pGen->pIn++;` |
|       2 |  9048 | `						}` |
|       2 |  9049 | `					}` |
|       6 |  9050 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9051 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9052 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9053 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9054 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9055 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9056 | `							return SXERR_ABORT;` |
|       - |  9057 | `						}` |
|     ! 0 |  9058 | `						goto done;` |
|       - |  9059 | `					}` |
|       6 |  9060 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9061 | `				}` |
|      60 |  9062 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9063 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9064 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9065 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9066 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9067 | `						return SXERR_ABORT;` |
|       - |  9068 | `					}` |
|     ! 0 |  9069 | `					goto done;` |
|       - |  9070 | `				}` |
|      60 |  9071 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9072 | `					pGen->pIn++;` |
|     ! 0 |  9073 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9074 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9075 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9076 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9077 | `							return SXERR_ABORT;` |
|       - |  9078 | `						}` |
|     ! 0 |  9079 | `						goto done;` |
|       - |  9080 | `					}` |
|     ! 0 |  9081 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9082 | `				}else{` |
|      60 |  9083 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9084 | `				}` |
|      60 |  9085 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9086 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9087 | `						return SXERR_ABORT;` |
|       - |  9088 | `					}` |
|     ! 0 |  9089 | `					goto done;` |
|       - |  9090 | `				}` |
|       - |  9091 | `			}` |
|      32 |  9092 | `		}else{` |
|     ! 0 |  9093 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9094 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9095 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9096 | `					return SXERR_ABORT;` |
|       - |  9097 | `				}` |
|     ! 0 |  9098 | `				goto done;` |
|       - |  9099 | `			}` |
|       - |  9100 | `		}` |
|       4 |  9101 | `	}` |
|       - |  9102 | `	/* Install the trait */` |
|      61 |  9103 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      61 |  9104 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9105 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9106 | `		return SXERR_ABORT;` |
|       - |  9107 | `	}` |
|      28 |  9108 | `done:` |
|       - |  9109 | `	/* Point beyond the trait body */` |
|      61 |  9110 | `	pGen->pIn = &pEnd[1];` |
|      61 |  9111 | `	pGen->pEnd = pTmp;` |
|      61 |  9112 | `	return PH7_OK;` |
|      33 |  9113 |  |
|       - |  9114 | `/*` |
|       - |  9115 | ` * Compile a user-defined class.` |
|       - |  9116 | ` *  According to the PHP language reference manual` |
|       - |  9117 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9118 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9119 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9120 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9121 | ` *   and functions (called "methods").` |
|       - |  9122 | ` */` |
|   85924 |  9123 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 |  9124 |  |
|       - |  9125 | `	sxi32 rc;` |
|   85929 |  9126 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   85929 |  9127 | `	return rc;` |
|       5 |  9128 |  |
|       - |  9129 | `/*` |
|       - |  9130 | ` * Exception handling.` |
|       - |  9131 | ` *  According to the PHP language reference manual` |
|       - |  9132 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9133 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9134 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9135 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9136 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9137 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9138 | ` *    (or re-thrown) within a catch block.` |
|       - |  9139 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9140 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9141 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9142 | ` *    been defined with set_exception_handler().` |
|       - |  9143 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9144 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9145 | ` */` |
|       - |  9146 | `/*` |
|       - |  9147 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9148 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9149 | ` * indicates failure.` |
|       - |  9150 | ` */` |
|    9622 |  9151 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  9152 |  |
|    9627 |  9153 | `	sxi32 rc = SXRET_OK;` |
|    9627 |  9154 | `	if( pRoot->pOp ){` |
|    9619 |  9155 | `		switch( pRoot->pOp->iOp ){` |
|    4807 |  9156 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9157 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9158 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9159 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9160 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9161 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    9619 |  9162 | `			break;` |
|     ! 0 |  9163 | `		default:` |
|       - |  9164 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9165 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9166 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9167 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9168 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9169 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9170 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9171 | `			}` |
|     ! 0 |  9172 | `			break;` |
|       - |  9173 | `		}` |
|    4820 |  9174 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9175 | `		/* Unexpected expression */` |
|     ! 0 |  9176 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9177 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9178 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9179 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9180 | `		}` |
|     ! 0 |  9181 | `	}` |
|    9627 |  9182 | `	return rc;` |
|       5 |  9183 |  |
|       - |  9184 | `/*` |
|       - |  9185 | ` * Compile a 'throw' statement.` |
|       - |  9186 | ` * throw: This is how you trigger an exception.` |
|       - |  9187 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9188 | ` */` |
|    9586 |  9189 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 |  9190 |  |
|    9591 |  9191 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9192 | `	GenBlock *pBlock;` |
|       - |  9193 | `	sxu32 nIdx;` |
|       - |  9194 | `	sxi32 rc;` |
|    9591 |  9195 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9196 | `	/* Compile the expression */` |
|    9591 |  9197 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    9591 |  9198 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9199 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9200 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9201 | `			return SXERR_ABORT;` |
|       - |  9202 | `		}` |
|     ! 0 |  9203 | `		return SXRET_OK;` |
|       - |  9204 | `	}` |
|    9591 |  9205 | `	pBlock = pGen->pCurrent;` |
|       - |  9206 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   44361 |  9207 | `	while(pBlock->pParent){` |
|   44357 |  9208 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    9587 |  9209 | `			break;` |
|       - |  9210 | `		}` |
|       - |  9211 | `		/* Point to the parent block */` |
|   34775 |  9212 | `		pBlock = pBlock->pParent;` |
|       5 |  9213 | `	}` |
|       - |  9214 | `	/* Emit the throw instruction */` |
|    9591 |  9215 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9216 | `	/* Emit the jump */` |
|    9591 |  9217 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    9591 |  9218 | `	return SXRET_OK;` |
|    4798 |  9219 |  |
|       - |  9220 | `/*` |
|       - |  9221 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9222 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9223 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9224 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9225 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9226 | ` */` |
|      36 |  9227 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9228 |  |
|      38 |  9229 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9230 | `	GenBlock *pBlock;` |
|       - |  9231 | `	sxu32 nIdx;` |
|       - |  9232 | `	sxi32 rc;` |
|      18 |  9233 | `	(void)iCompileFlag;` |
|      38 |  9234 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9235 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9236 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9237 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9238 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9239 | `			return SXERR_ABORT;` |
|       - |  9240 | `		}` |
|     ! 0 |  9241 | `		return SXRET_OK;` |
|       - |  9242 | `	}` |
|      38 |  9243 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9244 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9245 | `		return SXERR_ABORT;` |
|       - |  9246 | `	}` |
|      38 |  9247 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9248 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9249 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9250 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9251 | `			return SXERR_ABORT;` |
|       - |  9252 | `		}` |
|     ! 0 |  9253 | `		return SXRET_OK;` |
|       - |  9254 | `	}` |
|       - |  9255 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  9256 | `	pBlock = pGen->pCurrent;` |
|      60 |  9257 | `	while( pBlock->pParent ){` |
|      49 |  9258 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  9259 | `			break;` |
|       - |  9260 | `		}` |
|      23 |  9261 | `		pBlock = pBlock->pParent;` |
|       1 |  9262 | `	}` |
|      38 |  9263 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 |  9264 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 |  9265 | `	return SXRET_OK;` |
|      20 |  9266 |  |
|       - |  9267 | `/*` |
|       - |  9268 | ` * Compile a 'catch' block.` |
|       - |  9269 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  9270 | ` * an object containing the exception information.` |
|       - |  9271 | ` */` |
|     378 |  9272 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 |  9273 |  |
|     383 |  9274 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9275 | `	ph7_exception_block sCatch;` |
|       - |  9276 | `	SySet *pInstrContainer;` |
|       - |  9277 | `	SyString sClassName;` |
|       - |  9278 | `	GenBlock *pCatch;` |
|       - |  9279 | `	SyToken *pToken;` |
|       - |  9280 | `	SyString *pName;` |
|       - |  9281 | `	char *zDup;` |
|       - |  9282 | `	sxi32 rc;` |
|     383 |  9283 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  9284 | `	/* Zero the structure */` |
|     383 |  9285 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  9286 | `	/* Initialize fields */` |
|     383 |  9287 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     383 |  9288 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     383 |  9289 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  9290 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9291 | `			pToken = pGen->pIn;` |
|     ! 0 |  9292 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9293 | `				pToken--;` |
|     ! 0 |  9294 | `			}` |
|     ! 0 |  9295 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9296 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9297 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9298 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9299 | `				return SXERR_ABORT;` |
|       - |  9300 | `			}` |
|     ! 0 |  9301 | `			return SXERR_INVALID;` |
|       - |  9302 | `	}` |
|       - |  9303 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     383 |  9304 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     202 |  9305 | `	for(;;){` |
|       - |  9306 | `		SyBlob sResolved;` |
|     409 |  9307 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     409 |  9308 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 |  9309 | `			SyBlobRelease(&sResolved);` |
|       6 |  9310 | `			pToken = pGen->pIn;` |
|       6 |  9311 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9312 | `				pToken--;` |
|     ! 0 |  9313 | `			}` |
|       8 |  9314 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9315 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  9316 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 |  9317 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9318 | `				return SXERR_ABORT;` |
|       - |  9319 | `			}` |
|       6 |  9320 | `			return SXERR_INVALID;` |
|       - |  9321 | `		}` |
|       - |  9322 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - |  9323 | `		 * transient SyBlob allocation. */` |
|     605 |  9324 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     400 |  9325 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     405 |  9326 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     405 |  9327 | `		SyBlobRelease(&sResolved);` |
|     405 |  9328 | `		if( zDup == 0 ){` |
|     ! 0 |  9329 | `			goto Mem;` |
|       - |  9330 | `		}` |
|     405 |  9331 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     405 |  9332 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9333 | `			goto Mem;` |
|       - |  9334 | `		}` |
|       - |  9335 | `		/* Check for '\|' (multi-catch separator) */` |
|     413 |  9336 | `		if( pGen->pIn < pGen->pEnd &&` |
|     400 |  9337 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      31 |  9338 | `			pGen->pIn->sData.nByte == 1 &&` |
|      26 |  9339 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      28 |  9340 | `			pGen->pIn++; /* Consume the '\|' */` |
|      28 |  9341 | `			continue;` |
|       - |  9342 | `		}` |
|     379 |  9343 | `		break;` |
|     ! 0 |  9344 | `	}` |
|     561 |  9345 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     379 |  9346 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  9347 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9348 | `			pToken = pGen->pIn;` |
|     ! 0 |  9349 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9350 | `				pToken--;` |
|     ! 0 |  9351 | `			}` |
|     ! 0 |  9352 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9353 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9354 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9355 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9356 | `				return SXERR_ABORT;` |
|       - |  9357 | `			}` |
|     ! 0 |  9358 | `			return SXERR_INVALID;` |
|       - |  9359 | `	}` |
|     379 |  9360 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9361 | `	/* Duplicate instance name */` |
|     379 |  9362 | `	pName = &pGen->pIn->sData;` |
|     379 |  9363 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     379 |  9364 | `	if( zDup == 0 ){` |
|     ! 0 |  9365 | `		goto Mem;` |
|       - |  9366 | `	}` |
|     379 |  9367 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     379 |  9368 | `	pGen->pIn++;` |
|     379 |  9369 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9370 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9371 | `		pToken = pGen->pIn;` |
|     ! 0 |  9372 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9373 | `			pToken--;` |
|     ! 0 |  9374 | `		}` |
|     ! 0 |  9375 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9376 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9377 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9378 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9379 | `			return SXERR_ABORT;` |
|       - |  9380 | `		}` |
|     ! 0 |  9381 | `		return SXERR_INVALID;` |
|       - |  9382 | `	}` |
|       - |  9383 | `	/* Compile the block */` |
|     379 |  9384 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9385 | `	/* Create the catch block */` |
|     379 |  9386 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     379 |  9387 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9388 | `		return SXERR_ABORT;` |
|       - |  9389 | `	}` |
|       - |  9390 | `	/* Swap bytecode container */` |
|     379 |  9391 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     379 |  9392 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9393 | `	/* Compile the block */` |
|     379 |  9394 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9395 | `	/* Fix forward jumps now the destination is resolved  */` |
|     379 |  9396 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9397 | `	/* Emit the DONE instruction */` |
|     379 |  9398 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9399 | `	/* Leave the block */` |
|     379 |  9400 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9401 | `	/* Restore the default container */` |
|     379 |  9402 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9403 | `	/* Install the catch block */` |
|     379 |  9404 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     379 |  9405 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9406 | `		goto Mem;` |
|       - |  9407 | `	}` |
|     379 |  9408 | `	return SXRET_OK;` |
|     ! 0 |  9409 | `Mem:` |
|     ! 0 |  9410 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9411 | `	return SXERR_ABORT;` |
|     194 |  9412 |  |
|       - |  9413 | `/*` |
|       - |  9414 | ` * Compile a 'try' block.` |
|       - |  9415 | ` * A function using an exception should be in a "try" block.` |
|       - |  9416 | ` * If the exception does not trigger, the code will continue` |
|       - |  9417 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9418 | ` * is "thrown".` |
|       - |  9419 | ` */` |
|     382 |  9420 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 |  9421 |  |
|       - |  9422 | `	ph7_exception *pException;` |
|     387 |  9423 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9424 | `	GenBlock *pTry;` |
|       - |  9425 | `	sxu32 nJmpIdx;` |
|       - |  9426 | `	sxi32 rc;` |
|       - |  9427 | `	/* Create the exception container */` |
|     387 |  9428 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     387 |  9429 | `	if( pException == 0 ){` |
|     ! 0 |  9430 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9431 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9432 | `		return SXERR_ABORT;` |
|       - |  9433 | `	}` |
|       - |  9434 | `	/* Zero the structure */` |
|     387 |  9435 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9436 | `	/* Initialize fields */` |
|     387 |  9437 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     387 |  9438 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     387 |  9439 | `	pException->iHasFinally = 0;` |
|     387 |  9440 | `	pException->iFinallyDone = 0;` |
|     387 |  9441 | `	pException->pVm = pGen->pVm;` |
|       - |  9442 | `	/* Create the try block */` |
|     387 |  9443 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     387 |  9444 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9445 | `		return SXERR_ABORT;` |
|       - |  9446 | `	}` |
|       - |  9447 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     387 |  9448 | `	pTry->pUserData = pException;` |
|       - |  9449 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     387 |  9450 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9451 | `	/* Fix the jump later when the destination is resolved */` |
|     387 |  9452 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     387 |  9453 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9454 | `	/* Compile the block */` |
|     387 |  9455 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     387 |  9456 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9457 | `		return SXERR_ABORT;` |
|       - |  9458 | `	}` |
|       - |  9459 | `	/* Fix forward jumps now the destination is resolved */` |
|     387 |  9460 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9461 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     387 |  9462 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  9463 | `	/* Leave the block */` |
|     387 |  9464 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9465 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     387 |  9466 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     380 |  9467 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  9468 | `		/* Compile one or more catch blocks */` |
|     374 |  9469 | `		for(;;){` |
|     748 |  9470 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     582 |  9471 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     190 |  9472 | `					break;` |
|       - |  9473 | `			}` |
|     383 |  9474 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     383 |  9475 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9476 | `				return SXERR_ABORT;` |
|       - |  9477 | `			}` |
|       5 |  9478 | `		}` |
|     185 |  9479 | `	}` |
|       - |  9480 | `	/* Compile optional finally block */` |
|     387 |  9481 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     182 |  9482 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  9483 | `		SySet *pInstrContainer;` |
|       - |  9484 | `		GenBlock *pFinBlock;` |
|      34 |  9485 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  9486 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      34 |  9487 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      34 |  9488 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9489 | `			return SXERR_ABORT;` |
|       - |  9490 | `		}` |
|       - |  9491 | `		/* Swap bytecode container */` |
|      34 |  9492 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      34 |  9493 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  9494 | `		/* Compile the finally body */` |
|      34 |  9495 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      34 |  9496 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9497 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  9498 | `			return SXERR_ABORT;` |
|       - |  9499 | `		}` |
|       - |  9500 | `		/* Fix forward jumps now the destination is resolved */` |
|      34 |  9501 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9502 | `		/* Emit DONE to terminate the finally block */` |
|      34 |  9503 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9504 | `		/* Leave the block */` |
|      34 |  9505 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9506 | `		/* Restore the default container */` |
|      34 |  9507 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      34 |  9508 | `		pException->iHasFinally = 1;` |
|      15 |  9509 | `	}` |
|       - |  9510 | `	/* Must have at least one catch or finally */` |
|     387 |  9511 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 |  9512 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9513 | `			"Cannot use try without catch or finally");` |
|       8 |  9514 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9515 | `			return SXERR_ABORT;` |
|       - |  9516 | `		}` |
|       3 |  9517 | `	}` |
|     387 |  9518 | `	return SXRET_OK;` |
|     196 |  9519 |  |
|       - |  9520 | `/*` |
|       - |  9521 | ` * Compile a switch block.` |
|       - |  9522 | ` *  (See block-comment below for more information)` |
|       - |  9523 | ` */` |
|     112 |  9524 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 |  9525 |  |
|     117 |  9526 | `	sxi32 rc = SXRET_OK;` |
|     117 |  9527 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  9528 | `		/* Unexpected token */` |
|     ! 0 |  9529 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9530 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9531 | `			return SXERR_ABORT;` |
|       - |  9532 | `		}` |
|     ! 0 |  9533 | `		pGen->pIn++;` |
|     ! 0 |  9534 | `	}` |
|     117 |  9535 | `	pGen->pIn++;` |
|       - |  9536 | `	/* First instruction to execute in this block. */` |
|     117 |  9537 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  9538 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  9539 | `	 * or the '}' token */` |
|     206 |  9540 | `	for(;;){` |
|     417 |  9541 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9542 | `			/* No more input to process */` |
|     ! 0 |  9543 | `			break;` |
|       - |  9544 | `		}` |
|     417 |  9545 | `		rc = SXRET_OK;` |
|     417 |  9546 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 |  9547 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 |  9548 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  9549 | `					/* Unexpected token */` |
|     ! 0 |  9550 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9551 | `						&pGen->pIn->sData);` |
|     ! 0 |  9552 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9553 | `						return SXERR_ABORT;` |
|       - |  9554 | `					}` |
|       - |  9555 | `					/* FALL THROUGH */` |
|     ! 0 |  9556 | `				}` |
|      31 |  9557 | `				rc = SXERR_EOF;` |
|      31 |  9558 | `				break;` |
|       - |  9559 | `			}` |
|      32 |  9560 | `		}else{` |
|       - |  9561 | `			sxi32 nKwrd;` |
|       - |  9562 | `			/* Extract the keyword */` |
|     337 |  9563 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 |  9564 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 |  9565 | `				break;` |
|       - |  9566 | `			}` |
|     253 |  9567 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9568 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  9569 | `					/* Unexpected token */` |
|     ! 0 |  9570 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9571 | `						&pGen->pIn->sData);` |
|     ! 0 |  9572 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9573 | `						return SXERR_ABORT;` |
|       - |  9574 | `					}` |
|       - |  9575 | `					/* FALL THROUGH */` |
|     ! 0 |  9576 | `				}` |
|       - |  9577 | `				/* Block compiled */` |
|       3 |  9578 | `				break;` |
|       - |  9579 | `			}` |
|       - |  9580 | `		}` |
|       - |  9581 | `		/* Compile block */` |
|     305 |  9582 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 |  9583 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9584 | `			return SXERR_ABORT;` |
|       - |  9585 | `		}` |
|       5 |  9586 | `	}` |
|     117 |  9587 | `	return rc;` |
|      61 |  9588 |  |
|       - |  9589 | `/*` |
|       - |  9590 | ` * Compile a case eXpression.` |
|       - |  9591 | ` *  (See block-comment below for more information)` |
|       - |  9592 | ` */` |
|      92 |  9593 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 |  9594 |  |
|       - |  9595 | `	SySet *pInstrContainer;` |
|       - |  9596 | `	SyToken *pEnd,*pTmp;` |
|      97 |  9597 | `	sxi32 iNest = 0;` |
|       - |  9598 | `	sxi32 rc;` |
|       - |  9599 | `	/* Delimit the expression */` |
|      97 |  9600 | `	pEnd = pGen->pIn;` |
|     197 |  9601 | `	while( pEnd < pGen->pEnd ){` |
|     197 |  9602 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  9603 | `			/* Increment nesting level */` |
|       3 |  9604 | `			iNest++;` |
|     196 |  9605 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  9606 | `			/* Decrement nesting level */` |
|       3 |  9607 | `			iNest--;` |
|     194 |  9608 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 |  9609 | `			break;` |
|       - |  9610 | `		}` |
|     105 |  9611 | `		pEnd++;` |
|       5 |  9612 | `	}` |
|      97 |  9613 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  9614 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  9615 | `		if( rc == SXERR_ABORT ){` |
|       - |  9616 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9617 | `			return SXERR_ABORT;` |
|       - |  9618 | `		}` |
|     ! 0 |  9619 | `	}` |
|       - |  9620 | `	/* Swap token stream */` |
|      97 |  9621 | `	pTmp = pGen->pEnd;` |
|      97 |  9622 | `	pGen->pEnd = pEnd;` |
|      97 |  9623 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 |  9624 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 |  9625 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  9626 | `	/* Emit the done instruction */` |
|      97 |  9627 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 |  9628 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9629 | `	/* Update token stream */` |
|      97 |  9630 | `	pGen->pIn  = pEnd;` |
|      97 |  9631 | `	pGen->pEnd = pTmp;` |
|      97 |  9632 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9633 | `		return SXERR_ABORT;` |
|       - |  9634 | `	}` |
|      97 |  9635 | `	return SXRET_OK;` |
|      51 |  9636 |  |
|       - |  9637 | `/*` |
|       - |  9638 | ` * Compile the smart switch statement.` |
|       - |  9639 | ` * According to the PHP language reference manual` |
|       - |  9640 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  9641 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  9642 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  9643 | ` *  This is exactly what the switch statement is for.` |
|       - |  9644 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  9645 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  9646 | ` *  of the outer loop, use continue 2.` |
|       - |  9647 | ` *  Note that switch/case does loose comparision.` |
|       - |  9648 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  9649 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  9650 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  9651 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  9652 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  9653 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  9654 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  9655 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  9656 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  9657 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  9658 | ` *  list for the next case.` |
|       - |  9659 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  9660 | ` *  or floating-point numbers and strings.` |
|       - |  9661 | ` */` |
|      28 |  9662 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 |  9663 |  |
|       - |  9664 | `	GenBlock *pSwitchBlock;` |
|       - |  9665 | `	SyToken *pTmp,*pEnd;` |
|       - |  9666 | `	ph7_switch *pSwitch;` |
|       - |  9667 | `	sxu32 nToken;` |
|       - |  9668 | `	sxu32 nLine;` |
|       - |  9669 | `	sxi32 rc;` |
|      33 |  9670 | `	nLine = pGen->pIn->nLine;` |
|       - |  9671 | `	/* Jump the 'switch' keyword */` |
|      33 |  9672 | `	pGen->pIn++;` |
|      33 |  9673 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  9674 | `		/* Syntax error */` |
|     ! 0 |  9675 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  9676 | `		if( rc == SXERR_ABORT ){` |
|       - |  9677 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9678 | `			return SXERR_ABORT;` |
|       - |  9679 | `		}` |
|     ! 0 |  9680 | `		goto Synchronize;` |
|       - |  9681 | `	}` |
|       - |  9682 | `	/* Jump the left parenthesis '(' */` |
|      33 |  9683 | `	pGen->pIn++;` |
|      33 |  9684 | `	pEnd = 0; /* cc warning */` |
|       - |  9685 | `	/* Create the loop block */` |
|      47 |  9686 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  9687 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 |  9688 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9689 | `		return SXERR_ABORT;` |
|       - |  9690 | `	}` |
|       - |  9691 | `	/* Delimit the condition */` |
|      33 |  9692 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 |  9693 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  9694 | `		/* Empty expression */` |
|     ! 0 |  9695 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  9696 | `		if( rc == SXERR_ABORT ){` |
|       - |  9697 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9698 | `			return SXERR_ABORT;` |
|       - |  9699 | `		}` |
|     ! 0 |  9700 | `	}` |
|       - |  9701 | `	/* Swap token streams */` |
|      33 |  9702 | `	pTmp = pGen->pEnd;` |
|      33 |  9703 | `	pGen->pEnd = pEnd;` |
|       - |  9704 | `	/* Compile the expression */` |
|      33 |  9705 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 |  9706 | `	if( rc == SXERR_ABORT ){` |
|       - |  9707 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  9708 | `		return SXERR_ABORT;` |
|       - |  9709 | `	}` |
|       - |  9710 | `	/* Update token stream */` |
|      33 |  9711 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  9712 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9713 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9714 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9715 | `			return SXERR_ABORT;` |
|       - |  9716 | `		}` |
|     ! 0 |  9717 | `		pGen->pIn++;` |
|     ! 0 |  9718 | `	}` |
|      33 |  9719 | `	pGen->pIn  = &pEnd[1];` |
|      33 |  9720 | `	pGen->pEnd = pTmp;` |
|      33 |  9721 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 |  9722 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 |  9723 | `			pTmp = pGen->pIn;` |
|     ! 0 |  9724 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 |  9725 | `				pTmp--;` |
|     ! 0 |  9726 | `			}` |
|       - |  9727 | `			/* Unexpected token */` |
|     ! 0 |  9728 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 |  9729 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9730 | `				return SXERR_ABORT;` |
|       - |  9731 | `			}` |
|     ! 0 |  9732 | `			goto Synchronize;` |
|       - |  9733 | `	}` |
|       - |  9734 | `	/* Set the delimiter token */` |
|      33 |  9735 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 |  9736 | `		nToken = PH7_TK_KEYWORD;` |
|       - |  9737 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 |  9738 | `	}else{` |
|      31 |  9739 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - |  9740 | `	}` |
|      33 |  9741 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - |  9742 | `	/* Create the switch blocks container */` |
|      33 |  9743 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 |  9744 | `	if( pSwitch == 0 ){` |
|       - |  9745 | `		/* Abort compilation */` |
|     ! 0 |  9746 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9747 | `		return SXERR_ABORT;` |
|       - |  9748 | `	}` |
|       - |  9749 | `	/* Zero the structure */` |
|      33 |  9750 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - |  9751 | `	/* Initialize fields */` |
|      33 |  9752 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - |  9753 | `	/* Emit the switch instruction */` |
|      33 |  9754 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - |  9755 | `	/* Compile case blocks */` |
|     100 |  9756 | `	for(;;){` |
|       - |  9757 | `		sxu32 nKwrd;` |
|     119 |  9758 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9759 | `			/* No more input to process */` |
|     ! 0 |  9760 | `			break;` |
|       - |  9761 | `		}` |
|     119 |  9762 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9763 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - |  9764 | `				/* Unexpected token */` |
|     ! 0 |  9765 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9766 | `					&pGen->pIn->sData);` |
|     ! 0 |  9767 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9768 | `					return SXERR_ABORT;` |
|       - |  9769 | `				}` |
|       - |  9770 | `				/* FALL THROUGH */` |
|     ! 0 |  9771 | `			}` |
|       - |  9772 | `			/* Block compiled */` |
|     ! 0 |  9773 | `			break;` |
|       - |  9774 | `		}` |
|       - |  9775 | `		/* Extract the keyword */` |
|     119 |  9776 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 |  9777 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9778 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - |  9779 | `				/* Unexpected token */` |
|     ! 0 |  9780 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9781 | `					&pGen->pIn->sData);` |
|     ! 0 |  9782 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9783 | `					return SXERR_ABORT;` |
|       - |  9784 | `				}` |
|       - |  9785 | `				/* FALL THROUGH */` |
|     ! 0 |  9786 | `			}` |
|       - |  9787 | `			/* Block compiled */` |
|       3 |  9788 | `			break;` |
|       - |  9789 | `		}` |
|     117 |  9790 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - |  9791 | `			/*` |
|       - |  9792 | `			 * Accroding to the PHP language reference manual` |
|       - |  9793 | `			 *  A special case is the default case. This case matches anything` |
|       - |  9794 | `			 *  that wasn't matched by the other cases.` |
|       - |  9795 | `			 */` |
|      25 |  9796 | `			if( pSwitch->nDefault > 0 ){` |
|       - |  9797 | `				/* Default case already compiled */` |
|     ! 0 |  9798 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 |  9799 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9800 | `					return SXERR_ABORT;` |
|       - |  9801 | `				}` |
|     ! 0 |  9802 | `			}` |
|      25 |  9803 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - |  9804 | `			/* Compile the default block */` |
|      25 |  9805 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 |  9806 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9807 | `				return SXERR_ABORT;` |
|      25 |  9808 | `			}else if( rc == SXERR_EOF ){` |
|      23 |  9809 | `				break;` |
|       1 |  9810 | `			}` |
|      98 |  9811 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - |  9812 | `			ph7_case_expr sCase;` |
|       - |  9813 | `			/* Standard case block */` |
|      97 |  9814 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - |  9815 | `			/* initialize the structure */` |
|      97 |  9816 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  9817 | `			/* Compile the case expression */` |
|      97 |  9818 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 |  9819 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9820 | `				return SXERR_ABORT;` |
|       - |  9821 | `			}` |
|       - |  9822 | `			/* Compile the case block */` |
|      97 |  9823 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - |  9824 | `			/* Insert in the switch container */` |
|      97 |  9825 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 |  9826 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9827 | `				return SXERR_ABORT;` |
|      97 |  9828 | `			}else if( rc == SXERR_EOF ){` |
|       9 |  9829 | `				break;` |
|       - |  9830 | `			}` |
|      47 |  9831 | `		}else{` |
|       - |  9832 | `			/* Unexpected token */` |
|     ! 0 |  9833 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9834 | `				&pGen->pIn->sData);` |
|     ! 0 |  9835 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9836 | `				return SXERR_ABORT;` |
|       - |  9837 | `			}` |
|     ! 0 |  9838 | `			break;` |
|       - |  9839 | `		}` |
|       5 |  9840 | `	}` |
|       - |  9841 | `	/* Fix all jumps now the destination is resolved */` |
|      33 |  9842 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 |  9843 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9844 | `	/* Release the loop block */` |
|      33 |  9845 | `	GenStateLeaveBlock(pGen,0);` |
|      33 |  9846 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - |  9847 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 |  9848 | `		pGen->pIn++;` |
|      14 |  9849 | `	}` |
|       - |  9850 | `	/* Statement successfully compiled */` |
|      33 |  9851 | `	return SXRET_OK;` |
|     ! 0 |  9852 | `Synchronize:` |
|       - |  9853 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  9854 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 |  9855 | `		pGen->pIn++;` |
|     ! 0 |  9856 | `	}` |
|     ! 0 |  9857 | `	return SXRET_OK;` |
|      19 |  9858 |  |
|       - |  9859 | `/*` |
|       - |  9860 | ` * Chain operators participate in a postfix member-access chain.` |
|       - |  9861 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - |  9862 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - |  9863 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - |  9864 | ` */` |
|       - |  9865 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - |  9866 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - |  9867 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - |  9868 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - |  9869 |  |
|       - |  9870 | `/*` |
|       - |  9871 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - |  9872 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - |  9873 | ` * patched entries from the pending set.` |
|       - |  9874 | ` */` |
| 2302822 |  9875 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 |  9876 |  |
| 2302827 |  9877 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - |  9878 | `	sxu32 nTarget;` |
|       - |  9879 | `	sxu32 *aIdx;` |
|       - |  9880 | `	sxu32 i;` |
| 2302827 |  9881 | `	if( nCur <= nBaseline ){` |
| 2302737 |  9882 | `		return;` |
|       - |  9883 | `	}` |
|      93 |  9884 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      93 |  9885 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     191 |  9886 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     101 |  9887 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     101 |  9888 | `		if( pInstr ){` |
|     101 |  9889 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 |  9890 | `		}` |
|      52 |  9891 | `	}` |
|      93 |  9892 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1151416 |  9893 |  |
|       - |  9894 |  |
|       - |  9895 | `/*` |
|       - |  9896 | ` * By-reference out-parameters of builtin functions.` |
|       - |  9897 | ` *` |
|       - |  9898 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - |  9899 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - |  9900 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - |  9901 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - |  9902 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - |  9903 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - |  9904 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - |  9905 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - |  9906 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - |  9907 | ` * creates it" behaviour).` |
|       - |  9908 | ` *` |
|       - |  9909 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - |  9910 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - |  9911 | ` */` |
|  373934 |  9912 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 |  9913 |  |
|       - |  9914 | `	static const struct {` |
|       - |  9915 | `		const char *zName;` |
|       - |  9916 | `		sxu32 nByte;` |
|       - |  9917 | `		sxu32 mask;` |
|       - |  9918 | `	} aByRef[] = {` |
|       - |  9919 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - |  9920 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - |  9921 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - |  9922 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - |  9923 | `	};` |
|       - |  9924 | `	sxu32 i;` |
|  373939 |  9925 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1153 |  9926 | `		return 0;` |
|       - |  9927 | `	}` |
| 1863791 |  9928 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1491046 |  9929 | `		if( pName->nByte == aByRef[i].nByte` |
|  765187 |  9930 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      51 |  9931 | `			return aByRef[i].mask;` |
|       - |  9932 | `		}` |
|  745505 |  9933 | `	}` |
|  372745 |  9934 | `	return 0;` |
|  186972 |  9935 |  |
|       - |  9936 | `/*` |
|       - |  9937 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - |  9938 | ` *` |
|       - |  9939 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - |  9940 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - |  9941 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - |  9942 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - |  9943 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - |  9944 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - |  9945 | ` */` |
|  373934 |  9946 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 |  9947 |  |
|       - |  9948 | `	SyToken *p, *pEnd;` |
|  373939 |  9949 | `	pOut->zString = 0;` |
|  373939 |  9950 | `	pOut->nByte = 0;` |
|  373939 |  9951 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 |  9952 | `		return;` |
|       - |  9953 | `	}` |
|  373939 |  9954 | `	p = pLeft->pStart;` |
|  373939 |  9955 | `	pEnd = pLeft->pEnd;` |
|       - |  9956 | `	/* Optional single leading namespace separator (absolute path). */` |
|  373939 |  9957 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      24 |  9958 | `		p++;` |
|      10 |  9959 | `	}` |
|  373939 |  9960 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1127 |  9961 | `		return;` |
|       - |  9962 | `	}` |
|       - |  9963 | `	/* Must be a single component: nothing follows the name token. */` |
|  372817 |  9964 | `	if( p + 1 != pEnd ){` |
|      30 |  9965 | `		return;` |
|       - |  9966 | `	}` |
|  372791 |  9967 | `	*pOut = p->sData;` |
|  186972 |  9968 |  |
|       - |  9969 | `/*` |
|       - |  9970 | ` * Generate bytecode for a given expression tree.` |
|       - |  9971 | ` * If something goes wrong while generating bytecode` |
|       - |  9972 | ` * for the expression tree (A very unlikely scenario)` |
|       - |  9973 | ` * this function takes care of generating the appropriate` |
|       - |  9974 | ` * error message.` |
|       - |  9975 | ` */` |
| 3102344 |  9976 | `static sxi32 GenStateEmitExprCode(` |
|       - |  9977 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  9978 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - |  9979 | `	sxi32 iFlags /* Control flags */` |
|       - |  9980 | `	)` |
|       5 |  9981 |  |
|       - |  9982 | `	VmInstr *pInstr;` |
|       - |  9983 | `	sxu32 nJmpIdx;` |
| 3102349 |  9984 | `	sxi32 iP1 = 0;` |
| 3102349 |  9985 | `	sxu32 iP2 = 0;` |
| 3102349 |  9986 | `	void *p3  = 0;` |
|       - |  9987 | `	sxi32 iVmOp;` |
|       - |  9988 | `	sxi32 rc;` |
| 3102349 |  9989 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3102349 |  9990 | `	sxu32 nRhsNsBase = 0;` |
| 3102349 |  9991 | `	if( pNode->xCode ){` |
|       - |  9992 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - |  9993 | `		/* Compile node */` |
| 1921805 |  9994 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1921805 |  9995 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1921805 |  9996 | `		RE_SWAP_DELIMITER(pGen);` |
| 1921805 |  9997 | `		return rc;` |
|       - |  9998 | `	}` |
| 1180549 |  9999 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10000 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10001 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10002 | `		return SXERR_ABORT;` |
|       - | 10003 | `	}` |
| 1180549 | 10004 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1180549 | 10005 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      59 | 10006 | `		sxu32 nJmp = 0;` |
|       - | 10007 | `		sxu32 nNcNsBase;` |
|       - | 10008 | `		VmInstr *pInstrFix;` |
|       - | 10009 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10010 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10011 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10012 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10013 | `		 * stack slot carries a writable nIdx. */` |
|      59 | 10014 | `		if( pNode->pRight ){` |
|      59 | 10015 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10016 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      59 | 10017 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10018 | `				return rc;` |
|       - | 10019 | `			}` |
|      59 | 10020 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10021 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10022 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10023 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10024 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10025 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10026 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10027 | `			 * cascade for the actual write path stays correct. */` |
|      59 | 10028 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      59 | 10029 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      29 | 10030 | `				pInstrFix->iP2 = 3;` |
|      13 | 10031 | `			}` |
|      28 | 10032 | `		}` |
|       - | 10033 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      59 | 10034 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10035 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      59 | 10036 | `		if( pNode->pLeft ){` |
|      59 | 10037 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10038 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      59 | 10039 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10040 | `				return rc;` |
|       - | 10041 | `			}` |
|      59 | 10042 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      28 | 10043 | `		}` |
|       - | 10044 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      59 | 10045 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10046 | `		/* Patch the short-circuit jump to land after the store. */` |
|      59 | 10047 | `		if( nJmp > 0 ){` |
|      59 | 10048 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      59 | 10049 | `			if( pInstrFix ){` |
|      59 | 10050 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10051 | `			}` |
|      28 | 10052 | `		}` |
|      59 | 10053 | `		return SXRET_OK;` |
|       - | 10054 | `	}` |
| 1180493 | 10055 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10056 | `		sxu32 nJz,nJmp;` |
|       - | 10057 | `		sxu32 nTernaryNsBase;` |
|       - | 10058 | `		/* Ternary operator require special handling */` |
|       - | 10059 | `		/* Phase#1: Compile the condition */` |
|    2543 | 10060 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2543 | 10061 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2543 | 10062 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10063 | `			return rc;` |
|       - | 10064 | `		}` |
|       - | 10065 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10066 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10067 | `		 * condition expression, not leak past the ternary. */` |
|    2543 | 10068 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2543 | 10069 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2543 | 10070 | `		if( pNode->pLeft ){` |
|       - | 10071 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10072 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2475 | 10073 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10074 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2475 | 10075 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2475 | 10076 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2475 | 10077 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10078 | `				return rc;` |
|       - | 10079 | `			}` |
|    2475 | 10080 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1240 | 10081 | `		}else{` |
|       - | 10082 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10083 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10084 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10085 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10086 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10087 | `		}` |
|       - | 10088 | `		/* Phase#4: Emit the unconditional jump */` |
|    2543 | 10089 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10090 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2543 | 10091 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2543 | 10092 | `		if( pInstr ){` |
|    2543 | 10093 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1269 | 10094 | `		}` |
|    2543 | 10095 | `		if( !pNode->pLeft ){` |
|       - | 10096 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10097 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10098 | `		}` |
|       - | 10099 | `		/* Phase#6: Compile the 'else' expression */` |
|    2543 | 10100 | `		if( pNode->pRight ){` |
|    2543 | 10101 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2543 | 10102 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2543 | 10103 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10104 | `				return rc;` |
|       - | 10105 | `			}` |
|    2543 | 10106 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1269 | 10107 | `		}` |
|    2543 | 10108 | `		if( nJmp > 0 ){` |
|       - | 10109 | `			/* Phase#7: Fix the unconditional jump */` |
|    2543 | 10110 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2543 | 10111 | `			if( pInstr ){` |
|    2543 | 10112 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1269 | 10113 | `			}` |
|    1269 | 10114 | `		}` |
|       - | 10115 | `		/* All done */` |
|    2543 | 10116 | `		return SXRET_OK;` |
|       - | 10117 | `	}` |
| 1177955 | 10118 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10119 | `	/* Generate code for the left tree */` |
| 1177955 | 10120 | `	if( pNode->pLeft ){` |
| 1177917 | 10121 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1177917 | 10122 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10123 | `			ph7_expr_node **apNode;` |
|  374059 | 10124 | `			int hasSpread = 0;` |
|  374059 | 10125 | `			int hasNamed = 0;` |
|  374059 | 10126 | `			int bAnySpread = 0;` |
|  374059 | 10127 | `			sxu32 byRefMask = 0;` |
|       - | 10128 | `			sxi32 nArgs;` |
|       - | 10129 | `			sxi32 n;` |
|       - | 10130 | `			/* Recurse and generate bytecodes for function arguments */` |
|  374059 | 10131 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  374059 | 10132 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10133 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10134 | `			{` |
|  374059 | 10135 | `				int seenNamed = 0;` |
|  740761 | 10136 | `				for( n = 0; n < nArgs; ++n ){` |
|  366709 | 10137 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     188 | 10138 | `						seenNamed = 1;` |
|     188 | 10139 | `						hasNamed = 1;` |
|  366617 | 10140 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      23 | 10141 | `						bAnySpread = 1;` |
|  366515 | 10142 | `					}else if( seenNamed ){` |
|       3 | 10143 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10144 | `							"Cannot use positional argument after named argument");` |
|       3 | 10145 | `						return SXERR_SYNTAX;` |
|       - | 10146 | `					}` |
|  183356 | 10147 | `				}` |
|       - | 10148 | `			}` |
|       - | 10149 | `			/* Read-only load */` |
|  374057 | 10150 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10151 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10152 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10153 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10154 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  374057 | 10155 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  374057 | 10156 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  374052 | 10157 | `				if( pCallName->nByte == 5` |
|  205303 | 10158 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   19189 | 10159 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  364465 | 10160 | `				}else if( pCallName->nByte == 5` |
|  186119 | 10161 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      83 | 10162 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      39 | 10163 | `				}` |
|       - | 10164 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10165 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10166 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10167 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10168 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10169 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  374057 | 10170 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10171 | `					SyString sBuiltin;` |
|  373939 | 10172 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  373939 | 10173 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  186967 | 10174 | `				}` |
|  187026 | 10175 | `			}` |
|  740757 | 10176 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  366705 | 10177 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  366705 | 10178 | `				sxi32 iArgFlags = iFlags & ~EXPR_FLAG_LOAD_IDX_STORE;` |
|       - | 10179 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 10180 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate). */` |
|  366705 | 10181 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      31 | 10182 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      13 | 10183 | `				}` |
|  366705 | 10184 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  366705 | 10185 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10186 | `					return rc;` |
|       - | 10187 | `				}` |
|       - | 10188 | `				/* Each argument is an independent nullsafe scope. */` |
|  366705 | 10189 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  366705 | 10190 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10191 | `					/* Emit spread opcode to unpack this array argument */` |
|      23 | 10192 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      23 | 10193 | `					hasSpread = 1;` |
|      10 | 10194 | `				}` |
|  183355 | 10195 | `			}` |
|       - | 10196 | `			/* Total number of given arguments */` |
|  374057 | 10197 | `			iP1 = nArgs;` |
|  374057 | 10198 | `			iP2 = hasSpread;` |
|       - | 10199 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10200 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  374057 | 10201 | `			if( hasNamed ){` |
|     101 | 10202 | `				sxu32 nStrBytes = 0;` |
|       - | 10203 | `				char *zBuf;` |
|     297 | 10204 | `				for( n = 0; n < nArgs; ++n ){` |
|     199 | 10205 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10206 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      91 | 10207 | `					}` |
|     101 | 10208 | `				}` |
|       - | 10209 | `				{` |
|     101 | 10210 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     101 | 10211 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      98 | 10212 | `					&pGen->pVm->sAllocator, mapSize);` |
|     101 | 10213 | `				if( pMap ){` |
|     101 | 10214 | `					SyZero(pMap, mapSize);` |
|     101 | 10215 | `					pMap->bHasNamed = 1;` |
|     101 | 10216 | `					pMap->nTotal = (sxu32)nArgs;` |
|     101 | 10217 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     101 | 10218 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     297 | 10219 | `					for( n = 0; n < nArgs; ++n ){` |
|     199 | 10220 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10221 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     185 | 10222 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     185 | 10223 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     185 | 10224 | `							zBuf += nb;` |
|      91 | 10225 | `						}` |
|       - | 10226 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     101 | 10227 | `					}` |
|     101 | 10228 | `					p3 = (void *)pMap;` |
|      49 | 10229 | `				}` |
|       - | 10230 | `				}` |
|      49 | 10231 | `			}` |
|       - | 10232 | `			/* Remove stale flags now */` |
|  374057 | 10233 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  187026 | 10234 | `		}` |
| 1177915 | 10235 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1177915 | 10236 | `		if( rc != SXRET_OK ){` |
|      34 | 10237 | `			return rc;` |
|       - | 10238 | `		}` |
| 1177885 | 10239 | `		if( !bIsChainOp ){` |
|       - | 10240 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 10241 | `			 * target the end of that LHS chain, which is right here. */` |
|  550547 | 10242 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  275271 | 10243 | `		}` |
| 1177885 | 10244 | `		if( iVmOp == PH7_OP_CALL ){` |
|  374057 | 10245 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  374057 | 10246 | `			if( pInstr ){` |
|  374057 | 10247 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  372911 | 10248 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 10249 | `					sxu32 nQual;` |
|  372911 | 10250 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10251 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 10252 | `					 * so the later NEW handler (if any) can see it. */` |
|  372911 | 10253 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 10254 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 10255 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 10256 | `					 * imports — class imports must NOT affect function` |
|       - | 10257 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 10258 | `					 * before NEW; we store the original literal index in the` |
|       - | 10259 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 10260 | `					 * the unqualified name and re-qualify with class imports. */` |
|  372911 | 10261 | `					if( bAbsolute ){` |
|      24 | 10262 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      14 | 10263 | `					}else{` |
|  372891 | 10264 | `						int fromImport = 0;` |
|  372891 | 10265 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  372891 | 10266 | `						pInstr->iP2 = (sxi32)nQual;` |
|  372891 | 10267 | `						if( nQual != nOrig ){` |
|       - | 10268 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 10269 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 10270 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 10271 | `							if( !fromImport ){` |
|       - | 10272 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 10273 | `								if( p3 == 0 ){` |
|      67 | 10274 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 10275 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 10276 | `									if( pMap ){` |
|      67 | 10277 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 10278 | `										p3 = (void *)pMap;` |
|      31 | 10279 | `									}` |
|      31 | 10280 | `								}` |
|      67 | 10281 | `								if( p3 ){` |
|      67 | 10282 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 10283 | `								}` |
|      31 | 10284 | `							}` |
|      36 | 10285 | `						}` |
|       5 | 10286 | `					}` |
|  187604 | 10287 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 10288 | `					/* Method call,flag that */` |
|     871 | 10289 | `					pInstr->iP2 = 1;` |
|     433 | 10290 | `				}` |
|  187031 | 10291 | `			}` |
|  990859 | 10292 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 10293 | `			ph7_expr_node **apNode;` |
|       - | 10294 | `			sxi32 n;` |
|   81133 | 10295 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 10296 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 10297 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       - | 10298 | `			/* Recurse and generate bytecodes for array index */` |
|   81133 | 10299 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  146427 | 10300 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   65299 | 10301 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   65299 | 10302 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   65299 | 10303 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10304 | `					return rc;` |
|       - | 10305 | `				}` |
|       - | 10306 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   65299 | 10307 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   32652 | 10308 | `			}` |
|   81133 | 10309 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   65299 | 10310 | `				iP1 = 1; /* Node have an index associated with it */` |
|   32647 | 10311 | `			}` |
|   81133 | 10312 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 10313 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     241 | 10314 | `				iP2 = 4;` |
|   81015 | 10315 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 10316 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 10317 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      54 | 10318 | `				iP2 = 5;` |
|   80872 | 10319 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 10320 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 10321 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 10322 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 10323 | `				iP2 = 6;` |
|   80835 | 10324 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 10325 | `				/* Create an empty entry when the desired index is not found */` |
|   31943 | 10326 | `				iP2 = 1;` |
|   15974 | 10327 | `			}` |
|  763269 | 10328 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 10329 | `			/* POP the left node */` |
|      32 | 10330 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 10331 | `		}` |
|  588940 | 10332 | `	}` |
| 1177923 | 10333 | `	rc = SXRET_OK;` |
| 1177923 | 10334 | `	nJmpIdx = 0;` |
|       - | 10335 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 10336 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 10337 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1177923 | 10338 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     279 | 10339 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     279 | 10340 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     279 | 10341 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     279 | 10342 | `			int isSpecial = 0;` |
|     279 | 10343 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     191 | 10344 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     191 | 10345 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     201 | 10346 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     169 | 10347 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      86 | 10348 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      93 | 10349 | `					isSpecial = 1;` |
|      44 | 10350 | `				}` |
|     115 | 10351 | `			}` |
|     323 | 10352 | `			pInstr->iP1 = 0;` |
|     323 | 10353 | `			if( !isSpecial ){` |
|     147 | 10354 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      71 | 10355 | `			}` |
|       - | 10356 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 10357 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     235 | 10358 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     147 | 10359 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     147 | 10360 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 10361 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      46 | 10362 | `					return SXRET_OK;` |
|       - | 10363 | `				}` |
|      50 | 10364 | `			}` |
|      94 | 10365 | `		}` |
|     170 | 10366 | `	}` |
|       - | 10367 | `	/* Generate code for the right tree */` |
| 1177845 | 10368 | `	if( pNode->pRight ){` |
|  650435 | 10369 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 10370 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    9903 | 10371 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  645486 | 10372 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 10373 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3331 | 10374 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  638874 | 10375 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 10376 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 10377 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 10378 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  637198 | 10379 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 10380 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 10381 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 10382 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 10383 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 10384 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 10385 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     101 | 10386 | `			sxu32 nNsJmp = 0;` |
|     101 | 10387 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     101 | 10388 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  637038 | 10389 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  264115 | 10390 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  132055 | 10391 | `		}` |
|  650435 | 10392 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  650435 | 10393 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  650435 | 10394 | `		if( !bIsChainOp ){` |
|       - | 10395 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 10396 | `			 * operator instruction is emitted. */` |
|  478319 | 10397 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  239157 | 10398 | `		}` |
|  650435 | 10399 | `		if( iVmOp == PH7_OP_STORE ){` |
|  260721 | 10400 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  260692 | 10401 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 10402 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 10403 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 10404 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 10405 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 10406 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 10407 | `				 */` |
|      56 | 10408 | `				iVmOp = 0;` |
|  260695 | 10409 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  260669 | 10410 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 10411 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   72743 | 10412 | `					iP2 = 1;` |
|   36374 | 10413 | `				}else{` |
|  187931 | 10414 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10415 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   31897 | 10416 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   31897 | 10417 | `						iP1 = pInstr->iP1;` |
|   15951 | 10418 | `					}else{` |
|  156039 | 10419 | `						p3 = pInstr->p3;` |
|       - | 10420 | `					}` |
|       - | 10421 | `					/* POP the last dynamic load instruction */` |
|  187931 | 10422 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 10423 | `				}` |
|  130337 | 10424 | `			}` |
|  520077 | 10425 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      52 | 10426 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      52 | 10427 | `			if( pInstr ){` |
|      52 | 10428 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10429 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 10430 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 10431 | `					 */` |
|      15 | 10432 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 10433 | `					iP1 = pInstr->iP1;` |
|      15 | 10434 | `					iP2 = pInstr->iP2;` |
|      15 | 10435 | `					p3  = pInstr->p3;` |
|       8 | 10436 | `				}else{` |
|      38 | 10437 | `					p3 = pInstr->p3;` |
|       - | 10438 | `				}` |
|      25 | 10439 | `			}` |
|      25 | 10440 | `		}` |
|  325215 | 10441 | `	}` |
| 1177845 | 10442 | `	if( iVmOp > 0 ){` |
| 1177639 | 10443 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   12971 | 10444 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 10445 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    9465 | 10446 | `				iP1 = 1;` |
|    4735 | 10447 | `			}` |
| 1171156 | 10448 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 10449 | `			/* Namespace-qualify the class name for NEW */ {` |
|   16845 | 10450 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   16845 | 10451 | `				VmInstr *pCallInstr = 0;` |
|   16845 | 10452 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   16821 | 10453 | `					pCallInstr = pPeek;` |
|   16821 | 10454 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    8408 | 10455 | `				}` |
|   16845 | 10456 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   16843 | 10457 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10458 | `					sxu32 nLitForClass;` |
|       - | 10459 | `					/* If the CALL handler already qualified the name using` |
|       - | 10460 | `					 * function imports, recover the original unqualified` |
|       - | 10461 | `					 * literal so we can re-qualify with class imports. */` |
|   16843 | 10462 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 10463 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 10464 | `					}else{` |
|   16811 | 10465 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 10466 | `					}` |
|   16843 | 10467 | `					pPeek->iP1 = 0;` |
|   16843 | 10468 | `					if( !bAbsolute ){` |
|   16827 | 10469 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    8416 | 10470 | `					}else{` |
|      20 | 10471 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 10472 | `					}` |
|    8419 | 10473 | `				}` |
|       - | 10474 | `			}` |
|   16845 | 10475 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   16845 | 10476 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 10477 | `				VmInstr *pPrev;` |
|   16821 | 10478 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   16821 | 10479 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 10480 | `					/* Pop the call instruction, preserve named-arg map */` |
|   16821 | 10481 | `					iP1 = pInstr->iP1;` |
|   16821 | 10482 | `					if( pInstr->p3 ){` |
|      43 | 10483 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 10484 | `					}` |
|   16821 | 10485 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    8408 | 10486 | `				}` |
|    8413 | 10487 | `			}` |
| 1156253 | 10488 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 10489 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 10490 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     161 | 10491 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     161 | 10492 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     161 | 10493 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     161 | 10494 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     161 | 10495 | `				int isSpecialIs = 0;` |
|     161 | 10496 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     157 | 10497 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     157 | 10498 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     157 | 10499 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     152 | 10500 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      77 | 10501 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 10502 | `						isSpecialIs = 1;` |
|       5 | 10503 | `					}` |
|      77 | 10504 | `				}` |
|     163 | 10505 | `				pInstr->iP1 = 0;` |
|     163 | 10506 | `				if( !isSpecialIs && !bAbsolute ){` |
|     141 | 10507 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      68 | 10508 | `				}` |
|      82 | 10509 | `			}` |
| 1147758 | 10510 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 10511 | `			/* Prevent constant expansion for member/property names.` |
|       - | 10512 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 10513 | `			 * should not trigger constant lookup. */` |
|  172121 | 10514 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  172121 | 10515 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  172079 | 10516 | `				pInstr->iP1 = 0;` |
|   86037 | 10517 | `			}` |
|  172121 | 10518 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 10519 | `				/* Static member access,remember that */` |
|     201 | 10520 | `				iP1 = 1;` |
|     201 | 10521 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     201 | 10522 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      38 | 10523 | `					p3 = pInstr->p3;` |
|      38 | 10524 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      17 | 10525 | `				}` |
|      98 | 10526 | `			}` |
|   86058 | 10527 | `		}` |
|       - | 10528 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 10529 | `		 * This is the primary emit path for user-visible calls. */` |
| 1177637 | 10530 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  390897 | 10531 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  195446 | 10532 | `		}` |
|       - | 10533 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1177637 | 10534 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  588816 | 10535 | `	}` |
| 1177843 | 10536 | `	if( nJmpIdx > 0 ){` |
|       - | 10537 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   13353 | 10538 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   13353 | 10539 | `		if( pInstr ){` |
|   13353 | 10540 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    6674 | 10541 | `		}` |
|    6674 | 10542 | `	}` |
| 1177843 | 10543 | `	return rc;` |
| 1551158 | 10544 |  |
|       - | 10545 | `/*` |
|       - | 10546 | ` * Compile a PHP expression.` |
|       - | 10547 | ` * According to the PHP language reference manual:` |
|       - | 10548 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 10549 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 10550 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 10551 | ` *  is "anything that has a value".` |
|       - | 10552 | ` * If something goes wrong while compiling the expression,this` |
|       - | 10553 | ` * function takes care of generating the appropriate error` |
|       - | 10554 | ` * message.` |
|       - | 10555 | ` */` |
|  834506 | 10556 | `static sxi32 PH7_CompileExpr(` |
|       - | 10557 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10558 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 10559 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 10560 | `	)` |
|       5 | 10561 |  |
|       - | 10562 | `	ph7_expr_node *pRoot;` |
|       - | 10563 | `	SySet sExprNode;` |
|       - | 10564 | `	SyToken *pEnd;` |
|       - | 10565 | `	sxi32 nExpr;` |
|       - | 10566 | `	sxi32 iNest;` |
|       - | 10567 | `	sxi32 rc;` |
|       - | 10568 | `	sxu32 nNullsafeBase;` |
|       - | 10569 | `	/* Initialize worker variables */` |
|  834511 | 10570 | `	nExpr = 0;` |
|  834511 | 10571 | `	pRoot = 0;` |
|       - | 10572 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 10573 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  834511 | 10574 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  834511 | 10575 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  834511 | 10576 | `	SySetAlloc(&sExprNode,0x10);` |
|  834511 | 10577 | `	rc = SXRET_OK;` |
|       - | 10578 | `	/* Delimit the expression */` |
|  834511 | 10579 | `	pEnd = pGen->pIn;` |
|  834511 | 10580 | `	iNest = 0;` |
| 5583623 | 10581 | `	while( pEnd < pGen->pEnd ){` |
| 5298869 | 10582 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10583 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     411 | 10584 | `			iNest++;` |
| 5298666 | 10585 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     419 | 10586 | `			iNest--;` |
| 5298256 | 10587 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  550055 | 10588 | `			if( iNest <= 0 ){` |
|  549757 | 10589 | `				break;` |
|       - | 10590 | `			}` |
|     149 | 10591 | `		}` |
| 4749117 | 10592 | `		pEnd++;` |
|       5 | 10593 | `	}` |
|  834511 | 10594 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   19317 | 10595 | `		SyToken *pEnd2 = pGen->pIn;` |
|   19317 | 10596 | `		iNest = 0;` |
|       - | 10597 | `		/* Stop at the first comma */` |
|   38893 | 10598 | `		while( pEnd2 < pEnd ){` |
|   19585 | 10599 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      59 | 10600 | `				iNest++;` |
|   19558 | 10601 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      59 | 10602 | `				iNest--;` |
|   19504 | 10603 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      49 | 10604 | `				if( iNest <= 0 ){` |
|       5 | 10605 | `					break;` |
|       - | 10606 | `				}` |
|      20 | 10607 | `			}` |
|   19581 | 10608 | `			pEnd2++;` |
|       5 | 10609 | `		}` |
|   19317 | 10610 | `		if( pEnd2 <pEnd ){` |
|       5 | 10611 | `			pEnd = pEnd2;` |
|       2 | 10612 | `		}` |
|    9656 | 10613 | `	}` |
|  834511 | 10614 | `	if( pEnd > pGen->pIn ){` |
|  834501 | 10615 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 10616 | `		/* Swap delimiter */` |
|  834501 | 10617 | `		pGen->pEnd = pEnd;` |
|       - | 10618 | `		/* Try to get an expression tree */` |
|  834501 | 10619 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  834501 | 10620 | `		if( rc == SXRET_OK && pRoot ){` |
|  834319 | 10621 | `			rc = SXRET_OK;` |
|  834319 | 10622 | `			if( xTreeValidator ){` |
|       - | 10623 | `				/* Call the upper layer validator callback */` |
|   23343 | 10624 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   11669 | 10625 | `			}` |
|  834319 | 10626 | `			if( rc != SXERR_ABORT ){` |
|       - | 10627 | `				/* Generate code for the given tree */` |
|  834319 | 10628 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 10629 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 10630 | `				 * expression so they short-circuit to its end. */` |
|  834319 | 10631 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  417157 | 10632 | `			}` |
|  834319 | 10633 | `			nExpr = 1;` |
|  417157 | 10634 | `		}` |
|       - | 10635 | `		/* Release the whole tree */` |
|  834501 | 10636 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 10637 | `		/* Synchronize token stream */` |
|  834501 | 10638 | `		pGen->pEnd = pTmp;` |
|  834501 | 10639 | `		pGen->pIn  = pEnd;` |
|  834501 | 10640 | `		if( rc == SXERR_ABORT ){` |
|      14 | 10641 | `			SySetRelease(&sExprNode);` |
|      14 | 10642 | `			return SXERR_ABORT;` |
|       - | 10643 | `		}` |
|  417243 | 10644 | `	}` |
|  834501 | 10645 | `	SySetRelease(&sExprNode);` |
|  834501 | 10646 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  417258 | 10647 |  |
|       - | 10648 | `/*` |
|       - | 10649 | ` * Return a pointer to the node construct handler associated` |
|       - | 10650 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 10651 | ` */` |
|  212458 | 10652 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 10653 |  |
|  212463 | 10654 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 10655 | `		/* Numeric literal: Either real or integer */` |
|  111559 | 10656 | `		return PH7_CompileNumLiteral;` |
|  100909 | 10657 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 10658 | `		/* Double quoted string */` |
|   20613 | 10659 | `		return PH7_CompileString;` |
|   80301 | 10660 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 10661 | `		/* Single quoted string */` |
|   80187 | 10662 | `		return PH7_CompileSimpleString;` |
|     119 | 10663 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 10664 | `		/* Heredoc */` |
|      68 | 10665 | `		return PH7_CompileHereDoc;` |
|      55 | 10666 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 10667 | `		/* Nowdoc */` |
|      48 | 10668 | `		return PH7_CompileNowDoc;` |
|       8 | 10669 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 10670 | `		/* Backtick quoted string */` |
|       6 | 10671 | `		return PH7_CompileBacktic;` |
|       - | 10672 | `	}` |
|       3 | 10673 | `	return 0;` |
|  106234 | 10674 |  |
|       - | 10675 | `/*` |
|       - | 10676 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 10677 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 10678 | ` * in write context" parse error.` |
|       - | 10679 | ` */` |
|    6754 | 10680 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 10681 |  |
|       - | 10682 | `	sxi32 rc;` |
|    6759 | 10683 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6757 | 10684 | `		return SXRET_OK;` |
|       - | 10685 | `	}` |
|       5 | 10686 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 10687 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 10688 | `		"Can't use nullsafe operator in write context");` |
|       3 | 10689 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3382 | 10690 |  |
|       - | 10691 | `/*` |
|       - | 10692 | ` * Compile an unset() statement.` |
|       - | 10693 | ` * unset($var, $arr[$key], ...);` |
|       - | 10694 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 10695 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 10696 | ` * parent array before extracting the element to unset.` |
|       - | 10697 | ` */` |
|    2906 | 10698 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 10699 |  |
|    2911 | 10700 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2911 | 10701 | `	sxu32 nIdx = 0;` |
|       - | 10702 | `	SyString sName;` |
|       - | 10703 | `	sxi32 rc;` |
|       - | 10704 | `	/* Jump the 'unset' keyword */` |
|    2911 | 10705 | `	pGen->pIn++;` |
|       - | 10706 | `	/* Save delimiter */` |
|    2911 | 10707 | `	pTmp = pGen->pEnd;` |
|       - | 10708 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2911 | 10709 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2911 | 10710 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 10711 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 10712 | `		SyToken *pClose;` |
|    2911 | 10713 | `		pGen->pIn++;   /* Skip '(' */` |
|    2911 | 10714 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2911 | 10715 | `		pEnd = pClose; /* Stop at ')' */` |
|    1453 | 10716 | `	}` |
|    2911 | 10717 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 10718 | `	/* Resolve the 'unset' builtin name once */` |
|    2911 | 10719 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     359 | 10720 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     359 | 10721 | `		if( pObj == 0 ){` |
|     ! 0 | 10722 | `			return SXERR_ABORT;` |
|       - | 10723 | `		}` |
|     359 | 10724 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     359 | 10725 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     177 | 10726 | `	}` |
|       - | 10727 | `	/* Compile each comma-separated argument */` |
|    9667 | 10728 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6761 | 10729 | `		if( pGen->pIn < pNext ){` |
|    6761 | 10730 | `			pGen->pEnd = pNext;` |
|    6761 | 10731 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 10732 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 10733 | `				GenStateUnsetValidator);` |
|    6761 | 10734 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10735 | `				return SXERR_ABORT;` |
|       - | 10736 | `			}` |
|    6761 | 10737 | `			if( rc != SXERR_EMPTY ){` |
|       - | 10738 | `				/* Emit call for this single argument */` |
|    6759 | 10739 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6759 | 10740 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6759 | 10741 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3377 | 10742 | `			}` |
|    3378 | 10743 | `		}` |
|       - | 10744 | `		/* Jump trailing commas */` |
|   10613 | 10745 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3857 | 10746 | `			pNext++;` |
|       5 | 10747 | `		}` |
|    6761 | 10748 | `		pGen->pIn = pNext;` |
|       5 | 10749 | `	}` |
|       - | 10750 | `	/* Skip past the closing ')' if present */` |
|    2911 | 10751 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2911 | 10752 | `		pGen->pIn++;` |
|    1453 | 10753 | `	}` |
|       - | 10754 | `	/* Restore token stream */` |
|    2911 | 10755 | `	pGen->pEnd = pTmp;` |
|    2911 | 10756 | `	return SXRET_OK;` |
|    1458 | 10757 |  |
|       - | 10758 | `/*` |
|       - | 10759 | ` * PHP Language construct table.` |
|       - | 10760 | ` */` |
|       - | 10761 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 10762 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 10763 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 10764 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 10765 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 10766 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 10767 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 10768 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 10769 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 10770 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 10771 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 10772 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 10773 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 10774 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 10775 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 10776 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 10777 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 10778 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 10779 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 10780 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 10781 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 10782 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 10783 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 10784 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 10785 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 10786 | `};` |
|       - | 10787 | `/*` |
|       - | 10788 | ` * Return a pointer to the statement handler routine associated` |
|       - | 10789 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 10790 | ` */` |
|  562666 | 10791 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 10792 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 10793 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 10794 | `	)` |
|       5 | 10795 |  |
|  562671 | 10796 | `	sxu32 n = 0;` |
| 2904113 | 10797 | `	for(;;){` |
| 5808231 | 10798 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  120867 | 10799 | `			break;` |
|       - | 10800 | `		}` |
| 5687369 | 10801 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  441809 | 10802 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 10803 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 10804 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 10805 | `					/* 'static' (class context),return null */` |
|     ! 0 | 10806 | `					return 0;` |
|       - | 10807 | `				}` |
|     ! 0 | 10808 | `			}` |
|  441804 | 10809 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       6 | 10810 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 10811 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 10812 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 10813 | `				return 0;` |
|       - | 10814 | `			}` |
|       - | 10815 | `			/* Return a pointer to the handler.` |
|       - | 10816 | `			*/` |
|  441809 | 10817 | `			return aLangConstruct[n].xConstruct;` |
|       - | 10818 | `		}` |
| 5245565 | 10819 | `		n++;` |
|       5 | 10820 | `	}` |
|  120867 | 10821 | `	if( pLookahed ){` |
|  120867 | 10822 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   34677 | 10823 | `			return PH7_CompileClassInterface;` |
|   86195 | 10824 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   85929 | 10825 | `			return PH7_CompileClass;` |
|     271 | 10826 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      61 | 10827 | `			return PH7_CompileTrait;` |
|     210 | 10828 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      26 | 10829 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      25 | 10830 | `				return PH7_CompileAbstractClass;` |
|     190 | 10831 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 10832 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 10833 | `				return PH7_CompileFinalClass;` |
|       - | 10834 | `		}` |
|      94 | 10835 | `	}` |
|       - | 10836 | `	/* Not a language construct */` |
|     193 | 10837 | `	return 0;` |
|  281338 | 10838 |  |
|       - | 10839 | `/*` |
|       - | 10840 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 10841 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 10842 | ` */` |
|     188 | 10843 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 10844 |  |
|       - | 10845 | `	int rc;` |
|     193 | 10846 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     193 | 10847 | `	if( rc == FALSE ){` |
|      82 | 10848 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      81 | 10849 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 10850 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 10851 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 10852 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 10853 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 10854 | `			*/` |
|       - | 10855 | `			){` |
|      79 | 10856 | `				rc = TRUE;` |
|      37 | 10857 | `		}` |
|      41 | 10858 | `	}` |
|     193 | 10859 | `	return rc;` |
|       5 | 10860 |  |
|       - | 10861 | `/*` |
|       - | 10862 | ` * Compile a PHP chunk.` |
|       - | 10863 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10864 | ` * takes care of generating the appropriate error message.` |
|       - | 10865 | ` */` |
|  673838 | 10866 | `static sxi32 GenStateCompileChunk(` |
|       - | 10867 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10868 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 10869 | `	)` |
|       5 | 10870 |  |
|       - | 10871 | `	ProcLangConstruct xCons;` |
|       - | 10872 | `	sxi32 rc;` |
|  673843 | 10873 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  525527 | 10874 | `	for(;;){` |
|  862451 | 10875 | `		int bStmtIsDeclare = 0;` |
|  862451 | 10876 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10877 | `			/* No more input to process */` |
|   13335 | 10878 | `			break;` |
|       - | 10879 | `		}` |
|       - | 10880 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 10881 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  849121 | 10882 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  562671 | 10883 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  562671 | 10884 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 10885 | `				bStmtIsDeclare = 1;` |
|      20 | 10886 | `			}` |
|  281333 | 10887 | `		}` |
|  849121 | 10888 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 10889 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 10890 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  188583 | 10891 | `			pGen->bStrictTypesLocked = 1;` |
|   94289 | 10892 | `		}` |
|  849121 | 10893 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10894 | `			/* Compile block */` |
|      21 | 10895 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      21 | 10896 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10897 | `				break;` |
|       - | 10898 | `			}` |
|      13 | 10899 | `		}else{` |
|  849105 | 10900 | `			xCons = 0;` |
|  849105 | 10901 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  562671 | 10902 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 10903 | `				/* Try to extract a language construct handler */` |
|  562671 | 10904 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  562671 | 10905 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 10906 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10907 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 10908 | `						&pGen->pIn->sData);` |
|       9 | 10909 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10910 | `						break;` |
|       - | 10911 | `					}` |
|       - | 10912 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 10913 | `					 * this erroneous statement.` |
|       - | 10914 | `					 */` |
|       9 | 10915 | `					xCons = PH7_ErrorRecover;` |
|       4 | 10916 | `				}` |
|  567772 | 10917 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   46913 | 10918 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 10919 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 10920 | `				xCons = PH7_CompileLabel;` |
|      56 | 10921 | `			}` |
|  849105 | 10922 | `			if( xCons == 0 ){` |
|       - | 10923 | `				/* Assume an expression an try to compile it */` |
|  286507 | 10924 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  286507 | 10925 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 10926 | `					/* Pop l-value */` |
|  286357 | 10927 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  143176 | 10928 | `				}` |
|  143256 | 10929 | `			}else{` |
|       - | 10930 | `				/* Go compile the sucker */` |
|  562603 | 10931 | `				rc = xCons(&(*pGen));` |
|       - | 10932 | `			}` |
|  849105 | 10933 | `			if( rc == SXERR_ABORT ){` |
|       - | 10934 | `				/* Request to abort compilation */` |
|      14 | 10935 | `				break;` |
|       - | 10936 | `			}` |
|       - | 10937 | `		}` |
|       - | 10938 | `		/* Ignore trailing semi-colons ';' */` |
| 1374107 | 10939 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  525001 | 10940 | `			pGen->pIn++;` |
|       5 | 10941 | `		}` |
|  849111 | 10942 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 10943 | `			/* Compile a single statement and return */` |
|  660503 | 10944 | `			break;` |
|       - | 10945 | `		}` |
|       - | 10946 | `		/* LOOP ONE */` |
|       - | 10947 | `		/* LOOP TWO */` |
|       - | 10948 | `		/* LOOP THREE */` |
|       - | 10949 | `		/* LOOP FOUR */` |
|       5 | 10950 | `	}` |
|       - | 10951 | `	/* Return compilation status */` |
|  673843 | 10952 | `	return rc;` |
|       5 | 10953 |  |
|       - | 10954 | `/*` |
|       - | 10955 | ` * Compile a Raw PHP chunk.` |
|       - | 10956 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10957 | ` * takes care of generating the appropriate error message.` |
|       - | 10958 | ` */` |
|   13342 | 10959 | `static sxi32 PH7_CompilePHP(` |
|       - | 10960 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10961 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 10962 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 10963 | `	)` |
|       5 | 10964 |  |
|   13347 | 10965 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 10966 | `	sxi32 rc;` |
|       - | 10967 | `	/* Reset the token set */` |
|   13347 | 10968 | `	SySetReset(&(*pTokenSet));` |
|       - | 10969 | `	/* Mark as the default token set */` |
|   13347 | 10970 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 10971 | `	/* Advance the stream cursor */` |
|   13347 | 10972 | `	pGen->pRawIn++;` |
|       - | 10973 | `	/* Tokenize the PHP chunk first */` |
|   13347 | 10974 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 10975 | `	/* Point to the head and tail of the token stream. */` |
|   13347 | 10976 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   13347 | 10977 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   13347 | 10978 | `	if( is_expr ){` |
|     ! 0 | 10979 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 10980 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 10981 | `			/* A simple expression,compile it */` |
|     ! 0 | 10982 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 10983 | `		}` |
|       - | 10984 | `		/* Emit the DONE instruction */` |
|     ! 0 | 10985 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 10986 | `		return SXRET_OK;` |
|       - | 10987 | `	}` |
|   13347 | 10988 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 10989 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 10990 | `		/*` |
|       - | 10991 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 10992 | `		 * According to the PHP reference manual:` |
|       - | 10993 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 10994 | `		 *  immediately follow` |
|       - | 10995 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 10996 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 10997 | `		 * Symisc extension:` |
|       - | 10998 | `		 *   This short syntax works with all PHP opening` |
|       - | 10999 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 11000 | `		 *   only short tag.` |
|       - | 11001 | `		 */` |
|       - | 11002 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 11003 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 11004 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11005 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11006 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11007 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11008 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11009 | `		}` |
|       3 | 11010 | `		return SXRET_OK;` |
|       - | 11011 | `	}` |
|       - | 11012 | `	/* Compile the PHP chunk */` |
|   13345 | 11013 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11014 | `	/* Fix exceptions jumps */` |
|   13345 | 11015 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11016 | `	/* Fix gotos now, the jump destination is resolved */` |
|   13345 | 11017 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11018 | `		rc = SXERR_ABORT;` |
|       1 | 11019 | `	}` |
|       - | 11020 | `	/* Reset container */` |
|   13345 | 11021 | `	SySetReset(&pGen->aGoto);` |
|   13345 | 11022 | `	SySetReset(&pGen->aLabel);` |
|   13345 | 11023 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11024 | `	/* Compilation result */` |
|   13345 | 11025 | `	return rc;` |
|    6676 | 11026 |  |
|       - | 11027 | `/*` |
|       - | 11028 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11029 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11030 | ` * This is the only compile interface exported from this file.` |
|       - | 11031 | ` */` |
|   16010 | 11032 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11033 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11034 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11035 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11036 | `	)` |
|       5 | 11037 |  |
|       - | 11038 | `	SySet aPhpToken,aRawToken;` |
|       - | 11039 | `	ph7_gen_state *pCodeGen;` |
|       - | 11040 | `	ph7_value *pRawObj;` |
|       - | 11041 | `	sxu32 nObjIdx;` |
|       - | 11042 | `	sxi32 nRawObj;` |
|       - | 11043 | `	int is_expr;` |
|       - | 11044 | `	sxi8 bSavedStrict;` |
|       - | 11045 | `	sxi8 bSavedStrictLocked;` |
|       - | 11046 | `	sxi32 rc;` |
|   16015 | 11047 | `	if( pScript->nByte < 1 ){` |
|       - | 11048 | `		/* Nothing to compile */` |
|     ! 0 | 11049 | `		return PH7_OK;` |
|       - | 11050 | `	}` |
|       - | 11051 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11052 | `	 * file's flags so include/require restore them on return. */` |
|   16015 | 11053 | `	pCodeGen = &pVm->sCodeGen;` |
|   16015 | 11054 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   16015 | 11055 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   16015 | 11056 | `	pCodeGen->bStrictTypes = 0;` |
|   16015 | 11057 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11058 | `	/* Initialize the tokens containers */` |
|   16015 | 11059 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16015 | 11060 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16015 | 11061 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   16015 | 11062 | `	is_expr = 0;` |
|   16015 | 11063 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11064 | `		SyToken sTmp;` |
|       - | 11065 | `		/* PHP only: -*/` |
|    3217 | 11066 | `		sTmp.nLine = 1;` |
|    3217 | 11067 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3217 | 11068 | `		sTmp.pUserData = 0;` |
|    3217 | 11069 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3217 | 11070 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3217 | 11071 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11072 | `			/* A simple PHP expression */` |
|     ! 0 | 11073 | `			is_expr = 1;` |
|     ! 0 | 11074 | `		}` |
|    1611 | 11075 | `	}else{` |
|       - | 11076 | `		/* Tokenize raw text */` |
|   12803 | 11077 | `		SySetAlloc(&aRawToken,32);` |
|   12803 | 11078 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11079 | `	}` |
|       - | 11080 | `	/* Process high-level tokens */` |
|   16015 | 11081 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   16015 | 11082 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   16015 | 11083 | `	rc = PH7_OK;` |
|   16015 | 11084 | `	if( is_expr ){` |
|       - | 11085 | `		/* Compile the expression */` |
|     ! 0 | 11086 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 11087 | `		goto cleanup;` |
|       - | 11088 | `	}` |
|   16015 | 11089 | `	nObjIdx = 0;` |
|       - | 11090 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 11091 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 11092 | `	 * preventing namespace bleeding across include()d files. */` |
|   16015 | 11093 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 11094 | `	/* Start the compilation process */` |
|   14410 | 11095 | `	for(;;){` |
|   42155 | 11096 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   16003 | 11097 | `			break; /* No more tokens to process */` |
|       - | 11098 | `		}` |
|   26157 | 11099 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 11100 | `			/* Compile the PHP chunk */` |
|   13347 | 11101 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   13347 | 11102 | `			if( rc == SXERR_ABORT ){` |
|      16 | 11103 | `				break;` |
|       - | 11104 | `			}` |
|   13335 | 11105 | `			continue;` |
|       - | 11106 | `		}` |
|       - | 11107 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   12815 | 11108 | `		nRawObj = 0;` |
|   25667 | 11109 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 11110 | `			/* Consume the raw chunk without any processing */` |
|   12857 | 11111 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   12857 | 11112 | `			if( pRawObj == 0 ){` |
|     ! 0 | 11113 | `				rc = SXERR_MEM;` |
|     ! 0 | 11114 | `				break;` |
|       - | 11115 | `			}` |
|       - | 11116 | `			/* Mark as constant and emit the load constant instruction */` |
|   12857 | 11117 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   12857 | 11118 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   12857 | 11119 | `			++nRawObj;` |
|   12857 | 11120 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 11121 | `		}` |
|   12815 | 11122 | `		if( nRawObj > 0 ){` |
|       - | 11123 | `			/* Emit the consume instruction */` |
|   12815 | 11124 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6405 | 11125 | `		}` |
|    8010 | 11126 | `	}` |
|    8005 | 11127 | `cleanup:` |
|   16015 | 11128 | `	SySetRelease(&aRawToken);` |
|   16015 | 11129 | `	SySetRelease(&aPhpToken);` |
|       - | 11130 | `	/* Restore outer file's strict_types scope */` |
|   16015 | 11131 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   16015 | 11132 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   16015 | 11133 | `	return rc;` |
|    8010 | 11134 |  |
|       - | 11135 | `/*` |
|       - | 11136 | ` * Utility routines.Initialize the code generator.` |
|       - | 11137 | ` */` |
|    3148 | 11138 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 11139 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11140 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11141 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11142 | `	)` |
|       5 | 11143 |  |
|    3153 | 11144 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11145 | `	/* Zero the structure */` |
|    3153 | 11146 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 11147 | `	/* Initial state */` |
|    3153 | 11148 | `	pGen->pVm  = &(*pVm);` |
|    3153 | 11149 | `	pGen->xErr = xErr;` |
|    3153 | 11150 | `	pGen->pErrData = pErrData;` |
|    3153 | 11151 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3153 | 11152 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3153 | 11153 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3153 | 11154 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3153 | 11155 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 11156 | `	/* Error log buffer */` |
|    3153 | 11157 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 11158 | `	/* General purpose working buffer */` |
|    3153 | 11159 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 11160 | `	/* Namespace state */` |
|    3153 | 11161 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3153 | 11162 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3153 | 11163 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3153 | 11164 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11165 | `	/* Create the global scope */` |
|    3153 | 11166 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 11167 | `	/* Point to the global scope */` |
|    3153 | 11168 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3153 | 11169 | `	return SXRET_OK;` |
|       5 | 11170 |  |
|       - | 11171 | `/*` |
|       - | 11172 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 11173 | ` */` |
|   18844 | 11174 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 11175 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11176 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11177 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11178 | `	)` |
|       5 | 11179 |  |
|   18849 | 11180 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11181 | `	GenBlock *pBlock,*pParent;` |
|       - | 11182 | `	/* Reset state */` |
|   18849 | 11183 | `	SySetReset(&pGen->aLabel);` |
|   18849 | 11184 | `	SySetReset(&pGen->aGoto);` |
|   18849 | 11185 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   18849 | 11186 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   18849 | 11187 | `	SyBlobRelease(&pGen->sWorker);` |
|   18849 | 11188 | `	SyBlobRelease(&pGen->sNamespace);` |
|   18849 | 11189 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   18849 | 11190 | `	SyHashRelease(&pGen->hUseImports);` |
|   18849 | 11191 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   18849 | 11192 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   18849 | 11193 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   18849 | 11194 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   18849 | 11195 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11196 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 11197 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 11198 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 11199 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 11200 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 11201 | `	 * number of unique names, which is acceptable. */` |
|       - | 11202 | `	/* Point to the global scope */` |
|   18849 | 11203 | `	pBlock = pGen->pCurrent;` |
|   18849 | 11204 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 11205 | `		pParent = pBlock->pParent;` |
|     ! 0 | 11206 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 11207 | `		pBlock = pParent;` |
|     ! 0 | 11208 | `	}` |
|   18849 | 11209 | `	pGen->xErr = xErr;` |
|   18849 | 11210 | `	pGen->pErrData = pErrData;` |
|   18849 | 11211 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   18849 | 11212 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   18849 | 11213 | `	pGen->pIn = pGen->pEnd = 0;` |
|   18849 | 11214 | `	pGen->nErr = 0;` |
|   18849 | 11215 | `	return SXRET_OK;` |
|       5 | 11216 |  |
|       - | 11217 | `/*` |
|       - | 11218 | ` * Generate a compile-time error message.` |
|       - | 11219 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 11220 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 11221 | ` * abort compilation immediately.` |
|       - | 11222 | ` */` |
|     574 | 11223 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 11224 |  |
|     579 | 11225 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     579 | 11226 | `	const char *zErr = "Error";` |
|       - | 11227 | `	SyString *pFile;` |
|       - | 11228 | `	va_list ap;` |
|       - | 11229 | `	sxi32 rc;` |
|       - | 11230 | `	/* Reset the working buffer */` |
|     579 | 11231 | `	SyBlobReset(pWorker);` |
|       - | 11232 | `	/* Peek the processed file path if available */` |
|     579 | 11233 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     579 | 11234 | `	if( nErrType == E_ERROR ){` |
|       - | 11235 | `		/* Increment the error counter */` |
|     473 | 11236 | `		pGen->nErr++;` |
|     473 | 11237 | `		if( pGen->nErr > 15 ){` |
|       - | 11238 | `			/* Error count limit reached */` |
|       5 | 11239 | `			if( pGen->xErr ){` |
|       5 | 11240 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 11241 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 11242 | `				if( pFile ){` |
|       5 | 11243 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 11244 | `				}` |
|       5 | 11245 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 11246 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 11247 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 11248 | `				}` |
|       2 | 11249 | `			}` |
|       - | 11250 | `			/* Abort immediately */` |
|       5 | 11251 | `			return SXERR_ABORT;` |
|       - | 11252 | `		}` |
|     232 | 11253 | `	}` |
|     575 | 11254 | `	if( pGen->xErr == 0 ){` |
|       - | 11255 | `		/* No available error consumer,return immediately */` |
|       3 | 11256 | `		return SXRET_OK;` |
|       - | 11257 | `	}` |
|     572 | 11258 | `	switch(nErrType){` |
|     466 | 11259 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 11260 | `	case E_WARNING: zErr = "Warning";     break;` |
|      76 | 11261 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      12 | 11262 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 11263 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 11264 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 11265 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 11266 | `	default:` |
|     ! 0 | 11267 | `		break;` |
|       - | 11268 | `	}` |
|     572 | 11269 | `	rc = SXRET_OK;` |
|       - | 11270 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     572 | 11271 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     572 | 11272 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     572 | 11273 | `	va_start(ap,zFormat);` |
|     572 | 11274 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     572 | 11275 | `	va_end(ap);` |
|     572 | 11276 | `	if( pFile ){` |
|     572 | 11277 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     284 | 11278 | `	}` |
|       - | 11279 | `	/* Append a new line */` |
|     572 | 11280 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     572 | 11281 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 11282 | `		/* Consume the generated error message */` |
|     572 | 11283 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     284 | 11284 | `	}` |
|     572 | 11285 | `	return rc;` |
|     292 | 11286 |  |
|       - | 11287 |  |
