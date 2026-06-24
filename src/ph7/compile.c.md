# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5523/6879 lines (80.29%)

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
|      97 |   116 | `			aLabel[n].bRef = TRUE;` |
|      97 |   117 | `			if( ppOut ){` |
|      97 |   118 | `				*ppOut = &aLabel[n];` |
|      46 |   119 | `			}` |
|      97 |   120 | `			return SXRET_OK;` |
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
|    3584 |   131 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   132 |  |
|    3589 |   133 | `	GenBlock *pBlock = pCurrent;` |
|   10169 |   134 | `	for(;;){` |
|   20343 |   135 | `		if( pBlock->iFlags & iBlockType ){` |
|    3481 |   136 | `			iCount--; /* Decrement nesting level */` |
|    3481 |   137 | `			if( iCount < 1 ){` |
|       - |   138 | `				/* Block meet with the desired criteria */` |
|    3455 |   139 | `				return pBlock;` |
|       - |   140 | `			}` |
|      13 |   141 | `		}` |
|       - |   142 | `		/* Point to the upper block */` |
|   16893 |   143 | `		pBlock = pBlock->pParent;` |
|   16893 |   144 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   145 | `			/* Forbidden */` |
|      72 |   146 | `			break;` |
|       - |   147 | `		}` |
|       5 |   148 | `	}` |
|       - |   149 | `	/* No such block */` |
|     139 |   150 | `	return 0;` |
|    1797 |   151 |  |
|       - |   152 | `/*` |
|       - |   153 | ` * Initialize a freshly allocated block instance.` |
|       - |   154 | ` */` |
|  780488 |   155 | `static void GenStateInitBlock(` |
|       - |   156 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   157 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   158 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   159 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   160 | `	void *pUserData      /* Upper layer private data */` |
|       - |   161 | `	)` |
|       5 |   162 |  |
|       - |   163 | `	/* Initialize block fields */` |
|  780493 |   164 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  780493 |   165 | `	pBlock->pUserData   = pUserData;` |
|  780493 |   166 | `	pBlock->pGen        = pGen;` |
|  780493 |   167 | `	pBlock->iFlags      = iType;` |
|  780493 |   168 | `	pBlock->pParent     = 0;` |
|  780493 |   169 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  780493 |   170 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  780493 |   171 |  |
|       - |   172 | `/*` |
|       - |   173 | ` * Allocate a new block instance.` |
|       - |   174 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   175 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   176 | ` * processing on failure.` |
|       - |   177 | ` */` |
|  777182 |   178 | `static sxi32 GenStateEnterBlock(` |
|       - |   179 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   180 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   181 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   182 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   183 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   184 | `	)` |
|       5 |   185 |  |
|       - |   186 | `	GenBlock *pBlock;` |
|       - |   187 | `	/* Allocate a new block instance */` |
|  777187 |   188 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  777187 |   189 | `	if( pBlock == 0 ){` |
|       - |   190 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   191 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   192 | `		 */` |
|     ! 0 |   193 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   194 | `		/* Abort processing immediately */` |
|     ! 0 |   195 | `		return SXERR_ABORT;` |
|       - |   196 | `	}` |
|       - |   197 | `	/* Zero the structure */` |
|  777187 |   198 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  777187 |   199 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   200 | `	/* Link to the parent block */` |
|  777187 |   201 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   202 | `	/* Mark as the current block */` |
|  777187 |   203 | `	pGen->pCurrent = pBlock;` |
|  777187 |   204 | `	if( ppBlock ){` |
|       - |   205 | `		/* Write a pointer to the new instance */` |
|  377497 |   206 | `		*ppBlock = pBlock;` |
|  188746 |   207 | `	}` |
|  777187 |   208 | `	return SXRET_OK;` |
|  388596 |   209 |  |
|       - |   210 | `/*` |
|       - |   211 | ` * Release block fields without freeing the whole instance.` |
|       - |   212 | ` */` |
|  777174 |   213 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   214 |  |
|  777179 |   215 | `	SySetRelease(&pBlock->aPostContFix);` |
|  777179 |   216 | `	SySetRelease(&pBlock->aJumpFix);` |
|  777179 |   217 |  |
|       - |   218 | `/*` |
|       - |   219 | ` * Release a block.` |
|       - |   220 | ` */` |
|  777174 |   221 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   222 |  |
|  777179 |   223 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  777179 |   224 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   225 | `	/* Free the instance */` |
|  777179 |   226 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  777179 |   227 |  |
|       - |   228 | `/*` |
|       - |   229 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   230 | ` */` |
|  777174 |   231 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   232 |  |
|  777179 |   233 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  777179 |   234 | `	if( pBlock == 0 ){` |
|       - |   235 | `		/* No more block to pop */` |
|     ! 0 |   236 | `		return SXERR_EMPTY;` |
|       - |   237 | `	}` |
|       - |   238 | `	/* Point to the upper block */` |
|  777179 |   239 | `	pGen->pCurrent = pBlock->pParent;` |
|  777179 |   240 | `	if( ppBlock ){` |
|       - |   241 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   242 | `		*ppBlock = pBlock;` |
|     ! 0 |   243 | `	}else{` |
|       - |   244 | `		/* Safely release the block */` |
|  777179 |   245 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   246 | `	}` |
|  777179 |   247 | `	return SXRET_OK;` |
|  388592 |   248 |  |
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
|  220840 |   259 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   260 |  |
|       - |   261 | `	JumpFixup sJumpFix;` |
|       - |   262 | `	sxi32 rc;` |
|       - |   263 | `	/* Init the JumpFixup structure */` |
|  220845 |   264 | `	sJumpFix.nJumpType = nJumpType;` |
|  220845 |   265 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   266 | `	/* Insert in the jump fixup table */` |
|  220845 |   267 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  220845 |   268 | `	return rc;` |
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
|  543396 |   281 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   282 |  |
|       - |   283 | `	JumpFixup *aFix;` |
|       - |   284 | `	VmInstr *pInstr;` |
|       - |   285 | `	sxu32 nFixed;` |
|       - |   286 | `	sxu32 n;` |
|       - |   287 | `	/* Point to the jump fixup table */` |
|  543401 |   288 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   289 | `	/* Fix the desired jumps */` |
|  978837 |   290 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  435441 |   291 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   292 | `			/* Already fixed */` |
|  173793 |   293 | `			continue;` |
|       - |   294 | `		}` |
|  261653 |   295 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   296 | `			/* Not of our interest */` |
|   40815 |   297 | `			continue;` |
|       - |   298 | `		}` |
|       - |   299 | `		/* Point to the instruction to fix */` |
|  220843 |   300 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  220843 |   301 | `		if( pInstr ){` |
|  220843 |   302 | `			pInstr->iP2 = nJumpDest;` |
|  220843 |   303 | `			nFixed++;` |
|       - |   304 | `			/* Mark as fixed */` |
|  220843 |   305 | `			aFix[n].nJumpType = -1;` |
|  110419 |   306 | `		}` |
|  110424 |   307 | `	}` |
|       - |   308 | `	/* Total number of fixed jumps */` |
|  543401 |   309 | `	return nFixed;` |
|       5 |   310 |  |
|       - |   311 | `/*` |
|       - |   312 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   313 | ` * The goto statement can be used to jump to another section` |
|       - |   314 | ` * in the program.` |
|       - |   315 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   316 | ` * statement for more information.` |
|       - |   317 | ` */` |
|  220738 |   318 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   319 |  |
|       - |   320 | `	JumpFixup *pJump,*aJumps;` |
|       - |   321 | `	Label *pLabel,*aLabel;` |
|       - |   322 | `	VmInstr *pInstr;` |
|       - |   323 | `	sxi32 rc;` |
|       - |   324 | `	sxu32 n;` |
|       - |   325 | `	/* Point to the goto table */` |
|  220743 |   326 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   327 | `	/* Fix */` |
|  220889 |   328 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|      97 |   341 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|      11 |   342 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|      11 |   343 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   344 | `				return SXERR_ABORT;` |
|       - |   345 | `			}` |
|       4 |   346 | `		}` |
|       - |   347 | `		/* Fix the jump now the destination is resolved */` |
|      97 |   348 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      97 |   349 | `		if( pInstr ){` |
|      97 |   350 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   351 | `		}` |
|      51 |   352 | `	}` |
|  220741 |   353 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  220873 |   354 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   355 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   356 | `			/* Emit a warning */` |
|      40 |   357 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   358 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   359 | `		}` |
|      71 |   360 | `	}` |
|  220741 |   361 | `	return SXRET_OK;` |
|  110374 |   362 |  |
|       - |   363 | `/*` |
|       - |   364 | ` * Check if a given token value is installed in the literal table.` |
|       - |   365 | ` */` |
|  698214 |   366 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   367 |  |
|       - |   368 | `	SyHashEntry *pEntry;` |
|  698219 |   369 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  698219 |   370 | `	if( pEntry == 0 ){` |
|  303523 |   371 | `		return SXERR_NOTFOUND;` |
|       - |   372 | `	}` |
|  394701 |   373 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  394701 |   374 | `	return SXRET_OK;` |
|  349112 |   375 |  |
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
|  303518 |   386 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   387 |  |
|  303523 |   388 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  303523 |   389 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  151759 |   390 | `	}` |
|  303523 |   391 | `	return SXRET_OK;` |
|       5 |   392 |  |
|       - |   393 | `/*` |
|       - |   394 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   395 | ` * in the constant table.` |
|       - |   396 | ` */` |
|  116372 |   397 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   398 |  |
|       - |   399 | `	ph7_value *pObj;` |
|  116377 |   400 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   401 | `	/* Reserve a new constant */` |
|  116377 |   402 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  116377 |   403 | `	if( pObj == 0 ){` |
|     ! 0 |   404 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   405 | `		return 0;` |
|       - |   406 | `	}` |
|  116377 |   407 | `	*pIdx = nIdx;` |
|       - |   408 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   409 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   410 | `	 */` |
|  116377 |   411 | `	return pObj;` |
|   58191 |   412 |  |
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
|  417546 |   427 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   428 |  |
|       - |   429 | `	VmCallArgMap *pMap;` |
|  417551 |   430 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   431 | `	if( p3 == 0 ){` |
|      31 |   432 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   433 | `		if( pMap == 0 ) return 0;` |
|      31 |   434 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   435 | `		p3 = (void *)pMap;` |
|      14 |   436 | `	}` |
|      33 |   437 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   438 | `	return p3;` |
|  208778 |   439 |  |
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
|  117036 |   498 | `static int GenStateFindBadNumericSeparator(` |
|       - |   499 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   500 |  |
|  117041 |   501 | `	const char *z = pRaw->zString;` |
|  117041 |   502 | `	sxu32 n = pRaw->nByte;` |
|  117041 |   503 | `	int base = 10;` |
|       - |   504 | `	sxu32 i, start;` |
|  117041 |   505 | `	if( n < 2 ) return 0;` |
|    9853 |   506 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   507 | `		base = 16;` |
|    9818 |   508 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   509 | `		base = 2;` |
|     139 |   510 | `	}` |
|   35925 |   511 | `	for( i = 0; i < n; ++i ){` |
|   26091 |   512 | `		if( z[i] != '_' ) continue;` |
|     546 |   513 | `		if( i > 0 && i + 1 < n` |
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
|    9839 |   529 | `	return 0;` |
|   58523 |   530 |  |
|       - |   531 | `/*` |
|       - |   532 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   533 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   534 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   535 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   536 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   537 | ` * so callers can bail from the current construct).` |
|       - |   538 | ` */` |
|  117036 |   539 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   540 |  |
|  117041 |   541 | `	const char *zBad = 0;` |
|  117041 |   542 | `	sxu32 nBad = 0;` |
|       - |   543 | `	SyString sBad;` |
|       - |   544 | `	sxi32 rc;` |
|  117041 |   545 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  117027 |   546 | `		return SXRET_OK;` |
|       - |   547 | `	}` |
|      18 |   548 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   549 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   550 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   551 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   552 | `		return SXERR_ABORT;` |
|       - |   553 | `	}` |
|      18 |   554 | `	return SXERR_SYNTAX;` |
|   58523 |   555 |  |
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
|  117022 |   572 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   573 | `	SyMemBackend *pAlloc,` |
|       - |   574 | `	const SyString *pToken,` |
|       - |   575 | `	char *zScratch, sxu32 nScratch,` |
|       - |   576 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   577 |  |
|       - |   578 | `	sxu32 i, j;` |
|  117027 |   579 | `	int hasUnderscore = 0;` |
|       - |   580 | `	char *zBuf;` |
|  117027 |   581 | `	*pzAlloc = 0;` |
|  248221 |   582 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  131451 |   583 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   65602 |   584 | `	}` |
|  117027 |   585 | `	if( !hasUnderscore ){` |
|  116775 |   586 | `		SyStringDupPtr(pOut, pToken);` |
|  116775 |   587 | `		return SXRET_OK;` |
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
|   58516 |   604 |  |
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
|  117008 |   621 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   622 |  |
|  117013 |   623 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  117013 |   624 | `	sxu32 nIdx = 0;` |
|       - |   625 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  117013 |   626 | `	char *zAlloc = 0;` |
|       - |   627 | `	SyString sNum;` |
|       - |   628 | `	sxi32 rc;` |
|   58504 |   629 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  117013 |   630 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  117013 |   631 | `	if( rc != SXRET_OK ){` |
|      14 |   632 | `		return rc;` |
|       - |   633 | `	}` |
|  175502 |   634 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   58499 |   635 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  117003 |   636 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   637 | `		return SXERR_ABORT;` |
|       - |   638 | `	}` |
|  117003 |   639 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   640 | `		ph7_value *pObj;` |
|       - |   641 | `		sxi64 iValue;` |
|  116377 |   642 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  116377 |   643 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  116377 |   644 | `		if( pObj == 0 ){` |
|     ! 0 |   645 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   646 | `			return SXERR_ABORT;` |
|       - |   647 | `		}` |
|  116377 |   648 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   58191 |   649 | `	}else{` |
|       - |   650 | `		/* Real number */` |
|       - |   651 | `		ph7_value *pObj;` |
|       - |   652 | `		/* Reserve a new constant */` |
|     631 |   653 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     631 |   654 | `		if( pObj == 0 ){` |
|     ! 0 |   655 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   656 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   657 | `			return SXERR_ABORT;` |
|       - |   658 | `		}` |
|     631 |   659 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     631 |   660 | `		PH7_MemObjToReal(pObj);` |
|       - |   661 | `	}` |
|  117003 |   662 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   663 | `	/* Emit the load constant instruction */` |
|  117003 |   664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   665 | `	/* Node successfully compiled */` |
|  117003 |   666 | `	return SXRET_OK;` |
|   58509 |   667 |  |
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
|   83816 |   679 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   680 |  |
|   83821 |   681 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   682 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   683 | `	ph7_value *pObj;` |
|       - |   684 | `	sxu32 nIdx;` |
|   83821 |   685 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   686 | `	/* Delimit the string */` |
|   83821 |   687 | `	zIn  = pStr->zString;` |
|   83821 |   688 | `	zEnd = &zIn[pStr->nByte];` |
|   83821 |   689 | `	if( zIn >= zEnd ){` |
|       - |   690 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   691 | `		 * rather than reserving a new object each time. */` |
|    6783 |   692 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    6783 |   693 | `		return SXRET_OK;` |
|       - |   694 | `	}` |
|   77043 |   695 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   696 | `		/* Already processed,emit the load constant instruction` |
|       - |   697 | `		 * and return.` |
|       - |   698 | `		 */` |
|   30541 |   699 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   30541 |   700 | `		return SXRET_OK;` |
|       - |   701 | `	}` |
|       - |   702 | `	/* Reserve a new constant */` |
|   46507 |   703 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   46507 |   704 | `	if( pObj == 0 ){` |
|     ! 0 |   705 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   706 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   707 | `		return SXERR_ABORT;` |
|       - |   708 | `	}` |
|   46507 |   709 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   710 | `	/* Compile the node */` |
|   46554 |   711 | `	for(;;){` |
|   93113 |   712 | `		if( zIn >= zEnd ){` |
|       - |   713 | `			/* End of input */` |
|   46507 |   714 | `			break;` |
|       - |   715 | `		}` |
|   46611 |   716 | `		zCur = zIn;` |
|  734063 |   717 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  687457 |   718 | `			zIn++;` |
|       5 |   719 | `		}` |
|   46611 |   720 | `		if( zIn > zCur ){` |
|       - |   721 | `			/* Append raw contents*/` |
|   46587 |   722 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   23291 |   723 | `		}` |
|   46611 |   724 | `		zIn++;` |
|   46611 |   725 | `		if( zIn < zEnd ){` |
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
|   46611 |   740 | `		zIn++;` |
|       5 |   741 | `	}` |
|       - |   742 | `	/* Emit the load constant instruction */` |
|   46507 |   743 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   46507 |   744 | `	if( pStr->nByte < 1024 ){` |
|       - |   745 | `		/* Install in the literal table */` |
|   46507 |   746 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   23251 |   747 | `	}` |
|       - |   748 | `	/* Node successfully compiled */` |
|   46507 |   749 | `	return SXRET_OK;` |
|   41913 |   750 |  |
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
|     110 |   769 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       5 |   770 |  |
|     115 |   771 | `	SyString *pIn = &pGen->pIn->sData;` |
|     115 |   772 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   773 | `	const char *zPrefix;` |
|       - |   774 | `	const char *z, *zEnd;` |
|       - |   775 | `	char *zBuf, *zDst;` |
|     115 |   776 | `	if( nIndent == 0 ){` |
|       - |   777 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      69 |   778 | `		*pOut = *pIn;` |
|      69 |   779 | `		return SXRET_OK;` |
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
|      60 |   847 |  |
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
|      46 |   862 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |   863 |  |
|       - |   864 | `	SyString sStripped;` |
|       - |   865 | `	SyString *pStr;` |
|       - |   866 | `	ph7_value *pObj;` |
|       - |   867 | `	sxu32 nIdx;` |
|       - |   868 | `	sxi32 rc;` |
|      50 |   869 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      50 |   870 | `	if( rc != SXRET_OK ){` |
|       6 |   871 | `		return rc;` |
|       - |   872 | `	}` |
|      44 |   873 | `	pStr = &sStripped;` |
|      44 |   874 | `	nIdx = 0; /* Prevent compiler warning */` |
|      44 |   875 | `	if( pStr->nByte <= 0 ){` |
|       - |   876 | `		/* Empty string,load NULL */` |
|       7 |   877 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |   878 | `		return SXRET_OK;` |
|       - |   879 | `	}` |
|       - |   880 | `	/* Reserve a new constant */` |
|      38 |   881 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      38 |   882 | `	if( pObj == 0 ){` |
|     ! 0 |   883 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   884 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   885 | `		return SXERR_ABORT;` |
|       - |   886 | `	}` |
|       - |   887 | `	/* No processing is done here, simply a memcpy() operation */` |
|      38 |   888 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |   889 | `	/* Emit the load constant instruction */` |
|      38 |   890 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   891 | `	/* Node successfully compiled */` |
|      38 |   892 | `	return SXRET_OK;` |
|      27 |   893 |  |
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
|    2156 |   916 | `static sxi32 GenStateProcessStringExpression(` |
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
|    2161 |   927 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   928 | `	/* Preallocate some slots */` |
|    2161 |   929 | `	SySetAlloc(&sToken,0x08);` |
|       - |   930 | `	/* Tokenize the text */` |
|    2161 |   931 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   932 | `	/* Swap delimiter */` |
|    2161 |   933 | `	pTmpIn  = pGen->pIn;` |
|    2161 |   934 | `	pTmpEnd = pGen->pEnd;` |
|    2161 |   935 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2161 |   936 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   937 | `	/* Compile the expression */` |
|    2161 |   938 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   939 | `	/* Restore token stream */` |
|    2161 |   940 | `	pGen->pIn  = pTmpIn;` |
|    2161 |   941 | `	pGen->pEnd = pTmpEnd;` |
|       - |   942 | `	/* Release the token set */` |
|    2161 |   943 | `	SySetRelease(&sToken);` |
|       - |   944 | `	/* Compilation result */` |
|    2161 |   945 | `	return rc;` |
|       5 |   946 |  |
|       - |   947 | `/*` |
|       - |   948 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   949 | ` */` |
|   23612 |   950 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   951 |  |
|       - |   952 | `	ph7_value *pConstObj;` |
|   23617 |   953 | `	sxu32 nIdx = 0;` |
|       - |   954 | `	/* Reserve a new constant */` |
|   23617 |   955 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   23617 |   956 | `	if( pConstObj == 0 ){` |
|     ! 0 |   957 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   958 | `		return 0;` |
|       - |   959 | `	}` |
|   23617 |   960 | `	(*pCount)++;` |
|   23617 |   961 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   962 | `	/* Emit the load constant instruction */` |
|   23617 |   963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   23617 |   964 | `	return pConstObj;` |
|   11811 |   965 |  |
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
|   22164 |  1004 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1005 |  |
|   22169 |  1006 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1007 | `	const char *zIn,*zCur,*zEnd;` |
|   22169 |  1008 | `	ph7_value *pObj = 0;` |
|       - |  1009 | `	sxi32 iCons;` |
|       - |  1010 | `	sxi32 rc;` |
|       - |  1011 | `	/* Delimit the string */` |
|   22169 |  1012 | `	zIn  = pStr->zString;` |
|   22169 |  1013 | `	zEnd = &zIn[pStr->nByte];` |
|   22169 |  1014 | `	if( zIn >= zEnd ){` |
|       - |  1015 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1016 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1017 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1018 | `		 */` |
|     311 |  1019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     311 |  1020 | `		return SXRET_OK;` |
|       - |  1021 | `	}` |
|   21863 |  1022 | `	zCur = 0;` |
|       - |  1023 | `	/* Compile the node */` |
|   21863 |  1024 | `	iCons = 0;` |
|   12007 |  1025 | `	for(;;){` |
|   35939 |  1026 | `		zCur = zIn;` |
|  173041 |  1027 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  139263 |  1028 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1029 | `				break;` |
|  139139 |  1030 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2036 |  1031 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1019 |  1032 | `					break;` |
|       - |  1033 | `			}` |
|  137107 |  1034 | `			zIn++;` |
|       5 |  1035 | `		}` |
|   35939 |  1036 | `		if( zIn > zCur ){` |
|   16891 |  1037 | `			if( pObj == 0 ){` |
|   16417 |  1038 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   16417 |  1039 | `				if( pObj == 0 ){` |
|     ! 0 |  1040 | `					return SXERR_ABORT;` |
|       - |  1041 | `				}` |
|    8206 |  1042 | `			}` |
|   16891 |  1043 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    8443 |  1044 | `		}` |
|   35939 |  1045 | `		if( zIn >= zEnd ){` |
|   21863 |  1046 | `			break;` |
|       - |  1047 | `		}` |
|   14081 |  1048 | `		if( zIn[0] == '\\' ){` |
|   11925 |  1049 | `			const char *zPtr = 0;` |
|       - |  1050 | `			sxu32 n;` |
|   11925 |  1051 | `			zIn++;` |
|   11925 |  1052 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1053 | `				break;` |
|       - |  1054 | `			}` |
|   11925 |  1055 | `			if( pObj == 0 ){` |
|    7205 |  1056 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    7205 |  1057 | `				if( pObj == 0 ){` |
|     ! 0 |  1058 | `					return SXERR_ABORT;` |
|       - |  1059 | `				}` |
|    3600 |  1060 | `			}` |
|   11925 |  1061 | `			n = sizeof(char); /* size of conversion */` |
|   11925 |  1062 | `			switch( zIn[0] ){` |
|       7 |  1063 | `			case '$':` |
|       - |  1064 | `				/* Dollar sign */` |
|      15 |  1065 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|      15 |  1066 | `				break;` |
|      49 |  1067 | `			case '\\':` |
|       - |  1068 | `				/* A literal backslash */` |
|     102 |  1069 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     102 |  1070 | `				break;` |
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
|    5477 |  1083 | `			case 'n':` |
|       - |  1084 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   10959 |  1085 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   10959 |  1086 | `				break;` |
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
|     108 |  1103 | `			case '"':` |
|       - |  1104 | `				/* Double quote */` |
|     221 |  1105 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     221 |  1106 | `				break;` |
|      10 |  1107 | `			case '0':` |
|       - |  1108 | `				/* NUL byte */` |
|      21 |  1109 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      21 |  1110 | `				break;` |
|     228 |  1111 | `			case 'x':` |
|     457 |  1112 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  1113 | `					int c;` |
|       - |  1114 | `					/* Hex digit */` |
|     443 |  1115 | `					c = SyHexToint(zIn[1]) << 4;` |
|     443 |  1116 | `					if( &zIn[2] < zEnd ){` |
|     443 |  1117 | `						c +=  SyHexToint(zIn[2]);` |
|     221 |  1118 | `					}` |
|       - |  1119 | `					/* Output char */` |
|     443 |  1120 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     443 |  1121 | `					n += sizeof(char) * 2;` |
|     222 |  1122 | `				}else{` |
|       - |  1123 | `					/* Output literal character  */` |
|      15 |  1124 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  1125 | `				}` |
|     457 |  1126 | `				break;` |
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
|   11925 |  1154 | `			zIn += n;` |
|   11925 |  1155 | `			continue;` |
|       - |  1156 | `		}` |
|    2161 |  1157 | `		if( zIn[0] == '{' ){` |
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
|    2033 |  1191 | `			const char *zExpr = zIn;` |
|       - |  1192 | `			/* Assemble variable name */` |
|    1023 |  1193 | `			for(;;){` |
|       - |  1194 | `				/* Jump leading dollars */` |
|    4079 |  1195 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2033 |  1196 | `					zIn++;` |
|       5 |  1197 | `				}` |
|    1023 |  1198 | `				for(;;){` |
|   11536 |  1199 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8467 |  1200 | `						zIn++;` |
|       5 |  1201 | `					}` |
|    2051 |  1202 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1203 | `						/* UTF-8 stream */` |
|     ! 0 |  1204 | `						zIn++;` |
|     ! 0 |  1205 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1206 | `							zIn++;` |
|     ! 0 |  1207 | `						}` |
|     ! 0 |  1208 | `						continue;` |
|       - |  1209 | `					}` |
|    2051 |  1210 | `					break;` |
|     ! 0 |  1211 | `				}` |
|    2051 |  1212 | `				if( zIn >= zEnd ){` |
|     172 |  1213 | `					break;` |
|       - |  1214 | `				}` |
|    1883 |  1215 | `				if( zIn[0] == '[' ){` |
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
|    1873 |  1233 | `				}else if(zIn[0] == '{' ){` |
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
|    1869 |  1251 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1252 | `					/* Member access operator '->' */` |
|      21 |  1253 | `					zIn += 2;` |
|    1860 |  1254 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1255 | `					/* Static member access operator '::' */` |
|     ! 0 |  1256 | `					zIn += 2;` |
|     ! 0 |  1257 | `				}else{` |
|     928 |  1258 | `					break;` |
|       - |  1259 | `				}` |
|       3 |  1260 | `			}` |
|       - |  1261 | `			/* Process the expression */` |
|    2033 |  1262 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2033 |  1263 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1264 | `				return SXERR_ABORT;` |
|       - |  1265 | `			}` |
|    2033 |  1266 | `			if( rc != SXERR_EMPTY ){` |
|    2031 |  1267 | `				++iCons;` |
|    1013 |  1268 | `			}` |
|       - |  1269 | `		}` |
|       - |  1270 | `		/* Invalidate the previously used constant */` |
|    2161 |  1271 | `		pObj = 0;` |
|       5 |  1272 | `	}/*for(;;)*/` |
|   21863 |  1273 | `	if( iCons > 1 ){` |
|       - |  1274 | `		/* Concatenate all compiled constants */` |
|    1609 |  1275 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     802 |  1276 | `	}` |
|       - |  1277 | `	/* Node successfully compiled */` |
|   21863 |  1278 | `	return SXRET_OK;` |
|   11087 |  1279 |  |
|       - |  1280 | `/*` |
|       - |  1281 | ` * Compile a double quoted string.` |
|       - |  1282 | ` *  See the block-comment above for more information.` |
|       - |  1283 | ` */` |
|   22104 |  1284 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1285 |  |
|       - |  1286 | `	sxi32 rc;` |
|   22109 |  1287 | `	rc = GenStateCompileString(&(*pGen));` |
|   11052 |  1288 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1289 | `	/* Compilation result */` |
|   22109 |  1290 | `	return rc;` |
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
|   20500 |  1334 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   20505 |  1345 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1346 | `	/* Compile the expression*/` |
|   20505 |  1347 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1348 | `	/* Restore token stream */` |
|   20505 |  1349 | `	RE_SWAP_DELIMITER(pGen);` |
|   20505 |  1350 | `	return rc;` |
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
|      14 |  1364 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
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
|       - |  1385 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - |  1386 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - |  1387 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - |  1388 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - |  1389 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - |  1390 | ` */` |
|   22628 |  1391 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1392 |  |
|   22633 |  1393 | `	SyToken *pCur = pStart;` |
|   22633 |  1394 | `	sxi32 iNest = 0;` |
|   63959 |  1395 | `	while( pCur < pEnd ){` |
|   46473 |  1396 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5143 |  1397 | `			return pCur;` |
|       - |  1398 | `		}` |
|       - |  1399 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1400 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1401 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1402 | `		 */` |
|   41335 |  1403 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      95 |  1404 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      95 |  1405 | `			SyToken *pFn = pCur;` |
|      92 |  1406 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|     ! 0 |  1407 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3 |  1408 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1409 | `				pFn = &pCur[1];` |
|     ! 0 |  1410 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1411 | `			}` |
|      95 |  1412 | `			if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1413 | `				pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1414 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1415 | `					pCur++;` |
|     ! 0 |  1416 | `				}` |
|       5 |  1417 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1418 | `					pCur++;` |
|       5 |  1419 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1420 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1421 | `					if( pCur < pEnd ){` |
|       5 |  1422 | `						pCur++;` |
|       2 |  1423 | `					}` |
|       2 |  1424 | `				}` |
|       5 |  1425 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1426 | `					pCur++;` |
|     ! 0 |  1427 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1428 | `						&& pCur->sData.nByte == 1` |
|     ! 0 |  1429 | `						&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1430 | `						pCur++;` |
|     ! 0 |  1431 | `					}` |
|     ! 0 |  1432 | `					if( pCur < pEnd` |
|     ! 0 |  1433 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1434 | `						pCur++;` |
|     ! 0 |  1435 | `					}` |
|     ! 0 |  1436 | `				}` |
|       - |  1437 | `				/* The rest of the entry is the arrow-function body — no outer` |
|       - |  1438 | `				 * key to extract. */` |
|       5 |  1439 | `				return pEnd;` |
|       - |  1440 | `			}` |
|       - |  1441 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|       - |  1442 | `			 * entry separator. Skip past the full match span. */` |
|      91 |  1443 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1444 | `				pCur++; /* past 'match' */` |
|       3 |  1445 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1446 | `					pCur++;` |
|       3 |  1447 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1448 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1449 | `					if( pCur < pEnd ){` |
|       3 |  1450 | `						pCur++;` |
|       1 |  1451 | `					}` |
|       1 |  1452 | `				}` |
|       3 |  1453 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1454 | `					pCur++;` |
|       3 |  1455 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1456 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1457 | `					if( pCur < pEnd ){` |
|       3 |  1458 | `						pCur++;` |
|       1 |  1459 | `					}` |
|       1 |  1460 | `				}` |
|       3 |  1461 | `				continue;` |
|       - |  1462 | `			}` |
|      43 |  1463 | `		}` |
|   41329 |  1464 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     324 |  1465 | `			iNest++;` |
|   41169 |  1466 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1467 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1468 | `			 * parser will shortly detect any syntax error. */` |
|     324 |  1469 | `			iNest--;` |
|     160 |  1470 | `		}` |
|   41329 |  1471 | `		pCur++;` |
|       5 |  1472 | `	}` |
|   17491 |  1473 | `	return pEnd;` |
|   11319 |  1474 |  |
|       - |  1475 | `/*` |
|       - |  1476 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1477 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1478 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1479 | ` */` |
|   29174 |  1480 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1481 |  |
|       - |  1482 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1483 | `	SyToken *pKey,*pCur;` |
|   29179 |  1484 | `	sxi32 iEmitRef = 0;` |
|   29179 |  1485 | `	sxi32 iSpread = 0;` |
|   29179 |  1486 | `	sxi32 nPair = 0;` |
|       - |  1487 | `	sxi32 rc;` |
|   29179 |  1488 | `	xValidator = 0;` |
|   23957 |  1489 | `	for(;;){` |
|       - |  1490 | `		/* Jump leading commas */` |
|   54447 |  1491 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    6533 |  1492 | `			pGen->pIn++;` |
|       5 |  1493 | `		}` |
|   47919 |  1494 | `		pCur = pGen->pIn;` |
|   47919 |  1495 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1496 | `			/* No more entry to process */` |
|   29163 |  1497 | `			break;` |
|       - |  1498 | `		}` |
|   18761 |  1499 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1500 | `			continue;` |
|       - |  1501 | `		}` |
|       - |  1502 | `		/* Compile the key if available */` |
|   18761 |  1503 | `		pKey = pCur;` |
|   18761 |  1504 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   18761 |  1505 | `		rc = SXERR_EMPTY;` |
|   18761 |  1506 | `		if( pCur < pGen->pIn ){` |
|    1581 |  1507 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1508 | `				/* Missing value */` |
|      13 |  1509 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1510 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1511 | `					return SXERR_ABORT;` |
|       - |  1512 | `				}` |
|      13 |  1513 | `				return SXRET_OK;` |
|       - |  1514 | `			}` |
|       - |  1515 | `			/* Compile the expression holding the key */` |
|    1571 |  1516 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1517 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1571 |  1518 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1519 | `				return SXERR_ABORT;` |
|       - |  1520 | `			}` |
|    1571 |  1521 | `			pCur++; /* Jump the '=>' operator */` |
|   17968 |  1522 | `		}else if( pKey == pCur ){` |
|       - |  1523 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1524 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1525 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1526 | `		}else{` |
|       - |  1527 | `			/* Reset back the cursor and point to the entry value */` |
|   17185 |  1528 | `			pCur = pKey;` |
|       - |  1529 | `		}` |
|   18751 |  1530 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1531 | `			/* No available key,load NULL */` |
|   17187 |  1532 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    8591 |  1533 | `		}` |
|   18751 |  1534 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1535 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      45 |  1536 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      45 |  1537 | `			iEmitRef = 1;` |
|      45 |  1538 | `			pCur++; /* Jump the '&' token */` |
|      45 |  1539 | `			if( pCur >= pGen->pIn ){` |
|       - |  1540 | `				/* Missing value */` |
|       3 |  1541 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1542 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1543 | `					return SXERR_ABORT;` |
|       - |  1544 | `				}` |
|       3 |  1545 | `				return SXRET_OK;` |
|       - |  1546 | `			}` |
|      19 |  1547 | `		}` |
|       - |  1548 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - |  1549 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - |  1550 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - |  1551 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - |  1552 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   18749 |  1553 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   18749 |  1554 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - |  1555 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - |  1556 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - |  1557 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - |  1558 | `			 * output is engine-portable. */` |
|       6 |  1559 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - |  1560 | `				"syntax error, unexpected token \"...\"");` |
|       6 |  1561 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1562 | `				return SXERR_ABORT;` |
|       - |  1563 | `			}` |
|       6 |  1564 | `			return SXRET_OK;` |
|       - |  1565 | `		}` |
|       - |  1566 | `		/* Compile indice value */` |
|   18745 |  1567 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   18745 |  1568 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1569 | `			return SXERR_ABORT;` |
|       - |  1570 | `		}` |
|   18745 |  1571 | `		if( iSpread ){` |
|       - |  1572 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1573 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   18714 |  1574 | `		}else if( iEmitRef ){` |
|       - |  1575 | `			/* Emit the load reference instruction */` |
|      40 |  1576 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1577 | `		}` |
|   18745 |  1578 | `		xValidator = 0;` |
|   18745 |  1579 | `		iEmitRef = 0;` |
|   18745 |  1580 | `		iSpread = 0;` |
|   18745 |  1581 | `		nPair++;` |
|       5 |  1582 | `	}` |
|       - |  1583 | `	/* Emit the load map instruction */` |
|   29163 |  1584 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1585 | `	/* Node successfully compiled */` |
|   29163 |  1586 | `	return SXRET_OK;` |
|   14592 |  1587 |  |
|       - |  1588 | `/*` |
|       - |  1589 | ` * Compile the 'array' language construct.` |
|       - |  1590 | ` *	 According to the PHP language reference manual` |
|       - |  1591 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1592 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1593 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1594 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1595 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1596 | ` */` |
|   28256 |  1597 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1598 |  |
|       - |  1599 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   28261 |  1600 | `	pGen->pIn += 2;` |
|   28261 |  1601 | `	pGen->pEnd--;` |
|   14128 |  1602 | `	SXUNUSED(iCompileFlag);` |
|   28261 |  1603 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1604 |  |
|       - |  1605 | `/*` |
|       - |  1606 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1607 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1608 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1609 | ` */` |
|     918 |  1610 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1611 |  |
|       - |  1612 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     923 |  1613 | `	pGen->pIn++;` |
|     923 |  1614 | `	pGen->pEnd--;` |
|     459 |  1615 | `	SXUNUSED(iCompileFlag);` |
|     923 |  1616 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1617 |  |
|       - |  1618 | `/*` |
|       - |  1619 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1620 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1621 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1622 | ` * error message.` |
|       - |  1623 | ` * See the routine responible of compiling the list language construct` |
|       - |  1624 | ` * for more inforation.` |
|       - |  1625 | ` */` |
|     158 |  1626 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1627 |  |
|     162 |  1628 | `	sxi32 rc = SXRET_OK;` |
|     162 |  1629 | `	if( pRoot->pOp ){` |
|       4 |  1630 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       2 |  1631 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1632 | `				/* Unexpected expression */` |
|     ! 0 |  1633 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1634 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1635 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1636 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1637 | `				}` |
|       1 |  1638 | `		}` |
|     160 |  1639 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1640 | `		/* Unexpected expression */` |
|       6 |  1641 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1642 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1643 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1644 | `			rc = SXERR_INVALID;` |
|       2 |  1645 | `		}` |
|       2 |  1646 | `	}` |
|     162 |  1647 | `	return rc;` |
|       4 |  1648 |  |
|       - |  1649 | `/*` |
|       - |  1650 | ` * Compile the 'list' language construct.` |
|       - |  1651 | ` *  According to the PHP language reference` |
|       - |  1652 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1653 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1654 | ` *  Description` |
|       - |  1655 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1656 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1657 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1658 | ` *  Parameters` |
|       - |  1659 | ` *   $varname: A variable.` |
|       - |  1660 | ` *  Return Values` |
|       - |  1661 | ` *   The assigned array.` |
|       - |  1662 | ` */` |
|       - |  1663 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1664 | `struct NestedListEntry {` |
|       - |  1665 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1666 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1667 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1668 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1669 | `};` |
|       - |  1670 | `/*` |
|       - |  1671 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|       - |  1672 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|       - |  1673 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|       - |  1674 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|       - |  1675 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|       - |  1676 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|       - |  1677 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|       - |  1678 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|       - |  1679 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|       - |  1680 | ` */` |
|      22 |  1681 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       1 |  1682 |  |
|       - |  1683 | `	SyToken *pNext;` |
|       - |  1684 | `	sxi32 rc;` |
|      53 |  1685 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - |  1686 | `		SyToken *pArrow,*pTarget;` |
|       - |  1687 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      31 |  1688 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      31 |  1689 | `		pTarget = &pArrow[1];` |
|      31 |  1690 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - |  1691 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - |  1692 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 |  1693 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1694 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1695 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1696 | `		}` |
|       - |  1697 | `		/* DUP the source array (it is on the stack top) */` |
|      31 |  1698 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1699 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      31 |  1700 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      31 |  1701 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1702 | `			return SXERR_ABORT;` |
|       - |  1703 | `		}` |
|       - |  1704 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - |  1705 | `		 * iP2=0 is a read context: a missing key loads NULL silently, matching a` |
|       - |  1706 | ``		 * normal `$arr[$k]` read. (PHP also emits an "Undefined array key"`` |
|       - |  1707 | `		 * warning here; PHL omits it, like its other subscript reads — §3.7.) */` |
|      31 |  1708 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,0,0,0);` |
|      31 |  1709 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      28 |  1710 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      15 |  1711 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - |  1712 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - |  1713 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - |  1714 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 |  1715 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 |  1716 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 |  1717 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 |  1718 | `			pGen->pIn = pTarget;` |
|       5 |  1719 | `			pGen->pEnd = pNext;` |
|       5 |  1720 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 |  1721 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 |  1722 | `			pGen->pIn = pSavedIn;` |
|       5 |  1723 | `			pGen->pEnd = pSavedEnd;` |
|       5 |  1724 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1725 | `				return SXERR_ABORT;` |
|       - |  1726 | `			}` |
|       5 |  1727 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 |  1728 | `		}else{` |
|       - |  1729 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|       - |  1730 | `			 * is already on the stack as the value; compiling the target appends` |
|       - |  1731 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|       - |  1732 | `			 * assignment does. */` |
|       - |  1733 | `			VmInstr *pInstr;` |
|      27 |  1734 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|      27 |  1735 | `			sxi32 iP1 = 0, iP2 = 0;` |
|      27 |  1736 | `			void *p3 = 0;` |
|      27 |  1737 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|       - |  1738 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      27 |  1739 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  1740 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1741 | `			}` |
|      27 |  1742 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      27 |  1743 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       3 |  1744 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|      26 |  1745 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 |  1746 | `					iVmOp = PH7_OP_STORE_IDX;` |
|       3 |  1747 | `					iP1 = pInstr->iP1;` |
|       3 |  1748 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 |  1749 | `				}else{` |
|      23 |  1750 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|      23 |  1751 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  1752 | `				}` |
|      13 |  1753 | `			}` |
|      27 |  1754 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - |  1755 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|       - |  1756 | `			 * source array is back on top for the next entry. */` |
|      27 |  1757 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       - |  1758 | `		}` |
|      31 |  1759 | `		pGen->pIn = &pNext[1];` |
|       1 |  1760 | `	}` |
|      23 |  1761 | `	return SXRET_OK;` |
|      12 |  1762 |  |
|       - |  1763 | `/*` |
|       - |  1764 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1765 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1766 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1767 | ` */` |
|      98 |  1768 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       4 |  1769 |  |
|       - |  1770 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1771 | `	SyToken *pNext;` |
|       - |  1772 | `	SyToken *pClassifyIn;` |
|     102 |  1773 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - |  1774 | `	sxi32 nExpr;` |
|       - |  1775 | `	sxi32 rc;` |
|       - |  1776 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - |  1777 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - |  1778 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - |  1779 | `	 * list. */` |
|     102 |  1780 | `	pClassifyIn = pGen->pIn;` |
|     290 |  1781 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     192 |  1782 | `		if( pGen->pIn >= pNext ){` |
|      13 |  1783 | `			nEmpty++;` |
|     186 |  1784 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      31 |  1785 | `			nKeyed++;` |
|      16 |  1786 | `		}else{` |
|     150 |  1787 | `			nPositional++;` |
|       - |  1788 | `		}` |
|     192 |  1789 | `		pGen->pIn = &pNext[1];` |
|       4 |  1790 | `	}` |
|     102 |  1791 | `	pGen->pIn = pClassifyIn;` |
|     102 |  1792 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 |  1793 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1794 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1795 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1796 | `	}` |
|     102 |  1797 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 |  1798 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1799 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 |  1800 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1801 | `	}` |
|     102 |  1802 | `	if( nKeyed > 0 ){` |
|      23 |  1803 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - |  1804 | `	}` |
|      80 |  1805 | `	nExpr = 0;` |
|      80 |  1806 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     238 |  1807 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     162 |  1808 | `		if( pGen->pIn < pNext ){` |
|       - |  1809 | `			/* Check for nested list() */` |
|     150 |  1810 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1811 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1812 | `				/* Record this nested list for post-processing */` |
|       3 |  1813 | `				SyToken *pListEnd = 0;` |
|       3 |  1814 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1815 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1816 | `				}` |
|       3 |  1817 | `				if( pListEnd ){` |
|       - |  1818 | `					struct NestedListEntry sEntry;` |
|       3 |  1819 | `					sEntry.nIndex = nExpr;` |
|       3 |  1820 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1821 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1822 | `					sEntry.isShort = 0;` |
|       3 |  1823 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1824 | `				}` |
|       - |  1825 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1826 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     149 |  1827 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1828 | `				/* Nested short destructuring [...] */` |
|      13 |  1829 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1830 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1831 | `				if( pBracketEnd ){` |
|       - |  1832 | `					struct NestedListEntry sEntry;` |
|      13 |  1833 | `					sEntry.nIndex = nExpr;` |
|      13 |  1834 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1835 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1836 | `					sEntry.isShort = 1;` |
|      13 |  1837 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1838 | `				}` |
|       - |  1839 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1840 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1841 | `			}else{` |
|       - |  1842 | `				/* Compile the expression holding the variable */` |
|     136 |  1843 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     136 |  1844 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1845 | `					SySetRelease(&sNested);` |
|     ! 0 |  1846 | `					return SXRET_OK;` |
|       - |  1847 | `				}` |
|       - |  1848 | `			}` |
|      77 |  1849 | `		}else{` |
|       - |  1850 | `			/* Empty entry,load NULL */` |
|      13 |  1851 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1852 | `		}` |
|     162 |  1853 | `		nExpr++;` |
|       - |  1854 | `		/* Advance the stream cursor */` |
|     162 |  1855 | `		pGen->pIn = &pNext[1];` |
|       4 |  1856 | `	}` |
|       - |  1857 | `	/* Emit the LOAD_LIST instruction */` |
|      80 |  1858 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1859 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1860 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1861 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1862 | `	 */` |
|      80 |  1863 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1864 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1865 | `		sxu32 i;` |
|      27 |  1866 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1867 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1868 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1869 | `			ph7_value *pIdx;` |
|       - |  1870 | `			sxu32 nConstIdx;` |
|       - |  1871 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1872 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1873 | `			/* Push the integer index for this nested entry */` |
|      15 |  1874 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1875 | `			if( pIdx == 0 ){` |
|     ! 0 |  1876 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1877 | `				SySetRelease(&sNested);` |
|     ! 0 |  1878 | `				return SXERR_ABORT;` |
|       - |  1879 | `			}` |
|      15 |  1880 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1881 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1882 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1883 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1884 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1885 | `			 */` |
|      15 |  1886 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1887 | `			/* Recursively compile the inner list */` |
|      15 |  1888 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1889 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1890 | `			if( apNested[i].isShort ){` |
|      13 |  1891 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1892 | `			}else{` |
|       3 |  1893 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1894 | `			}` |
|      15 |  1895 | `			pGen->pIn = pSavedIn;` |
|      15 |  1896 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1897 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1898 | `				SySetRelease(&sNested);` |
|     ! 0 |  1899 | `				return SXERR_ABORT;` |
|       - |  1900 | `			}` |
|       - |  1901 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1902 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1903 | `		}` |
|       6 |  1904 | `	}` |
|      80 |  1905 | `	SySetRelease(&sNested);` |
|       - |  1906 | `	/* Node successfully compiled */` |
|      80 |  1907 | `	return SXRET_OK;` |
|      53 |  1908 |  |
|      34 |  1909 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1910 |  |
|       - |  1911 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      36 |  1912 | `	pGen->pIn += 2;` |
|      36 |  1913 | `	pGen->pEnd--;` |
|      17 |  1914 | `	SXUNUSED(iCompileFlag);` |
|      36 |  1915 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1916 |  |
|      64 |  1917 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       3 |  1918 |  |
|       - |  1919 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      67 |  1920 | `	pGen->pIn++;` |
|      67 |  1921 | `	pGen->pEnd--;` |
|      32 |  1922 | `	SXUNUSED(iCompileFlag);` |
|      67 |  1923 | `	return GenStateCompileListBody(pGen);` |
|       3 |  1924 |  |
|       - |  1925 | `/* Forward declarations */` |
|       - |  1926 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  1927 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  1928 | `static int GenStateIsReadonly(SyToken *pTok);` |
|       - |  1929 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|       - |  1930 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|       - |  1931 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1932 | `/*` |
|       - |  1933 | ` * Compile an annoynmous function or a closure.` |
|       - |  1934 | ` * According to the PHP language reference` |
|       - |  1935 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1936 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1937 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1938 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1939 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1940 | ` *  Example Anonymous function variable assignment example` |
|       - |  1941 | ` * <?php` |
|       - |  1942 | ` * $greet = function($name)` |
|       - |  1943 | ` * {` |
|       - |  1944 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1945 | ` * };` |
|       - |  1946 | ` * $greet('World');` |
|       - |  1947 | ` * $greet('PHP');` |
|       - |  1948 | ` * ?>` |
|       - |  1949 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1950 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1951 | ` */` |
|     252 |  1952 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1953 |  |
|       - |  1954 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1955 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1956 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1957 | `							  * one thread is allowed to compile the script.` |
|       - |  1958 | `						      */` |
|       - |  1959 | `	ph7_value *pObj;` |
|       - |  1960 | `	SyString sName;` |
|       - |  1961 | `	sxu32 nIdx;` |
|       - |  1962 | `	sxu32 nLen;` |
|       - |  1963 | `	sxi32 rc;` |
|       - |  1964 |  |
|     257 |  1965 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     257 |  1966 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1967 | `		pGen->pIn++;` |
|     ! 0 |  1968 | `	}` |
|       - |  1969 | `	/* Reserve a constant for the lambda */` |
|     257 |  1970 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     257 |  1971 | `	if( pObj == 0 ){` |
|     ! 0 |  1972 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1973 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1974 | `		return SXERR_ABORT;` |
|       - |  1975 | `	}` |
|       - |  1976 | `	/* Generate a unique name */` |
|     257 |  1977 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1978 | `	/* Make sure the generated name is unique */` |
|     257 |  1979 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1980 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1981 | `	}` |
|     257 |  1982 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     257 |  1983 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - |  1984 | `	/* Compile the lambda body */` |
|     257 |  1985 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     257 |  1986 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1987 | `		return SXERR_ABORT;` |
|       - |  1988 | `	}` |
|     257 |  1989 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - |  1990 | `		/* Emit the load closure instruction */` |
|      21 |  1991 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|      13 |  1992 | `	}else{` |
|       - |  1993 | `		/* Emit the load constant instruction */` |
|     241 |  1994 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1995 | `	}` |
|       - |  1996 | `	/* Node successfully compiled */` |
|     257 |  1997 | `	return SXRET_OK;` |
|     131 |  1998 |  |
|       - |  1999 | `/*` |
|       - |  2000 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  2001 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  2002 | ` * enclosing arrow level, or has already been captured.` |
|       - |  2003 | ` */` |
|     150 |  2004 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  2005 | `	ph7_gen_state *pGen,` |
|       - |  2006 | `	ph7_vm_func *pFunc,` |
|       - |  2007 | `	const char *zName,` |
|       - |  2008 | `	sxu32 nByte,` |
|       - |  2009 | `	SyString *aShadow,` |
|       - |  2010 | `	sxu32 nShadow)` |
|       2 |  2011 |  |
|       - |  2012 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2013 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2014 | `	sxu32 n, nEnv;` |
|       - |  2015 | `	char *zDup;` |
|     152 |  2016 | `	if( nByte == 0 ){` |
|     ! 0 |  2017 | `		return SXRET_OK;` |
|       - |  2018 | `	}` |
|     150 |  2019 | `	if( nByte == sizeof("this")-1` |
|      81 |  2020 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2021 | `		return SXRET_OK;` |
|       - |  2022 | `	}` |
|     182 |  2023 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     128 |  2024 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     125 |  2025 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      98 |  2026 | `			return SXRET_OK;` |
|       - |  2027 | `		}` |
|      17 |  2028 | `	}` |
|      53 |  2029 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  2030 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  2031 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  2032 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  2033 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  2034 | `			return SXRET_OK;` |
|       - |  2035 | `		}` |
|      15 |  2036 | `	}` |
|      53 |  2037 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  2038 | `	if( zDup == 0 ){` |
|     ! 0 |  2039 | `		return SXERR_ABORT;` |
|       - |  2040 | `	}` |
|      53 |  2041 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  2042 | `	sEnv.iFlags = 0;` |
|      53 |  2043 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  2044 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  2045 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  2046 | `	return SXRET_OK;` |
|      77 |  2047 |  |
|       - |  2048 | `/*` |
|       - |  2049 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2050 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2051 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2052 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2053 | ` */` |
|      14 |  2054 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2055 | `	ph7_gen_state *pGen,` |
|       - |  2056 | `	ph7_vm_func *pFunc,` |
|       - |  2057 | `	const char *zIn,` |
|       - |  2058 | `	const char *zEnd,` |
|       - |  2059 | `	SyString *aShadow,` |
|       - |  2060 | `	sxu32 nShadow)` |
|       1 |  2061 |  |
|       - |  2062 | `	sxi32 rc;` |
|     159 |  2063 | `	while( zIn < zEnd ){` |
|     145 |  2064 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  2065 | `			zIn++;` |
|     ! 0 |  2066 | `			if( zIn < zEnd ){` |
|     ! 0 |  2067 | `				zIn++;` |
|     ! 0 |  2068 | `			}` |
|     ! 0 |  2069 | `			continue;` |
|       - |  2070 | `		}` |
|     144 |  2071 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  2072 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  2073 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2074 | `			const char *zName;` |
|      13 |  2075 | `			zIn++; /* skip '$' */` |
|      13 |  2076 | `			zName = zIn;` |
|      39 |  2077 | `			while( zIn < zEnd ){` |
|      35 |  2078 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  2079 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2080 | `					zIn++;` |
|     ! 0 |  2081 | `					while( zIn < zEnd` |
|     ! 0 |  2082 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2083 | `						zIn++;` |
|     ! 0 |  2084 | `					}` |
|     ! 0 |  2085 | `					continue;` |
|       - |  2086 | `				}` |
|      35 |  2087 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  2088 | `					break;` |
|       - |  2089 | `				}` |
|      27 |  2090 | `				zIn++;` |
|       1 |  2091 | `			}` |
|      13 |  2092 | `			if( zIn > zName ){` |
|      19 |  2093 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  2094 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  2095 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2096 | `					return SXERR_ABORT;` |
|       - |  2097 | `				}` |
|       6 |  2098 | `			}` |
|      13 |  2099 | `			continue;` |
|       - |  2100 | `		}` |
|     133 |  2101 | `		zIn++;` |
|       1 |  2102 | `	}` |
|      15 |  2103 | `	return SXRET_OK;` |
|       8 |  2104 |  |
|       - |  2105 | `/*` |
|       - |  2106 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  2107 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  2108 | ` *   - plain $<id> pairs` |
|       - |  2109 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  2110 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  2111 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  2112 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  2113 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  2114 | ` *     are never mistakenly captured.` |
|       - |  2115 | ` */` |
|     136 |  2116 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2117 | `	ph7_gen_state *pGen,` |
|       - |  2118 | `	ph7_vm_func *pFunc,` |
|       - |  2119 | `	SyToken *pStart,` |
|       - |  2120 | `	SyToken *pEnd,` |
|       - |  2121 | `	SyString *aShadow,` |
|       - |  2122 | `	sxu32 nShadow)` |
|       2 |  2123 |  |
|     138 |  2124 | `	SyToken *pScan = pStart;` |
|       - |  2125 | `	sxi32 rc;` |
|     512 |  2126 | `	while( pScan < pEnd ){` |
|     376 |  2127 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 |  2128 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 |  2129 | `				pScan->sData.zString,` |
|      14 |  2130 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 |  2131 | `				aShadow,nShadow);` |
|      15 |  2132 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2133 | `				return SXERR_ABORT;` |
|       - |  2134 | `			}` |
|      15 |  2135 | `			pScan++;` |
|      15 |  2136 | `			continue;` |
|       - |  2137 | `		}` |
|     362 |  2138 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  2139 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  2140 | `			SyToken *pFnKw = pScan;` |
|      20 |  2141 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2142 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  2143 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2144 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2145 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2146 | `			}` |
|      21 |  2147 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2148 | `				SyToken *pInnerSigStart;` |
|       - |  2149 | `				SyToken *pInnerSigEnd;` |
|       - |  2150 | `				SyToken *pInnerBodyEnd;` |
|       - |  2151 | `				SyString *aInnerShadow;` |
|       - |  2152 | `				sxu32 nInnerShadow;` |
|       - |  2153 | `				sxu32 nInnerParamMax;` |
|       - |  2154 | `				SyToken *p;` |
|       - |  2155 | `				int iNestInner;` |
|      19 |  2156 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2157 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2158 | `					pScan++;` |
|     ! 0 |  2159 | `				}` |
|      19 |  2160 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2161 | `					pScan++;` |
|     ! 0 |  2162 | `					continue;` |
|       - |  2163 | `				}` |
|      19 |  2164 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2165 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2166 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2167 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2168 | `					pScan = pEnd;` |
|     ! 0 |  2169 | `					continue;` |
|       - |  2170 | `				}` |
|       - |  2171 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2172 | `				nInnerParamMax = 0;` |
|      57 |  2173 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2174 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2175 | `						nInnerParamMax++;` |
|       6 |  2176 | `					}` |
|      20 |  2177 | `				}` |
|      19 |  2178 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2179 | `					&pGen->pVm->sAllocator,` |
|      18 |  2180 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2181 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2182 | `					return SXERR_ABORT;` |
|       - |  2183 | `				}` |
|      19 |  2184 | `				nInnerShadow = 0;` |
|      25 |  2185 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2186 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2187 | `				}` |
|      57 |  2188 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2189 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2190 | `						continue;` |
|       - |  2191 | `					}` |
|      13 |  2192 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2193 | `						break;` |
|       - |  2194 | `					}` |
|      13 |  2195 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2196 | `						continue;` |
|       - |  2197 | `					}` |
|      13 |  2198 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2199 | `				}` |
|      19 |  2200 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2201 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2202 | `					pScan++;` |
|     ! 0 |  2203 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2204 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2205 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2206 | `						pScan++;` |
|     ! 0 |  2207 | `					}` |
|     ! 0 |  2208 | `					if( pScan < pEnd` |
|     ! 0 |  2209 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2210 | `						pScan++;` |
|     ! 0 |  2211 | `					}` |
|     ! 0 |  2212 | `				}` |
|      19 |  2213 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2214 | `					pScan++; /* past '=>' */` |
|       9 |  2215 | `				}` |
|      19 |  2216 | `				pInnerBodyEnd = pScan;` |
|      19 |  2217 | `				iNestInner = 0;` |
|     131 |  2218 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2219 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2220 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2221 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2222 | `						break;` |
|       - |  2223 | `					}` |
|     113 |  2224 | `					if( pInnerBodyEnd->nType &` |
|       - |  2225 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2226 | `						iNestInner++;` |
|     112 |  2227 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2228 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2229 | `						iNestInner--;` |
|       1 |  2230 | `					}` |
|     113 |  2231 | `					pInnerBodyEnd++;` |
|       1 |  2232 | `				}` |
|       - |  2233 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2234 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2235 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2236 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2237 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2238 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2239 | `				 *` |
|       - |  2240 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2241 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2242 | `				 * range after the '=' sign. */` |
|       - |  2243 | `				{` |
|      19 |  2244 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2245 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2246 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2247 | `						SyToken *pEq = 0;` |
|      13 |  2248 | `						int iNestArg = 0;` |
|      49 |  2249 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2250 | `							if( iNestArg == 0` |
|      39 |  2251 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2252 | `								break;` |
|       - |  2253 | `							}` |
|      37 |  2254 | `							if( pArgEnd->nType &` |
|       - |  2255 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2256 | `								iNestArg++;` |
|      37 |  2257 | `							}else if( pArgEnd->nType &` |
|       - |  2258 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2259 | `								iNestArg--;` |
|     ! 0 |  2260 | `							}` |
|      36 |  2261 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2262 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2263 | `								pEq = pArgEnd;` |
|       3 |  2264 | `							}` |
|      37 |  2265 | `							pArgEnd++;` |
|       1 |  2266 | `						}` |
|      13 |  2267 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2268 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2269 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2270 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2271 | `								return SXERR_ABORT;` |
|       - |  2272 | `							}` |
|       3 |  2273 | `						}` |
|      13 |  2274 | `						pArgStart = pArgEnd;` |
|      12 |  2275 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2276 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2277 | `							pArgStart++;` |
|       1 |  2278 | `						}` |
|       1 |  2279 | `					}` |
|       - |  2280 | `				}` |
|      28 |  2281 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2282 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2283 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2284 | `					return SXERR_ABORT;` |
|       - |  2285 | `				}` |
|      19 |  2286 | `				pScan = pInnerBodyEnd;` |
|      19 |  2287 | `				continue;` |
|       - |  2288 | `			}` |
|       1 |  2289 | `		}` |
|     344 |  2290 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     206 |  2291 | `			pScan++;` |
|     206 |  2292 | `			continue;` |
|       - |  2293 | `		}` |
|       - |  2294 | `		{` |
|       - |  2295 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     140 |  2296 | `			SyToken *pDollar = pScan;` |
|     207 |  2297 | `			while( &pDollar[1] < pEnd` |
|     140 |  2298 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2299 | `				pDollar++;` |
|     ! 0 |  2300 | `			}` |
|     140 |  2301 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2302 | `				break;` |
|       - |  2303 | `			}` |
|     140 |  2304 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2305 | `				pScan = pDollar + 1;` |
|     ! 0 |  2306 | `				continue;` |
|       - |  2307 | `			}` |
|     209 |  2308 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     138 |  2309 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      69 |  2310 | `				aShadow,nShadow);` |
|     140 |  2311 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2312 | `				return SXERR_ABORT;` |
|       - |  2313 | `			}` |
|     140 |  2314 | `			pScan = pDollar + 2;` |
|       - |  2315 | `		}` |
|       2 |  2316 | `	}` |
|     138 |  2317 | `	return SXRET_OK;` |
|      70 |  2318 |  |
|       - |  2319 | `/*` |
|       - |  2320 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2321 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2322 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2323 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2324 | ` * $this is also made available.` |
|       - |  2325 | ` */` |
|     118 |  2326 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2327 |  |
|       - |  2328 | `	ph7_vm_func *pFunc;` |
|       - |  2329 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2330 | `	GenBlock *pBlock;` |
|       - |  2331 | `	SySet *pInstrContainer;` |
|       - |  2332 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2333 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2334 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2335 | `	SyToken *pSavedEnd;` |
|       - |  2336 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2337 | `	char zName[512];` |
|       - |  2338 | `	static int iCnt = 1;` |
|       - |  2339 | `	char *zDup;` |
|       - |  2340 | `	sxu32 nLen;` |
|       - |  2341 | `	sxu32 nLine;` |
|     122 |  2342 | `	sxi32 iFlags = 0;` |
|     122 |  2343 | `	int bStatic = 0;` |
|       - |  2344 | `	sxi32 rc;` |
|       - |  2345 | `	sxu32 n;` |
|      59 |  2346 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2347 |  |
|     122 |  2348 | `	nLine = pGen->pIn->nLine;` |
|       - |  2349 | `	/* Optional 'static' prefix */` |
|     118 |  2350 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     122 |  2351 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2352 | `		bStatic = 1;` |
|       3 |  2353 | `		pGen->pIn++;` |
|       1 |  2354 | `	}` |
|       - |  2355 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     118 |  2356 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     122 |  2357 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2358 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2359 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2360 | `		return SXERR_SYNTAX;` |
|       - |  2361 | `	}` |
|     122 |  2362 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2363 | `	/* Optional '&' — return by reference */` |
|     122 |  2364 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2365 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2366 | `		pGen->pIn++;` |
|     ! 0 |  2367 | `	}` |
|       - |  2368 | `	/* Expect '(' */` |
|     122 |  2369 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2370 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2371 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2372 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2373 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2374 | `		}else{` |
|     ! 0 |  2375 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2376 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2377 | `		}` |
|       3 |  2378 | `		return SXERR_SYNTAX;` |
|       - |  2379 | `	}` |
|     119 |  2380 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2381 | `	/* Delimit the parameter list */` |
|     119 |  2382 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     119 |  2383 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2384 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2385 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2386 | `		return SXERR_SYNTAX;` |
|       - |  2387 | `	}` |
|       - |  2388 | `	/* Allocate the function state */` |
|     117 |  2389 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     117 |  2390 | `	if( pFunc == 0 ){` |
|     ! 0 |  2391 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2392 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2393 | `		return SXERR_ABORT;` |
|       - |  2394 | `	}` |
|       - |  2395 | `	/* Generate a unique lambda name */` |
|     117 |  2396 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     219 |  2397 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     104 |  2398 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2399 | `	}` |
|     117 |  2400 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     117 |  2401 | `	if( zDup == 0 ){` |
|     ! 0 |  2402 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2403 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2404 | `		return SXERR_ABORT;` |
|       - |  2405 | `	}` |
|     117 |  2406 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2407 | `	/* Collect function arguments */` |
|     117 |  2408 | `	if( pGen->pIn < pSigEnd ){` |
|      87 |  2409 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      87 |  2410 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2411 | `			return SXERR_ABORT;` |
|       - |  2412 | `		}` |
|      42 |  2413 | `	}` |
|       - |  2414 | `	/* Point past ')' and parse optional return type */` |
|     117 |  2415 | `	pGen->pIn = &pSigEnd[1];` |
|     117 |  2416 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     117 |  2417 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2418 | `		return SXERR_ABORT;` |
|     117 |  2419 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2420 | `		return SXERR_SYNTAX;` |
|       - |  2421 | `	}` |
|       - |  2422 | `	/* Expect '=>' */` |
|     117 |  2423 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2424 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2425 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2426 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2427 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2428 | `		}else{` |
|     ! 0 |  2429 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2430 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2431 | `		}` |
|       3 |  2432 | `		return SXERR_SYNTAX;` |
|       - |  2433 | `	}` |
|     114 |  2434 | `	pGen->pIn++; /* Jump '=>' */` |
|     114 |  2435 | `	pBodyStart = pGen->pIn;` |
|     114 |  2436 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2437 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2438 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2439 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2440 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     114 |  2441 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2442 | `	{` |
|     114 |  2443 | `		SyString *aShadow = 0;` |
|     114 |  2444 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     114 |  2445 | `		if( nShadow > 0 ){` |
|      84 |  2446 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      82 |  2447 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      84 |  2448 | `			if( aShadow == 0 ){` |
|     ! 0 |  2449 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2450 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2451 | `				return SXERR_ABORT;` |
|       - |  2452 | `			}` |
|     184 |  2453 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     102 |  2454 | `				aShadow[n] = aArgs[n].sName;` |
|      52 |  2455 | `			}` |
|      41 |  2456 | `		}` |
|     170 |  2457 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      56 |  2458 | `			aShadow,nShadow);` |
|     114 |  2459 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2460 | `			return SXERR_ABORT;` |
|       - |  2461 | `		}` |
|       - |  2462 | `	}` |
|       - |  2463 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2464 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2465 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2466 | `	 * $this. */` |
|     114 |  2467 | `	if( !bStatic ){` |
|       - |  2468 | `		char *zThisDup;` |
|     112 |  2469 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     112 |  2470 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2471 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2472 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2473 | `			return SXERR_ABORT;` |
|       - |  2474 | `		}` |
|     112 |  2475 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     112 |  2476 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     112 |  2477 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     112 |  2478 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     112 |  2479 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      55 |  2480 | `	}` |
|       - |  2481 | `	/* Arrow functions are always closures */` |
|     114 |  2482 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2483 | `	/* Compile the body expression as an implicit return */` |
|     170 |  2484 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      56 |  2485 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     114 |  2486 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2487 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2488 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2489 | `		return SXERR_ABORT;` |
|       - |  2490 | `	}` |
|     114 |  2491 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     114 |  2492 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     114 |  2493 | `	pSavedEnd = pGen->pEnd;` |
|     114 |  2494 | `	pGen->pIn = pBodyStart;` |
|     114 |  2495 | `	pGen->pEnd = pBodyEnd;` |
|     114 |  2496 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     114 |  2497 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2498 | `		return SXERR_ABORT;` |
|       - |  2499 | `	}` |
|       - |  2500 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2501 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2502 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2503 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     114 |  2504 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     114 |  2505 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     114 |  2506 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     114 |  2507 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     114 |  2508 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2509 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     114 |  2510 | `	pGen->pIn = pBodyEnd;` |
|     114 |  2511 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2512 | `	/* Emit the load-closure instruction */` |
|     114 |  2513 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     114 |  2514 | `	return SXRET_OK;` |
|      63 |  2515 |  |
|       - |  2516 | `/*` |
|       - |  2517 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2518 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2519 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2520 | ` * expression's value.` |
|       - |  2521 | ` */` |
|     346 |  2522 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2523 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2524 |  |
|       - |  2525 | `	SySet *pInstrContainer;` |
|       - |  2526 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2527 | `	GenBlock *pArmBlock;` |
|       - |  2528 | `	sxi32 rc;` |
|     349 |  2529 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2530 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2531 | `	pGen->pIn  = pStart;` |
|     349 |  2532 | `	pGen->pEnd = pStop;` |
|     349 |  2533 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2534 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2535 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2536 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2537 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2538 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2539 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2540 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2541 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2542 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2543 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2544 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2545 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2546 | `		return SXERR_ABORT;` |
|       - |  2547 | `	}` |
|     349 |  2548 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2549 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2550 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2551 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2552 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2553 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2554 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2555 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2556 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2557 | `		return SXERR_ABORT;` |
|       - |  2558 | `	}` |
|     349 |  2559 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2560 | `		return SXERR_EMPTY;` |
|       - |  2561 | `	}` |
|     349 |  2562 | `	return SXRET_OK;` |
|     176 |  2563 |  |
|       - |  2564 | `/*` |
|       - |  2565 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2566 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2567 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2568 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2569 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2570 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2571 | ` */` |
|       - |  2572 | `/*` |
|       - |  2573 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2574 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2575 | ` * caller can bail out of the current expression.` |
|       - |  2576 | ` */` |
|       2 |  2577 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2578 |  |
|       - |  2579 | `	va_list ap;` |
|       - |  2580 | `	sxi32 rc;` |
|       - |  2581 | `	SyBlob sMsg;` |
|       3 |  2582 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2583 | `	va_start(ap,zFmt);` |
|       3 |  2584 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2585 | `	va_end(ap);` |
|       3 |  2586 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2587 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2588 | `	SyBlobRelease(&sMsg);` |
|       3 |  2589 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2590 | `		return SXERR_ABORT;` |
|       - |  2591 | `	}` |
|       3 |  2592 | `	return SXERR_SYNTAX;` |
|       2 |  2593 |  |
|       - |  2594 | `/*` |
|       - |  2595 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2596 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2597 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2598 | ` */` |
|     348 |  2599 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2600 |  |
|     352 |  2601 | `	SyToken *pCur = pStart;` |
|     352 |  2602 | `	int iNest = 0;` |
|     814 |  2603 | `	while( pCur < pEnd ){` |
|     780 |  2604 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2605 | `			iNest++;` |
|     774 |  2606 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2607 | `			iNest--;` |
|     762 |  2608 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2609 | `			return pCur;` |
|       - |  2610 | `		}` |
|     466 |  2611 | `		pCur++;` |
|       4 |  2612 | `	}` |
|      37 |  2613 | `	return pEnd;` |
|     178 |  2614 |  |
|      70 |  2615 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2616 |  |
|       - |  2617 | `	ph7_match *pMatch;` |
|       - |  2618 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2619 | `	int bHasDefault = 0;` |
|       - |  2620 | `	sxu32 nLine;` |
|       - |  2621 | `	sxi32 rc;` |
|      35 |  2622 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2623 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2624 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2625 | `	/* Expect '(' */` |
|      75 |  2626 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2627 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2628 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2629 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2630 | `	}` |
|      75 |  2631 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2632 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2633 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2634 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2635 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2636 | `	}` |
|      75 |  2637 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2638 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2639 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2640 | `	}` |
|       - |  2641 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2642 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2643 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2644 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2645 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2646 | `		return SXERR_ABORT;` |
|       - |  2647 | `	}` |
|      75 |  2648 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2649 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2650 | `	/* Expect '{' */` |
|      75 |  2651 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2652 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2653 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2654 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2655 | `	}` |
|      75 |  2656 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2657 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2658 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2659 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2660 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2661 | `	}` |
|       - |  2662 | `	/* Allocate ph7_match container */` |
|      75 |  2663 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2664 | `	if( pMatch == 0 ){` |
|     ! 0 |  2665 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2666 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2667 | `		return SXERR_ABORT;` |
|       - |  2668 | `	}` |
|      75 |  2669 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2670 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2671 | `	/* Iterate arms */` |
|     253 |  2672 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2673 | `		ph7_match_arm sArm;` |
|       - |  2674 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2675 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2676 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2677 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2678 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2679 | `		/* 'default' arm? */` |
|     182 |  2680 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2681 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2682 | `			if( bHasDefault ){` |
|       3 |  2683 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2684 | `					"Match expressions may only contain one default arm");` |
|       4 |  2685 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2686 | `			}` |
|      20 |  2687 | `			sArm.bDefault = 1;` |
|      20 |  2688 | `			bHasDefault = 1;` |
|      20 |  2689 | `			pGen->pIn++;` |
|      20 |  2690 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2691 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2692 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2693 | `			}` |
|      20 |  2694 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2695 | `		}else{` |
|       - |  2696 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2697 | `			pCondStart = pGen->pIn;` |
|     166 |  2698 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2699 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2700 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2701 | `				SySet sCondBc;` |
|       9 |  2702 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2703 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2704 | `						"syntax error, empty match condition expression");` |
|       - |  2705 | `				}` |
|       9 |  2706 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2707 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2708 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2709 | `					return SXERR_ABORT;` |
|       - |  2710 | `				}` |
|       9 |  2711 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2712 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2713 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2714 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2715 | `			}` |
|     166 |  2716 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2717 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2718 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2719 | `			}` |
|     163 |  2720 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2721 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2722 | `					"syntax error, empty match condition expression");` |
|       - |  2723 | `			}` |
|       - |  2724 | `			{` |
|       - |  2725 | `				SySet sCondBc;` |
|     163 |  2726 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2727 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2728 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2729 | `					return SXERR_ABORT;` |
|       - |  2730 | `				}` |
|     163 |  2731 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2732 | `			}` |
|     163 |  2733 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2734 | `		}` |
|       - |  2735 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2736 | `		pResStart = pGen->pIn;` |
|     181 |  2737 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2738 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2739 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2740 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2741 | `		}` |
|     181 |  2742 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2743 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2744 | `			return SXERR_ABORT;` |
|       - |  2745 | `		}` |
|     181 |  2746 | `		pGen->pIn = pResEnd;` |
|     181 |  2747 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2748 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2749 | `		}` |
|     181 |  2750 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2751 | `	}` |
|      69 |  2752 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2753 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2754 | `	return SXRET_OK;` |
|      40 |  2755 |  |
|       - |  2756 | `/*` |
|       - |  2757 | ` * Compile a backtick quoted string.` |
|       - |  2758 | ` */` |
|       4 |  2759 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2760 |  |
|       - |  2761 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2762 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2763 | `	 */` |
|       8 |  2764 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2765 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2766 | `		ph7_lib_version()` |
|       - |  2767 | `		);` |
|       - |  2768 | `	/* Load NULL */` |
|       6 |  2769 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2770 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2771 | `	/* Node successfully compiled */` |
|       6 |  2772 | `	return SXRET_OK;` |
|       2 |  2773 |  |
|       - |  2774 | `/*` |
|       - |  2775 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2776 | ` * construct.` |
|       - |  2777 | ` */` |
|      80 |  2778 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2779 |  |
|       - |  2780 | `	SyString *pName;` |
|       - |  2781 | `	sxu32 nKeyID;` |
|       - |  2782 | `	sxi32 rc;` |
|       - |  2783 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      85 |  2784 | `	pName = &pGen->pIn->sData;` |
|      85 |  2785 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  2786 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      85 |  2787 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2788 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2789 | `		/* Compile arguments one after one */` |
|       9 |  2790 | `		pTmp = pGen->pEnd;` |
|       - |  2791 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2792 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2793 | `		 *  mean that the following expression is valid:` |
|       - |  2794 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2795 | `		 */` |
|       9 |  2796 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2797 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2798 | `			if( pGen->pIn < pNext ){` |
|       9 |  2799 | `				pGen->pEnd = pNext;` |
|       9 |  2800 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2801 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2802 | `					return SXERR_ABORT;` |
|       - |  2803 | `				}` |
|       9 |  2804 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2805 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2806 | `					 * without the overhead of a function call.` |
|       - |  2807 | `					 * This is a very powerful optimization that improve` |
|       - |  2808 | `					 * performance greatly.` |
|       - |  2809 | `					 */` |
|       9 |  2810 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2811 | `				}` |
|       4 |  2812 | `			}` |
|       - |  2813 | `			/* Jump trailing commas */` |
|       9 |  2814 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2815 | `				pNext++;` |
|     ! 0 |  2816 | `			}` |
|       9 |  2817 | `			pGen->pIn = pNext;` |
|       1 |  2818 | `		}` |
|       - |  2819 | `		/* Restore token stream */` |
|       9 |  2820 | `		pGen->pEnd = pTmp;` |
|       5 |  2821 | `	}else{` |
|      77 |  2822 | `		sxi32 nArg = 0;` |
|      77 |  2823 | `		sxu32 nIdx = 0;` |
|      77 |  2824 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      77 |  2825 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2826 | `			return SXERR_ABORT;` |
|      77 |  2827 | `		}else if(rc != SXERR_EMPTY ){` |
|      77 |  2828 | `			nArg = 1;` |
|      36 |  2829 | `		}` |
|      77 |  2830 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2831 | `			ph7_value *pObj;` |
|       - |  2832 | `			/* Emit the call instruction */` |
|      29 |  2833 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  2834 | `			if( pObj == 0 ){` |
|     ! 0 |  2835 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2836 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2837 | `				return SXERR_ABORT;` |
|       - |  2838 | `			}` |
|      29 |  2839 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2840 | `			/* Install in the literal table */` |
|      29 |  2841 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      12 |  2842 | `		}` |
|       - |  2843 | `		/* Emit the call instruction */` |
|      77 |  2844 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      77 |  2845 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2846 | `	}` |
|       - |  2847 | `	/* Node successfully compiled */` |
|      85 |  2848 | `	return SXRET_OK;` |
|      45 |  2849 |  |
|       - |  2850 | `/*` |
|       - |  2851 | ` * Compile a node holding a variable declaration.` |
|       - |  2852 | ` * According to the PHP language reference` |
|       - |  2853 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2854 | ` *  The variable name is case-sensitive.` |
|       - |  2855 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2856 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2857 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2858 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2859 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2860 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2861 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2862 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2863 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2864 | ` *  the chapter on Expressions.` |
|       - |  2865 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2866 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2867 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2868 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2869 | ` *  is being assigned (the source variable).` |
|       - |  2870 | ` */` |
| 1036032 |  2871 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2872 |  |
| 1036037 |  2873 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2874 | `	sxi32 iVv;` |
|       - |  2875 | `	sxi32 iP1;` |
|       - |  2876 | `	void *p3;` |
|       - |  2877 | `	sxi32 rc;` |
| 1036037 |  2878 | `	iVv = -1; /* Variable variable counter */` |
| 2072081 |  2879 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1036049 |  2880 | `		pGen->pIn++;` |
| 1036049 |  2881 | `		iVv++;` |
|       5 |  2882 | `	}` |
| 1036037 |  2883 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2884 | `		/* Invalid variable name */` |
|     ! 0 |  2885 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2886 | `		if( rc == SXERR_ABORT ){` |
|       - |  2887 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2888 | `			return SXERR_ABORT;` |
|       - |  2889 | `		}` |
|     ! 0 |  2890 | `		return SXRET_OK;` |
|       - |  2891 | `	}` |
| 1036037 |  2892 | `	p3  = 0;` |
| 1036037 |  2893 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2894 | `		/* Dynamic variable creation */` |
|      19 |  2895 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  2896 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  2897 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2898 | `			/* Empty expression */` |
|       3 |  2899 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2900 | `			return SXRET_OK;` |
|       - |  2901 | `		}` |
|       - |  2902 | `		/* Compile the expression holding the variable name */` |
|      16 |  2903 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2904 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2905 | `			return SXERR_ABORT;` |
|      16 |  2906 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2907 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2908 | `			return SXRET_OK;` |
|       - |  2909 | `		}` |
|       7 |  2910 | `	}else{` |
|       - |  2911 | `		SyHashEntry *pEntry;` |
|       - |  2912 | `		SyString *pName;` |
| 1036021 |  2913 | `		char *zName = 0;` |
|       - |  2914 | `		/* Extract variable name */` |
| 1036021 |  2915 | `		pName = &pGen->pIn->sData;` |
|       - |  2916 | `		/* Advance the stream cursor */` |
| 1036021 |  2917 | `		pGen->pIn++;` |
| 1036021 |  2918 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1036021 |  2919 | `		if( pEntry == 0 ){` |
|       - |  2920 | `			/* Duplicate name */` |
|  139025 |  2921 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  139025 |  2922 | `			if( zName == 0 ){` |
|     ! 0 |  2923 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2924 | `				return SXERR_ABORT;` |
|       - |  2925 | `			}` |
|       - |  2926 | `			/* Install in the hashtable */` |
|  139025 |  2927 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   69515 |  2928 | `		}else{` |
|       - |  2929 | `			/* Name already available */` |
|  897001 |  2930 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2931 | `		}` |
| 1036021 |  2932 | `		p3 = (void *)zName;` |
|       - |  2933 | `	}` |
| 1036033 |  2934 | `	iP1 = 0;` |
| 1036033 |  2935 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  377399 |  2936 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2937 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  377381 |  2938 | `			iP1 = 1;` |
|  188688 |  2939 | `		}` |
|  188697 |  2940 | `	}` |
|       - |  2941 | `	/* Emit the load instruction */` |
| 1036033 |  2942 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1036045 |  2943 | `	while( iVv > 0 ){` |
|      13 |  2944 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2945 | `		iVv--;` |
|       1 |  2946 | `	}` |
|       - |  2947 | `	/* Node successfully compiled */` |
| 1036033 |  2948 | `	return SXRET_OK;` |
|  518021 |  2949 |  |
|       - |  2950 | `/*` |
|       - |  2951 | ` * Load a literal.` |
|       - |  2952 | ` */` |
|  729168 |  2953 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2954 |  |
|  729173 |  2955 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2956 | `	ph7_value *pObj;` |
|       - |  2957 | `	SyString *pStr;` |
|       - |  2958 | `	sxu32 nIdx;` |
|       - |  2959 | `	/* Extract token value */` |
|  729173 |  2960 | `	pStr = &pToken->sData;` |
|       - |  2961 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  729173 |  2962 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  154539 |  2963 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2964 | `			/* NULL constant are always indexed at 0 */` |
|   56929 |  2965 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   56929 |  2966 | `			return SXRET_OK;` |
|   97615 |  2967 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2968 | `			/* TRUE constant are always indexed at 1 */` |
|     669 |  2969 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     669 |  2970 | `			return SXRET_OK;` |
|       5 |  2971 | `		}` |
|  680839 |  2972 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  115454 |  2973 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2974 | `			/* FALSE constant are always indexed at 2 */` |
|   43653 |  2975 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   43653 |  2976 | `			return SXRET_OK;` |
|  582777 |  2977 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  103572 |  2978 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2979 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    9929 |  2980 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    9929 |  2981 | `			if( pObj == 0 ){` |
|     ! 0 |  2982 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2983 | `				return SXERR_ABORT;` |
|       - |  2984 | `			}` |
|    9929 |  2985 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2986 | `			/* Emit the load constant instruction */` |
|    9929 |  2987 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    9929 |  2988 | `			return SXRET_OK;` |
|  537802 |  2989 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   33470 |  2990 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2991 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       8 |  2992 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       8 |  2993 | `			if( pObj == 0 ){` |
|     ! 0 |  2994 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2995 | `				return SXERR_ABORT;` |
|       - |  2996 | `			}` |
|       8 |  2997 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2998 | `				SyString sNs;` |
|       8 |  2999 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  3000 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       5 |  3001 | `			}else{` |
|     ! 0 |  3002 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  3003 | `			}` |
|       8 |  3004 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       8 |  3005 | `			return SXRET_OK;` |
|  528045 |  3006 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   22871 |  3007 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  529937 |  3008 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   17776 |  3009 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  3010 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  3011 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  3012 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  3013 | `				/* Point to the upper block */` |
|      11 |  3014 | `				pBlock = pBlock->pParent;` |
|       1 |  3015 | `			}` |
|      11 |  3016 | `			if( pBlock == 0 ){` |
|       - |  3017 | `				/* Called in the global scope,load NULL */` |
|       5 |  3018 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  3019 | `			}else{` |
|       - |  3020 | `				/* Extract the target function/method */` |
|       7 |  3021 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  3022 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  3023 | `					/* Not a class method,Load null */` |
|       3 |  3024 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3025 | `				}else{` |
|       5 |  3026 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  3027 | `					if( pObj == 0 ){` |
|     ! 0 |  3028 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3029 | `						return SXERR_ABORT;` |
|       - |  3030 | `					}` |
|       5 |  3031 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  3032 | `					/* Emit the load constant instruction */` |
|       5 |  3033 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  3034 | `				}` |
|       - |  3035 | `			}` |
|      11 |  3036 | `			return SXRET_OK;` |
|       - |  3037 | `	}` |
|       - |  3038 | `	/* Query literal table */` |
|  617997 |  3039 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3040 | `		ph7_value *pLitObj;` |
|       - |  3041 | `		/* Unknown literal,install it in the literal table */` |
|  256549 |  3042 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  256549 |  3043 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3044 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3045 | `			return SXERR_ABORT;` |
|       - |  3046 | `		}` |
|  256549 |  3047 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  256549 |  3048 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  128272 |  3049 | `	}` |
|       - |  3050 | `	/* Emit the load constant instruction */` |
|  617997 |  3051 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  617997 |  3052 | `	return SXRET_OK;` |
|  364589 |  3053 |  |
|       - |  3054 | `/*` |
|       - |  3055 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3056 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3057 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3058 | ` * Otherwise, load the simple literal directly.` |
|       - |  3059 | ` */` |
|  729208 |  3060 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3061 |  |
|       - |  3062 | `	sxi32 rc;` |
|  729213 |  3063 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3064 | `		return SXRET_OK;` |
|       - |  3065 | `	}` |
|       - |  3066 | `	/* Check if this is a multi-token namespace path */` |
|  729213 |  3067 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3068 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      45 |  3069 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      45 |  3070 | `		int isAbsolute = 0;` |
|      45 |  3071 | `		SyBlobReset(pWorker);` |
|       - |  3072 | `		/* Check for leading backslash (absolute path) */` |
|      45 |  3073 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      43 |  3074 | `			isAbsolute = 1;` |
|      43 |  3075 | `			pGen->pIn++; /* Skip leading backslash */` |
|      19 |  3076 | `		}` |
|       - |  3077 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      45 |  3078 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3079 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3080 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3081 | `		}` |
|       - |  3082 | `		/* Collect all path components */` |
|     141 |  3083 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     141 |  3084 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      53 |  3085 | `				SyBlobAppend(pWorker,"\\",1);` |
|      29 |  3086 | `			}else{` |
|      93 |  3087 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3088 | `			}` |
|     141 |  3089 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      45 |  3090 | `				pGen->pIn++;` |
|      45 |  3091 | `				break;` |
|       - |  3092 | `			}` |
|     101 |  3093 | `			pGen->pIn++;` |
|       5 |  3094 | `		}` |
|      45 |  3095 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3096 | `			ph7_value *pObj;` |
|       - |  3097 | `			SyString sPath;` |
|       - |  3098 | `			sxu32 nIdx;` |
|      45 |  3099 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3100 | `			/* Install in the literal table */` |
|      45 |  3101 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      21 |  3102 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      21 |  3103 | `				if( pObj == 0 ){` |
|     ! 0 |  3104 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3105 | `					return SXERR_ABORT;` |
|       - |  3106 | `				}` |
|      21 |  3107 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      21 |  3108 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 |  3109 | `			}` |
|       - |  3110 | `			/* Emit the load constant instruction.` |
|       - |  3111 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3112 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      65 |  3113 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      20 |  3114 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      20 |  3115 | `				nIdx,0,0);` |
|      45 |  3116 | `			return SXRET_OK;` |
|       - |  3117 | `		}` |
|     ! 0 |  3118 | `	}` |
|       - |  3119 | `	/* Single-token literal: load directly */` |
|  729173 |  3120 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  729173 |  3121 | `	return rc;` |
|  364609 |  3122 |  |
|       - |  3123 | `/*` |
|       - |  3124 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3125 | ` */` |
|  729208 |  3126 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3127 |  |
|       - |  3128 | `	sxi32 rc;` |
|  729213 |  3129 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  729213 |  3130 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3131 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3132 | `		return rc;` |
|       - |  3133 | `	}` |
|       - |  3134 | `	/* Node successfully compiled */` |
|  729213 |  3135 | `	return SXRET_OK;` |
|  364609 |  3136 |  |
|       - |  3137 | `/*` |
|       - |  3138 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3139 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3140 | ` */` |
|       8 |  3141 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3142 |  |
|       - |  3143 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3144 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3145 | `		pGen->pIn++;` |
|       1 |  3146 | `	}` |
|       9 |  3147 | `	return SXRET_OK;` |
|       1 |  3148 |  |
|       - |  3149 | `/*` |
|       - |  3150 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3151 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3152 | ` */` |
|     104 |  3153 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3154 |  |
|     109 |  3155 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      29 |  3156 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3157 | `			return TRUE;` |
|      27 |  3158 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3159 | `			return TRUE;` |
|       2 |  3160 | `		}` |
|      93 |  3161 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3162 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3163 | `			return TRUE;` |
|       - |  3164 | `		}` |
|     ! 0 |  3165 | `	}` |
|       - |  3166 | `	/* Not a reserved constant */` |
|     101 |  3167 | `	return FALSE;` |
|      57 |  3168 |  |
|       - |  3169 | `/*` |
|       - |  3170 | ` * Compile the 'const' statement.` |
|       - |  3171 | ` * According to the PHP language reference` |
|       - |  3172 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3173 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3174 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3175 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3176 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3177 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3178 | ` *  Syntax` |
|       - |  3179 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3180 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3181 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3182 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3183 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3184 | ` *  to get a list of all defined constants.` |
|       - |  3185 | ` *` |
|       - |  3186 | ` * Symisc eXtension.` |
|       - |  3187 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3188 | ` *  would allow only simple scalar value.` |
|       - |  3189 | ` *  Example` |
|       - |  3190 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3191 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3192 | ` */` |
|      32 |  3193 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3194 |  |
|       - |  3195 | `	SySet *pConsCode,*pInstrContainer;` |
|      37 |  3196 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3197 | `	SyString *pName;` |
|       - |  3198 | `	sxi32 rc;` |
|      37 |  3199 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      37 |  3200 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3201 | `		/* Invalid constant name */` |
|       9 |  3202 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       9 |  3203 | `		if( rc == SXERR_ABORT ){` |
|       - |  3204 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3205 | `			return SXERR_ABORT;` |
|       - |  3206 | `		}` |
|       9 |  3207 | `		goto Synchronize;` |
|       - |  3208 | `	}` |
|       - |  3209 | `	/* Peek constant name */` |
|      30 |  3210 | `	pName = &pGen->pIn->sData;` |
|       - |  3211 | `	/* Make sure the constant name isn't reserved */` |
|      30 |  3212 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3213 | `		/* Reserved constant */` |
|      10 |  3214 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3215 | `		if( rc == SXERR_ABORT ){` |
|       - |  3216 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3217 | `			return SXERR_ABORT;` |
|       - |  3218 | `		}` |
|      10 |  3219 | `		goto Synchronize;` |
|       - |  3220 | `	}` |
|      21 |  3221 | `	pGen->pIn++;` |
|      21 |  3222 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3223 | `		/* Invalid statement*/` |
|       6 |  3224 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3225 | `		if( rc == SXERR_ABORT ){` |
|       - |  3226 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3227 | `			return SXERR_ABORT;` |
|       - |  3228 | `		}` |
|       6 |  3229 | `		goto Synchronize;` |
|       - |  3230 | `	}` |
|      15 |  3231 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3232 | `	/* Allocate a new constant value container */` |
|      15 |  3233 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3234 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3235 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3236 | `		return SXERR_ABORT;` |
|       - |  3237 | `	}` |
|      15 |  3238 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3239 | `	/* Swap bytecode container */` |
|      15 |  3240 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3241 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3242 | `	/* Compile constant value */` |
|      15 |  3243 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3244 | `	/* Emit the done instruction */` |
|      15 |  3245 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3246 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3247 | `	if( rc == SXERR_ABORT ){` |
|       - |  3248 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3249 | `		return SXERR_ABORT;` |
|       - |  3250 | `	}` |
|      15 |  3251 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3252 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3253 | `	{` |
|       - |  3254 | `		SyBlob sFQN;` |
|       - |  3255 | `		SyString sFQNStr;` |
|      15 |  3256 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3257 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3258 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3259 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3260 | `		SyBlobRelease(&sFQN);` |
|       - |  3261 | `	}` |
|      15 |  3262 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3263 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3264 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3265 | `	}` |
|      15 |  3266 | `	return SXRET_OK;` |
|       9 |  3267 | `Synchronize:` |
|       - |  3268 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3269 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      42 |  3270 | `		pGen->pIn++;` |
|       4 |  3271 | `	}` |
|      22 |  3272 | `	return SXRET_OK;` |
|      21 |  3273 |  |
|       - |  3274 | `/*` |
|       - |  3275 | ` * Compile the 'continue' statement.` |
|       - |  3276 | ` * According to the PHP language reference` |
|       - |  3277 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3278 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3279 | ` *  iteration.` |
|       - |  3280 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3281 | ` *  the purposes of continue.` |
|       - |  3282 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3283 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3284 | ` *  Note:` |
|       - |  3285 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3286 | ` */` |
|       - |  3287 | `/*` |
|       - |  3288 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3289 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3290 | ` * break/continue crosses a try boundary.` |
|       - |  3291 | ` *` |
|       - |  3292 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3293 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3294 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3295 | ` */` |
|    3446 |  3296 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3297 |  |
|    3451 |  3298 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   20189 |  3299 | `	while( pBlock && pBlock != pTarget ){` |
|   16743 |  3300 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3301 | `			if( pBlock->pUserData ){` |
|       - |  3302 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3303 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3304 | `			}else{` |
|       - |  3305 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3306 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3307 | `				 * exception context from a sub-execution.` |
|       - |  3308 | `				 */` |
|     ! 0 |  3309 | `				break;` |
|       - |  3310 | `			}` |
|       1 |  3311 | `		}` |
|   16743 |  3312 | `		pBlock = pBlock->pParent;` |
|       5 |  3313 | `	}` |
|    3451 |  3314 |  |
|    3350 |  3315 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3316 |  |
|       - |  3317 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3318 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3319 | `	sxu32 nLineLocal;` |
|       - |  3320 | `	sxi32 rc;` |
|    3355 |  3321 | `	nLineLocal = pGen->pIn->nLine;` |
|    3355 |  3322 | `	iLevel = 0;` |
|       - |  3323 | `	/* Jump the 'continue' keyword */` |
|    3355 |  3324 | `	pGen->pIn++;` |
|    3355 |  3325 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3326 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3327 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3328 | `		 */` |
|       - |  3329 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3330 | `		char *zAlloc = 0;` |
|       - |  3331 | `		SyString sNum;` |
|      17 |  3332 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3333 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3334 | `			return SXERR_ABORT;` |
|       - |  3335 | `		}` |
|      17 |  3336 | `		if( rc == SXRET_OK ){` |
|      20 |  3337 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3338 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3339 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3340 | `				return SXERR_ABORT;` |
|       - |  3341 | `			}` |
|      14 |  3342 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3343 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3344 | `		}` |
|      17 |  3345 | `		if( iLevel < 2 ){` |
|       3 |  3346 | `			iLevel = 0;` |
|       1 |  3347 | `		}` |
|      17 |  3348 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3349 | `	}` |
|       - |  3350 | `	/* Point to the target loop */` |
|    3355 |  3351 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3355 |  3352 | `	if( pLoop == 0 ){` |
|       - |  3353 | `		/* Illegal continue */` |
|      12 |  3354 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      12 |  3355 | `		if( rc == SXERR_ABORT ){` |
|       - |  3356 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3357 | `			return SXERR_ABORT;` |
|       - |  3358 | `		}` |
|       7 |  3359 | `	}else{` |
|    3345 |  3360 | `		sxu32 nInstrIdx = 0;` |
|       - |  3361 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3345 |  3362 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3345 |  3363 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3364 | `			/* According to the PHP language reference manual` |
|       - |  3365 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3366 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3367 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3368 | `			 */` |
|       5 |  3369 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3370 | `			if( rc == SXRET_OK ){` |
|       5 |  3371 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3372 | `			}` |
|       3 |  3373 | `		}else{` |
|       - |  3374 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3341 |  3375 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3341 |  3376 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3377 | `				JumpFixup sJumpFix;` |
|       - |  3378 | `				/* Post-continue */` |
|      14 |  3379 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3380 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3381 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3382 | `			}` |
|       - |  3383 | `		}` |
|       - |  3384 | `	}` |
|    3355 |  3385 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3386 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3387 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3388 | `	}` |
|       - |  3389 | `	/* Statement successfully compiled */` |
|    3355 |  3390 | `	return SXRET_OK;` |
|    1680 |  3391 |  |
|       - |  3392 | `/*` |
|       - |  3393 | ` * Compile the 'break' statement.` |
|       - |  3394 | ` * According to the PHP language reference` |
|       - |  3395 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3396 | ` *  structure.` |
|       - |  3397 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3398 | ` *  enclosing structures are to be broken out of.` |
|       - |  3399 | ` */` |
|     122 |  3400 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3401 |  |
|       - |  3402 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3403 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3404 | `	sxi32 rc;` |
|     127 |  3405 | `	iLevel = 0;` |
|       - |  3406 | `	/* Jump the 'break' keyword */` |
|     127 |  3407 | `	pGen->pIn++;` |
|     127 |  3408 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3409 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3410 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3411 | `		 */` |
|       - |  3412 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3413 | `		char *zAlloc = 0;` |
|       - |  3414 | `		SyString sNum;` |
|      17 |  3415 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3416 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3417 | `			return SXERR_ABORT;` |
|       - |  3418 | `		}` |
|      17 |  3419 | `		if( rc == SXRET_OK ){` |
|      20 |  3420 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3421 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3422 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3423 | `				return SXERR_ABORT;` |
|       - |  3424 | `			}` |
|      14 |  3425 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3426 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3427 | `		}` |
|      17 |  3428 | `		if( iLevel < 2 ){` |
|       3 |  3429 | `			iLevel = 0;` |
|       1 |  3430 | `		}` |
|      17 |  3431 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3432 | `	}` |
|       - |  3433 | `	/* Extract the target loop */` |
|     127 |  3434 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     127 |  3435 | `	if( pLoop == 0 ){` |
|       - |  3436 | `		/* Illegal break */` |
|      19 |  3437 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      19 |  3438 | `		if( rc == SXERR_ABORT ){` |
|       - |  3439 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3440 | `			return SXERR_ABORT;` |
|       - |  3441 | `		}` |
|      11 |  3442 | `	}else{` |
|       - |  3443 | `		sxu32 nInstrIdx;` |
|       - |  3444 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|     111 |  3445 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|     111 |  3446 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|     111 |  3447 | `		if( rc == SXRET_OK ){` |
|       - |  3448 | `			/* Fix the jump later when the jump destination is resolved */` |
|     111 |  3449 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      53 |  3450 | `		}` |
|       - |  3451 | `	}` |
|     127 |  3452 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3453 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3454 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3455 | `	}` |
|       - |  3456 | `	/* Statement successfully compiled */` |
|     127 |  3457 | `	return SXRET_OK;` |
|      66 |  3458 |  |
|       - |  3459 | `/*` |
|       - |  3460 | ` * Compile or record a label.` |
|       - |  3461 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3462 | ` * Example` |
|       - |  3463 | ` *  goto LABEL;` |
|       - |  3464 | ` *   echo 'Foo';` |
|       - |  3465 | ` *  LABEL:` |
|       - |  3466 | ` *   echo 'Bar';` |
|       - |  3467 | ` */` |
|     112 |  3468 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3469 |  |
|       - |  3470 | `	GenBlock *pBlock;` |
|       - |  3471 | `	Label sLabel;` |
|       - |  3472 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3473 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3474 | `	if( pBlock ){` |
|       - |  3475 | `		sxi32 rc;` |
|       8 |  3476 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3477 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3478 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3479 | `			return SXERR_ABORT;` |
|       - |  3480 | `		}` |
|       4 |  3481 | `	}else{` |
|     113 |  3482 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3483 | `		char *zDup;` |
|       - |  3484 | `		/* Initialize label fields */` |
|     113 |  3485 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3486 | `		/* Duplicate label name */` |
|     113 |  3487 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3488 | `		if( zDup == 0 ){` |
|     ! 0 |  3489 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3490 | `			return SXERR_ABORT;` |
|       - |  3491 | `		}` |
|     113 |  3492 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3493 | `		sLabel.bRef  = FALSE;` |
|     113 |  3494 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3495 | `		pBlock = pGen->pCurrent;` |
|     221 |  3496 | `		while( pBlock ){` |
|     133 |  3497 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      23 |  3498 | `				break;` |
|       - |  3499 | `			}` |
|       - |  3500 | `			/* Point to the upper block */` |
|     113 |  3501 | `			pBlock = pBlock->pParent;` |
|       5 |  3502 | `		}` |
|     113 |  3503 | `		if( pBlock ){` |
|      23 |  3504 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      13 |  3505 | `		}else{` |
|      93 |  3506 | `			sLabel.pFunc = 0;` |
|       - |  3507 | `		}` |
|       - |  3508 | `		/* Insert in label set */` |
|     113 |  3509 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3510 | `	}` |
|     117 |  3511 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3512 | `	return SXRET_OK;` |
|      61 |  3513 |  |
|       - |  3514 | `/*` |
|       - |  3515 | ` * Compile the so hated 'goto' statement.` |
|       - |  3516 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3517 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3518 | ` * a compiler it has to do this.` |
|       - |  3519 | ` * According to the PHP language reference manual` |
|       - |  3520 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3521 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3522 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3523 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3524 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3525 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3526 | ` *   of a multi-level break` |
|       - |  3527 | ` */` |
|     152 |  3528 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3529 |  |
|       - |  3530 | `	JumpFixup sJump;` |
|       - |  3531 | `	sxi32 rc;` |
|     157 |  3532 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3533 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3534 | `		/* Missing label */` |
|     ! 0 |  3535 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3536 | `		if( rc == SXERR_ABORT ){` |
|       - |  3537 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3538 | `			return SXERR_ABORT;` |
|       - |  3539 | `		}` |
|     ! 0 |  3540 | `		return SXRET_OK;` |
|       - |  3541 | `	}` |
|     157 |  3542 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  3543 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       6 |  3544 | `		if( rc == SXERR_ABORT ){` |
|       - |  3545 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3546 | `			return SXERR_ABORT;` |
|       - |  3547 | `		}` |
|       4 |  3548 | `	}else{` |
|     153 |  3549 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3550 | `		GenBlock *pBlock;` |
|       - |  3551 | `		char *zDup;` |
|       - |  3552 | `		/* Prepare the jump destination */` |
|     153 |  3553 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3554 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3555 | `		/* Duplicate label name */` |
|     153 |  3556 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3557 | `		if( zDup == 0 ){` |
|     ! 0 |  3558 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3559 | `			return SXERR_ABORT;` |
|       - |  3560 | `		}` |
|     153 |  3561 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3562 | `		pBlock = pGen->pCurrent;` |
|     315 |  3563 | `		while( pBlock ){` |
|     199 |  3564 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      37 |  3565 | `				break;` |
|       - |  3566 | `			}` |
|       - |  3567 | `			/* Point to the upper block */` |
|     167 |  3568 | `			pBlock = pBlock->pParent;` |
|       5 |  3569 | `		}` |
|     153 |  3570 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       9 |  3571 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       9 |  3572 | `			if( rc == SXERR_ABORT ){` |
|       - |  3573 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3574 | `				return SXERR_ABORT;` |
|       - |  3575 | `			}` |
|       3 |  3576 | `		}` |
|     153 |  3577 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      30 |  3578 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      17 |  3579 | `		}else{` |
|     127 |  3580 | `			sJump.pFunc = 0;` |
|       - |  3581 | `		}` |
|       - |  3582 | `		/* Emit the unconditional jump */` |
|     153 |  3583 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3584 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3585 | `		}` |
|       - |  3586 | `	}` |
|     157 |  3587 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3588 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3589 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3590 | `	}` |
|       - |  3591 | `	/* Statement successfully compiled */` |
|     157 |  3592 | `	return SXRET_OK;` |
|      81 |  3593 |  |
|       - |  3594 | `/*` |
|       - |  3595 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3596 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3597 | ` * failure.` |
|       - |  3598 | ` */` |
|      20 |  3599 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  3600 |  |
|       - |  3601 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3602 | `	sxu32 nRawObj;` |
|      10 |  3603 | `	sxu32 nObjIdx;` |
|       - |  3604 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3605 | `	 * a PHP block.` |
|       - |  3606 | `	 */` |
|      10 |  3607 | `Consume:` |
|      22 |  3608 | `	nRawObj = nObjIdx = 0;` |
|      22 |  3609 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3610 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3611 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3612 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3613 | `			return SXERR_ABORT;` |
|       - |  3614 | `		}` |
|       - |  3615 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3616 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3617 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3618 | `		++nRawObj;` |
|     ! 0 |  3619 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3620 | `	}` |
|      22 |  3621 | `	if( nRawObj > 0 ){` |
|       - |  3622 | `		/* Emit the consume instruction */` |
|     ! 0 |  3623 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3624 | `	}` |
|      22 |  3625 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3626 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3627 | `		/* Reset the token set */` |
|     ! 0 |  3628 | `		SySetReset(pTokenSet);` |
|       - |  3629 | `		/* Tokenize input */` |
|     ! 0 |  3630 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3631 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3632 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3633 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3634 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3635 | `		/* Advance the stream cursor */` |
|     ! 0 |  3636 | `		pGen->pRawIn++;` |
|       - |  3637 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3638 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3639 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3640 | `			sxi32 rc;` |
|       - |  3641 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3642 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3643 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3644 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3645 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3646 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3647 | `				return SXERR_ABORT;` |
|     ! 0 |  3648 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3649 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3650 | `			}` |
|     ! 0 |  3651 | `			goto Consume;` |
|       - |  3652 | `		}` |
|     ! 0 |  3653 | `	}else{` |
|       - |  3654 | `		/* No more chunks to process */` |
|      22 |  3655 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  3656 | `		return SXERR_EOF;` |
|       - |  3657 | `	}` |
|     ! 0 |  3658 | `	return SXRET_OK;` |
|      12 |  3659 |  |
|       - |  3660 | `/*` |
|       - |  3661 | ` * Compile a PHP block.` |
|       - |  3662 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3663 | ` * optionally delimited by braces {}.` |
|       - |  3664 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3665 | ` * and this function takes care of generating the appropriate error` |
|       - |  3666 | ` * message.` |
|       - |  3667 | ` */` |
|  401336 |  3668 | `static sxi32 PH7_CompileBlock(` |
|       - |  3669 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3670 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3671 | `	)` |
|       5 |  3672 |  |
|       - |  3673 | `	sxi32 rc;` |
|       - |  3674 | `	sxu32 nLine;` |
|  401341 |  3675 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  399695 |  3676 | `		nLine = pGen->pIn->nLine;` |
|  399695 |  3677 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  399695 |  3678 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3679 | `			return SXERR_ABORT;` |
|       - |  3680 | `		}` |
|  399695 |  3681 | `		pGen->pIn++;` |
|       - |  3682 | `		/* Compile until we hit the closing braces '}' */` |
|  545890 |  3683 | `		for(;;){` |
| 1091785 |  3684 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  3685 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  3686 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3687 | `			 	   return SXERR_ABORT;` |
|       - |  3688 | `				}` |
|      22 |  3689 | `				if( rc == SXERR_EOF ){` |
|       - |  3690 | `					/* No more token to process. Missing closing braces */` |
|      22 |  3691 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      22 |  3692 | `					break;` |
|       - |  3693 | `				}` |
|     ! 0 |  3694 | `			}` |
| 1091765 |  3695 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3696 | `				/* Closing braces found,break immediately*/` |
|  399675 |  3697 | `				pGen->pIn++;` |
|  399675 |  3698 | `				break;` |
|       - |  3699 | `			}` |
|       - |  3700 | `			/* Compile a single statement */` |
|  692095 |  3701 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  692095 |  3702 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3703 | `				return SXERR_ABORT;` |
|       - |  3704 | `			}` |
|       5 |  3705 | `		}` |
|  399695 |  3706 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  201496 |  3707 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3708 | `		pGen->pIn++;` |
|     ! 0 |  3709 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3710 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3711 | `			return SXERR_ABORT;` |
|       - |  3712 | `		}` |
|       - |  3713 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3714 | `		for(;;){` |
|     ! 0 |  3715 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3716 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3717 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3718 | `			 	   return SXERR_ABORT;` |
|       - |  3719 | `				}` |
|     ! 0 |  3720 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3721 | `					/* No more token to process */` |
|     ! 0 |  3722 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3723 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3724 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3725 | `					}` |
|     ! 0 |  3726 | `					break;` |
|       - |  3727 | `				}` |
|     ! 0 |  3728 | `			}` |
|     ! 0 |  3729 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3730 | `				sxi32 nKwrd;` |
|       - |  3731 | `				/* Keyword found */` |
|     ! 0 |  3732 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3733 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3734 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3735 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3736 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3737 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3738 | `						}` |
|     ! 0 |  3739 | `						break;` |
|       - |  3740 | `				}` |
|     ! 0 |  3741 | `			}` |
|       - |  3742 | `			/* Compile a single statement */` |
|     ! 0 |  3743 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3744 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3745 | `				return SXERR_ABORT;` |
|       - |  3746 | `			}` |
|     ! 0 |  3747 | `		}` |
|     ! 0 |  3748 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3749 | `	}else{` |
|       - |  3750 | `		/* Compile a single statement */` |
|    1651 |  3751 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1651 |  3752 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3753 | `			return SXERR_ABORT;` |
|       - |  3754 | `		}` |
|       - |  3755 | `	}` |
|       - |  3756 | `	/* Jump trailing semi-colons ';' */` |
|  401341 |  3757 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3758 | `		pGen->pIn++;` |
|     ! 0 |  3759 | `	}` |
|  401341 |  3760 | `	return SXRET_OK;` |
|  200673 |  3761 |  |
|       - |  3762 | `/*` |
|       - |  3763 | ` * Compile the gentle 'while' statement.` |
|       - |  3764 | ` * According to the PHP language reference` |
|       - |  3765 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3766 | ` *  The basic form of a while statement is:` |
|       - |  3767 | ` *  while (expr)` |
|       - |  3768 | ` *   statement` |
|       - |  3769 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3770 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3771 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3772 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3773 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3774 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3775 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3776 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3777 | ` *  while (expr):` |
|       - |  3778 | ` *    statement` |
|       - |  3779 | ` *   endwhile;` |
|       - |  3780 | ` */` |
|   13344 |  3781 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3782 |  |
|   13349 |  3783 | `	GenBlock *pWhileBlock = 0;` |
|   13349 |  3784 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3785 | `	sxu32 nFalseJump;` |
|       - |  3786 | `	sxu32 nLine;` |
|       - |  3787 | `	sxi32 rc;` |
|   13349 |  3788 | `	nLine = pGen->pIn->nLine;` |
|       - |  3789 | `	/* Jump the 'while' keyword */` |
|   13349 |  3790 | `	pGen->pIn++;` |
|   13349 |  3791 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3792 | `		/* Syntax error */` |
|     ! 0 |  3793 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3794 | `		if( rc == SXERR_ABORT ){` |
|       - |  3795 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3796 | `			return SXERR_ABORT;` |
|       - |  3797 | `		}` |
|     ! 0 |  3798 | `		goto Synchronize;` |
|       - |  3799 | `	}` |
|       - |  3800 | `	/* Jump the left parenthesis '(' */` |
|   13349 |  3801 | `	pGen->pIn++;` |
|       - |  3802 | `	/* Create the loop block */` |
|   13349 |  3803 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   13349 |  3804 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3805 | `		return SXERR_ABORT;` |
|       - |  3806 | `	}` |
|       - |  3807 | `	/* Delimit the condition */` |
|   13349 |  3808 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   13349 |  3809 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3810 | `		/* Empty expression */` |
|       3 |  3811 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3812 | `		if( rc == SXERR_ABORT ){` |
|       - |  3813 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3814 | `			return SXERR_ABORT;` |
|       - |  3815 | `		}` |
|       1 |  3816 | `	}` |
|       - |  3817 | `	/* Swap token streams */` |
|   13349 |  3818 | `	pTmp = pGen->pEnd;` |
|   13349 |  3819 | `	pGen->pEnd = pEnd;` |
|       - |  3820 | `	/* Compile the expression */` |
|   13349 |  3821 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   13349 |  3822 | `	if( rc == SXERR_ABORT ){` |
|       - |  3823 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3824 | `		return SXERR_ABORT;` |
|       - |  3825 | `	}` |
|       - |  3826 | `	/* Update token stream */` |
|   13349 |  3827 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3828 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3829 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3830 | `			return SXERR_ABORT;` |
|       - |  3831 | `		}` |
|     ! 0 |  3832 | `		pGen->pIn++;` |
|     ! 0 |  3833 | `	}` |
|       - |  3834 | `	/* Synchronize pointers */` |
|   13349 |  3835 | `	pGen->pIn  = &pEnd[1];` |
|   13349 |  3836 | `	pGen->pEnd = pTmp;` |
|       - |  3837 | `	/* Emit the false jump */` |
|   13349 |  3838 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3839 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   13349 |  3840 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3841 | `	/* Compile the loop body */` |
|   13349 |  3842 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   13349 |  3843 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3844 | `		return SXERR_ABORT;` |
|       - |  3845 | `	}` |
|       - |  3846 | `	/* Emit the unconditional jump to the start of the loop */` |
|   13349 |  3847 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3848 | `	/* Fix all jumps now the destination is resolved */` |
|   13349 |  3849 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3850 | `	/* Release the loop block */` |
|   13349 |  3851 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3852 | `	/* Statement successfully compiled */` |
|   13349 |  3853 | `	return SXRET_OK;` |
|     ! 0 |  3854 | `Synchronize:` |
|       - |  3855 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3856 | `	 * compiling this erroneous block.` |
|       - |  3857 | `	 */` |
|     ! 0 |  3858 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3859 | `		pGen->pIn++;` |
|     ! 0 |  3860 | `	}` |
|     ! 0 |  3861 | `	return SXRET_OK;` |
|    6677 |  3862 |  |
|       - |  3863 | `/*` |
|       - |  3864 | ` * Compile the ugly do..while() statement.` |
|       - |  3865 | ` * According to the PHP language reference` |
|       - |  3866 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3867 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3868 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3869 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3870 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3871 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3872 | ` *  would end immediately).` |
|       - |  3873 | ` *  There is just one syntax for do-while loops:` |
|       - |  3874 | ` *  <?php` |
|       - |  3875 | ` *  $i = 0;` |
|       - |  3876 | ` *  do {` |
|       - |  3877 | ` *   echo $i;` |
|       - |  3878 | ` *  } while ($i > 0);` |
|       - |  3879 | ` * ?>` |
|       - |  3880 | ` */` |
|       2 |  3881 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3882 |  |
|       3 |  3883 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3884 | `	GenBlock *pDoBlock = 0;` |
|       - |  3885 | `	sxu32 nLine;` |
|       - |  3886 | `	sxi32 rc;` |
|       3 |  3887 | `	nLine = pGen->pIn->nLine;` |
|       - |  3888 | `	/* Jump the 'do' keyword */` |
|       3 |  3889 | `	pGen->pIn++;` |
|       - |  3890 | `	/* Create the loop block */` |
|       3 |  3891 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3892 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3893 | `		return SXERR_ABORT;` |
|       - |  3894 | `	}` |
|       - |  3895 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3896 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3897 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3898 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3899 | `		return SXERR_ABORT;` |
|       - |  3900 | `	}` |
|       3 |  3901 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3902 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3903 | `	}` |
|       3 |  3904 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3905 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3906 | `			/* Missing 'while' statement */` |
|       3 |  3907 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3908 | `			if( rc == SXERR_ABORT ){` |
|       - |  3909 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3910 | `				return SXERR_ABORT;` |
|       - |  3911 | `			}` |
|       3 |  3912 | `			goto Synchronize;` |
|       - |  3913 | `	}` |
|       - |  3914 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3915 | `	pGen->pIn++;` |
|     ! 0 |  3916 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3917 | `		/* Syntax error */` |
|     ! 0 |  3918 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3919 | `		if( rc == SXERR_ABORT ){` |
|       - |  3920 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3921 | `			return SXERR_ABORT;` |
|       - |  3922 | `		}` |
|     ! 0 |  3923 | `		goto Synchronize;` |
|       - |  3924 | `	}` |
|       - |  3925 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3926 | `	pGen->pIn++;` |
|       - |  3927 | `	/* Delimit the condition */` |
|     ! 0 |  3928 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3929 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3930 | `		/* Empty expression */` |
|     ! 0 |  3931 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3932 | `		if( rc == SXERR_ABORT ){` |
|       - |  3933 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3934 | `			return SXERR_ABORT;` |
|       - |  3935 | `		}` |
|     ! 0 |  3936 | `		goto Synchronize;` |
|       - |  3937 | `	}` |
|       - |  3938 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3939 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3940 | `		JumpFixup *aPost;` |
|       - |  3941 | `		VmInstr *pInstr;` |
|       - |  3942 | `		sxu32 nJumpDest;` |
|       - |  3943 | `		sxu32 n;` |
|     ! 0 |  3944 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3945 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3946 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3947 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3948 | `			if( pInstr ){` |
|       - |  3949 | `				/* Fix */` |
|     ! 0 |  3950 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3951 | `			}` |
|     ! 0 |  3952 | `		}` |
|     ! 0 |  3953 | `	}` |
|       - |  3954 | `	/* Swap token streams */` |
|     ! 0 |  3955 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3956 | `	pGen->pEnd = pEnd;` |
|       - |  3957 | `	/* Compile the expression */` |
|     ! 0 |  3958 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3959 | `	if( rc == SXERR_ABORT ){` |
|       - |  3960 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3961 | `		return SXERR_ABORT;` |
|       - |  3962 | `	}` |
|       - |  3963 | `	/* Update token stream */` |
|     ! 0 |  3964 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3965 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3966 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3967 | `			return SXERR_ABORT;` |
|       - |  3968 | `		}` |
|     ! 0 |  3969 | `		pGen->pIn++;` |
|     ! 0 |  3970 | `	}` |
|     ! 0 |  3971 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3972 | `	pGen->pEnd = pTmp;` |
|       - |  3973 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3974 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3975 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3976 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3977 | `	/* Release the loop block */` |
|     ! 0 |  3978 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3979 | `	/* Statement successfully compiled */` |
|     ! 0 |  3980 | `	return SXRET_OK;` |
|       1 |  3981 | `Synchronize:` |
|       - |  3982 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3983 | `	 * compiling this erroneous block.` |
|       - |  3984 | `	 */` |
|       3 |  3985 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3986 | `		pGen->pIn++;` |
|     ! 0 |  3987 | `	}` |
|       3 |  3988 | `	return SXRET_OK;` |
|       2 |  3989 |  |
|       - |  3990 | `/*` |
|       - |  3991 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3992 | ` * According to the PHP language reference` |
|       - |  3993 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3994 | ` *  The syntax of a for loop is:` |
|       - |  3995 | ` *  for (expr1; expr2; expr3)` |
|       - |  3996 | ` *   statement` |
|       - |  3997 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3998 | ` *  the beginning of the loop.` |
|       - |  3999 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  4000 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  4001 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  4002 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  4003 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  4004 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  4005 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  4006 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  4007 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  4008 | ` *  of using the for truth expression.` |
|       - |  4009 | ` */` |
|   13344 |  4010 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4011 |  |
|   13349 |  4012 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   13349 |  4013 | `	GenBlock *pForBlock = 0;` |
|       - |  4014 | `	sxu32 nFalseJump;` |
|       - |  4015 | `	sxu32 nLine;` |
|       - |  4016 | `	sxi32 rc;` |
|   13349 |  4017 | `	nLine = pGen->pIn->nLine;` |
|       - |  4018 | `	/* Jump the 'for' keyword */` |
|   13349 |  4019 | `	pGen->pIn++;` |
|   13349 |  4020 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4021 | `		/* Syntax error */` |
|     ! 0 |  4022 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4023 | `		if( rc == SXERR_ABORT ){` |
|       - |  4024 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4025 | `			return SXERR_ABORT;` |
|       - |  4026 | `		}` |
|     ! 0 |  4027 | `		return SXRET_OK;` |
|       - |  4028 | `	}` |
|       - |  4029 | `	/* Jump the left parenthesis '(' */` |
|   13349 |  4030 | `	pGen->pIn++;` |
|       - |  4031 | `	/* Delimit the init-expr;condition;post-expr */` |
|   13349 |  4032 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   13349 |  4033 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4034 | `		/* Empty expression */` |
|     ! 0 |  4035 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4036 | `		if( rc == SXERR_ABORT ){` |
|       - |  4037 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4038 | `			return SXERR_ABORT;` |
|       - |  4039 | `		}` |
|       - |  4040 | `		/* Synchronize */` |
|     ! 0 |  4041 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4042 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4043 | `			pGen->pIn++;` |
|     ! 0 |  4044 | `		}` |
|     ! 0 |  4045 | `		return SXRET_OK;` |
|       - |  4046 | `	}` |
|       - |  4047 | `	/* Swap token streams */` |
|   13349 |  4048 | `	pTmp = pGen->pEnd;` |
|   13349 |  4049 | `	pGen->pEnd = pEnd;` |
|       - |  4050 | `	/* Compile initialization expressions if available */` |
|   13349 |  4051 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4052 | `	/* Pop operand lvalues */` |
|   13349 |  4053 | `	if( rc == SXERR_ABORT ){` |
|       - |  4054 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4055 | `		return SXERR_ABORT;` |
|   13349 |  4056 | `	}else if( rc != SXERR_EMPTY ){` |
|   13347 |  4057 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6671 |  4058 | `	}` |
|   13349 |  4059 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4060 | `		/* Syntax error */` |
|     ! 0 |  4061 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4062 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4063 | `		if( rc == SXERR_ABORT ){` |
|       - |  4064 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4065 | `			return SXERR_ABORT;` |
|       - |  4066 | `		}` |
|     ! 0 |  4067 | `		return SXRET_OK;` |
|       - |  4068 | `	}` |
|       - |  4069 | `	/* Jump the trailing ';' */` |
|   13349 |  4070 | `	pGen->pIn++;` |
|       - |  4071 | `	/* Create the loop block */` |
|   13349 |  4072 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   13349 |  4073 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4074 | `		return SXERR_ABORT;` |
|       - |  4075 | `	}` |
|       - |  4076 | `	/* Deffer continue jumps */` |
|   13349 |  4077 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4078 | `	/* Compile the condition */` |
|   13349 |  4079 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   13349 |  4080 | `	if( rc == SXERR_ABORT ){` |
|       - |  4081 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4082 | `		return SXERR_ABORT;` |
|   13349 |  4083 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4084 | `		/* Emit the false jump */` |
|   13347 |  4085 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4086 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   13347 |  4087 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    6671 |  4088 | `	}` |
|   13349 |  4089 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4090 | `		/* Syntax error */` |
|       6 |  4091 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4092 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4093 | `		if( rc == SXERR_ABORT ){` |
|       - |  4094 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4095 | `			return SXERR_ABORT;` |
|       - |  4096 | `		}` |
|       6 |  4097 | `		return SXRET_OK;` |
|       - |  4098 | `	}` |
|       - |  4099 | `	/* Jump the trailing ';' */` |
|   13345 |  4100 | `	pGen->pIn++;` |
|       - |  4101 | `	/* Save the post condition stream */` |
|   13345 |  4102 | `	pPostStart = pGen->pIn;` |
|       - |  4103 | `	/* Compile the loop body */` |
|   13345 |  4104 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   13345 |  4105 | `	pGen->pEnd = pTmp;` |
|   13345 |  4106 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   13345 |  4107 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4108 | `		return SXERR_ABORT;` |
|       - |  4109 | `	}` |
|       - |  4110 | `	/* Fix post-continue jumps */` |
|   13345 |  4111 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4112 | `		JumpFixup *aPost;` |
|       - |  4113 | `		VmInstr *pInstr;` |
|       - |  4114 | `		sxu32 nJumpDest;` |
|       - |  4115 | `		sxu32 n;` |
|      14 |  4116 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4117 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4118 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4119 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4120 | `			if( pInstr ){` |
|       - |  4121 | `				/* Fix jump */` |
|      14 |  4122 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4123 | `			}` |
|       8 |  4124 | `		}` |
|       6 |  4125 | `	}` |
|       - |  4126 | `	/* compile the post-expressions if available */` |
|   13345 |  4127 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4128 | `		pPostStart++;` |
|     ! 0 |  4129 | `	}` |
|   13345 |  4130 | `	if( pPostStart < pEnd ){` |
|       - |  4131 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   13345 |  4132 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   13345 |  4133 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   13345 |  4134 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4135 | `			/* Syntax error */` |
|     ! 0 |  4136 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4137 | `			if( rc == SXERR_ABORT ){` |
|       - |  4138 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4139 | `				return SXERR_ABORT;` |
|       - |  4140 | `			}` |
|     ! 0 |  4141 | `			return SXRET_OK;` |
|       - |  4142 | `		}` |
|   13345 |  4143 | `		RE_SWAP_DELIMITER(pGen);` |
|   13345 |  4144 | `		if( rc == SXERR_ABORT ){` |
|       - |  4145 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4146 | `			return SXERR_ABORT;` |
|   13345 |  4147 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4148 | `			/* Pop operand lvalue */` |
|   13345 |  4149 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6670 |  4150 | `		}` |
|    6670 |  4151 | `	}` |
|       - |  4152 | `	/* Emit the unconditional jump to the start of the loop */` |
|   13345 |  4153 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4154 | `	/* Fix all jumps now the destination is resolved */` |
|   13345 |  4155 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4156 | `	/* Release the loop block */` |
|   13345 |  4157 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4158 | `	/* Statement successfully compiled */` |
|   13345 |  4159 | `	return SXRET_OK;` |
|    6677 |  4160 |  |
|       - |  4161 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4162 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4163 | ` * are allowed.` |
|       - |  4164 | ` */` |
|    7156 |  4165 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4166 |  |
|    7161 |  4167 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7161 |  4168 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4169 | `		/* Unexpected expression */` |
|     ! 0 |  4170 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4171 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4172 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4173 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4174 | `		}` |
|     ! 0 |  4175 | `	}` |
|    7161 |  4176 | `	return rc;` |
|       5 |  4177 |  |
|       - |  4178 | `/*` |
|       - |  4179 | ` * Compile the 'foreach' statement.` |
|       - |  4180 | ` * According to the PHP language reference` |
|       - |  4181 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4182 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4183 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4184 | ` *  is a minor but useful extension of the first:` |
|       - |  4185 | ` *  foreach (array_expression as $value)` |
|       - |  4186 | ` *    statement` |
|       - |  4187 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4188 | ` *   statement` |
|       - |  4189 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4190 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4191 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4192 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4193 | ` *  to the variable $key on each loop.` |
|       - |  4194 | ` *  Note:` |
|       - |  4195 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4196 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4197 | ` *  Note:` |
|       - |  4198 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4199 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4200 | ` *  or after the foreach without resetting it.` |
|       - |  4201 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4202 | ` *  of copying the value.` |
|       - |  4203 | ` */` |
|    3666 |  4204 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4205 |  |
|    3671 |  4206 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3671 |  4207 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3671 |  4208 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4209 | `	ph7_foreach_info *pInfo;` |
|       - |  4210 | `	sxu32 nFalseJump;` |
|       - |  4211 | `	VmInstr *pInstr;` |
|       - |  4212 | `	sxu32 nLine;` |
|       - |  4213 | `	sxi32 rc;` |
|    3671 |  4214 | `	nLine = pGen->pIn->nLine;` |
|       - |  4215 | `	/* Jump the 'foreach' keyword */` |
|    3671 |  4216 | `	pGen->pIn++;` |
|    3671 |  4217 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4218 | `		/* Syntax error */` |
|     ! 0 |  4219 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4220 | `		if( rc == SXERR_ABORT ){` |
|       - |  4221 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4222 | `			return SXERR_ABORT;` |
|       - |  4223 | `		}` |
|     ! 0 |  4224 | `		goto Synchronize;` |
|       - |  4225 | `	}` |
|       - |  4226 | `	/* Jump the left parenthesis '(' */` |
|    3671 |  4227 | `	pGen->pIn++;` |
|       - |  4228 | `	/* Create the loop block */` |
|    3671 |  4229 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3671 |  4230 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4231 | `		return SXERR_ABORT;` |
|       - |  4232 | `	}` |
|       - |  4233 | `	/* Delimit the expression */` |
|    3671 |  4234 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3671 |  4235 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4236 | `		/* Empty expression */` |
|     ! 0 |  4237 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4238 | `		if( rc == SXERR_ABORT ){` |
|       - |  4239 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4240 | `			return SXERR_ABORT;` |
|       - |  4241 | `		}` |
|       - |  4242 | `		/* Synchronize */` |
|     ! 0 |  4243 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4244 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4245 | `			pGen->pIn++;` |
|     ! 0 |  4246 | `		}` |
|     ! 0 |  4247 | `		return SXRET_OK;` |
|       - |  4248 | `	}` |
|       - |  4249 | `	/* Compile the array expression */` |
|    3671 |  4250 | `	pCur = pGen->pIn;` |
|   25221 |  4251 | `	while( pCur < pEnd ){` |
|   25221 |  4252 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3685 |  4253 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3685 |  4254 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4255 | `				/* Break with the first 'as' found */` |
|    3671 |  4256 | `				break;` |
|       - |  4257 | `			}` |
|       7 |  4258 | `		}` |
|       - |  4259 | `		/* Advance the stream cursor */` |
|   21555 |  4260 | `		pCur++;` |
|       5 |  4261 | `	}` |
|    3671 |  4262 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4263 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4264 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4265 | `		if( rc == SXERR_ABORT ){` |
|       - |  4266 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4267 | `			return SXERR_ABORT;` |
|       - |  4268 | `		}` |
|     ! 0 |  4269 | `		goto Synchronize;` |
|       - |  4270 | `	}` |
|       - |  4271 | `	/* Swap token streams */` |
|    3671 |  4272 | `	pTmp = pGen->pEnd;` |
|    3671 |  4273 | `	pGen->pEnd = pCur;` |
|    3671 |  4274 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3671 |  4275 | `	if( rc == SXERR_ABORT ){` |
|       - |  4276 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4277 | `		return SXERR_ABORT;` |
|       - |  4278 | `	}` |
|       - |  4279 | `	/* Update token stream */` |
|    3671 |  4280 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4281 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4282 | `		if( rc == SXERR_ABORT ){` |
|       - |  4283 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4284 | `			return SXERR_ABORT;` |
|       - |  4285 | `		}` |
|     ! 0 |  4286 | `		pGen->pIn++;` |
|     ! 0 |  4287 | `	}` |
|    3671 |  4288 | `	pCur++; /* Jump the 'as' keyword */` |
|    3671 |  4289 | `	pGen->pIn = pCur;` |
|    3671 |  4290 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4291 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4292 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4293 | `			return SXERR_ABORT;` |
|       - |  4294 | `		}` |
|     ! 0 |  4295 | `	}` |
|       - |  4296 | `	/* Create the foreach context */` |
|    3671 |  4297 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3671 |  4298 | `	if( pInfo == 0 ){` |
|     ! 0 |  4299 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4300 | `		return SXERR_ABORT;` |
|       - |  4301 | `	}` |
|       - |  4302 | `	/* Zero the structure */` |
|    3671 |  4303 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4304 | `	/* Initialize structure fields */` |
|    3671 |  4305 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4306 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4307 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4308 | `	 * '=>'. */` |
|    3671 |  4309 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    3671 |  4310 | `	if( pCur < pEnd ){` |
|       - |  4311 | `		/* Compile the expression holding the key name */` |
|    3507 |  4312 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4313 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4314 | `			if( rc == SXERR_ABORT ){` |
|       - |  4315 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4316 | `				return SXERR_ABORT;` |
|       - |  4317 | `			}` |
|     ! 0 |  4318 | `		}else{` |
|    3507 |  4319 | `			pGen->pEnd = pCur;` |
|    3507 |  4320 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3507 |  4321 | `			if( rc == SXERR_ABORT ){` |
|       - |  4322 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4323 | `				return SXERR_ABORT;` |
|       - |  4324 | `			}` |
|    3507 |  4325 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3507 |  4326 | `			if( pInstr->p3 ){` |
|       - |  4327 | `				/* Record key name */` |
|    3507 |  4328 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1751 |  4329 | `			}` |
|    3507 |  4330 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4331 | `		}` |
|    3507 |  4332 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1751 |  4333 | `	}` |
|    3671 |  4334 | `	pGen->pEnd = pEnd;` |
|    3671 |  4335 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4336 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4337 | `		if( rc == SXERR_ABORT ){` |
|       - |  4338 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4339 | `			return SXERR_ABORT;` |
|       - |  4340 | `		}` |
|     ! 0 |  4341 | `		goto Synchronize;` |
|       - |  4342 | `	}` |
|    3671 |  4343 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4344 | `		pGen->pIn++;` |
|       - |  4345 | `		/* Pass by reference  */` |
|      11 |  4346 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4347 | `	}` |
|       - |  4348 | `	/* Check if the value target is list() */` |
|    3671 |  4349 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4350 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4351 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4352 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4353 | `		 */` |
|       - |  4354 | `		static int iForeachListCnt = 0;` |
|       - |  4355 | `		char zTmp[128];` |
|       - |  4356 | `		sxu32 nLen;` |
|       - |  4357 | `		char *zDup;` |
|      10 |  4358 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4359 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4360 | `		if( zDup == 0 ){` |
|     ! 0 |  4361 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4362 | `			return SXERR_ABORT;` |
|       - |  4363 | `		}` |
|      10 |  4364 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4365 | `		/* Save list() token boundaries */` |
|      10 |  4366 | `		pListStart = pGen->pIn;` |
|       - |  4367 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4368 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4369 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4370 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4371 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4372 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4373 | `				return SXERR_ABORT;` |
|       - |  4374 | `			}` |
|       3 |  4375 | `			goto Synchronize;` |
|       - |  4376 | `		}` |
|       7 |  4377 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4378 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4379 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4380 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4381 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4382 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4383 | `				return SXERR_ABORT;` |
|       - |  4384 | `			}` |
|     ! 0 |  4385 | `			goto Synchronize;` |
|       - |  4386 | `		}` |
|       7 |  4387 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4388 | `		pListEnd = pGen->pIn;` |
|       7 |  4389 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3666 |  4390 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4391 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4392 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4393 | `		 */` |
|       - |  4394 | `		static int iForeachShortListCnt = 0;` |
|       - |  4395 | `		char zTmp[128];` |
|       - |  4396 | `		sxu32 nLen;` |
|       - |  4397 | `		char *zDup;` |
|       5 |  4398 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       5 |  4399 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       5 |  4400 | `		if( zDup == 0 ){` |
|     ! 0 |  4401 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4402 | `			return SXERR_ABORT;` |
|       - |  4403 | `		}` |
|       5 |  4404 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4405 | `		/* Save [...] token boundaries */` |
|       5 |  4406 | `		pListStart = pGen->pIn;` |
|       - |  4407 | `		/* Advance past [...] */` |
|       5 |  4408 | `		pGen->pIn++; /* Jump '[' */` |
|       5 |  4409 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       5 |  4410 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4411 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4412 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4413 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4414 | `				return SXERR_ABORT;` |
|       - |  4415 | `			}` |
|     ! 0 |  4416 | `			goto Synchronize;` |
|       - |  4417 | `		}` |
|       5 |  4418 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       5 |  4419 | `		pListEnd = pGen->pIn;` |
|       5 |  4420 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       3 |  4421 | `	}else{` |
|       - |  4422 | `		/* Compile the expression holding the value name */` |
|    3659 |  4423 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3659 |  4424 | `		if( rc == SXERR_ABORT ){` |
|       - |  4425 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4426 | `			return SXERR_ABORT;` |
|       - |  4427 | `		}` |
|    3659 |  4428 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3659 |  4429 | `		if( pInstr->p3 ){` |
|       - |  4430 | `			/* Record value name */` |
|    3659 |  4431 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1827 |  4432 | `		}` |
|       - |  4433 | `	}` |
|       - |  4434 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3669 |  4435 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4436 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3669 |  4437 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4438 | `	/* Record the first instruction to execute */` |
|    3669 |  4439 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4440 | `	/* Emit the FOREACH_STEP instruction */` |
|    3669 |  4441 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4442 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3669 |  4443 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4444 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3669 |  4445 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4446 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4447 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4448 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4449 | `		 */` |
|      11 |  4450 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4451 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4452 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4453 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4454 | `		 */` |
|      11 |  4455 | `		pSavedIn = pGen->pIn;` |
|      11 |  4456 | `		pSavedEnd = pGen->pEnd;` |
|      11 |  4457 | `		pGen->pIn = pListStart;` |
|      11 |  4458 | `		pGen->pEnd = pListEnd;` |
|      11 |  4459 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       5 |  4460 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       3 |  4461 | `		}else{` |
|       7 |  4462 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4463 | `		}` |
|      11 |  4464 | `		pGen->pIn = pSavedIn;` |
|      11 |  4465 | `		pGen->pEnd = pSavedEnd;` |
|      11 |  4466 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4467 | `			return SXERR_ABORT;` |
|       - |  4468 | `		}` |
|       - |  4469 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      11 |  4470 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       5 |  4471 | `	}` |
|       - |  4472 | `	/* Compile the loop body */` |
|    3669 |  4473 | `	pGen->pIn = &pEnd[1];` |
|    3669 |  4474 | `	pGen->pEnd = pTmp;` |
|    3669 |  4475 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3669 |  4476 | `	if( rc == SXERR_ABORT ){` |
|       - |  4477 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4478 | `		return SXERR_ABORT;` |
|       - |  4479 | `	}` |
|       - |  4480 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3669 |  4481 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4482 | `	/* Fix all jumps now the destination is resolved */` |
|    3669 |  4483 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4484 | `	/* Release the loop block */` |
|    3669 |  4485 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4486 | `	/* Statement successfully compiled */` |
|    3669 |  4487 | `	return SXRET_OK;` |
|       1 |  4488 | `Synchronize:` |
|       - |  4489 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4490 | `	 * compiling this erroneous block.` |
|       - |  4491 | `	 */` |
|       3 |  4492 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4493 | `		pGen->pIn++;` |
|     ! 0 |  4494 | `	}` |
|       3 |  4495 | `	return SXRET_OK;` |
|    1838 |  4496 |  |
|       - |  4497 | `/*` |
|       - |  4498 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4499 | ` * According to the PHP language reference` |
|       - |  4500 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4501 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4502 | ` *  that is similar to that of C:` |
|       - |  4503 | ` *  if (expr)` |
|       - |  4504 | ` *   statement` |
|       - |  4505 | ` *  else construct:` |
|       - |  4506 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4507 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4508 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4509 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4510 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4511 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4512 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4513 | ` *  elseif` |
|       - |  4514 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4515 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4516 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4517 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4518 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4519 | ` *   <?php` |
|       - |  4520 | ` *    if ($a > $b) {` |
|       - |  4521 | ` *     echo "a is bigger than b";` |
|       - |  4522 | ` *    } elseif ($a == $b) {` |
|       - |  4523 | ` *     echo "a is equal to b";` |
|       - |  4524 | ` *    } else {` |
|       - |  4525 | ` *     echo "a is smaller than b";` |
|       - |  4526 | ` *    }` |
|       - |  4527 | ` *    ?>` |
|       - |  4528 | ` */` |
|  138788 |  4529 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4530 |  |
|  138793 |  4531 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  138793 |  4532 | `	GenBlock *pCondBlock = 0;` |
|       - |  4533 | `	sxu32 nJumpIdx;` |
|       - |  4534 | `	sxu32 nKeyID;` |
|       - |  4535 | `	sxi32 rc;` |
|       - |  4536 | `	/* Jump the 'if' keyword */` |
|  138793 |  4537 | `	pGen->pIn++;` |
|  138793 |  4538 | `	pToken = pGen->pIn;` |
|       - |  4539 | `	/* Create the conditional block */` |
|  138793 |  4540 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  138793 |  4541 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4542 | `		return SXERR_ABORT;` |
|       - |  4543 | `	}` |
|       - |  4544 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   76063 |  4545 | `	for(;;){` |
|  152131 |  4546 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4547 | `			/* Syntax error */` |
|     ! 0 |  4548 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4549 | `				pToken--;` |
|     ! 0 |  4550 | `			}` |
|     ! 0 |  4551 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4552 | `			if( rc == SXERR_ABORT ){` |
|       - |  4553 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4554 | `				return SXERR_ABORT;` |
|       - |  4555 | `			}` |
|     ! 0 |  4556 | `			goto Synchronize;` |
|       - |  4557 | `		}` |
|       - |  4558 | `		/* Jump the left parenthesis '(' */` |
|  152131 |  4559 | `		pToken++;` |
|       - |  4560 | `		/* Delimit the condition */` |
|  152131 |  4561 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  152131 |  4562 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4563 | `			/* Syntax error */` |
|     ! 0 |  4564 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4565 | `				pToken--;` |
|     ! 0 |  4566 | `			}` |
|     ! 0 |  4567 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4568 | `			if( rc == SXERR_ABORT ){` |
|       - |  4569 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4570 | `				return SXERR_ABORT;` |
|       - |  4571 | `			}` |
|     ! 0 |  4572 | `			goto Synchronize;` |
|       - |  4573 | `		}` |
|       - |  4574 | `		/* Swap token streams */` |
|  152131 |  4575 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4576 | `		/* Compile the condition */` |
|  152131 |  4577 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4578 | `		/* Update token stream */` |
|  152131 |  4579 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4580 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4581 | `			pGen->pIn++;` |
|     ! 0 |  4582 | `		}` |
|  152131 |  4583 | `		pGen->pIn  = &pEnd[1];` |
|  152131 |  4584 | `		pGen->pEnd = pTmp;` |
|  152131 |  4585 | `		if( rc == SXERR_ABORT ){` |
|       - |  4586 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4587 | `			return SXERR_ABORT;` |
|       - |  4588 | `		}` |
|       - |  4589 | `		/* Emit the false jump */` |
|  152131 |  4590 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4591 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  152131 |  4592 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4593 | `		/* Compile the body */` |
|  152131 |  4594 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  152131 |  4595 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4596 | `			return SXERR_ABORT;` |
|       - |  4597 | `		}` |
|  152131 |  4598 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   42410 |  4599 | `			break;` |
|       - |  4600 | `		}` |
|       - |  4601 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   67321 |  4602 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   67321 |  4603 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   43301 |  4604 | `			break;` |
|       - |  4605 | `		}` |
|       - |  4606 | `		/* Emit the unconditional jump */` |
|   24025 |  4607 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4608 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   24025 |  4609 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   24025 |  4610 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   17299 |  4611 | `			pToken = &pGen->pIn[1];` |
|   17299 |  4612 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    6664 |  4613 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5346 |  4614 | `					break;` |
|       - |  4615 | `			}` |
|    6617 |  4616 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3306 |  4617 | `		}` |
|   13343 |  4618 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4619 | `		/* Synchronize cursors */` |
|   13343 |  4620 | `		pToken = pGen->pIn;` |
|       - |  4621 | `		/* Fix the false jump */` |
|   13343 |  4622 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4623 | `	} /* For(;;) */` |
|       - |  4624 | `	/* Fix the false jump */` |
|  138793 |  4625 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  138793 |  4626 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   53978 |  4627 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4628 | `			/* Compile the else block */` |
|   10687 |  4629 | `			pGen->pIn++;` |
|   10687 |  4630 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   10687 |  4631 | `			if( rc == SXERR_ABORT ){` |
|       - |  4632 |  |
|     ! 0 |  4633 | `				return SXERR_ABORT;` |
|       - |  4634 | `			}` |
|    5341 |  4635 | `	}` |
|  138793 |  4636 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4637 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  138793 |  4638 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4639 | `	/* Release the conditional block */` |
|  138793 |  4640 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4641 | `	/* Statement successfully compiled */` |
|  138793 |  4642 | `	return SXRET_OK;` |
|     ! 0 |  4643 | `Synchronize:` |
|       - |  4644 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4645 | `	 */` |
|     ! 0 |  4646 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4647 | `		pGen->pIn++;` |
|     ! 0 |  4648 | `	}` |
|     ! 0 |  4649 | `	return SXRET_OK;` |
|   69399 |  4650 |  |
|       - |  4651 | `/*` |
|       - |  4652 | ` * Compile the global construct.` |
|       - |  4653 | ` * According to the PHP language reference` |
|       - |  4654 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4655 | ` *  to be used in that function.` |
|       - |  4656 | ` *  Example #1 Using global` |
|       - |  4657 | ` *  <?php` |
|       - |  4658 | ` *   $a = 1;` |
|       - |  4659 | ` *   $b = 2;` |
|       - |  4660 | ` *   function Sum()` |
|       - |  4661 | ` *   {` |
|       - |  4662 | ` *    global $a, $b;` |
|       - |  4663 | ` *    $b = $a + $b;` |
|       - |  4664 | ` *   }` |
|       - |  4665 | ` *   Sum();` |
|       - |  4666 | ` *   echo $b;` |
|       - |  4667 | ` *  ?>` |
|       - |  4668 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4669 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4670 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4671 | ` */` |
|      36 |  4672 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4673 |  |
|      41 |  4674 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4675 | `	sxi32 nExpr;` |
|       - |  4676 | `	sxi32 rc;` |
|       - |  4677 | `	/* Jump the 'global' keyword */` |
|      41 |  4678 | `	pGen->pIn++;` |
|      41 |  4679 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4680 | `		/* Nothing to process */` |
|     ! 0 |  4681 | `		return SXRET_OK;` |
|       - |  4682 | `	}` |
|      41 |  4683 | `	pTmp = pGen->pEnd;` |
|      41 |  4684 | `	nExpr = 0;` |
|      87 |  4685 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4686 | `		if( pGen->pIn < pNext ){` |
|      51 |  4687 | `			pGen->pEnd = pNext;` |
|      51 |  4688 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4689 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4690 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4691 | `					return SXERR_ABORT;` |
|       - |  4692 | `				}` |
|     ! 0 |  4693 | `			}else{` |
|      51 |  4694 | `				pGen->pIn++;` |
|      51 |  4695 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4696 | `					/* Emit a warning */` |
|     ! 0 |  4697 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4698 | `				}else{` |
|      51 |  4699 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4700 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4701 | `						return SXERR_ABORT;` |
|      51 |  4702 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4703 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4704 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4705 | `							/* Variable name, not a constant */` |
|      51 |  4706 | `							pLast->iP1 = 0;` |
|      23 |  4707 | `						}` |
|      51 |  4708 | `						nExpr++;` |
|      23 |  4709 | `					}` |
|       - |  4710 | `				}` |
|       - |  4711 | `			}` |
|      23 |  4712 | `		}` |
|       - |  4713 | `		/* Next expression in the stream */` |
|      51 |  4714 | `		pGen->pIn = pNext;` |
|       - |  4715 | `		/* Jump trailing commas */` |
|      61 |  4716 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4717 | `			pGen->pIn++;` |
|       5 |  4718 | `		}` |
|       5 |  4719 | `	}` |
|       - |  4720 | `	/* Restore token stream */` |
|      41 |  4721 | `	pGen->pEnd = pTmp;` |
|      41 |  4722 | `	if( nExpr > 0 ){` |
|       - |  4723 | `		/* Emit the uplink instruction */` |
|      41 |  4724 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4725 | `	}` |
|      41 |  4726 | `	return SXRET_OK;` |
|      23 |  4727 |  |
|       - |  4728 | `/*` |
|       - |  4729 | ` * Compile the return statement.` |
|       - |  4730 | ` * According to the PHP language reference` |
|       - |  4731 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4732 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4733 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4734 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4735 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4736 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4737 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4738 | ` *  from within the main script file, then script execution end.` |
|       - |  4739 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4740 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4741 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4742 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4743 | ` */` |
|  219328 |  4744 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4745 |  |
|  219333 |  4746 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4747 | `	sxi32 rc;` |
|       - |  4748 | `	/* Jump the 'return' keyword */` |
|  219333 |  4749 | `	pGen->pIn++;` |
|  219333 |  4750 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4751 | `		/* Compile the expression */` |
|  219307 |  4752 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  219307 |  4753 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4754 | `			return SXERR_ABORT;` |
|  219307 |  4755 | `		}else if(rc != SXERR_EMPTY ){` |
|  219307 |  4756 | `			nRet = 1;` |
|  109651 |  4757 | `		}` |
|  109651 |  4758 | `	}` |
|       - |  4759 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4760 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4761 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4762 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4763 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  219333 |  4764 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  219333 |  4765 | `	return SXRET_OK;` |
|  109669 |  4766 |  |
|       - |  4767 | `/*` |
|       - |  4768 | ` * Compile a yield expression.` |
|       - |  4769 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4770 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4771 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4772 | ` */` |
|      72 |  4773 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4774 |  |
|       - |  4775 | `	SyToken *pTmp, *pSplit;` |
|      77 |  4776 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      77 |  4777 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4778 | `	sxi32 rc;` |
|      36 |  4779 | `	(void)iCompileFlag;` |
|       - |  4780 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      77 |  4781 | `	pGen->pIn++;` |
|       - |  4782 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4783 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      77 |  4784 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4785 | `		/* Bare yield — no value */` |
|     ! 0 |  4786 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4787 | `		return SXRET_OK;` |
|       - |  4788 | `	}` |
|       - |  4789 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      77 |  4790 | `	pSplit = 0;` |
|       - |  4791 | `	{` |
|      77 |  4792 | `		SyToken *pCur = pGen->pIn;` |
|      77 |  4793 | `		sxi32 nNest = 0;` |
|     163 |  4794 | `		while( pCur < pGen->pEnd ){` |
|     105 |  4795 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4796 | `				nNest++;` |
|     105 |  4797 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4798 | `				nNest--;` |
|     105 |  4799 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4800 | `				pSplit = pCur;` |
|      16 |  4801 | `				break;` |
|       - |  4802 | `			}` |
|      91 |  4803 | `			pCur++;` |
|       5 |  4804 | `		}` |
|       - |  4805 | `	}` |
|      77 |  4806 | `	pTmp = pGen->pEnd;` |
|      77 |  4807 | `	if( pSplit ){` |
|       - |  4808 | `		/* yield $key => $value */` |
|      16 |  4809 | `		pGen->pEnd = pSplit;` |
|      16 |  4810 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4811 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4812 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4813 | `		pGen->pEnd = pTmp;` |
|      16 |  4814 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4815 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4816 | `		iP1 = 1;` |
|      16 |  4817 | `		iP2 = 1;` |
|       9 |  4818 | `	}else{` |
|       - |  4819 | `		/* yield $value */` |
|      63 |  4820 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      63 |  4821 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      63 |  4822 | `		if( rc != SXERR_EMPTY ){` |
|      63 |  4823 | `			iP1 = 1;` |
|      29 |  4824 | `		}` |
|       - |  4825 | `	}` |
|      77 |  4826 | `	pGen->pEnd = pTmp;` |
|      77 |  4827 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      77 |  4828 | `	return SXRET_OK;` |
|      41 |  4829 |  |
|       - |  4830 | `/*` |
|       - |  4831 | ` * Compile the die/exit language construct.` |
|       - |  4832 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4833 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4834 | ` */` |
|     120 |  4835 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4836 |  |
|     125 |  4837 | `	sxi32 nExpr = 0;` |
|       - |  4838 | `	sxi32 rc;` |
|       - |  4839 | `	/* Jump the die/exit keyword */` |
|     125 |  4840 | `	pGen->pIn++;` |
|     125 |  4841 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4842 | `		/* Compile the expression */` |
|     125 |  4843 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4844 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4845 | `			return SXERR_ABORT;` |
|     125 |  4846 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4847 | `			nExpr = 1;` |
|      60 |  4848 | `		}` |
|      60 |  4849 | `	}` |
|       - |  4850 | `	/* Emit the HALT instruction */` |
|     125 |  4851 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4852 | `	return SXRET_OK;` |
|      65 |  4853 |  |
|       - |  4854 | `/*` |
|       - |  4855 | ` * Compile the 'echo' language construct.` |
|       - |  4856 | ` */` |
|   13916 |  4857 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4858 |  |
|   13921 |  4859 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4860 | `	sxi32 rc;` |
|       - |  4861 | `	/* Jump the 'echo' keyword */` |
|   13921 |  4862 | `	pGen->pIn++;` |
|       - |  4863 | `	/* Compile arguments one after one */` |
|   13921 |  4864 | `	pTmp = pGen->pEnd;` |
|   30303 |  4865 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   16387 |  4866 | `		if( pGen->pIn < pNext ){` |
|   16387 |  4867 | `			pGen->pEnd = pNext;` |
|   16387 |  4868 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   16387 |  4869 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4870 | `				return SXERR_ABORT;` |
|   16387 |  4871 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4872 | `				/* Emit the consume instruction */` |
|   16363 |  4873 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    8179 |  4874 | `			}` |
|    8191 |  4875 | `		}` |
|       - |  4876 | `		/* Jump trailing commas */` |
|   18853 |  4877 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    2471 |  4878 | `			pNext++;` |
|       5 |  4879 | `		}` |
|   16387 |  4880 | `		pGen->pIn = pNext;` |
|       5 |  4881 | `	}` |
|       - |  4882 | `	/* Restore token stream */` |
|   13921 |  4883 | `	pGen->pEnd = pTmp;` |
|   13921 |  4884 | `	return SXRET_OK;` |
|    6963 |  4885 |  |
|       - |  4886 | `/*` |
|       - |  4887 | ` * Compile the static statement.` |
|       - |  4888 | ` * According to the PHP language reference` |
|       - |  4889 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4890 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4891 | ` *  when program execution leaves this scope.` |
|       - |  4892 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4893 | ` * Symisc eXtension.` |
|       - |  4894 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4895 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4896 | ` *  Example` |
|       - |  4897 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4898 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4899 | ` */` |
|       6 |  4900 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       2 |  4901 |  |
|       - |  4902 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4903 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4904 | `	GenBlock *pBlock;` |
|       - |  4905 | `	SyString *pName;` |
|       - |  4906 | `	char *zDup;` |
|       - |  4907 | `	sxu32 nLine;` |
|       - |  4908 | `	sxi32 rc;` |
|       - |  4909 | `	/* Jump the static keyword */` |
|       8 |  4910 | `	nLine = pGen->pIn->nLine;` |
|       8 |  4911 | `	pGen->pIn++;` |
|       - |  4912 | `	/* Extract the enclosing function if any */` |
|       8 |  4913 | `	pBlock = pGen->pCurrent;` |
|      14 |  4914 | `	while( pBlock ){` |
|      14 |  4915 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       8 |  4916 | `			break;` |
|       - |  4917 | `		}` |
|       - |  4918 | `		/* Point to the upper block */` |
|       8 |  4919 | `		pBlock = pBlock->pParent;` |
|       2 |  4920 | `	}` |
|       8 |  4921 | `	if( pBlock == 0 ){` |
|       - |  4922 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4923 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4924 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4925 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4926 | `				return SXERR_ABORT;` |
|       - |  4927 | `			}` |
|     ! 0 |  4928 | `			goto Synchronize;` |
|       - |  4929 | `		}` |
|       - |  4930 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4931 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4932 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4933 | `			return SXERR_ABORT;` |
|     ! 0 |  4934 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4935 | `			/* Emit the POP instruction */` |
|     ! 0 |  4936 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4937 | `		}` |
|     ! 0 |  4938 | `		return SXRET_OK;` |
|       - |  4939 | `	}` |
|       8 |  4940 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4941 | `	/* Make sure we are dealing with a valid statement */` |
|       8 |  4942 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       4 |  4943 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4944 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4945 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4946 | `				return SXERR_ABORT;` |
|       - |  4947 | `			}` |
|       3 |  4948 | `			goto Synchronize;` |
|       - |  4949 | `	}` |
|       5 |  4950 | `	pGen->pIn++;` |
|       - |  4951 | `	/* Extract variable name */` |
|       5 |  4952 | `	pName = &pGen->pIn->sData;` |
|       5 |  4953 | `	pGen->pIn++; /* Jump the var name */` |
|       5 |  4954 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4955 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4956 | `		goto Synchronize;` |
|       - |  4957 | `	}` |
|       - |  4958 | `	/* Initialize the structure describing the static variable */` |
|       5 |  4959 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       5 |  4960 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4961 | `	/* Duplicate variable name */` |
|       5 |  4962 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       5 |  4963 | `	if( zDup == 0 ){` |
|     ! 0 |  4964 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4965 | `		return SXERR_ABORT;` |
|       - |  4966 | `	}` |
|       5 |  4967 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4968 | `	/* Check if we have an expression to compile */` |
|       5 |  4969 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4970 | `		SySet *pInstrContainer;` |
|       - |  4971 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4972 | `		 * Static variable can take any complex expression including function` |
|       - |  4973 | `		 * call as their initialization value.` |
|       - |  4974 | `		 * Example:` |
|       - |  4975 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4976 | `		 */` |
|       5 |  4977 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4978 | `		/* Swap bytecode container */` |
|       5 |  4979 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       5 |  4980 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4981 | `		/* Compile the expression */` |
|       5 |  4982 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4983 | `		/* Emit the done instruction */` |
|       5 |  4984 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4985 | `		/* Restore default bytecode container */` |
|       5 |  4986 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       2 |  4987 | `	}` |
|       - |  4988 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       5 |  4989 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       5 |  4990 | `	return SXRET_OK;` |
|       1 |  4991 | `Synchronize:` |
|       - |  4992 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  4993 | `	 * statement.` |
|       - |  4994 | `	 */` |
|       5 |  4995 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  4996 | `		pGen->pIn++;` |
|       1 |  4997 | `	}` |
|       3 |  4998 | `	return SXRET_OK;` |
|       5 |  4999 |  |
|       - |  5000 | `/*` |
|       - |  5001 | ` * Compile the var statement.` |
|       - |  5002 | ` * Symisc Extension:` |
|       - |  5003 | ` *      var statement can be used outside of a class definition.` |
|       - |  5004 | ` */` |
|       4 |  5005 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5006 |  |
|       - |  5007 | `	sxu32 nLine;` |
|       - |  5008 | `	sxi32 rc;` |
|       5 |  5009 | `	nLine = pGen->pIn->nLine;` |
|       - |  5010 | `	/* Jump the 'var' keyword */` |
|       5 |  5011 | `	pGen->pIn++;` |
|       5 |  5012 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5013 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5014 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5015 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5016 | `			pGen->pIn++;` |
|     ! 0 |  5017 | `		}` |
|     ! 0 |  5018 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5019 | `			return SXERR_ABORT;` |
|       - |  5020 | `		}` |
|     ! 0 |  5021 | `	}else{` |
|       - |  5022 | `		/* Compile the expression */` |
|       5 |  5023 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5024 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5025 | `			return SXERR_ABORT;` |
|       5 |  5026 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5027 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5028 | `		}` |
|       - |  5029 | `	}` |
|       5 |  5030 | `	return SXRET_OK;` |
|       3 |  5031 |  |
|       - |  5032 | `/*` |
|       - |  5033 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5034 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5035 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5036 | ` */` |
|       - |  5037 | `/*` |
|       - |  5038 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5039 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5040 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5041 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5042 | ` *` |
|       - |  5043 | ` * Resolution order:` |
|       - |  5044 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5045 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5046 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5047 | ` *` |
|       - |  5048 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5049 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5050 | ` * Returns the (possibly new) literal index.` |
|       - |  5051 | ` */` |
|  409732 |  5052 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5053 |  |
|       - |  5054 | `	ph7_value *pLit;` |
|       - |  5055 | `	const char *zLit;` |
|       - |  5056 | `	SyString sQualified;` |
|       - |  5057 | `	sxu32 nLit;` |
|       - |  5058 | `	sxu32 k;` |
|       - |  5059 | `	sxu32 nNewIdx;` |
|       - |  5060 | `	int hasNsSep;` |
|       - |  5061 | `	SyHashEntry *pImport;` |
|       - |  5062 | `	ph7_value *pNew;` |
|  409737 |  5063 | `	if( pFromImport ){` |
|  391685 |  5064 | `		*pFromImport = 0;` |
|  195840 |  5065 | `	}` |
|  409737 |  5066 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  409737 |  5067 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5068 | `		return nOrigIdx;` |
|       - |  5069 | `	}` |
|  409737 |  5070 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  409737 |  5071 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5072 | `	/* Skip if already qualified (contains backslash) */` |
|  409737 |  5073 | `	hasNsSep = 0;` |
| 4432767 |  5074 | `	for( k = 0; k < nLit; k++ ){` |
| 4023043 |  5075 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2011520 |  5076 | `	}` |
|  409737 |  5077 | `	if( hasNsSep ){` |
|      11 |  5078 | `		return nOrigIdx;` |
|       - |  5079 | `	}` |
|       - |  5080 | `	/* Check use imports first (works even outside namespaces) */` |
|  409729 |  5081 | `	SyBlobReset(&pGen->sWorker);` |
|  409729 |  5082 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  409729 |  5083 | `	if( pImport ){` |
|      41 |  5084 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5085 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5086 | `		if( pFromImport ){` |
|      18 |  5087 | `			*pFromImport = 1;` |
|       8 |  5088 | `		}` |
|      23 |  5089 | `	}else{` |
|  409693 |  5090 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  409603 |  5091 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5092 | `		}` |
|       - |  5093 | `		/* Prepend current namespace */` |
|      95 |  5094 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5095 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5096 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5097 | `	}` |
|       - |  5098 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5099 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5100 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5101 | `		return nNewIdx; /* Already interned */` |
|       - |  5102 | `	}` |
|      79 |  5103 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5104 | `	if( pNew == 0 ){` |
|     ! 0 |  5105 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5106 | `	}` |
|      79 |  5107 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5108 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5109 | `	return nNewIdx;` |
|  204871 |  5110 |  |
|       - |  5111 | `/*` |
|       - |  5112 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5113 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5114 | ` */` |
|   90008 |  5115 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5116 |  |
|       - |  5117 | `	SyHashEntry *pImport;` |
|       - |  5118 | `	/* Check use imports first */` |
|   90013 |  5119 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   90013 |  5120 | `	if( pImport ){` |
|      15 |  5121 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 |  5122 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      15 |  5123 | `		return;` |
|       - |  5124 | `	}` |
|       - |  5125 | `	/* Prepend current namespace if active */` |
|   90001 |  5126 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5127 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5128 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5129 | `	}` |
|   90001 |  5130 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   45009 |  5131 |  |
|       - |  5132 | `/*` |
|       - |  5133 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5134 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5135 | ` * The caller must release pOut when done.` |
|       - |  5136 | ` */` |
|  126824 |  5137 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5138 |  |
|  126829 |  5139 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5140 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5141 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5142 | `	}` |
|  126829 |  5143 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  126829 |  5144 |  |
|       - |  5145 | `/*` |
|       - |  5146 | ` * Compile a namespace statement` |
|       - |  5147 | ` * According to the PHP language reference manual` |
|       - |  5148 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5149 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5150 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5151 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5152 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5153 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5154 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5155 | ` *  programming world.` |
|       - |  5156 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5157 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5158 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5159 | ` *  classes/functions/constants.` |
|       - |  5160 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5161 | ` *  readability of source code.` |
|       - |  5162 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5163 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5164 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5165 | ` *       class MyClass {}` |
|       - |  5166 | ` *       function myfunction() {}` |
|       - |  5167 | ` *       const MYCONST = 1;` |
|       - |  5168 | ` *       $a = new MyClass;` |
|       - |  5169 | ` *       $c = new \my\name\MyClass;` |
|       - |  5170 | ` *       $a = strlen('hi');` |
|       - |  5171 | ` *       $d = namespace\MYCONST;` |
|       - |  5172 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5173 | ` *       echo constant($d);` |
|       - |  5174 | ` * NOTE` |
|       - |  5175 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5176 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5177 | ` */` |
|       - |  5178 | `/*` |
|       - |  5179 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5180 | ` */` |
|      14 |  5181 | `static const char * TokenTypeName(sxu32 nType)` |
|       4 |  5182 |  |
|      18 |  5183 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 |  5184 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 |  5185 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 |  5186 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 |  5187 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 |  5188 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5189 | `	return "token";` |
|      11 |  5190 |  |
|     106 |  5191 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5192 |  |
|       - |  5193 | `	sxu32 nLine;` |
|       - |  5194 | `	sxi32 rc;` |
|     111 |  5195 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5196 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5197 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5198 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5199 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5200 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5201 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5202 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5203 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5204 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5205 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5206 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5207 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5208 | `		return SXRET_OK;` |
|       - |  5209 | `	}` |
|     111 |  5210 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5211 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5212 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5213 | `		return SXRET_OK;` |
|       - |  5214 | `	}` |
|     111 |  5215 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5216 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5217 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5218 | `		return SXRET_OK;` |
|       - |  5219 | `	}` |
|       - |  5220 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5221 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5222 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5223 | `			/* Append backslash separator */` |
|      27 |  5224 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5225 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5226 | `			}` |
|      16 |  5227 | `		}else{` |
|       - |  5228 | `			/* Append identifier */` |
|     131 |  5229 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5230 | `		}` |
|     153 |  5231 | `		pGen->pIn++;` |
|       5 |  5232 | `	}` |
|       - |  5233 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5234 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5235 | `	{` |
|     111 |  5236 | `		char *zNsDup = 0;` |
|     111 |  5237 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5238 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5239 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5240 | `		}` |
|     111 |  5241 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5242 | `	}` |
|     111 |  5243 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5244 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5245 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5246 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5247 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5248 | `			return SXERR_ABORT;` |
|       - |  5249 | `		}` |
|       2 |  5250 | `	}` |
|     111 |  5251 | `	return SXRET_OK;` |
|      58 |  5252 |  |
|       - |  5253 | `/*` |
|       - |  5254 | ` * Compile the 'use' statement` |
|       - |  5255 | ` * According to the PHP language reference manual` |
|       - |  5256 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5257 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5258 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5259 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5260 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5261 | ` *  a function or constant is not supported.` |
|       - |  5262 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5263 | ` * NOTE` |
|       - |  5264 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5265 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5266 | ` */` |
|      68 |  5267 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5268 |  |
|       - |  5269 | `	sxu32 nLine;` |
|       - |  5270 | `	sxi32 rc;` |
|       - |  5271 | `	SyBlob sPath;` |
|       - |  5272 | `	SyString sAlias;` |
|       - |  5273 | `	SyToken *pLast;` |
|       - |  5274 | `	char *zDup;` |
|       - |  5275 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5276 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5277 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5278 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5279 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5280 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5281 | `	iUseType = 0;` |
|      73 |  5282 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5283 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5284 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5285 | `			iUseType = 1;` |
|      16 |  5286 | `			pGen->pIn++;` |
|      23 |  5287 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5288 | `			iUseType = 2;` |
|      16 |  5289 | `			pGen->pIn++;` |
|       7 |  5290 | `		}` |
|      14 |  5291 | `	}` |
|       - |  5292 | `	/* Select target hash tables based on import type */` |
|      73 |  5293 | `	switch( iUseType ){` |
|       7 |  5294 | `		case 1:` |
|      16 |  5295 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5296 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5297 | `			break;` |
|       7 |  5298 | `		case 2:` |
|      16 |  5299 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5300 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5301 | `			break;` |
|      20 |  5302 | `		default:` |
|      45 |  5303 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5304 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5305 | `			break;` |
|       - |  5306 | `	}` |
|      73 |  5307 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5308 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5309 | `	for(;;){` |
|      75 |  5310 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5311 | `			break;` |
|       - |  5312 | `		}` |
|      75 |  5313 | `		SyBlobReset(&sPath);` |
|      75 |  5314 | `		pLast = 0;` |
|       - |  5315 | `		/* Collect the full namespace path */` |
|     261 |  5316 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5317 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5318 | `				pLast = pGen->pIn;` |
|     131 |  5319 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5320 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5321 | `				}` |
|     131 |  5322 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5323 | `			}` |
|     191 |  5324 | `			pGen->pIn++;` |
|       5 |  5325 | `		}` |
|      75 |  5326 | `		if( pLast == 0 ){` |
|       - |  5327 | `			/* Empty path */` |
|       5 |  5328 | `			break;` |
|       - |  5329 | `		}` |
|       - |  5330 | `		/* Default alias is the last component of the path */` |
|      71 |  5331 | `		sAlias = pLast->sData;` |
|       - |  5332 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5333 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5334 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      19 |  5335 | `			pGen->pIn++; /* Jump 'as' */` |
|      19 |  5336 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      19 |  5337 | `				sAlias = pGen->pIn->sData;` |
|      19 |  5338 | `				pGen->pIn++;` |
|       8 |  5339 | `			}` |
|       8 |  5340 | `		}` |
|       - |  5341 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5342 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5343 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5344 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5345 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5346 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5347 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5348 | `				return SXERR_ABORT;` |
|       - |  5349 | `			}` |
|       2 |  5350 | `		}` |
|       - |  5351 | `		/* Register the import: alias -> FQN.` |
|       - |  5352 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5353 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5354 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5355 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5356 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5357 | `		if( zDup ){` |
|      71 |  5358 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5359 | `			if( pVmHash ){` |
|       - |  5360 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5361 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5362 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5363 | `				if( zAliasDup ){` |
|      43 |  5364 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5365 | `				}` |
|      19 |  5366 | `			}` |
|      71 |  5367 | `			if( iUseType == 2 ){` |
|       - |  5368 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5369 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5370 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5371 | `				if( zAliasDup ){` |
|       - |  5372 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5373 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5374 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5375 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5376 | `					if( azPair ){` |
|      16 |  5377 | `						azPair[0] = zAliasDup;` |
|      16 |  5378 | `						azPair[1] = zDup;` |
|      16 |  5379 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5380 | `					}` |
|       7 |  5381 | `				}` |
|       7 |  5382 | `			}` |
|      33 |  5383 | `		}` |
|       - |  5384 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5385 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5386 | `			pGen->pIn++;` |
|       2 |  5387 | `		}else{` |
|      37 |  5388 | `			break;` |
|       - |  5389 | `		}` |
|       1 |  5390 | `	}` |
|      73 |  5391 | `	SyBlobRelease(&sPath);` |
|      73 |  5392 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5393 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5394 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5395 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5396 | `			return SXERR_ABORT;` |
|       - |  5397 | `		}` |
|       1 |  5398 | `	}` |
|      73 |  5399 | `	return SXRET_OK;` |
|      39 |  5400 |  |
|       - |  5401 | `/*` |
|       - |  5402 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5403 | ` *` |
|       - |  5404 | ` * According to the PHP language reference manual.` |
|       - |  5405 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5406 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5407 | ` *  declare (directive)` |
|       - |  5408 | ` *   statement` |
|       - |  5409 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5410 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5411 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5412 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5413 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5414 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5415 | ` * <?php` |
|       - |  5416 | ` * // these are the same:` |
|       - |  5417 | ` * // you can use this:` |
|       - |  5418 | ` * declare(ticks=1) {` |
|       - |  5419 | ` *   // entire script here` |
|       - |  5420 | ` * }` |
|       - |  5421 | ` * // or you can use this:` |
|       - |  5422 | ` * declare(ticks=1);` |
|       - |  5423 | ` * // entire script here` |
|       - |  5424 | ` * ?>` |
|       - |  5425 | ` *` |
|       - |  5426 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5427 | ` */` |
|       - |  5428 | `/*` |
|       - |  5429 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5430 | ` */` |
|      68 |  5431 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5432 |  |
|     103 |  5433 | `	return SyStringLength(pName) == nWant` |
|      68 |  5434 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5435 |  |
|       - |  5436 |  |
|      40 |  5437 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5438 |  |
|      45 |  5439 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5440 | `	SyToken *pBodyEnd = 0;` |
|       - |  5441 | `	SyToken *pBodyStart;` |
|       - |  5442 | `	SyToken *pCursor;` |
|       - |  5443 | `	int bHasStrictTypes;` |
|       - |  5444 | `	int bBlockForm;` |
|       - |  5445 | `	int bPlacementOk;` |
|       - |  5446 | `	sxi32 rc;` |
|      45 |  5447 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5448 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5449 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5450 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5451 | `			return SXERR_ABORT;` |
|       - |  5452 | `		}` |
|       6 |  5453 | `		goto Synchro;` |
|       - |  5454 | `	}` |
|      41 |  5455 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5456 | `	pBodyStart = pGen->pIn;` |
|       - |  5457 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5458 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5459 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5460 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5461 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5462 | `			return SXERR_ABORT;` |
|       - |  5463 | `		}` |
|     ! 0 |  5464 | `		return SXRET_OK;` |
|       - |  5465 | `	}` |
|       - |  5466 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5467 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5468 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5469 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5470 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5471 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5472 | `			return SXERR_ABORT;` |
|       - |  5473 | `		}` |
|     ! 0 |  5474 | `	}` |
|      41 |  5475 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5476 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5477 | `	bHasStrictTypes = 0;` |
|       - |  5478 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5479 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5480 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5481 | `	pCursor = pBodyStart;` |
|      53 |  5482 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5483 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5484 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5485 | `				bHasStrictTypes = 1;` |
|      37 |  5486 | `				break;` |
|       - |  5487 | `			}` |
|       2 |  5488 | `		}` |
|      14 |  5489 | `		pCursor++;` |
|       2 |  5490 | `	}` |
|      41 |  5491 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5492 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5493 | `			"strict_types declaration must not use block mode");` |
|       3 |  5494 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5495 | `		return SXRET_OK;` |
|       - |  5496 | `	}` |
|      39 |  5497 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5498 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5499 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5500 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5501 | `		return SXRET_OK;` |
|       - |  5502 | `	}` |
|       - |  5503 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5504 | `	pCursor = pBodyStart;` |
|      65 |  5505 | `	while( pCursor < pBodyEnd ){` |
|       - |  5506 | `		SyToken *pNameTok;` |
|       - |  5507 | `		SyToken *pEqTok;` |
|       - |  5508 | `		SyToken *pValTok;` |
|       - |  5509 | `		SyString *pDirName;` |
|       - |  5510 | `		int bIsStrict;` |
|       - |  5511 | `		int iStrictValue;` |
|      37 |  5512 | `		pNameTok = pCursor;` |
|      37 |  5513 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5514 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5515 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5516 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5517 | `			return SXRET_OK;` |
|       - |  5518 | `		}` |
|      37 |  5519 | `		pEqTok = pNameTok + 1;` |
|      37 |  5520 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5521 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5522 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5523 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5524 | `			return SXRET_OK;` |
|       - |  5525 | `		}` |
|      37 |  5526 | `		pValTok = pEqTok + 1;` |
|      37 |  5527 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5528 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5529 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5530 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5531 | `			return SXRET_OK;` |
|       - |  5532 | `		}` |
|      37 |  5533 | `		pDirName = &pNameTok->sData;` |
|      37 |  5534 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5535 | `		if( bIsStrict ){` |
|       - |  5536 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5537 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5538 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5539 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5540 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5541 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5542 | `				return SXRET_OK;` |
|       - |  5543 | `			}` |
|      33 |  5544 | `			iStrictValue = -1;` |
|      33 |  5545 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5546 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5547 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5548 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5549 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5550 | `			}` |
|      33 |  5551 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5552 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5553 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5554 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5555 | `				return SXRET_OK;` |
|       - |  5556 | `			}` |
|      30 |  5557 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5558 | `		}else{` |
|       - |  5559 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5560 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5561 | `			 * behavior don't regress. */` |
|       8 |  5562 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5563 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5564 | `				ph7_lib_version()` |
|       - |  5565 | `				);` |
|       - |  5566 | `		}` |
|      35 |  5567 | `		pCursor = pValTok + 1;` |
|       - |  5568 | `		/* Consume separating comma (or end). */` |
|      35 |  5569 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5570 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5571 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5572 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5573 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5574 | `				return SXRET_OK;` |
|       - |  5575 | `			}` |
|       3 |  5576 | `			pCursor++;` |
|       1 |  5577 | `		}` |
|       5 |  5578 | `	}` |
|       - |  5579 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5580 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5581 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5582 | `	return SXRET_OK;` |
|       2 |  5583 | `Synchro:` |
|       - |  5584 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5585 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5586 | `		pGen->pIn++;` |
|       2 |  5587 | `	}` |
|       6 |  5588 | `	return SXRET_OK;` |
|      25 |  5589 |  |
|       - |  5590 | `/*` |
|       - |  5591 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5592 | ` * as follows:` |
|       - |  5593 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5594 | ` * {` |
|       - |  5595 | ` *   return "Making a cup of $type.\n";` |
|       - |  5596 | ` * }` |
|       - |  5597 | ` * Symisc eXtension.` |
|       - |  5598 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5599 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5600 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5601 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5602 | ` *      {` |
|       - |  5603 | ` *       var_dump($a);` |
|       - |  5604 | ` *      }` |
|       - |  5605 | ` *     //call test without args` |
|       - |  5606 | ` *      test();` |
|       - |  5607 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5608 | ` *      Example:` |
|       - |  5609 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5610 | ` * 3 -) Function overloading!!` |
|       - |  5611 | ` *      Example:` |
|       - |  5612 | ` *      function foo($a) {` |
|       - |  5613 | ` *   	  return $a.PHP_EOL;` |
|       - |  5614 | ` *	    }` |
|       - |  5615 | ` *	    function foo($a, $b) {` |
|       - |  5616 | ` *   	  return $a + $b;` |
|       - |  5617 | ` *	    }` |
|       - |  5618 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5619 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5620 | ` *      // Same arg` |
|       - |  5621 | ` *	   function foo(string $a)` |
|       - |  5622 | ` *	   {` |
|       - |  5623 | ` *	     echo "a is a string\n";` |
|       - |  5624 | ` *	     var_dump($a);` |
|       - |  5625 | ` *	   }` |
|       - |  5626 | ` *	  function foo(int $a)` |
|       - |  5627 | ` *	  {` |
|       - |  5628 | ` *	    echo "a is integer\n";` |
|       - |  5629 | ` *	    var_dump($a);` |
|       - |  5630 | ` *	  }` |
|       - |  5631 | ` *	  function foo(array $a)` |
|       - |  5632 | ` *	  {` |
|       - |  5633 | ` * 	    echo "a is an array\n";` |
|       - |  5634 | ` * 	    var_dump($a);` |
|       - |  5635 | ` *	  }` |
|       - |  5636 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5637 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5638 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5639 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5640 | ` * introduced by the PH7 engine.` |
|       - |  5641 | ` */` |
|   62848 |  5642 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5643 |  |
|       - |  5644 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5645 | `	SySet *pInstrContainer;` |
|       - |  5646 | `	sxi32 rc;` |
|       - |  5647 | `	/* Swap token stream */` |
|   62853 |  5648 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   62853 |  5649 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   62853 |  5650 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5651 | `	/* Compile the expression holding the argument value */` |
|   62853 |  5652 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5653 | `	/* Emit the done instruction */` |
|   62853 |  5654 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   62853 |  5655 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   62853 |  5656 | `	RE_SWAP_DELIMITER(pGen);` |
|   62853 |  5657 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5658 | `		return SXERR_ABORT;` |
|       - |  5659 | `	}` |
|   62853 |  5660 | `	return SXRET_OK;` |
|   31429 |  5661 |  |
|       - |  5662 | `/*` |
|       - |  5663 | ` * Collect function arguments one after one.` |
|       - |  5664 | ` * According to the PHP language reference manual.` |
|       - |  5665 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5666 | ` * list of expressions.` |
|       - |  5667 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5668 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5669 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5670 | ` * for more information.` |
|       - |  5671 | ` * Example #1 Passing arrays to functions` |
|       - |  5672 | ` * <?php` |
|       - |  5673 | ` * function takes_array($input)` |
|       - |  5674 | ` * {` |
|       - |  5675 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5676 | ` * }` |
|       - |  5677 | ` * ?>` |
|       - |  5678 | ` * Making arguments be passed by reference` |
|       - |  5679 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5680 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5681 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5682 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5683 | ` * to the argument name in the function definition:` |
|       - |  5684 | ` * Example #2 Passing function parameters by reference` |
|       - |  5685 | ` * <?php` |
|       - |  5686 | ` * function add_some_extra(&$string)` |
|       - |  5687 | ` * {` |
|       - |  5688 | ` *   $string .= 'and something extra.';` |
|       - |  5689 | ` * }` |
|       - |  5690 | ` * $str = 'This is a string, ';` |
|       - |  5691 | ` * add_some_extra($str);` |
|       - |  5692 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5693 | ` * ?>` |
|       - |  5694 | ` *` |
|       - |  5695 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5696 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5697 | ` * on these extension.` |
|       - |  5698 | ` */` |
|   87168 |  5699 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5700 |  |
|       - |  5701 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5702 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5703 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5704 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5705 | `	sxi32 rc;` |
|       - |  5706 |  |
|   87173 |  5707 | `	pIn = pGen->pIn;` |
|   87173 |  5708 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5709 | `	/* Process arguments one after one */` |
|  109015 |  5710 | `	for(;;){` |
|  218035 |  5711 | `		if( pIn >= pEnd ){` |
|       - |  5712 | `			/* No more arguments to process */` |
|   87161 |  5713 | `			break;` |
|       - |  5714 | `		}` |
|  130879 |  5715 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  130879 |  5716 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  130879 |  5717 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  130879 |  5718 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5719 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5720 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5721 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5722 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5723 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5724 | `		{` |
|  130879 |  5725 | `			int bReadonly = 0, bVisSeen = 0;` |
|  130879 |  5726 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  130879 |  5727 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5728 | `				bReadonly = 1;` |
|       3 |  5729 | `				pIn++;` |
|       1 |  5730 | `			}` |
|  130879 |  5731 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   59703 |  5732 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   59703 |  5733 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      67 |  5734 | `					bVisSeen = 1;` |
|      67 |  5735 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      89 |  5736 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      29 |  5737 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      67 |  5738 | `					pIn++;` |
|      67 |  5739 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5740 | `						bReadonly = 1;` |
|      16 |  5741 | `						pIn++;` |
|       6 |  5742 | `					}` |
|      31 |  5743 | `				}` |
|   29849 |  5744 | `			}` |
|  130879 |  5745 | `			if( bVisSeen \|\| bReadonly ){` |
|      69 |  5746 | `				if( !bCtorCtx ){` |
|       6 |  5747 | `					if( bAbstractCtx ){` |
|       3 |  5748 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5749 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5750 | `					}else{` |
|       3 |  5751 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5752 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5753 | `					}` |
|       6 |  5754 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5755 | `						return SXERR_ABORT;` |
|       - |  5756 | `					}` |
|       6 |  5757 | `					return SXERR_SYNTAX;` |
|       - |  5758 | `				}` |
|      65 |  5759 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      65 |  5760 | `				sArg.iPromoteVis = iVis;` |
|      65 |  5761 | `				if( bReadonly ){` |
|      18 |  5762 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5763 | `				}` |
|      30 |  5764 | `			}` |
|       - |  5765 | `		}` |
|       - |  5766 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  130870 |  5767 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  101941 |  5768 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   71351 |  5769 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   69663 |  5770 | `			sxu32 nLineLocal = pIn->nLine;` |
|   69663 |  5771 | `			sxi32 iTFlags = 0;` |
|   69663 |  5772 | `			pGen->pIn = pIn;` |
|   69663 |  5773 | `			rc = GenStateParseUnionTypeDecl(` |
|   34829 |  5774 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   34829 |  5775 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5776 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5777 | `				/* bAllowVoid */ 0,` |
|   34829 |  5778 | `						nLineLocal);` |
|   69663 |  5779 | `			pIn = pGen->pIn;` |
|   69663 |  5780 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5781 | `				return SXERR_ABORT;` |
|   69663 |  5782 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5783 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5784 | `				return SXERR_SYNTAX;` |
|   69661 |  5785 | `			}else if( rc == SXERR_SYNTAX ){` |
|       6 |  5786 | `				if( pIn < pEnd ){` |
|       8 |  5787 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5788 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5789 | `						&pIn->sData);` |
|       4 |  5790 | `				}else{` |
|     ! 0 |  5791 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5792 | `						"syntax error, unexpected end of file");` |
|       - |  5793 | `				}` |
|       6 |  5794 | `				return SXERR_SYNTAX;` |
|       - |  5795 | `			}` |
|   69657 |  5796 | `			sArg.iFlags \|= iTFlags;` |
|   34826 |  5797 | `		}` |
|  130869 |  5798 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5799 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5800 | `			return rc;` |
|       - |  5801 | `		}` |
|  130869 |  5802 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5803 | `			/* Pass by reference,record that */` |
|    3339 |  5804 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3339 |  5805 | `			pIn++;` |
|    1667 |  5806 | `		}` |
|  130869 |  5807 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5808 | `			/* Variadic parameter: ...$args */` |
|      47 |  5809 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      47 |  5810 | `			pIn++;` |
|      21 |  5811 | `		}` |
|  130869 |  5812 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5813 | `			/* Invalid argument */` |
|     ! 0 |  5814 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5815 | `			return rc;` |
|       - |  5816 | `		}` |
|  130869 |  5817 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5818 | `		/* Copy argument name */` |
|  130869 |  5819 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  130869 |  5820 | `		if( zDup == 0 ){` |
|     ! 0 |  5821 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5822 | `			return SXERR_ABORT;` |
|       - |  5823 | `		}` |
|  130869 |  5824 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  130869 |  5825 | `		pIn++;` |
|  130869 |  5826 | `		if( pIn < pEnd ){` |
|   73499 |  5827 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5828 | `				SyToken *pDefend;` |
|   62855 |  5829 | `				sxi32 iNest = 0;` |
|   62855 |  5830 | `				pIn++; /* Jump the equal sign */` |
|   62855 |  5831 | `				pDefend = pIn;` |
|       - |  5832 | `				/* Process the default value associated with this argument */` |
|  132315 |  5833 | `				while( pDefend < pEnd ){` |
|  102533 |  5834 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   33073 |  5835 | `						break;` |
|       - |  5836 | `					}` |
|   69465 |  5837 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5838 | `						/* Increment nesting level */` |
|    3311 |  5839 | `						iNest++;` |
|   67812 |  5840 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5841 | `						/* Decrement nesting level */` |
|    3311 |  5842 | `						iNest--;` |
|    1653 |  5843 | `					}` |
|   69465 |  5844 | `					pDefend++;` |
|       5 |  5845 | `				}` |
|   62855 |  5846 | `				if( pIn >= pDefend ){` |
|       3 |  5847 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5848 | `					return rc;` |
|       - |  5849 | `				}` |
|       - |  5850 | `				/* Process default value */` |
|   62853 |  5851 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   62853 |  5852 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5853 | `					return rc;` |
|       - |  5854 | `				}` |
|       - |  5855 | `				/* Point beyond the default value */` |
|   62853 |  5856 | `				pIn = pDefend;` |
|   31424 |  5857 | `			}` |
|   73497 |  5858 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5859 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5860 | `				return rc;` |
|       - |  5861 | `			}` |
|   73497 |  5862 | `			pIn++; /* Jump the trailing comma */` |
|   36746 |  5863 | `		}` |
|       - |  5864 | `		/* Append argument signature */` |
|  130867 |  5865 | `		if( sArg.nType > 0 ){` |
|   69613 |  5866 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5867 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    9945 |  5868 | `				int marker = 'o';` |
|    9945 |  5869 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    9945 |  5870 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    4975 |  5871 | `			}else{` |
|       - |  5872 | `				int c;` |
|   59673 |  5873 | `				c = 'n'; /* cc warning */` |
|       - |  5874 | `				/* Type leading character */` |
|   59673 |  5875 | `				switch(sArg.nType){` |
|     ! 0 |  5876 | `				case MEMOBJ_HASHMAP:` |
|       - |  5877 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5878 | `					c = 'h';` |
|     ! 0 |  5879 | `					break;` |
|    8311 |  5880 | `				case MEMOBJ_INT:` |
|       - |  5881 | `					/* Integer */` |
|   16627 |  5882 | `					c = 'i';` |
|   16627 |  5883 | `					break;` |
|       1 |  5884 | `				case MEMOBJ_BOOL:` |
|       - |  5885 | `					/* Bool */` |
|       3 |  5886 | `					c = 'b';` |
|       3 |  5887 | `					break;` |
|       2 |  5888 | `				case MEMOBJ_REAL:` |
|       - |  5889 | `					/* Float */` |
|       5 |  5890 | `					c = 'f';` |
|       5 |  5891 | `					break;` |
|   21512 |  5892 | `				case MEMOBJ_STRING:` |
|       - |  5893 | `					/* String */` |
|   43029 |  5894 | `					c = 's';` |
|   43029 |  5895 | `					break;` |
|       7 |  5896 | `				case MEMOBJ_OBJ:` |
|       - |  5897 | `					/* Object */` |
|      16 |  5898 | `					c = 'o';` |
|      14 |  5899 | `					break;` |
|       1 |  5900 | `				default:` |
|       2 |  5901 | `					break;` |
|       - |  5902 | `				}` |
|   59673 |  5903 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5904 | `			}` |
|   34809 |  5905 | `		}else{` |
|       - |  5906 | `			/* No type is associated with this parameter which mean` |
|       - |  5907 | `			 * that this function is not condidate for overloading.` |
|       - |  5908 | `			 */` |
|   61259 |  5909 | `			SyBlobRelease(&sSig);` |
|       - |  5910 | `		}` |
|       - |  5911 | `		/* Save in the argument set */` |
|  130867 |  5912 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5913 | `	}` |
|   87161 |  5914 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5915 | `		/* Save function signature */` |
|   43133 |  5916 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   21564 |  5917 | `	}` |
|   87161 |  5918 | `	return SXRET_OK;` |
|   43589 |  5919 |  |
|       - |  5920 | `/*` |
|       - |  5921 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5922 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5923 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5924 | ` */` |
|  206952 |  5925 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5926 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5927 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5928 | `	)` |
|       5 |  5929 |  |
|       - |  5930 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5931 | `	GenBlock *pBlock;` |
|       - |  5932 | `	sxu32 nGotoOfft;` |
|       - |  5933 | `	sxi32 rc;` |
|       - |  5934 | `	/* Attach the new function */` |
|  206957 |  5935 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  206957 |  5936 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5937 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5938 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5939 | `		return SXERR_ABORT;` |
|       - |  5940 | `	}` |
|  206957 |  5941 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5942 | `	/* Swap bytecode containers */` |
|  206957 |  5943 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  206957 |  5944 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5945 | `	/* Emit constructor property promotion prologue:` |
|       - |  5946 | `	 *   $this->NAME = $NAME;` |
|       - |  5947 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5948 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5949 | `	{` |
|  206957 |  5950 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5951 | `		sxu32 i;` |
|  311247 |  5952 | `		for( i = 0; i < nArg; i++ ){` |
|  104295 |  5953 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5954 | `			char *zSrc;` |
|       - |  5955 | `			sxu32 nSrc,nName;` |
|       - |  5956 | `			SySet sToken;` |
|       - |  5957 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5958 | `			sxi32 rcPromote;` |
|  104295 |  5959 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  104245 |  5960 | `				continue;` |
|       - |  5961 | `			}` |
|       - |  5962 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5963 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5964 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5965 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5966 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      55 |  5967 | `			nName = SyStringLength(&pArg->sName);` |
|      55 |  5968 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      55 |  5969 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      55 |  5970 | `			if( zSrc == 0 ){` |
|     ! 0 |  5971 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5972 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5973 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5974 | `				return SXERR_ABORT;` |
|       - |  5975 | `			}` |
|       - |  5976 | `			{` |
|      55 |  5977 | `				char *z = zSrc;` |
|      55 |  5978 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      55 |  5979 | `				z += sizeof("$this->")-1;` |
|      55 |  5980 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      55 |  5981 | `				z += nName;` |
|      55 |  5982 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      55 |  5983 | `				z += sizeof(" = $")-1;` |
|      55 |  5984 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      55 |  5985 | `				z += nName;` |
|      55 |  5986 | `				*z = 0;` |
|       - |  5987 | `			}` |
|      55 |  5988 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      55 |  5989 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      55 |  5990 | `			pTmpIn = pGen->pIn;` |
|      55 |  5991 | `			pTmpEnd = pGen->pEnd;` |
|      55 |  5992 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      55 |  5993 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      55 |  5994 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      55 |  5995 | `			pGen->pIn = pTmpIn;` |
|      55 |  5996 | `			pGen->pEnd = pTmpEnd;` |
|      55 |  5997 | `			SySetRelease(&sToken);` |
|      55 |  5998 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  5999 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6000 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6001 | `				return SXERR_ABORT;` |
|       - |  6002 | `			}` |
|       - |  6003 | `			/* Discard the assignment result — this is a statement expression. */` |
|      55 |  6004 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      30 |  6005 | `		}` |
|       - |  6006 | `	}` |
|       - |  6007 | `	/* Compile the body */` |
|  206957 |  6008 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  6009 | `	/* Fix exception jumps now the destination is resolved */` |
|  206957 |  6010 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6011 | `	/* Emit the final return if not yet done */` |
|  206957 |  6012 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6013 | `	/* Fix gotos jumps now the destination is resolved */` |
|  206957 |  6014 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6015 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6016 | `	}` |
|  206957 |  6017 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6018 | `	/* Restore the default container */` |
|  206957 |  6019 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6020 | `	/* Leave function block */` |
|  206957 |  6021 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  206957 |  6022 | `	if( rc == SXERR_ABORT ){` |
|       - |  6023 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6024 | `		return SXERR_ABORT;` |
|       - |  6025 | `	}` |
|       - |  6026 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6027 | `	{` |
|  206957 |  6028 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6029 | `		sxu32 i;` |
| 4042453 |  6030 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 3835537 |  6031 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      41 |  6032 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      41 |  6033 | `				break;` |
|       - |  6034 | `			}` |
| 1917753 |  6035 | `		}` |
|       - |  6036 | `	}` |
|       - |  6037 | `	/* All done, function body compiled */` |
|  206957 |  6038 | `	return SXRET_OK;` |
|  103481 |  6039 |  |
|       - |  6040 | `/*` |
|       - |  6041 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6042 | ` * According to the PHP language reference manual.` |
|       - |  6043 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6044 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6045 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6046 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6047 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6048 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6049 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6050 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6051 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6052 | ` *` |
|       - |  6053 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6054 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6055 | ` * on these extension.` |
|       - |  6056 | ` */` |
|       - |  6057 | `/*` |
|       - |  6058 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6059 | ` */` |
|     334 |  6060 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6061 |  |
|       - |  6062 | `	sxu32 i;` |
|     947 |  6063 | `	for( i = 0; i < n; i++ ){` |
|     811 |  6064 | `		int a = zA[i], b = zB[i];` |
|     811 |  6065 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     811 |  6066 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     811 |  6067 | `		if( a != b ) return a - b;` |
|     309 |  6068 | `	}` |
|     141 |  6069 | `	return 0;` |
|     172 |  6070 |  |
|       - |  6071 | `/*` |
|       - |  6072 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6073 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6074 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6075 | ` */` |
|       - |  6076 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6077 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6078 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6079 |  |
|       - |  6080 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  6081 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  6082 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  6083 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  6084 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  6085 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  6086 |  |
|       - |  6087 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6088 | `struct PhlTypeAtom {` |
|       - |  6089 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6090 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6091 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6092 | `	sxu32 nCanon;` |
|       - |  6093 | `};` |
|       - |  6094 |  |
|       - |  6095 | `/*` |
|       - |  6096 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6097 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6098 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6099 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6100 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6101 | ` * already be consumed by the caller.` |
|       - |  6102 | ` */` |
|   70302 |  6103 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6104 |  |
|   70307 |  6105 | `	SyToken *pIn = pGen->pIn;` |
|   70307 |  6106 | `	SyZero(pOut, sizeof(*pOut));` |
|   70307 |  6107 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   70307 |  6108 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6109 | `		return SXERR_SYNTAX;` |
|       - |  6110 | `	}` |
|       - |  6111 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   70307 |  6112 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6113 | `		pIn++;` |
|       8 |  6114 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6115 | `			return SXERR_SYNTAX;` |
|       - |  6116 | `		}` |
|       3 |  6117 | `	}` |
|   70307 |  6118 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6119 | `		return SXERR_SYNTAX;` |
|       - |  6120 | `	}` |
|   70307 |  6121 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   60141 |  6122 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   60141 |  6123 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      24 |  6124 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   60131 |  6125 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      61 |  6126 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   60093 |  6127 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   16849 |  6128 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   51643 |  6129 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   43163 |  6130 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   21642 |  6131 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      32 |  6132 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      48 |  6133 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      28 |  6134 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      22 |  6135 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       4 |  6136 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  6137 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  6138 | `			pOut->sClass = pIn->sData;` |
|       4 |  6139 | `		}else{` |
|       3 |  6140 | `			return SXERR_SYNTAX;` |
|       - |  6141 | `		}` |
|   60139 |  6142 | `		pIn++;` |
|   30072 |  6143 | `	}else{` |
|       - |  6144 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6145 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   10171 |  6146 | `		SyString *pT = &pIn->sData;` |
|   10171 |  6147 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      18 |  6148 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      18 |  6149 | `			pIn++;` |
|   10163 |  6150 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     117 |  6151 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     117 |  6152 | `			pIn++;` |
|   10099 |  6153 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  6154 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  6155 | `			pIn++;` |
|       2 |  6156 | `		}else{` |
|       - |  6157 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   10041 |  6158 | `			SyToken *pFirst = pIn;` |
|   10041 |  6159 | `			SyToken *pLast = pIn;` |
|   10041 |  6160 | `			pOut->nType = SXU32_HIGH;` |
|   10041 |  6161 | `			pOut->sClass = pIn->sData;` |
|   10041 |  6162 | `			pIn++;` |
|   15057 |  6163 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   10044 |  6164 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6165 | `				pLast = &pIn[1];` |
|       3 |  6166 | `				pIn += 2;` |
|       1 |  6167 | `			}` |
|   10041 |  6168 | `			if( pLast != pFirst ){` |
|       3 |  6169 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6170 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6171 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6172 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6173 | `			}` |
|       - |  6174 | `		}` |
|       - |  6175 | `	}` |
|   70305 |  6176 | `	pGen->pIn = pIn;` |
|   70305 |  6177 | `	return SXRET_OK;` |
|   35156 |  6178 |  |
|       - |  6179 |  |
|       - |  6180 | `/*` |
|       - |  6181 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6182 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6183 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6184 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6185 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6186 | ` */` |
|   70198 |  6187 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6188 |  |
|       - |  6189 | `	int i;` |
|   70203 |  6190 | `	int nNonNull = 0;` |
|  140489 |  6191 | `	for( i = 0; i < nAtoms; i++ ){` |
|   70291 |  6192 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   70275 |  6193 | `			nNonNull++;` |
|   35135 |  6194 | `		}` |
|   35148 |  6195 | `	}` |
|   70203 |  6196 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6197 | `		/* Shorthand: ?T */` |
|      65 |  6198 | `		for( i = 0; i < nAtoms; i++ ){` |
|      65 |  6199 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      65 |  6200 | `			SyBlobAppend(pBlob, "?", 1);` |
|      65 |  6201 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      15 |  6202 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       9 |  6203 | `			}else{` |
|      53 |  6204 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6205 | `			}` |
|      65 |  6206 | `			return;` |
|     ! 0 |  6207 | `		}` |
|     ! 0 |  6208 | `	}` |
|       - |  6209 | `	{` |
|   70141 |  6210 | `		int bFirst = 1;` |
|       - |  6211 | `		/* 1) Classes in declaration order */` |
|  140359 |  6212 | `		for( i = 0; i < nAtoms; i++ ){` |
|   70223 |  6213 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   10033 |  6214 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   10033 |  6215 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   10033 |  6216 | `				bFirst = 0;` |
|    5014 |  6217 | `			}` |
|   35114 |  6218 | `		}` |
|       - |  6219 | `		/* 2) Built-ins in canonical order */` |
|       - |  6220 | `		{` |
|       - |  6221 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6222 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6223 | `			int k;` |
|  490957 |  6224 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  781963 |  6225 | `				for( i = 0; i < nAtoms; i++ ){` |
|  421217 |  6226 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   60075 |  6227 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   60075 |  6228 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   60075 |  6229 | `						bFirst = 0;` |
|   60075 |  6230 | `						break;` |
|       - |  6231 | `					}` |
|  180576 |  6232 | `				}` |
|  210413 |  6233 | `			}` |
|       - |  6234 | `		}` |
|       - |  6235 | `		/* 3) null suffix */` |
|   70141 |  6236 | `		if( bNullable ){` |
|      12 |  6237 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      12 |  6238 | `			SyBlobAppend(pBlob, "null", 4);` |
|       5 |  6239 | `		}` |
|       - |  6240 | `	}` |
|   35104 |  6241 |  |
|       - |  6242 |  |
|       - |  6243 | `/*` |
|       - |  6244 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6245 | ` *` |
|       - |  6246 | ` * Outputs:` |
|       - |  6247 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6248 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6249 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6250 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6251 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6252 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6253 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6254 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6255 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6256 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6257 | ` *` |
|       - |  6258 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6259 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6260 | ` */` |
|   70208 |  6261 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6262 | `	ph7_gen_state *pGen,` |
|       - |  6263 | `	sxu32 *pnType,` |
|       - |  6264 | `	SyString *pClass,` |
|       - |  6265 | `	SySet *pAlts,` |
|       - |  6266 | `	sxi32 *piTypeFlags,` |
|       - |  6267 | `	SyString *pTypeText,` |
|       - |  6268 | `	int iNullableFlag,` |
|       - |  6269 | `	int iUnionFlag,` |
|       - |  6270 | `	int bAllowVoid,` |
|       - |  6271 | `	sxu32 nLine` |
|       5 |  6272 | `){` |
|       - |  6273 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   70213 |  6274 | `	int nAtoms = 0;` |
|   70213 |  6275 | `	int bShortNullable = 0;` |
|   70213 |  6276 | `	int bExplicitNull = 0;` |
|       - |  6277 | `	sxi32 rc;` |
|   70213 |  6278 | `	*pnType = 0;` |
|   70213 |  6279 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   70213 |  6280 | `	*piTypeFlags = 0;` |
|   70213 |  6281 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6282 |  |
|   70213 |  6283 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6284 | `		return SXRET_OK;` |
|       - |  6285 | `	}` |
|       - |  6286 | ``	/* Optional `?` shorthand prefix */`` |
|   70208 |  6287 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      63 |  6288 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      61 |  6289 | `		bShortNullable = 1;` |
|      61 |  6290 | `		pGen->pIn++;` |
|      61 |  6291 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6292 | `			return SXERR_SYNTAX;` |
|       - |  6293 | `		}` |
|      29 |  6294 | `	}` |
|       - |  6295 | `	/* First atom is mandatory */` |
|   70213 |  6296 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   70213 |  6297 | `	if( rc != SXRET_OK ){` |
|       3 |  6298 | `		return rc;` |
|       - |  6299 | `	}` |
|   70211 |  6300 | `	nAtoms = 1;` |
|       - |  6301 | ``	/* Subsequent atoms separated by `\|` */`` |
|  105452 |  6302 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   70354 |  6303 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     101 |  6304 | `		if( bShortNullable ){` |
|       - |  6305 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6306 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6307 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  6308 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6309 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6310 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6311 | `		}` |
|      99 |  6312 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6313 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6314 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6315 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6316 | `		}` |
|      99 |  6317 | ``		pGen->pIn++; /* skip `\|` */`` |
|      99 |  6318 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      99 |  6319 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  6320 | `			return rc;` |
|       - |  6321 | `		}` |
|      99 |  6322 | `		nAtoms++;` |
|       5 |  6323 | `	}` |
|       - |  6324 | `	/* Validation pass.` |
|       - |  6325 | `	 *` |
|       - |  6326 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6327 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6328 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6329 | `	 */` |
|       - |  6330 | `	{` |
|       - |  6331 | `		int i, j;` |
|   70209 |  6332 | `		int bHasNonNull = 0;` |
|  140501 |  6333 | `		for( i = 0; i < nAtoms; i++ ){` |
|   70303 |  6334 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     117 |  6335 | `				if( nAtoms > 1 ){` |
|       3 |  6336 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6337 | `						"Void can only be used as a standalone type");` |
|       3 |  6338 | `					return SXERR_SYNTAX;` |
|       - |  6339 | `				}` |
|     115 |  6340 | `				if( !bAllowVoid ){` |
|     ! 0 |  6341 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6342 | `						"void cannot be used here");` |
|     ! 0 |  6343 | `					return SXERR_SYNTAX;` |
|       - |  6344 | `				}` |
|     115 |  6345 | `				if( bShortNullable ){` |
|     ! 0 |  6346 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6347 | `						"Void type cannot be nullable");` |
|     ! 0 |  6348 | `					return SXERR_SYNTAX;` |
|       - |  6349 | `				}` |
|      55 |  6350 | `			}` |
|   70301 |  6351 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6352 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6353 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6354 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6355 | `				 * does not return), and folding them would mislead any` |
|       - |  6356 | `				 * future return-enforcement work. */` |
|       3 |  6357 | `				if( nAtoms > 1 ){` |
|       3 |  6358 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6359 | `						"never can only be used as a standalone type");` |
|       3 |  6360 | `					return SXERR_SYNTAX;` |
|       - |  6361 | `				}` |
|     ! 0 |  6362 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6363 | `					"never type is not yet implemented");` |
|     ! 0 |  6364 | `				return SXERR_SYNTAX;` |
|       - |  6365 | `			}` |
|   70299 |  6366 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      18 |  6367 | `				bExplicitNull = 1;` |
|      10 |  6368 | `			}else{` |
|   70283 |  6369 | `				bHasNonNull = 1;` |
|       - |  6370 | `			}` |
|       - |  6371 | `			/* Duplicate detection */` |
|   70423 |  6372 | `			for( j = 0; j < i; j++ ){` |
|     131 |  6373 | `				int bDup = 0;` |
|     131 |  6374 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      18 |  6375 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6376 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      15 |  6377 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6378 | `								aAtoms[j].sClass.zString,` |
|      12 |  6379 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6380 | `							bDup = 1;` |
|     ! 0 |  6381 | `						}` |
|       9 |  6382 | `					}else{` |
|       3 |  6383 | `						bDup = 1;` |
|       - |  6384 | `					}` |
|       7 |  6385 | `				}` |
|     131 |  6386 | `				if( bDup ){` |
|       - |  6387 | `					const char *zName;` |
|       - |  6388 | `					sxu32 nName;` |
|       3 |  6389 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6390 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6391 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6392 | `					}else{` |
|       3 |  6393 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6394 | `						nName = aAtoms[i].nCanon;` |
|       - |  6395 | `					}` |
|       4 |  6396 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6397 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6398 | `					return SXERR_SYNTAX;` |
|       - |  6399 | `				}` |
|      67 |  6400 | `			}` |
|   35151 |  6401 | `		}` |
|   70203 |  6402 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6403 | `			if( bShortNullable ){` |
|       - |  6404 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6405 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6406 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6407 | `				return SXERR_SYNTAX;` |
|       - |  6408 | `			}` |
|       - |  6409 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6410 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6411 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6412 | `			 * atom, so set it here. */` |
|       7 |  6413 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6414 | `		}` |
|       - |  6415 | `	}` |
|       - |  6416 | `	/* Compute nullability flag */` |
|   70203 |  6417 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      76 |  6418 | `		*piTypeFlags \|= iNullableFlag;` |
|      36 |  6419 | `	}` |
|       - |  6420 | `	/* Build canonical type text */` |
|   70203 |  6421 | `	if( pTypeText ){` |
|       - |  6422 | `		SyBlob sBlob;` |
|   70203 |  6423 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  105274 |  6424 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   35099 |  6425 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   70203 |  6426 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  105137 |  6427 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   70088 |  6428 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   70093 |  6429 | `			if( zDup ){` |
|   70093 |  6430 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   35044 |  6431 | `			}` |
|   35044 |  6432 | `		}` |
|   70203 |  6433 | `		SyBlobRelease(&sBlob);` |
|   35099 |  6434 | `	}` |
|       - |  6435 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6436 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6437 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6438 | `	{` |
|   70203 |  6439 | `		int nNonNull = 0;` |
|   70203 |  6440 | `		int iNonNullIdx = -1;` |
|       - |  6441 | `		int i;` |
|  140489 |  6442 | `		for( i = 0; i < nAtoms; i++ ){` |
|   70291 |  6443 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   70275 |  6444 | `				nNonNull++;` |
|   70275 |  6445 | `				iNonNullIdx = i;` |
|   35135 |  6446 | `			}` |
|   35148 |  6447 | `		}` |
|   70203 |  6448 | `		if( nNonNull <= 1 ){` |
|       - |  6449 | `			/* Fast path: store as single type. */` |
|   70141 |  6450 | `			if( iNonNullIdx >= 0 ){` |
|   70135 |  6451 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   70135 |  6452 | `				if( pA->nType == SXU32_HIGH ){` |
|   15023 |  6453 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    5006 |  6454 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   10017 |  6455 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   10017 |  6456 | `					*pnType = SXU32_HIGH;` |
|   10017 |  6457 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   65129 |  6458 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     115 |  6459 | `					*pnType = MEMOBJ_VOID;` |
|      60 |  6460 | `				}else{` |
|       - |  6461 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6462 | `					 * pass above rejects it as not-yet-implemented. */` |
|   60013 |  6463 | `					*pnType = pA->nType;` |
|       - |  6464 | `				}` |
|   35065 |  6465 | `			}` |
|   35073 |  6466 | `		}else{` |
|       - |  6467 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      67 |  6468 | `			*piTypeFlags \|= iUnionFlag;` |
|     211 |  6469 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6470 | `				ph7_type_alt sAlt;` |
|     149 |  6471 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     145 |  6472 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     145 |  6473 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      45 |  6474 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      14 |  6475 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      31 |  6476 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      31 |  6477 | `					sAlt.nType = SXU32_HIGH;` |
|      31 |  6478 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      17 |  6479 | `				}else{` |
|     117 |  6480 | `					sAlt.nType = aAtoms[i].nType;` |
|     117 |  6481 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6482 | `				}` |
|     145 |  6483 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      75 |  6484 | `			}` |
|       - |  6485 | `		}` |
|       - |  6486 | `	}` |
|   70203 |  6487 | `	return SXRET_OK;` |
|   35109 |  6488 |  |
|       - |  6489 |  |
|       - |  6490 | `/*` |
|       - |  6491 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6492 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6493 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6494 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6495 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6496 | `` *          and union types `: T\|U`.`` |
|       - |  6497 | ` */` |
|  293090 |  6498 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6499 |  |
|  293095 |  6500 | `	sxi32 iFlags = 0;` |
|       - |  6501 | `	sxi32 rc;` |
|       - |  6502 | `	sxu32 nLine;` |
|  293095 |  6503 | `	pFunc->nReturnType = 0;` |
|  293095 |  6504 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  293095 |  6505 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  293095 |  6506 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  292743 |  6507 | `		return SXRET_OK;` |
|       - |  6508 | `	}` |
|     357 |  6509 | `	pGen->pIn++; /* Skip ':' */` |
|     357 |  6510 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6511 | `		return SXRET_OK;` |
|       - |  6512 | `	}` |
|     357 |  6513 | `	nLine = pGen->pIn->nLine;` |
|     357 |  6514 | `	rc = GenStateParseUnionTypeDecl(` |
|     176 |  6515 | `		pGen,` |
|     176 |  6516 | `		&pFunc->nReturnType,` |
|     176 |  6517 | `		&pFunc->sReturnClass,` |
|     176 |  6518 | `		&pFunc->aReturnUnion,` |
|       - |  6519 | `		&iFlags,` |
|     176 |  6520 | `		&pFunc->sReturnTypeName,` |
|       - |  6521 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6522 | `		/* iUnionFlag */ 0,` |
|       - |  6523 | `		/* bAllowVoid */ 1,` |
|     176 |  6524 | `		nLine);` |
|     176 |  6525 | `	(void)iFlags;` |
|     357 |  6526 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6527 | `		return SXERR_ABORT;` |
|       - |  6528 | `	}` |
|     357 |  6529 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6530 | `		/* Error already reported */` |
|     ! 0 |  6531 | `		return SXERR_SYNTAX;` |
|       - |  6532 | `	}` |
|     357 |  6533 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6534 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6535 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6536 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6537 | `				&pGen->pIn->sData);` |
|       3 |  6538 | `		}else{` |
|     ! 0 |  6539 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6540 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6541 | `		}` |
|       5 |  6542 | `		return SXERR_SYNTAX;` |
|       - |  6543 | `	}` |
|     353 |  6544 | `	return SXRET_OK;` |
|  146550 |  6545 |  |
|       - |  6546 |  |
|   44084 |  6547 | `static sxi32 GenStateCompileFunc(` |
|       - |  6548 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6549 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6550 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6551 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6552 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6553 | `	)` |
|       5 |  6554 |  |
|       - |  6555 | `	ph7_vm_func *pFunc;` |
|       - |  6556 | `	SyToken *pEnd;` |
|       - |  6557 | `	sxu32 nLine;` |
|       - |  6558 | `	char *zName;` |
|       - |  6559 | `	sxi32 rc;` |
|       - |  6560 | `	/* Extract line number */` |
|   44089 |  6561 | `	nLine = pGen->pIn->nLine;` |
|       - |  6562 | `	/* Jump the left parenthesis '(' */` |
|   44089 |  6563 | `	pGen->pIn++;` |
|       - |  6564 | `	/* Delimit the function signature */` |
|   44089 |  6565 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   44089 |  6566 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6567 | `		/* Syntax error */` |
|       9 |  6568 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       9 |  6569 | `		if( rc == SXERR_ABORT ){` |
|       - |  6570 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6571 | `			return SXERR_ABORT;` |
|       - |  6572 | `		}` |
|       9 |  6573 | `		pGen->pIn = pGen->pEnd;` |
|       9 |  6574 | `		return SXRET_OK;` |
|       - |  6575 | `	}` |
|       - |  6576 | `	/* Create the function state */` |
|   44083 |  6577 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   44083 |  6578 | `	if( pFunc == 0 ){` |
|     ! 0 |  6579 | `		goto OutOfMem;` |
|       - |  6580 | `	}` |
|       - |  6581 | `	/* Build the function name, prepending namespace if active */` |
|   44090 |  6582 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6583 | `		SyBlob sFQN;` |
|       - |  6584 | `		sxu32 nLen;` |
|      16 |  6585 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6586 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6587 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6588 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6589 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6590 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6591 | `		SyBlobRelease(&sFQN);` |
|      16 |  6592 | `		if( zName == 0 ){` |
|     ! 0 |  6593 | `			goto OutOfMem;` |
|       - |  6594 | `		}` |
|      16 |  6595 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6596 | `	}else{` |
|   44069 |  6597 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   44069 |  6598 | `		if( zName == 0 ){` |
|     ! 0 |  6599 | `			goto OutOfMem;` |
|       - |  6600 | `		}` |
|   44069 |  6601 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6602 | `	}` |
|   44083 |  6603 | `	if( pGen->pIn < pEnd ){` |
|       - |  6604 | `		/* Collect function arguments */` |
|   30521 |  6605 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   30521 |  6606 | `		if( rc == SXERR_ABORT ){` |
|       - |  6607 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6608 | `			return SXERR_ABORT;` |
|       - |  6609 | `		}` |
|   15258 |  6610 | `	}` |
|       - |  6611 | `	/* Point past ')' and parse optional return type ': type' */` |
|   44083 |  6612 | `	pGen->pIn = &pEnd[1];` |
|       - |  6613 | `	{` |
|   44083 |  6614 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   44083 |  6615 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6616 | `			return SXERR_ABORT;` |
|   44083 |  6617 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6618 | `			return SXERR_SYNTAX;` |
|       - |  6619 | `		}` |
|       - |  6620 | `	}` |
|   44079 |  6621 | `	if( bHandleClosure ){` |
|       - |  6622 | `		ph7_vm_func_closure_env sEnv;` |
|     257 |  6623 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     252 |  6624 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     139 |  6625 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      21 |  6626 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6627 | `				/* Closure,record environment variable */` |
|      21 |  6628 | `				pGen->pIn++;` |
|      21 |  6629 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6630 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6631 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6632 | `						return SXERR_ABORT;` |
|       - |  6633 | `					}` |
|     ! 0 |  6634 | `				}` |
|      21 |  6635 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6636 | `				/* Compile until we hit the first closing parenthesis */` |
|      41 |  6637 | `				while( pGen->pIn < pGen->pEnd ){` |
|      41 |  6638 | `					int iFlagsLocal = 0;` |
|      41 |  6639 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      21 |  6640 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      21 |  6641 | `						break;` |
|       - |  6642 | `					}` |
|      25 |  6643 | `					nLineLocal = pGen->pIn->nLine;` |
|      25 |  6644 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6645 | `						/* Pass by reference,record that */` |
|     ! 0 |  6646 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6647 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6648 | `							);` |
|     ! 0 |  6649 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6650 | `						pGen->pIn++;` |
|     ! 0 |  6651 | `					}` |
|      20 |  6652 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      25 |  6653 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6654 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6655 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6656 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6657 | `								return SXERR_ABORT;` |
|       - |  6658 | `							}` |
|       - |  6659 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6660 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6661 | `								pGen->pIn++;` |
|     ! 0 |  6662 | `							}` |
|     ! 0 |  6663 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6664 | `								pGen->pIn++;` |
|     ! 0 |  6665 | `							}` |
|     ! 0 |  6666 | `							break;` |
|       - |  6667 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6668 | `					}else{` |
|       - |  6669 | `						SyString *pNameLocal;` |
|       - |  6670 | `						char *zDup;` |
|       - |  6671 | `						/* Duplicate variable name */` |
|      25 |  6672 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      25 |  6673 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      25 |  6674 | `						if( zDup ){` |
|       - |  6675 | `							/* Zero the structure */` |
|      25 |  6676 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      25 |  6677 | `							sEnv.iFlags = iFlagsLocal;` |
|      25 |  6678 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      25 |  6679 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      25 |  6680 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6681 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6682 | `									got_this = 1;` |
|     ! 0 |  6683 | `							}` |
|       - |  6684 | `							/* Save imported variable */` |
|      25 |  6685 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      15 |  6686 | `						}else{` |
|     ! 0 |  6687 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6688 | `							 return SXERR_ABORT;` |
|       - |  6689 | `						}` |
|       - |  6690 | `					}` |
|      25 |  6691 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      31 |  6692 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6693 | `						/* Ignore trailing commas */` |
|       7 |  6694 | `						pGen->pIn++;` |
|       1 |  6695 | `					}` |
|       5 |  6696 | `				}` |
|      21 |  6697 | `				if( !got_this ){` |
|       - |  6698 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6699 | `					 * available to the closure environment.` |
|       - |  6700 | `					 */` |
|      21 |  6701 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      21 |  6702 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      21 |  6703 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      21 |  6704 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      21 |  6705 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       8 |  6706 | `				}` |
|      21 |  6707 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6708 | `					/* Mark as closure */` |
|      21 |  6709 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       8 |  6710 | `				}` |
|       8 |  6711 | `		}` |
|     126 |  6712 | `	}` |
|       - |  6713 | `	/* Compile the body */` |
|   44079 |  6714 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   44079 |  6715 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6716 | `		return SXERR_ABORT;` |
|       - |  6717 | `	}` |
|   44079 |  6718 | `	if( ppFunc ){` |
|     257 |  6719 | `		*ppFunc = pFunc;` |
|     126 |  6720 | `	}` |
|   44079 |  6721 | `	rc = SXRET_OK;` |
|   44079 |  6722 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6723 | `		/* Finally register the function */` |
|   44063 |  6724 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   22029 |  6725 | `	}` |
|   44079 |  6726 | `	if( rc == SXRET_OK ){` |
|   44079 |  6727 | `		return SXRET_OK;` |
|       - |  6728 | `	}` |
|       - |  6729 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6730 | `OutOfMem:` |
|       - |  6731 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6732 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6733 | `	 */` |
|     ! 0 |  6734 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6735 | `	return SXERR_ABORT;` |
|   22047 |  6736 |  |
|       - |  6737 | `/*` |
|       - |  6738 | ` * Compile a standard PHP function.` |
|       - |  6739 | ` *  Refer to the block-comment above for more information.` |
|       - |  6740 | ` */` |
|   43840 |  6741 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6742 |  |
|       - |  6743 | `	SyString *pName;` |
|       - |  6744 | `	sxi32 iFlags;` |
|       - |  6745 | `	sxu32 nLine;` |
|       - |  6746 | `	sxi32 rc;` |
|       - |  6747 |  |
|   43845 |  6748 | `	nLine = pGen->pIn->nLine;` |
|   43845 |  6749 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   43845 |  6750 | `	iFlags = 0;` |
|   43845 |  6751 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6752 | `		/* Return by reference,remember that */` |
|       7 |  6753 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6754 | `		/* Jump the '&' token */` |
|       7 |  6755 | `		pGen->pIn++;` |
|       3 |  6756 | `	}` |
|   43845 |  6757 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6758 | `		/* Invalid function name */` |
|       8 |  6759 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       8 |  6760 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6761 | `			return SXERR_ABORT;` |
|       - |  6762 | `		}` |
|       - |  6763 | `		/* Sychronize with the next semi-colon or braces*/` |
|      22 |  6764 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      16 |  6765 | `			pGen->pIn++;` |
|       2 |  6766 | `		}` |
|       8 |  6767 | `		return SXRET_OK;` |
|       - |  6768 | `	}` |
|   43839 |  6769 | `	pName = &pGen->pIn->sData;` |
|   43839 |  6770 | `	nLine = pGen->pIn->nLine;` |
|       - |  6771 | `	/* Jump the function name */` |
|   43839 |  6772 | `	pGen->pIn++;` |
|   43839 |  6773 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6774 | `		/* Syntax error */` |
|       3 |  6775 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6776 | `		if( rc == SXERR_ABORT ){` |
|       - |  6777 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6778 | `			return SXERR_ABORT;` |
|       - |  6779 | `		}` |
|       - |  6780 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6781 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6782 | `			pGen->pIn++;` |
|     ! 0 |  6783 | `		}` |
|       3 |  6784 | `		return SXRET_OK;` |
|       - |  6785 | `	}` |
|       - |  6786 | `	/* Compile function body */` |
|   43837 |  6787 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   43837 |  6788 | `	return rc;` |
|   21925 |  6789 |  |
|       - |  6790 | `/*` |
|       - |  6791 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6792 | ` * According to the PHP language reference manual` |
|       - |  6793 | ` *  Visibility:` |
|       - |  6794 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6795 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6796 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6797 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6798 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6799 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6800 | ` */` |
|  312400 |  6801 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  6802 |  |
|  312405 |  6803 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   10015 |  6804 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  302395 |  6805 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   43035 |  6806 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6807 | `	}` |
|       - |  6808 | `	/* Assume public by default */` |
|  259365 |  6809 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  156205 |  6810 |  |
|       - |  6811 | `/*` |
|       - |  6812 | ` * Compile a class constant.` |
|       - |  6813 | ` * According to the PHP language reference manual` |
|       - |  6814 | ` *  Class Constants` |
|       - |  6815 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6816 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6817 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6818 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6819 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6820 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6821 | ` * Symisc eXtension.` |
|       - |  6822 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6823 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6824 | ` *  Example:` |
|       - |  6825 | ` *   class Test{` |
|       - |  6826 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6827 | ` *   };` |
|       - |  6828 | ` *   var_dump(TEST::MyConst);` |
|       - |  6829 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6830 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6831 | ` */` |
|       - |  6832 | `/*` |
|       - |  6833 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  6834 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  6835 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  6836 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  6837 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  6838 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  6839 | ` */` |
|      76 |  6840 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  6841 |  |
|       - |  6842 | `	SyToken *p0, *p1;` |
|      81 |  6843 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6844 | `		return 0;` |
|       - |  6845 | `	}` |
|      81 |  6846 | `	p0 = pGen->pIn;` |
|       - |  6847 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      81 |  6848 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  6849 | `		return 1;` |
|       - |  6850 | `	}` |
|      81 |  6851 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  6852 | `		return 1;` |
|       - |  6853 | `	}` |
|       - |  6854 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  6855 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  6856 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      77 |  6857 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      77 |  6858 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      77 |  6859 | `		if( p1 ){` |
|      77 |  6860 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      24 |  6861 | `				return 1;` |
|       - |  6862 | `			}` |
|      56 |  6863 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  6864 | `				return 1;` |
|       - |  6865 | `			}` |
|      24 |  6866 | `		}` |
|      24 |  6867 | `	}` |
|      52 |  6868 | `	return 0;` |
|      43 |  6869 |  |
|       - |  6870 | `/*` |
|       - |  6871 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  6872 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  6873 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  6874 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  6875 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  6876 | ` * share the same backing.` |
|       - |  6877 | ` */` |
|     192 |  6878 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  6879 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  6880 |  |
|     197 |  6881 | `	pAttr->nType = nType;` |
|     197 |  6882 | `	pAttr->sClass = *pClass;` |
|     197 |  6883 | `	pAttr->sTypeName = *pTypeName;` |
|     197 |  6884 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  6885 | `		sxu32 i;` |
|      46 |  6886 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      32 |  6887 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      32 |  6888 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      18 |  6889 | `		}` |
|       7 |  6890 | `	}` |
|     197 |  6891 |  |
|      76 |  6892 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  6893 |  |
|      81 |  6894 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6895 | `	SySet *pInstrContainer;` |
|       - |  6896 | `	ph7_class_attr *pCons;` |
|       - |  6897 | `	SyString *pName;` |
|       - |  6898 | `	sxi32 rc;` |
|      81 |  6899 | `	sxu32 nType = 0;` |
|       - |  6900 | `	SyString sTypeClass;` |
|       - |  6901 | `	SyString sTypeText;` |
|       - |  6902 | `	SySet aUnionAlts;` |
|      81 |  6903 | `	sxi32 iTypeFlags = 0;` |
|      81 |  6904 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      81 |  6905 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      81 |  6906 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  6907 | `	/* Extract visibility level */` |
|      81 |  6908 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  6909 | `	/* Mark as constant */` |
|      81 |  6910 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      81 |  6911 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  6912 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  6913 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|      95 |  6914 | `	if( GenStateClassConstHasType(pGen) ){` |
|      46 |  6915 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      28 |  6916 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  6917 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  6918 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  6919 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  6920 | `		 * and success paths release. */` |
|      32 |  6921 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  6922 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  6923 | `			goto Synchronize;` |
|      32 |  6924 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  6925 | `			return SXERR_ABORT;` |
|      32 |  6926 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  6927 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  6928 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  6929 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6930 | `				return SXERR_ABORT;` |
|       - |  6931 | `			}` |
|     ! 0 |  6932 | `			goto Synchronize;` |
|       - |  6933 | `		}` |
|      32 |  6934 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      14 |  6935 | `	}` |
|      38 |  6936 | `loop:` |
|      83 |  6937 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6938 | `		/* Invalid constant name */` |
|     ! 0 |  6939 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6940 | `		if( rc == SXERR_ABORT ){` |
|       - |  6941 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6942 | `			return SXERR_ABORT;` |
|       - |  6943 | `		}` |
|     ! 0 |  6944 | `		goto Synchronize;` |
|       - |  6945 | `	}` |
|       - |  6946 | `	/* Peek constant name */` |
|      83 |  6947 | `	pName = &pGen->pIn->sData;` |
|       - |  6948 | `	/* Make sure the constant name isn't reserved */` |
|      83 |  6949 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6950 | `		/* Reserved constant name */` |
|     ! 0 |  6951 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6952 | `		if( rc == SXERR_ABORT ){` |
|       - |  6953 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6954 | `			return SXERR_ABORT;` |
|       - |  6955 | `		}` |
|     ! 0 |  6956 | `		goto Synchronize;` |
|       - |  6957 | `	}` |
|       - |  6958 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      83 |  6959 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      46 |  6960 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      28 |  6961 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      14 |  6962 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      32 |  6963 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6964 | `			return SXERR_ABORT;` |
|      32 |  6965 | `		}else if( rc != SXRET_OK ){` |
|       3 |  6966 | `			goto Synchronize;` |
|       - |  6967 | `		}` |
|      13 |  6968 | `	}` |
|       - |  6969 | `	/* Advance the stream cursor */` |
|      81 |  6970 | `	pGen->pIn++;` |
|      81 |  6971 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6972 | `		/* Invalid declaration */` |
|     ! 0 |  6973 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6974 | `		if( rc == SXERR_ABORT ){` |
|       - |  6975 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6976 | `			return SXERR_ABORT;` |
|       - |  6977 | `		}` |
|     ! 0 |  6978 | `		goto Synchronize;` |
|       - |  6979 | `	}` |
|      81 |  6980 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6981 | `	/* Allocate a new class attribute */` |
|      81 |  6982 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      81 |  6983 | `	if( pCons == 0 ){` |
|     ! 0 |  6984 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6985 | `		return SXERR_ABORT;` |
|       - |  6986 | `	}` |
|      81 |  6987 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      29 |  6988 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      13 |  6989 | `	}` |
|       - |  6990 | `	/* Swap bytecode container */` |
|      81 |  6991 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      81 |  6992 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6993 | `	/* Compile constant value.` |
|       - |  6994 | `	 */` |
|      81 |  6995 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      81 |  6996 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6997 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6998 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6999 | `			return SXERR_ABORT;` |
|       - |  7000 | `		}` |
|       1 |  7001 | `	}` |
|       - |  7002 | `	/* Emit the done instruction */` |
|      81 |  7003 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      81 |  7004 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      81 |  7005 | `	if( rc == SXERR_ABORT ){` |
|       - |  7006 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7007 | `		return SXERR_ABORT;` |
|       - |  7008 | `	}` |
|       - |  7009 | `	/* All done,install the constant */` |
|      81 |  7010 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      81 |  7011 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7012 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7013 | `		return SXERR_ABORT;` |
|       - |  7014 | `	}` |
|      81 |  7015 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7016 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7017 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7018 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7019 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7020 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7021 | `				pTok--;` |
|     ! 0 |  7022 | `			}` |
|     ! 0 |  7023 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7024 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7025 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7026 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7027 | `				return SXERR_ABORT;` |
|       - |  7028 | `			}` |
|     ! 0 |  7029 | `		}else{` |
|       3 |  7030 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7031 | `				goto loop;` |
|       - |  7032 | `			}` |
|       - |  7033 | `		}` |
|     ! 0 |  7034 | `	}` |
|      79 |  7035 | `	SySetRelease(&aUnionAlts);` |
|      79 |  7036 | `	return SXRET_OK;` |
|       1 |  7037 | `Synchronize:` |
|       3 |  7038 | `	SySetRelease(&aUnionAlts);` |
|       - |  7039 | `	/* Synchronize with the first semi-colon */` |
|       9 |  7040 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  7041 | `		pGen->pIn++;` |
|       1 |  7042 | `	}` |
|       3 |  7043 | `	return SXERR_CORRUPT;` |
|      43 |  7044 |  |
|       - |  7045 | `/*` |
|       - |  7046 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7047 | ` * According to the PHP language reference manual` |
|       - |  7048 | ` *  Properties` |
|       - |  7049 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7050 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7051 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7052 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7053 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7054 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7055 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7056 | ` * Symisc eXtension.` |
|       - |  7057 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7058 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7059 | ` *  Example:` |
|       - |  7060 | ` *   class Test{` |
|       - |  7061 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7062 | ` *   };` |
|       - |  7063 | ` *   var_dump(TEST::myVar);` |
|       - |  7064 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7065 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7066 | ` */` |
|       - |  7067 | `/*` |
|       - |  7068 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7069 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7070 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7071 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7072 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7073 | ` */` |
|  162998 |  7074 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7075 |  |
|  163003 |  7076 | `	SyToken *p = pStart;` |
|  163003 |  7077 | `	if( p >= pEnd ) return 0;` |
|  163003 |  7078 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      18 |  7079 | `		p++;` |
|      18 |  7080 | `		if( p >= pEnd ) return 0;` |
|       8 |  7081 | `	}` |
|  163003 |  7082 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  7083 | `		p++;` |
|       3 |  7084 | `		if( p >= pEnd ) return 0;` |
|       1 |  7085 | `	}` |
|  163003 |  7086 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7087 | `		return 0;` |
|       - |  7088 | `	}` |
|       - |  7089 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  7090 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  7091 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  7092 | `	 * dispatch site. */` |
|  163003 |  7093 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  162981 |  7094 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  162976 |  7095 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3516 |  7096 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  162833 |  7097 | `			return 0;` |
|       - |  7098 | `		}` |
|      74 |  7099 | `	}` |
|     175 |  7100 | `	p++;` |
|       - |  7101 | `	/* Consume optional namespace path */` |
|     177 |  7102 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7103 | `		p += 2;` |
|       1 |  7104 | `	}` |
|       - |  7105 | ``	/* Consume any `\| Type` union alternatives */`` |
|     273 |  7106 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|     108 |  7107 | `		&& p->sData.zString[0] == '\|' ){` |
|      16 |  7108 | `		p++;` |
|      16 |  7109 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      16 |  7110 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      16 |  7111 | `		p++;` |
|      16 |  7112 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7113 | `			p += 2;` |
|     ! 0 |  7114 | `		}` |
|       4 |  7115 | `	}` |
|     175 |  7116 | `	if( p >= pEnd ) return 0;` |
|     175 |  7117 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   81504 |  7118 |  |
|       - |  7119 |  |
|       - |  7120 | `/*` |
|       - |  7121 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7122 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7123 | ` * if not). Recognized forms:` |
|       - |  7124 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7125 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7126 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7127 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7128 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7129 | ` * on unrecoverable error.` |
|       - |  7130 | ` *` |
|       - |  7131 | ` * When a type is parsed:` |
|       - |  7132 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7133 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7134 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7135 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7136 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7137 | ` */` |
|     170 |  7138 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7139 | `	ph7_gen_state *pGen,` |
|       - |  7140 | `	sxu32 *pnType,` |
|       - |  7141 | `	SyString *pClass,` |
|       - |  7142 | `	sxi32 *piTypeFlags,` |
|       - |  7143 | `	SyString *pTypeText,` |
|       - |  7144 | `	SySet *pAlts` |
|       5 |  7145 | `){` |
|     175 |  7146 | `	sxi32 iFlags = 0;` |
|       - |  7147 | `	sxi32 rc;` |
|     175 |  7148 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7149 | `		return SXRET_OK;` |
|       - |  7150 | `	}` |
|       - |  7151 | `	/* If the first token is '$', there's no type */` |
|     175 |  7152 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7153 | `		return SXRET_OK;` |
|       - |  7154 | `	}` |
|     175 |  7155 | `	rc = GenStateParseUnionTypeDecl(` |
|      85 |  7156 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7157 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7158 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7159 | `		/* bAllowVoid */ 0,` |
|     170 |  7160 | `		pGen->pIn->nLine);` |
|     175 |  7161 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7162 | `		return rc;` |
|       - |  7163 | `	}` |
|       - |  7164 | `	/* Verify next token is '$' (start of property name) */` |
|     175 |  7165 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7166 | `		return SXERR_SYNTAX;` |
|       - |  7167 | `	}` |
|     175 |  7168 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     175 |  7169 | `	return SXRET_OK;` |
|      90 |  7170 |  |
|       - |  7171 |  |
|       - |  7172 | `/*` |
|       - |  7173 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7174 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7175 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7176 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7177 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7178 | ` * by the type parser itself before reaching here.` |
|       - |  7179 | ` *` |
|       - |  7180 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7181 | ` * use in the error message.` |
|       - |  7182 | ` */` |
|     286 |  7183 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7184 | `	sxu32 nType,` |
|       - |  7185 | `	const SyString *pClass,` |
|       - |  7186 | `	const char **pzName,` |
|       - |  7187 | `	sxu32 *pnName)` |
|       5 |  7188 |  |
|       - |  7189 | `	const char *z;` |
|       - |  7190 | `	sxu32 n;` |
|     291 |  7191 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     255 |  7192 | `		return 0;` |
|       - |  7193 | `	}` |
|      41 |  7194 | `	z = pClass->zString;` |
|      41 |  7195 | `	n = pClass->nByte;` |
|      41 |  7196 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7197 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7198 | `	}` |
|       - |  7199 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7200 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7201 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      33 |  7202 | `	return 0;` |
|     148 |  7203 |  |
|       - |  7204 |  |
|       - |  7205 | `/*` |
|       - |  7206 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7207 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7208 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7209 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7210 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7211 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7212 | ` *` |
|       - |  7213 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7214 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7215 | ` */` |
|     250 |  7216 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7217 | `	ph7_gen_state *pGen,` |
|       - |  7218 | `	ph7_class *pClass,` |
|       - |  7219 | `	const SyString *pMemberName,` |
|       - |  7220 | `	sxu32 nType,` |
|       - |  7221 | `	const SyString *pTypeClass,` |
|       - |  7222 | `	const SyString *pTypeText,` |
|       - |  7223 | `	SySet *pUnionAlts,` |
|       - |  7224 | `	const char *zErrFmt,` |
|       - |  7225 | `	sxu32 nLine)` |
|       5 |  7226 |  |
|     255 |  7227 | `	const char *zBad = 0;` |
|     255 |  7228 | `	sxu32 nBad = 0;` |
|       - |  7229 | `	SyString sFallback;` |
|       - |  7230 | `	const SyString *pBad;` |
|       - |  7231 | `	sxi32 rc;` |
|     255 |  7232 | `	int bDisallowed = 0;` |
|     255 |  7233 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7234 | `		bDisallowed = 1;` |
|     253 |  7235 | `	}else if( pUnionAlts ){` |
|       - |  7236 | `		sxu32 i;` |
|      56 |  7237 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      40 |  7238 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      40 |  7239 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7240 | `				bDisallowed = 1;` |
|       3 |  7241 | `				break;` |
|       - |  7242 | `			}` |
|      21 |  7243 | `		}` |
|       9 |  7244 | `	}` |
|     255 |  7245 | `	if( !bDisallowed ){` |
|     249 |  7246 | `		return SXRET_OK;` |
|       - |  7247 | `	}` |
|       - |  7248 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7249 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7250 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7251 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7252 | `		pBad = pTypeText;` |
|       5 |  7253 | `	}else{` |
|     ! 0 |  7254 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7255 | `		pBad = &sFallback;` |
|       - |  7256 | `	}` |
|      11 |  7257 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7258 | `		zErrFmt,` |
|       3 |  7259 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7260 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7261 | `		return SXERR_ABORT;` |
|       - |  7262 | `	}` |
|       8 |  7263 | `	return SXERR_SYNTAX;` |
|     130 |  7264 |  |
|       - |  7265 | `/*` |
|       - |  7266 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7267 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7268 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7269 | ` * than promoted to a lexer keyword.` |
|       - |  7270 | ` */` |
| 1475020 |  7271 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7272 |  |
| 1504703 |  7273 | `	return (pTok->nType & PH7_TK_ID)` |
|  767188 |  7274 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1504698 |  7275 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7276 |  |
|   63424 |  7277 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7278 |  |
|   63429 |  7279 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7280 | `	ph7_class_attr *pAttr;` |
|       - |  7281 | `	SyString *pName;` |
|       - |  7282 | `	sxi32 rc;` |
|   63429 |  7283 | `	sxu32 nType = 0;` |
|       - |  7284 | `	SyString sTypeClass;` |
|       - |  7285 | `	SyString sTypeText;` |
|       - |  7286 | `	SySet aUnionAlts;` |
|   63429 |  7287 | `	sxi32 iTypeFlags = 0;` |
|   63429 |  7288 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   63429 |  7289 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   63429 |  7290 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7291 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7292 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7293 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   63429 |  7294 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7295 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7296 | `	}` |
|       - |  7297 | `	/* Extract visibility level */` |
|   63429 |  7298 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7299 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   63514 |  7300 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     175 |  7301 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     175 |  7302 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7303 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7304 | `			goto Synchronize;` |
|     175 |  7305 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7306 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7307 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7308 | `				&pGen->pIn->sData);` |
|     ! 0 |  7309 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7310 | `				return SXERR_ABORT;` |
|       - |  7311 | `			}` |
|     ! 0 |  7312 | `			goto Synchronize;` |
|     175 |  7313 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7314 | `			return SXERR_ABORT;` |
|       - |  7315 | `		}` |
|      85 |  7316 | `	}` |
|     ! 0 |  7317 | `loop:` |
|   63433 |  7318 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7319 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7320 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7321 | `			return SXERR_ABORT;` |
|       - |  7322 | `		}` |
|     ! 0 |  7323 | `		goto Synchronize;` |
|       - |  7324 | `	}` |
|   63433 |  7325 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   63433 |  7326 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7327 | `		/* Invalid attribute name */` |
|     ! 0 |  7328 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7329 | `		if( rc == SXERR_ABORT ){` |
|       - |  7330 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7331 | `			return SXERR_ABORT;` |
|       - |  7332 | `		}` |
|     ! 0 |  7333 | `		goto Synchronize;` |
|       - |  7334 | `	}` |
|       - |  7335 | `	/* Peek attribute name */` |
|   63433 |  7336 | `	pName = &pGen->pIn->sData;` |
|       - |  7337 | `	/* Advance the stream cursor */` |
|   63433 |  7338 | `	pGen->pIn++;` |
|   63433 |  7339 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7340 | `		/* Invalid declaration */` |
|       3 |  7341 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7342 | `		if( rc == SXERR_ABORT ){` |
|       - |  7343 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7344 | `			return SXERR_ABORT;` |
|       - |  7345 | `		}` |
|       3 |  7346 | `		goto Synchronize;` |
|       - |  7347 | `	}` |
|       - |  7348 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7349 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   63431 |  7350 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7351 | `		const char *zRoErr = 0;` |
|      39 |  7352 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7353 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7354 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7355 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7356 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7357 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7358 | `		}` |
|      39 |  7359 | `		if( zRoErr ){` |
|      13 |  7360 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7361 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7362 | `				return SXERR_ABORT;` |
|       - |  7363 | `			}` |
|      13 |  7364 | `			goto Synchronize;` |
|       - |  7365 | `		}` |
|      12 |  7366 | `	}` |
|       - |  7367 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7368 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7369 | `	 * by the type parser. */` |
|   63421 |  7370 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     257 |  7371 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7372 | `			&sTypeText,` |
|     168 |  7373 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      84 |  7374 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     173 |  7375 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7376 | `			return SXERR_ABORT;` |
|     173 |  7377 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7378 | `			goto Synchronize;` |
|       - |  7379 | `		}` |
|      84 |  7380 | `	}` |
|       - |  7381 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   63421 |  7382 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7383 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7384 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7385 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7386 | `			return SXERR_ABORT;` |
|       - |  7387 | `		}` |
|       3 |  7388 | `		goto Synchronize;` |
|       - |  7389 | `	}` |
|       - |  7390 | `	/* Allocate a new class attribute */` |
|   63419 |  7391 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   63419 |  7392 | `	if( pAttr == 0 ){` |
|     ! 0 |  7393 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7394 | `		return SXERR_ABORT;` |
|       - |  7395 | `	}` |
|   63419 |  7396 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     171 |  7397 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      83 |  7398 | `	}` |
|   63419 |  7399 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7400 | `		SySet *pInstrContainer;` |
|   20281 |  7401 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7402 | `		/* Swap bytecode container */` |
|   20281 |  7403 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   20281 |  7404 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7405 | `		/* Compile attribute value.` |
|       - |  7406 | `		 */` |
|   20281 |  7407 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   20281 |  7408 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7409 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7410 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7411 | `				return SXERR_ABORT;` |
|       - |  7412 | `			}` |
|     ! 0 |  7413 | `		}` |
|       - |  7414 | `		/* Emit the done instruction */` |
|   20281 |  7415 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   20281 |  7416 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   10138 |  7417 | `	}` |
|       - |  7418 | `	/* All done,install the attribute */` |
|   63419 |  7419 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   63419 |  7420 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7421 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7422 | `		return SXERR_ABORT;` |
|       - |  7423 | `	}` |
|   63419 |  7424 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7425 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7426 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7427 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7428 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7429 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7430 | `				pTok--;` |
|     ! 0 |  7431 | `			}` |
|     ! 0 |  7432 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7433 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7434 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7435 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7436 | `				return SXERR_ABORT;` |
|       - |  7437 | `			}` |
|     ! 0 |  7438 | `		}else{` |
|       5 |  7439 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7440 | `				goto loop;` |
|       - |  7441 | `			}` |
|       - |  7442 | `		}` |
|     ! 0 |  7443 | `	}` |
|   63415 |  7444 | `	SySetRelease(&aUnionAlts);` |
|   63415 |  7445 | `	return SXRET_OK;` |
|       7 |  7446 | `Synchronize:` |
|       - |  7447 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7448 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      16 |  7449 | `		pGen->pIn++;` |
|       2 |  7450 | `	}` |
|      17 |  7451 | `	SySetRelease(&aUnionAlts);` |
|      17 |  7452 | `	return SXERR_CORRUPT;` |
|   31717 |  7453 |  |
|       - |  7454 | `/*` |
|       - |  7455 | ` * Compile a class method.` |
|       - |  7456 | ` *` |
|       - |  7457 | ` * Refer to the official documentation for more information` |
|       - |  7458 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7459 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7460 | ` * overloading and many more.` |
|       - |  7461 | ` */` |
|  248900 |  7462 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7463 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7464 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7465 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7466 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7467 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7468 | `	)` |
|       5 |  7469 |  |
|  248905 |  7470 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7471 | `	ph7_class_method *pMeth;` |
|       - |  7472 | `	sxi32 iFuncFlags;` |
|       - |  7473 | `	SyString *pName;` |
|       - |  7474 | `	SyToken *pEnd;` |
|       - |  7475 | `	sxi32 rc;` |
|       - |  7476 | `	/* Extract visibility level */` |
|  248905 |  7477 | `	iProtection = GetProtectionLevel(iProtection);` |
|  248905 |  7478 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  248905 |  7479 | `	iFuncFlags = 0;` |
|  248905 |  7480 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7481 | `		/* Invalid method name */` |
|     ! 0 |  7482 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7483 | `		if( rc == SXERR_ABORT ){` |
|       - |  7484 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7485 | `			return SXERR_ABORT;` |
|       - |  7486 | `		}` |
|     ! 0 |  7487 | `		goto Synchronize;` |
|       - |  7488 | `	}` |
|  248905 |  7489 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7490 | `		/* Return by reference,remember that */` |
|     ! 0 |  7491 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7492 | `		/* Jump the '&' token */` |
|     ! 0 |  7493 | `		pGen->pIn++;` |
|     ! 0 |  7494 | `	}` |
|  248905 |  7495 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7496 | `		/* Invalid method name */` |
|     ! 0 |  7497 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7498 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7499 | `			return SXERR_ABORT;` |
|       - |  7500 | `		}` |
|     ! 0 |  7501 | `		goto Synchronize;` |
|       - |  7502 | `	}` |
|       - |  7503 | `	/* Peek method name */` |
|  248905 |  7504 | `	pName = &pGen->pIn->sData;` |
|  248905 |  7505 | `	nLine = pGen->pIn->nLine;` |
|       - |  7506 | `	/* Jump the method name */` |
|  248905 |  7507 | `	pGen->pIn++;` |
|  248905 |  7508 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7509 | `		/* Abstract method */` |
|   86015 |  7510 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7511 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7512 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7513 | `				&pClass->sName,pName);` |
|     ! 0 |  7514 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7515 | `				return SXERR_ABORT;` |
|       - |  7516 | `			}` |
|     ! 0 |  7517 | `		}` |
|       - |  7518 | `		/* Assemble method signature only */` |
|   86015 |  7519 | `		doBody = FALSE;` |
|   43005 |  7520 | `	}` |
|  248905 |  7521 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7522 | `		/* Syntax error */` |
|     ! 0 |  7523 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7524 | `		if( rc == SXERR_ABORT ){` |
|       - |  7525 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7526 | `			return SXERR_ABORT;` |
|       - |  7527 | `		}` |
|     ! 0 |  7528 | `		goto Synchronize;` |
|       - |  7529 | `	}` |
|       - |  7530 | `	/* Allocate a new class_method instance */` |
|  248905 |  7531 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  248905 |  7532 | `	if( pMeth == 0 ){` |
|     ! 0 |  7533 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7534 | `		return SXERR_ABORT;` |
|       - |  7535 | `	}` |
|       - |  7536 | `	/* Jump the left parenthesis '(' */` |
|  248905 |  7537 | `	pGen->pIn++;` |
|  248905 |  7538 | `	pEnd = 0; /* cc warning */` |
|       - |  7539 | `	/* Delimit the method signature */` |
|  248905 |  7540 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  248905 |  7541 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7542 | `		/* Syntax error */` |
|       3 |  7543 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7544 | `		if( rc == SXERR_ABORT ){` |
|       - |  7545 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7546 | `			return SXERR_ABORT;` |
|       - |  7547 | `		}` |
|       3 |  7548 | `		goto Synchronize;` |
|       - |  7549 | `	}` |
|       - |  7550 | `	{` |
|  248903 |  7551 | `		int bIsCtor = 0;` |
|  248903 |  7552 | `		int bAbstractCtor = 0;` |
|  248898 |  7553 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  147701 |  7554 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  238912 |  7555 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   19987 |  7556 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7557 | `				bAbstractCtor = 1;` |
|       2 |  7558 | `			}else{` |
|   19985 |  7559 | `				bIsCtor = 1;` |
|       - |  7560 | `			}` |
|    9991 |  7561 | `		}` |
|  248903 |  7562 | `		if( pGen->pIn < pEnd ){` |
|       - |  7563 | `			/* Collect method arguments */` |
|   56573 |  7564 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   56573 |  7565 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7566 | `				return SXERR_ABORT;` |
|       - |  7567 | `			}` |
|   28284 |  7568 | `		}` |
|       - |  7569 | `	}` |
|       - |  7570 | `	/* Point past ')' and parse optional return type ': type' */` |
|  248903 |  7571 | `	pGen->pIn = &pEnd[1];` |
|       - |  7572 | `	{` |
|  248903 |  7573 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  248903 |  7574 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7575 | `			return SXERR_ABORT;` |
|  248903 |  7576 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7577 | `			goto Synchronize;` |
|       - |  7578 | `		}` |
|       - |  7579 | `	}` |
|       - |  7580 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7581 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7582 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7583 | `	{` |
|  248903 |  7584 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7585 | `		sxu32 i;` |
|  338629 |  7586 | `		for( i = 0; i < nArg; i++ ){` |
|   89741 |  7587 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7588 | `			ph7_class_attr *pAttr;` |
|   89741 |  7589 | `			sxi32 iAttrFlags = 0;` |
|   89741 |  7590 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   89681 |  7591 | `				continue;` |
|       - |  7592 | `			}` |
|      65 |  7593 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7594 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7595 | `					"Cannot declare variadic promoted property");` |
|       3 |  7596 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7597 | `					return SXERR_ABORT;` |
|       - |  7598 | `				}` |
|       3 |  7599 | `				goto Synchronize;` |
|       - |  7600 | `			}` |
|       - |  7601 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7602 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7603 | `			 * appear as an alternative of a union type. */` |
|      58 |  7604 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      13 |  7605 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      86 |  7606 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      54 |  7607 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      54 |  7608 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      27 |  7609 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      59 |  7610 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7611 | `					return SXERR_ABORT;` |
|      59 |  7612 | `				}else if( rc != SXRET_OK ){` |
|       6 |  7613 | `					goto Synchronize;` |
|       - |  7614 | `				}` |
|      25 |  7615 | `			}` |
|       - |  7616 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      59 |  7617 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7618 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7619 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7620 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7621 | `					return SXERR_ABORT;` |
|       - |  7622 | `				}` |
|       3 |  7623 | `				goto Synchronize;` |
|       - |  7624 | `			}` |
|      57 |  7625 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      51 |  7626 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      23 |  7627 | `			}` |
|      57 |  7628 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7629 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7630 | `			}` |
|      57 |  7631 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7632 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7633 | `			}` |
|      57 |  7634 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  7635 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  7636 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  7637 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  7638 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7639 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  7640 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7641 | `						return SXERR_ABORT;` |
|       - |  7642 | `					}` |
|       3 |  7643 | `					goto Synchronize;` |
|       - |  7644 | `				}` |
|      22 |  7645 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7646 | `			}` |
|      55 |  7647 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      55 |  7648 | `			if( pAttr == 0 ){` |
|     ! 0 |  7649 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7650 | `				return SXERR_ABORT;` |
|       - |  7651 | `			}` |
|      55 |  7652 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      51 |  7653 | `				pAttr->nType = pArg->nType;` |
|      51 |  7654 | `				pAttr->sClass = pArg->sClass;` |
|      51 |  7655 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      51 |  7656 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7657 | `					sxu32 k;` |
|     ! 0 |  7658 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7659 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7660 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7661 | `					}` |
|     ! 0 |  7662 | `				}` |
|      23 |  7663 | `			}` |
|      55 |  7664 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      55 |  7665 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7666 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7667 | `				return SXERR_ABORT;` |
|       - |  7668 | `			}` |
|      30 |  7669 | `		}` |
|       - |  7670 | `	}` |
|  248893 |  7671 | `	if( doBody ){` |
|       - |  7672 | `		/* Compile method body */` |
|  162883 |  7673 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  162883 |  7674 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7675 | `			return SXERR_ABORT;` |
|       - |  7676 | `		}` |
|   81444 |  7677 | `	}else{` |
|       - |  7678 | `		/* Only method signature is allowed */` |
|   86015 |  7679 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7680 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7681 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7682 | `				if( rc == SXERR_ABORT ){` |
|       - |  7683 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7684 | `					return SXERR_ABORT;` |
|       - |  7685 | `				}` |
|     ! 0 |  7686 | `				return SXERR_CORRUPT;` |
|       - |  7687 | `			}` |
|       - |  7688 | `	}` |
|       - |  7689 | `	/* All done,install the method */` |
|  248893 |  7690 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  248893 |  7691 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7692 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7693 | `		return SXERR_ABORT;` |
|       - |  7694 | `	}` |
|  248893 |  7695 | `	return SXRET_OK;` |
|       6 |  7696 | `Synchronize:` |
|       - |  7697 | `	/* Synchronize with the first semi-colon */` |
|      40 |  7698 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  7699 | `		pGen->pIn++;` |
|       4 |  7700 | `	}` |
|      16 |  7701 | `	return SXERR_CORRUPT;` |
|  124455 |  7702 |  |
|       - |  7703 | `/*` |
|       - |  7704 | ` * Compile an object interface.` |
|       - |  7705 | ` *  According to the PHP language reference manual` |
|       - |  7706 | ` *   Object Interfaces:` |
|       - |  7707 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7708 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7709 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7710 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7711 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7712 | ` */` |
|   36418 |  7713 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  7714 |  |
|   36423 |  7715 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7716 | `	ph7_class *pClass,*pBase;` |
|       - |  7717 | `	SyToken *pEnd,*pTmp;` |
|       - |  7718 | `	SyString *pName;` |
|       - |  7719 | `	sxi32 nKwrd;` |
|       - |  7720 | `	sxi32 rc;` |
|       - |  7721 | `	/* Jump the 'interface' keyword */` |
|   36423 |  7722 | `	pGen->pIn++;` |
|       - |  7723 | `	/* Extract interface name */` |
|   36423 |  7724 | `	pName = &pGen->pIn->sData;` |
|       - |  7725 | `	/* Advance the stream cursor */` |
|   36423 |  7726 | `	pGen->pIn++;` |
|       - |  7727 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7728 | `		SyBlob sFQN;` |
|       - |  7729 | `		SyString sFQNStr;` |
|   36423 |  7730 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   36423 |  7731 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   36423 |  7732 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   36423 |  7733 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   36423 |  7734 | `		SyBlobRelease(&sFQN);` |
|       - |  7735 | `	}` |
|   36423 |  7736 | `	if( pClass == 0 ){` |
|     ! 0 |  7737 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7738 | `		return SXERR_ABORT;` |
|       - |  7739 | `	}` |
|       - |  7740 | `	/* Mark as an interface */` |
|   36423 |  7741 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7742 | `	/* Assume no base class is given */` |
|   36423 |  7743 | `	pBase = 0;` |
|   36423 |  7744 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    9931 |  7745 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    9931 |  7746 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7747 | `			SyBlob sResolved;` |
|       - |  7748 | `			SyString sBaseName;` |
|       - |  7749 | `			sxu32 nRefLine;` |
|       - |  7750 | `			/* Extract base interface */` |
|    9931 |  7751 | `			pGen->pIn++;` |
|    9931 |  7752 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    9931 |  7753 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    9931 |  7754 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7755 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7756 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7757 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7758 | `					pName);` |
|     ! 0 |  7759 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7760 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7761 | `					return SXERR_ABORT;` |
|       - |  7762 | `				}` |
|     ! 0 |  7763 | `				return SXRET_OK;` |
|       - |  7764 | `			}` |
|   14894 |  7765 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    9926 |  7766 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    9931 |  7767 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7768 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7769 | `			/* Only interfaces is allowed */` |
|    9931 |  7770 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7771 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7772 | `			}` |
|    9931 |  7773 | `			if( pBase == 0 ){` |
|     ! 0 |  7774 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7775 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  7776 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7777 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7778 | `					return SXERR_ABORT;` |
|       - |  7779 | `				}` |
|     ! 0 |  7780 | `			}` |
|    9931 |  7781 | `			SyBlobRelease(&sResolved);` |
|    4963 |  7782 | `		}` |
|    4963 |  7783 | `	}` |
|   36423 |  7784 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7785 | `		/* Syntax error */` |
|     ! 0 |  7786 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7787 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7788 | `		if( rc == SXERR_ABORT ){` |
|       - |  7789 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7790 | `			return SXERR_ABORT;` |
|       - |  7791 | `		}` |
|     ! 0 |  7792 | `		return SXRET_OK;` |
|       - |  7793 | `	}` |
|   36423 |  7794 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   36423 |  7795 | `	pEnd = 0; /* cc warning */` |
|       - |  7796 | `	/* Delimit the interface body */` |
|   36423 |  7797 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   36423 |  7798 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7799 | `		/* Syntax error */` |
|     ! 0 |  7800 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7801 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7802 | `		if( rc == SXERR_ABORT ){` |
|       - |  7803 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7804 | `			return SXERR_ABORT;` |
|       - |  7805 | `		}` |
|     ! 0 |  7806 | `		return SXRET_OK;` |
|       - |  7807 | `	}` |
|       - |  7808 | `	/* Swap token stream */` |
|   36423 |  7809 | `	pTmp = pGen->pEnd;` |
|   36423 |  7810 | `	pGen->pEnd = pEnd;` |
|       - |  7811 | `	/* Start the parse process` |
|       - |  7812 | `	 * Note (According to the PHP reference manual):` |
|       - |  7813 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7814 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7815 | `	 */` |
|   61210 |  7816 | `	for(;;){` |
|       - |  7817 | `		/* Jump leading/trailing semi-colons */` |
|  208427 |  7818 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   86007 |  7819 | `			pGen->pIn++;` |
|       5 |  7820 | `		}` |
|  122425 |  7821 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7822 | `			/* End of interface body */` |
|   36421 |  7823 | `			break;` |
|       - |  7824 | `		}` |
|   86009 |  7825 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7826 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7827 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7828 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7829 | `			if( rc == SXERR_ABORT ){` |
|       - |  7830 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7831 | `				return SXERR_ABORT;` |
|       - |  7832 | `			}` |
|     ! 0 |  7833 | `			goto done;` |
|       - |  7834 | `		}` |
|       - |  7835 | `		/* Extract the current keyword */` |
|   86009 |  7836 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   86009 |  7837 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7838 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7839 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7840 | `			const char *zKind = "member";` |
|       3 |  7841 | `			SyString *pMemberName = 0;` |
|       3 |  7842 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7843 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7844 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7845 | `					zKind = "constant";` |
|       3 |  7846 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7847 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7848 | `					}` |
|       1 |  7849 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7850 | `					zKind = "method";` |
|     ! 0 |  7851 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7852 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7853 | `					}` |
|     ! 0 |  7854 | `				}` |
|       1 |  7855 | `			}` |
|       3 |  7856 | `			if( pMemberName ){` |
|       4 |  7857 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7858 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7859 | `			}else{` |
|     ! 0 |  7860 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7861 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7862 | `			}` |
|       3 |  7863 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7864 | `				return SXERR_ABORT;` |
|       - |  7865 | `			}` |
|       3 |  7866 | `			goto done;` |
|       - |  7867 | `		}` |
|   86007 |  7868 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7869 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7870 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7871 | `			if( rc == SXERR_ABORT ){` |
|       - |  7872 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7873 | `				return SXERR_ABORT;` |
|       - |  7874 | `			}` |
|     ! 0 |  7875 | `			goto done;` |
|       - |  7876 | `		}` |
|   86007 |  7877 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7878 | `			/* Advance the stream cursor */` |
|   85997 |  7879 | `			pGen->pIn++;` |
|   85997 |  7880 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7881 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7882 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7883 | `				if( rc == SXERR_ABORT ){` |
|       - |  7884 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7885 | `					return SXERR_ABORT;` |
|       - |  7886 | `				}` |
|     ! 0 |  7887 | `				goto done;` |
|       - |  7888 | `			}` |
|   85997 |  7889 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   85997 |  7890 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7891 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7892 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7893 | `				if( rc == SXERR_ABORT ){` |
|       - |  7894 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7895 | `					return SXERR_ABORT;` |
|       - |  7896 | `				}` |
|     ! 0 |  7897 | `				goto done;` |
|       - |  7898 | `			}` |
|   42996 |  7899 | `		}` |
|   86007 |  7900 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7901 | `			/* Parse constant */` |
|       7 |  7902 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       7 |  7903 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7904 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7905 | `					return SXERR_ABORT;` |
|       - |  7906 | `				}` |
|     ! 0 |  7907 | `				goto done;` |
|       - |  7908 | `			}` |
|       4 |  7909 | `		}else{` |
|   86001 |  7910 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   86001 |  7911 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7912 | `				/* Static method,record that */` |
|    9923 |  7913 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7914 | `				/* Advance the stream cursor */` |
|    9923 |  7915 | `				pGen->pIn++;` |
|    9918 |  7916 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    9923 |  7917 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7918 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7919 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7920 | `						if( rc == SXERR_ABORT ){` |
|       - |  7921 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7922 | `							return SXERR_ABORT;` |
|       - |  7923 | `						}` |
|     ! 0 |  7924 | `						goto done;` |
|       - |  7925 | `				}` |
|    4959 |  7926 | `			}` |
|       - |  7927 | `			/* Process method signature (no body for interface methods) */` |
|   86001 |  7928 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   86001 |  7929 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7930 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7931 | `					return SXERR_ABORT;` |
|       - |  7932 | `				}` |
|     ! 0 |  7933 | `				goto done;` |
|       - |  7934 | `			}` |
|       - |  7935 | `		}` |
|       5 |  7936 | `	}` |
|       - |  7937 | `	/* Install the interface */` |
|   36421 |  7938 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   36421 |  7939 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7940 | `		/* Inherit from the base interface */` |
|    9931 |  7941 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    4963 |  7942 | `	}` |
|   36421 |  7943 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7944 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7945 | `		return SXERR_ABORT;` |
|       - |  7946 | `	}` |
|   18208 |  7947 | `done:` |
|       - |  7948 | `	/* Point beyond the interface body */` |
|   36423 |  7949 | `	pGen->pIn  = &pEnd[1];` |
|   36423 |  7950 | `	pGen->pEnd = pTmp;` |
|   36423 |  7951 | `	return PH7_OK;` |
|   18214 |  7952 |  |
|       - |  7953 | `/*` |
|       - |  7954 | ` * Compile a user-defined class.` |
|       - |  7955 | ` * According to the PHP language reference manual` |
|       - |  7956 | ` *  class` |
|       - |  7957 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7958 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7959 | ` *  of the properties and methods belonging to the class.` |
|       - |  7960 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7961 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7962 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7963 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7964 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7965 | ` *  (called "methods").` |
|       - |  7966 | ` */` |
|       - |  7967 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7968 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7969 | `struct TraitUseEntry {` |
|       - |  7970 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7971 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7972 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7973 | `};` |
|       - |  7974 | `/*` |
|       - |  7975 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7976 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7977 | ` */` |
|   90326 |  7978 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  7979 |  |
|       - |  7980 | `	ph7_class **apIface;` |
|       - |  7981 | `	sxu32 nIface,i;` |
|       - |  7982 | `	sxi32 rc;` |
|   90331 |  7983 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7984 | `		return SXRET_OK;` |
|       - |  7985 | `	}` |
|   90331 |  7986 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   90331 |  7987 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  179793 |  7988 | `	for(i = 0; i < nIface; i++){` |
|   89467 |  7989 | `		ph7_class *pIface = apIface[i];` |
|       - |  7990 | `		SyHashEntry *pEntry;` |
|   89467 |  7991 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  238641 |  7992 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  149179 |  7993 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7994 | `			ph7_class_method *pImplMeth;` |
|  149179 |  7995 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7996 | `			/* Find the implementing method in the class */` |
|  149179 |  7997 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  149179 |  7998 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  7999 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8000 | `			}` |
|       - |  8001 | `			/* Check visibility: interface methods must be implemented as public */` |
|  149165 |  8002 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8003 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8004 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8005 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8006 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8007 | `					return SXERR_ABORT;` |
|       - |  8008 | `				}` |
|       1 |  8009 | `			}` |
|       - |  8010 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8011 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8012 | `			 */` |
|       - |  8013 | `			{` |
|  149165 |  8014 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  149165 |  8015 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  149165 |  8016 | `				int sigError = 0;` |
|  149165 |  8017 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8018 | `					sigError = 1;` |
|  149164 |  8019 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8020 | `					/* Extra parameters must all have default values */` |
|       6 |  8021 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8022 | `					sxu32 k;` |
|       8 |  8023 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8024 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8025 | `							sigError = 1;` |
|       3 |  8026 | `							break;` |
|       - |  8027 | `						}` |
|       2 |  8028 | `					}` |
|       2 |  8029 | `				}` |
|  149165 |  8030 | `				if( sigError ){` |
|       - |  8031 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8032 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8033 | `					sxu32 j;` |
|       6 |  8034 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8035 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8036 | `					/* Build implementing method signature */` |
|       6 |  8037 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8038 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8039 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8040 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8041 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8042 | `					}` |
|       - |  8043 | `					/* Build interface method signature */` |
|       6 |  8044 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8045 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8046 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8047 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8048 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8049 | `					}` |
|       8 |  8050 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8051 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8052 | `						&pClass->sName,pMName,` |
|       4 |  8053 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8054 | `						&pIface->sName,pMName,` |
|       4 |  8055 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8056 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8057 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8058 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8059 | `						return SXERR_ABORT;` |
|       - |  8060 | `					}` |
|       2 |  8061 | `				}` |
|       - |  8062 | `			}` |
|       5 |  8063 | `		}` |
|   44736 |  8064 | `	}` |
|   90331 |  8065 | `	return SXRET_OK;` |
|   45168 |  8066 |  |
|       - |  8067 | `/*` |
|       - |  8068 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8069 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8070 | ` */` |
|   90326 |  8071 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8072 |  |
|       - |  8073 | `	ph7_class_method *pMeth;` |
|       - |  8074 | `	SyHashEntry *pEntry;` |
|       - |  8075 | `	sxu32 nAbstract;` |
|       - |  8076 | `	SyBlob sMsg;` |
|       - |  8077 | `	sxi32 rc;` |
|       - |  8078 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   90331 |  8079 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      29 |  8080 | `		return SXRET_OK;` |
|       - |  8081 | `	}` |
|       - |  8082 | `	/* Count abstract methods */` |
|   90307 |  8083 | `	nAbstract = 0;` |
|   90307 |  8084 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  875293 |  8085 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  784991 |  8086 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  784991 |  8087 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8088 | `			nAbstract++;` |
|       8 |  8089 | `		}` |
|       5 |  8090 | `	}` |
|   90307 |  8091 | `	if( nAbstract == 0 ){` |
|   90293 |  8092 | `		return SXRET_OK;` |
|       - |  8093 | `	}` |
|       - |  8094 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8095 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8096 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8097 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8098 | `		&pClass->sName,nAbstract,` |
|       7 |  8099 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8100 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8101 | `	/* Second pass: list methods with origins */` |
|       - |  8102 | `	{` |
|      18 |  8103 | `		sxu32 nListed = 0;` |
|      18 |  8104 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8105 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8106 | `			ph7_class *pOrigin = 0;` |
|       - |  8107 | `			SyString *pMName;` |
|      22 |  8108 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8109 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8110 | `				continue;` |
|       - |  8111 | `			}` |
|      20 |  8112 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8113 | `			if( nListed > 0 ){` |
|       3 |  8114 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8115 | `			}` |
|       - |  8116 | `			/* Find the origin of this abstract method.` |
|       - |  8117 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8118 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8119 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8120 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8121 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8122 | `			 * class's namespace.` |
|       - |  8123 | `			 */` |
|       - |  8124 | `			{` |
|       - |  8125 | `				ph7_class **apIface;` |
|       - |  8126 | `				ph7_class **apTrait;` |
|       - |  8127 | `				ph7_class *pWalk;` |
|       - |  8128 | `				sxu32 i;` |
|       - |  8129 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8130 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8131 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8132 | `				 */` |
|      20 |  8133 | `				if( pClass->pBase ){` |
|      11 |  8134 | `					pWalk = pClass->pBase;` |
|      19 |  8135 | `					while( pWalk ){` |
|       - |  8136 | `						ph7_class_method *pParentMeth;` |
|      13 |  8137 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8138 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8139 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8140 | `							 * in this class's ancestor chain.` |
|       - |  8141 | `							 */` |
|      13 |  8142 | `							int fromIface = 0;` |
|      13 |  8143 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8144 | `							while( pAnc ){` |
|       - |  8145 | `								ph7_class **apPI;` |
|       - |  8146 | `								sxu32 j;` |
|      15 |  8147 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8148 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8149 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8150 | `										fromIface = 1;` |
|      10 |  8151 | `										break;` |
|       - |  8152 | `									}` |
|     ! 0 |  8153 | `								}` |
|      15 |  8154 | `								if( fromIface ) break;` |
|       6 |  8155 | `								pAnc = pAnc->pBase;` |
|       2 |  8156 | `							}` |
|      13 |  8157 | `							if( !fromIface ){` |
|       3 |  8158 | `								pOrigin = pWalk;` |
|       3 |  8159 | `								break;` |
|       - |  8160 | `							}` |
|       4 |  8161 | `						}` |
|      10 |  8162 | `						pWalk = pWalk->pBase;` |
|       2 |  8163 | `					}` |
|       4 |  8164 | `				}` |
|       - |  8165 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8166 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8167 | `				 */` |
|      20 |  8168 | `				if( !pOrigin ){` |
|      18 |  8169 | `					pWalk = pClass;` |
|      40 |  8170 | `					while( pWalk && !pOrigin ){` |
|      26 |  8171 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8172 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8173 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8174 | `							ph7_class *pDeepest = 0;` |
|      28 |  8175 | `							while( pIface ){` |
|      16 |  8176 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8177 | `									pDeepest = pIface;` |
|       6 |  8178 | `								}` |
|      16 |  8179 | `								pIface = pIface->pBase;` |
|       4 |  8180 | `							}` |
|      16 |  8181 | `							if( pDeepest ){` |
|      16 |  8182 | `								pOrigin = pDeepest;` |
|      16 |  8183 | `								break;` |
|       - |  8184 | `							}` |
|     ! 0 |  8185 | `						}` |
|      26 |  8186 | `						pWalk = pWalk->pBase;` |
|       4 |  8187 | `					}` |
|       7 |  8188 | `				}` |
|       - |  8189 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8190 | `				if( !pOrigin ){` |
|       3 |  8191 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8192 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8193 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8194 | `							pOrigin = pClass;` |
|       3 |  8195 | `							break;` |
|       - |  8196 | `						}` |
|     ! 0 |  8197 | `					}` |
|       1 |  8198 | `				}` |
|       - |  8199 | `			}` |
|      20 |  8200 | `			if( pOrigin ){` |
|      20 |  8201 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8202 | `			}else{` |
|       - |  8203 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8204 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8205 | `			}` |
|      20 |  8206 | `			nListed++;` |
|       4 |  8207 | `		}` |
|       - |  8208 | `	}` |
|      18 |  8209 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8210 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8211 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8212 | `	SyBlobRelease(&sMsg);` |
|      18 |  8213 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8214 | `		return SXERR_ABORT;` |
|       - |  8215 | `	}` |
|      18 |  8216 | `	return SXRET_OK;` |
|   45168 |  8217 |  |
|       - |  8218 | `/*` |
|       - |  8219 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8220 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8221 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8222 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8223 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8224 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8225 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8226 | ` */` |
|   90004 |  8227 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8228 |  |
|   90009 |  8229 | `	int isAbsolute = 0;` |
|   90009 |  8230 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8231 | `	SyBlob sName;` |
|   90009 |  8232 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      51 |  8233 | `		isAbsolute = 1;` |
|      51 |  8234 | `		pGen->pIn++;` |
|      24 |  8235 | `	}` |
|   90009 |  8236 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       9 |  8237 | `		pGen->pIn = pStart;` |
|       9 |  8238 | `		return SXERR_INVALID;` |
|       - |  8239 | `	}` |
|   90003 |  8240 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   90003 |  8241 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   90003 |  8242 | `	pGen->pIn++;` |
|  135015 |  8243 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   45022 |  8244 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8245 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8246 | `		pGen->pIn++;` |
|      13 |  8247 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8248 | `		pGen->pIn++;` |
|       1 |  8249 | `	}` |
|   90003 |  8250 | `	if( isAbsolute ){` |
|      49 |  8251 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      26 |  8252 | `	}else{` |
|       - |  8253 | `		SyString sRaw;` |
|   89957 |  8254 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   89957 |  8255 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8256 | `	}` |
|   90003 |  8257 | `	SyBlobRelease(&sName);` |
|   90003 |  8258 | `	return SXRET_OK;` |
|   45007 |  8259 |  |
|       - |  8260 | `/*` |
|       - |  8261 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8262 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8263 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8264 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8265 | ` * either direction cannot run unbounded.` |
|       - |  8266 | ` */` |
|       - |  8267 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   10044 |  8268 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8269 |  |
|       - |  8270 | `	ph7_class **apParent;` |
|       - |  8271 | `	sxu32 n;` |
|   16809 |  8272 | `	while( pInterface ){` |
|   13387 |  8273 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8274 | `			return FALSE;` |
|       - |  8275 | `		}` |
|   16705 |  8276 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    6636 |  8277 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    6627 |  8278 | `			return TRUE;` |
|       - |  8279 | `		}` |
|    6765 |  8280 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    6765 |  8281 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8282 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8283 | `				return TRUE;` |
|       - |  8284 | `			}` |
|     ! 0 |  8285 | `		}` |
|    6765 |  8286 | `		pInterface = pInterface->pBase;` |
|    6765 |  8287 | `		iDepth++;` |
|       5 |  8288 | `	}` |
|    3427 |  8289 | `	return FALSE;` |
|    5027 |  8290 |  |
|   10044 |  8291 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8292 |  |
|   10049 |  8293 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8294 |  |
|       - |  8295 | `/*` |
|       - |  8296 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8297 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8298 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8299 | ` */` |
|    6622 |  8300 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8301 |  |
|    6631 |  8302 | `	while( pBase ){` |
|      10 |  8303 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8304 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8305 | `			return TRUE;` |
|       - |  8306 | `		}` |
|      10 |  8307 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8308 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8309 | `			return TRUE;` |
|       - |  8310 | `		}` |
|       5 |  8311 | `		pBase = pBase->pBase;` |
|       1 |  8312 | `	}` |
|    6623 |  8313 | `	return FALSE;` |
|    3316 |  8314 |  |
|       - |  8315 | `/*` |
|       - |  8316 | ` * Compile a class declaration, named or anonymous.` |
|       - |  8317 | ` *` |
|       - |  8318 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  8319 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  8320 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  8321 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  8322 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  8323 | ` * implements, body, install) is shared by both paths.` |
|       - |  8324 | ` */` |
|   90356 |  8325 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  8326 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  8327 |  |
|   90361 |  8328 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8329 | `	ph7_class *pClass,*pBase;` |
|       - |  8330 | `	SyToken *pEnd,*pTmp;` |
|       - |  8331 | `	sxi32 iProtection;` |
|       - |  8332 | `	SySet aInterfaces;` |
|       - |  8333 | `	SySet aUseEntries;` |
|       - |  8334 | `	sxi32 iAttrflags;` |
|       - |  8335 | `	SyString *pName;` |
|       - |  8336 | `	sxi32 nKwrd;` |
|       - |  8337 | `	sxi32 rc;` |
|       - |  8338 | `	/* Jump the 'class' keyword */` |
|   90361 |  8339 | `	pGen->pIn++;` |
|   90361 |  8340 | `	if( pAnonName ){` |
|       - |  8341 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  8342 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  8343 | `		 * then use the synthesized name. */` |
|      21 |  8344 | `		*ppArgStart = *ppArgEnd = 0;` |
|      21 |  8345 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  8346 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  8347 | `			*ppArgStart = pGen->pIn;` |
|      10 |  8348 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  8349 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  8350 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  8351 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  8352 | `		}` |
|      21 |  8353 | `		pName = pAnonName;` |
|      21 |  8354 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      11 |  8355 | `	}else{` |
|   90341 |  8356 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8357 | `			/* Syntax error */` |
|     ! 0 |  8358 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8359 | `			if( rc == SXERR_ABORT ){` |
|       - |  8360 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8361 | `				return SXERR_ABORT;` |
|       - |  8362 | `			}` |
|       - |  8363 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8364 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8365 | `				pGen->pIn++;` |
|     ! 0 |  8366 | `			}` |
|     ! 0 |  8367 | `			return SXRET_OK;` |
|       - |  8368 | `		}` |
|       - |  8369 | `		/* Extract class name */` |
|   90341 |  8370 | `		pName = &pGen->pIn->sData;` |
|       - |  8371 | `		/* Advance the stream cursor */` |
|   90341 |  8372 | `		pGen->pIn++;` |
|       - |  8373 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  8374 | `			SyBlob sFQN;` |
|       - |  8375 | `			SyString sFQNStr;` |
|   90341 |  8376 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   90341 |  8377 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   90341 |  8378 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   90341 |  8379 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   90341 |  8380 | `			SyBlobRelease(&sFQN);` |
|       - |  8381 | `		}` |
|       - |  8382 | `	}` |
|   90361 |  8383 | `	if( pClass == 0 ){` |
|     ! 0 |  8384 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8385 | `		return SXERR_ABORT;` |
|       - |  8386 | `	}` |
|       - |  8387 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   90361 |  8388 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   90361 |  8389 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8390 | `	/* Assume a standalone class */` |
|   90361 |  8391 | `	pBase = 0;` |
|   90361 |  8392 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   79623 |  8393 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   79623 |  8394 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8395 | `			SyBlob sResolved;` |
|       - |  8396 | `			SyString sBaseName;` |
|       - |  8397 | `			sxu32 nRefLine;` |
|   69587 |  8398 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   69587 |  8399 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   69587 |  8400 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   69587 |  8401 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8402 | `				SyBlobRelease(&sResolved);` |
|       4 |  8403 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8404 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8405 | `					pName);` |
|       3 |  8406 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8407 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8408 | `					return SXERR_ABORT;` |
|       - |  8409 | `				}` |
|       3 |  8410 | `				return SXRET_OK;` |
|       - |  8411 | `			}` |
|  104375 |  8412 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   69580 |  8413 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   69585 |  8414 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8415 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8416 | `			/* Interfaces are not allowed */` |
|   69585 |  8417 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8418 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8419 | `			}` |
|   69585 |  8420 | `			if( pBase == 0 ){` |
|     ! 0 |  8421 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8422 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8423 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8424 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8425 | `					return SXERR_ABORT;` |
|       - |  8426 | `				}` |
|     ! 0 |  8427 | `			}else{` |
|   69585 |  8428 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8429 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8430 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8431 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8432 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8433 | `						return SXERR_ABORT;` |
|       - |  8434 | `					}` |
|     ! 0 |  8435 | `				}` |
|       - |  8436 | `			}` |
|   69585 |  8437 | `			SyBlobRelease(&sResolved);` |
|   34790 |  8438 | `		}` |
|   79621 |  8439 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8440 | `			ph7_class *pInterface;` |
|       - |  8441 | `			/* Interface implementation */` |
|   10049 |  8442 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5022 |  8443 | `			for(;;){` |
|       - |  8444 | `				SyBlob sResolved;` |
|       - |  8445 | `				SyString sIntName;` |
|       - |  8446 | `				sxu32 nRefLine;` |
|   10049 |  8447 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10049 |  8448 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10049 |  8449 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8450 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8451 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8452 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8453 | `						pName);` |
|     ! 0 |  8454 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8455 | `						return SXERR_ABORT;` |
|       - |  8456 | `					}` |
|     ! 0 |  8457 | `					break;` |
|       - |  8458 | `				}` |
|   20093 |  8459 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   10044 |  8460 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10049 |  8461 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8462 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8463 | `				/* Only interfaces are allowed */` |
|   10049 |  8464 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8465 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8466 | `				}` |
|   10049 |  8467 | `				if( pInterface == 0 ){` |
|     ! 0 |  8468 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8469 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8470 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8471 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8472 | `						return SXERR_ABORT;` |
|       - |  8473 | `					}` |
|     ! 0 |  8474 | `				}else{` |
|       - |  8475 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8476 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8477 | `					 * unless they already extend Exception or Error.` |
|       - |  8478 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8479 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8480 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   10049 |  8481 | `					SyString *pFqn = &pClass->sName;` |
|   10049 |  8482 | `					int bIsExceptionOrError =` |
|    8330 |  8483 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   16721 |  8484 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    8396 |  8485 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3316 |  8486 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   13355 |  8487 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    9936 |  8488 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3309 |  8489 | `						!bIsExceptionOrError ){` |
|      12 |  8490 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8491 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8492 | `							&pClass->sName);` |
|       9 |  8493 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8494 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8495 | `							return SXERR_ABORT;` |
|       - |  8496 | `						}` |
|       - |  8497 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8498 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8499 | `					}else{` |
|   10043 |  8500 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8501 | `					}` |
|       - |  8502 | `				}` |
|   10049 |  8503 | `				SyBlobRelease(&sResolved);` |
|   10049 |  8504 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5027 |  8505 | `					break;` |
|       - |  8506 | `				}` |
|     ! 0 |  8507 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  8508 | `			}` |
|    5022 |  8509 | `		}` |
|   39808 |  8510 | `	}` |
|   90359 |  8511 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8512 | `		/* Syntax error */` |
|     ! 0 |  8513 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8514 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8515 | `		if( rc == SXERR_ABORT ){` |
|       - |  8516 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8517 | `			return SXERR_ABORT;` |
|       - |  8518 | `		}` |
|     ! 0 |  8519 | `		return SXRET_OK;` |
|       - |  8520 | `	}` |
|   90359 |  8521 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   90359 |  8522 | `	pEnd = 0; /* cc warning */` |
|       - |  8523 | `	/* Delimit the class body */` |
|   90359 |  8524 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   90359 |  8525 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8526 | `		/* Syntax error */` |
|     ! 0 |  8527 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8528 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8529 | `		if( rc == SXERR_ABORT ){` |
|       - |  8530 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8531 | `			return SXERR_ABORT;` |
|       - |  8532 | `		}` |
|     ! 0 |  8533 | `		return SXRET_OK;` |
|       - |  8534 | `	}` |
|       - |  8535 | `	/* Swap token stream */` |
|   90359 |  8536 | `	pTmp = pGen->pEnd;` |
|   90359 |  8537 | `	pGen->pEnd = pEnd;` |
|       - |  8538 | `	/* Set the inherited flags */` |
|   90359 |  8539 | `	pClass->iFlags = iFlags;` |
|       - |  8540 | `	/* Start the parse process */` |
|  126627 |  8541 | `	for(;;){` |
|       - |  8542 | `		/* Jump leading/trailing semi-colons */` |
|  380209 |  8543 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   63513 |  8544 | `			pGen->pIn++;` |
|       5 |  8545 | `		}` |
|  316701 |  8546 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8547 | `			/* End of class body */` |
|   90331 |  8548 | `			break;` |
|       - |  8549 | `		}` |
|  226370 |  8550 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  113190 |  8551 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  8552 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8553 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8554 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8555 | `			if( rc == SXERR_ABORT ){` |
|       - |  8556 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8557 | `				return SXERR_ABORT;` |
|       - |  8558 | `			}` |
|     ! 0 |  8559 | `			goto done;` |
|       - |  8560 | `		}` |
|       - |  8561 | `		/* Assume public visibility */` |
|  226375 |  8562 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  226375 |  8563 | `		iAttrflags = 0;` |
|       - |  8564 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  8565 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  8566 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  8567 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  226375 |  8568 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8569 | `			int bMod = 0;` |
|     ! 0 |  8570 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8571 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  8572 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  8573 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  8574 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  8575 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  8576 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  8577 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  8578 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  8579 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  8580 | `			}` |
|     ! 0 |  8581 | `			if( !bMod ){` |
|     ! 0 |  8582 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8583 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8584 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8585 | `						return SXERR_ABORT;` |
|       - |  8586 | `					}` |
|     ! 0 |  8587 | `					goto done;` |
|       - |  8588 | `				}` |
|     ! 0 |  8589 | `				continue;` |
|       - |  8590 | `			}` |
|     ! 0 |  8591 | `		}` |
|  226375 |  8592 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8593 | `			/* Extract the current keyword */` |
|  226375 |  8594 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  226375 |  8595 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8596 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8597 | `				TraitUseEntry sUse;` |
|      49 |  8598 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      49 |  8599 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      49 |  8600 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      30 |  8601 | `				for(;;){` |
|       - |  8602 | `					ph7_class *pTrait;` |
|       - |  8603 | `					SyString *pTraitName;` |
|      57 |  8604 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8605 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8606 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8607 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8608 | `							return SXERR_ABORT;` |
|       - |  8609 | `						}` |
|     ! 0 |  8610 | `						break;` |
|       - |  8611 | `					}` |
|      57 |  8612 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8613 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8614 | `						SyBlob sResolved;` |
|      57 |  8615 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      57 |  8616 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     109 |  8617 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      52 |  8618 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      57 |  8619 | `						SyBlobRelease(&sResolved);` |
|       - |  8620 | `					}` |
|       - |  8621 | `					/* Only traits are allowed */` |
|      57 |  8622 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8623 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8624 | `					}` |
|      57 |  8625 | `					if( pTrait == 0 ){` |
|     ! 0 |  8626 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8627 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8628 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8629 | `							return SXERR_ABORT;` |
|       - |  8630 | `						}` |
|     ! 0 |  8631 | `					}else{` |
|      57 |  8632 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8633 | `					}` |
|      57 |  8634 | `					pGen->pIn++; /* Advance past trait name */` |
|      57 |  8635 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      27 |  8636 | `						break;` |
|       - |  8637 | `					}` |
|      10 |  8638 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  8639 | `				}` |
|       - |  8640 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      49 |  8641 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8642 | `					SyToken *pBlock;` |
|      10 |  8643 | `					pGen->pIn++; /* Jump '{' */` |
|      10 |  8644 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      10 |  8645 | `					sUse.pResolvStart = pGen->pIn;` |
|      10 |  8646 | `					sUse.pResolvEnd = pBlock;` |
|      10 |  8647 | `					if( pBlock < pGen->pEnd ){` |
|      10 |  8648 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       6 |  8649 | `					}else{` |
|     ! 0 |  8650 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8651 | `					}` |
|       4 |  8652 | `				}` |
|      49 |  8653 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8654 | `				/* The semicolon will be consumed by the outer loop */` |
|      49 |  8655 | `				continue;` |
|       - |  8656 | `			}` |
|  226331 |  8657 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  222829 |  8658 | `				iProtection = nKwrd;` |
|  222829 |  8659 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  8660 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  222829 |  8661 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  8662 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  8663 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  8664 | `				}` |
|  222824 |  8665 | `				if( pGen->pIn >= pGen->pEnd` |
|  222829 |  8666 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8667 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8668 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8669 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8670 | `					if( rc == SXERR_ABORT ){` |
|       - |  8671 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8672 | `						return SXERR_ABORT;` |
|       - |  8673 | `					}` |
|     ! 0 |  8674 | `					goto done;` |
|       - |  8675 | `				}` |
|  222829 |  8676 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8677 | `					/* Attribute declaration (untyped) */` |
|   63237 |  8678 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   63237 |  8679 | `					if( rc != SXRET_OK ){` |
|       9 |  8680 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8681 | `							return SXERR_ABORT;` |
|       - |  8682 | `						}` |
|       9 |  8683 | `						goto done;` |
|       - |  8684 | `					}` |
|   63231 |  8685 | `					continue;` |
|       - |  8686 | `				}` |
|  159597 |  8687 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8688 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     159 |  8689 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     159 |  8690 | `					if( rc != SXRET_OK ){` |
|       8 |  8691 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8692 | `							return SXERR_ABORT;` |
|       - |  8693 | `						}` |
|       8 |  8694 | `						goto done;` |
|       - |  8695 | `					}` |
|     153 |  8696 | `					continue;` |
|       - |  8697 | `				}` |
|       - |  8698 | `				/* Extract the keyword */` |
|  159443 |  8699 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   79719 |  8700 | `			}` |
|  162945 |  8701 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8702 | `				/* Process constant declaration */` |
|      65 |  8703 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      65 |  8704 | `				if( rc != SXRET_OK ){` |
|       3 |  8705 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8706 | `						return SXERR_ABORT;` |
|       - |  8707 | `					}` |
|       3 |  8708 | `					goto done;` |
|       - |  8709 | `				}` |
|      34 |  8710 | `			}else{` |
|  162885 |  8711 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8712 | `					/* Static method or attribute,record that */` |
|    3355 |  8713 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    3355 |  8714 | `					pGen->pIn++; /* Jump the static keyword */` |
|    3355 |  8715 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8716 | `						/* Extract the keyword */` |
|    3349 |  8717 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    3349 |  8718 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8719 | `							iProtection = nKwrd;` |
|     ! 0 |  8720 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8721 | `						}` |
|    1672 |  8722 | `					}` |
|       - |  8723 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  8724 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  8725 | `					 * than a generic "expecting method" parse error. */` |
|    3355 |  8726 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8727 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8728 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  8729 | `					}` |
|    3350 |  8730 | `					if( pGen->pIn >= pGen->pEnd` |
|    3355 |  8731 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8732 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8733 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8734 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8735 | `						if( rc == SXERR_ABORT ){` |
|       - |  8736 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8737 | `							return SXERR_ABORT;` |
|       - |  8738 | `						}` |
|     ! 0 |  8739 | `						goto done;` |
|       - |  8740 | `					}` |
|    3355 |  8741 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8742 | `						/* Attribute declaration */` |
|       5 |  8743 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8744 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8745 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8746 | `								return SXERR_ABORT;` |
|       - |  8747 | `							}` |
|     ! 0 |  8748 | `							goto done;` |
|       - |  8749 | `						}` |
|       5 |  8750 | `						continue;` |
|       - |  8751 | `					}` |
|    3351 |  8752 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8753 | `						/* Typed static attribute declaration */` |
|      15 |  8754 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  8755 | `						if( rc != SXRET_OK ){` |
|       3 |  8756 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8757 | `								return SXERR_ABORT;` |
|       - |  8758 | `							}` |
|       3 |  8759 | `							goto done;` |
|       - |  8760 | `						}` |
|      13 |  8761 | `						continue;` |
|       - |  8762 | `					}` |
|       - |  8763 | `					/* Extract the keyword */` |
|    3339 |  8764 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  161202 |  8765 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8766 | `					/* Abstract method,record that */` |
|      12 |  8767 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8768 | `					/* Mark the whole class as abstract */` |
|      12 |  8769 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8770 | `					/* Advance the stream cursor */` |
|      12 |  8771 | `					pGen->pIn++;` |
|      12 |  8772 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8773 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8774 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8775 | `							iProtection = nKwrd;` |
|      10 |  8776 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8777 | `						}` |
|       5 |  8778 | `					}` |
|      12 |  8779 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8780 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8781 | `							/* Static method */` |
|     ! 0 |  8782 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8783 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8784 | `					}` |
|      12 |  8785 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8786 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8787 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8788 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8789 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8790 | `							if( rc == SXERR_ABORT ){` |
|       - |  8791 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8792 | `								return SXERR_ABORT;` |
|       - |  8793 | `							}` |
|     ! 0 |  8794 | `							goto done;` |
|       - |  8795 | `					}` |
|      12 |  8796 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  159530 |  8797 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8798 | `					/* final method ,record that */` |
|      17 |  8799 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  8800 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  8801 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8802 | `						/* Extract the keyword */` |
|      17 |  8803 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  8804 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  8805 | `							iProtection = nKwrd;` |
|       9 |  8806 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  8807 | `						}` |
|       7 |  8808 | `					}` |
|      17 |  8809 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  8810 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  8811 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  8812 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  8813 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  8814 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  8815 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  8816 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  8817 | `									return SXERR_ABORT;` |
|       - |  8818 | `								}` |
|     ! 0 |  8819 | `								goto done;` |
|       - |  8820 | `							}` |
|      12 |  8821 | `							continue;` |
|       - |  8822 | `					}` |
|       6 |  8823 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8824 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8825 | `							/* Static method */` |
|     ! 0 |  8826 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8827 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8828 | `					}` |
|       6 |  8829 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8830 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8831 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8832 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8833 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8834 | `							if( rc == SXERR_ABORT ){` |
|       - |  8835 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8836 | `								return SXERR_ABORT;` |
|       - |  8837 | `							}` |
|     ! 0 |  8838 | `							goto done;` |
|       - |  8839 | `					}` |
|       6 |  8840 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8841 | `				}` |
|  162859 |  8842 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8843 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8844 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8845 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8846 | `						if( rc == SXERR_ABORT ){` |
|       - |  8847 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8848 | `							return SXERR_ABORT;` |
|       - |  8849 | `						}` |
|     ! 0 |  8850 | `						goto done;` |
|       - |  8851 | `				}` |
|  162859 |  8852 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8853 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8854 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8855 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8856 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8857 | `						if( rc == SXERR_ABORT ){` |
|       - |  8858 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8859 | `							return SXERR_ABORT;` |
|       - |  8860 | `						}` |
|     ! 0 |  8861 | `						goto done;` |
|       - |  8862 | `					}` |
|       - |  8863 | `					/* Attribute declaration */` |
|       7 |  8864 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8865 | `				}else{` |
|       - |  8866 | `					/* Process method declaration */` |
|  162853 |  8867 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8868 | `				}` |
|  162859 |  8869 | `				if( rc != SXRET_OK ){` |
|      16 |  8870 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8871 | `						return SXERR_ABORT;` |
|       - |  8872 | `					}` |
|      16 |  8873 | `					goto done;` |
|       - |  8874 | `				}` |
|       - |  8875 | `			}` |
|   81455 |  8876 | `		}else{` |
|       - |  8877 | `			/* Attribute declaration */` |
|     ! 0 |  8878 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8879 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8880 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8881 | `					return SXERR_ABORT;` |
|       - |  8882 | `				}` |
|     ! 0 |  8883 | `				goto done;` |
|       - |  8884 | `			}` |
|       - |  8885 | `		}` |
|       5 |  8886 | `	}` |
|       - |  8887 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8888 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8889 | `	 */` |
|       - |  8890 | `	{` |
|       - |  8891 | `		TraitUseEntry *apUse;` |
|       - |  8892 | `		sxu32 nU;` |
|   90331 |  8893 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   90375 |  8894 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      49 |  8895 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      49 |  8896 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      49 |  8897 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      49 |  8898 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8899 | `			sxu32 nT;` |
|      49 |  8900 | `			if( !hasResolution ){` |
|       - |  8901 | `				/* No conflict resolution block: use standard trait application */` |
|      83 |  8902 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      47 |  8903 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      47 |  8904 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8905 | `						break;` |
|       - |  8906 | `					}` |
|      26 |  8907 | `				}` |
|      23 |  8908 | `			}else{` |
|       - |  8909 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8910 | `				 * then use the block to resolve method conflicts.` |
|       - |  8911 | `				 */` |
|       - |  8912 | `				SyToken *pR;` |
|      20 |  8913 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      12 |  8914 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8915 | `					ph7_class_attr *pAR;` |
|       - |  8916 | `					SyHashEntry *pER;` |
|       - |  8917 | `					SyString *pNR;` |
|      12 |  8918 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      17 |  8919 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8920 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8921 | `						pNR = &pAR->sName;` |
|     ! 0 |  8922 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8923 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8924 | `						}` |
|     ! 0 |  8925 | `					}` |
|      12 |  8926 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       7 |  8927 | `				}` |
|       - |  8928 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      10 |  8929 | `				pR = pUse->pResolvStart;` |
|      22 |  8930 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8931 | `					SyString sTrait,sMethod;` |
|       - |  8932 | `					ph7_class *pSrcTrait;` |
|       - |  8933 | `					ph7_class_method *pMeth;` |
|       - |  8934 | `					sxi32 nRKwrd;` |
|      34 |  8935 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      22 |  8936 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      14 |  8937 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      14 |  8938 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      14 |  8939 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      14 |  8940 | `					sMethod = pR->sData;` |
|      14 |  8941 | `					pR++;` |
|      14 |  8942 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8943 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8944 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8945 | `							sTrait = sMethod;` |
|       7 |  8946 | `							pR++;` |
|       7 |  8947 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8948 | `							sMethod = pR->sData;` |
|       7 |  8949 | `							pR++;` |
|       3 |  8950 | `						}` |
|       3 |  8951 | `					}` |
|      14 |  8952 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8953 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8954 | `						continue;` |
|       - |  8955 | `					}` |
|      14 |  8956 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      14 |  8957 | `					pR++;` |
|      14 |  8958 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8959 | `						pSrcTrait = 0;` |
|       7 |  8960 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8961 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8962 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8963 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8964 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8965 | `								break;` |
|       - |  8966 | `							}` |
|       2 |  8967 | `						}` |
|       5 |  8968 | `						if( pSrcTrait ){` |
|       5 |  8969 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8970 | `							if( pMeth ){` |
|       5 |  8971 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8972 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8973 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8974 | `								}` |
|       2 |  8975 | `							}` |
|       2 |  8976 | `						}` |
|       2 |  8977 | `					}` |
|      30 |  8978 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       2 |  8979 | `				}` |
|       - |  8980 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      20 |  8981 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8982 | `					ph7_class_method *pMR;` |
|       - |  8983 | `					SyHashEntry *pER;` |
|       - |  8984 | `					SyString *pNR;` |
|      12 |  8985 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      35 |  8986 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      20 |  8987 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      20 |  8988 | `						pNR = &pMR->sFunc.sName;` |
|      20 |  8989 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8990 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8991 | `						}` |
|       2 |  8992 | `					}` |
|       7 |  8993 | `				}` |
|       - |  8994 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      10 |  8995 | `				pR = pUse->pResolvStart;` |
|      22 |  8996 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8997 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8998 | `					ph7_class *pSrcTrait;` |
|       - |  8999 | `					ph7_class_method *pMeth;` |
|      22 |  9000 | `					int hasQual = 0;` |
|       - |  9001 | `					sxi32 nRKwrd;` |
|      34 |  9002 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      22 |  9003 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      14 |  9004 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      14 |  9005 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      14 |  9006 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      14 |  9007 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      14 |  9008 | `					sMethod = pR->sData;` |
|      14 |  9009 | `					pR++;` |
|      14 |  9010 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9011 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9012 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9013 | `							sTrait = sMethod;` |
|       7 |  9014 | `							hasQual = 1;` |
|       7 |  9015 | `							pR++;` |
|       7 |  9016 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9017 | `							sMethod = pR->sData;` |
|       7 |  9018 | `							pR++;` |
|       3 |  9019 | `						}` |
|       3 |  9020 | `					}` |
|      14 |  9021 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9022 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9023 | `						continue;` |
|       - |  9024 | `					}` |
|      14 |  9025 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      14 |  9026 | `					pR++;` |
|      14 |  9027 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      10 |  9028 | `						sxi32 iNewVis = -1;` |
|      10 |  9029 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9030 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9031 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9032 | `								iNewVis = nAK;` |
|       7 |  9033 | `								pR++;` |
|       3 |  9034 | `							}` |
|       3 |  9035 | `						}` |
|      10 |  9036 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       8 |  9037 | `							sAlias = pR->sData;` |
|       8 |  9038 | `							pR++;` |
|       3 |  9039 | `						}` |
|      10 |  9040 | `						pMeth = 0;` |
|      10 |  9041 | `						if( hasQual ){` |
|       3 |  9042 | `							pSrcTrait = 0;` |
|       5 |  9043 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9044 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9045 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9046 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9047 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9048 | `									break;` |
|       - |  9049 | `								}` |
|       2 |  9050 | `							}` |
|       3 |  9051 | `							if( pSrcTrait ){` |
|       3 |  9052 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9053 | `							}` |
|       2 |  9054 | `						}else{` |
|       7 |  9055 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9056 | `						}` |
|      10 |  9057 | `						if( pMeth ){` |
|      10 |  9058 | `							if( sAlias.nByte > 0 ){` |
|       - |  9059 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9060 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9061 | `								 */` |
|       - |  9062 | `								ph7_class_method *pAlias;` |
|       - |  9063 | `								char *zAliasDup;` |
|       8 |  9064 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       8 |  9065 | `								if( pAlias ){` |
|       8 |  9066 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       8 |  9067 | `									if( iNewVis >= 0 ){` |
|       5 |  9068 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9069 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9070 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9071 | `									}` |
|       8 |  9072 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       8 |  9073 | `									if( zAliasDup ){` |
|       8 |  9074 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  9075 | `									}` |
|       5 |  9076 | `								}` |
|       6 |  9077 | `							}else if( iNewVis >= 0 ){` |
|       - |  9078 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9079 | `								ph7_class_method *pCopy;` |
|       3 |  9080 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9081 | `								if( pCopy ){` |
|       3 |  9082 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9083 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9084 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9085 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9086 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9087 | `									/* Replace the method in the class hash */` |
|       3 |  9088 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9089 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9090 | `								}` |
|       1 |  9091 | `							}` |
|       4 |  9092 | `						}` |
|       4 |  9093 | `						SXUNUSED(hasQual);` |
|       4 |  9094 | `					}` |
|      18 |  9095 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       2 |  9096 | `				}` |
|       - |  9097 | `			}` |
|      49 |  9098 | `			SySetRelease(&pUse->aTraits);` |
|      27 |  9099 | `		}` |
|       - |  9100 | `	}` |
|       - |  9101 | `	/* Install the class */` |
|   90331 |  9102 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   90331 |  9103 | `	if( rc == SXRET_OK ){` |
|       - |  9104 | `		ph7_class **apInterface;` |
|       - |  9105 | `		sxu32 n;` |
|   90331 |  9106 | `		if( pBase ){` |
|       - |  9107 | `			/* Inherit from base class and mark as a subclass */` |
|   69585 |  9108 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   34790 |  9109 | `		}` |
|   90331 |  9110 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  100369 |  9111 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9112 | `			/* Implements one or more interface */` |
|   10043 |  9113 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   10043 |  9114 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9115 | `				break;` |
|       - |  9116 | `			}` |
|    5024 |  9117 | `		}` |
|       - |  9118 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9119 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   90326 |  9120 | `		if( rc == SXRET_OK` |
|   90326 |  9121 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   90331 |  9122 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   79431 |  9123 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9124 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   79431 |  9125 | `			if( pStringable ){` |
|   79431 |  9126 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   79431 |  9127 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9128 | `				sxu32 i;` |
|   79431 |  9129 | `				int bAlready = 0;` |
|   86047 |  9130 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    6623 |  9131 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9132 | `						bAlready = 1;` |
|       3 |  9133 | `						break;` |
|       - |  9134 | `					}` |
|    3313 |  9135 | `				}` |
|   79431 |  9136 | `				if( !bAlready ){` |
|   79429 |  9137 | `					PH7_ClassImplement(pClass,pStringable);` |
|   39712 |  9138 | `				}` |
|   39713 |  9139 | `			}` |
|   39713 |  9140 | `		}` |
|       - |  9141 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   90331 |  9142 | `		if( rc == SXRET_OK ){` |
|   90331 |  9143 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   90331 |  9144 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9145 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9146 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9147 | `				return SXERR_ABORT;` |
|       - |  9148 | `			}` |
|   45163 |  9149 | `		}` |
|       - |  9150 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   90331 |  9151 | `		if( rc == SXRET_OK ){` |
|   90331 |  9152 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   90331 |  9153 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9154 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9155 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9156 | `				return SXERR_ABORT;` |
|       - |  9157 | `			}` |
|   45163 |  9158 | `		}` |
|   45163 |  9159 | `	}` |
|   90331 |  9160 | `	SySetRelease(&aUseEntries);` |
|   90331 |  9161 | `	SySetRelease(&aInterfaces);` |
|   90331 |  9162 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9163 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9164 | `		return SXERR_ABORT;` |
|       - |  9165 | `	}` |
|   45163 |  9166 | `done:` |
|       - |  9167 | `	/* Point beyond the class body */` |
|   90359 |  9168 | `	pGen->pIn = &pEnd[1];` |
|   90359 |  9169 | `	pGen->pEnd = pTmp;` |
|   90359 |  9170 | `	return PH7_OK;` |
|   45183 |  9171 |  |
|       - |  9172 | `/* Compile a named class declaration (the common case). */` |
|   90336 |  9173 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9174 |  |
|   90341 |  9175 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9176 |  |
|       - |  9177 | `/*` |
|       - |  9178 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9179 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9180 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9181 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9182 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9183 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9184 | ` */` |
|      20 |  9185 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  9186 |  |
|       - |  9187 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9188 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9189 | `	SyString sName;` |
|       - |  9190 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9191 | `	ph7_value *pObj;` |
|      21 |  9192 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9193 | `	sxu32 nIdx,nLen;` |
|       - |  9194 | `	sxi32 nArg,rc;` |
|      10 |  9195 | `	SXUNUSED(iCompileFlag);` |
|       - |  9196 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      21 |  9197 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      21 |  9198 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9199 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9200 | `	}` |
|      21 |  9201 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9202 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9203 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9204 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      21 |  9205 | `	pArgStart = pArgEnd = 0;` |
|      21 |  9206 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      21 |  9207 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9208 | `		return rc;` |
|       - |  9209 | `	}` |
|       - |  9210 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9211 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      21 |  9212 | `	nArg = 0;` |
|      21 |  9213 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9214 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9215 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9216 | `		SyToken *pArgNext;` |
|       7 |  9217 | `		pGen->pIn = pArgStart;` |
|       7 |  9218 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9219 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9220 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9221 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 |  9222 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9223 | `					pGen->pIn = pSavedIn;` |
|     ! 0 |  9224 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 |  9225 | `					return SXERR_ABORT;` |
|       - |  9226 | `				}` |
|       7 |  9227 | `				nArg++;` |
|       3 |  9228 | `			}` |
|       7 |  9229 | `			pGen->pIn = &pArgNext[1];` |
|       1 |  9230 | `		}` |
|       7 |  9231 | `		pGen->pIn = pSavedIn;` |
|       7 |  9232 | `		pGen->pEnd = pSavedEnd;` |
|       3 |  9233 | `	}` |
|       - |  9234 | `	/* Load the synthesized class name */` |
|      21 |  9235 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      21 |  9236 | `	if( pObj == 0 ){` |
|     ! 0 |  9237 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9238 | `		return SXERR_ABORT;` |
|       - |  9239 | `	}` |
|      21 |  9240 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      21 |  9241 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  9242 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      21 |  9243 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      21 |  9244 | `	return SXRET_OK;` |
|      11 |  9245 |  |
|       - |  9246 | `/*` |
|       - |  9247 | ` * Compile a user-defined abstract class.` |
|       - |  9248 | ` *  According to the PHP language reference manual` |
|       - |  9249 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9250 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9251 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9252 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9253 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9254 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9255 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9256 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9257 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9258 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9259 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9260 | ` *   could differ.` |
|       - |  9261 | ` */` |
|       - |  9262 | `/*` |
|       - |  9263 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9264 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9265 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9266 | ` */` |
|  891604 |  9267 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9268 |  |
|  891609 |  9269 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  590891 |  9270 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  590891 |  9271 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  590873 |  9272 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  295411 |  9273 | `	}` |
|  891545 |  9274 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  891485 |  9275 | `	return FALSE;` |
|  445807 |  9276 |  |
|       - |  9277 | `/*` |
|       - |  9278 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9279 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9280 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9281 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9282 | ` */` |
|  891480 |  9283 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9284 |  |
|  891485 |  9285 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  891485 |  9286 | `	sxi32 iFlags = 0,iFlag;` |
|  891609 |  9287 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     129 |  9288 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9289 | `			pDup = pIn;` |
|       2 |  9290 | `		}` |
|     129 |  9291 | `		iFlags \|= iFlag;` |
|     129 |  9292 | `		pIn++;` |
|       5 |  9293 | `	}` |
|  891485 |  9294 | `	*ppIn = pIn;` |
|  891485 |  9295 | `	if( ppDup ){ *ppDup = pDup; }` |
|  891485 |  9296 | `	return iFlags;` |
|       5 |  9297 |  |
|       - |  9298 | `/*` |
|       - |  9299 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9300 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9301 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9302 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9303 | `` * `readonly`) to their existing handlers.`` |
|       - |  9304 | ` */` |
|  891428 |  9305 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9306 |  |
|  891433 |  9307 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  445773 |  9308 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  891456 |  9309 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9310 |  |
|       - |  9311 | `/*` |
|       - |  9312 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9313 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9314 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9315 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9316 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9317 | ` */` |
|      52 |  9318 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9319 |  |
|       - |  9320 | `	SyToken *pDup;` |
|      57 |  9321 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9322 | `	sxi32 rc;` |
|      57 |  9323 | `	if( pDup ){` |
|       4 |  9324 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9325 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9326 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9327 | `			return SXERR_ABORT;` |
|       - |  9328 | `		}` |
|       1 |  9329 | `	}` |
|      52 |  9330 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|      31 |  9331 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9332 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9333 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9334 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9335 | `			return SXERR_ABORT;` |
|       - |  9336 | `		}` |
|       1 |  9337 | `	}` |
|      57 |  9338 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|      31 |  9339 |  |
|       - |  9340 | `/*` |
|       - |  9341 | ` * Compile a user-defined trait.` |
|       - |  9342 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9343 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9344 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9345 | ` */` |
|      56 |  9346 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9347 |  |
|      61 |  9348 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9349 | `	ph7_class *pClass;` |
|       - |  9350 | `	SyToken *pEnd,*pTmp;` |
|       - |  9351 | `	sxi32 iProtection;` |
|       - |  9352 | `	sxi32 iAttrflags;` |
|       - |  9353 | `	SyString *pName;` |
|       - |  9354 | `	sxi32 nKwrd;` |
|       - |  9355 | `	sxi32 rc;` |
|       - |  9356 | `	/* Jump the 'trait' keyword */` |
|      61 |  9357 | `	pGen->pIn++;` |
|      61 |  9358 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9359 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9360 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9361 | `			return SXERR_ABORT;` |
|       - |  9362 | `		}` |
|     ! 0 |  9363 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9364 | `			pGen->pIn++;` |
|     ! 0 |  9365 | `		}` |
|     ! 0 |  9366 | `		return SXRET_OK;` |
|       - |  9367 | `	}` |
|       - |  9368 | `	/* Extract trait name */` |
|      61 |  9369 | `	pName = &pGen->pIn->sData;` |
|      61 |  9370 | `	pGen->pIn++;` |
|       - |  9371 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9372 | `		SyBlob sFQN;` |
|       - |  9373 | `		SyString sFQNStr;` |
|      61 |  9374 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      61 |  9375 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      61 |  9376 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      61 |  9377 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      61 |  9378 | `		SyBlobRelease(&sFQN);` |
|       - |  9379 | `	}` |
|      61 |  9380 | `	if( pClass == 0 ){` |
|     ! 0 |  9381 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9382 | `		return SXERR_ABORT;` |
|       - |  9383 | `	}` |
|       - |  9384 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      61 |  9385 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9386 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9387 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9388 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9389 | `			return SXERR_ABORT;` |
|       - |  9390 | `		}` |
|     ! 0 |  9391 | `		return SXRET_OK;` |
|       - |  9392 | `	}` |
|      61 |  9393 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      61 |  9394 | `	pEnd = 0;` |
|      61 |  9395 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      61 |  9396 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  9397 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  9398 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9399 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9400 | `			return SXERR_ABORT;` |
|       - |  9401 | `		}` |
|     ! 0 |  9402 | `		return SXRET_OK;` |
|       - |  9403 | `	}` |
|       - |  9404 | `	/* Swap token stream */` |
|      61 |  9405 | `	pTmp = pGen->pEnd;` |
|      61 |  9406 | `	pGen->pEnd = pEnd;` |
|       - |  9407 | `	/* Mark as trait */` |
|      61 |  9408 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9409 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      56 |  9410 | `	for(;;){` |
|     161 |  9411 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9412 | `			pGen->pIn++;` |
|       4 |  9413 | `		}` |
|     137 |  9414 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      61 |  9415 | `			break;` |
|       - |  9416 | `		}` |
|      81 |  9417 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9418 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9419 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9420 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9421 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9422 | `				return SXERR_ABORT;` |
|       - |  9423 | `			}` |
|     ! 0 |  9424 | `			goto done;` |
|       - |  9425 | `		}` |
|      81 |  9426 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      81 |  9427 | `		iAttrflags = 0;` |
|      81 |  9428 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      81 |  9429 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      81 |  9430 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9431 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9432 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9433 | `				for(;;){` |
|       - |  9434 | `					ph7_class *pUsedTrait;` |
|       - |  9435 | `					SyString *pUsedName;` |
|       5 |  9436 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9437 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9438 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9439 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9440 | `							return SXERR_ABORT;` |
|       - |  9441 | `						}` |
|     ! 0 |  9442 | `						break;` |
|       - |  9443 | `					}` |
|       5 |  9444 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9445 | `					{` |
|       - |  9446 | `						SyBlob sResolved;` |
|       5 |  9447 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9448 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9449 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9450 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9451 | `						SyBlobRelease(&sResolved);` |
|       - |  9452 | `					}` |
|       5 |  9453 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9454 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9455 | `					}` |
|       5 |  9456 | `					if( pUsedTrait == 0 ){` |
|       4 |  9457 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9458 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9459 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9460 | `							return SXERR_ABORT;` |
|       - |  9461 | `						}` |
|       2 |  9462 | `					}else{` |
|       3 |  9463 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9464 | `					}` |
|       5 |  9465 | `					pGen->pIn++;` |
|       5 |  9466 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  9467 | `						break;` |
|       - |  9468 | `					}` |
|     ! 0 |  9469 | `					pGen->pIn++;` |
|     ! 0 |  9470 | `				}` |
|       5 |  9471 | `				continue;` |
|       - |  9472 | `			}` |
|      77 |  9473 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  9474 | `				iProtection = nKwrd;` |
|      73 |  9475 | `				pGen->pIn++;` |
|      68 |  9476 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  9477 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  9478 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9479 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9480 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9481 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9482 | `						return SXERR_ABORT;` |
|       - |  9483 | `					}` |
|     ! 0 |  9484 | `					goto done;` |
|       - |  9485 | `				}` |
|      73 |  9486 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  9487 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9488 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9489 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9490 | `							return SXERR_ABORT;` |
|       - |  9491 | `						}` |
|     ! 0 |  9492 | `						goto done;` |
|       - |  9493 | `					}` |
|      12 |  9494 | `					continue;` |
|       - |  9495 | `				}` |
|      63 |  9496 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  9497 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  9498 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9499 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9500 | `							return SXERR_ABORT;` |
|       - |  9501 | `						}` |
|     ! 0 |  9502 | `						goto done;` |
|       - |  9503 | `					}` |
|       5 |  9504 | `					continue;` |
|       - |  9505 | `				}` |
|      58 |  9506 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  9507 | `			}` |
|      62 |  9508 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  9509 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9510 | `					"Traits cannot have constants");` |
|     ! 0 |  9511 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9512 | `					return SXERR_ABORT;` |
|       - |  9513 | `				}` |
|     ! 0 |  9514 | `				goto done;` |
|     ! 0 |  9515 | `			}else{` |
|      62 |  9516 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9517 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9518 | `					pGen->pIn++;` |
|       5 |  9519 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9520 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9521 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9522 | `							iProtection = nKwrd;` |
|     ! 0 |  9523 | `							pGen->pIn++;` |
|     ! 0 |  9524 | `						}` |
|       1 |  9525 | `					}` |
|       4 |  9526 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9527 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  9528 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9529 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9530 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9531 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9532 | `							return SXERR_ABORT;` |
|       - |  9533 | `						}` |
|     ! 0 |  9534 | `						goto done;` |
|       - |  9535 | `					}` |
|       5 |  9536 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9537 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9538 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9539 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9540 | `								return SXERR_ABORT;` |
|       - |  9541 | `							}` |
|     ! 0 |  9542 | `							goto done;` |
|       - |  9543 | `						}` |
|       3 |  9544 | `						continue;` |
|       - |  9545 | `					}` |
|       3 |  9546 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9547 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9548 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9549 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9550 | `								return SXERR_ABORT;` |
|       - |  9551 | `							}` |
|     ! 0 |  9552 | `							goto done;` |
|       - |  9553 | `						}` |
|     ! 0 |  9554 | `						continue;` |
|       - |  9555 | `					}` |
|       3 |  9556 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      59 |  9557 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 |  9558 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 |  9559 | `					pGen->pIn++;` |
|       6 |  9560 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 |  9561 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  9562 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  9563 | `							iProtection = nKwrd;` |
|       6 |  9564 | `							pGen->pIn++;` |
|       2 |  9565 | `						}` |
|       2 |  9566 | `					}` |
|       6 |  9567 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9568 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9569 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9570 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9571 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9572 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9573 | `							return SXERR_ABORT;` |
|       - |  9574 | `						}` |
|     ! 0 |  9575 | `						goto done;` |
|       - |  9576 | `					}` |
|       6 |  9577 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9578 | `				}` |
|      60 |  9579 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9580 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9581 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9582 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9583 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9584 | `						return SXERR_ABORT;` |
|       - |  9585 | `					}` |
|     ! 0 |  9586 | `					goto done;` |
|       - |  9587 | `				}` |
|      60 |  9588 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9589 | `					pGen->pIn++;` |
|     ! 0 |  9590 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9591 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9592 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9593 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9594 | `							return SXERR_ABORT;` |
|       - |  9595 | `						}` |
|     ! 0 |  9596 | `						goto done;` |
|       - |  9597 | `					}` |
|     ! 0 |  9598 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9599 | `				}else{` |
|      60 |  9600 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9601 | `				}` |
|      60 |  9602 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9603 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9604 | `						return SXERR_ABORT;` |
|       - |  9605 | `					}` |
|     ! 0 |  9606 | `					goto done;` |
|       - |  9607 | `				}` |
|       - |  9608 | `			}` |
|      32 |  9609 | `		}else{` |
|     ! 0 |  9610 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9611 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9612 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9613 | `					return SXERR_ABORT;` |
|       - |  9614 | `				}` |
|     ! 0 |  9615 | `				goto done;` |
|       - |  9616 | `			}` |
|       - |  9617 | `		}` |
|       4 |  9618 | `	}` |
|       - |  9619 | `	/* Install the trait */` |
|      61 |  9620 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      61 |  9621 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9622 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9623 | `		return SXERR_ABORT;` |
|       - |  9624 | `	}` |
|      28 |  9625 | `done:` |
|       - |  9626 | `	/* Point beyond the trait body */` |
|      61 |  9627 | `	pGen->pIn = &pEnd[1];` |
|      61 |  9628 | `	pGen->pEnd = pTmp;` |
|      61 |  9629 | `	return PH7_OK;` |
|      33 |  9630 |  |
|       - |  9631 | `/*` |
|       - |  9632 | ` * Compile a user-defined class.` |
|       - |  9633 | ` *  According to the PHP language reference manual` |
|       - |  9634 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9635 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9636 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9637 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9638 | ` *   and functions (called "methods").` |
|       - |  9639 | ` */` |
|   90284 |  9640 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 |  9641 |  |
|       - |  9642 | `	sxi32 rc;` |
|   90289 |  9643 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   90289 |  9644 | `	return rc;` |
|       5 |  9645 |  |
|       - |  9646 | `/*` |
|       - |  9647 | ` * Exception handling.` |
|       - |  9648 | ` *  According to the PHP language reference manual` |
|       - |  9649 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9650 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9651 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9652 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9653 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9654 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9655 | ` *    (or re-thrown) within a catch block.` |
|       - |  9656 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9657 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9658 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9659 | ` *    been defined with set_exception_handler().` |
|       - |  9660 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9661 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9662 | ` */` |
|       - |  9663 | `/*` |
|       - |  9664 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9665 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9666 | ` * indicates failure.` |
|       - |  9667 | ` */` |
|   10130 |  9668 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  9669 |  |
|   10135 |  9670 | `	sxi32 rc = SXRET_OK;` |
|   10135 |  9671 | `	if( pRoot->pOp ){` |
|   10127 |  9672 | `		switch( pRoot->pOp->iOp ){` |
|    5061 |  9673 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9674 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9675 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9676 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9677 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9678 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   10127 |  9679 | `			break;` |
|     ! 0 |  9680 | `		default:` |
|       - |  9681 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9682 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9683 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9684 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9685 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9686 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9687 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9688 | `			}` |
|     ! 0 |  9689 | `			break;` |
|       - |  9690 | `		}` |
|    5074 |  9691 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9692 | `		/* Unexpected expression */` |
|     ! 0 |  9693 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9694 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9695 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9696 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9697 | `		}` |
|     ! 0 |  9698 | `	}` |
|   10135 |  9699 | `	return rc;` |
|       5 |  9700 |  |
|       - |  9701 | `/*` |
|       - |  9702 | ` * Compile a 'throw' statement.` |
|       - |  9703 | ` * throw: This is how you trigger an exception.` |
|       - |  9704 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9705 | ` */` |
|   10094 |  9706 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 |  9707 |  |
|   10099 |  9708 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9709 | `	GenBlock *pBlock;` |
|       - |  9710 | `	sxu32 nIdx;` |
|       - |  9711 | `	sxi32 rc;` |
|   10099 |  9712 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9713 | `	/* Compile the expression */` |
|   10099 |  9714 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   10099 |  9715 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9716 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9717 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9718 | `			return SXERR_ABORT;` |
|       - |  9719 | `		}` |
|     ! 0 |  9720 | `		return SXRET_OK;` |
|       - |  9721 | `	}` |
|   10099 |  9722 | `	pBlock = pGen->pCurrent;` |
|       - |  9723 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   46641 |  9724 | `	while(pBlock->pParent){` |
|   46637 |  9725 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   10095 |  9726 | `			break;` |
|       - |  9727 | `		}` |
|       - |  9728 | `		/* Point to the parent block */` |
|   36547 |  9729 | `		pBlock = pBlock->pParent;` |
|       5 |  9730 | `	}` |
|       - |  9731 | `	/* Emit the throw instruction */` |
|   10099 |  9732 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9733 | `	/* Emit the jump */` |
|   10099 |  9734 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   10099 |  9735 | `	return SXRET_OK;` |
|    5052 |  9736 |  |
|       - |  9737 | `/*` |
|       - |  9738 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9739 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9740 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9741 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9742 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9743 | ` */` |
|      36 |  9744 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9745 |  |
|      38 |  9746 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9747 | `	GenBlock *pBlock;` |
|       - |  9748 | `	sxu32 nIdx;` |
|       - |  9749 | `	sxi32 rc;` |
|      18 |  9750 | `	(void)iCompileFlag;` |
|      38 |  9751 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9752 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9753 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9754 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9755 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9756 | `			return SXERR_ABORT;` |
|       - |  9757 | `		}` |
|     ! 0 |  9758 | `		return SXRET_OK;` |
|       - |  9759 | `	}` |
|      38 |  9760 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9761 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9762 | `		return SXERR_ABORT;` |
|       - |  9763 | `	}` |
|      38 |  9764 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9765 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9766 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9767 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9768 | `			return SXERR_ABORT;` |
|       - |  9769 | `		}` |
|     ! 0 |  9770 | `		return SXRET_OK;` |
|       - |  9771 | `	}` |
|       - |  9772 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  9773 | `	pBlock = pGen->pCurrent;` |
|      60 |  9774 | `	while( pBlock->pParent ){` |
|      49 |  9775 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  9776 | `			break;` |
|       - |  9777 | `		}` |
|      23 |  9778 | `		pBlock = pBlock->pParent;` |
|       1 |  9779 | `	}` |
|      38 |  9780 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 |  9781 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 |  9782 | `	return SXRET_OK;` |
|      20 |  9783 |  |
|       - |  9784 | `/*` |
|       - |  9785 | ` * Compile a 'catch' block.` |
|       - |  9786 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  9787 | ` * an object containing the exception information.` |
|       - |  9788 | ` */` |
|     426 |  9789 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 |  9790 |  |
|     431 |  9791 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9792 | `	ph7_exception_block sCatch;` |
|       - |  9793 | `	SySet *pInstrContainer;` |
|       - |  9794 | `	SyString sClassName;` |
|       - |  9795 | `	GenBlock *pCatch;` |
|       - |  9796 | `	SyToken *pToken;` |
|       - |  9797 | `	SyString *pName;` |
|       - |  9798 | `	char *zDup;` |
|       - |  9799 | `	sxi32 rc;` |
|     431 |  9800 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  9801 | `	/* Zero the structure */` |
|     431 |  9802 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  9803 | `	/* Initialize fields */` |
|     431 |  9804 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     431 |  9805 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     431 |  9806 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  9807 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9808 | `			pToken = pGen->pIn;` |
|     ! 0 |  9809 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9810 | `				pToken--;` |
|     ! 0 |  9811 | `			}` |
|     ! 0 |  9812 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9813 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9814 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9815 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9816 | `				return SXERR_ABORT;` |
|       - |  9817 | `			}` |
|     ! 0 |  9818 | `			return SXERR_INVALID;` |
|       - |  9819 | `	}` |
|       - |  9820 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     431 |  9821 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     226 |  9822 | `	for(;;){` |
|       - |  9823 | `		SyBlob sResolved;` |
|     457 |  9824 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     457 |  9825 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 |  9826 | `			SyBlobRelease(&sResolved);` |
|       6 |  9827 | `			pToken = pGen->pIn;` |
|       6 |  9828 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9829 | `				pToken--;` |
|     ! 0 |  9830 | `			}` |
|       8 |  9831 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9832 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  9833 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 |  9834 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9835 | `				return SXERR_ABORT;` |
|       - |  9836 | `			}` |
|       6 |  9837 | `			return SXERR_INVALID;` |
|       - |  9838 | `		}` |
|       - |  9839 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - |  9840 | `		 * transient SyBlob allocation. */` |
|     677 |  9841 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     448 |  9842 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     453 |  9843 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     453 |  9844 | `		SyBlobRelease(&sResolved);` |
|     453 |  9845 | `		if( zDup == 0 ){` |
|     ! 0 |  9846 | `			goto Mem;` |
|       - |  9847 | `		}` |
|     453 |  9848 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     453 |  9849 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9850 | `			goto Mem;` |
|       - |  9851 | `		}` |
|       - |  9852 | `		/* Check for '\|' (multi-catch separator) */` |
|     448 |  9853 | `		if( pGen->pIn < pGen->pEnd &&` |
|     448 |  9854 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      31 |  9855 | `			pGen->pIn->sData.nByte == 1 &&` |
|      26 |  9856 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      28 |  9857 | `			pGen->pIn++; /* Consume the '\|' */` |
|      28 |  9858 | `			continue;` |
|       - |  9859 | `		}` |
|     427 |  9860 | `		break;` |
|     ! 0 |  9861 | `	}` |
|     422 |  9862 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     427 |  9863 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  9864 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9865 | `			pToken = pGen->pIn;` |
|     ! 0 |  9866 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9867 | `				pToken--;` |
|     ! 0 |  9868 | `			}` |
|     ! 0 |  9869 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9870 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9871 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9872 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9873 | `				return SXERR_ABORT;` |
|       - |  9874 | `			}` |
|     ! 0 |  9875 | `			return SXERR_INVALID;` |
|       - |  9876 | `	}` |
|     427 |  9877 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9878 | `	/* Duplicate instance name */` |
|     427 |  9879 | `	pName = &pGen->pIn->sData;` |
|     427 |  9880 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     427 |  9881 | `	if( zDup == 0 ){` |
|     ! 0 |  9882 | `		goto Mem;` |
|       - |  9883 | `	}` |
|     427 |  9884 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     427 |  9885 | `	pGen->pIn++;` |
|     427 |  9886 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9887 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9888 | `		pToken = pGen->pIn;` |
|     ! 0 |  9889 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9890 | `			pToken--;` |
|     ! 0 |  9891 | `		}` |
|     ! 0 |  9892 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9893 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9894 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9895 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9896 | `			return SXERR_ABORT;` |
|       - |  9897 | `		}` |
|     ! 0 |  9898 | `		return SXERR_INVALID;` |
|       - |  9899 | `	}` |
|       - |  9900 | `	/* Compile the block */` |
|     427 |  9901 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9902 | `	/* Create the catch block */` |
|     427 |  9903 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     427 |  9904 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9905 | `		return SXERR_ABORT;` |
|       - |  9906 | `	}` |
|       - |  9907 | `	/* Swap bytecode container */` |
|     427 |  9908 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     427 |  9909 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9910 | `	/* Compile the block */` |
|     427 |  9911 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9912 | `	/* Fix forward jumps now the destination is resolved  */` |
|     427 |  9913 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9914 | `	/* Emit the DONE instruction */` |
|     427 |  9915 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9916 | `	/* Leave the block */` |
|     427 |  9917 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9918 | `	/* Restore the default container */` |
|     427 |  9919 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9920 | `	/* Install the catch block */` |
|     427 |  9921 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     427 |  9922 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9923 | `		goto Mem;` |
|       - |  9924 | `	}` |
|     427 |  9925 | `	return SXRET_OK;` |
|     ! 0 |  9926 | `Mem:` |
|     ! 0 |  9927 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9928 | `	return SXERR_ABORT;` |
|     218 |  9929 |  |
|       - |  9930 | `/*` |
|       - |  9931 | ` * Compile a 'try' block.` |
|       - |  9932 | ` * A function using an exception should be in a "try" block.` |
|       - |  9933 | ` * If the exception does not trigger, the code will continue` |
|       - |  9934 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9935 | ` * is "thrown".` |
|       - |  9936 | ` */` |
|     440 |  9937 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 |  9938 |  |
|       - |  9939 | `	ph7_exception *pException;` |
|     445 |  9940 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9941 | `	GenBlock *pTry;` |
|       - |  9942 | `	sxu32 nJmpIdx;` |
|       - |  9943 | `	sxi32 rc;` |
|       - |  9944 | `	/* Create the exception container */` |
|     445 |  9945 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     445 |  9946 | `	if( pException == 0 ){` |
|     ! 0 |  9947 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9948 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9949 | `		return SXERR_ABORT;` |
|       - |  9950 | `	}` |
|       - |  9951 | `	/* Zero the structure */` |
|     445 |  9952 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9953 | `	/* Initialize fields */` |
|     445 |  9954 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     445 |  9955 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     445 |  9956 | `	pException->iHasFinally = 0;` |
|     445 |  9957 | `	pException->iFinallyDone = 0;` |
|     445 |  9958 | `	pException->pVm = pGen->pVm;` |
|       - |  9959 | `	/* Create the try block */` |
|     445 |  9960 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     445 |  9961 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9962 | `		return SXERR_ABORT;` |
|       - |  9963 | `	}` |
|       - |  9964 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     445 |  9965 | `	pTry->pUserData = pException;` |
|       - |  9966 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     445 |  9967 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9968 | `	/* Fix the jump later when the destination is resolved */` |
|     445 |  9969 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     445 |  9970 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9971 | `	/* Compile the block */` |
|     445 |  9972 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     445 |  9973 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9974 | `		return SXERR_ABORT;` |
|       - |  9975 | `	}` |
|       - |  9976 | `	/* Fix forward jumps now the destination is resolved */` |
|     445 |  9977 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9978 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     445 |  9979 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  9980 | `	/* Leave the block */` |
|     445 |  9981 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9982 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     445 |  9983 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     438 |  9984 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  9985 | `		/* Compile one or more catch blocks */` |
|     422 |  9986 | `		for(;;){` |
|     844 |  9987 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     664 |  9988 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     214 |  9989 | `					break;` |
|       - |  9990 | `			}` |
|     431 |  9991 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     431 |  9992 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9993 | `				return SXERR_ABORT;` |
|       - |  9994 | `			}` |
|       5 |  9995 | `		}` |
|     209 |  9996 | `	}` |
|       - |  9997 | `	/* Compile optional finally block */` |
|     445 |  9998 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     218 |  9999 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10000 | `		SySet *pInstrContainer;` |
|       - | 10001 | `		GenBlock *pFinBlock;` |
|      53 | 10002 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10003 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      53 | 10004 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      53 | 10005 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10006 | `			return SXERR_ABORT;` |
|       - | 10007 | `		}` |
|       - | 10008 | `		/* Swap bytecode container */` |
|      53 | 10009 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      53 | 10010 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10011 | `		/* Compile the finally body */` |
|      53 | 10012 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      53 | 10013 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10014 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10015 | `			return SXERR_ABORT;` |
|       - | 10016 | `		}` |
|       - | 10017 | `		/* Fix forward jumps now the destination is resolved */` |
|      53 | 10018 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10019 | `		/* Emit DONE to terminate the finally block */` |
|      53 | 10020 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10021 | `		/* Leave the block */` |
|      53 | 10022 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10023 | `		/* Restore the default container */` |
|      53 | 10024 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      53 | 10025 | `		pException->iHasFinally = 1;` |
|      24 | 10026 | `	}` |
|       - | 10027 | `	/* Must have at least one catch or finally */` |
|     445 | 10028 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       9 | 10029 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10030 | `			"Cannot use try without catch or finally");` |
|       9 | 10031 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10032 | `			return SXERR_ABORT;` |
|       - | 10033 | `		}` |
|       3 | 10034 | `	}` |
|     445 | 10035 | `	return SXRET_OK;` |
|     225 | 10036 |  |
|       - | 10037 | `/*` |
|       - | 10038 | ` * Compile a switch block.` |
|       - | 10039 | ` *  (See block-comment below for more information)` |
|       - | 10040 | ` */` |
|     112 | 10041 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 10042 |  |
|     117 | 10043 | `	sxi32 rc = SXRET_OK;` |
|     117 | 10044 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 10045 | `		/* Unexpected token */` |
|     ! 0 | 10046 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10047 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10048 | `			return SXERR_ABORT;` |
|       - | 10049 | `		}` |
|     ! 0 | 10050 | `		pGen->pIn++;` |
|     ! 0 | 10051 | `	}` |
|     117 | 10052 | `	pGen->pIn++;` |
|       - | 10053 | `	/* First instruction to execute in this block. */` |
|     117 | 10054 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10055 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 10056 | `	 * or the '}' token */` |
|     206 | 10057 | `	for(;;){` |
|     417 | 10058 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10059 | `			/* No more input to process */` |
|     ! 0 | 10060 | `			break;` |
|       - | 10061 | `		}` |
|     417 | 10062 | `		rc = SXRET_OK;` |
|     417 | 10063 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 10064 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 10065 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 10066 | `					/* Unexpected token */` |
|     ! 0 | 10067 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10068 | `						&pGen->pIn->sData);` |
|     ! 0 | 10069 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10070 | `						return SXERR_ABORT;` |
|       - | 10071 | `					}` |
|       - | 10072 | `					/* FALL THROUGH */` |
|     ! 0 | 10073 | `				}` |
|      31 | 10074 | `				rc = SXERR_EOF;` |
|      31 | 10075 | `				break;` |
|       - | 10076 | `			}` |
|      32 | 10077 | `		}else{` |
|       - | 10078 | `			sxi32 nKwrd;` |
|       - | 10079 | `			/* Extract the keyword */` |
|     337 | 10080 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 10081 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 10082 | `				break;` |
|       - | 10083 | `			}` |
|     253 | 10084 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10085 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 10086 | `					/* Unexpected token */` |
|     ! 0 | 10087 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10088 | `						&pGen->pIn->sData);` |
|     ! 0 | 10089 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10090 | `						return SXERR_ABORT;` |
|       - | 10091 | `					}` |
|       - | 10092 | `					/* FALL THROUGH */` |
|     ! 0 | 10093 | `				}` |
|       - | 10094 | `				/* Block compiled */` |
|       3 | 10095 | `				break;` |
|       - | 10096 | `			}` |
|       - | 10097 | `		}` |
|       - | 10098 | `		/* Compile block */` |
|     305 | 10099 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 10100 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10101 | `			return SXERR_ABORT;` |
|       - | 10102 | `		}` |
|       5 | 10103 | `	}` |
|     117 | 10104 | `	return rc;` |
|      61 | 10105 |  |
|       - | 10106 | `/*` |
|       - | 10107 | ` * Compile a case eXpression.` |
|       - | 10108 | ` *  (See block-comment below for more information)` |
|       - | 10109 | ` */` |
|      92 | 10110 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 10111 |  |
|       - | 10112 | `	SySet *pInstrContainer;` |
|       - | 10113 | `	SyToken *pEnd,*pTmp;` |
|      97 | 10114 | `	sxi32 iNest = 0;` |
|       - | 10115 | `	sxi32 rc;` |
|       - | 10116 | `	/* Delimit the expression */` |
|      97 | 10117 | `	pEnd = pGen->pIn;` |
|     197 | 10118 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 10119 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 10120 | `			/* Increment nesting level */` |
|       3 | 10121 | `			iNest++;` |
|     196 | 10122 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 10123 | `			/* Decrement nesting level */` |
|       3 | 10124 | `			iNest--;` |
|     194 | 10125 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 10126 | `			break;` |
|       - | 10127 | `		}` |
|     105 | 10128 | `		pEnd++;` |
|       5 | 10129 | `	}` |
|      97 | 10130 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 10131 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 10132 | `		if( rc == SXERR_ABORT ){` |
|       - | 10133 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10134 | `			return SXERR_ABORT;` |
|       - | 10135 | `		}` |
|     ! 0 | 10136 | `	}` |
|       - | 10137 | `	/* Swap token stream */` |
|      97 | 10138 | `	pTmp = pGen->pEnd;` |
|      97 | 10139 | `	pGen->pEnd = pEnd;` |
|      97 | 10140 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 10141 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 10142 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 10143 | `	/* Emit the done instruction */` |
|      97 | 10144 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 10145 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10146 | `	/* Update token stream */` |
|      97 | 10147 | `	pGen->pIn  = pEnd;` |
|      97 | 10148 | `	pGen->pEnd = pTmp;` |
|      97 | 10149 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10150 | `		return SXERR_ABORT;` |
|       - | 10151 | `	}` |
|      97 | 10152 | `	return SXRET_OK;` |
|      51 | 10153 |  |
|       - | 10154 | `/*` |
|       - | 10155 | ` * Compile the smart switch statement.` |
|       - | 10156 | ` * According to the PHP language reference manual` |
|       - | 10157 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 10158 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 10159 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 10160 | ` *  This is exactly what the switch statement is for.` |
|       - | 10161 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 10162 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 10163 | ` *  of the outer loop, use continue 2.` |
|       - | 10164 | ` *  Note that switch/case does loose comparision.` |
|       - | 10165 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 10166 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 10167 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 10168 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 10169 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 10170 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 10171 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 10172 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 10173 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 10174 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 10175 | ` *  list for the next case.` |
|       - | 10176 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 10177 | ` *  or floating-point numbers and strings.` |
|       - | 10178 | ` */` |
|      28 | 10179 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 10180 |  |
|       - | 10181 | `	GenBlock *pSwitchBlock;` |
|       - | 10182 | `	SyToken *pTmp,*pEnd;` |
|       - | 10183 | `	ph7_switch *pSwitch;` |
|       - | 10184 | `	sxu32 nToken;` |
|       - | 10185 | `	sxu32 nLine;` |
|       - | 10186 | `	sxi32 rc;` |
|      33 | 10187 | `	nLine = pGen->pIn->nLine;` |
|       - | 10188 | `	/* Jump the 'switch' keyword */` |
|      33 | 10189 | `	pGen->pIn++;` |
|      33 | 10190 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 10191 | `		/* Syntax error */` |
|     ! 0 | 10192 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 10193 | `		if( rc == SXERR_ABORT ){` |
|       - | 10194 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10195 | `			return SXERR_ABORT;` |
|       - | 10196 | `		}` |
|     ! 0 | 10197 | `		goto Synchronize;` |
|       - | 10198 | `	}` |
|       - | 10199 | `	/* Jump the left parenthesis '(' */` |
|      33 | 10200 | `	pGen->pIn++;` |
|      33 | 10201 | `	pEnd = 0; /* cc warning */` |
|       - | 10202 | `	/* Create the loop block */` |
|      47 | 10203 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 10204 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 10205 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10206 | `		return SXERR_ABORT;` |
|       - | 10207 | `	}` |
|       - | 10208 | `	/* Delimit the condition */` |
|      33 | 10209 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 10210 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 10211 | `		/* Empty expression */` |
|     ! 0 | 10212 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 10213 | `		if( rc == SXERR_ABORT ){` |
|       - | 10214 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10215 | `			return SXERR_ABORT;` |
|       - | 10216 | `		}` |
|     ! 0 | 10217 | `	}` |
|       - | 10218 | `	/* Swap token streams */` |
|      33 | 10219 | `	pTmp = pGen->pEnd;` |
|      33 | 10220 | `	pGen->pEnd = pEnd;` |
|       - | 10221 | `	/* Compile the expression */` |
|      33 | 10222 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 10223 | `	if( rc == SXERR_ABORT ){` |
|       - | 10224 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 10225 | `		return SXERR_ABORT;` |
|       - | 10226 | `	}` |
|       - | 10227 | `	/* Update token stream */` |
|      33 | 10228 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 10229 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10230 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10231 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10232 | `			return SXERR_ABORT;` |
|       - | 10233 | `		}` |
|     ! 0 | 10234 | `		pGen->pIn++;` |
|     ! 0 | 10235 | `	}` |
|      33 | 10236 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 10237 | `	pGen->pEnd = pTmp;` |
|      33 | 10238 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 10239 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 10240 | `			pTmp = pGen->pIn;` |
|     ! 0 | 10241 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 10242 | `				pTmp--;` |
|     ! 0 | 10243 | `			}` |
|       - | 10244 | `			/* Unexpected token */` |
|     ! 0 | 10245 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 10246 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10247 | `				return SXERR_ABORT;` |
|       - | 10248 | `			}` |
|     ! 0 | 10249 | `			goto Synchronize;` |
|       - | 10250 | `	}` |
|       - | 10251 | `	/* Set the delimiter token */` |
|      33 | 10252 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 10253 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 10254 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 10255 | `	}else{` |
|      31 | 10256 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 10257 | `	}` |
|      33 | 10258 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 10259 | `	/* Create the switch blocks container */` |
|      33 | 10260 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 10261 | `	if( pSwitch == 0 ){` |
|       - | 10262 | `		/* Abort compilation */` |
|     ! 0 | 10263 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10264 | `		return SXERR_ABORT;` |
|       - | 10265 | `	}` |
|       - | 10266 | `	/* Zero the structure */` |
|      33 | 10267 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 10268 | `	/* Initialize fields */` |
|      33 | 10269 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 10270 | `	/* Emit the switch instruction */` |
|      33 | 10271 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 10272 | `	/* Compile case blocks */` |
|     100 | 10273 | `	for(;;){` |
|       - | 10274 | `		sxu32 nKwrd;` |
|     119 | 10275 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10276 | `			/* No more input to process */` |
|     ! 0 | 10277 | `			break;` |
|       - | 10278 | `		}` |
|     119 | 10279 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 10280 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 10281 | `				/* Unexpected token */` |
|     ! 0 | 10282 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10283 | `					&pGen->pIn->sData);` |
|     ! 0 | 10284 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10285 | `					return SXERR_ABORT;` |
|       - | 10286 | `				}` |
|       - | 10287 | `				/* FALL THROUGH */` |
|     ! 0 | 10288 | `			}` |
|       - | 10289 | `			/* Block compiled */` |
|     ! 0 | 10290 | `			break;` |
|       - | 10291 | `		}` |
|       - | 10292 | `		/* Extract the keyword */` |
|     119 | 10293 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 10294 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10295 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 10296 | `				/* Unexpected token */` |
|     ! 0 | 10297 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10298 | `					&pGen->pIn->sData);` |
|     ! 0 | 10299 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10300 | `					return SXERR_ABORT;` |
|       - | 10301 | `				}` |
|       - | 10302 | `				/* FALL THROUGH */` |
|     ! 0 | 10303 | `			}` |
|       - | 10304 | `			/* Block compiled */` |
|       3 | 10305 | `			break;` |
|       - | 10306 | `		}` |
|     117 | 10307 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 10308 | `			/*` |
|       - | 10309 | `			 * Accroding to the PHP language reference manual` |
|       - | 10310 | `			 *  A special case is the default case. This case matches anything` |
|       - | 10311 | `			 *  that wasn't matched by the other cases.` |
|       - | 10312 | `			 */` |
|      25 | 10313 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 10314 | `				/* Default case already compiled */` |
|     ! 0 | 10315 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 10316 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10317 | `					return SXERR_ABORT;` |
|       - | 10318 | `				}` |
|     ! 0 | 10319 | `			}` |
|      25 | 10320 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 10321 | `			/* Compile the default block */` |
|      25 | 10322 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 10323 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10324 | `				return SXERR_ABORT;` |
|      25 | 10325 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 10326 | `				break;` |
|       1 | 10327 | `			}` |
|      98 | 10328 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 10329 | `			ph7_case_expr sCase;` |
|       - | 10330 | `			/* Standard case block */` |
|      97 | 10331 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 10332 | `			/* initialize the structure */` |
|      97 | 10333 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 10334 | `			/* Compile the case expression */` |
|      97 | 10335 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 10336 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10337 | `				return SXERR_ABORT;` |
|       - | 10338 | `			}` |
|       - | 10339 | `			/* Compile the case block */` |
|      97 | 10340 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 10341 | `			/* Insert in the switch container */` |
|      97 | 10342 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 10343 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10344 | `				return SXERR_ABORT;` |
|      97 | 10345 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 10346 | `				break;` |
|       - | 10347 | `			}` |
|      47 | 10348 | `		}else{` |
|       - | 10349 | `			/* Unexpected token */` |
|     ! 0 | 10350 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10351 | `				&pGen->pIn->sData);` |
|     ! 0 | 10352 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10353 | `				return SXERR_ABORT;` |
|       - | 10354 | `			}` |
|     ! 0 | 10355 | `			break;` |
|       - | 10356 | `		}` |
|       5 | 10357 | `	}` |
|       - | 10358 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 10359 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 10360 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10361 | `	/* Release the loop block */` |
|      33 | 10362 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 10363 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 10364 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 10365 | `		pGen->pIn++;` |
|      14 | 10366 | `	}` |
|       - | 10367 | `	/* Statement successfully compiled */` |
|      33 | 10368 | `	return SXRET_OK;` |
|     ! 0 | 10369 | `Synchronize:` |
|       - | 10370 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 10371 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 10372 | `		pGen->pIn++;` |
|     ! 0 | 10373 | `	}` |
|     ! 0 | 10374 | `	return SXRET_OK;` |
|      19 | 10375 |  |
|       - | 10376 | `/*` |
|       - | 10377 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 10378 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 10379 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 10380 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 10381 | ` */` |
|       - | 10382 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 10383 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 10384 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 10385 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 10386 |  |
|       - | 10387 | `/*` |
|       - | 10388 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 10389 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 10390 | ` * patched entries from the pending set.` |
|       - | 10391 | ` */` |
| 2417702 | 10392 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 10393 |  |
| 2417707 | 10394 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 10395 | `	sxu32 nTarget;` |
|       - | 10396 | `	sxu32 *aIdx;` |
|       - | 10397 | `	sxu32 i;` |
| 2417707 | 10398 | `	if( nCur <= nBaseline ){` |
| 2417617 | 10399 | `		return;` |
|       - | 10400 | `	}` |
|      93 | 10401 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      93 | 10402 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     191 | 10403 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     101 | 10404 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     101 | 10405 | `		if( pInstr ){` |
|     101 | 10406 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 | 10407 | `		}` |
|      52 | 10408 | `	}` |
|      93 | 10409 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1208856 | 10410 |  |
|       - | 10411 |  |
|       - | 10412 | `/*` |
|       - | 10413 | ` * By-reference out-parameters of builtin functions.` |
|       - | 10414 | ` *` |
|       - | 10415 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 10416 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 10417 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 10418 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 10419 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 10420 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 10421 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 10422 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 10423 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 10424 | ` * creates it" behaviour).` |
|       - | 10425 | ` *` |
|       - | 10426 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 10427 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 10428 | ` */` |
|  392774 | 10429 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 10430 |  |
|       - | 10431 | `	static const struct {` |
|       - | 10432 | `		const char *zName;` |
|       - | 10433 | `		sxu32 nByte;` |
|       - | 10434 | `		sxu32 mask;` |
|       - | 10435 | `	} aByRef[] = {` |
|       - | 10436 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10437 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10438 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10439 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10440 | `	};` |
|       - | 10441 | `	sxu32 i;` |
|  392779 | 10442 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1197 | 10443 | `		return 0;` |
|       - | 10444 | `	}` |
| 1957771 | 10445 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1566230 | 10446 | `		if( pName->nByte == aByRef[i].nByte` |
|  803810 | 10447 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      51 | 10448 | `			return aByRef[i].mask;` |
|       - | 10449 | `		}` |
|  783097 | 10450 | `	}` |
|  391541 | 10451 | `	return 0;` |
|  196392 | 10452 |  |
|       - | 10453 | `/*` |
|       - | 10454 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 10455 | ` *` |
|       - | 10456 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 10457 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 10458 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 10459 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 10460 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 10461 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 10462 | ` */` |
|  392774 | 10463 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 10464 |  |
|       - | 10465 | `	SyToken *p, *pEnd;` |
|  392779 | 10466 | `	pOut->zString = 0;` |
|  392779 | 10467 | `	pOut->nByte = 0;` |
|  392779 | 10468 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 10469 | `		return;` |
|       - | 10470 | `	}` |
|  392779 | 10471 | `	p = pLeft->pStart;` |
|  392779 | 10472 | `	pEnd = pLeft->pEnd;` |
|       - | 10473 | `	/* Optional single leading namespace separator (absolute path). */` |
|  392779 | 10474 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      26 | 10475 | `		p++;` |
|      11 | 10476 | `	}` |
|  392779 | 10477 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1171 | 10478 | `		return;` |
|       - | 10479 | `	}` |
|       - | 10480 | `	/* Must be a single component: nothing follows the name token. */` |
|  391613 | 10481 | `	if( p + 1 != pEnd ){` |
|      30 | 10482 | `		return;` |
|       - | 10483 | `	}` |
|  391587 | 10484 | `	*pOut = p->sData;` |
|  196392 | 10485 |  |
|       - | 10486 | `/*` |
|       - | 10487 | ` * Generate bytecode for a given expression tree.` |
|       - | 10488 | ` * If something goes wrong while generating bytecode` |
|       - | 10489 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 10490 | ` * this function takes care of generating the appropriate` |
|       - | 10491 | ` * error message.` |
|       - | 10492 | ` */` |
| 3257644 | 10493 | `static sxi32 GenStateEmitExprCode(` |
|       - | 10494 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10495 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 10496 | `	sxi32 iFlags /* Control flags */` |
|       - | 10497 | `	)` |
|       5 | 10498 |  |
|       - | 10499 | `	VmInstr *pInstr;` |
|       - | 10500 | `	sxu32 nJmpIdx;` |
| 3257649 | 10501 | `	sxi32 iP1 = 0;` |
| 3257649 | 10502 | `	sxu32 iP2 = 0;` |
| 3257649 | 10503 | `	void *p3  = 0;` |
|       - | 10504 | `	sxi32 iVmOp;` |
|       - | 10505 | `	sxi32 rc;` |
| 3257649 | 10506 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3257649 | 10507 | `	sxu32 nRhsNsBase = 0;` |
| 3257649 | 10508 | `	if( pNode->xCode ){` |
|       - | 10509 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 10510 | `		/* Compile node */` |
| 2018179 | 10511 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2018179 | 10512 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2018179 | 10513 | `		RE_SWAP_DELIMITER(pGen);` |
| 2018179 | 10514 | `		return rc;` |
|       - | 10515 | `	}` |
| 1239475 | 10516 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10517 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10518 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10519 | `		return SXERR_ABORT;` |
|       - | 10520 | `	}` |
| 1239475 | 10521 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1239475 | 10522 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      59 | 10523 | `		sxu32 nJmp = 0;` |
|       - | 10524 | `		sxu32 nNcNsBase;` |
|       - | 10525 | `		VmInstr *pInstrFix;` |
|       - | 10526 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10527 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10528 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10529 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10530 | `		 * stack slot carries a writable nIdx. */` |
|      59 | 10531 | `		if( pNode->pRight ){` |
|      59 | 10532 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10533 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      59 | 10534 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10535 | `				return rc;` |
|       - | 10536 | `			}` |
|      59 | 10537 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10538 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10539 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10540 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10541 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10542 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10543 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10544 | `			 * cascade for the actual write path stays correct. */` |
|      59 | 10545 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      59 | 10546 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      29 | 10547 | `				pInstrFix->iP2 = 3;` |
|      13 | 10548 | `			}` |
|      28 | 10549 | `		}` |
|       - | 10550 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      59 | 10551 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10552 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      59 | 10553 | `		if( pNode->pLeft ){` |
|      59 | 10554 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10555 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      59 | 10556 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10557 | `				return rc;` |
|       - | 10558 | `			}` |
|      59 | 10559 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      28 | 10560 | `		}` |
|       - | 10561 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      59 | 10562 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10563 | `		/* Patch the short-circuit jump to land after the store. */` |
|      59 | 10564 | `		if( nJmp > 0 ){` |
|      59 | 10565 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      59 | 10566 | `			if( pInstrFix ){` |
|      59 | 10567 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10568 | `			}` |
|      28 | 10569 | `		}` |
|      59 | 10570 | `		return SXRET_OK;` |
|       - | 10571 | `	}` |
| 1239419 | 10572 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10573 | `		sxu32 nJz,nJmp;` |
|       - | 10574 | `		sxu32 nTernaryNsBase;` |
|       - | 10575 | `		/* Ternary operator require special handling */` |
|       - | 10576 | `		/* Phase#1: Compile the condition */` |
|    2631 | 10577 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2631 | 10578 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2631 | 10579 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10580 | `			return rc;` |
|       - | 10581 | `		}` |
|       - | 10582 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10583 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10584 | `		 * condition expression, not leak past the ternary. */` |
|    2631 | 10585 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2631 | 10586 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2631 | 10587 | `		if( pNode->pLeft ){` |
|       - | 10588 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10589 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2563 | 10590 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10591 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2563 | 10592 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2563 | 10593 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2563 | 10594 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10595 | `				return rc;` |
|       - | 10596 | `			}` |
|    2563 | 10597 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1284 | 10598 | `		}else{` |
|       - | 10599 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10600 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10601 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10602 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10603 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10604 | `		}` |
|       - | 10605 | `		/* Phase#4: Emit the unconditional jump */` |
|    2631 | 10606 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10607 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2631 | 10608 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2631 | 10609 | `		if( pInstr ){` |
|    2631 | 10610 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1313 | 10611 | `		}` |
|    2631 | 10612 | `		if( !pNode->pLeft ){` |
|       - | 10613 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10614 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10615 | `		}` |
|       - | 10616 | `		/* Phase#6: Compile the 'else' expression */` |
|    2631 | 10617 | `		if( pNode->pRight ){` |
|    2631 | 10618 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2631 | 10619 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2631 | 10620 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10621 | `				return rc;` |
|       - | 10622 | `			}` |
|    2631 | 10623 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1313 | 10624 | `		}` |
|    2631 | 10625 | `		if( nJmp > 0 ){` |
|       - | 10626 | `			/* Phase#7: Fix the unconditional jump */` |
|    2631 | 10627 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2631 | 10628 | `			if( pInstr ){` |
|    2631 | 10629 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1313 | 10630 | `			}` |
|    1313 | 10631 | `		}` |
|       - | 10632 | `		/* All done */` |
|    2631 | 10633 | `		return SXRET_OK;` |
|       - | 10634 | `	}` |
| 1236793 | 10635 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10636 | `	/* Generate code for the left tree */` |
| 1236793 | 10637 | `	if( pNode->pLeft ){` |
| 1236755 | 10638 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1236755 | 10639 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10640 | `			ph7_expr_node **apNode;` |
|  392899 | 10641 | `			int hasSpread = 0;` |
|  392899 | 10642 | `			int hasNamed = 0;` |
|  392899 | 10643 | `			int bAnySpread = 0;` |
|  392899 | 10644 | `			sxu32 byRefMask = 0;` |
|       - | 10645 | `			sxi32 nArgs;` |
|       - | 10646 | `			sxi32 n;` |
|       - | 10647 | `			/* Recurse and generate bytecodes for function arguments */` |
|  392899 | 10648 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  392899 | 10649 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10650 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10651 | `			{` |
|  392899 | 10652 | `				int seenNamed = 0;` |
|  778171 | 10653 | `				for( n = 0; n < nArgs; ++n ){` |
|  385279 | 10654 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     188 | 10655 | `						seenNamed = 1;` |
|     188 | 10656 | `						hasNamed = 1;` |
|  385187 | 10657 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      23 | 10658 | `						bAnySpread = 1;` |
|  385085 | 10659 | `					}else if( seenNamed ){` |
|       3 | 10660 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10661 | `							"Cannot use positional argument after named argument");` |
|       3 | 10662 | `						return SXERR_SYNTAX;` |
|       - | 10663 | `					}` |
|  192641 | 10664 | `				}` |
|       - | 10665 | `			}` |
|       - | 10666 | `			/* Read-only load */` |
|  392897 | 10667 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10668 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10669 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10670 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10671 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  392897 | 10672 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  392897 | 10673 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  392892 | 10674 | `				if( pCallName->nByte == 5` |
|  215616 | 10675 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   20139 | 10676 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  382830 | 10677 | `				}else if( pCallName->nByte == 5` |
|  195482 | 10678 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      83 | 10679 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      39 | 10680 | `				}` |
|       - | 10681 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10682 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10683 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10684 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10685 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10686 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  392897 | 10687 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10688 | `					SyString sBuiltin;` |
|  392779 | 10689 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  392779 | 10690 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  196387 | 10691 | `				}` |
|  196446 | 10692 | `			}` |
|  778167 | 10693 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  385275 | 10694 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  385275 | 10695 | `				sxi32 iArgFlags = iFlags & ~EXPR_FLAG_LOAD_IDX_STORE;` |
|       - | 10696 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 10697 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate). */` |
|  385275 | 10698 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      31 | 10699 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      13 | 10700 | `				}` |
|  385275 | 10701 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  385275 | 10702 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10703 | `					return rc;` |
|       - | 10704 | `				}` |
|       - | 10705 | `				/* Each argument is an independent nullsafe scope. */` |
|  385275 | 10706 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  385275 | 10707 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10708 | `					/* Emit spread opcode to unpack this array argument */` |
|      23 | 10709 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      23 | 10710 | `					hasSpread = 1;` |
|      10 | 10711 | `				}` |
|  192640 | 10712 | `			}` |
|       - | 10713 | `			/* Total number of given arguments */` |
|  392897 | 10714 | `			iP1 = nArgs;` |
|  392897 | 10715 | `			iP2 = hasSpread;` |
|       - | 10716 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10717 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  392897 | 10718 | `			if( hasNamed ){` |
|     101 | 10719 | `				sxu32 nStrBytes = 0;` |
|       - | 10720 | `				char *zBuf;` |
|     297 | 10721 | `				for( n = 0; n < nArgs; ++n ){` |
|     199 | 10722 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10723 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      91 | 10724 | `					}` |
|     101 | 10725 | `				}` |
|       - | 10726 | `				{` |
|     101 | 10727 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     101 | 10728 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      98 | 10729 | `					&pGen->pVm->sAllocator, mapSize);` |
|     101 | 10730 | `				if( pMap ){` |
|     101 | 10731 | `					SyZero(pMap, mapSize);` |
|     101 | 10732 | `					pMap->bHasNamed = 1;` |
|     101 | 10733 | `					pMap->nTotal = (sxu32)nArgs;` |
|     101 | 10734 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     101 | 10735 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     297 | 10736 | `					for( n = 0; n < nArgs; ++n ){` |
|     199 | 10737 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10738 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     185 | 10739 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     185 | 10740 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     185 | 10741 | `							zBuf += nb;` |
|      91 | 10742 | `						}` |
|       - | 10743 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     101 | 10744 | `					}` |
|     101 | 10745 | `					p3 = (void *)pMap;` |
|      49 | 10746 | `				}` |
|       - | 10747 | `				}` |
|      49 | 10748 | `			}` |
|       - | 10749 | `			/* Remove stale flags now */` |
|  392897 | 10750 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  196446 | 10751 | `		}` |
| 1236753 | 10752 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1236753 | 10753 | `		if( rc != SXRET_OK ){` |
|      34 | 10754 | `			return rc;` |
|       - | 10755 | `		}` |
| 1236723 | 10756 | `		if( !bIsChainOp ){` |
|       - | 10757 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 10758 | `			 * target the end of that LHS chain, which is right here. */` |
|  577753 | 10759 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  288874 | 10760 | `		}` |
| 1236723 | 10761 | `		if( iVmOp == PH7_OP_CALL ){` |
|  392897 | 10762 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  392897 | 10763 | `			if( pInstr ){` |
|  392897 | 10764 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  391707 | 10765 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 10766 | `					sxu32 nQual;` |
|  391707 | 10767 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10768 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 10769 | `					 * so the later NEW handler (if any) can see it. */` |
|  391707 | 10770 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 10771 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 10772 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 10773 | `					 * imports — class imports must NOT affect function` |
|       - | 10774 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 10775 | `					 * before NEW; we store the original literal index in the` |
|       - | 10776 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 10777 | `					 * the unqualified name and re-qualify with class imports. */` |
|  391707 | 10778 | `					if( bAbsolute ){` |
|      26 | 10779 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      15 | 10780 | `					}else{` |
|  391685 | 10781 | `						int fromImport = 0;` |
|  391685 | 10782 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  391685 | 10783 | `						pInstr->iP2 = (sxi32)nQual;` |
|  391685 | 10784 | `						if( nQual != nOrig ){` |
|       - | 10785 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 10786 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 10787 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 10788 | `							if( !fromImport ){` |
|       - | 10789 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 10790 | `								if( p3 == 0 ){` |
|      67 | 10791 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 10792 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 10793 | `									if( pMap ){` |
|      67 | 10794 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 10795 | `										p3 = (void *)pMap;` |
|      31 | 10796 | `									}` |
|      31 | 10797 | `								}` |
|      67 | 10798 | `								if( p3 ){` |
|      67 | 10799 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 10800 | `								}` |
|      31 | 10801 | `							}` |
|      36 | 10802 | `						}` |
|       5 | 10803 | `					}` |
|  197046 | 10804 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 10805 | `					/* Method call,flag that */` |
|     915 | 10806 | `					pInstr->iP2 = 1;` |
|     455 | 10807 | `				}` |
|  196451 | 10808 | `			}` |
| 1040277 | 10809 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 10810 | `			ph7_expr_node **apNode;` |
|       - | 10811 | `			sxi32 n;` |
|   85137 | 10812 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 10813 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 10814 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       - | 10815 | `			/* Recurse and generate bytecodes for array index */` |
|   85137 | 10816 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  153645 | 10817 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   68513 | 10818 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   68513 | 10819 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   68513 | 10820 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10821 | `					return rc;` |
|       - | 10822 | `				}` |
|       - | 10823 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   68513 | 10824 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   34259 | 10825 | `			}` |
|   85137 | 10826 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   68513 | 10827 | `				iP1 = 1; /* Node have an index associated with it */` |
|   34254 | 10828 | `			}` |
|   85137 | 10829 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 10830 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     243 | 10831 | `				iP2 = 4;` |
|   85018 | 10832 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 10833 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 10834 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      54 | 10835 | `				iP2 = 5;` |
|   84874 | 10836 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 10837 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 10838 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 10839 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 10840 | `				iP2 = 6;` |
|   84837 | 10841 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 10842 | `				/* Create an empty entry when the desired index is not found */` |
|   33525 | 10843 | `				iP2 = 1;` |
|   16765 | 10844 | `			}` |
|  801265 | 10845 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 10846 | `			/* POP the left node */` |
|      32 | 10847 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 10848 | `		}` |
|  618359 | 10849 | `	}` |
| 1236761 | 10850 | `	rc = SXRET_OK;` |
| 1236761 | 10851 | `	nJmpIdx = 0;` |
|       - | 10852 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 10853 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 10854 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1236761 | 10855 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     327 | 10856 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     327 | 10857 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     327 | 10858 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     327 | 10859 | `			int isSpecial = 0;` |
|     327 | 10860 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     239 | 10861 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     239 | 10862 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     234 | 10863 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     232 | 10864 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     110 | 10865 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      93 | 10866 | `					isSpecial = 1;` |
|      44 | 10867 | `				}` |
|     139 | 10868 | `			}` |
|     371 | 10869 | `			pInstr->iP1 = 0;` |
|     371 | 10870 | `			if( !isSpecial ){` |
|     195 | 10871 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      95 | 10872 | `			}` |
|       - | 10873 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 10874 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     283 | 10875 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     195 | 10876 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     195 | 10877 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 10878 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      46 | 10879 | `					return SXRET_OK;` |
|       - | 10880 | `				}` |
|      74 | 10881 | `			}` |
|     118 | 10882 | `		}` |
|     194 | 10883 | `	}` |
|       - | 10884 | `	/* Generate code for the right tree */` |
| 1236683 | 10885 | `	if( pNode->pRight ){` |
|  682745 | 10886 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 10887 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   10385 | 10888 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  677555 | 10889 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 10890 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3489 | 10891 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  670623 | 10892 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 10893 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 10894 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 10895 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  668868 | 10896 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 10897 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 10898 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 10899 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 10900 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 10901 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 10902 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     101 | 10903 | `			sxu32 nNsJmp = 0;` |
|     101 | 10904 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     101 | 10905 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  668708 | 10906 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  277187 | 10907 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  138591 | 10908 | `		}` |
|  682745 | 10909 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  682745 | 10910 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  682745 | 10911 | `		if( !bIsChainOp ){` |
|       - | 10912 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 10913 | `			 * operator instruction is emitted. */` |
|  501841 | 10914 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  250918 | 10915 | `		}` |
|  682745 | 10916 | `		if( iVmOp == PH7_OP_STORE ){` |
|  273623 | 10917 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  273592 | 10918 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 10919 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 10920 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 10921 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 10922 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 10923 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 10924 | `				 */` |
|      74 | 10925 | `				iVmOp = 0;` |
|  273588 | 10926 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  273553 | 10927 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 10928 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   76455 | 10929 | `					iP2 = 1;` |
|   38230 | 10930 | `				}else{` |
|  197103 | 10931 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10932 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   33477 | 10933 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   33477 | 10934 | `						iP1 = pInstr->iP1;` |
|   16741 | 10935 | `					}else{` |
|  163631 | 10936 | `						p3 = pInstr->p3;` |
|       - | 10937 | `					}` |
|       - | 10938 | `					/* POP the last dynamic load instruction */` |
|  197103 | 10939 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 10940 | `				}` |
|  136779 | 10941 | `			}` |
|  545936 | 10942 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      54 | 10943 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      54 | 10944 | `			if( pInstr ){` |
|      54 | 10945 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10946 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 10947 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 10948 | `					 */` |
|      17 | 10949 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 10950 | `					iP1 = pInstr->iP1;` |
|      17 | 10951 | `					iP2 = pInstr->iP2;` |
|      17 | 10952 | `					p3  = pInstr->p3;` |
|       9 | 10953 | `				}else{` |
|      38 | 10954 | `					p3 = pInstr->p3;` |
|       - | 10955 | `				}` |
|      26 | 10956 | `			}` |
|      26 | 10957 | `		}` |
|  341370 | 10958 | `	}` |
| 1236678 | 10959 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    8931 | 10960 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 10961 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 10962 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      21 | 10963 | `		iVmOp = 0;` |
|      10 | 10964 | `	}` |
| 1236683 | 10965 | `	if( iVmOp > 0 ){` |
| 1236439 | 10966 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   13617 | 10967 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 10968 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    9943 | 10969 | `				iP1 = 1;` |
|    4974 | 10970 | `			}` |
| 1229633 | 10971 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 10972 | `			/* Namespace-qualify the class name for NEW */ {` |
|   17743 | 10973 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   17743 | 10974 | `				VmInstr *pCallInstr = 0;` |
|   17743 | 10975 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   17669 | 10976 | `					pCallInstr = pPeek;` |
|   17669 | 10977 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    8832 | 10978 | `				}` |
|   17743 | 10979 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   17741 | 10980 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10981 | `					sxu32 nLitForClass;` |
|       - | 10982 | `					/* If the CALL handler already qualified the name using` |
|       - | 10983 | `					 * function imports, recover the original unqualified` |
|       - | 10984 | `					 * literal so we can re-qualify with class imports. */` |
|   17741 | 10985 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 10986 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 10987 | `					}else{` |
|   17709 | 10988 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 10989 | `					}` |
|   17741 | 10990 | `					pPeek->iP1 = 0;` |
|   17741 | 10991 | `					if( !bAbsolute ){` |
|   17723 | 10992 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    8864 | 10993 | `					}else{` |
|      22 | 10994 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 10995 | `					}` |
|    8868 | 10996 | `				}` |
|       - | 10997 | `			}` |
|   17743 | 10998 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   17743 | 10999 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 11000 | `				VmInstr *pPrev;` |
|   17669 | 11001 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   17669 | 11002 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 11003 | `					/* Pop the call instruction, preserve named-arg map */` |
|   17669 | 11004 | `					iP1 = pInstr->iP1;` |
|   17669 | 11005 | `					if( pInstr->p3 ){` |
|      43 | 11006 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 11007 | `					}` |
|   17669 | 11008 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    8832 | 11009 | `				}` |
|    8837 | 11010 | `			}` |
| 1213958 | 11011 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 11012 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 11013 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     169 | 11014 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     169 | 11015 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     169 | 11016 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     169 | 11017 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     169 | 11018 | `				int isSpecialIs = 0;` |
|     169 | 11019 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     165 | 11020 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     165 | 11021 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     160 | 11022 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     165 | 11023 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      81 | 11024 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 11025 | `						isSpecialIs = 1;` |
|       5 | 11026 | `					}` |
|      81 | 11027 | `				}` |
|     171 | 11028 | `				pInstr->iP1 = 0;` |
|     171 | 11029 | `				if( !isSpecialIs && !bAbsolute ){` |
|     149 | 11030 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      72 | 11031 | `				}` |
|      86 | 11032 | `			}` |
| 1205010 | 11033 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 11034 | `			/* Prevent constant expansion for member/property names.` |
|       - | 11035 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 11036 | `			 * should not trigger constant lookup. */` |
|  180909 | 11037 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  180909 | 11038 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  180867 | 11039 | `				pInstr->iP1 = 0;` |
|   90431 | 11040 | `			}` |
|  180909 | 11041 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 11042 | `				/* Static member access,remember that */` |
|     249 | 11043 | `				iP1 = 1;` |
|     249 | 11044 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     249 | 11045 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      38 | 11046 | `					p3 = pInstr->p3;` |
|      38 | 11047 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      17 | 11048 | `				}` |
|     122 | 11049 | `			}` |
|   90452 | 11050 | `		}` |
|       - | 11051 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 11052 | `		 * This is the primary emit path for user-visible calls. */` |
| 1236437 | 11053 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  410635 | 11054 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  205315 | 11055 | `		}` |
|       - | 11056 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1236437 | 11057 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  618216 | 11058 | `	}` |
| 1236681 | 11059 | `	if( nJmpIdx > 0 ){` |
|       - | 11060 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   13993 | 11061 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   13993 | 11062 | `		if( pInstr ){` |
|   13993 | 11063 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    6994 | 11064 | `		}` |
|    6994 | 11065 | `	}` |
| 1236681 | 11066 | `	return rc;` |
| 1628808 | 11067 |  |
|       - | 11068 | `/*` |
|       - | 11069 | ` * Compile a PHP expression.` |
|       - | 11070 | ` * According to the PHP language reference manual:` |
|       - | 11071 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 11072 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 11073 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 11074 | ` *  is "anything that has a value".` |
|       - | 11075 | ` * If something goes wrong while compiling the expression,this` |
|       - | 11076 | ` * function takes care of generating the appropriate error` |
|       - | 11077 | ` * message.` |
|       - | 11078 | ` */` |
|  876610 | 11079 | `static sxi32 PH7_CompileExpr(` |
|       - | 11080 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11081 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 11082 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 11083 | `	)` |
|       5 | 11084 |  |
|       - | 11085 | `	ph7_expr_node *pRoot;` |
|       - | 11086 | `	SySet sExprNode;` |
|       - | 11087 | `	SyToken *pEnd;` |
|       - | 11088 | `	sxi32 nExpr;` |
|       - | 11089 | `	sxi32 iNest;` |
|       - | 11090 | `	sxi32 rc;` |
|       - | 11091 | `	sxu32 nNullsafeBase;` |
|       - | 11092 | `	/* Initialize worker variables */` |
|  876615 | 11093 | `	nExpr = 0;` |
|  876615 | 11094 | `	pRoot = 0;` |
|       - | 11095 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 11096 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  876615 | 11097 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  876615 | 11098 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  876615 | 11099 | `	SySetAlloc(&sExprNode,0x10);` |
|  876615 | 11100 | `	rc = SXRET_OK;` |
|       - | 11101 | `	/* Delimit the expression */` |
|  876615 | 11102 | `	pEnd = pGen->pIn;` |
|  876615 | 11103 | `	iNest = 0;` |
| 5864039 | 11104 | `	while( pEnd < pGen->pEnd ){` |
| 5564717 | 11105 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11106 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     457 | 11107 | `			iNest++;` |
| 5564491 | 11108 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     465 | 11109 | `			iNest--;` |
| 5564035 | 11110 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  577617 | 11111 | `			if( iNest <= 0 ){` |
|  577293 | 11112 | `				break;` |
|       - | 11113 | `			}` |
|     162 | 11114 | `		}` |
| 4987429 | 11115 | `		pEnd++;` |
|       5 | 11116 | `	}` |
|  876615 | 11117 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   20357 | 11118 | `		SyToken *pEnd2 = pGen->pIn;` |
|   20357 | 11119 | `		iNest = 0;` |
|       - | 11120 | `		/* Stop at the first comma */` |
|   41001 | 11121 | `		while( pEnd2 < pEnd ){` |
|   20655 | 11122 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      65 | 11123 | `				iNest++;` |
|   20625 | 11124 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      65 | 11125 | `				iNest--;` |
|   20565 | 11126 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 11127 | `				if( iNest <= 0 ){` |
|       7 | 11128 | `					break;` |
|       - | 11129 | `				}` |
|      23 | 11130 | `			}` |
|   20649 | 11131 | `			pEnd2++;` |
|       5 | 11132 | `		}` |
|   20357 | 11133 | `		if( pEnd2 <pEnd ){` |
|       7 | 11134 | `			pEnd = pEnd2;` |
|       3 | 11135 | `		}` |
|   10176 | 11136 | `	}` |
|  876615 | 11137 | `	if( pEnd > pGen->pIn ){` |
|  876605 | 11138 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 11139 | `		/* Swap delimiter */` |
|  876605 | 11140 | `		pGen->pEnd = pEnd;` |
|       - | 11141 | `		/* Try to get an expression tree */` |
|  876605 | 11142 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  876605 | 11143 | `		if( rc == SXRET_OK && pRoot ){` |
|  876423 | 11144 | `			rc = SXRET_OK;` |
|  876423 | 11145 | `			if( xTreeValidator ){` |
|       - | 11146 | `				/* Call the upper layer validator callback */` |
|   24309 | 11147 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   12152 | 11148 | `			}` |
|  876423 | 11149 | `			if( rc != SXERR_ABORT ){` |
|       - | 11150 | `				/* Generate code for the given tree */` |
|  876423 | 11151 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 11152 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 11153 | `				 * expression so they short-circuit to its end. */` |
|  876423 | 11154 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  438209 | 11155 | `			}` |
|  876423 | 11156 | `			nExpr = 1;` |
|  438209 | 11157 | `		}` |
|       - | 11158 | `		/* Release the whole tree */` |
|  876605 | 11159 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 11160 | `		/* Synchronize token stream */` |
|  876605 | 11161 | `		pGen->pEnd = pTmp;` |
|  876605 | 11162 | `		pGen->pIn  = pEnd;` |
|  876605 | 11163 | `		if( rc == SXERR_ABORT ){` |
|      14 | 11164 | `			SySetRelease(&sExprNode);` |
|      14 | 11165 | `			return SXERR_ABORT;` |
|       - | 11166 | `		}` |
|  438295 | 11167 | `	}` |
|  876605 | 11168 | `	SySetRelease(&sExprNode);` |
|  876605 | 11169 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  438310 | 11170 |  |
|       - | 11171 | `/*` |
|       - | 11172 | ` * Return a pointer to the node construct handler associated` |
|       - | 11173 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 11174 | ` */` |
|  223140 | 11175 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 11176 |  |
|  223145 | 11177 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 11178 | `		/* Numeric literal: Either real or integer */` |
|  117103 | 11179 | `		return PH7_CompileNumLiteral;` |
|  106047 | 11180 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 11181 | `		/* Double quoted string */` |
|   22115 | 11182 | `		return PH7_CompileString;` |
|   83937 | 11183 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 11184 | `		/* Single quoted string */` |
|   83821 | 11185 | `		return PH7_CompileSimpleString;` |
|     121 | 11186 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 11187 | `		/* Heredoc */` |
|      68 | 11188 | `		return PH7_CompileHereDoc;` |
|      57 | 11189 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 11190 | `		/* Nowdoc */` |
|      50 | 11191 | `		return PH7_CompileNowDoc;` |
|       8 | 11192 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 11193 | `		/* Backtick quoted string */` |
|       6 | 11194 | `		return PH7_CompileBacktic;` |
|       - | 11195 | `	}` |
|       3 | 11196 | `	return 0;` |
|  111575 | 11197 |  |
|       - | 11198 | `/*` |
|       - | 11199 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 11200 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 11201 | ` * in write context" parse error.` |
|       - | 11202 | ` */` |
|    6824 | 11203 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 11204 |  |
|       - | 11205 | `	sxi32 rc;` |
|    6829 | 11206 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6827 | 11207 | `		return SXRET_OK;` |
|       - | 11208 | `	}` |
|       5 | 11209 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 11210 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 11211 | `		"Can't use nullsafe operator in write context");` |
|       3 | 11212 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3417 | 11213 |  |
|       - | 11214 | `/*` |
|       - | 11215 | ` * Compile an unset() statement.` |
|       - | 11216 | ` * unset($var, $arr[$key], ...);` |
|       - | 11217 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 11218 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 11219 | ` * parent array before extracting the element to unset.` |
|       - | 11220 | ` */` |
|    2946 | 11221 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 11222 |  |
|    2951 | 11223 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2951 | 11224 | `	sxu32 nIdx = 0;` |
|       - | 11225 | `	SyString sName;` |
|       - | 11226 | `	sxi32 rc;` |
|       - | 11227 | `	/* Jump the 'unset' keyword */` |
|    2951 | 11228 | `	pGen->pIn++;` |
|       - | 11229 | `	/* Save delimiter */` |
|    2951 | 11230 | `	pTmp = pGen->pEnd;` |
|       - | 11231 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2951 | 11232 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2951 | 11233 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 11234 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 11235 | `		SyToken *pClose;` |
|    2951 | 11236 | `		pGen->pIn++;   /* Skip '(' */` |
|    2951 | 11237 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2951 | 11238 | `		pEnd = pClose; /* Stop at ')' */` |
|    1473 | 11239 | `	}` |
|    2951 | 11240 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 11241 | `	/* Resolve the 'unset' builtin name once */` |
|    2951 | 11242 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     363 | 11243 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     363 | 11244 | `		if( pObj == 0 ){` |
|     ! 0 | 11245 | `			return SXERR_ABORT;` |
|       - | 11246 | `		}` |
|     363 | 11247 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     363 | 11248 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     179 | 11249 | `	}` |
|       - | 11250 | `	/* Compile each comma-separated argument */` |
|    9777 | 11251 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6831 | 11252 | `		if( pGen->pIn < pNext ){` |
|    6831 | 11253 | `			pGen->pEnd = pNext;` |
|    6831 | 11254 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 11255 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 11256 | `				GenStateUnsetValidator);` |
|    6831 | 11257 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11258 | `				return SXERR_ABORT;` |
|       - | 11259 | `			}` |
|    6831 | 11260 | `			if( rc != SXERR_EMPTY ){` |
|       - | 11261 | `				/* Emit call for this single argument */` |
|    6829 | 11262 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6829 | 11263 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6829 | 11264 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3412 | 11265 | `			}` |
|    3413 | 11266 | `		}` |
|       - | 11267 | `		/* Jump trailing commas */` |
|   10713 | 11268 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3887 | 11269 | `			pNext++;` |
|       5 | 11270 | `		}` |
|    6831 | 11271 | `		pGen->pIn = pNext;` |
|       5 | 11272 | `	}` |
|       - | 11273 | `	/* Skip past the closing ')' if present */` |
|    2951 | 11274 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2951 | 11275 | `		pGen->pIn++;` |
|    1473 | 11276 | `	}` |
|       - | 11277 | `	/* Restore token stream */` |
|    2951 | 11278 | `	pGen->pEnd = pTmp;` |
|    2951 | 11279 | `	return SXRET_OK;` |
|    1478 | 11280 |  |
|       - | 11281 | `/*` |
|       - | 11282 | ` * PHP Language construct table.` |
|       - | 11283 | ` */` |
|       - | 11284 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 11285 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 11286 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 11287 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 11288 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 11289 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 11290 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 11291 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 11292 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 11293 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 11294 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 11295 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 11296 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 11297 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 11298 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 11299 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 11300 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 11301 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 11302 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 11303 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 11304 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 11305 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 11306 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 11307 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 11308 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 11309 | `};` |
|       - | 11310 | `/*` |
|       - | 11311 | ` * Return a pointer to the statement handler routine associated` |
|       - | 11312 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 11313 | ` */` |
|  590722 | 11314 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 11315 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 11316 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 11317 | `	)` |
|       5 | 11318 |  |
|  590727 | 11319 | `	sxu32 n = 0;` |
| 3049188 | 11320 | `	for(;;){` |
| 6098381 | 11321 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  126955 | 11322 | `			break;` |
|       - | 11323 | `		}` |
| 5971431 | 11324 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  463777 | 11325 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 11326 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 11327 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 11328 | `					/* 'static' (class context),return null */` |
|     ! 0 | 11329 | `					return 0;` |
|       - | 11330 | `				}` |
|     ! 0 | 11331 | `			}` |
|  463772 | 11332 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       6 | 11333 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 11334 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 11335 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 11336 | `				return 0;` |
|       - | 11337 | `			}` |
|       - | 11338 | `			/* Return a pointer to the handler.` |
|       - | 11339 | `			*/` |
|  463777 | 11340 | `			return aLangConstruct[n].xConstruct;` |
|       - | 11341 | `		}` |
| 5507659 | 11342 | `		n++;` |
|       5 | 11343 | `	}` |
|  126955 | 11344 | `	if( pLookahed ){` |
|  126955 | 11345 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   36423 | 11346 | `			return PH7_CompileClassInterface;` |
|   90537 | 11347 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   90289 | 11348 | `			return PH7_CompileClass;` |
|     253 | 11349 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      61 | 11350 | `			return PH7_CompileTrait;` |
|       - | 11351 | `		}` |
|       - | 11352 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 11353 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 11354 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 11355 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|      96 | 11356 | `	}` |
|       - | 11357 | `	/* Not a language construct */` |
|     197 | 11358 | `	return 0;` |
|  295366 | 11359 |  |
|       - | 11360 | `/*` |
|       - | 11361 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 11362 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 11363 | ` */` |
|     192 | 11364 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 11365 |  |
|       - | 11366 | `	int rc;` |
|     197 | 11367 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     197 | 11368 | `	if( rc == FALSE ){` |
|      82 | 11369 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      81 | 11370 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 11371 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 11372 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 11373 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 11374 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 11375 | `			*/` |
|       - | 11376 | `			){` |
|      79 | 11377 | `				rc = TRUE;` |
|      37 | 11378 | `		}` |
|      41 | 11379 | `	}` |
|     197 | 11380 | `	return rc;` |
|       5 | 11381 |  |
|       - | 11382 | `/*` |
|       - | 11383 | ` * Compile a PHP chunk.` |
|       - | 11384 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11385 | ` * takes care of generating the appropriate error message.` |
|       - | 11386 | ` */` |
|  707522 | 11387 | `static sxi32 GenStateCompileChunk(` |
|       - | 11388 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11389 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 11390 | `	)` |
|       5 | 11391 |  |
|       - | 11392 | `	ProcLangConstruct xCons;` |
|       - | 11393 | `	sxi32 rc;` |
|  707527 | 11394 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  551459 | 11395 | `	for(;;){` |
|  905225 | 11396 | `		int bStmtIsDeclare = 0;` |
|  905225 | 11397 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11398 | `			/* No more input to process */` |
|   13781 | 11399 | `			break;` |
|       - | 11400 | `		}` |
|       - | 11401 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 11402 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  891449 | 11403 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  590753 | 11404 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  590753 | 11405 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 11406 | `				bStmtIsDeclare = 1;` |
|      20 | 11407 | `			}` |
|  295374 | 11408 | `		}` |
|  891449 | 11409 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 11410 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 11411 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  197673 | 11412 | `			pGen->bStrictTypesLocked = 1;` |
|   98834 | 11413 | `		}` |
|  891449 | 11414 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11415 | `			/* Compile block */` |
|      21 | 11416 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      21 | 11417 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11418 | `				break;` |
|       - | 11419 | `			}` |
|      13 | 11420 | `		}else{` |
|  891433 | 11421 | `			xCons = 0;` |
|  891433 | 11422 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 11423 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 11424 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 11425 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|      57 | 11426 | `				xCons = PH7_CompileClassModifiers;` |
|  891407 | 11427 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  590727 | 11428 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 11429 | `				/* Try to extract a language construct handler */` |
|  590727 | 11430 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  590727 | 11431 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 11432 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 11433 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 11434 | `						&pGen->pIn->sData);` |
|       9 | 11435 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11436 | `						break;` |
|       - | 11437 | `					}` |
|       - | 11438 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 11439 | `					 * this erroneous statement.` |
|       - | 11440 | `					 */` |
|       9 | 11441 | `					xCons = PH7_ErrorRecover;` |
|       4 | 11442 | `				}` |
|  596020 | 11443 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   49311 | 11444 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 11445 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 11446 | `				xCons = PH7_CompileLabel;` |
|      56 | 11447 | `			}` |
|  891433 | 11448 | `			if( xCons == 0 ){` |
|       - | 11449 | `				/* Assume an expression an try to compile it */` |
|  300731 | 11450 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  300731 | 11451 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 11452 | `					/* Pop l-value */` |
|  300581 | 11453 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  150288 | 11454 | `				}` |
|  150368 | 11455 | `			}else{` |
|       - | 11456 | `				/* Go compile the sucker */` |
|  590707 | 11457 | `				rc = xCons(&(*pGen));` |
|       - | 11458 | `			}` |
|  891433 | 11459 | `			if( rc == SXERR_ABORT ){` |
|       - | 11460 | `				/* Request to abort compilation */` |
|      14 | 11461 | `				break;` |
|       - | 11462 | `			}` |
|       - | 11463 | `		}` |
|       - | 11464 | `		/* Ignore trailing semi-colons ';' */` |
| 1442443 | 11465 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  551009 | 11466 | `			pGen->pIn++;` |
|       5 | 11467 | `		}` |
|  891439 | 11468 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 11469 | `			/* Compile a single statement and return */` |
|  693741 | 11470 | `			break;` |
|       - | 11471 | `		}` |
|       - | 11472 | `		/* LOOP ONE */` |
|       - | 11473 | `		/* LOOP TWO */` |
|       - | 11474 | `		/* LOOP THREE */` |
|       - | 11475 | `		/* LOOP FOUR */` |
|       5 | 11476 | `	}` |
|       - | 11477 | `	/* Return compilation status */` |
|  707527 | 11478 | `	return rc;` |
|       5 | 11479 |  |
|       - | 11480 | `/*` |
|       - | 11481 | ` * Compile a Raw PHP chunk.` |
|       - | 11482 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11483 | ` * takes care of generating the appropriate error message.` |
|       - | 11484 | ` */` |
|   13788 | 11485 | `static sxi32 PH7_CompilePHP(` |
|       - | 11486 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11487 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 11488 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 11489 | `	)` |
|       5 | 11490 |  |
|   13793 | 11491 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 11492 | `	sxi32 rc;` |
|       - | 11493 | `	/* Reset the token set */` |
|   13793 | 11494 | `	SySetReset(&(*pTokenSet));` |
|       - | 11495 | `	/* Mark as the default token set */` |
|   13793 | 11496 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 11497 | `	/* Advance the stream cursor */` |
|   13793 | 11498 | `	pGen->pRawIn++;` |
|       - | 11499 | `	/* Tokenize the PHP chunk first */` |
|   13793 | 11500 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 11501 | `	/* Point to the head and tail of the token stream. */` |
|   13793 | 11502 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   13793 | 11503 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   13793 | 11504 | `	if( is_expr ){` |
|     ! 0 | 11505 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 11506 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 11507 | `			/* A simple expression,compile it */` |
|     ! 0 | 11508 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 11509 | `		}` |
|       - | 11510 | `		/* Emit the DONE instruction */` |
|     ! 0 | 11511 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 11512 | `		return SXRET_OK;` |
|       - | 11513 | `	}` |
|   13793 | 11514 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 11515 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 11516 | `		/*` |
|       - | 11517 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 11518 | `		 * According to the PHP reference manual:` |
|       - | 11519 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 11520 | `		 *  immediately follow` |
|       - | 11521 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 11522 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 11523 | `		 * Symisc extension:` |
|       - | 11524 | `		 *   This short syntax works with all PHP opening` |
|       - | 11525 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 11526 | `		 *   only short tag.` |
|       - | 11527 | `		 */` |
|       - | 11528 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 11529 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 11530 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11531 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11532 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11533 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11534 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11535 | `		}` |
|       3 | 11536 | `		return SXRET_OK;` |
|       - | 11537 | `	}` |
|       - | 11538 | `	/* Compile the PHP chunk */` |
|   13791 | 11539 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11540 | `	/* Fix exceptions jumps */` |
|   13791 | 11541 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11542 | `	/* Fix gotos now, the jump destination is resolved */` |
|   13791 | 11543 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11544 | `		rc = SXERR_ABORT;` |
|       1 | 11545 | `	}` |
|       - | 11546 | `	/* Reset container */` |
|   13791 | 11547 | `	SySetReset(&pGen->aGoto);` |
|   13791 | 11548 | `	SySetReset(&pGen->aLabel);` |
|   13791 | 11549 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11550 | `	/* Compilation result */` |
|   13791 | 11551 | `	return rc;` |
|    6899 | 11552 |  |
|       - | 11553 | `/*` |
|       - | 11554 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11555 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11556 | ` * This is the only compile interface exported from this file.` |
|       - | 11557 | ` */` |
|   16582 | 11558 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11559 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11560 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11561 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11562 | `	)` |
|       5 | 11563 |  |
|       - | 11564 | `	SySet aPhpToken,aRawToken;` |
|       - | 11565 | `	ph7_gen_state *pCodeGen;` |
|       - | 11566 | `	ph7_value *pRawObj;` |
|       - | 11567 | `	sxu32 nObjIdx;` |
|       - | 11568 | `	sxi32 nRawObj;` |
|       - | 11569 | `	int is_expr;` |
|       - | 11570 | `	sxi8 bSavedStrict;` |
|       - | 11571 | `	sxi8 bSavedStrictLocked;` |
|       - | 11572 | `	sxi32 rc;` |
|   16587 | 11573 | `	if( pScript->nByte < 1 ){` |
|       - | 11574 | `		/* Nothing to compile */` |
|     ! 0 | 11575 | `		return PH7_OK;` |
|       - | 11576 | `	}` |
|       - | 11577 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11578 | `	 * file's flags so include/require restore them on return. */` |
|   16587 | 11579 | `	pCodeGen = &pVm->sCodeGen;` |
|   16587 | 11580 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   16587 | 11581 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   16587 | 11582 | `	pCodeGen->bStrictTypes = 0;` |
|   16587 | 11583 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11584 | `	/* Initialize the tokens containers */` |
|   16587 | 11585 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16587 | 11586 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16587 | 11587 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   16587 | 11588 | `	is_expr = 0;` |
|   16587 | 11589 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11590 | `		SyToken sTmp;` |
|       - | 11591 | `		/* PHP only: -*/` |
|    3379 | 11592 | `		sTmp.nLine = 1;` |
|    3379 | 11593 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3379 | 11594 | `		sTmp.pUserData = 0;` |
|    3379 | 11595 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3379 | 11596 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3379 | 11597 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11598 | `			/* A simple PHP expression */` |
|     ! 0 | 11599 | `			is_expr = 1;` |
|     ! 0 | 11600 | `		}` |
|    1692 | 11601 | `	}else{` |
|       - | 11602 | `		/* Tokenize raw text */` |
|   13213 | 11603 | `		SySetAlloc(&aRawToken,32);` |
|   13213 | 11604 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11605 | `	}` |
|       - | 11606 | `	/* Process high-level tokens */` |
|   16587 | 11607 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   16587 | 11608 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   16587 | 11609 | `	rc = PH7_OK;` |
|   16587 | 11610 | `	if( is_expr ){` |
|       - | 11611 | `		/* Compile the expression */` |
|     ! 0 | 11612 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 11613 | `		goto cleanup;` |
|       - | 11614 | `	}` |
|   16587 | 11615 | `	nObjIdx = 0;` |
|       - | 11616 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 11617 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 11618 | `	 * preventing namespace bleeding across include()d files. */` |
|   16587 | 11619 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 11620 | `	/* Start the compilation process */` |
|   14901 | 11621 | `	for(;;){` |
|   43583 | 11622 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   16575 | 11623 | `			break; /* No more tokens to process */` |
|       - | 11624 | `		}` |
|   27013 | 11625 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 11626 | `			/* Compile the PHP chunk */` |
|   13793 | 11627 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   13793 | 11628 | `			if( rc == SXERR_ABORT ){` |
|      16 | 11629 | `				break;` |
|       - | 11630 | `			}` |
|   13781 | 11631 | `			continue;` |
|       - | 11632 | `		}` |
|       - | 11633 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13225 | 11634 | `		nRawObj = 0;` |
|   26487 | 11635 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 11636 | `			/* Consume the raw chunk without any processing */` |
|   13267 | 11637 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13267 | 11638 | `			if( pRawObj == 0 ){` |
|     ! 0 | 11639 | `				rc = SXERR_MEM;` |
|     ! 0 | 11640 | `				break;` |
|       - | 11641 | `			}` |
|       - | 11642 | `			/* Mark as constant and emit the load constant instruction */` |
|   13267 | 11643 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13267 | 11644 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13267 | 11645 | `			++nRawObj;` |
|   13267 | 11646 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 11647 | `		}` |
|   13225 | 11648 | `		if( nRawObj > 0 ){` |
|       - | 11649 | `			/* Emit the consume instruction */` |
|   13225 | 11650 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6610 | 11651 | `		}` |
|    8296 | 11652 | `	}` |
|    8291 | 11653 | `cleanup:` |
|   16587 | 11654 | `	SySetRelease(&aRawToken);` |
|   16587 | 11655 | `	SySetRelease(&aPhpToken);` |
|       - | 11656 | `	/* Restore outer file's strict_types scope */` |
|   16587 | 11657 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   16587 | 11658 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   16587 | 11659 | `	return rc;` |
|    8296 | 11660 |  |
|       - | 11661 | `/*` |
|       - | 11662 | ` * Utility routines.Initialize the code generator.` |
|       - | 11663 | ` */` |
|    3306 | 11664 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 11665 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11666 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11667 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11668 | `	)` |
|       5 | 11669 |  |
|    3311 | 11670 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11671 | `	/* Zero the structure */` |
|    3311 | 11672 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 11673 | `	/* Initial state */` |
|    3311 | 11674 | `	pGen->pVm  = &(*pVm);` |
|    3311 | 11675 | `	pGen->xErr = xErr;` |
|    3311 | 11676 | `	pGen->pErrData = pErrData;` |
|    3311 | 11677 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3311 | 11678 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3311 | 11679 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3311 | 11680 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3311 | 11681 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 11682 | `	/* Error log buffer */` |
|    3311 | 11683 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 11684 | `	/* General purpose working buffer */` |
|    3311 | 11685 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 11686 | `	/* Namespace state */` |
|    3311 | 11687 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3311 | 11688 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3311 | 11689 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3311 | 11690 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11691 | `	/* Create the global scope */` |
|    3311 | 11692 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 11693 | `	/* Point to the global scope */` |
|    3311 | 11694 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3311 | 11695 | `	return SXRET_OK;` |
|       5 | 11696 |  |
|       - | 11697 | `/*` |
|       - | 11698 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 11699 | ` */` |
|   19548 | 11700 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 11701 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11702 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11703 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11704 | `	)` |
|       5 | 11705 |  |
|   19553 | 11706 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11707 | `	GenBlock *pBlock,*pParent;` |
|       - | 11708 | `	/* Reset state */` |
|   19553 | 11709 | `	SySetReset(&pGen->aLabel);` |
|   19553 | 11710 | `	SySetReset(&pGen->aGoto);` |
|   19553 | 11711 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   19553 | 11712 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   19553 | 11713 | `	SyBlobRelease(&pGen->sWorker);` |
|   19553 | 11714 | `	SyBlobRelease(&pGen->sNamespace);` |
|   19553 | 11715 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   19553 | 11716 | `	SyHashRelease(&pGen->hUseImports);` |
|   19553 | 11717 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   19553 | 11718 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   19553 | 11719 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   19553 | 11720 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   19553 | 11721 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11722 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 11723 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 11724 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 11725 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 11726 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 11727 | `	 * number of unique names, which is acceptable. */` |
|       - | 11728 | `	/* Point to the global scope */` |
|   19553 | 11729 | `	pBlock = pGen->pCurrent;` |
|   19553 | 11730 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 11731 | `		pParent = pBlock->pParent;` |
|     ! 0 | 11732 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 11733 | `		pBlock = pParent;` |
|     ! 0 | 11734 | `	}` |
|   19553 | 11735 | `	pGen->xErr = xErr;` |
|   19553 | 11736 | `	pGen->pErrData = pErrData;` |
|   19553 | 11737 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   19553 | 11738 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   19553 | 11739 | `	pGen->pIn = pGen->pEnd = 0;` |
|   19553 | 11740 | `	pGen->nErr = 0;` |
|   19553 | 11741 | `	return SXRET_OK;` |
|       5 | 11742 |  |
|       - | 11743 | `/*` |
|       - | 11744 | ` * Generate a compile-time error message.` |
|       - | 11745 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 11746 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 11747 | ` * abort compilation immediately.` |
|       - | 11748 | ` */` |
|     602 | 11749 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 11750 |  |
|     607 | 11751 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     607 | 11752 | `	const char *zErr = "Error";` |
|       - | 11753 | `	SyString *pFile;` |
|       - | 11754 | `	va_list ap;` |
|       - | 11755 | `	sxi32 rc;` |
|       - | 11756 | `	/* Reset the working buffer */` |
|     607 | 11757 | `	SyBlobReset(pWorker);` |
|       - | 11758 | `	/* Peek the processed file path if available */` |
|     607 | 11759 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     607 | 11760 | `	if( nErrType == E_ERROR ){` |
|       - | 11761 | `		/* Increment the error counter */` |
|     501 | 11762 | `		pGen->nErr++;` |
|     501 | 11763 | `		if( pGen->nErr > 15 ){` |
|       - | 11764 | `			/* Error count limit reached */` |
|       6 | 11765 | `			if( pGen->xErr ){` |
|       6 | 11766 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       6 | 11767 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       6 | 11768 | `				if( pFile ){` |
|       6 | 11769 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 11770 | `				}` |
|       6 | 11771 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       6 | 11772 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       6 | 11773 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 11774 | `				}` |
|       2 | 11775 | `			}` |
|       - | 11776 | `			/* Abort immediately */` |
|       6 | 11777 | `			return SXERR_ABORT;` |
|       - | 11778 | `		}` |
|     246 | 11779 | `	}` |
|     603 | 11780 | `	if( pGen->xErr == 0 ){` |
|       - | 11781 | `		/* No available error consumer,return immediately */` |
|       3 | 11782 | `		return SXRET_OK;` |
|       - | 11783 | `	}` |
|     600 | 11784 | `	switch(nErrType){` |
|     494 | 11785 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 11786 | `	case E_WARNING: zErr = "Warning";     break;` |
|      76 | 11787 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      12 | 11788 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 11789 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 11790 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 11791 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 11792 | `	default:` |
|     ! 0 | 11793 | `		break;` |
|       - | 11794 | `	}` |
|     600 | 11795 | `	rc = SXRET_OK;` |
|       - | 11796 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     600 | 11797 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     600 | 11798 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     600 | 11799 | `	va_start(ap,zFormat);` |
|     600 | 11800 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     600 | 11801 | `	va_end(ap);` |
|     600 | 11802 | `	if( pFile ){` |
|     600 | 11803 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     298 | 11804 | `	}` |
|       - | 11805 | `	/* Append a new line */` |
|     600 | 11806 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     600 | 11807 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 11808 | `		/* Consume the generated error message */` |
|     600 | 11809 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     298 | 11810 | `	}` |
|     600 | 11811 | `	return rc;` |
|     306 | 11812 |  |
|       - | 11813 |  |
