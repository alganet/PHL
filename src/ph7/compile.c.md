# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5673/7045 lines (80.53%)

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
|      92 |   122 | `	}` |
|       - |   123 | `	/* No such destination */` |
|      59 |   124 | `	return SXERR_NOTFOUND;` |
|      79 |   125 |  |
|       - |   126 | `/*` |
|       - |   127 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|       - |   128 | ` * compiled blocks.` |
|       - |   129 | ` * Return a pointer to that block on success. NULL otherwise.` |
|       - |   130 | ` */` |
|    3816 |   131 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   132 |  |
|    3821 |   133 | `	GenBlock *pBlock = pCurrent;` |
|   10865 |   134 | `	for(;;){` |
|   21735 |   135 | `		if( pBlock->iFlags & iBlockType ){` |
|    3713 |   136 | `			iCount--; /* Decrement nesting level */` |
|    3713 |   137 | `			if( iCount < 1 ){` |
|       - |   138 | `				/* Block meet with the desired criteria */` |
|    3687 |   139 | `				return pBlock;` |
|       - |   140 | `			}` |
|      13 |   141 | `		}` |
|       - |   142 | `		/* Point to the upper block */` |
|   18053 |   143 | `		pBlock = pBlock->pParent;` |
|   18053 |   144 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   145 | `			/* Forbidden */` |
|      72 |   146 | `			break;` |
|       - |   147 | `		}` |
|       5 |   148 | `	}` |
|       - |   149 | `	/* No such block */` |
|     139 |   150 | `	return 0;` |
|    1913 |   151 |  |
|       - |   152 | `/*` |
|       - |   153 | ` * Initialize a freshly allocated block instance.` |
|       - |   154 | ` */` |
|  807588 |   155 | `static void GenStateInitBlock(` |
|       - |   156 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   157 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   158 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   159 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   160 | `	void *pUserData      /* Upper layer private data */` |
|       - |   161 | `	)` |
|       5 |   162 |  |
|       - |   163 | `	/* Initialize block fields */` |
|  807593 |   164 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  807593 |   165 | `	pBlock->pUserData   = pUserData;` |
|  807593 |   166 | `	pBlock->pGen        = pGen;` |
|  807593 |   167 | `	pBlock->iFlags      = iType;` |
|  807593 |   168 | `	pBlock->pParent     = 0;` |
|  807593 |   169 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  807593 |   170 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  807593 |   171 |  |
|       - |   172 | `/*` |
|       - |   173 | ` * Allocate a new block instance.` |
|       - |   174 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   175 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   176 | ` * processing on failure.` |
|       - |   177 | ` */` |
|  804050 |   178 | `static sxi32 GenStateEnterBlock(` |
|       - |   179 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   180 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   181 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   182 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   183 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   184 | `	)` |
|       5 |   185 |  |
|       - |   186 | `	GenBlock *pBlock;` |
|       - |   187 | `	/* Allocate a new block instance */` |
|  804055 |   188 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  804055 |   189 | `	if( pBlock == 0 ){` |
|       - |   190 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   191 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   192 | `		 */` |
|     ! 0 |   193 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   194 | `		/* Abort processing immediately */` |
|     ! 0 |   195 | `		return SXERR_ABORT;` |
|       - |   196 | `	}` |
|       - |   197 | `	/* Zero the structure */` |
|  804055 |   198 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  804055 |   199 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   200 | `	/* Link to the parent block */` |
|  804055 |   201 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   202 | `	/* Mark as the current block */` |
|  804055 |   203 | `	pGen->pCurrent = pBlock;` |
|  804055 |   204 | `	if( ppBlock ){` |
|       - |   205 | `		/* Write a pointer to the new instance */` |
|  390149 |   206 | `		*ppBlock = pBlock;` |
|  195072 |   207 | `	}` |
|  804055 |   208 | `	return SXRET_OK;` |
|  402030 |   209 |  |
|       - |   210 | `/*` |
|       - |   211 | ` * Release block fields without freeing the whole instance.` |
|       - |   212 | ` */` |
|  804042 |   213 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   214 |  |
|  804047 |   215 | `	SySetRelease(&pBlock->aPostContFix);` |
|  804047 |   216 | `	SySetRelease(&pBlock->aJumpFix);` |
|  804047 |   217 |  |
|       - |   218 | `/*` |
|       - |   219 | ` * Release a block.` |
|       - |   220 | ` */` |
|  804042 |   221 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   222 |  |
|  804047 |   223 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  804047 |   224 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   225 | `	/* Free the instance */` |
|  804047 |   226 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  804047 |   227 |  |
|       - |   228 | `/*` |
|       - |   229 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   230 | ` */` |
|  804042 |   231 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   232 |  |
|  804047 |   233 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  804047 |   234 | `	if( pBlock == 0 ){` |
|       - |   235 | `		/* No more block to pop */` |
|     ! 0 |   236 | `		return SXERR_EMPTY;` |
|       - |   237 | `	}` |
|       - |   238 | `	/* Point to the upper block */` |
|  804047 |   239 | `	pGen->pCurrent = pBlock->pParent;` |
|  804047 |   240 | `	if( ppBlock ){` |
|       - |   241 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   242 | `		*ppBlock = pBlock;` |
|     ! 0 |   243 | `	}else{` |
|       - |   244 | `		/* Safely release the block */` |
|  804047 |   245 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   246 | `	}` |
|  804047 |   247 | `	return SXRET_OK;` |
|  402026 |   248 |  |
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
|  239802 |   259 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   260 |  |
|       - |   261 | `	JumpFixup sJumpFix;` |
|       - |   262 | `	sxi32 rc;` |
|       - |   263 | `	/* Init the JumpFixup structure */` |
|  239807 |   264 | `	sJumpFix.nJumpType = nJumpType;` |
|  239807 |   265 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   266 | `	/* Insert in the jump fixup table */` |
|  239807 |   267 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  239807 |   268 | `	return rc;` |
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
|  566966 |   281 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   282 |  |
|       - |   283 | `	JumpFixup *aFix;` |
|       - |   284 | `	VmInstr *pInstr;` |
|       - |   285 | `	sxu32 nFixed;` |
|       - |   286 | `	sxu32 n;` |
|       - |   287 | `	/* Point to the jump fixup table */` |
|  566971 |   288 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   289 | `	/* Fix the desired jumps */` |
| 1035837 |   290 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  468871 |   291 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   292 | `			/* Already fixed */` |
|  185477 |   293 | `			continue;` |
|       - |   294 | `		}` |
|  283399 |   295 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   296 | `			/* Not of our interest */` |
|   43599 |   297 | `			continue;` |
|       - |   298 | `		}` |
|       - |   299 | `		/* Point to the instruction to fix */` |
|  239805 |   300 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  239805 |   301 | `		if( pInstr ){` |
|  239805 |   302 | `			pInstr->iP2 = nJumpDest;` |
|  239805 |   303 | `			nFixed++;` |
|       - |   304 | `			/* Mark as fixed */` |
|  239805 |   305 | `			aFix[n].nJumpType = -1;` |
|  119900 |   306 | `		}` |
|  119905 |   307 | `	}` |
|       - |   308 | `	/* Total number of fixed jumps */` |
|  566971 |   309 | `	return nFixed;` |
|       5 |   310 |  |
|       - |   311 | `/*` |
|       - |   312 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   313 | ` * The goto statement can be used to jump to another section` |
|       - |   314 | ` * in the program.` |
|       - |   315 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   316 | ` * statement for more information.` |
|       - |   317 | ` */` |
|  221776 |   318 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   319 |  |
|       - |   320 | `	JumpFixup *pJump,*aJumps;` |
|       - |   321 | `	Label *pLabel,*aLabel;` |
|       - |   322 | `	VmInstr *pInstr;` |
|       - |   323 | `	sxi32 rc;` |
|       - |   324 | `	sxu32 n;` |
|       - |   325 | `	/* Point to the goto table */` |
|  221781 |   326 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   327 | `	/* Fix */` |
|  221927 |   328 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|     153 |   329 | `		pJump = &aJumps[n];` |
|       - |   330 | `		/* Extract the target label */` |
|     153 |   331 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|     153 |   332 | `		if( rc != SXRET_OK ){` |
|       - |   333 | `			/* No such label */` |
|      59 |   334 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|      59 |   335 | `			if( rc == SXERR_ABORT ){` |
|       3 |   336 | `				return SXERR_ABORT;` |
|       - |   337 | `			}` |
|      57 |   338 | `			continue;` |
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
|  221779 |   353 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  221911 |   354 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   355 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   356 | `			/* Emit a warning */` |
|      40 |   357 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   358 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   359 | `		}` |
|      71 |   360 | `	}` |
|  221779 |   361 | `	return SXRET_OK;` |
|  110893 |   362 |  |
|       - |   363 | `/*` |
|       - |   364 | ` * Check if a given token value is installed in the literal table.` |
|       - |   365 | ` */` |
|  735762 |   366 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   367 |  |
|       - |   368 | `	SyHashEntry *pEntry;` |
|  735767 |   369 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  735767 |   370 | `	if( pEntry == 0 ){` |
|  328493 |   371 | `		return SXERR_NOTFOUND;` |
|       - |   372 | `	}` |
|  407279 |   373 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  407279 |   374 | `	return SXRET_OK;` |
|  367886 |   375 |  |
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
|  328488 |   386 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   387 |  |
|  328493 |   388 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  328493 |   389 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  164244 |   390 | `	}` |
|  328493 |   391 | `	return SXRET_OK;` |
|       5 |   392 |  |
|       - |   393 | `/*` |
|       - |   394 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   395 | ` * in the constant table.` |
|       - |   396 | ` */` |
|  124148 |   397 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   398 |  |
|       - |   399 | `	ph7_value *pObj;` |
|  124153 |   400 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   401 | `	/* Reserve a new constant */` |
|  124153 |   402 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  124153 |   403 | `	if( pObj == 0 ){` |
|     ! 0 |   404 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   405 | `		return 0;` |
|       - |   406 | `	}` |
|  124153 |   407 | `	*pIdx = nIdx;` |
|       - |   408 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   409 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   410 | `	 */` |
|  124153 |   411 | `	return pObj;` |
|   62079 |   412 |  |
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
|  453444 |   427 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   428 |  |
|       - |   429 | `	VmCallArgMap *pMap;` |
|  453449 |   430 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   431 | `	if( p3 == 0 ){` |
|      31 |   432 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   433 | `		if( pMap == 0 ) return 0;` |
|      31 |   434 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   435 | `		p3 = (void *)pMap;` |
|      14 |   436 | `	}` |
|      33 |   437 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   438 | `	return p3;` |
|  226727 |   439 |  |
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
|  124812 |   498 | `static int GenStateFindBadNumericSeparator(` |
|       - |   499 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   500 |  |
|  124817 |   501 | `	const char *z = pRaw->zString;` |
|  124817 |   502 | `	sxu32 n = pRaw->nByte;` |
|  124817 |   503 | `	int base = 10;` |
|       - |   504 | `	sxu32 i, start;` |
|  124817 |   505 | `	if( n < 2 ) return 0;` |
|   10365 |   506 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   507 | `		base = 16;` |
|   10330 |   508 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   509 | `		base = 2;` |
|     139 |   510 | `	}` |
|   37473 |   511 | `	for( i = 0; i < n; ++i ){` |
|   27127 |   512 | `		if( z[i] != '_' ) continue;` |
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
|   10351 |   529 | `	return 0;` |
|   62411 |   530 |  |
|       - |   531 | `/*` |
|       - |   532 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   533 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   534 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   535 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   536 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   537 | ` * so callers can bail from the current construct).` |
|       - |   538 | ` */` |
|  124812 |   539 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   540 |  |
|  124817 |   541 | `	const char *zBad = 0;` |
|  124817 |   542 | `	sxu32 nBad = 0;` |
|       - |   543 | `	SyString sBad;` |
|       - |   544 | `	sxi32 rc;` |
|  124817 |   545 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  124803 |   546 | `		return SXRET_OK;` |
|       - |   547 | `	}` |
|      18 |   548 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   549 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   550 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   551 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   552 | `		return SXERR_ABORT;` |
|       - |   553 | `	}` |
|      18 |   554 | `	return SXERR_SYNTAX;` |
|   62411 |   555 |  |
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
|  124798 |   572 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   573 | `	SyMemBackend *pAlloc,` |
|       - |   574 | `	const SyString *pToken,` |
|       - |   575 | `	char *zScratch, sxu32 nScratch,` |
|       - |   576 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   577 |  |
|       - |   578 | `	sxu32 i, j;` |
|  124803 |   579 | `	int hasUnderscore = 0;` |
|       - |   580 | `	char *zBuf;` |
|  124803 |   581 | `	*pzAlloc = 0;` |
|  264297 |   582 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  139751 |   583 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   69752 |   584 | `	}` |
|  124803 |   585 | `	if( !hasUnderscore ){` |
|  124551 |   586 | `		SyStringDupPtr(pOut, pToken);` |
|  124551 |   587 | `		return SXRET_OK;` |
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
|   62404 |   604 |  |
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
|  124784 |   621 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   622 |  |
|  124789 |   623 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  124789 |   624 | `	sxu32 nIdx = 0;` |
|       - |   625 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  124789 |   626 | `	char *zAlloc = 0;` |
|       - |   627 | `	SyString sNum;` |
|       - |   628 | `	sxi32 rc;` |
|   62392 |   629 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  124789 |   630 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  124789 |   631 | `	if( rc != SXRET_OK ){` |
|      14 |   632 | `		return rc;` |
|       - |   633 | `	}` |
|  187166 |   634 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   62387 |   635 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  124779 |   636 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   637 | `		return SXERR_ABORT;` |
|       - |   638 | `	}` |
|  124779 |   639 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   640 | `		ph7_value *pObj;` |
|       - |   641 | `		sxi64 iValue;` |
|  124153 |   642 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  124153 |   643 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  124153 |   644 | `		if( pObj == 0 ){` |
|     ! 0 |   645 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   646 | `			return SXERR_ABORT;` |
|       - |   647 | `		}` |
|  124153 |   648 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   62079 |   649 | `	}else{` |
|       - |   650 | `		/* Real number */` |
|       - |   651 | `		ph7_value *pObj;` |
|       - |   652 | `		/* Reserve a new constant */` |
|     630 |   653 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     630 |   654 | `		if( pObj == 0 ){` |
|     ! 0 |   655 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   656 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   657 | `			return SXERR_ABORT;` |
|       - |   658 | `		}` |
|     630 |   659 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     630 |   660 | `		PH7_MemObjToReal(pObj);` |
|       - |   661 | `	}` |
|  124779 |   662 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   663 | `	/* Emit the load constant instruction */` |
|  124779 |   664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   665 | `	/* Node successfully compiled */` |
|  124779 |   666 | `	return SXRET_OK;` |
|   62397 |   667 |  |
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
|   92726 |   679 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   680 |  |
|   92731 |   681 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   682 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   683 | `	ph7_value *pObj;` |
|       - |   684 | `	sxu32 nIdx;` |
|   92731 |   685 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   686 | `	/* Delimit the string */` |
|   92731 |   687 | `	zIn  = pStr->zString;` |
|   92731 |   688 | `	zEnd = &zIn[pStr->nByte];` |
|   92731 |   689 | `	if( zIn >= zEnd ){` |
|       - |   690 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   691 | `		 * rather than reserving a new object each time. */` |
|    7247 |   692 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7247 |   693 | `		return SXRET_OK;` |
|       - |   694 | `	}` |
|   85489 |   695 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   696 | `		/* Already processed,emit the load constant instruction` |
|       - |   697 | `		 * and return.` |
|       - |   698 | `		 */` |
|   32271 |   699 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   32271 |   700 | `		return SXRET_OK;` |
|       - |   701 | `	}` |
|       - |   702 | `	/* Reserve a new constant */` |
|   53223 |   703 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   53223 |   704 | `	if( pObj == 0 ){` |
|     ! 0 |   705 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   706 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   707 | `		return SXERR_ABORT;` |
|       - |   708 | `	}` |
|   53223 |   709 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   710 | `	/* Compile the node */` |
|   53273 |   711 | `	for(;;){` |
|  106551 |   712 | `		if( zIn >= zEnd ){` |
|       - |   713 | `			/* End of input */` |
|   53223 |   714 | `			break;` |
|       - |   715 | `		}` |
|   53333 |   716 | `		zCur = zIn;` |
|  947063 |   717 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  893735 |   718 | `			zIn++;` |
|       5 |   719 | `		}` |
|   53333 |   720 | `		if( zIn > zCur ){` |
|       - |   721 | `			/* Append raw contents*/` |
|   53309 |   722 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   26652 |   723 | `		}` |
|   53333 |   724 | `		zIn++;` |
|   53333 |   725 | `		if( zIn < zEnd ){` |
|     132 |   726 | `			if( zIn[0] == '\\' ){` |
|       - |   727 | `				/* A literal backslash */` |
|      23 |   728 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     121 |   729 | `			}else if( zIn[0] == '\'' ){` |
|       - |   730 | `				/* A single quote */` |
|      11 |   731 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   732 | `			}else{` |
|       - |   733 | `				/* verbatim copy */` |
|     100 |   734 | `				zIn--;` |
|     100 |   735 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     100 |   736 | `				zIn++;` |
|       - |   737 | `			}` |
|      65 |   738 | `		}` |
|       - |   739 | `		/* Advance the stream cursor */` |
|   53333 |   740 | `		zIn++;` |
|       5 |   741 | `	}` |
|       - |   742 | `	/* Emit the load constant instruction */` |
|   53223 |   743 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   53223 |   744 | `	if( pStr->nByte < 1024 ){` |
|       - |   745 | `		/* Install in the literal table */` |
|   53223 |   746 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   26609 |   747 | `	}` |
|       - |   748 | `	/* Node successfully compiled */` |
|   53223 |   749 | `	return SXRET_OK;` |
|   46368 |   750 |  |
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
|       4 |   770 |  |
|     114 |   771 | `	SyString *pIn = &pGen->pIn->sData;` |
|     114 |   772 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   773 | `	const char *zPrefix;` |
|       - |   774 | `	const char *z, *zEnd;` |
|       - |   775 | `	char *zBuf, *zDst;` |
|     114 |   776 | `	if( nIndent == 0 ){` |
|       - |   777 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      68 |   778 | `		*pOut = *pIn;` |
|      68 |   779 | `		return SXRET_OK;` |
|       - |   780 | `	}` |
|       - |   781 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |   782 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |   783 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |   784 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |   785 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |   786 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      47 |   787 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      47 |   788 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   789 | `		zPrefix += 2;` |
|     ! 0 |   790 | `	}else{` |
|      47 |   791 | `		zPrefix += 1;` |
|       - |   792 | `	}` |
|       - |   793 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      47 |   794 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      47 |   795 | `	if( zBuf == 0 ){` |
|     ! 0 |   796 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   797 | `		return SXERR_ABORT;` |
|       - |   798 | `	}` |
|      47 |   799 | `	zDst = zBuf;` |
|      47 |   800 | `	z = pIn->zString;` |
|      47 |   801 | `	zEnd = z + pIn->nByte;` |
|     129 |   802 | `	while( z < zEnd ){` |
|      71 |   803 | `		const char *zLine = z;` |
|       - |   804 | `		sxu32 nLine;` |
|       - |   805 | `		int bEmpty;` |
|     799 |   806 | `		while( z < zEnd && z[0] != '\n' ){` |
|     731 |   807 | `			z++;` |
|       3 |   808 | `		}` |
|      71 |   809 | `		nLine = (sxu32)(z - zLine);` |
|      71 |   810 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      71 |   811 | `		if( !bEmpty ){` |
|       - |   812 | `			sxu32 i;` |
|      67 |   813 | `			if( nLine < nIndent ){` |
|     ! 0 |   814 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   815 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   816 | `					nIndent);` |
|     ! 0 |   817 | `				return SXERR_ABORT;` |
|       - |   818 | `			}` |
|     269 |   819 | `			for( i = 0; i < nIndent; i++ ){` |
|     213 |   820 | `				if( zLine[i] != zPrefix[i] ){` |
|      10 |   821 | `					unsigned char c = (unsigned char)zLine[i];` |
|      10 |   822 | `					if( c == ' ' \|\| c == '\t' ){` |
|       5 |   823 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   824 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       3 |   825 | `					}else{` |
|       7 |   826 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   827 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   828 | `							nIndent);` |
|       - |   829 | `					}` |
|      10 |   830 | `					return SXERR_ABORT;` |
|       - |   831 | `				}` |
|     103 |   832 | `			}` |
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
|    2222 |   916 | `static sxi32 GenStateProcessStringExpression(` |
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
|    2227 |   927 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   928 | `	/* Preallocate some slots */` |
|    2227 |   929 | `	SySetAlloc(&sToken,0x08);` |
|       - |   930 | `	/* Tokenize the text */` |
|    2227 |   931 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   932 | `	/* Swap delimiter */` |
|    2227 |   933 | `	pTmpIn  = pGen->pIn;` |
|    2227 |   934 | `	pTmpEnd = pGen->pEnd;` |
|    2227 |   935 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2227 |   936 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   937 | `	/* Compile the expression */` |
|    2227 |   938 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   939 | `	/* Restore token stream */` |
|    2227 |   940 | `	pGen->pIn  = pTmpIn;` |
|    2227 |   941 | `	pGen->pEnd = pTmpEnd;` |
|       - |   942 | `	/* Release the token set */` |
|    2227 |   943 | `	SySetRelease(&sToken);` |
|       - |   944 | `	/* Compilation result */` |
|    2227 |   945 | `	return rc;` |
|       5 |   946 |  |
|       - |   947 | `/*` |
|       - |   948 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   949 | ` */` |
|   24670 |   950 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   951 |  |
|       - |   952 | `	ph7_value *pConstObj;` |
|   24675 |   953 | `	sxu32 nIdx = 0;` |
|       - |   954 | `	/* Reserve a new constant */` |
|   24675 |   955 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   24675 |   956 | `	if( pConstObj == 0 ){` |
|     ! 0 |   957 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   958 | `		return 0;` |
|       - |   959 | `	}` |
|   24675 |   960 | `	(*pCount)++;` |
|   24675 |   961 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   962 | `	/* Emit the load constant instruction */` |
|   24675 |   963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   24675 |   964 | `	return pConstObj;` |
|   12340 |   965 |  |
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
|   23202 |  1004 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1005 |  |
|   23207 |  1006 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1007 | `	const char *zIn,*zCur,*zEnd;` |
|   23207 |  1008 | `	ph7_value *pObj = 0;` |
|       - |  1009 | `	sxi32 iCons;` |
|       - |  1010 | `	sxi32 rc;` |
|       - |  1011 | `	/* Delimit the string */` |
|   23207 |  1012 | `	zIn  = pStr->zString;` |
|   23207 |  1013 | `	zEnd = &zIn[pStr->nByte];` |
|   23207 |  1014 | `	if( zIn >= zEnd ){` |
|       - |  1015 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1016 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1017 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1018 | `		 */` |
|     313 |  1019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     313 |  1020 | `		return SXRET_OK;` |
|       - |  1021 | `	}` |
|   22899 |  1022 | `	zCur = 0;` |
|       - |  1023 | `	/* Compile the node */` |
|   22899 |  1024 | `	iCons = 0;` |
|   12558 |  1025 | `	for(;;){` |
|   37581 |  1026 | `		zCur = zIn;` |
|  177185 |  1027 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  141831 |  1028 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1029 | `				break;` |
|  141707 |  1030 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2102 |  1031 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1052 |  1032 | `					break;` |
|       - |  1033 | `			}` |
|  139609 |  1034 | `			zIn++;` |
|       5 |  1035 | `		}` |
|   37581 |  1036 | `		if( zIn > zCur ){` |
|   17515 |  1037 | `			if( pObj == 0 ){` |
|   17041 |  1038 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   17041 |  1039 | `				if( pObj == 0 ){` |
|     ! 0 |  1040 | `					return SXERR_ABORT;` |
|       - |  1041 | `				}` |
|    8518 |  1042 | `			}` |
|   17515 |  1043 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    8755 |  1044 | `		}` |
|   37581 |  1045 | `		if( zIn >= zEnd ){` |
|   22899 |  1046 | `			break;` |
|       - |  1047 | `		}` |
|   14687 |  1048 | `		if( zIn[0] == '\\' ){` |
|   12465 |  1049 | `			const char *zPtr = 0;` |
|       - |  1050 | `			sxu32 n;` |
|   12465 |  1051 | `			zIn++;` |
|   12465 |  1052 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1053 | `				break;` |
|       - |  1054 | `			}` |
|   12465 |  1055 | `			if( pObj == 0 ){` |
|    7639 |  1056 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    7639 |  1057 | `				if( pObj == 0 ){` |
|     ! 0 |  1058 | `					return SXERR_ABORT;` |
|       - |  1059 | `				}` |
|    3817 |  1060 | `			}` |
|   12465 |  1061 | `			n = sizeof(char); /* size of conversion */` |
|   12465 |  1062 | `			switch( zIn[0] ){` |
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
|    5747 |  1083 | `			case 'n':` |
|       - |  1084 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   11499 |  1085 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   11499 |  1086 | `				break;` |
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
|   12465 |  1154 | `			zIn += n;` |
|   12465 |  1155 | `			continue;` |
|       - |  1156 | `		}` |
|    2227 |  1157 | `		if( zIn[0] == '{' ){` |
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
|    2099 |  1191 | `			const char *zExpr = zIn;` |
|       - |  1192 | `			/* Assemble variable name */` |
|    1056 |  1193 | `			for(;;){` |
|       - |  1194 | `				/* Jump leading dollars */` |
|    4211 |  1195 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2099 |  1196 | `					zIn++;` |
|       5 |  1197 | `				}` |
|    1056 |  1198 | `				for(;;){` |
|   11723 |  1199 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8555 |  1200 | `						zIn++;` |
|       5 |  1201 | `					}` |
|    2117 |  1202 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1203 | `						/* UTF-8 stream */` |
|     ! 0 |  1204 | `						zIn++;` |
|     ! 0 |  1205 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1206 | `							zIn++;` |
|     ! 0 |  1207 | `						}` |
|     ! 0 |  1208 | `						continue;` |
|       - |  1209 | `					}` |
|    2117 |  1210 | `					break;` |
|     ! 0 |  1211 | `				}` |
|    2117 |  1212 | `				if( zIn >= zEnd ){` |
|     199 |  1213 | `					break;` |
|       - |  1214 | `				}` |
|    1923 |  1215 | `				if( zIn[0] == '[' ){` |
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
|    1913 |  1233 | `				}else if(zIn[0] == '{' ){` |
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
|    1909 |  1251 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1252 | `					/* Member access operator '->' */` |
|      21 |  1253 | `					zIn += 2;` |
|    1900 |  1254 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1255 | `					/* Static member access operator '::' */` |
|     ! 0 |  1256 | `					zIn += 2;` |
|     ! 0 |  1257 | `				}else{` |
|     948 |  1258 | `					break;` |
|       - |  1259 | `				}` |
|       3 |  1260 | `			}` |
|       - |  1261 | `			/* Process the expression */` |
|    2099 |  1262 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2099 |  1263 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1264 | `				return SXERR_ABORT;` |
|       - |  1265 | `			}` |
|    2099 |  1266 | `			if( rc != SXERR_EMPTY ){` |
|    2097 |  1267 | `				++iCons;` |
|    1046 |  1268 | `			}` |
|       - |  1269 | `		}` |
|       - |  1270 | `		/* Invalidate the previously used constant */` |
|    2227 |  1271 | `		pObj = 0;` |
|       5 |  1272 | `	}/*for(;;)*/` |
|   22899 |  1273 | `	if( iCons > 1 ){` |
|       - |  1274 | `		/* Concatenate all compiled constants */` |
|    1661 |  1275 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     828 |  1276 | `	}` |
|       - |  1277 | `	/* Node successfully compiled */` |
|   22899 |  1278 | `	return SXRET_OK;` |
|   11606 |  1279 |  |
|       - |  1280 | `/*` |
|       - |  1281 | ` * Compile a double quoted string.` |
|       - |  1282 | ` *  See the block-comment above for more information.` |
|       - |  1283 | ` */` |
|   23142 |  1284 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1285 |  |
|       - |  1286 | `	sxi32 rc;` |
|   23147 |  1287 | `	rc = GenStateCompileString(&(*pGen));` |
|   11571 |  1288 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1289 | `	/* Compilation result */` |
|   23147 |  1290 | `	return rc;` |
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
|      62 |  1308 | `	sOrig = pGen->pIn->sData;` |
|      62 |  1309 | `	pGen->pIn->sData = sStripped;` |
|      62 |  1310 | `	rc = GenStateCompileString(&(*pGen));` |
|      62 |  1311 | `	pGen->pIn->sData = sOrig;` |
|      30 |  1312 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      62 |  1313 | `	return rc;` |
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
|   21630 |  1334 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   21635 |  1345 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1346 | `	/* Compile the expression*/` |
|   21635 |  1347 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1348 | `	/* Restore token stream */` |
|   21635 |  1349 | `	RE_SWAP_DELIMITER(pGen);` |
|   21635 |  1350 | `	return rc;` |
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
|       5 |  1361 |  |
|      41 |  1362 | `	sxi32 rc = SXRET_OK;` |
|      41 |  1363 | `	if( pRoot->pOp ){` |
|      14 |  1364 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1365 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      17 |  1366 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1367 | `			/* Unexpected expression */` |
|      14 |  1368 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1369 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      14 |  1370 | `			if( rc != SXERR_ABORT ){` |
|      14 |  1371 | `				rc = SXERR_INVALID;` |
|       5 |  1372 | `			}` |
|      10 |  1373 | `		}` |
|      31 |  1374 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1375 | `		/* Unexpected expression */` |
|       3 |  1376 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1377 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1378 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1379 | `			rc = SXERR_INVALID;` |
|       1 |  1380 | `		}` |
|       1 |  1381 | `	}` |
|      41 |  1382 | `	return rc;` |
|       5 |  1383 |  |
|       - |  1384 | `/*` |
|       - |  1385 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - |  1386 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - |  1387 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - |  1388 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - |  1389 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - |  1390 | ` */` |
|   23958 |  1391 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1392 |  |
|   23963 |  1393 | `	SyToken *pCur = pStart;` |
|   23963 |  1394 | `	sxi32 iNest = 0;` |
|   67809 |  1395 | `	while( pCur < pEnd ){` |
|   49291 |  1396 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5441 |  1397 | `			return pCur;` |
|       - |  1398 | `		}` |
|       - |  1399 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1400 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1401 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1402 | `		 */` |
|   43855 |  1403 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   43849 |  1464 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     326 |  1465 | `			iNest++;` |
|   43688 |  1466 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1467 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1468 | `			 * parser will shortly detect any syntax error. */` |
|     326 |  1469 | `			iNest--;` |
|     161 |  1470 | `		}` |
|   43849 |  1471 | `		pCur++;` |
|       5 |  1472 | `	}` |
|   18523 |  1473 | `	return pEnd;` |
|   11984 |  1474 |  |
|       - |  1475 | `/*` |
|       - |  1476 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1477 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1478 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1479 | ` */` |
|   31104 |  1480 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1481 |  |
|       - |  1482 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1483 | `	SyToken *pKey,*pCur;` |
|   31109 |  1484 | `	sxi32 iEmitRef = 0;` |
|   31109 |  1485 | `	sxi32 iSpread = 0;` |
|   31109 |  1486 | `	sxi32 nPair = 0;` |
|       - |  1487 | `	sxi32 rc;` |
|   31109 |  1488 | `	xValidator = 0;` |
|   25459 |  1489 | `	for(;;){` |
|       - |  1490 | `		/* Jump leading commas */` |
|   57765 |  1491 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    6847 |  1492 | `			pGen->pIn++;` |
|       5 |  1493 | `		}` |
|   50923 |  1494 | `		pCur = pGen->pIn;` |
|   50923 |  1495 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1496 | `			/* No more entry to process */` |
|   31093 |  1497 | `			break;` |
|       - |  1498 | `		}` |
|   19835 |  1499 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1500 | `			continue;` |
|       - |  1501 | `		}` |
|       - |  1502 | `		/* Compile the key if available */` |
|   19835 |  1503 | `		pKey = pCur;` |
|   19835 |  1504 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   19835 |  1505 | `		rc = SXERR_EMPTY;` |
|   19835 |  1506 | `		if( pCur < pGen->pIn ){` |
|    1637 |  1507 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1508 | `				/* Missing value */` |
|      12 |  1509 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      12 |  1510 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1511 | `					return SXERR_ABORT;` |
|       - |  1512 | `				}` |
|      12 |  1513 | `				return SXRET_OK;` |
|       - |  1514 | `			}` |
|       - |  1515 | `			/* Compile the expression holding the key */` |
|    1627 |  1516 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1517 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1627 |  1518 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1519 | `				return SXERR_ABORT;` |
|       - |  1520 | `			}` |
|    1627 |  1521 | `			pCur++; /* Jump the '=>' operator */` |
|   19014 |  1522 | `		}else if( pKey == pCur ){` |
|       - |  1523 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1524 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1525 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1526 | `		}else{` |
|       - |  1527 | `			/* Reset back the cursor and point to the entry value */` |
|   18203 |  1528 | `			pCur = pKey;` |
|       - |  1529 | `		}` |
|   19825 |  1530 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1531 | `			/* No available key,load NULL */` |
|   18205 |  1532 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9100 |  1533 | `		}` |
|   19825 |  1534 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   19823 |  1553 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   19823 |  1554 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   19819 |  1567 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   19819 |  1568 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1569 | `			return SXERR_ABORT;` |
|       - |  1570 | `		}` |
|   19819 |  1571 | `		if( iSpread ){` |
|       - |  1572 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1573 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   19788 |  1574 | `		}else if( iEmitRef ){` |
|       - |  1575 | `			/* Emit the load reference instruction */` |
|      41 |  1576 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1577 | `		}` |
|   19819 |  1578 | `		xValidator = 0;` |
|   19819 |  1579 | `		iEmitRef = 0;` |
|   19819 |  1580 | `		iSpread = 0;` |
|   19819 |  1581 | `		nPair++;` |
|       5 |  1582 | `	}` |
|       - |  1583 | `	/* Emit the load map instruction */` |
|   31093 |  1584 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1585 | `	/* Node successfully compiled */` |
|   31093 |  1586 | `	return SXRET_OK;` |
|   15557 |  1587 |  |
|       - |  1588 | `/*` |
|       - |  1589 | ` * Compile the 'array' language construct.` |
|       - |  1590 | ` *	 According to the PHP language reference manual` |
|       - |  1591 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1592 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1593 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1594 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1595 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1596 | ` */` |
|   30112 |  1597 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1598 |  |
|       - |  1599 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   30117 |  1600 | `	pGen->pIn += 2;` |
|   30117 |  1601 | `	pGen->pEnd--;` |
|   15056 |  1602 | `	SXUNUSED(iCompileFlag);` |
|   30117 |  1603 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1604 |  |
|       - |  1605 | `/*` |
|       - |  1606 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1607 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1608 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1609 | ` */` |
|     992 |  1610 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1611 |  |
|       - |  1612 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     997 |  1613 | `	pGen->pIn++;` |
|     997 |  1614 | `	pGen->pEnd--;` |
|     496 |  1615 | `	SXUNUSED(iCompileFlag);` |
|     997 |  1616 | `	return GenStateCompileArrayBody(pGen);` |
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
|     270 |  1952 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1953 |  |
|       - |  1954 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1955 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1956 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1957 | `							  * one thread is allowed to compile the script.` |
|       - |  1958 | `						      */` |
|       - |  1959 | `	SyString sName;` |
|       - |  1960 | `	sxu32 nLen;` |
|       - |  1961 | `	sxi32 rc;` |
|     135 |  1962 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1963 |  |
|     275 |  1964 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     275 |  1965 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1966 | `		pGen->pIn++;` |
|     ! 0 |  1967 | `	}` |
|       - |  1968 | `	/* Generate a unique name */` |
|     275 |  1969 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1970 | `	/* Make sure the generated name is unique */` |
|     275 |  1971 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1972 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1973 | `	}` |
|     275 |  1974 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  1975 | `	/* Compile the lambda body */` |
|     275 |  1976 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     275 |  1977 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1978 | `		return SXERR_ABORT;` |
|       - |  1979 | `	}` |
|       - |  1980 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  1981 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  1982 | `	 * the handler wraps either in a Closure instance. */` |
|     275 |  1983 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  1984 | `	/* Node successfully compiled */` |
|     275 |  1985 | `	return SXRET_OK;` |
|     140 |  1986 |  |
|       - |  1987 | `/*` |
|       - |  1988 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1989 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1990 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1991 | ` */` |
|     166 |  1992 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  1993 | `	ph7_gen_state *pGen,` |
|       - |  1994 | `	ph7_vm_func *pFunc,` |
|       - |  1995 | `	const char *zName,` |
|       - |  1996 | `	sxu32 nByte,` |
|       - |  1997 | `	SyString *aShadow,` |
|       - |  1998 | `	sxu32 nShadow)` |
|       2 |  1999 |  |
|       - |  2000 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2001 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2002 | `	sxu32 n, nEnv;` |
|       - |  2003 | `	char *zDup;` |
|     168 |  2004 | `	if( nByte == 0 ){` |
|     ! 0 |  2005 | `		return SXRET_OK;` |
|       - |  2006 | `	}` |
|     166 |  2007 | `	if( nByte == sizeof("this")-1` |
|      89 |  2008 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2009 | `		return SXRET_OK;` |
|       - |  2010 | `	}` |
|     202 |  2011 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     148 |  2012 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     145 |  2013 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     114 |  2014 | `			return SXRET_OK;` |
|       - |  2015 | `		}` |
|      19 |  2016 | `	}` |
|      53 |  2017 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  2018 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  2019 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  2020 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  2021 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  2022 | `			return SXRET_OK;` |
|       - |  2023 | `		}` |
|      15 |  2024 | `	}` |
|      53 |  2025 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  2026 | `	if( zDup == 0 ){` |
|     ! 0 |  2027 | `		return SXERR_ABORT;` |
|       - |  2028 | `	}` |
|      53 |  2029 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  2030 | `	sEnv.iFlags = 0;` |
|      53 |  2031 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  2032 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  2033 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  2034 | `	return SXRET_OK;` |
|      85 |  2035 |  |
|       - |  2036 | `/*` |
|       - |  2037 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2038 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2039 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2040 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2041 | ` */` |
|      20 |  2042 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2043 | `	ph7_gen_state *pGen,` |
|       - |  2044 | `	ph7_vm_func *pFunc,` |
|       - |  2045 | `	const char *zIn,` |
|       - |  2046 | `	const char *zEnd,` |
|       - |  2047 | `	SyString *aShadow,` |
|       - |  2048 | `	sxu32 nShadow)` |
|       1 |  2049 |  |
|       - |  2050 | `	sxi32 rc;` |
|     181 |  2051 | `	while( zIn < zEnd ){` |
|     161 |  2052 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  2053 | `			zIn++;` |
|     ! 0 |  2054 | `			if( zIn < zEnd ){` |
|     ! 0 |  2055 | `				zIn++;` |
|     ! 0 |  2056 | `			}` |
|     ! 0 |  2057 | `			continue;` |
|       - |  2058 | `		}` |
|     160 |  2059 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  2060 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  2061 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2062 | `			const char *zName;` |
|      13 |  2063 | `			zIn++; /* skip '$' */` |
|      13 |  2064 | `			zName = zIn;` |
|      39 |  2065 | `			while( zIn < zEnd ){` |
|      35 |  2066 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  2067 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2068 | `					zIn++;` |
|     ! 0 |  2069 | `					while( zIn < zEnd` |
|     ! 0 |  2070 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2071 | `						zIn++;` |
|     ! 0 |  2072 | `					}` |
|     ! 0 |  2073 | `					continue;` |
|       - |  2074 | `				}` |
|      35 |  2075 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  2076 | `					break;` |
|       - |  2077 | `				}` |
|      27 |  2078 | `				zIn++;` |
|       1 |  2079 | `			}` |
|      13 |  2080 | `			if( zIn > zName ){` |
|      19 |  2081 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  2082 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  2083 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2084 | `					return SXERR_ABORT;` |
|       - |  2085 | `				}` |
|       6 |  2086 | `			}` |
|      13 |  2087 | `			continue;` |
|       - |  2088 | `		}` |
|     149 |  2089 | `		zIn++;` |
|       1 |  2090 | `	}` |
|      21 |  2091 | `	return SXRET_OK;` |
|      11 |  2092 |  |
|       - |  2093 | `/*` |
|       - |  2094 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  2095 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  2096 | ` *   - plain $<id> pairs` |
|       - |  2097 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  2098 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  2099 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  2100 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  2101 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  2102 | ` *     are never mistakenly captured.` |
|       - |  2103 | ` */` |
|     162 |  2104 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2105 | `	ph7_gen_state *pGen,` |
|       - |  2106 | `	ph7_vm_func *pFunc,` |
|       - |  2107 | `	SyToken *pStart,` |
|       - |  2108 | `	SyToken *pEnd,` |
|       - |  2109 | `	SyString *aShadow,` |
|       - |  2110 | `	sxu32 nShadow)` |
|       2 |  2111 |  |
|     164 |  2112 | `	SyToken *pScan = pStart;` |
|       - |  2113 | `	sxi32 rc;` |
|     584 |  2114 | `	while( pScan < pEnd ){` |
|     422 |  2115 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      31 |  2116 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|      10 |  2117 | `				pScan->sData.zString,` |
|      20 |  2118 | `				pScan->sData.zString + pScan->sData.nByte,` |
|      10 |  2119 | `				aShadow,nShadow);` |
|      21 |  2120 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2121 | `				return SXERR_ABORT;` |
|       - |  2122 | `			}` |
|      21 |  2123 | `			pScan++;` |
|      21 |  2124 | `			continue;` |
|       - |  2125 | `		}` |
|     402 |  2126 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  2127 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  2128 | `			SyToken *pFnKw = pScan;` |
|      20 |  2129 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2130 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  2131 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2132 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2133 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2134 | `			}` |
|      21 |  2135 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2136 | `				SyToken *pInnerSigStart;` |
|       - |  2137 | `				SyToken *pInnerSigEnd;` |
|       - |  2138 | `				SyToken *pInnerBodyEnd;` |
|       - |  2139 | `				SyString *aInnerShadow;` |
|       - |  2140 | `				sxu32 nInnerShadow;` |
|       - |  2141 | `				sxu32 nInnerParamMax;` |
|       - |  2142 | `				SyToken *p;` |
|       - |  2143 | `				int iNestInner;` |
|      19 |  2144 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2145 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2146 | `					pScan++;` |
|     ! 0 |  2147 | `				}` |
|      19 |  2148 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2149 | `					pScan++;` |
|     ! 0 |  2150 | `					continue;` |
|       - |  2151 | `				}` |
|      19 |  2152 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2153 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2154 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2155 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2156 | `					pScan = pEnd;` |
|     ! 0 |  2157 | `					continue;` |
|       - |  2158 | `				}` |
|       - |  2159 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2160 | `				nInnerParamMax = 0;` |
|      57 |  2161 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2162 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2163 | `						nInnerParamMax++;` |
|       6 |  2164 | `					}` |
|      20 |  2165 | `				}` |
|      19 |  2166 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2167 | `					&pGen->pVm->sAllocator,` |
|      18 |  2168 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2169 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2170 | `					return SXERR_ABORT;` |
|       - |  2171 | `				}` |
|      19 |  2172 | `				nInnerShadow = 0;` |
|      25 |  2173 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2174 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2175 | `				}` |
|      57 |  2176 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2177 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2178 | `						continue;` |
|       - |  2179 | `					}` |
|      13 |  2180 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2181 | `						break;` |
|       - |  2182 | `					}` |
|      13 |  2183 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2184 | `						continue;` |
|       - |  2185 | `					}` |
|      13 |  2186 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2187 | `				}` |
|      19 |  2188 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2189 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2190 | `					pScan++;` |
|     ! 0 |  2191 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2192 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2193 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2194 | `						pScan++;` |
|     ! 0 |  2195 | `					}` |
|     ! 0 |  2196 | `					if( pScan < pEnd` |
|     ! 0 |  2197 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2198 | `						pScan++;` |
|     ! 0 |  2199 | `					}` |
|     ! 0 |  2200 | `				}` |
|      19 |  2201 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2202 | `					pScan++; /* past '=>' */` |
|       9 |  2203 | `				}` |
|      19 |  2204 | `				pInnerBodyEnd = pScan;` |
|      19 |  2205 | `				iNestInner = 0;` |
|     131 |  2206 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2207 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2208 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2209 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2210 | `						break;` |
|       - |  2211 | `					}` |
|     113 |  2212 | `					if( pInnerBodyEnd->nType &` |
|       - |  2213 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2214 | `						iNestInner++;` |
|     112 |  2215 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2216 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2217 | `						iNestInner--;` |
|       1 |  2218 | `					}` |
|     113 |  2219 | `					pInnerBodyEnd++;` |
|       1 |  2220 | `				}` |
|       - |  2221 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2222 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2223 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2224 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2225 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2226 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2227 | `				 *` |
|       - |  2228 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2229 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2230 | `				 * range after the '=' sign. */` |
|       - |  2231 | `				{` |
|      19 |  2232 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2233 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2234 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2235 | `						SyToken *pEq = 0;` |
|      13 |  2236 | `						int iNestArg = 0;` |
|      49 |  2237 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2238 | `							if( iNestArg == 0` |
|      39 |  2239 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2240 | `								break;` |
|       - |  2241 | `							}` |
|      37 |  2242 | `							if( pArgEnd->nType &` |
|       - |  2243 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2244 | `								iNestArg++;` |
|      37 |  2245 | `							}else if( pArgEnd->nType &` |
|       - |  2246 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2247 | `								iNestArg--;` |
|     ! 0 |  2248 | `							}` |
|      36 |  2249 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2250 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2251 | `								pEq = pArgEnd;` |
|       3 |  2252 | `							}` |
|      37 |  2253 | `							pArgEnd++;` |
|       1 |  2254 | `						}` |
|      13 |  2255 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2256 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2257 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2258 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2259 | `								return SXERR_ABORT;` |
|       - |  2260 | `							}` |
|       3 |  2261 | `						}` |
|      13 |  2262 | `						pArgStart = pArgEnd;` |
|      12 |  2263 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2264 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2265 | `							pArgStart++;` |
|       1 |  2266 | `						}` |
|       1 |  2267 | `					}` |
|       - |  2268 | `				}` |
|      28 |  2269 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2270 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2271 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2272 | `					return SXERR_ABORT;` |
|       - |  2273 | `				}` |
|      19 |  2274 | `				pScan = pInnerBodyEnd;` |
|      19 |  2275 | `				continue;` |
|       - |  2276 | `			}` |
|       1 |  2277 | `		}` |
|     384 |  2278 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     230 |  2279 | `			pScan++;` |
|     230 |  2280 | `			continue;` |
|       - |  2281 | `		}` |
|       - |  2282 | `		{` |
|       - |  2283 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     156 |  2284 | `			SyToken *pDollar = pScan;` |
|     231 |  2285 | `			while( &pDollar[1] < pEnd` |
|     156 |  2286 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2287 | `				pDollar++;` |
|     ! 0 |  2288 | `			}` |
|     156 |  2289 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2290 | `				break;` |
|       - |  2291 | `			}` |
|     156 |  2292 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2293 | `				pScan = pDollar + 1;` |
|     ! 0 |  2294 | `				continue;` |
|       - |  2295 | `			}` |
|     233 |  2296 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     154 |  2297 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      77 |  2298 | `				aShadow,nShadow);` |
|     156 |  2299 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2300 | `				return SXERR_ABORT;` |
|       - |  2301 | `			}` |
|     156 |  2302 | `			pScan = pDollar + 2;` |
|       - |  2303 | `		}` |
|       2 |  2304 | `	}` |
|     164 |  2305 | `	return SXRET_OK;` |
|      83 |  2306 |  |
|       - |  2307 | `/*` |
|       - |  2308 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2309 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2310 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2311 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2312 | ` * $this is also made available.` |
|       - |  2313 | ` */` |
|     144 |  2314 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2315 |  |
|       - |  2316 | `	ph7_vm_func *pFunc;` |
|       - |  2317 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2318 | `	GenBlock *pBlock;` |
|       - |  2319 | `	SySet *pInstrContainer;` |
|       - |  2320 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2321 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2322 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2323 | `	SyToken *pSavedEnd;` |
|       - |  2324 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2325 | `	char zName[512];` |
|       - |  2326 | `	static int iCnt = 1;` |
|       - |  2327 | `	char *zDup;` |
|       - |  2328 | `	sxu32 nLen;` |
|       - |  2329 | `	sxu32 nLine;` |
|     148 |  2330 | `	sxi32 iFlags = 0;` |
|     148 |  2331 | `	int bStatic = 0;` |
|       - |  2332 | `	sxi32 rc;` |
|       - |  2333 | `	sxu32 n;` |
|      72 |  2334 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2335 |  |
|     148 |  2336 | `	nLine = pGen->pIn->nLine;` |
|       - |  2337 | `	/* Optional 'static' prefix */` |
|     144 |  2338 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     148 |  2339 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2340 | `		bStatic = 1;` |
|       3 |  2341 | `		pGen->pIn++;` |
|       1 |  2342 | `	}` |
|       - |  2343 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     144 |  2344 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     148 |  2345 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2346 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2347 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2348 | `		return SXERR_SYNTAX;` |
|       - |  2349 | `	}` |
|     148 |  2350 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2351 | `	/* Optional '&' — return by reference */` |
|     148 |  2352 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2353 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2354 | `		pGen->pIn++;` |
|     ! 0 |  2355 | `	}` |
|       - |  2356 | `	/* Expect '(' */` |
|     148 |  2357 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2358 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2359 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2360 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2361 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2362 | `		}else{` |
|     ! 0 |  2363 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2364 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2365 | `		}` |
|       3 |  2366 | `		return SXERR_SYNTAX;` |
|       - |  2367 | `	}` |
|     146 |  2368 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2369 | `	/* Delimit the parameter list */` |
|     146 |  2370 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     146 |  2371 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2372 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2373 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2374 | `		return SXERR_SYNTAX;` |
|       - |  2375 | `	}` |
|       - |  2376 | `	/* Allocate the function state */` |
|     143 |  2377 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     143 |  2378 | `	if( pFunc == 0 ){` |
|     ! 0 |  2379 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2380 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2381 | `		return SXERR_ABORT;` |
|       - |  2382 | `	}` |
|       - |  2383 | `	/* Generate a unique lambda name */` |
|     143 |  2384 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     245 |  2385 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     104 |  2386 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2387 | `	}` |
|     143 |  2388 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     143 |  2389 | `	if( zDup == 0 ){` |
|     ! 0 |  2390 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2391 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2392 | `		return SXERR_ABORT;` |
|       - |  2393 | `	}` |
|     143 |  2394 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2395 | `	/* Collect function arguments */` |
|     143 |  2396 | `	if( pGen->pIn < pSigEnd ){` |
|     101 |  2397 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     101 |  2398 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2399 | `			return SXERR_ABORT;` |
|       - |  2400 | `		}` |
|      49 |  2401 | `	}` |
|       - |  2402 | `	/* Point past ')' and parse optional return type */` |
|     143 |  2403 | `	pGen->pIn = &pSigEnd[1];` |
|     143 |  2404 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     143 |  2405 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2406 | `		return SXERR_ABORT;` |
|     143 |  2407 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2408 | `		return SXERR_SYNTAX;` |
|       - |  2409 | `	}` |
|       - |  2410 | `	/* Expect '=>' */` |
|     143 |  2411 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2412 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2413 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2414 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2415 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2416 | `		}else{` |
|     ! 0 |  2417 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2418 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2419 | `		}` |
|       3 |  2420 | `		return SXERR_SYNTAX;` |
|       - |  2421 | `	}` |
|     140 |  2422 | `	pGen->pIn++; /* Jump '=>' */` |
|     140 |  2423 | `	pBodyStart = pGen->pIn;` |
|     140 |  2424 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2425 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2426 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2427 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2428 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     140 |  2429 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2430 | `	{` |
|     140 |  2431 | `		SyString *aShadow = 0;` |
|     140 |  2432 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     140 |  2433 | `		if( nShadow > 0 ){` |
|      98 |  2434 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      96 |  2435 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      98 |  2436 | `			if( aShadow == 0 ){` |
|     ! 0 |  2437 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2438 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2439 | `				return SXERR_ABORT;` |
|       - |  2440 | `			}` |
|     216 |  2441 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     120 |  2442 | `				aShadow[n] = aArgs[n].sName;` |
|      61 |  2443 | `			}` |
|      48 |  2444 | `		}` |
|     209 |  2445 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      69 |  2446 | `			aShadow,nShadow);` |
|     140 |  2447 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2448 | `			return SXERR_ABORT;` |
|       - |  2449 | `		}` |
|       - |  2450 | `	}` |
|       - |  2451 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2452 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2453 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2454 | `	 * $this. */` |
|     140 |  2455 | `	if( !bStatic ){` |
|       - |  2456 | `		char *zThisDup;` |
|     138 |  2457 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     138 |  2458 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2459 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2460 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2461 | `			return SXERR_ABORT;` |
|       - |  2462 | `		}` |
|     138 |  2463 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     138 |  2464 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     138 |  2465 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     138 |  2466 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     138 |  2467 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      68 |  2468 | `	}` |
|       - |  2469 | `	/* Arrow functions are always closures */` |
|     140 |  2470 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2471 | `	/* Compile the body expression as an implicit return */` |
|     209 |  2472 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      69 |  2473 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     140 |  2474 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2475 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2476 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2477 | `		return SXERR_ABORT;` |
|       - |  2478 | `	}` |
|     140 |  2479 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     140 |  2480 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     140 |  2481 | `	pSavedEnd = pGen->pEnd;` |
|     140 |  2482 | `	pGen->pIn = pBodyStart;` |
|     140 |  2483 | `	pGen->pEnd = pBodyEnd;` |
|     140 |  2484 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     140 |  2485 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2486 | `		return SXERR_ABORT;` |
|       - |  2487 | `	}` |
|       - |  2488 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2489 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2490 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2491 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     140 |  2492 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     140 |  2493 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     140 |  2494 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     140 |  2495 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     140 |  2496 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2497 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     140 |  2498 | `	pGen->pIn = pBodyEnd;` |
|     140 |  2499 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2500 | `	/* Emit the load-closure instruction */` |
|     140 |  2501 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     140 |  2502 | `	return SXRET_OK;` |
|      76 |  2503 |  |
|       - |  2504 | `/*` |
|       - |  2505 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2506 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2507 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2508 | ` * expression's value.` |
|       - |  2509 | ` */` |
|     346 |  2510 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2511 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2512 |  |
|       - |  2513 | `	SySet *pInstrContainer;` |
|       - |  2514 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2515 | `	GenBlock *pArmBlock;` |
|       - |  2516 | `	sxi32 rc;` |
|     349 |  2517 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2518 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2519 | `	pGen->pIn  = pStart;` |
|     349 |  2520 | `	pGen->pEnd = pStop;` |
|     349 |  2521 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2522 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2523 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2524 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2525 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2526 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2527 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2528 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2529 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2530 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2531 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2532 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2533 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2534 | `		return SXERR_ABORT;` |
|       - |  2535 | `	}` |
|     349 |  2536 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2538 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2539 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2540 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2541 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2542 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2543 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2544 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2545 | `		return SXERR_ABORT;` |
|       - |  2546 | `	}` |
|     349 |  2547 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2548 | `		return SXERR_EMPTY;` |
|       - |  2549 | `	}` |
|     349 |  2550 | `	return SXRET_OK;` |
|     176 |  2551 |  |
|       - |  2552 | `/*` |
|       - |  2553 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2554 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2555 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2556 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2557 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2558 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2559 | ` */` |
|       - |  2560 | `/*` |
|       - |  2561 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2562 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2563 | ` * caller can bail out of the current expression.` |
|       - |  2564 | ` */` |
|       2 |  2565 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2566 |  |
|       - |  2567 | `	va_list ap;` |
|       - |  2568 | `	sxi32 rc;` |
|       - |  2569 | `	SyBlob sMsg;` |
|       3 |  2570 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2571 | `	va_start(ap,zFmt);` |
|       3 |  2572 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2573 | `	va_end(ap);` |
|       3 |  2574 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2575 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2576 | `	SyBlobRelease(&sMsg);` |
|       3 |  2577 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2578 | `		return SXERR_ABORT;` |
|       - |  2579 | `	}` |
|       3 |  2580 | `	return SXERR_SYNTAX;` |
|       2 |  2581 |  |
|       - |  2582 | `/*` |
|       - |  2583 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2584 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2585 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2586 | ` */` |
|     348 |  2587 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2588 |  |
|     352 |  2589 | `	SyToken *pCur = pStart;` |
|     352 |  2590 | `	int iNest = 0;` |
|     814 |  2591 | `	while( pCur < pEnd ){` |
|     780 |  2592 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2593 | `			iNest++;` |
|     774 |  2594 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2595 | `			iNest--;` |
|     762 |  2596 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2597 | `			return pCur;` |
|       - |  2598 | `		}` |
|     466 |  2599 | `		pCur++;` |
|       4 |  2600 | `	}` |
|      37 |  2601 | `	return pEnd;` |
|     178 |  2602 |  |
|      70 |  2603 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2604 |  |
|       - |  2605 | `	ph7_match *pMatch;` |
|       - |  2606 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2607 | `	int bHasDefault = 0;` |
|       - |  2608 | `	sxu32 nLine;` |
|       - |  2609 | `	sxi32 rc;` |
|      35 |  2610 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2611 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2612 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2613 | `	/* Expect '(' */` |
|      75 |  2614 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2615 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2616 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2617 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2618 | `	}` |
|      75 |  2619 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2620 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2621 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2622 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2623 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2624 | `	}` |
|      75 |  2625 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2626 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2627 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2628 | `	}` |
|       - |  2629 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2630 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2631 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2632 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2633 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2634 | `		return SXERR_ABORT;` |
|       - |  2635 | `	}` |
|      75 |  2636 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2637 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2638 | `	/* Expect '{' */` |
|      75 |  2639 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2640 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2641 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2642 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2643 | `	}` |
|      75 |  2644 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2645 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2646 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2647 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2648 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2649 | `	}` |
|       - |  2650 | `	/* Allocate ph7_match container */` |
|      75 |  2651 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2652 | `	if( pMatch == 0 ){` |
|     ! 0 |  2653 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2654 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2655 | `		return SXERR_ABORT;` |
|       - |  2656 | `	}` |
|      75 |  2657 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2658 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2659 | `	/* Iterate arms */` |
|     253 |  2660 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2661 | `		ph7_match_arm sArm;` |
|       - |  2662 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2663 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2664 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2665 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2666 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2667 | `		/* 'default' arm? */` |
|     182 |  2668 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2669 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2670 | `			if( bHasDefault ){` |
|       3 |  2671 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2672 | `					"Match expressions may only contain one default arm");` |
|       4 |  2673 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2674 | `			}` |
|      20 |  2675 | `			sArm.bDefault = 1;` |
|      20 |  2676 | `			bHasDefault = 1;` |
|      20 |  2677 | `			pGen->pIn++;` |
|      20 |  2678 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2679 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2680 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2681 | `			}` |
|      20 |  2682 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2683 | `		}else{` |
|       - |  2684 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2685 | `			pCondStart = pGen->pIn;` |
|     166 |  2686 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2687 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2688 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2689 | `				SySet sCondBc;` |
|       9 |  2690 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2691 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2692 | `						"syntax error, empty match condition expression");` |
|       - |  2693 | `				}` |
|       9 |  2694 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2695 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2696 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2697 | `					return SXERR_ABORT;` |
|       - |  2698 | `				}` |
|       9 |  2699 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2700 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2701 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2702 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2703 | `			}` |
|     166 |  2704 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2705 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2706 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2707 | `			}` |
|     163 |  2708 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2709 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2710 | `					"syntax error, empty match condition expression");` |
|       - |  2711 | `			}` |
|       - |  2712 | `			{` |
|       - |  2713 | `				SySet sCondBc;` |
|     163 |  2714 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2715 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2716 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2717 | `					return SXERR_ABORT;` |
|       - |  2718 | `				}` |
|     163 |  2719 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2720 | `			}` |
|     163 |  2721 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2722 | `		}` |
|       - |  2723 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2724 | `		pResStart = pGen->pIn;` |
|     181 |  2725 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2726 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2727 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2728 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2729 | `		}` |
|     181 |  2730 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2731 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2732 | `			return SXERR_ABORT;` |
|       - |  2733 | `		}` |
|     181 |  2734 | `		pGen->pIn = pResEnd;` |
|     181 |  2735 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2736 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2737 | `		}` |
|     181 |  2738 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2739 | `	}` |
|      69 |  2740 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2741 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2742 | `	return SXRET_OK;` |
|      40 |  2743 |  |
|       - |  2744 | `/*` |
|       - |  2745 | ` * Compile a backtick quoted string.` |
|       - |  2746 | ` */` |
|       4 |  2747 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2748 |  |
|       - |  2749 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2750 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2751 | `	 */` |
|       8 |  2752 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2753 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2754 | `		ph7_lib_version()` |
|       - |  2755 | `		);` |
|       - |  2756 | `	/* Load NULL */` |
|       6 |  2757 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2758 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2759 | `	/* Node successfully compiled */` |
|       6 |  2760 | `	return SXRET_OK;` |
|       2 |  2761 |  |
|       - |  2762 | `/*` |
|       - |  2763 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2764 | ` * construct.` |
|       - |  2765 | ` */` |
|      80 |  2766 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2767 |  |
|       - |  2768 | `	SyString *pName;` |
|       - |  2769 | `	sxu32 nKeyID;` |
|       - |  2770 | `	sxi32 rc;` |
|       - |  2771 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      85 |  2772 | `	pName = &pGen->pIn->sData;` |
|      85 |  2773 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  2774 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      85 |  2775 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2776 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2777 | `		/* Compile arguments one after one */` |
|       9 |  2778 | `		pTmp = pGen->pEnd;` |
|       - |  2779 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2780 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2781 | `		 *  mean that the following expression is valid:` |
|       - |  2782 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2783 | `		 */` |
|       9 |  2784 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2785 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2786 | `			if( pGen->pIn < pNext ){` |
|       9 |  2787 | `				pGen->pEnd = pNext;` |
|       9 |  2788 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2789 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2790 | `					return SXERR_ABORT;` |
|       - |  2791 | `				}` |
|       9 |  2792 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2793 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2794 | `					 * without the overhead of a function call.` |
|       - |  2795 | `					 * This is a very powerful optimization that improve` |
|       - |  2796 | `					 * performance greatly.` |
|       - |  2797 | `					 */` |
|       9 |  2798 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2799 | `				}` |
|       4 |  2800 | `			}` |
|       - |  2801 | `			/* Jump trailing commas */` |
|       9 |  2802 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2803 | `				pNext++;` |
|     ! 0 |  2804 | `			}` |
|       9 |  2805 | `			pGen->pIn = pNext;` |
|       1 |  2806 | `		}` |
|       - |  2807 | `		/* Restore token stream */` |
|       9 |  2808 | `		pGen->pEnd = pTmp;` |
|       5 |  2809 | `	}else{` |
|      77 |  2810 | `		sxi32 nArg = 0;` |
|      77 |  2811 | `		sxu32 nIdx = 0;` |
|      77 |  2812 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      77 |  2813 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2814 | `			return SXERR_ABORT;` |
|      77 |  2815 | `		}else if(rc != SXERR_EMPTY ){` |
|      77 |  2816 | `			nArg = 1;` |
|      36 |  2817 | `		}` |
|      77 |  2818 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2819 | `			ph7_value *pObj;` |
|       - |  2820 | `			/* Emit the call instruction */` |
|      29 |  2821 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  2822 | `			if( pObj == 0 ){` |
|     ! 0 |  2823 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2824 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2825 | `				return SXERR_ABORT;` |
|       - |  2826 | `			}` |
|      29 |  2827 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2828 | `			/* Install in the literal table */` |
|      29 |  2829 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      12 |  2830 | `		}` |
|       - |  2831 | `		/* Emit the call instruction */` |
|      77 |  2832 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      77 |  2833 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2834 | `	}` |
|       - |  2835 | `	/* Node successfully compiled */` |
|      85 |  2836 | `	return SXRET_OK;` |
|      45 |  2837 |  |
|       - |  2838 | `/*` |
|       - |  2839 | ` * Compile a node holding a variable declaration.` |
|       - |  2840 | ` * According to the PHP language reference` |
|       - |  2841 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2842 | ` *  The variable name is case-sensitive.` |
|       - |  2843 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2844 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2845 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2846 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2847 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2848 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2849 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2850 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2851 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2852 | ` *  the chapter on Expressions.` |
|       - |  2853 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2854 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2855 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2856 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2857 | ` *  is being assigned (the source variable).` |
|       - |  2858 | ` */` |
| 1086138 |  2859 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2860 |  |
| 1086143 |  2861 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2862 | `	sxi32 iVv;` |
|       - |  2863 | `	sxi32 iP1;` |
|       - |  2864 | `	void *p3;` |
|       - |  2865 | `	sxi32 rc;` |
| 1086143 |  2866 | `	iVv = -1; /* Variable variable counter */` |
| 2172293 |  2867 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1086155 |  2868 | `		pGen->pIn++;` |
| 1086155 |  2869 | `		iVv++;` |
|       5 |  2870 | `	}` |
| 1086143 |  2871 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2872 | `		/* Invalid variable name */` |
|     ! 0 |  2873 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2874 | `		if( rc == SXERR_ABORT ){` |
|       - |  2875 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2876 | `			return SXERR_ABORT;` |
|       - |  2877 | `		}` |
|     ! 0 |  2878 | `		return SXRET_OK;` |
|       - |  2879 | `	}` |
| 1086143 |  2880 | `	p3  = 0;` |
| 1086143 |  2881 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2882 | `		/* Dynamic variable creation */` |
|      19 |  2883 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  2884 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  2885 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2886 | `			/* Empty expression */` |
|       3 |  2887 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2888 | `			return SXRET_OK;` |
|       - |  2889 | `		}` |
|       - |  2890 | `		/* Compile the expression holding the variable name */` |
|      16 |  2891 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2892 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2893 | `			return SXERR_ABORT;` |
|      16 |  2894 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2895 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2896 | `			return SXRET_OK;` |
|       - |  2897 | `		}` |
|       7 |  2898 | `	}else{` |
|       - |  2899 | `		SyHashEntry *pEntry;` |
|       - |  2900 | `		SyString *pName;` |
| 1086127 |  2901 | `		char *zName = 0;` |
|       - |  2902 | `		/* Extract variable name */` |
| 1086127 |  2903 | `		pName = &pGen->pIn->sData;` |
|       - |  2904 | `		/* Advance the stream cursor */` |
| 1086127 |  2905 | `		pGen->pIn++;` |
| 1086127 |  2906 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1086127 |  2907 | `		if( pEntry == 0 ){` |
|       - |  2908 | `			/* Duplicate name */` |
|  145221 |  2909 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  145221 |  2910 | `			if( zName == 0 ){` |
|     ! 0 |  2911 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2912 | `				return SXERR_ABORT;` |
|       - |  2913 | `			}` |
|       - |  2914 | `			/* Install in the hashtable */` |
|  145221 |  2915 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   72613 |  2916 | `		}else{` |
|       - |  2917 | `			/* Name already available */` |
|  940911 |  2918 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2919 | `		}` |
| 1086127 |  2920 | `		p3 = (void *)zName;` |
|       - |  2921 | `	}` |
| 1086139 |  2922 | `	iP1 = 0;` |
| 1086139 |  2923 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  403005 |  2924 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2925 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  402987 |  2926 | `			iP1 = 1;` |
|  201491 |  2927 | `		}` |
|  201500 |  2928 | `	}` |
|       - |  2929 | `	/* Emit the load instruction */` |
| 1086139 |  2930 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1086151 |  2931 | `	while( iVv > 0 ){` |
|      13 |  2932 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2933 | `		iVv--;` |
|       1 |  2934 | `	}` |
|       - |  2935 | `	/* Node successfully compiled */` |
| 1086139 |  2936 | `	return SXRET_OK;` |
|  543074 |  2937 |  |
|       - |  2938 | `/*` |
|       - |  2939 | ` * Load a literal.` |
|       - |  2940 | ` */` |
|  762458 |  2941 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2942 |  |
|  762463 |  2943 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2944 | `	ph7_value *pObj;` |
|       - |  2945 | `	SyString *pStr;` |
|       - |  2946 | `	sxu32 nIdx;` |
|       - |  2947 | `	/* Extract token value */` |
|  762463 |  2948 | `	pStr = &pToken->sData;` |
|       - |  2949 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  762463 |  2950 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  165349 |  2951 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2952 | `			/* NULL constant are always indexed at 0 */` |
|   60897 |  2953 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   60897 |  2954 | `			return SXRET_OK;` |
|  104457 |  2955 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2956 | `			/* TRUE constant are always indexed at 1 */` |
|     713 |  2957 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     713 |  2958 | `			return SXRET_OK;` |
|       5 |  2959 | `		}` |
|  701857 |  2960 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  105732 |  2961 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2962 | `			/* FALSE constant are always indexed at 2 */` |
|   46685 |  2963 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   46685 |  2964 | `			return SXRET_OK;` |
|  605836 |  2965 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  110794 |  2966 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2967 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   10625 |  2968 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   10625 |  2969 | `			if( pObj == 0 ){` |
|     ! 0 |  2970 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2971 | `				return SXERR_ABORT;` |
|       - |  2972 | `			}` |
|   10625 |  2973 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2974 | `			/* Emit the load constant instruction */` |
|   10625 |  2975 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   10625 |  2976 | `			return SXRET_OK;` |
|  557725 |  2977 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   35812 |  2978 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2979 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  2980 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  2981 | `			if( pObj == 0 ){` |
|     ! 0 |  2982 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2983 | `				return SXERR_ABORT;` |
|       - |  2984 | `			}` |
|       7 |  2985 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2986 | `				SyString sNs;` |
|       7 |  2987 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  2988 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  2989 | `			}else{` |
|     ! 0 |  2990 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2991 | `			}` |
|       7 |  2992 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  2993 | `			return SXRET_OK;` |
|  547267 |  2994 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   24473 |  2995 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  549351 |  2996 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   19100 |  2997 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  2998 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  2999 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  3000 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  3001 | `				/* Point to the upper block */` |
|      11 |  3002 | `				pBlock = pBlock->pParent;` |
|       1 |  3003 | `			}` |
|      11 |  3004 | `			if( pBlock == 0 ){` |
|       - |  3005 | `				/* Called in the global scope,load NULL */` |
|       5 |  3006 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  3007 | `			}else{` |
|       - |  3008 | `				/* Extract the target function/method */` |
|       7 |  3009 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  3010 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  3011 | `					/* Not a class method,Load null */` |
|       3 |  3012 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3013 | `				}else{` |
|       5 |  3014 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  3015 | `					if( pObj == 0 ){` |
|     ! 0 |  3016 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3017 | `						return SXERR_ABORT;` |
|       - |  3018 | `					}` |
|       5 |  3019 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  3020 | `					/* Emit the load constant instruction */` |
|       5 |  3021 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  3022 | `				}` |
|       - |  3023 | `			}` |
|      11 |  3024 | `			return SXRET_OK;` |
|       - |  3025 | `	}` |
|       - |  3026 | `	/* Query literal table */` |
|  643547 |  3027 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3028 | `		ph7_value *pLitObj;` |
|       - |  3029 | `		/* Unknown literal,install it in the literal table */` |
|  271265 |  3030 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  271265 |  3031 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3032 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3033 | `			return SXERR_ABORT;` |
|       - |  3034 | `		}` |
|  271265 |  3035 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  271265 |  3036 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  135630 |  3037 | `	}` |
|       - |  3038 | `	/* Emit the load constant instruction */` |
|  643547 |  3039 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  643547 |  3040 | `	return SXRET_OK;` |
|  381234 |  3041 |  |
|       - |  3042 | `/*` |
|       - |  3043 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3044 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3045 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3046 | ` * Otherwise, load the simple literal directly.` |
|       - |  3047 | ` */` |
|  766036 |  3048 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3049 |  |
|       - |  3050 | `	sxi32 rc;` |
|  766041 |  3051 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3052 | `		return SXRET_OK;` |
|       - |  3053 | `	}` |
|       - |  3054 | `	/* Check if this is a multi-token namespace path */` |
|  766041 |  3055 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3056 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3583 |  3057 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3583 |  3058 | `		int isAbsolute = 0;` |
|    3583 |  3059 | `		SyBlobReset(pWorker);` |
|       - |  3060 | `		/* Check for leading backslash (absolute path) */` |
|    3583 |  3061 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3581 |  3062 | `			isAbsolute = 1;` |
|    3581 |  3063 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1788 |  3064 | `		}` |
|       - |  3065 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3583 |  3066 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3067 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3068 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3069 | `		}` |
|       - |  3070 | `		/* Collect all path components */` |
|    3679 |  3071 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    3679 |  3072 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      53 |  3073 | `				SyBlobAppend(pWorker,"\\",1);` |
|      29 |  3074 | `			}else{` |
|    3631 |  3075 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3076 | `			}` |
|    3679 |  3077 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3583 |  3078 | `				pGen->pIn++;` |
|    3583 |  3079 | `				break;` |
|       - |  3080 | `			}` |
|     101 |  3081 | `			pGen->pIn++;` |
|       5 |  3082 | `		}` |
|    3583 |  3083 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3084 | `			ph7_value *pObj;` |
|       - |  3085 | `			SyString sPath;` |
|       - |  3086 | `			sxu32 nIdx;` |
|    3583 |  3087 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3088 | `			/* Install in the literal table */` |
|    3583 |  3089 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3559 |  3090 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3559 |  3091 | `				if( pObj == 0 ){` |
|     ! 0 |  3092 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3093 | `					return SXERR_ABORT;` |
|       - |  3094 | `				}` |
|    3559 |  3095 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3559 |  3096 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1777 |  3097 | `			}` |
|       - |  3098 | `			/* Emit the load constant instruction.` |
|       - |  3099 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3100 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5372 |  3101 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1789 |  3102 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1789 |  3103 | `				nIdx,0,0);` |
|    3583 |  3104 | `			return SXRET_OK;` |
|       - |  3105 | `		}` |
|     ! 0 |  3106 | `	}` |
|       - |  3107 | `	/* Single-token literal: load directly */` |
|  762463 |  3108 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  762463 |  3109 | `	return rc;` |
|  383023 |  3110 |  |
|       - |  3111 | `/*` |
|       - |  3112 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3113 | ` */` |
|       - |  3114 | `/*` |
|       - |  3115 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - |  3116 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - |  3117 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - |  3118 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - |  3119 | ` */` |
|     ! 0 |  3120 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 |  3121 |  |
|     ! 0 |  3122 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 |  3123 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - |  3124 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 |  3125 | `	return SXERR_SYNTAX;` |
|     ! 0 |  3126 |  |
|  766036 |  3127 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3128 |  |
|       - |  3129 | `	sxi32 rc;` |
|  766041 |  3130 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  766041 |  3131 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3132 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3133 | `		return rc;` |
|       - |  3134 | `	}` |
|       - |  3135 | `	/* Node successfully compiled */` |
|  766041 |  3136 | `	return SXRET_OK;` |
|  383023 |  3137 |  |
|       - |  3138 | `/*` |
|       - |  3139 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3140 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3141 | ` */` |
|       8 |  3142 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3143 |  |
|       - |  3144 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3145 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3146 | `		pGen->pIn++;` |
|       1 |  3147 | `	}` |
|       9 |  3148 | `	return SXRET_OK;` |
|       1 |  3149 |  |
|       - |  3150 | `/*` |
|       - |  3151 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3152 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3153 | ` */` |
|     106 |  3154 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3155 |  |
|     111 |  3156 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      30 |  3157 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3158 | `			return TRUE;` |
|      28 |  3159 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3160 | `			return TRUE;` |
|       2 |  3161 | `		}` |
|      95 |  3162 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3163 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3164 | `			return TRUE;` |
|       - |  3165 | `		}` |
|     ! 0 |  3166 | `	}` |
|       - |  3167 | `	/* Not a reserved constant */` |
|     103 |  3168 | `	return FALSE;` |
|      58 |  3169 |  |
|       - |  3170 | `/*` |
|       - |  3171 | ` * Compile the 'const' statement.` |
|       - |  3172 | ` * According to the PHP language reference` |
|       - |  3173 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3174 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3175 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3176 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3177 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3178 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3179 | ` *  Syntax` |
|       - |  3180 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3181 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3182 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3183 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3184 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3185 | ` *  to get a list of all defined constants.` |
|       - |  3186 | ` *` |
|       - |  3187 | ` * Symisc eXtension.` |
|       - |  3188 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3189 | ` *  would allow only simple scalar value.` |
|       - |  3190 | ` *  Example` |
|       - |  3191 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3192 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3193 | ` */` |
|      32 |  3194 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3195 |  |
|       - |  3196 | `	SySet *pConsCode,*pInstrContainer;` |
|      37 |  3197 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3198 | `	SyString *pName;` |
|       - |  3199 | `	sxi32 rc;` |
|      37 |  3200 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      37 |  3201 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3202 | `		/* Invalid constant name */` |
|       9 |  3203 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       9 |  3204 | `		if( rc == SXERR_ABORT ){` |
|       - |  3205 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3206 | `			return SXERR_ABORT;` |
|       - |  3207 | `		}` |
|       9 |  3208 | `		goto Synchronize;` |
|       - |  3209 | `	}` |
|       - |  3210 | `	/* Peek constant name */` |
|      31 |  3211 | `	pName = &pGen->pIn->sData;` |
|       - |  3212 | `	/* Make sure the constant name isn't reserved */` |
|      31 |  3213 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3214 | `		/* Reserved constant */` |
|      10 |  3215 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3216 | `		if( rc == SXERR_ABORT ){` |
|       - |  3217 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3218 | `			return SXERR_ABORT;` |
|       - |  3219 | `		}` |
|      10 |  3220 | `		goto Synchronize;` |
|       - |  3221 | `	}` |
|      21 |  3222 | `	pGen->pIn++;` |
|      21 |  3223 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3224 | `		/* Invalid statement*/` |
|       6 |  3225 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3226 | `		if( rc == SXERR_ABORT ){` |
|       - |  3227 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3228 | `			return SXERR_ABORT;` |
|       - |  3229 | `		}` |
|       6 |  3230 | `		goto Synchronize;` |
|       - |  3231 | `	}` |
|      15 |  3232 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3233 | `	/* Allocate a new constant value container */` |
|      15 |  3234 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3235 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3236 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3237 | `		return SXERR_ABORT;` |
|       - |  3238 | `	}` |
|      15 |  3239 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3240 | `	/* Swap bytecode container */` |
|      15 |  3241 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3242 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3243 | `	/* Compile constant value */` |
|      15 |  3244 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3245 | `	/* Emit the done instruction */` |
|      15 |  3246 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3247 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3248 | `	if( rc == SXERR_ABORT ){` |
|       - |  3249 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3250 | `		return SXERR_ABORT;` |
|       - |  3251 | `	}` |
|      15 |  3252 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3253 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3254 | `	{` |
|       - |  3255 | `		SyBlob sFQN;` |
|       - |  3256 | `		SyString sFQNStr;` |
|      15 |  3257 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3258 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3259 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3260 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3261 | `		SyBlobRelease(&sFQN);` |
|       - |  3262 | `	}` |
|      15 |  3263 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3264 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3265 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3266 | `	}` |
|      15 |  3267 | `	return SXRET_OK;` |
|       9 |  3268 | `Synchronize:` |
|       - |  3269 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3270 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      42 |  3271 | `		pGen->pIn++;` |
|       4 |  3272 | `	}` |
|      22 |  3273 | `	return SXRET_OK;` |
|      21 |  3274 |  |
|       - |  3275 | `/*` |
|       - |  3276 | ` * Compile the 'continue' statement.` |
|       - |  3277 | ` * According to the PHP language reference` |
|       - |  3278 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3279 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3280 | ` *  iteration.` |
|       - |  3281 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3282 | ` *  the purposes of continue.` |
|       - |  3283 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3284 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3285 | ` *  Note:` |
|       - |  3286 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3287 | ` */` |
|       - |  3288 | `/*` |
|       - |  3289 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3290 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3291 | ` * break/continue crosses a try boundary.` |
|       - |  3292 | ` *` |
|       - |  3293 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3294 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3295 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3296 | ` */` |
|    3678 |  3297 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3298 |  |
|    3683 |  3299 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   21581 |  3300 | `	while( pBlock && pBlock != pTarget ){` |
|   17903 |  3301 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3302 | `			if( pBlock->pUserData ){` |
|       - |  3303 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3304 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3305 | `			}else{` |
|       - |  3306 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3307 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3308 | `				 * exception context from a sub-execution.` |
|       - |  3309 | `				 */` |
|     ! 0 |  3310 | `				break;` |
|       - |  3311 | `			}` |
|       1 |  3312 | `		}` |
|   17903 |  3313 | `		pBlock = pBlock->pParent;` |
|       5 |  3314 | `	}` |
|    3683 |  3315 |  |
|    3582 |  3316 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3317 |  |
|       - |  3318 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3319 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3320 | `	sxu32 nLineLocal;` |
|       - |  3321 | `	sxi32 rc;` |
|    3587 |  3322 | `	nLineLocal = pGen->pIn->nLine;` |
|    3587 |  3323 | `	iLevel = 0;` |
|       - |  3324 | `	/* Jump the 'continue' keyword */` |
|    3587 |  3325 | `	pGen->pIn++;` |
|    3587 |  3326 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3327 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3328 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3329 | `		 */` |
|       - |  3330 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3331 | `		char *zAlloc = 0;` |
|       - |  3332 | `		SyString sNum;` |
|      17 |  3333 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3334 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3335 | `			return SXERR_ABORT;` |
|       - |  3336 | `		}` |
|      17 |  3337 | `		if( rc == SXRET_OK ){` |
|      20 |  3338 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3339 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3340 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3341 | `				return SXERR_ABORT;` |
|       - |  3342 | `			}` |
|      14 |  3343 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3344 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3345 | `		}` |
|      17 |  3346 | `		if( iLevel < 2 ){` |
|       3 |  3347 | `			iLevel = 0;` |
|       1 |  3348 | `		}` |
|      17 |  3349 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3350 | `	}` |
|       - |  3351 | `	/* Point to the target loop */` |
|    3587 |  3352 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3587 |  3353 | `	if( pLoop == 0 ){` |
|       - |  3354 | `		/* Illegal continue */` |
|      13 |  3355 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      13 |  3356 | `		if( rc == SXERR_ABORT ){` |
|       - |  3357 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3358 | `			return SXERR_ABORT;` |
|       - |  3359 | `		}` |
|       8 |  3360 | `	}else{` |
|    3577 |  3361 | `		sxu32 nInstrIdx = 0;` |
|       - |  3362 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3577 |  3363 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3577 |  3364 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3365 | `			/* According to the PHP language reference manual` |
|       - |  3366 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3367 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3368 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3369 | `			 */` |
|       5 |  3370 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3371 | `			if( rc == SXRET_OK ){` |
|       5 |  3372 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3373 | `			}` |
|       3 |  3374 | `		}else{` |
|       - |  3375 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3573 |  3376 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3573 |  3377 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3378 | `				JumpFixup sJumpFix;` |
|       - |  3379 | `				/* Post-continue */` |
|      14 |  3380 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3381 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3382 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3383 | `			}` |
|       - |  3384 | `		}` |
|       - |  3385 | `	}` |
|    3587 |  3386 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3387 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3388 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3389 | `	}` |
|       - |  3390 | `	/* Statement successfully compiled */` |
|    3587 |  3391 | `	return SXRET_OK;` |
|    1796 |  3392 |  |
|       - |  3393 | `/*` |
|       - |  3394 | ` * Compile the 'break' statement.` |
|       - |  3395 | ` * According to the PHP language reference` |
|       - |  3396 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3397 | ` *  structure.` |
|       - |  3398 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3399 | ` *  enclosing structures are to be broken out of.` |
|       - |  3400 | ` */` |
|     122 |  3401 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3402 |  |
|       - |  3403 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3404 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3405 | `	sxi32 rc;` |
|     127 |  3406 | `	iLevel = 0;` |
|       - |  3407 | `	/* Jump the 'break' keyword */` |
|     127 |  3408 | `	pGen->pIn++;` |
|     127 |  3409 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3410 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3411 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3412 | `		 */` |
|       - |  3413 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      18 |  3414 | `		char *zAlloc = 0;` |
|       - |  3415 | `		SyString sNum;` |
|      18 |  3416 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      18 |  3417 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3418 | `			return SXERR_ABORT;` |
|       - |  3419 | `		}` |
|      18 |  3420 | `		if( rc == SXRET_OK ){` |
|      21 |  3421 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3422 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3423 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3424 | `				return SXERR_ABORT;` |
|       - |  3425 | `			}` |
|      15 |  3426 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3427 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3428 | `		}` |
|      18 |  3429 | `		if( iLevel < 2 ){` |
|       3 |  3430 | `			iLevel = 0;` |
|       1 |  3431 | `		}` |
|      18 |  3432 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3433 | `	}` |
|       - |  3434 | `	/* Extract the target loop */` |
|     127 |  3435 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     127 |  3436 | `	if( pLoop == 0 ){` |
|       - |  3437 | `		/* Illegal break */` |
|      19 |  3438 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      19 |  3439 | `		if( rc == SXERR_ABORT ){` |
|       - |  3440 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3441 | `			return SXERR_ABORT;` |
|       - |  3442 | `		}` |
|      11 |  3443 | `	}else{` |
|       - |  3444 | `		sxu32 nInstrIdx;` |
|       - |  3445 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|     111 |  3446 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|     111 |  3447 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|     111 |  3448 | `		if( rc == SXRET_OK ){` |
|       - |  3449 | `			/* Fix the jump later when the jump destination is resolved */` |
|     111 |  3450 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      53 |  3451 | `		}` |
|       - |  3452 | `	}` |
|     127 |  3453 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3454 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3455 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3456 | `	}` |
|       - |  3457 | `	/* Statement successfully compiled */` |
|     127 |  3458 | `	return SXRET_OK;` |
|      66 |  3459 |  |
|       - |  3460 | `/*` |
|       - |  3461 | ` * Compile or record a label.` |
|       - |  3462 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3463 | ` * Example` |
|       - |  3464 | ` *  goto LABEL;` |
|       - |  3465 | ` *   echo 'Foo';` |
|       - |  3466 | ` *  LABEL:` |
|       - |  3467 | ` *   echo 'Bar';` |
|       - |  3468 | ` */` |
|     112 |  3469 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3470 |  |
|       - |  3471 | `	GenBlock *pBlock;` |
|       - |  3472 | `	Label sLabel;` |
|       - |  3473 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3474 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3475 | `	if( pBlock ){` |
|       - |  3476 | `		sxi32 rc;` |
|       8 |  3477 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3478 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3479 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3480 | `			return SXERR_ABORT;` |
|       - |  3481 | `		}` |
|       4 |  3482 | `	}else{` |
|     113 |  3483 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3484 | `		char *zDup;` |
|       - |  3485 | `		/* Initialize label fields */` |
|     113 |  3486 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3487 | `		/* Duplicate label name */` |
|     113 |  3488 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3489 | `		if( zDup == 0 ){` |
|     ! 0 |  3490 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3491 | `			return SXERR_ABORT;` |
|       - |  3492 | `		}` |
|     113 |  3493 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3494 | `		sLabel.bRef  = FALSE;` |
|     113 |  3495 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3496 | `		pBlock = pGen->pCurrent;` |
|     221 |  3497 | `		while( pBlock ){` |
|     133 |  3498 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      23 |  3499 | `				break;` |
|       - |  3500 | `			}` |
|       - |  3501 | `			/* Point to the upper block */` |
|     113 |  3502 | `			pBlock = pBlock->pParent;` |
|       5 |  3503 | `		}` |
|     113 |  3504 | `		if( pBlock ){` |
|      23 |  3505 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      13 |  3506 | `		}else{` |
|      93 |  3507 | `			sLabel.pFunc = 0;` |
|       - |  3508 | `		}` |
|       - |  3509 | `		/* Insert in label set */` |
|     113 |  3510 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3511 | `	}` |
|     117 |  3512 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3513 | `	return SXRET_OK;` |
|      61 |  3514 |  |
|       - |  3515 | `/*` |
|       - |  3516 | ` * Compile the so hated 'goto' statement.` |
|       - |  3517 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3518 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3519 | ` * a compiler it has to do this.` |
|       - |  3520 | ` * According to the PHP language reference manual` |
|       - |  3521 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3522 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3523 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3524 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3525 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3526 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3527 | ` *   of a multi-level break` |
|       - |  3528 | ` */` |
|     152 |  3529 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3530 |  |
|       - |  3531 | `	JumpFixup sJump;` |
|       - |  3532 | `	sxi32 rc;` |
|     157 |  3533 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3534 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3535 | `		/* Missing label */` |
|     ! 0 |  3536 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3537 | `		if( rc == SXERR_ABORT ){` |
|       - |  3538 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3539 | `			return SXERR_ABORT;` |
|       - |  3540 | `		}` |
|     ! 0 |  3541 | `		return SXRET_OK;` |
|       - |  3542 | `	}` |
|     157 |  3543 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  3544 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       6 |  3545 | `		if( rc == SXERR_ABORT ){` |
|       - |  3546 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3547 | `			return SXERR_ABORT;` |
|       - |  3548 | `		}` |
|       4 |  3549 | `	}else{` |
|     153 |  3550 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3551 | `		GenBlock *pBlock;` |
|       - |  3552 | `		char *zDup;` |
|       - |  3553 | `		/* Prepare the jump destination */` |
|     153 |  3554 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3555 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3556 | `		/* Duplicate label name */` |
|     153 |  3557 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3558 | `		if( zDup == 0 ){` |
|     ! 0 |  3559 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3560 | `			return SXERR_ABORT;` |
|       - |  3561 | `		}` |
|     153 |  3562 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3563 | `		pBlock = pGen->pCurrent;` |
|     315 |  3564 | `		while( pBlock ){` |
|     199 |  3565 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      36 |  3566 | `				break;` |
|       - |  3567 | `			}` |
|       - |  3568 | `			/* Point to the upper block */` |
|     167 |  3569 | `			pBlock = pBlock->pParent;` |
|       5 |  3570 | `		}` |
|     153 |  3571 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       8 |  3572 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       8 |  3573 | `			if( rc == SXERR_ABORT ){` |
|       - |  3574 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3575 | `				return SXERR_ABORT;` |
|       - |  3576 | `			}` |
|       3 |  3577 | `		}` |
|     153 |  3578 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      30 |  3579 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      17 |  3580 | `		}else{` |
|     127 |  3581 | `			sJump.pFunc = 0;` |
|       - |  3582 | `		}` |
|       - |  3583 | `		/* Emit the unconditional jump */` |
|     153 |  3584 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3585 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3586 | `		}` |
|       - |  3587 | `	}` |
|     157 |  3588 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3589 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3590 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3591 | `	}` |
|       - |  3592 | `	/* Statement successfully compiled */` |
|     157 |  3593 | `	return SXRET_OK;` |
|      81 |  3594 |  |
|       - |  3595 | `/*` |
|       - |  3596 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3597 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3598 | ` * failure.` |
|       - |  3599 | ` */` |
|      20 |  3600 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  3601 |  |
|       - |  3602 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3603 | `	sxu32 nRawObj;` |
|      10 |  3604 | `	sxu32 nObjIdx;` |
|       - |  3605 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3606 | `	 * a PHP block.` |
|       - |  3607 | `	 */` |
|      10 |  3608 | `Consume:` |
|      21 |  3609 | `	nRawObj = nObjIdx = 0;` |
|      21 |  3610 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3611 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3612 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3613 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3614 | `			return SXERR_ABORT;` |
|       - |  3615 | `		}` |
|       - |  3616 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3617 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3618 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3619 | `		++nRawObj;` |
|     ! 0 |  3620 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3621 | `	}` |
|      21 |  3622 | `	if( nRawObj > 0 ){` |
|       - |  3623 | `		/* Emit the consume instruction */` |
|     ! 0 |  3624 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3625 | `	}` |
|      21 |  3626 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3627 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3628 | `		/* Reset the token set */` |
|     ! 0 |  3629 | `		SySetReset(pTokenSet);` |
|       - |  3630 | `		/* Tokenize input */` |
|     ! 0 |  3631 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3632 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3633 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3634 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3635 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3636 | `		/* Advance the stream cursor */` |
|     ! 0 |  3637 | `		pGen->pRawIn++;` |
|       - |  3638 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3639 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3640 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3641 | `			sxi32 rc;` |
|       - |  3642 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3643 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3644 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3645 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3646 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3647 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3648 | `				return SXERR_ABORT;` |
|     ! 0 |  3649 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3650 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3651 | `			}` |
|     ! 0 |  3652 | `			goto Consume;` |
|       - |  3653 | `		}` |
|     ! 0 |  3654 | `	}else{` |
|       - |  3655 | `		/* No more chunks to process */` |
|      21 |  3656 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  3657 | `		return SXERR_EOF;` |
|       - |  3658 | `	}` |
|     ! 0 |  3659 | `	return SXRET_OK;` |
|      11 |  3660 |  |
|       - |  3661 | `/*` |
|       - |  3662 | ` * Compile a PHP block.` |
|       - |  3663 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3664 | ` * optionally delimited by braces {}.` |
|       - |  3665 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3666 | ` * and this function takes care of generating the appropriate error` |
|       - |  3667 | ` * message.` |
|       - |  3668 | ` */` |
|  415586 |  3669 | `static sxi32 PH7_CompileBlock(` |
|       - |  3670 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3671 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3672 | `	)` |
|       5 |  3673 |  |
|       - |  3674 | `	sxi32 rc;` |
|       - |  3675 | `	sxu32 nLine;` |
|  415591 |  3676 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  413911 |  3677 | `		nLine = pGen->pIn->nLine;` |
|  413911 |  3678 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  413911 |  3679 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3680 | `			return SXERR_ABORT;` |
|       - |  3681 | `		}` |
|  413911 |  3682 | `		pGen->pIn++;` |
|       - |  3683 | `		/* Compile until we hit the closing braces '}' */` |
|  570334 |  3684 | `		for(;;){` |
| 1140673 |  3685 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  3686 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  3687 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3688 | `			 	   return SXERR_ABORT;` |
|       - |  3689 | `				}` |
|      21 |  3690 | `				if( rc == SXERR_EOF ){` |
|       - |  3691 | `					/* No more token to process. Missing closing braces */` |
|      21 |  3692 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 |  3693 | `					break;` |
|       - |  3694 | `				}` |
|     ! 0 |  3695 | `			}` |
| 1140653 |  3696 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3697 | `				/* Closing braces found,break immediately*/` |
|  413891 |  3698 | `				pGen->pIn++;` |
|  413891 |  3699 | `				break;` |
|       - |  3700 | `			}` |
|       - |  3701 | `			/* Compile a single statement */` |
|  726767 |  3702 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  726767 |  3703 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3704 | `				return SXERR_ABORT;` |
|       - |  3705 | `			}` |
|       5 |  3706 | `		}` |
|  413911 |  3707 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  208638 |  3708 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3709 | `		pGen->pIn++;` |
|     ! 0 |  3710 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3711 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3712 | `			return SXERR_ABORT;` |
|       - |  3713 | `		}` |
|       - |  3714 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3715 | `		for(;;){` |
|     ! 0 |  3716 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3717 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3718 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3719 | `			 	   return SXERR_ABORT;` |
|       - |  3720 | `				}` |
|     ! 0 |  3721 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3722 | `					/* No more token to process */` |
|     ! 0 |  3723 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3724 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3725 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3726 | `					}` |
|     ! 0 |  3727 | `					break;` |
|       - |  3728 | `				}` |
|     ! 0 |  3729 | `			}` |
|     ! 0 |  3730 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3731 | `				sxi32 nKwrd;` |
|       - |  3732 | `				/* Keyword found */` |
|     ! 0 |  3733 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3734 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3735 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3736 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3737 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3738 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3739 | `						}` |
|     ! 0 |  3740 | `						break;` |
|       - |  3741 | `				}` |
|     ! 0 |  3742 | `			}` |
|       - |  3743 | `			/* Compile a single statement */` |
|     ! 0 |  3744 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3745 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3746 | `				return SXERR_ABORT;` |
|       - |  3747 | `			}` |
|     ! 0 |  3748 | `		}` |
|     ! 0 |  3749 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3750 | `	}else{` |
|       - |  3751 | `		/* Compile a single statement */` |
|    1685 |  3752 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1685 |  3753 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3754 | `			return SXERR_ABORT;` |
|       - |  3755 | `		}` |
|       - |  3756 | `	}` |
|       - |  3757 | `	/* Jump trailing semi-colons ';' */` |
|  415591 |  3758 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3759 | `		pGen->pIn++;` |
|     ! 0 |  3760 | `	}` |
|  415591 |  3761 | `	return SXRET_OK;` |
|  207798 |  3762 |  |
|       - |  3763 | `/*` |
|       - |  3764 | ` * Compile the gentle 'while' statement.` |
|       - |  3765 | ` * According to the PHP language reference` |
|       - |  3766 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3767 | ` *  The basic form of a while statement is:` |
|       - |  3768 | ` *  while (expr)` |
|       - |  3769 | ` *   statement` |
|       - |  3770 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3771 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3772 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3773 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3774 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3775 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3776 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3777 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3778 | ` *  while (expr):` |
|       - |  3779 | ` *    statement` |
|       - |  3780 | ` *   endwhile;` |
|       - |  3781 | ` */` |
|   14272 |  3782 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3783 |  |
|   14277 |  3784 | `	GenBlock *pWhileBlock = 0;` |
|   14277 |  3785 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3786 | `	sxu32 nFalseJump;` |
|       - |  3787 | `	sxu32 nLine;` |
|       - |  3788 | `	sxi32 rc;` |
|   14277 |  3789 | `	nLine = pGen->pIn->nLine;` |
|       - |  3790 | `	/* Jump the 'while' keyword */` |
|   14277 |  3791 | `	pGen->pIn++;` |
|   14277 |  3792 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3793 | `		/* Syntax error */` |
|     ! 0 |  3794 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3795 | `		if( rc == SXERR_ABORT ){` |
|       - |  3796 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3797 | `			return SXERR_ABORT;` |
|       - |  3798 | `		}` |
|     ! 0 |  3799 | `		goto Synchronize;` |
|       - |  3800 | `	}` |
|       - |  3801 | `	/* Jump the left parenthesis '(' */` |
|   14277 |  3802 | `	pGen->pIn++;` |
|       - |  3803 | `	/* Create the loop block */` |
|   14277 |  3804 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   14277 |  3805 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3806 | `		return SXERR_ABORT;` |
|       - |  3807 | `	}` |
|       - |  3808 | `	/* Delimit the condition */` |
|   14277 |  3809 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14277 |  3810 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3811 | `		/* Empty expression */` |
|       3 |  3812 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3813 | `		if( rc == SXERR_ABORT ){` |
|       - |  3814 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3815 | `			return SXERR_ABORT;` |
|       - |  3816 | `		}` |
|       1 |  3817 | `	}` |
|       - |  3818 | `	/* Swap token streams */` |
|   14277 |  3819 | `	pTmp = pGen->pEnd;` |
|   14277 |  3820 | `	pGen->pEnd = pEnd;` |
|       - |  3821 | `	/* Compile the expression */` |
|   14277 |  3822 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14277 |  3823 | `	if( rc == SXERR_ABORT ){` |
|       - |  3824 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3825 | `		return SXERR_ABORT;` |
|       - |  3826 | `	}` |
|       - |  3827 | `	/* Update token stream */` |
|   14277 |  3828 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3829 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3830 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3831 | `			return SXERR_ABORT;` |
|       - |  3832 | `		}` |
|     ! 0 |  3833 | `		pGen->pIn++;` |
|     ! 0 |  3834 | `	}` |
|       - |  3835 | `	/* Synchronize pointers */` |
|   14277 |  3836 | `	pGen->pIn  = &pEnd[1];` |
|   14277 |  3837 | `	pGen->pEnd = pTmp;` |
|       - |  3838 | `	/* Emit the false jump */` |
|   14277 |  3839 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3840 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14277 |  3841 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3842 | `	/* Compile the loop body */` |
|   14277 |  3843 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   14277 |  3844 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3845 | `		return SXERR_ABORT;` |
|       - |  3846 | `	}` |
|       - |  3847 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14277 |  3848 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3849 | `	/* Fix all jumps now the destination is resolved */` |
|   14277 |  3850 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3851 | `	/* Release the loop block */` |
|   14277 |  3852 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3853 | `	/* Statement successfully compiled */` |
|   14277 |  3854 | `	return SXRET_OK;` |
|     ! 0 |  3855 | `Synchronize:` |
|       - |  3856 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3857 | `	 * compiling this erroneous block.` |
|       - |  3858 | `	 */` |
|     ! 0 |  3859 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3860 | `		pGen->pIn++;` |
|     ! 0 |  3861 | `	}` |
|     ! 0 |  3862 | `	return SXRET_OK;` |
|    7141 |  3863 |  |
|       - |  3864 | `/*` |
|       - |  3865 | ` * Compile the ugly do..while() statement.` |
|       - |  3866 | ` * According to the PHP language reference` |
|       - |  3867 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3868 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3869 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3870 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3871 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3872 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3873 | ` *  would end immediately).` |
|       - |  3874 | ` *  There is just one syntax for do-while loops:` |
|       - |  3875 | ` *  <?php` |
|       - |  3876 | ` *  $i = 0;` |
|       - |  3877 | ` *  do {` |
|       - |  3878 | ` *   echo $i;` |
|       - |  3879 | ` *  } while ($i > 0);` |
|       - |  3880 | ` * ?>` |
|       - |  3881 | ` */` |
|       2 |  3882 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3883 |  |
|       3 |  3884 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3885 | `	GenBlock *pDoBlock = 0;` |
|       - |  3886 | `	sxu32 nLine;` |
|       - |  3887 | `	sxi32 rc;` |
|       3 |  3888 | `	nLine = pGen->pIn->nLine;` |
|       - |  3889 | `	/* Jump the 'do' keyword */` |
|       3 |  3890 | `	pGen->pIn++;` |
|       - |  3891 | `	/* Create the loop block */` |
|       3 |  3892 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3893 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3894 | `		return SXERR_ABORT;` |
|       - |  3895 | `	}` |
|       - |  3896 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3897 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3898 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3899 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3900 | `		return SXERR_ABORT;` |
|       - |  3901 | `	}` |
|       3 |  3902 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3903 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3904 | `	}` |
|       3 |  3905 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3906 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3907 | `			/* Missing 'while' statement */` |
|       3 |  3908 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3909 | `			if( rc == SXERR_ABORT ){` |
|       - |  3910 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3911 | `				return SXERR_ABORT;` |
|       - |  3912 | `			}` |
|       3 |  3913 | `			goto Synchronize;` |
|       - |  3914 | `	}` |
|       - |  3915 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3916 | `	pGen->pIn++;` |
|     ! 0 |  3917 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3918 | `		/* Syntax error */` |
|     ! 0 |  3919 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3920 | `		if( rc == SXERR_ABORT ){` |
|       - |  3921 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3922 | `			return SXERR_ABORT;` |
|       - |  3923 | `		}` |
|     ! 0 |  3924 | `		goto Synchronize;` |
|       - |  3925 | `	}` |
|       - |  3926 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3927 | `	pGen->pIn++;` |
|       - |  3928 | `	/* Delimit the condition */` |
|     ! 0 |  3929 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3930 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3931 | `		/* Empty expression */` |
|     ! 0 |  3932 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3933 | `		if( rc == SXERR_ABORT ){` |
|       - |  3934 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3935 | `			return SXERR_ABORT;` |
|       - |  3936 | `		}` |
|     ! 0 |  3937 | `		goto Synchronize;` |
|       - |  3938 | `	}` |
|       - |  3939 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3940 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3941 | `		JumpFixup *aPost;` |
|       - |  3942 | `		VmInstr *pInstr;` |
|       - |  3943 | `		sxu32 nJumpDest;` |
|       - |  3944 | `		sxu32 n;` |
|     ! 0 |  3945 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3946 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3947 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3948 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3949 | `			if( pInstr ){` |
|       - |  3950 | `				/* Fix */` |
|     ! 0 |  3951 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3952 | `			}` |
|     ! 0 |  3953 | `		}` |
|     ! 0 |  3954 | `	}` |
|       - |  3955 | `	/* Swap token streams */` |
|     ! 0 |  3956 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3957 | `	pGen->pEnd = pEnd;` |
|       - |  3958 | `	/* Compile the expression */` |
|     ! 0 |  3959 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3960 | `	if( rc == SXERR_ABORT ){` |
|       - |  3961 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3962 | `		return SXERR_ABORT;` |
|       - |  3963 | `	}` |
|       - |  3964 | `	/* Update token stream */` |
|     ! 0 |  3965 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3966 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3967 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3968 | `			return SXERR_ABORT;` |
|       - |  3969 | `		}` |
|     ! 0 |  3970 | `		pGen->pIn++;` |
|     ! 0 |  3971 | `	}` |
|     ! 0 |  3972 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3973 | `	pGen->pEnd = pTmp;` |
|       - |  3974 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3975 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3976 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3977 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3978 | `	/* Release the loop block */` |
|     ! 0 |  3979 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3980 | `	/* Statement successfully compiled */` |
|     ! 0 |  3981 | `	return SXRET_OK;` |
|       1 |  3982 | `Synchronize:` |
|       - |  3983 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3984 | `	 * compiling this erroneous block.` |
|       - |  3985 | `	 */` |
|       3 |  3986 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3987 | `		pGen->pIn++;` |
|     ! 0 |  3988 | `	}` |
|       3 |  3989 | `	return SXRET_OK;` |
|       2 |  3990 |  |
|       - |  3991 | `/*` |
|       - |  3992 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3993 | ` * According to the PHP language reference` |
|       - |  3994 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3995 | ` *  The syntax of a for loop is:` |
|       - |  3996 | ` *  for (expr1; expr2; expr3)` |
|       - |  3997 | ` *   statement` |
|       - |  3998 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3999 | ` *  the beginning of the loop.` |
|       - |  4000 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  4001 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  4002 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  4003 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  4004 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  4005 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  4006 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  4007 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  4008 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  4009 | ` *  of using the for truth expression.` |
|       - |  4010 | ` */` |
|   14272 |  4011 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4012 |  |
|   14277 |  4013 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   14277 |  4014 | `	GenBlock *pForBlock = 0;` |
|       - |  4015 | `	sxu32 nFalseJump;` |
|       - |  4016 | `	sxu32 nLine;` |
|       - |  4017 | `	sxi32 rc;` |
|   14277 |  4018 | `	nLine = pGen->pIn->nLine;` |
|       - |  4019 | `	/* Jump the 'for' keyword */` |
|   14277 |  4020 | `	pGen->pIn++;` |
|   14277 |  4021 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4022 | `		/* Syntax error */` |
|     ! 0 |  4023 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4024 | `		if( rc == SXERR_ABORT ){` |
|       - |  4025 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4026 | `			return SXERR_ABORT;` |
|       - |  4027 | `		}` |
|     ! 0 |  4028 | `		return SXRET_OK;` |
|       - |  4029 | `	}` |
|       - |  4030 | `	/* Jump the left parenthesis '(' */` |
|   14277 |  4031 | `	pGen->pIn++;` |
|       - |  4032 | `	/* Delimit the init-expr;condition;post-expr */` |
|   14277 |  4033 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14277 |  4034 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4035 | `		/* Empty expression */` |
|     ! 0 |  4036 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4037 | `		if( rc == SXERR_ABORT ){` |
|       - |  4038 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4039 | `			return SXERR_ABORT;` |
|       - |  4040 | `		}` |
|       - |  4041 | `		/* Synchronize */` |
|     ! 0 |  4042 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4043 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4044 | `			pGen->pIn++;` |
|     ! 0 |  4045 | `		}` |
|     ! 0 |  4046 | `		return SXRET_OK;` |
|       - |  4047 | `	}` |
|       - |  4048 | `	/* Swap token streams */` |
|   14277 |  4049 | `	pTmp = pGen->pEnd;` |
|   14277 |  4050 | `	pGen->pEnd = pEnd;` |
|       - |  4051 | `	/* Compile initialization expressions if available */` |
|   14277 |  4052 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4053 | `	/* Pop operand lvalues */` |
|   14277 |  4054 | `	if( rc == SXERR_ABORT ){` |
|       - |  4055 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4056 | `		return SXERR_ABORT;` |
|   14277 |  4057 | `	}else if( rc != SXERR_EMPTY ){` |
|   14275 |  4058 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7135 |  4059 | `	}` |
|   14277 |  4060 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4061 | `		/* Syntax error */` |
|     ! 0 |  4062 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4063 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4064 | `		if( rc == SXERR_ABORT ){` |
|       - |  4065 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4066 | `			return SXERR_ABORT;` |
|       - |  4067 | `		}` |
|     ! 0 |  4068 | `		return SXRET_OK;` |
|       - |  4069 | `	}` |
|       - |  4070 | `	/* Jump the trailing ';' */` |
|   14277 |  4071 | `	pGen->pIn++;` |
|       - |  4072 | `	/* Create the loop block */` |
|   14277 |  4073 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   14277 |  4074 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4075 | `		return SXERR_ABORT;` |
|       - |  4076 | `	}` |
|       - |  4077 | `	/* Deffer continue jumps */` |
|   14277 |  4078 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4079 | `	/* Compile the condition */` |
|   14277 |  4080 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14277 |  4081 | `	if( rc == SXERR_ABORT ){` |
|       - |  4082 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4083 | `		return SXERR_ABORT;` |
|   14277 |  4084 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4085 | `		/* Emit the false jump */` |
|   14275 |  4086 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4087 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14275 |  4088 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7135 |  4089 | `	}` |
|   14277 |  4090 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4091 | `		/* Syntax error */` |
|       6 |  4092 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4093 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4094 | `		if( rc == SXERR_ABORT ){` |
|       - |  4095 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4096 | `			return SXERR_ABORT;` |
|       - |  4097 | `		}` |
|       6 |  4098 | `		return SXRET_OK;` |
|       - |  4099 | `	}` |
|       - |  4100 | `	/* Jump the trailing ';' */` |
|   14273 |  4101 | `	pGen->pIn++;` |
|       - |  4102 | `	/* Save the post condition stream */` |
|   14273 |  4103 | `	pPostStart = pGen->pIn;` |
|       - |  4104 | `	/* Compile the loop body */` |
|   14273 |  4105 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   14273 |  4106 | `	pGen->pEnd = pTmp;` |
|   14273 |  4107 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   14273 |  4108 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4109 | `		return SXERR_ABORT;` |
|       - |  4110 | `	}` |
|       - |  4111 | `	/* Fix post-continue jumps */` |
|   14273 |  4112 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4113 | `		JumpFixup *aPost;` |
|       - |  4114 | `		VmInstr *pInstr;` |
|       - |  4115 | `		sxu32 nJumpDest;` |
|       - |  4116 | `		sxu32 n;` |
|      14 |  4117 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4118 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4119 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4120 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4121 | `			if( pInstr ){` |
|       - |  4122 | `				/* Fix jump */` |
|      14 |  4123 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4124 | `			}` |
|       8 |  4125 | `		}` |
|       6 |  4126 | `	}` |
|       - |  4127 | `	/* compile the post-expressions if available */` |
|   14273 |  4128 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4129 | `		pPostStart++;` |
|     ! 0 |  4130 | `	}` |
|   14273 |  4131 | `	if( pPostStart < pEnd ){` |
|       - |  4132 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   14273 |  4133 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   14273 |  4134 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14273 |  4135 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4136 | `			/* Syntax error */` |
|     ! 0 |  4137 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4138 | `			if( rc == SXERR_ABORT ){` |
|       - |  4139 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4140 | `				return SXERR_ABORT;` |
|       - |  4141 | `			}` |
|     ! 0 |  4142 | `			return SXRET_OK;` |
|       - |  4143 | `		}` |
|   14273 |  4144 | `		RE_SWAP_DELIMITER(pGen);` |
|   14273 |  4145 | `		if( rc == SXERR_ABORT ){` |
|       - |  4146 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4147 | `			return SXERR_ABORT;` |
|   14273 |  4148 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4149 | `			/* Pop operand lvalue */` |
|   14273 |  4150 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7134 |  4151 | `		}` |
|    7134 |  4152 | `	}` |
|       - |  4153 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14273 |  4154 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4155 | `	/* Fix all jumps now the destination is resolved */` |
|   14273 |  4156 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4157 | `	/* Release the loop block */` |
|   14273 |  4158 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4159 | `	/* Statement successfully compiled */` |
|   14273 |  4160 | `	return SXRET_OK;` |
|    7141 |  4161 |  |
|       - |  4162 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4163 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4164 | ` * are allowed.` |
|       - |  4165 | ` */` |
|    7654 |  4166 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4167 |  |
|    7659 |  4168 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7659 |  4169 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4170 | `		/* Unexpected expression */` |
|     ! 0 |  4171 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4172 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4173 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4174 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4175 | `		}` |
|     ! 0 |  4176 | `	}` |
|    7659 |  4177 | `	return rc;` |
|       5 |  4178 |  |
|       - |  4179 | `/*` |
|       - |  4180 | ` * Compile the 'foreach' statement.` |
|       - |  4181 | ` * According to the PHP language reference` |
|       - |  4182 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4183 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4184 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4185 | ` *  is a minor but useful extension of the first:` |
|       - |  4186 | ` *  foreach (array_expression as $value)` |
|       - |  4187 | ` *    statement` |
|       - |  4188 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4189 | ` *   statement` |
|       - |  4190 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4191 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4192 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4193 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4194 | ` *  to the variable $key on each loop.` |
|       - |  4195 | ` *  Note:` |
|       - |  4196 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4197 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4198 | ` *  Note:` |
|       - |  4199 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4200 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4201 | ` *  or after the foreach without resetting it.` |
|       - |  4202 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4203 | ` *  of copying the value.` |
|       - |  4204 | ` */` |
|    3922 |  4205 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4206 |  |
|    3927 |  4207 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3927 |  4208 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3927 |  4209 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4210 | `	ph7_foreach_info *pInfo;` |
|       - |  4211 | `	sxu32 nFalseJump;` |
|       - |  4212 | `	VmInstr *pInstr;` |
|       - |  4213 | `	sxu32 nLine;` |
|       - |  4214 | `	sxi32 rc;` |
|    3927 |  4215 | `	nLine = pGen->pIn->nLine;` |
|       - |  4216 | `	/* Jump the 'foreach' keyword */` |
|    3927 |  4217 | `	pGen->pIn++;` |
|    3927 |  4218 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4219 | `		/* Syntax error */` |
|     ! 0 |  4220 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4221 | `		if( rc == SXERR_ABORT ){` |
|       - |  4222 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4223 | `			return SXERR_ABORT;` |
|       - |  4224 | `		}` |
|     ! 0 |  4225 | `		goto Synchronize;` |
|       - |  4226 | `	}` |
|       - |  4227 | `	/* Jump the left parenthesis '(' */` |
|    3927 |  4228 | `	pGen->pIn++;` |
|       - |  4229 | `	/* Create the loop block */` |
|    3927 |  4230 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3927 |  4231 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4232 | `		return SXERR_ABORT;` |
|       - |  4233 | `	}` |
|       - |  4234 | `	/* Delimit the expression */` |
|    3927 |  4235 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3927 |  4236 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4237 | `		/* Empty expression */` |
|     ! 0 |  4238 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4239 | `		if( rc == SXERR_ABORT ){` |
|       - |  4240 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4241 | `			return SXERR_ABORT;` |
|       - |  4242 | `		}` |
|       - |  4243 | `		/* Synchronize */` |
|     ! 0 |  4244 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4245 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4246 | `			pGen->pIn++;` |
|     ! 0 |  4247 | `		}` |
|     ! 0 |  4248 | `		return SXRET_OK;` |
|       - |  4249 | `	}` |
|       - |  4250 | `	/* Compile the array expression */` |
|    3927 |  4251 | `	pCur = pGen->pIn;` |
|   26955 |  4252 | `	while( pCur < pEnd ){` |
|   26955 |  4253 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3941 |  4254 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3941 |  4255 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4256 | `				/* Break with the first 'as' found */` |
|    3927 |  4257 | `				break;` |
|       - |  4258 | `			}` |
|       7 |  4259 | `		}` |
|       - |  4260 | `		/* Advance the stream cursor */` |
|   23033 |  4261 | `		pCur++;` |
|       5 |  4262 | `	}` |
|    3927 |  4263 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4264 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4265 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4266 | `		if( rc == SXERR_ABORT ){` |
|       - |  4267 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4268 | `			return SXERR_ABORT;` |
|       - |  4269 | `		}` |
|     ! 0 |  4270 | `		goto Synchronize;` |
|       - |  4271 | `	}` |
|       - |  4272 | `	/* Swap token streams */` |
|    3927 |  4273 | `	pTmp = pGen->pEnd;` |
|    3927 |  4274 | `	pGen->pEnd = pCur;` |
|    3927 |  4275 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3927 |  4276 | `	if( rc == SXERR_ABORT ){` |
|       - |  4277 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4278 | `		return SXERR_ABORT;` |
|       - |  4279 | `	}` |
|       - |  4280 | `	/* Update token stream */` |
|    3927 |  4281 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4282 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4283 | `		if( rc == SXERR_ABORT ){` |
|       - |  4284 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4285 | `			return SXERR_ABORT;` |
|       - |  4286 | `		}` |
|     ! 0 |  4287 | `		pGen->pIn++;` |
|     ! 0 |  4288 | `	}` |
|    3927 |  4289 | `	pCur++; /* Jump the 'as' keyword */` |
|    3927 |  4290 | `	pGen->pIn = pCur;` |
|    3927 |  4291 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4292 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4293 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4294 | `			return SXERR_ABORT;` |
|       - |  4295 | `		}` |
|     ! 0 |  4296 | `	}` |
|       - |  4297 | `	/* Create the foreach context */` |
|    3927 |  4298 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3927 |  4299 | `	if( pInfo == 0 ){` |
|     ! 0 |  4300 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4301 | `		return SXERR_ABORT;` |
|       - |  4302 | `	}` |
|       - |  4303 | `	/* Zero the structure */` |
|    3927 |  4304 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4305 | `	/* Initialize structure fields */` |
|    3927 |  4306 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4307 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4308 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4309 | `	 * '=>'. */` |
|    3927 |  4310 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    3927 |  4311 | `	if( pCur < pEnd ){` |
|       - |  4312 | `		/* Compile the expression holding the key name */` |
|    3749 |  4313 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4314 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4315 | `			if( rc == SXERR_ABORT ){` |
|       - |  4316 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4317 | `				return SXERR_ABORT;` |
|       - |  4318 | `			}` |
|     ! 0 |  4319 | `		}else{` |
|    3749 |  4320 | `			pGen->pEnd = pCur;` |
|    3749 |  4321 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3749 |  4322 | `			if( rc == SXERR_ABORT ){` |
|       - |  4323 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4324 | `				return SXERR_ABORT;` |
|       - |  4325 | `			}` |
|    3749 |  4326 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3749 |  4327 | `			if( pInstr->p3 ){` |
|       - |  4328 | `				/* Record key name */` |
|    3749 |  4329 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1872 |  4330 | `			}` |
|    3749 |  4331 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4332 | `		}` |
|    3749 |  4333 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1872 |  4334 | `	}` |
|    3927 |  4335 | `	pGen->pEnd = pEnd;` |
|    3927 |  4336 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4337 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4338 | `		if( rc == SXERR_ABORT ){` |
|       - |  4339 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4340 | `			return SXERR_ABORT;` |
|       - |  4341 | `		}` |
|     ! 0 |  4342 | `		goto Synchronize;` |
|       - |  4343 | `	}` |
|    3927 |  4344 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4345 | `		pGen->pIn++;` |
|       - |  4346 | `		/* Pass by reference  */` |
|      11 |  4347 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4348 | `	}` |
|       - |  4349 | `	/* Check if the value target is list() */` |
|    3927 |  4350 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4351 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4352 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4353 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4354 | `		 */` |
|       - |  4355 | `		static int iForeachListCnt = 0;` |
|       - |  4356 | `		char zTmp[128];` |
|       - |  4357 | `		sxu32 nLen;` |
|       - |  4358 | `		char *zDup;` |
|      10 |  4359 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4360 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4361 | `		if( zDup == 0 ){` |
|     ! 0 |  4362 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4363 | `			return SXERR_ABORT;` |
|       - |  4364 | `		}` |
|      10 |  4365 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4366 | `		/* Save list() token boundaries */` |
|      10 |  4367 | `		pListStart = pGen->pIn;` |
|       - |  4368 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4369 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4370 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4371 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4372 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4373 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4374 | `				return SXERR_ABORT;` |
|       - |  4375 | `			}` |
|       3 |  4376 | `			goto Synchronize;` |
|       - |  4377 | `		}` |
|       7 |  4378 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4379 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4380 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4381 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4382 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4383 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4384 | `				return SXERR_ABORT;` |
|       - |  4385 | `			}` |
|     ! 0 |  4386 | `			goto Synchronize;` |
|       - |  4387 | `		}` |
|       7 |  4388 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4389 | `		pListEnd = pGen->pIn;` |
|       7 |  4390 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3922 |  4391 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4392 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4393 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4394 | `		 */` |
|       - |  4395 | `		static int iForeachShortListCnt = 0;` |
|       - |  4396 | `		char zTmp[128];` |
|       - |  4397 | `		sxu32 nLen;` |
|       - |  4398 | `		char *zDup;` |
|       5 |  4399 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       5 |  4400 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       5 |  4401 | `		if( zDup == 0 ){` |
|     ! 0 |  4402 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4403 | `			return SXERR_ABORT;` |
|       - |  4404 | `		}` |
|       5 |  4405 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4406 | `		/* Save [...] token boundaries */` |
|       5 |  4407 | `		pListStart = pGen->pIn;` |
|       - |  4408 | `		/* Advance past [...] */` |
|       5 |  4409 | `		pGen->pIn++; /* Jump '[' */` |
|       5 |  4410 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       5 |  4411 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4412 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4413 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4414 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4415 | `				return SXERR_ABORT;` |
|       - |  4416 | `			}` |
|     ! 0 |  4417 | `			goto Synchronize;` |
|       - |  4418 | `		}` |
|       5 |  4419 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       5 |  4420 | `		pListEnd = pGen->pIn;` |
|       5 |  4421 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       3 |  4422 | `	}else{` |
|       - |  4423 | `		/* Compile the expression holding the value name */` |
|    3915 |  4424 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3915 |  4425 | `		if( rc == SXERR_ABORT ){` |
|       - |  4426 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4427 | `			return SXERR_ABORT;` |
|       - |  4428 | `		}` |
|    3915 |  4429 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3915 |  4430 | `		if( pInstr->p3 ){` |
|       - |  4431 | `			/* Record value name */` |
|    3915 |  4432 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1955 |  4433 | `		}` |
|       - |  4434 | `	}` |
|       - |  4435 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3925 |  4436 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4437 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3925 |  4438 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4439 | `	/* Record the first instruction to execute */` |
|    3925 |  4440 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4441 | `	/* Emit the FOREACH_STEP instruction */` |
|    3925 |  4442 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4443 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3925 |  4444 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4445 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3925 |  4446 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4447 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4448 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4449 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4450 | `		 */` |
|      11 |  4451 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4452 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4453 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4454 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4455 | `		 */` |
|      11 |  4456 | `		pSavedIn = pGen->pIn;` |
|      11 |  4457 | `		pSavedEnd = pGen->pEnd;` |
|      11 |  4458 | `		pGen->pIn = pListStart;` |
|      11 |  4459 | `		pGen->pEnd = pListEnd;` |
|      11 |  4460 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       5 |  4461 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       3 |  4462 | `		}else{` |
|       7 |  4463 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4464 | `		}` |
|      11 |  4465 | `		pGen->pIn = pSavedIn;` |
|      11 |  4466 | `		pGen->pEnd = pSavedEnd;` |
|      11 |  4467 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4468 | `			return SXERR_ABORT;` |
|       - |  4469 | `		}` |
|       - |  4470 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      11 |  4471 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       5 |  4472 | `	}` |
|       - |  4473 | `	/* Compile the loop body */` |
|    3925 |  4474 | `	pGen->pIn = &pEnd[1];` |
|    3925 |  4475 | `	pGen->pEnd = pTmp;` |
|    3925 |  4476 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3925 |  4477 | `	if( rc == SXERR_ABORT ){` |
|       - |  4478 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4479 | `		return SXERR_ABORT;` |
|       - |  4480 | `	}` |
|       - |  4481 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3925 |  4482 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4483 | `	/* Fix all jumps now the destination is resolved */` |
|    3925 |  4484 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4485 | `	/* Release the loop block */` |
|    3925 |  4486 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4487 | `	/* Statement successfully compiled */` |
|    3925 |  4488 | `	return SXRET_OK;` |
|       1 |  4489 | `Synchronize:` |
|       - |  4490 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4491 | `	 * compiling this erroneous block.` |
|       - |  4492 | `	 */` |
|       3 |  4493 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4494 | `		pGen->pIn++;` |
|     ! 0 |  4495 | `	}` |
|       3 |  4496 | `	return SXRET_OK;` |
|    1966 |  4497 |  |
|       - |  4498 | `/*` |
|       - |  4499 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4500 | ` * According to the PHP language reference` |
|       - |  4501 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4502 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4503 | ` *  that is similar to that of C:` |
|       - |  4504 | ` *  if (expr)` |
|       - |  4505 | ` *   statement` |
|       - |  4506 | ` *  else construct:` |
|       - |  4507 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4508 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4509 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4510 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4511 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4512 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4513 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4514 | ` *  elseif` |
|       - |  4515 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4516 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4517 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4518 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4519 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4520 | ` *   <?php` |
|       - |  4521 | ` *    if ($a > $b) {` |
|       - |  4522 | ` *     echo "a is bigger than b";` |
|       - |  4523 | ` *    } elseif ($a == $b) {` |
|       - |  4524 | ` *     echo "a is equal to b";` |
|       - |  4525 | ` *    } else {` |
|       - |  4526 | ` *     echo "a is smaller than b";` |
|       - |  4527 | ` *    }` |
|       - |  4528 | ` *    ?>` |
|       - |  4529 | ` */` |
|  148336 |  4530 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4531 |  |
|  148341 |  4532 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  148341 |  4533 | `	GenBlock *pCondBlock = 0;` |
|       - |  4534 | `	sxu32 nJumpIdx;` |
|       - |  4535 | `	sxu32 nKeyID;` |
|       - |  4536 | `	sxi32 rc;` |
|       - |  4537 | `	/* Jump the 'if' keyword */` |
|  148341 |  4538 | `	pGen->pIn++;` |
|  148341 |  4539 | `	pToken = pGen->pIn;` |
|       - |  4540 | `	/* Create the conditional block */` |
|  148341 |  4541 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  148341 |  4542 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4543 | `		return SXERR_ABORT;` |
|       - |  4544 | `	}` |
|       - |  4545 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   81301 |  4546 | `	for(;;){` |
|  162607 |  4547 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4548 | `			/* Syntax error */` |
|     ! 0 |  4549 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4550 | `				pToken--;` |
|     ! 0 |  4551 | `			}` |
|     ! 0 |  4552 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4553 | `			if( rc == SXERR_ABORT ){` |
|       - |  4554 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4555 | `				return SXERR_ABORT;` |
|       - |  4556 | `			}` |
|     ! 0 |  4557 | `			goto Synchronize;` |
|       - |  4558 | `		}` |
|       - |  4559 | `		/* Jump the left parenthesis '(' */` |
|  162607 |  4560 | `		pToken++;` |
|       - |  4561 | `		/* Delimit the condition */` |
|  162607 |  4562 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  162607 |  4563 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4564 | `			/* Syntax error */` |
|     ! 0 |  4565 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4566 | `				pToken--;` |
|     ! 0 |  4567 | `			}` |
|     ! 0 |  4568 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4569 | `			if( rc == SXERR_ABORT ){` |
|       - |  4570 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4571 | `				return SXERR_ABORT;` |
|       - |  4572 | `			}` |
|     ! 0 |  4573 | `			goto Synchronize;` |
|       - |  4574 | `		}` |
|       - |  4575 | `		/* Swap token streams */` |
|  162607 |  4576 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4577 | `		/* Compile the condition */` |
|  162607 |  4578 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4579 | `		/* Update token stream */` |
|  162607 |  4580 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4581 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4582 | `			pGen->pIn++;` |
|     ! 0 |  4583 | `		}` |
|  162607 |  4584 | `		pGen->pIn  = &pEnd[1];` |
|  162607 |  4585 | `		pGen->pEnd = pTmp;` |
|  162607 |  4586 | `		if( rc == SXERR_ABORT ){` |
|       - |  4587 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4588 | `			return SXERR_ABORT;` |
|       - |  4589 | `		}` |
|       - |  4590 | `		/* Emit the false jump */` |
|  162607 |  4591 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4592 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  162607 |  4593 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4594 | `		/* Compile the body */` |
|  162607 |  4595 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  162607 |  4596 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4597 | `			return SXERR_ABORT;` |
|       - |  4598 | `		}` |
|  162607 |  4599 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   45328 |  4600 | `			break;` |
|       - |  4601 | `		}` |
|       - |  4602 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   71961 |  4603 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   71961 |  4604 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   46317 |  4605 | `			break;` |
|       - |  4606 | `		}` |
|       - |  4607 | `		/* Emit the unconditional jump */` |
|   25649 |  4608 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4609 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   25649 |  4610 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   25649 |  4611 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   18459 |  4612 | `			pToken = &pGen->pIn[1];` |
|   18459 |  4613 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7128 |  4614 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5694 |  4615 | `					break;` |
|       - |  4616 | `			}` |
|    7081 |  4617 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3538 |  4618 | `		}` |
|   14271 |  4619 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4620 | `		/* Synchronize cursors */` |
|   14271 |  4621 | `		pToken = pGen->pIn;` |
|       - |  4622 | `		/* Fix the false jump */` |
|   14271 |  4623 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4624 | `	} /* For(;;) */` |
|       - |  4625 | `	/* Fix the false jump */` |
|  148341 |  4626 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  148341 |  4627 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   57690 |  4628 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4629 | `			/* Compile the else block */` |
|   11383 |  4630 | `			pGen->pIn++;` |
|   11383 |  4631 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11383 |  4632 | `			if( rc == SXERR_ABORT ){` |
|       - |  4633 |  |
|     ! 0 |  4634 | `				return SXERR_ABORT;` |
|       - |  4635 | `			}` |
|    5689 |  4636 | `	}` |
|  148341 |  4637 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4638 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  148341 |  4639 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4640 | `	/* Release the conditional block */` |
|  148341 |  4641 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4642 | `	/* Statement successfully compiled */` |
|  148341 |  4643 | `	return SXRET_OK;` |
|     ! 0 |  4644 | `Synchronize:` |
|       - |  4645 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4646 | `	 */` |
|     ! 0 |  4647 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4648 | `		pGen->pIn++;` |
|     ! 0 |  4649 | `	}` |
|     ! 0 |  4650 | `	return SXRET_OK;` |
|   74173 |  4651 |  |
|       - |  4652 | `/*` |
|       - |  4653 | ` * Compile the global construct.` |
|       - |  4654 | ` * According to the PHP language reference` |
|       - |  4655 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4656 | ` *  to be used in that function.` |
|       - |  4657 | ` *  Example #1 Using global` |
|       - |  4658 | ` *  <?php` |
|       - |  4659 | ` *   $a = 1;` |
|       - |  4660 | ` *   $b = 2;` |
|       - |  4661 | ` *   function Sum()` |
|       - |  4662 | ` *   {` |
|       - |  4663 | ` *    global $a, $b;` |
|       - |  4664 | ` *    $b = $a + $b;` |
|       - |  4665 | ` *   }` |
|       - |  4666 | ` *   Sum();` |
|       - |  4667 | ` *   echo $b;` |
|       - |  4668 | ` *  ?>` |
|       - |  4669 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4670 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4671 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4672 | ` */` |
|      36 |  4673 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4674 |  |
|      41 |  4675 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4676 | `	sxi32 nExpr;` |
|       - |  4677 | `	sxi32 rc;` |
|       - |  4678 | `	/* Jump the 'global' keyword */` |
|      41 |  4679 | `	pGen->pIn++;` |
|      41 |  4680 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4681 | `		/* Nothing to process */` |
|     ! 0 |  4682 | `		return SXRET_OK;` |
|       - |  4683 | `	}` |
|      41 |  4684 | `	pTmp = pGen->pEnd;` |
|      41 |  4685 | `	nExpr = 0;` |
|      87 |  4686 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4687 | `		if( pGen->pIn < pNext ){` |
|      51 |  4688 | `			pGen->pEnd = pNext;` |
|      51 |  4689 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4690 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4691 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4692 | `					return SXERR_ABORT;` |
|       - |  4693 | `				}` |
|     ! 0 |  4694 | `			}else{` |
|      51 |  4695 | `				pGen->pIn++;` |
|      51 |  4696 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4697 | `					/* Emit a warning */` |
|     ! 0 |  4698 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4699 | `				}else{` |
|      51 |  4700 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4701 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4702 | `						return SXERR_ABORT;` |
|      51 |  4703 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4704 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4705 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4706 | `							/* Variable name, not a constant */` |
|      51 |  4707 | `							pLast->iP1 = 0;` |
|      23 |  4708 | `						}` |
|      51 |  4709 | `						nExpr++;` |
|      23 |  4710 | `					}` |
|       - |  4711 | `				}` |
|       - |  4712 | `			}` |
|      23 |  4713 | `		}` |
|       - |  4714 | `		/* Next expression in the stream */` |
|      51 |  4715 | `		pGen->pIn = pNext;` |
|       - |  4716 | `		/* Jump trailing commas */` |
|      61 |  4717 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4718 | `			pGen->pIn++;` |
|       5 |  4719 | `		}` |
|       5 |  4720 | `	}` |
|       - |  4721 | `	/* Restore token stream */` |
|      41 |  4722 | `	pGen->pEnd = pTmp;` |
|      41 |  4723 | `	if( nExpr > 0 ){` |
|       - |  4724 | `		/* Emit the uplink instruction */` |
|      41 |  4725 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4726 | `	}` |
|      41 |  4727 | `	return SXRET_OK;` |
|      23 |  4728 |  |
|       - |  4729 | `/*` |
|       - |  4730 | ` * Compile the return statement.` |
|       - |  4731 | ` * According to the PHP language reference` |
|       - |  4732 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4733 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4734 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4735 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4736 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4737 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4738 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4739 | ` *  from within the main script file, then script execution end.` |
|       - |  4740 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4741 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4742 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4743 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4744 | ` */` |
|  220738 |  4745 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4746 |  |
|  220743 |  4747 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4748 | `	sxi32 rc;` |
|       - |  4749 | `	/* Jump the 'return' keyword */` |
|  220743 |  4750 | `	pGen->pIn++;` |
|  220743 |  4751 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4752 | `		/* Compile the expression */` |
|  220715 |  4753 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  220715 |  4754 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4755 | `			return SXERR_ABORT;` |
|  220715 |  4756 | `		}else if(rc != SXERR_EMPTY ){` |
|  220715 |  4757 | `			nRet = 1;` |
|  110355 |  4758 | `		}` |
|  110355 |  4759 | `	}` |
|       - |  4760 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4761 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4762 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4763 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4764 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  220743 |  4765 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  220743 |  4766 | `	return SXRET_OK;` |
|  110374 |  4767 |  |
|       - |  4768 | `/*` |
|       - |  4769 | ` * Compile a yield expression.` |
|       - |  4770 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4771 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4772 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4773 | ` */` |
|     170 |  4774 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4775 |  |
|       - |  4776 | `	SyToken *pTmp, *pSplit;` |
|     175 |  4777 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     175 |  4778 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4779 | `	sxi32 rc;` |
|      85 |  4780 | `	(void)iCompileFlag;` |
|       - |  4781 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     175 |  4782 | `	pGen->pIn++;` |
|       - |  4783 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4784 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4785 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4786 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4787 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     170 |  4788 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     102 |  4789 | `		&& pGen->pIn->sData.nByte == 4` |
|      41 |  4790 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      40 |  4791 | `		pGen->pIn++; /* Skip 'from' */` |
|      40 |  4792 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      40 |  4793 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4794 | `			return SXERR_ABORT;` |
|       - |  4795 | `		}` |
|      40 |  4796 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  4797 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  4798 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  4799 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  4800 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4801 | `				return SXERR_ABORT;` |
|       - |  4802 | `			}` |
|     ! 0 |  4803 | `		}` |
|      40 |  4804 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      40 |  4805 | `		return SXRET_OK;` |
|       - |  4806 | `	}` |
|     139 |  4807 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4808 | `		/* Bare yield — no value */` |
|       3 |  4809 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  4810 | `		return SXRET_OK;` |
|       - |  4811 | `	}` |
|       - |  4812 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     137 |  4813 | `	pSplit = 0;` |
|       - |  4814 | `	{` |
|     137 |  4815 | `		SyToken *pCur = pGen->pIn;` |
|     137 |  4816 | `		sxi32 nNest = 0;` |
|     285 |  4817 | `		while( pCur < pGen->pEnd ){` |
|     167 |  4818 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4819 | `				nNest++;` |
|     167 |  4820 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4821 | `				nNest--;` |
|     167 |  4822 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4823 | `				pSplit = pCur;` |
|      16 |  4824 | `				break;` |
|       - |  4825 | `			}` |
|     153 |  4826 | `			pCur++;` |
|       5 |  4827 | `		}` |
|       - |  4828 | `	}` |
|     137 |  4829 | `	pTmp = pGen->pEnd;` |
|     137 |  4830 | `	if( pSplit ){` |
|       - |  4831 | `		/* yield $key => $value */` |
|      16 |  4832 | `		pGen->pEnd = pSplit;` |
|      16 |  4833 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4834 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4835 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4836 | `		pGen->pEnd = pTmp;` |
|      16 |  4837 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4838 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4839 | `		iP1 = 1;` |
|      16 |  4840 | `		iP2 = 1;` |
|       9 |  4841 | `	}else{` |
|       - |  4842 | `		/* yield $value */` |
|     123 |  4843 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     123 |  4844 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     123 |  4845 | `		if( rc != SXERR_EMPTY ){` |
|     123 |  4846 | `			iP1 = 1;` |
|      59 |  4847 | `		}` |
|       - |  4848 | `	}` |
|     137 |  4849 | `	pGen->pEnd = pTmp;` |
|     137 |  4850 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     137 |  4851 | `	return SXRET_OK;` |
|      90 |  4852 |  |
|       - |  4853 | `/*` |
|       - |  4854 | ` * Compile the die/exit language construct.` |
|       - |  4855 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4856 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4857 | ` */` |
|     120 |  4858 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4859 |  |
|     125 |  4860 | `	sxi32 nExpr = 0;` |
|       - |  4861 | `	sxi32 rc;` |
|       - |  4862 | `	/* Jump the die/exit keyword */` |
|     125 |  4863 | `	pGen->pIn++;` |
|     125 |  4864 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4865 | `		/* Compile the expression */` |
|     125 |  4866 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4867 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4868 | `			return SXERR_ABORT;` |
|     125 |  4869 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4870 | `			nExpr = 1;` |
|      60 |  4871 | `		}` |
|      60 |  4872 | `	}` |
|       - |  4873 | `	/* Emit the HALT instruction */` |
|     125 |  4874 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4875 | `	return SXRET_OK;` |
|      65 |  4876 |  |
|       - |  4877 | `/*` |
|       - |  4878 | ` * Compile the 'echo' language construct.` |
|       - |  4879 | ` */` |
|   14504 |  4880 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4881 |  |
|   14509 |  4882 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4883 | `	sxi32 rc;` |
|       - |  4884 | `	/* Jump the 'echo' keyword */` |
|   14509 |  4885 | `	pGen->pIn++;` |
|       - |  4886 | `	/* Compile arguments one after one */` |
|   14509 |  4887 | `	pTmp = pGen->pEnd;` |
|   31927 |  4888 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   17423 |  4889 | `		if( pGen->pIn < pNext ){` |
|   17423 |  4890 | `			pGen->pEnd = pNext;` |
|   17423 |  4891 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   17423 |  4892 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4893 | `				return SXERR_ABORT;` |
|   17423 |  4894 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4895 | `				/* Emit the consume instruction */` |
|   17399 |  4896 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    8697 |  4897 | `			}` |
|    8709 |  4898 | `		}` |
|       - |  4899 | `		/* Jump trailing commas */` |
|   20337 |  4900 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    2919 |  4901 | `			pNext++;` |
|       5 |  4902 | `		}` |
|   17423 |  4903 | `		pGen->pIn = pNext;` |
|       5 |  4904 | `	}` |
|       - |  4905 | `	/* Restore token stream */` |
|   14509 |  4906 | `	pGen->pEnd = pTmp;` |
|   14509 |  4907 | `	return SXRET_OK;` |
|    7257 |  4908 |  |
|       - |  4909 | `/*` |
|       - |  4910 | ` * Compile the static statement.` |
|       - |  4911 | ` * According to the PHP language reference` |
|       - |  4912 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4913 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4914 | ` *  when program execution leaves this scope.` |
|       - |  4915 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4916 | ` * Symisc eXtension.` |
|       - |  4917 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4918 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4919 | ` *  Example` |
|       - |  4920 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4921 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4922 | ` */` |
|       6 |  4923 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       2 |  4924 |  |
|       - |  4925 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4926 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4927 | `	GenBlock *pBlock;` |
|       - |  4928 | `	SyString *pName;` |
|       - |  4929 | `	char *zDup;` |
|       - |  4930 | `	sxu32 nLine;` |
|       - |  4931 | `	sxi32 rc;` |
|       - |  4932 | `	/* Jump the static keyword */` |
|       8 |  4933 | `	nLine = pGen->pIn->nLine;` |
|       8 |  4934 | `	pGen->pIn++;` |
|       - |  4935 | `	/* Extract the enclosing function if any */` |
|       8 |  4936 | `	pBlock = pGen->pCurrent;` |
|      14 |  4937 | `	while( pBlock ){` |
|      14 |  4938 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       8 |  4939 | `			break;` |
|       - |  4940 | `		}` |
|       - |  4941 | `		/* Point to the upper block */` |
|       8 |  4942 | `		pBlock = pBlock->pParent;` |
|       2 |  4943 | `	}` |
|       8 |  4944 | `	if( pBlock == 0 ){` |
|       - |  4945 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4946 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4947 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4948 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4949 | `				return SXERR_ABORT;` |
|       - |  4950 | `			}` |
|     ! 0 |  4951 | `			goto Synchronize;` |
|       - |  4952 | `		}` |
|       - |  4953 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4954 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4955 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4956 | `			return SXERR_ABORT;` |
|     ! 0 |  4957 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4958 | `			/* Emit the POP instruction */` |
|     ! 0 |  4959 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4960 | `		}` |
|     ! 0 |  4961 | `		return SXRET_OK;` |
|       - |  4962 | `	}` |
|       8 |  4963 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4964 | `	/* Make sure we are dealing with a valid statement */` |
|       8 |  4965 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       4 |  4966 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4967 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4968 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4969 | `				return SXERR_ABORT;` |
|       - |  4970 | `			}` |
|       3 |  4971 | `			goto Synchronize;` |
|       - |  4972 | `	}` |
|       5 |  4973 | `	pGen->pIn++;` |
|       - |  4974 | `	/* Extract variable name */` |
|       5 |  4975 | `	pName = &pGen->pIn->sData;` |
|       5 |  4976 | `	pGen->pIn++; /* Jump the var name */` |
|       5 |  4977 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4978 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4979 | `		goto Synchronize;` |
|       - |  4980 | `	}` |
|       - |  4981 | `	/* Initialize the structure describing the static variable */` |
|       5 |  4982 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       5 |  4983 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4984 | `	/* Duplicate variable name */` |
|       5 |  4985 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       5 |  4986 | `	if( zDup == 0 ){` |
|     ! 0 |  4987 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4988 | `		return SXERR_ABORT;` |
|       - |  4989 | `	}` |
|       5 |  4990 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4991 | `	/* Check if we have an expression to compile */` |
|       5 |  4992 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4993 | `		SySet *pInstrContainer;` |
|       - |  4994 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4995 | `		 * Static variable can take any complex expression including function` |
|       - |  4996 | `		 * call as their initialization value.` |
|       - |  4997 | `		 * Example:` |
|       - |  4998 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4999 | `		 */` |
|       5 |  5000 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5001 | `		/* Swap bytecode container */` |
|       5 |  5002 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       5 |  5003 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5004 | `		/* Compile the expression */` |
|       5 |  5005 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5006 | `		/* Emit the done instruction */` |
|       5 |  5007 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5008 | `		/* Restore default bytecode container */` |
|       5 |  5009 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       2 |  5010 | `	}` |
|       - |  5011 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       5 |  5012 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       5 |  5013 | `	return SXRET_OK;` |
|       1 |  5014 | `Synchronize:` |
|       - |  5015 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5016 | `	 * statement.` |
|       - |  5017 | `	 */` |
|       5 |  5018 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5019 | `		pGen->pIn++;` |
|       1 |  5020 | `	}` |
|       3 |  5021 | `	return SXRET_OK;` |
|       5 |  5022 |  |
|       - |  5023 | `/*` |
|       - |  5024 | ` * Compile the var statement.` |
|       - |  5025 | ` * Symisc Extension:` |
|       - |  5026 | ` *      var statement can be used outside of a class definition.` |
|       - |  5027 | ` */` |
|       4 |  5028 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5029 |  |
|       - |  5030 | `	sxu32 nLine;` |
|       - |  5031 | `	sxi32 rc;` |
|       5 |  5032 | `	nLine = pGen->pIn->nLine;` |
|       - |  5033 | `	/* Jump the 'var' keyword */` |
|       5 |  5034 | `	pGen->pIn++;` |
|       5 |  5035 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5036 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5037 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5038 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5039 | `			pGen->pIn++;` |
|     ! 0 |  5040 | `		}` |
|     ! 0 |  5041 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5042 | `			return SXERR_ABORT;` |
|       - |  5043 | `		}` |
|     ! 0 |  5044 | `	}else{` |
|       - |  5045 | `		/* Compile the expression */` |
|       5 |  5046 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5047 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5048 | `			return SXERR_ABORT;` |
|       5 |  5049 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5050 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5051 | `		}` |
|       - |  5052 | `	}` |
|       5 |  5053 | `	return SXRET_OK;` |
|       3 |  5054 |  |
|       - |  5055 | `/*` |
|       - |  5056 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5057 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5058 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5059 | ` */` |
|       - |  5060 | `/*` |
|       - |  5061 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5062 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5063 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5064 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5065 | ` *` |
|       - |  5066 | ` * Resolution order:` |
|       - |  5067 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5068 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5069 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5070 | ` *` |
|       - |  5071 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5072 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5073 | ` * Returns the (possibly new) literal index.` |
|       - |  5074 | ` */` |
|  438346 |  5075 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5076 |  |
|       - |  5077 | `	ph7_value *pLit;` |
|       - |  5078 | `	const char *zLit;` |
|       - |  5079 | `	SyString sQualified;` |
|       - |  5080 | `	sxu32 nLit;` |
|       - |  5081 | `	sxu32 k;` |
|       - |  5082 | `	sxu32 nNewIdx;` |
|       - |  5083 | `	int hasNsSep;` |
|       - |  5084 | `	SyHashEntry *pImport;` |
|       - |  5085 | `	ph7_value *pNew;` |
|  438351 |  5086 | `	if( pFromImport ){` |
|  418893 |  5087 | `		*pFromImport = 0;` |
|  209444 |  5088 | `	}` |
|  438351 |  5089 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  438351 |  5090 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5091 | `		return nOrigIdx;` |
|       - |  5092 | `	}` |
|  438351 |  5093 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  438351 |  5094 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5095 | `	/* Skip if already qualified (contains backslash) */` |
|  438351 |  5096 | `	hasNsSep = 0;` |
| 4740755 |  5097 | `	for( k = 0; k < nLit; k++ ){` |
| 4302417 |  5098 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2151207 |  5099 | `	}` |
|  438351 |  5100 | `	if( hasNsSep ){` |
|      10 |  5101 | `		return nOrigIdx;` |
|       - |  5102 | `	}` |
|       - |  5103 | `	/* Check use imports first (works even outside namespaces) */` |
|  438343 |  5104 | `	SyBlobReset(&pGen->sWorker);` |
|  438343 |  5105 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  438343 |  5106 | `	if( pImport ){` |
|      41 |  5107 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5108 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5109 | `		if( pFromImport ){` |
|      18 |  5110 | `			*pFromImport = 1;` |
|       8 |  5111 | `		}` |
|      23 |  5112 | `	}else{` |
|  438307 |  5113 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  438217 |  5114 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5115 | `		}` |
|       - |  5116 | `		/* Prepend current namespace */` |
|      95 |  5117 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5118 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5119 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5120 | `	}` |
|       - |  5121 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5122 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5123 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5124 | `		return nNewIdx; /* Already interned */` |
|       - |  5125 | `	}` |
|      79 |  5126 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5127 | `	if( pNew == 0 ){` |
|     ! 0 |  5128 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5129 | `	}` |
|      79 |  5130 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5131 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5132 | `	return nNewIdx;` |
|  219178 |  5133 |  |
|       - |  5134 | `/*` |
|       - |  5135 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5136 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5137 | ` */` |
|   96446 |  5138 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5139 |  |
|       - |  5140 | `	SyHashEntry *pImport;` |
|       - |  5141 | `	/* Check use imports first */` |
|   96451 |  5142 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   96451 |  5143 | `	if( pImport ){` |
|      14 |  5144 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      14 |  5145 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      14 |  5146 | `		return;` |
|       - |  5147 | `	}` |
|       - |  5148 | `	/* Prepend current namespace if active */` |
|   96439 |  5149 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5150 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5151 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5152 | `	}` |
|   96439 |  5153 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   48228 |  5154 |  |
|       - |  5155 | `/*` |
|       - |  5156 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5157 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5158 | ` * The caller must release pOut when done.` |
|       - |  5159 | ` */` |
|  139328 |  5160 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5161 |  |
|  139333 |  5162 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5163 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5164 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5165 | `	}` |
|  139333 |  5166 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  139333 |  5167 |  |
|       - |  5168 | `/*` |
|       - |  5169 | ` * Compile a namespace statement` |
|       - |  5170 | ` * According to the PHP language reference manual` |
|       - |  5171 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5172 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5173 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5174 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5175 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5176 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5177 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5178 | ` *  programming world.` |
|       - |  5179 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5180 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5181 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5182 | ` *  classes/functions/constants.` |
|       - |  5183 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5184 | ` *  readability of source code.` |
|       - |  5185 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5186 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5187 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5188 | ` *       class MyClass {}` |
|       - |  5189 | ` *       function myfunction() {}` |
|       - |  5190 | ` *       const MYCONST = 1;` |
|       - |  5191 | ` *       $a = new MyClass;` |
|       - |  5192 | ` *       $c = new \my\name\MyClass;` |
|       - |  5193 | ` *       $a = strlen('hi');` |
|       - |  5194 | ` *       $d = namespace\MYCONST;` |
|       - |  5195 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5196 | ` *       echo constant($d);` |
|       - |  5197 | ` * NOTE` |
|       - |  5198 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5199 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5200 | ` */` |
|       - |  5201 | `/*` |
|       - |  5202 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5203 | ` */` |
|      14 |  5204 | `static const char * TokenTypeName(sxu32 nType)` |
|       3 |  5205 |  |
|      17 |  5206 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      10 |  5207 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      10 |  5208 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      10 |  5209 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      10 |  5210 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      10 |  5211 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5212 | `	return "token";` |
|      10 |  5213 |  |
|     106 |  5214 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5215 |  |
|       - |  5216 | `	sxu32 nLine;` |
|       - |  5217 | `	sxi32 rc;` |
|     111 |  5218 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5219 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5220 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5221 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5222 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5223 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5224 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5225 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5226 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5227 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5228 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5229 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5230 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5231 | `		return SXRET_OK;` |
|       - |  5232 | `	}` |
|     111 |  5233 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5234 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5235 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5236 | `		return SXRET_OK;` |
|       - |  5237 | `	}` |
|     111 |  5238 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5239 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5240 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5241 | `		return SXRET_OK;` |
|       - |  5242 | `	}` |
|       - |  5243 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5244 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5245 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5246 | `			/* Append backslash separator */` |
|      27 |  5247 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5248 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5249 | `			}` |
|      16 |  5250 | `		}else{` |
|       - |  5251 | `			/* Append identifier */` |
|     131 |  5252 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5253 | `		}` |
|     153 |  5254 | `		pGen->pIn++;` |
|       5 |  5255 | `	}` |
|       - |  5256 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5257 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5258 | `	{` |
|     111 |  5259 | `		char *zNsDup = 0;` |
|     111 |  5260 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5261 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5262 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5263 | `		}` |
|     111 |  5264 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5265 | `	}` |
|     111 |  5266 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5267 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5268 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5269 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5270 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5271 | `			return SXERR_ABORT;` |
|       - |  5272 | `		}` |
|       2 |  5273 | `	}` |
|     111 |  5274 | `	return SXRET_OK;` |
|      58 |  5275 |  |
|       - |  5276 | `/*` |
|       - |  5277 | ` * Compile the 'use' statement` |
|       - |  5278 | ` * According to the PHP language reference manual` |
|       - |  5279 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5280 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5281 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5282 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5283 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5284 | ` *  a function or constant is not supported.` |
|       - |  5285 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5286 | ` * NOTE` |
|       - |  5287 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5288 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5289 | ` */` |
|      68 |  5290 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5291 |  |
|       - |  5292 | `	sxu32 nLine;` |
|       - |  5293 | `	sxi32 rc;` |
|       - |  5294 | `	SyBlob sPath;` |
|       - |  5295 | `	SyString sAlias;` |
|       - |  5296 | `	SyToken *pLast;` |
|       - |  5297 | `	char *zDup;` |
|       - |  5298 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5299 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5300 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5301 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5302 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5303 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5304 | `	iUseType = 0;` |
|      73 |  5305 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5306 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5307 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5308 | `			iUseType = 1;` |
|      16 |  5309 | `			pGen->pIn++;` |
|      23 |  5310 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5311 | `			iUseType = 2;` |
|      16 |  5312 | `			pGen->pIn++;` |
|       7 |  5313 | `		}` |
|      14 |  5314 | `	}` |
|       - |  5315 | `	/* Select target hash tables based on import type */` |
|      73 |  5316 | `	switch( iUseType ){` |
|       7 |  5317 | `		case 1:` |
|      16 |  5318 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5319 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5320 | `			break;` |
|       7 |  5321 | `		case 2:` |
|      16 |  5322 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5323 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5324 | `			break;` |
|      20 |  5325 | `		default:` |
|      45 |  5326 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5327 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5328 | `			break;` |
|       - |  5329 | `	}` |
|      73 |  5330 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5331 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5332 | `	for(;;){` |
|      75 |  5333 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5334 | `			break;` |
|       - |  5335 | `		}` |
|      75 |  5336 | `		SyBlobReset(&sPath);` |
|      75 |  5337 | `		pLast = 0;` |
|       - |  5338 | `		/* Collect the full namespace path */` |
|     261 |  5339 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5340 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5341 | `				pLast = pGen->pIn;` |
|     131 |  5342 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5343 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5344 | `				}` |
|     131 |  5345 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5346 | `			}` |
|     191 |  5347 | `			pGen->pIn++;` |
|       5 |  5348 | `		}` |
|      75 |  5349 | `		if( pLast == 0 ){` |
|       - |  5350 | `			/* Empty path */` |
|       5 |  5351 | `			break;` |
|       - |  5352 | `		}` |
|       - |  5353 | `		/* Default alias is the last component of the path */` |
|      71 |  5354 | `		sAlias = pLast->sData;` |
|       - |  5355 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5356 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5357 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      19 |  5358 | `			pGen->pIn++; /* Jump 'as' */` |
|      19 |  5359 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      19 |  5360 | `				sAlias = pGen->pIn->sData;` |
|      19 |  5361 | `				pGen->pIn++;` |
|       8 |  5362 | `			}` |
|       8 |  5363 | `		}` |
|       - |  5364 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5365 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5366 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5367 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5368 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5369 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5370 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5371 | `				return SXERR_ABORT;` |
|       - |  5372 | `			}` |
|       2 |  5373 | `		}` |
|       - |  5374 | `		/* Register the import: alias -> FQN.` |
|       - |  5375 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5376 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5377 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5378 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5379 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5380 | `		if( zDup ){` |
|      71 |  5381 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5382 | `			if( pVmHash ){` |
|       - |  5383 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5384 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5385 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5386 | `				if( zAliasDup ){` |
|      43 |  5387 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5388 | `				}` |
|      19 |  5389 | `			}` |
|      71 |  5390 | `			if( iUseType == 2 ){` |
|       - |  5391 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5392 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5393 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5394 | `				if( zAliasDup ){` |
|       - |  5395 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5396 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5397 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5398 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5399 | `					if( azPair ){` |
|      16 |  5400 | `						azPair[0] = zAliasDup;` |
|      16 |  5401 | `						azPair[1] = zDup;` |
|      16 |  5402 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5403 | `					}` |
|       7 |  5404 | `				}` |
|       7 |  5405 | `			}` |
|      33 |  5406 | `		}` |
|       - |  5407 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5408 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5409 | `			pGen->pIn++;` |
|       2 |  5410 | `		}else{` |
|      37 |  5411 | `			break;` |
|       - |  5412 | `		}` |
|       1 |  5413 | `	}` |
|      73 |  5414 | `	SyBlobRelease(&sPath);` |
|      73 |  5415 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5416 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5417 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5418 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5419 | `			return SXERR_ABORT;` |
|       - |  5420 | `		}` |
|       1 |  5421 | `	}` |
|      73 |  5422 | `	return SXRET_OK;` |
|      39 |  5423 |  |
|       - |  5424 | `/*` |
|       - |  5425 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5426 | ` *` |
|       - |  5427 | ` * According to the PHP language reference manual.` |
|       - |  5428 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5429 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5430 | ` *  declare (directive)` |
|       - |  5431 | ` *   statement` |
|       - |  5432 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5433 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5434 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5435 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5436 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5437 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5438 | ` * <?php` |
|       - |  5439 | ` * // these are the same:` |
|       - |  5440 | ` * // you can use this:` |
|       - |  5441 | ` * declare(ticks=1) {` |
|       - |  5442 | ` *   // entire script here` |
|       - |  5443 | ` * }` |
|       - |  5444 | ` * // or you can use this:` |
|       - |  5445 | ` * declare(ticks=1);` |
|       - |  5446 | ` * // entire script here` |
|       - |  5447 | ` * ?>` |
|       - |  5448 | ` *` |
|       - |  5449 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5450 | ` */` |
|       - |  5451 | `/*` |
|       - |  5452 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5453 | ` */` |
|      68 |  5454 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5455 |  |
|     103 |  5456 | `	return SyStringLength(pName) == nWant` |
|      68 |  5457 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5458 |  |
|       - |  5459 |  |
|      40 |  5460 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5461 |  |
|      45 |  5462 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5463 | `	SyToken *pBodyEnd = 0;` |
|       - |  5464 | `	SyToken *pBodyStart;` |
|       - |  5465 | `	SyToken *pCursor;` |
|       - |  5466 | `	int bHasStrictTypes;` |
|       - |  5467 | `	int bBlockForm;` |
|       - |  5468 | `	int bPlacementOk;` |
|       - |  5469 | `	sxi32 rc;` |
|      45 |  5470 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5471 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 |  5472 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 |  5473 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5474 | `			return SXERR_ABORT;` |
|       - |  5475 | `		}` |
|       5 |  5476 | `		goto Synchro;` |
|       - |  5477 | `	}` |
|      41 |  5478 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5479 | `	pBodyStart = pGen->pIn;` |
|       - |  5480 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5481 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5482 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5483 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5484 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5485 | `			return SXERR_ABORT;` |
|       - |  5486 | `		}` |
|     ! 0 |  5487 | `		return SXRET_OK;` |
|       - |  5488 | `	}` |
|       - |  5489 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5490 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5491 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5492 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5493 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5494 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5495 | `			return SXERR_ABORT;` |
|       - |  5496 | `		}` |
|     ! 0 |  5497 | `	}` |
|      41 |  5498 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5499 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5500 | `	bHasStrictTypes = 0;` |
|       - |  5501 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5502 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5503 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5504 | `	pCursor = pBodyStart;` |
|      53 |  5505 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5506 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5507 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5508 | `				bHasStrictTypes = 1;` |
|      37 |  5509 | `				break;` |
|       - |  5510 | `			}` |
|       2 |  5511 | `		}` |
|      14 |  5512 | `		pCursor++;` |
|       2 |  5513 | `	}` |
|      41 |  5514 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5515 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5516 | `			"strict_types declaration must not use block mode");` |
|       3 |  5517 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5518 | `		return SXRET_OK;` |
|       - |  5519 | `	}` |
|      39 |  5520 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5521 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5522 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5523 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5524 | `		return SXRET_OK;` |
|       - |  5525 | `	}` |
|       - |  5526 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5527 | `	pCursor = pBodyStart;` |
|      65 |  5528 | `	while( pCursor < pBodyEnd ){` |
|       - |  5529 | `		SyToken *pNameTok;` |
|       - |  5530 | `		SyToken *pEqTok;` |
|       - |  5531 | `		SyToken *pValTok;` |
|       - |  5532 | `		SyString *pDirName;` |
|       - |  5533 | `		int bIsStrict;` |
|       - |  5534 | `		int iStrictValue;` |
|      37 |  5535 | `		pNameTok = pCursor;` |
|      37 |  5536 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5537 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5538 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5539 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5540 | `			return SXRET_OK;` |
|       - |  5541 | `		}` |
|      37 |  5542 | `		pEqTok = pNameTok + 1;` |
|      37 |  5543 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5544 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5545 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5546 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5547 | `			return SXRET_OK;` |
|       - |  5548 | `		}` |
|      37 |  5549 | `		pValTok = pEqTok + 1;` |
|      37 |  5550 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5551 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5552 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5553 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5554 | `			return SXRET_OK;` |
|       - |  5555 | `		}` |
|      37 |  5556 | `		pDirName = &pNameTok->sData;` |
|      37 |  5557 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5558 | `		if( bIsStrict ){` |
|       - |  5559 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5560 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5561 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5562 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5563 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5564 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5565 | `				return SXRET_OK;` |
|       - |  5566 | `			}` |
|      33 |  5567 | `			iStrictValue = -1;` |
|      33 |  5568 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5569 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5570 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5571 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5572 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5573 | `			}` |
|      33 |  5574 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5575 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5576 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5577 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5578 | `				return SXRET_OK;` |
|       - |  5579 | `			}` |
|      30 |  5580 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5581 | `		}else{` |
|       - |  5582 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5583 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5584 | `			 * behavior don't regress. */` |
|       8 |  5585 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5586 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5587 | `				ph7_lib_version()` |
|       - |  5588 | `				);` |
|       - |  5589 | `		}` |
|      35 |  5590 | `		pCursor = pValTok + 1;` |
|       - |  5591 | `		/* Consume separating comma (or end). */` |
|      35 |  5592 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5593 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5594 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5595 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5596 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5597 | `				return SXRET_OK;` |
|       - |  5598 | `			}` |
|       3 |  5599 | `			pCursor++;` |
|       1 |  5600 | `		}` |
|       5 |  5601 | `	}` |
|       - |  5602 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5603 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5604 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5605 | `	return SXRET_OK;` |
|       2 |  5606 | `Synchro:` |
|       - |  5607 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 |  5608 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 |  5609 | `		pGen->pIn++;` |
|       1 |  5610 | `	}` |
|       5 |  5611 | `	return SXRET_OK;` |
|      25 |  5612 |  |
|       - |  5613 | `/*` |
|       - |  5614 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5615 | ` * as follows:` |
|       - |  5616 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5617 | ` * {` |
|       - |  5618 | ` *   return "Making a cup of $type.\n";` |
|       - |  5619 | ` * }` |
|       - |  5620 | ` * Symisc eXtension.` |
|       - |  5621 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5622 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5623 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5624 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5625 | ` *      {` |
|       - |  5626 | ` *       var_dump($a);` |
|       - |  5627 | ` *      }` |
|       - |  5628 | ` *     //call test without args` |
|       - |  5629 | ` *      test();` |
|       - |  5630 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5631 | ` *      Example:` |
|       - |  5632 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5633 | ` * 3 -) Function overloading!!` |
|       - |  5634 | ` *      Example:` |
|       - |  5635 | ` *      function foo($a) {` |
|       - |  5636 | ` *   	  return $a.PHP_EOL;` |
|       - |  5637 | ` *	    }` |
|       - |  5638 | ` *	    function foo($a, $b) {` |
|       - |  5639 | ` *   	  return $a + $b;` |
|       - |  5640 | ` *	    }` |
|       - |  5641 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5642 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5643 | ` *      // Same arg` |
|       - |  5644 | ` *	   function foo(string $a)` |
|       - |  5645 | ` *	   {` |
|       - |  5646 | ` *	     echo "a is a string\n";` |
|       - |  5647 | ` *	     var_dump($a);` |
|       - |  5648 | ` *	   }` |
|       - |  5649 | ` *	  function foo(int $a)` |
|       - |  5650 | ` *	  {` |
|       - |  5651 | ` *	    echo "a is integer\n";` |
|       - |  5652 | ` *	    var_dump($a);` |
|       - |  5653 | ` *	  }` |
|       - |  5654 | ` *	  function foo(array $a)` |
|       - |  5655 | ` *	  {` |
|       - |  5656 | ` * 	    echo "a is an array\n";` |
|       - |  5657 | ` * 	    var_dump($a);` |
|       - |  5658 | ` *	  }` |
|       - |  5659 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5660 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5661 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5662 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5663 | ` * introduced by the PH7 engine.` |
|       - |  5664 | ` */` |
|   67258 |  5665 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5666 |  |
|       - |  5667 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5668 | `	SySet *pInstrContainer;` |
|       - |  5669 | `	sxi32 rc;` |
|       - |  5670 | `	/* Swap token stream */` |
|   67263 |  5671 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   67263 |  5672 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   67263 |  5673 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5674 | `	/* Compile the expression holding the argument value */` |
|   67263 |  5675 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5676 | `	/* Emit the done instruction */` |
|   67263 |  5677 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   67263 |  5678 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   67263 |  5679 | `	RE_SWAP_DELIMITER(pGen);` |
|   67263 |  5680 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5681 | `		return SXERR_ABORT;` |
|       - |  5682 | `	}` |
|   67263 |  5683 | `	return SXRET_OK;` |
|   33634 |  5684 |  |
|       - |  5685 | `/*` |
|       - |  5686 | ` * Collect function arguments one after one.` |
|       - |  5687 | ` * According to the PHP language reference manual.` |
|       - |  5688 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5689 | ` * list of expressions.` |
|       - |  5690 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5691 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5692 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5693 | ` * for more information.` |
|       - |  5694 | ` * Example #1 Passing arrays to functions` |
|       - |  5695 | ` * <?php` |
|       - |  5696 | ` * function takes_array($input)` |
|       - |  5697 | ` * {` |
|       - |  5698 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5699 | ` * }` |
|       - |  5700 | ` * ?>` |
|       - |  5701 | ` * Making arguments be passed by reference` |
|       - |  5702 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5703 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5704 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5705 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5706 | ` * to the argument name in the function definition:` |
|       - |  5707 | ` * Example #2 Passing function parameters by reference` |
|       - |  5708 | ` * <?php` |
|       - |  5709 | ` * function add_some_extra(&$string)` |
|       - |  5710 | ` * {` |
|       - |  5711 | ` *   $string .= 'and something extra.';` |
|       - |  5712 | ` * }` |
|       - |  5713 | ` * $str = 'This is a string, ';` |
|       - |  5714 | ` * add_some_extra($str);` |
|       - |  5715 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5716 | ` * ?>` |
|       - |  5717 | ` *` |
|       - |  5718 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5719 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5720 | ` * on these extension.` |
|       - |  5721 | ` */` |
|   89784 |  5722 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5723 |  |
|       - |  5724 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5725 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5726 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5727 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5728 | `	sxi32 rc;` |
|       - |  5729 |  |
|   89789 |  5730 | `	pIn = pGen->pIn;` |
|   89789 |  5731 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5732 | `	/* Process arguments one after one */` |
|  113143 |  5733 | `	for(;;){` |
|  226291 |  5734 | `		if( pIn >= pEnd ){` |
|       - |  5735 | `			/* No more arguments to process */` |
|   89775 |  5736 | `			break;` |
|       - |  5737 | `		}` |
|  136521 |  5738 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  136521 |  5739 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  136521 |  5740 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  136521 |  5741 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5742 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5743 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5744 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5745 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5746 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5747 | `		{` |
|  136521 |  5748 | `			int bReadonly = 0, bVisSeen = 0;` |
|  136521 |  5749 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  136521 |  5750 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5751 | `				bReadonly = 1;` |
|       3 |  5752 | `				pIn++;` |
|       1 |  5753 | `			}` |
|  136521 |  5754 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   63913 |  5755 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   63913 |  5756 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      71 |  5757 | `					bVisSeen = 1;` |
|      71 |  5758 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      95 |  5759 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      31 |  5760 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      71 |  5761 | `					pIn++;` |
|      71 |  5762 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5763 | `						bReadonly = 1;` |
|      16 |  5764 | `						pIn++;` |
|       6 |  5765 | `					}` |
|      33 |  5766 | `				}` |
|   31954 |  5767 | `			}` |
|  136521 |  5768 | `			if( bVisSeen \|\| bReadonly ){` |
|      73 |  5769 | `				if( !bCtorCtx ){` |
|       6 |  5770 | `					if( bAbstractCtx ){` |
|       3 |  5771 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5772 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5773 | `					}else{` |
|       3 |  5774 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5775 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5776 | `					}` |
|       6 |  5777 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5778 | `						return SXERR_ABORT;` |
|       - |  5779 | `					}` |
|       6 |  5780 | `					return SXERR_SYNTAX;` |
|       - |  5781 | `				}` |
|      69 |  5782 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      69 |  5783 | `				sArg.iPromoteVis = iVis;` |
|      69 |  5784 | `				if( bReadonly ){` |
|      18 |  5785 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5786 | `				}` |
|      32 |  5787 | `			}` |
|       - |  5788 | `		}` |
|       - |  5789 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  136512 |  5790 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  109114 |  5791 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   79939 |  5792 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   78133 |  5793 | `			sxu32 nLineLocal = pIn->nLine;` |
|   78133 |  5794 | `			sxi32 iTFlags = 0;` |
|   78133 |  5795 | `			pGen->pIn = pIn;` |
|   78133 |  5796 | `			rc = GenStateParseUnionTypeDecl(` |
|   39064 |  5797 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   39064 |  5798 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5799 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5800 | `				/* bAllowVoid */ 0,` |
|   39064 |  5801 | `						nLineLocal);` |
|   78133 |  5802 | `			pIn = pGen->pIn;` |
|   78133 |  5803 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5804 | `				return SXERR_ABORT;` |
|   78133 |  5805 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5806 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5807 | `				return SXERR_SYNTAX;` |
|   78131 |  5808 | `			}else if( rc == SXERR_SYNTAX ){` |
|       8 |  5809 | `				if( pIn < pEnd ){` |
|      11 |  5810 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5811 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       3 |  5812 | `						&pIn->sData);` |
|       5 |  5813 | `				}else{` |
|     ! 0 |  5814 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5815 | `						"syntax error, unexpected end of file");` |
|       - |  5816 | `				}` |
|       8 |  5817 | `				return SXERR_SYNTAX;` |
|       - |  5818 | `			}` |
|   78125 |  5819 | `			sArg.iFlags \|= iTFlags;` |
|   39060 |  5820 | `		}` |
|  136509 |  5821 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5822 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5823 | `			return rc;` |
|       - |  5824 | `		}` |
|  136509 |  5825 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5826 | `			/* Pass by reference,record that */` |
|    3571 |  5827 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3571 |  5828 | `			pIn++;` |
|    1783 |  5829 | `		}` |
|  136509 |  5830 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5831 | `			/* Variadic parameter: ...$args */` |
|      49 |  5832 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      49 |  5833 | `			pIn++;` |
|      22 |  5834 | `		}` |
|  136509 |  5835 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5836 | `			/* Invalid argument */` |
|     ! 0 |  5837 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5838 | `			return rc;` |
|       - |  5839 | `		}` |
|  136509 |  5840 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5841 | `		/* Copy argument name */` |
|  136509 |  5842 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  136509 |  5843 | `		if( zDup == 0 ){` |
|     ! 0 |  5844 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5845 | `			return SXERR_ABORT;` |
|       - |  5846 | `		}` |
|  136509 |  5847 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  136509 |  5848 | `		pIn++;` |
|  136509 |  5849 | `		if( pIn < pEnd ){` |
|   78615 |  5850 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5851 | `				SyToken *pDefend;` |
|   67265 |  5852 | `				sxi32 iNest = 0;` |
|   67265 |  5853 | `				pIn++; /* Jump the equal sign */` |
|   67265 |  5854 | `				pDefend = pIn;` |
|       - |  5855 | `				/* Process the default value associated with this argument */` |
|  141599 |  5856 | `				while( pDefend < pEnd ){` |
|  109727 |  5857 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   35393 |  5858 | `						break;` |
|       - |  5859 | `					}` |
|   74339 |  5860 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5861 | `						/* Increment nesting level */` |
|    3543 |  5862 | `						iNest++;` |
|   72570 |  5863 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5864 | `						/* Decrement nesting level */` |
|    3543 |  5865 | `						iNest--;` |
|    1769 |  5866 | `					}` |
|   74339 |  5867 | `					pDefend++;` |
|       5 |  5868 | `				}` |
|   67265 |  5869 | `				if( pIn >= pDefend ){` |
|       3 |  5870 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5871 | `					return rc;` |
|       - |  5872 | `				}` |
|       - |  5873 | `				/* Process default value */` |
|   67263 |  5874 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   67263 |  5875 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5876 | `					return rc;` |
|       - |  5877 | `				}` |
|       - |  5878 | `				/* Point beyond the default value */` |
|   67263 |  5879 | `				pIn = pDefend;` |
|   33629 |  5880 | `			}` |
|   78613 |  5881 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5882 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5883 | `				return rc;` |
|       - |  5884 | `			}` |
|   78613 |  5885 | `			pIn++; /* Jump the trailing comma */` |
|   39304 |  5886 | `		}` |
|       - |  5887 | `		/* Append argument signature */` |
|  136507 |  5888 | `		if( sArg.nType > 0 ){` |
|   78071 |  5889 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5890 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   14197 |  5891 | `				int marker = 'o';` |
|   14197 |  5892 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   14197 |  5893 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7101 |  5894 | `			}else{` |
|       - |  5895 | `				int c;` |
|   63879 |  5896 | `				c = 'n'; /* cc warning */` |
|       - |  5897 | `				/* Type leading character */` |
|   63879 |  5898 | `				switch(sArg.nType){` |
|       3 |  5899 | `				case MEMOBJ_HASHMAP:` |
|       - |  5900 | `					/* Hashmap aka 'array' */` |
|       7 |  5901 | `					c = 'h';` |
|       7 |  5902 | `					break;` |
|    8901 |  5903 | `				case MEMOBJ_INT:` |
|       - |  5904 | `					/* Integer */` |
|   17807 |  5905 | `					c = 'i';` |
|   17807 |  5906 | `					break;` |
|       2 |  5907 | `				case MEMOBJ_BOOL:` |
|       - |  5908 | `					/* Bool */` |
|       5 |  5909 | `					c = 'b';` |
|       5 |  5910 | `					break;` |
|       2 |  5911 | `				case MEMOBJ_REAL:` |
|       - |  5912 | `					/* Float */` |
|       5 |  5913 | `					c = 'f';` |
|       5 |  5914 | `					break;` |
|   23021 |  5915 | `				case MEMOBJ_STRING:` |
|       - |  5916 | `					/* String */` |
|   46047 |  5917 | `					c = 's';` |
|   46047 |  5918 | `					break;` |
|       7 |  5919 | `				case MEMOBJ_OBJ:` |
|       - |  5920 | `					/* Object */` |
|      16 |  5921 | `					c = 'o';` |
|      14 |  5922 | `					break;` |
|       1 |  5923 | `				default:` |
|       2 |  5924 | `					break;` |
|       - |  5925 | `				}` |
|   63879 |  5926 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5927 | `			}` |
|   39038 |  5928 | `		}else{` |
|       - |  5929 | `			/* No type is associated with this parameter which mean` |
|       - |  5930 | `			 * that this function is not condidate for overloading.` |
|       - |  5931 | `			 */` |
|   58441 |  5932 | `			SyBlobRelease(&sSig);` |
|       - |  5933 | `		}` |
|       - |  5934 | `		/* Save in the argument set */` |
|  136507 |  5935 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5936 | `	}` |
|   89775 |  5937 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5938 | `		/* Save function signature */` |
|   49733 |  5939 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   24864 |  5940 | `	}` |
|   89775 |  5941 | `	return SXRET_OK;` |
|   44897 |  5942 |  |
|       - |  5943 | `/*` |
|       - |  5944 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5945 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5946 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5947 | ` */` |
|  207548 |  5948 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5949 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5950 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5951 | `	)` |
|       5 |  5952 |  |
|       - |  5953 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5954 | `	GenBlock *pBlock;` |
|       - |  5955 | `	sxu32 nGotoOfft;` |
|       - |  5956 | `	sxi32 rc;` |
|       - |  5957 | `	/* Attach the new function */` |
|  207553 |  5958 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  207553 |  5959 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5960 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5961 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5962 | `		return SXERR_ABORT;` |
|       - |  5963 | `	}` |
|  207553 |  5964 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5965 | `	/* Swap bytecode containers */` |
|  207553 |  5966 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  207553 |  5967 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5968 | `	/* Emit constructor property promotion prologue:` |
|       - |  5969 | `	 *   $this->NAME = $NAME;` |
|       - |  5970 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5971 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5972 | `	{` |
|  207553 |  5973 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5974 | `		sxu32 i;` |
|  315609 |  5975 | `		for( i = 0; i < nArg; i++ ){` |
|  108061 |  5976 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5977 | `			char *zSrc;` |
|       - |  5978 | `			sxu32 nSrc,nName;` |
|       - |  5979 | `			SySet sToken;` |
|       - |  5980 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5981 | `			sxi32 rcPromote;` |
|  108061 |  5982 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  108007 |  5983 | `				continue;` |
|       - |  5984 | `			}` |
|       - |  5985 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5986 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5987 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5988 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5989 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      59 |  5990 | `			nName = SyStringLength(&pArg->sName);` |
|      59 |  5991 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      59 |  5992 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      59 |  5993 | `			if( zSrc == 0 ){` |
|     ! 0 |  5994 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5995 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5996 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5997 | `				return SXERR_ABORT;` |
|       - |  5998 | `			}` |
|       - |  5999 | `			{` |
|      59 |  6000 | `				char *z = zSrc;` |
|      59 |  6001 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      59 |  6002 | `				z += sizeof("$this->")-1;` |
|      59 |  6003 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6004 | `				z += nName;` |
|      59 |  6005 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      59 |  6006 | `				z += sizeof(" = $")-1;` |
|      59 |  6007 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6008 | `				z += nName;` |
|      59 |  6009 | `				*z = 0;` |
|       - |  6010 | `			}` |
|      59 |  6011 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      59 |  6012 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      59 |  6013 | `			pTmpIn = pGen->pIn;` |
|      59 |  6014 | `			pTmpEnd = pGen->pEnd;` |
|      59 |  6015 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      59 |  6016 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      59 |  6017 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      59 |  6018 | `			pGen->pIn = pTmpIn;` |
|      59 |  6019 | `			pGen->pEnd = pTmpEnd;` |
|      59 |  6020 | `			SySetRelease(&sToken);` |
|      59 |  6021 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6022 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6023 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6024 | `				return SXERR_ABORT;` |
|       - |  6025 | `			}` |
|       - |  6026 | `			/* Discard the assignment result — this is a statement expression. */` |
|      59 |  6027 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      32 |  6028 | `		}` |
|       - |  6029 | `	}` |
|       - |  6030 | `	/* Compile the body */` |
|  207553 |  6031 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  6032 | `	/* Fix exception jumps now the destination is resolved */` |
|  207553 |  6033 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6034 | `	/* Emit the final return if not yet done */` |
|  207553 |  6035 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6036 | `	/* Fix gotos jumps now the destination is resolved */` |
|  207553 |  6037 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6038 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6039 | `	}` |
|  207553 |  6040 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6041 | `	/* Restore the default container */` |
|  207553 |  6042 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6043 | `	/* Leave function block */` |
|  207553 |  6044 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  207553 |  6045 | `	if( rc == SXERR_ABORT ){` |
|       - |  6046 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6047 | `		return SXERR_ABORT;` |
|       - |  6048 | `	}` |
|       - |  6049 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6050 | `	{` |
|  207553 |  6051 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6052 | `		sxu32 i;` |
| 4220837 |  6053 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4013389 |  6054 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     105 |  6055 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     105 |  6056 | `				break;` |
|       - |  6057 | `			}` |
| 2006647 |  6058 | `		}` |
|       - |  6059 | `	}` |
|       - |  6060 | `	/* All done, function body compiled */` |
|  207553 |  6061 | `	return SXRET_OK;` |
|  103779 |  6062 |  |
|       - |  6063 | `/*` |
|       - |  6064 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6065 | ` * According to the PHP language reference manual.` |
|       - |  6066 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6067 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6068 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6069 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6070 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6071 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6072 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6073 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6074 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6075 | ` *` |
|       - |  6076 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6077 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6078 | ` * on these extension.` |
|       - |  6079 | ` */` |
|       - |  6080 | `/*` |
|       - |  6081 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6082 | ` */` |
|     484 |  6083 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6084 |  |
|       - |  6085 | `	sxu32 i;` |
|    1337 |  6086 | `	for( i = 0; i < n; i++ ){` |
|    1149 |  6087 | `		int a = zA[i], b = zB[i];` |
|    1149 |  6088 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1149 |  6089 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1149 |  6090 | `		if( a != b ) return a - b;` |
|     429 |  6091 | `	}` |
|     193 |  6092 | `	return 0;` |
|     247 |  6093 |  |
|       - |  6094 | `/*` |
|       - |  6095 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6096 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6097 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6098 | ` */` |
|       - |  6099 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6100 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6101 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6102 |  |
|       - |  6103 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6104 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6105 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6106 |  |
|       - |  6107 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6108 | `struct PhlTypeAtom {` |
|       - |  6109 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6110 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6111 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6112 | `	sxu32 nCanon;` |
|       - |  6113 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6114 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6115 | `};` |
|       - |  6116 |  |
|       - |  6117 | `/*` |
|       - |  6118 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6119 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6120 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6121 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6122 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6123 | ` * already be consumed by the caller.` |
|       - |  6124 | ` */` |
|   78964 |  6125 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6126 |  |
|   78969 |  6127 | `	SyToken *pIn = pGen->pIn;` |
|   78969 |  6128 | `	SyZero(pOut, sizeof(*pOut));` |
|   78969 |  6129 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   78969 |  6130 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6131 | `		return SXERR_SYNTAX;` |
|       - |  6132 | `	}` |
|       - |  6133 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   78969 |  6134 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6135 | `		pIn++;` |
|       8 |  6136 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6137 | `			return SXERR_SYNTAX;` |
|       - |  6138 | `		}` |
|       3 |  6139 | `	}` |
|   78969 |  6140 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6141 | `		return SXERR_SYNTAX;` |
|       - |  6142 | `	}` |
|   78969 |  6143 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   64411 |  6144 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   64411 |  6145 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      32 |  6146 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   64397 |  6147 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      71 |  6148 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   64350 |  6149 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   18055 |  6150 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   55292 |  6151 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   46205 |  6152 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   23167 |  6153 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      33 |  6154 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      53 |  6155 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6156 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      25 |  6157 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       7 |  6158 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      11 |  6159 | `			pOut->nType = SXU32_HIGH;` |
|      11 |  6160 | `			pOut->sClass = pIn->sData;` |
|       7 |  6161 | `		}else{` |
|       3 |  6162 | `			return SXERR_SYNTAX;` |
|       - |  6163 | `		}` |
|   64409 |  6164 | `		pIn++;` |
|   32207 |  6165 | `	}else{` |
|       - |  6166 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6167 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   14563 |  6168 | `		SyString *pT = &pIn->sData;` |
|   14563 |  6169 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6170 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6171 | `			pIn++;` |
|   14549 |  6172 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     157 |  6173 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     157 |  6174 | `			pIn++;` |
|   14459 |  6175 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  6176 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  6177 | `			pIn++;` |
|       2 |  6178 | `		}else{` |
|       - |  6179 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   14381 |  6180 | `			SyToken *pFirst = pIn;` |
|   14381 |  6181 | `			SyToken *pLast = pIn;` |
|   14381 |  6182 | `			pOut->nType = SXU32_HIGH;` |
|   14381 |  6183 | `			pOut->sClass = pIn->sData;` |
|   14381 |  6184 | `			pIn++;` |
|   21567 |  6185 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   14384 |  6186 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6187 | `				pLast = &pIn[1];` |
|       3 |  6188 | `				pIn += 2;` |
|       1 |  6189 | `			}` |
|   14381 |  6190 | `			if( pLast != pFirst ){` |
|       3 |  6191 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6192 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6193 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6194 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6195 | `			}` |
|       - |  6196 | `		}` |
|       - |  6197 | `	}` |
|   78967 |  6198 | `	pGen->pIn = pIn;` |
|   78967 |  6199 | `	return SXRET_OK;` |
|   39487 |  6200 |  |
|       - |  6201 |  |
|       - |  6202 | `/*` |
|       - |  6203 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6204 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6205 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6206 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6207 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6208 | ` */` |
|   78810 |  6209 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6210 |  |
|       - |  6211 | `	int i;` |
|   78815 |  6212 | `	int nNonNull = 0;` |
|   78815 |  6213 | `	int bAnyIntersection = 0;` |
|       - |  6214 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   78815 |  6215 | `	sxu32 nMaxGroup = 0;` |
| 2600735 |  6216 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  157759 |  6217 | `	for( i = 0; i < nAtoms; i++ ){` |
|   78949 |  6218 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   78921 |  6219 | `			nNonNull++;` |
|   78921 |  6220 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   78921 |  6221 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   78921 |  6222 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   39458 |  6223 | `			}` |
|   39458 |  6224 | `		}` |
|   39477 |  6225 | `	}` |
|  157725 |  6226 | `	for( i = 0; i < nAtoms; i++ ){` |
|   78931 |  6227 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      19 |  6228 | `			bAnyIntersection = 1;` |
|      19 |  6229 | `			break;` |
|       - |  6230 | `		}` |
|   39460 |  6231 | `	}` |
|   78815 |  6232 | `	if( bAnyIntersection ){` |
|       - |  6233 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6234 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6235 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      19 |  6236 | `		sxu32 g, nGroups = 0;` |
|      19 |  6237 | `		int bFirstGroup = 1;` |
|      39 |  6238 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      39 |  6239 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      23 |  6240 | `			int bFirstMember = 1;` |
|       - |  6241 | `			int bWrap;` |
|      23 |  6242 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6243 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6244 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6245 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6246 | `			 * parens, matching PHP's canonical text. */` |
|      31 |  6247 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      23 |  6248 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      23 |  6249 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      71 |  6250 | `			for( i = 0; i < nAtoms; i++ ){` |
|      51 |  6251 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      39 |  6252 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      39 |  6253 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      37 |  6254 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      20 |  6255 | `				}else{` |
|       3 |  6256 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6257 | `				}` |
|      39 |  6258 | `				bFirstMember = 0;` |
|      21 |  6259 | `			}` |
|      23 |  6260 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      23 |  6261 | `			bFirstGroup = 0;` |
|      13 |  6262 | `		}` |
|      19 |  6263 | `		if( bNullable ){` |
|     ! 0 |  6264 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6265 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6266 | `		}` |
|      57 |  6267 | `		return;` |
|       - |  6268 | `	}` |
|   78799 |  6269 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6270 | `		/* Shorthand: ?T */` |
|      81 |  6271 | `		for( i = 0; i < nAtoms; i++ ){` |
|      81 |  6272 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      81 |  6273 | `			SyBlobAppend(pBlob, "?", 1);` |
|      81 |  6274 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      22 |  6275 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      13 |  6276 | `			}else{` |
|      63 |  6277 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6278 | `			}` |
|      81 |  6279 | `			return;` |
|     ! 0 |  6280 | `		}` |
|     ! 0 |  6281 | `	}` |
|       - |  6282 | `	{` |
|   78723 |  6283 | `		int bFirst = 1;` |
|       - |  6284 | `		/* 1) Classes in declaration order */` |
|  157543 |  6285 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78825 |  6286 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   14337 |  6287 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   14337 |  6288 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   14337 |  6289 | `				bFirst = 0;` |
|    7166 |  6290 | `			}` |
|   39415 |  6291 | `		}` |
|       - |  6292 | `		/* 2) Built-ins in canonical order */` |
|       - |  6293 | `		{` |
|       - |  6294 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6295 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6296 | `			int k;` |
|  551031 |  6297 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  880803 |  6298 | `				for( i = 0; i < nAtoms; i++ ){` |
|  472817 |  6299 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   64327 |  6300 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   64327 |  6301 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   64327 |  6302 | `						bFirst = 0;` |
|   64327 |  6303 | `						break;` |
|       - |  6304 | `					}` |
|  204250 |  6305 | `				}` |
|  236159 |  6306 | `			}` |
|       - |  6307 | `		}` |
|       - |  6308 | `		/* 3) null suffix */` |
|   78723 |  6309 | `		if( bNullable ){` |
|      20 |  6310 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      20 |  6311 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6312 | `		}` |
|       - |  6313 | `	}` |
|   39410 |  6314 |  |
|       - |  6315 |  |
|       - |  6316 | `/*` |
|       - |  6317 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6318 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6319 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6320 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6321 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6322 | ` * whether it was parenthesized.` |
|       - |  6323 | ` *` |
|       - |  6324 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6325 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6326 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6327 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6328 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6329 | ` */` |
|   78946 |  6330 | `static sxi32 GenStateParsePart(` |
|       - |  6331 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6332 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6333 |  |
|       - |  6334 | `	sxi32 rc;` |
|   78951 |  6335 | `	int nMembers = 0;` |
|   78951 |  6336 | `	int bParen = 0;` |
|   78951 |  6337 | `	*pnMembers = 0;` |
|   78951 |  6338 | `	*pbParen = 0;` |
|   78951 |  6339 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6340 | `		bParen = 1;` |
|       6 |  6341 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6342 | `	}` |
|   39473 |  6343 | `	for(;;){` |
|   78969 |  6344 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6345 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6346 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6347 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6348 | `		}` |
|   78969 |  6349 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   78969 |  6350 | `		if( rc != SXRET_OK ){` |
|       3 |  6351 | `			return rc;` |
|       - |  6352 | `		}` |
|   78967 |  6353 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   78967 |  6354 | `		(*pnAtoms)++;` |
|   78967 |  6355 | `		nMembers++;` |
|       - |  6356 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   78967 |  6357 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      24 |  6358 | `			SyToken *pNext = &pGen->pIn[1];` |
|      20 |  6359 | `			if( pNext < pGen->pEnd` |
|      24 |  6360 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      22 |  6361 | `				pGen->pIn++; /* skip '&' */` |
|      22 |  6362 | `				continue;` |
|       - |  6363 | `			}` |
|       1 |  6364 | `		}` |
|   78949 |  6365 | `		break;` |
|     ! 0 |  6366 | `	}` |
|   78949 |  6367 | `	if( bParen ){` |
|       6 |  6368 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6369 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6370 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6371 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6372 | `		}` |
|       6 |  6373 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6374 | `		if( nMembers < 2 ){` |
|     ! 0 |  6375 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6376 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6377 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6378 | `		}` |
|       2 |  6379 | `	}` |
|   78949 |  6380 | `	*pnMembers = nMembers;` |
|   78949 |  6381 | `	*pbParen = bParen;` |
|   78949 |  6382 | `	return SXRET_OK;` |
|   39478 |  6383 |  |
|       - |  6384 |  |
|       - |  6385 | `/*` |
|       - |  6386 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6387 | ` *` |
|       - |  6388 | ` * Outputs:` |
|       - |  6389 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6390 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6391 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6392 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6393 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6394 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6395 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6396 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6397 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6398 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6399 | ` *` |
|       - |  6400 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6401 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6402 | ` */` |
|   78822 |  6403 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6404 | `	ph7_gen_state *pGen,` |
|       - |  6405 | `	sxu32 *pnType,` |
|       - |  6406 | `	SyString *pClass,` |
|       - |  6407 | `	SySet *pAlts,` |
|       - |  6408 | `	sxi32 *piTypeFlags,` |
|       - |  6409 | `	SyString *pTypeText,` |
|       - |  6410 | `	int iNullableFlag,` |
|       - |  6411 | `	int iUnionFlag,` |
|       - |  6412 | `	int bAllowVoid,` |
|       - |  6413 | `	sxu32 nLine` |
|       5 |  6414 | `){` |
|       - |  6415 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   78827 |  6416 | `	int nAtoms = 0;` |
|   78827 |  6417 | `	int bShortNullable = 0;` |
|   78827 |  6418 | `	int bExplicitNull = 0;` |
|       - |  6419 | `	sxi32 rc;` |
|   78827 |  6420 | `	*pnType = 0;` |
|   78827 |  6421 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   78827 |  6422 | `	*piTypeFlags = 0;` |
|   78827 |  6423 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6424 |  |
|   78827 |  6425 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6426 | `		return SXRET_OK;` |
|       - |  6427 | `	}` |
|       - |  6428 | ``	/* Optional `?` shorthand prefix */`` |
|   78822 |  6429 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      71 |  6430 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      71 |  6431 | `		bShortNullable = 1;` |
|      71 |  6432 | `		pGen->pIn++;` |
|      71 |  6433 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6434 | `			return SXERR_SYNTAX;` |
|       - |  6435 | `		}` |
|      33 |  6436 | `	}` |
|       - |  6437 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6438 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6439 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6440 | `	{` |
|       - |  6441 | `		int nMembers, bParen;` |
|   78827 |  6442 | `		sxu32 iGroup = 0;` |
|   78827 |  6443 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   78827 |  6444 | `		if( rc != SXRET_OK ){` |
|       4 |  6445 | `			return rc;` |
|       - |  6446 | `		}` |
|       - |  6447 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6448 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6449 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6450 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6451 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  118418 |  6452 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   79013 |  6453 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     131 |  6454 | `			if( bShortNullable ){` |
|       - |  6455 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6456 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6457 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6458 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6459 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6460 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6461 | `			}` |
|     129 |  6462 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6463 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6464 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6465 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6466 | `			}` |
|     129 |  6467 | ``			pGen->pIn++; /* skip `\|` */`` |
|     129 |  6468 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     129 |  6469 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6470 | `				return rc;` |
|       - |  6471 | `			}` |
|       5 |  6472 | `		}` |
|   78823 |  6473 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6474 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6475 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6476 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6477 | `		}` |
|       - |  6478 | `	}` |
|       - |  6479 | `	/* Validation pass.` |
|       - |  6480 | `	 *` |
|       - |  6481 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6482 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6483 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6484 | `	 */` |
|       - |  6485 | `	{` |
|       - |  6486 | `		int i, j;` |
|   78823 |  6487 | `		int bHasNonNull = 0;` |
|   78823 |  6488 | `		int bAnyIntersection = 0;` |
|       - |  6489 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6490 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6491 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2600999 |  6492 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  157783 |  6493 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78965 |  6494 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   39485 |  6495 | `		}` |
|  157745 |  6496 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78945 |  6497 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   39466 |  6498 | `		}` |
|       - |  6499 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6500 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   78823 |  6501 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6502 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6503 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6504 | `			return SXERR_SYNTAX;` |
|       - |  6505 | `		}` |
|  157773 |  6506 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6507 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6508 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6509 | ``			 * `true`/`false` in an intersection). */`` |
|   78963 |  6510 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      38 |  6511 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      38 |  6512 | `				if( bClassLike ){` |
|      35 |  6513 | `					SyString *pC = &aAtoms[i].sClass;` |
|      32 |  6514 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      32 |  6515 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      32 |  6516 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      35 |  6517 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6518 | `						bClassLike = 0;` |
|     ! 0 |  6519 | `					}` |
|      16 |  6520 | `				}` |
|      38 |  6521 | `				if( !bClassLike ){` |
|       - |  6522 | `					const char *zName; sxu32 nName;` |
|       3 |  6523 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6524 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6525 | `					}else{` |
|       3 |  6526 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6527 | `					}` |
|       4 |  6528 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6529 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6530 | `						(int)nName, zName);` |
|       3 |  6531 | `					return SXERR_SYNTAX;` |
|       - |  6532 | `				}` |
|      16 |  6533 | `			}` |
|   78961 |  6534 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     157 |  6535 | `				if( nAtoms > 1 ){` |
|       3 |  6536 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6537 | `						"Void can only be used as a standalone type");` |
|       3 |  6538 | `					return SXERR_SYNTAX;` |
|       - |  6539 | `				}` |
|     155 |  6540 | `				if( !bAllowVoid ){` |
|     ! 0 |  6541 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6542 | `						"void cannot be used here");` |
|     ! 0 |  6543 | `					return SXERR_SYNTAX;` |
|       - |  6544 | `				}` |
|     155 |  6545 | `				if( bShortNullable ){` |
|     ! 0 |  6546 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6547 | `						"Void type cannot be nullable");` |
|     ! 0 |  6548 | `					return SXERR_SYNTAX;` |
|       - |  6549 | `				}` |
|      75 |  6550 | `			}` |
|   78959 |  6551 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6552 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6553 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6554 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6555 | `				 * does not return), and folding them would mislead any` |
|       - |  6556 | `				 * future return-enforcement work. */` |
|       3 |  6557 | `				if( nAtoms > 1 ){` |
|       3 |  6558 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6559 | `						"never can only be used as a standalone type");` |
|       3 |  6560 | `					return SXERR_SYNTAX;` |
|       - |  6561 | `				}` |
|     ! 0 |  6562 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6563 | `					"never type is not yet implemented");` |
|     ! 0 |  6564 | `				return SXERR_SYNTAX;` |
|       - |  6565 | `			}` |
|   78957 |  6566 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  6567 | `				bExplicitNull = 1;` |
|      18 |  6568 | `			}else{` |
|   78929 |  6569 | `				bHasNonNull = 1;` |
|       - |  6570 | `			}` |
|       - |  6571 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  6572 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  6573 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  6574 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  6575 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   79137 |  6576 | `			for( j = 0; j < i; j++ ){` |
|     187 |  6577 | `				int bDup = 0;` |
|     187 |  6578 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     359 |  6579 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     182 |  6580 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     187 |  6581 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     179 |  6582 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      40 |  6583 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      34 |  6584 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      37 |  6585 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      16 |  6586 | `								aAtoms[j].sClass.zString,` |
|      32 |  6587 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6588 | `							bDup = 1;` |
|     ! 0 |  6589 | `						}` |
|      21 |  6590 | `					}else{` |
|       3 |  6591 | `						bDup = 1;` |
|       - |  6592 | `					}` |
|      18 |  6593 | `				}` |
|     179 |  6594 | `				if( bDup ){` |
|       - |  6595 | `					const char *zName;` |
|       - |  6596 | `					sxu32 nName;` |
|       3 |  6597 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6598 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6599 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6600 | `					}else{` |
|       3 |  6601 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6602 | `						nName = aAtoms[i].nCanon;` |
|       - |  6603 | `					}` |
|       4 |  6604 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6605 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6606 | `					return SXERR_SYNTAX;` |
|       - |  6607 | `				}` |
|      91 |  6608 | `			}` |
|   39480 |  6609 | `		}` |
|   78815 |  6610 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6611 | `			if( bShortNullable ){` |
|       - |  6612 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6613 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6614 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6615 | `				return SXERR_SYNTAX;` |
|       - |  6616 | `			}` |
|       - |  6617 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6618 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6619 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6620 | `			 * atom, so set it here. */` |
|       7 |  6621 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6622 | `		}` |
|       - |  6623 | `	}` |
|       - |  6624 | `	/* Compute nullability flag */` |
|   78815 |  6625 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      97 |  6626 | `		*piTypeFlags \|= iNullableFlag;` |
|      46 |  6627 | `	}` |
|       - |  6628 | `	/* Build canonical type text */` |
|   78815 |  6629 | `	if( pTypeText ){` |
|       - |  6630 | `		SyBlob sBlob;` |
|   78815 |  6631 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  118188 |  6632 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   39405 |  6633 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   78815 |  6634 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  117995 |  6635 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   78660 |  6636 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   78665 |  6637 | `			if( zDup ){` |
|   78665 |  6638 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   39330 |  6639 | `			}` |
|   39330 |  6640 | `		}` |
|   78815 |  6641 | `		SyBlobRelease(&sBlob);` |
|   39405 |  6642 | `	}` |
|       - |  6643 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6644 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6645 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6646 | `	{` |
|   78815 |  6647 | `		int nNonNull = 0;` |
|   78815 |  6648 | `		int iNonNullIdx = -1;` |
|       - |  6649 | `		int i;` |
|  157759 |  6650 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78949 |  6651 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   78921 |  6652 | `				nNonNull++;` |
|   78921 |  6653 | `				iNonNullIdx = i;` |
|   39458 |  6654 | `			}` |
|   39477 |  6655 | `		}` |
|   78815 |  6656 | `		if( nNonNull <= 1 ){` |
|       - |  6657 | `			/* Fast path: store as single type. */` |
|   78723 |  6658 | `			if( iNonNullIdx >= 0 ){` |
|   78717 |  6659 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   78717 |  6660 | `				if( pA->nType == SXU32_HIGH ){` |
|   21470 |  6661 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7155 |  6662 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   14315 |  6663 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   14315 |  6664 | `					*pnType = SXU32_HIGH;` |
|   14315 |  6665 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   71562 |  6666 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     155 |  6667 | `					*pnType = MEMOBJ_VOID;` |
|      80 |  6668 | `				}else{` |
|       - |  6669 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6670 | `					 * pass above rejects it as not-yet-implemented. */` |
|   64257 |  6671 | `					*pnType = pA->nType;` |
|       - |  6672 | `				}` |
|   39356 |  6673 | `			}` |
|   39364 |  6674 | `		}else{` |
|       - |  6675 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      97 |  6676 | `			*piTypeFlags \|= iUnionFlag;` |
|     311 |  6677 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6678 | `				ph7_type_alt sAlt;` |
|     219 |  6679 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     209 |  6680 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     209 |  6681 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     209 |  6682 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     116 |  6683 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      37 |  6684 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      79 |  6685 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      79 |  6686 | `					sAlt.nType = SXU32_HIGH;` |
|      79 |  6687 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      42 |  6688 | `				}else{` |
|     135 |  6689 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  6690 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6691 | `				}` |
|     209 |  6692 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     107 |  6693 | `			}` |
|       - |  6694 | `		}` |
|       - |  6695 | `	}` |
|   78815 |  6696 | `	return SXRET_OK;` |
|   39416 |  6697 |  |
|       - |  6698 |  |
|       - |  6699 | `/*` |
|       - |  6700 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6701 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6702 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6703 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6704 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6705 | `` *          and union types `: T\|U`.`` |
|       - |  6706 | ` */` |
|  299744 |  6707 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6708 |  |
|  299749 |  6709 | `	sxi32 iFlags = 0;` |
|       - |  6710 | `	sxi32 rc;` |
|       - |  6711 | `	sxu32 nLine;` |
|  299749 |  6712 | `	pFunc->nReturnType = 0;` |
|  299749 |  6713 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  299749 |  6714 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  299749 |  6715 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  299267 |  6716 | `		return SXRET_OK;` |
|       - |  6717 | `	}` |
|     487 |  6718 | `	pGen->pIn++; /* Skip ':' */` |
|     487 |  6719 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6720 | `		return SXRET_OK;` |
|       - |  6721 | `	}` |
|     487 |  6722 | `	nLine = pGen->pIn->nLine;` |
|     487 |  6723 | `	rc = GenStateParseUnionTypeDecl(` |
|     241 |  6724 | `		pGen,` |
|     241 |  6725 | `		&pFunc->nReturnType,` |
|     241 |  6726 | `		&pFunc->sReturnClass,` |
|     241 |  6727 | `		&pFunc->aReturnUnion,` |
|       - |  6728 | `		&iFlags,` |
|     241 |  6729 | `		&pFunc->sReturnTypeName,` |
|       - |  6730 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  6731 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  6732 | `		/* iUnionFlag */ 0,` |
|       - |  6733 | `		/* bAllowVoid */ 1,` |
|     241 |  6734 | `		nLine);` |
|     487 |  6735 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6736 | `		return SXERR_ABORT;` |
|       - |  6737 | `	}` |
|     487 |  6738 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6739 | `		/* Error already reported */` |
|     ! 0 |  6740 | `		return SXERR_SYNTAX;` |
|       - |  6741 | `	}` |
|     487 |  6742 | `	if( rc == SXERR_SYNTAX ){` |
|       6 |  6743 | `		if( pGen->pIn < pGen->pEnd ){` |
|       8 |  6744 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6745 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6746 | `				&pGen->pIn->sData);` |
|       4 |  6747 | `		}else{` |
|     ! 0 |  6748 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6749 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6750 | `		}` |
|       6 |  6751 | `		return SXERR_SYNTAX;` |
|       - |  6752 | `	}` |
|     483 |  6753 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     483 |  6754 | `	return SXRET_OK;` |
|  149877 |  6755 |  |
|       - |  6756 |  |
|   47312 |  6757 | `static sxi32 GenStateCompileFunc(` |
|       - |  6758 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6759 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6760 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6761 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6762 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6763 | `	)` |
|       5 |  6764 |  |
|       - |  6765 | `	ph7_vm_func *pFunc;` |
|       - |  6766 | `	SyToken *pEnd;` |
|       - |  6767 | `	sxu32 nLine;` |
|       - |  6768 | `	char *zName;` |
|       - |  6769 | `	sxi32 rc;` |
|       - |  6770 | `	/* Extract line number */` |
|   47317 |  6771 | `	nLine = pGen->pIn->nLine;` |
|       - |  6772 | `	/* Jump the left parenthesis '(' */` |
|   47317 |  6773 | `	pGen->pIn++;` |
|       - |  6774 | `	/* Delimit the function signature */` |
|   47317 |  6775 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   47317 |  6776 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6777 | `		/* Syntax error */` |
|       8 |  6778 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       8 |  6779 | `		if( rc == SXERR_ABORT ){` |
|       - |  6780 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6781 | `			return SXERR_ABORT;` |
|       - |  6782 | `		}` |
|       8 |  6783 | `		pGen->pIn = pGen->pEnd;` |
|       8 |  6784 | `		return SXRET_OK;` |
|       - |  6785 | `	}` |
|       - |  6786 | `	/* Create the function state */` |
|   47311 |  6787 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   47311 |  6788 | `	if( pFunc == 0 ){` |
|     ! 0 |  6789 | `		goto OutOfMem;` |
|       - |  6790 | `	}` |
|       - |  6791 | `	/* Build the function name, prepending namespace if active */` |
|   47318 |  6792 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6793 | `		SyBlob sFQN;` |
|       - |  6794 | `		sxu32 nLen;` |
|      16 |  6795 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6796 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6797 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6798 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6799 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6800 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6801 | `		SyBlobRelease(&sFQN);` |
|      16 |  6802 | `		if( zName == 0 ){` |
|     ! 0 |  6803 | `			goto OutOfMem;` |
|       - |  6804 | `		}` |
|      16 |  6805 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6806 | `	}else{` |
|   47297 |  6807 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   47297 |  6808 | `		if( zName == 0 ){` |
|     ! 0 |  6809 | `			goto OutOfMem;` |
|       - |  6810 | `		}` |
|   47297 |  6811 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6812 | `	}` |
|   47311 |  6813 | `	if( pGen->pIn < pEnd ){` |
|       - |  6814 | `		/* Collect function arguments */` |
|   32645 |  6815 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   32645 |  6816 | `		if( rc == SXERR_ABORT ){` |
|       - |  6817 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6818 | `			return SXERR_ABORT;` |
|       - |  6819 | `		}` |
|   16320 |  6820 | `	}` |
|       - |  6821 | `	/* Point past ')' and parse optional return type ': type' */` |
|   47311 |  6822 | `	pGen->pIn = &pEnd[1];` |
|       - |  6823 | `	{` |
|   47311 |  6824 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   47311 |  6825 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6826 | `			return SXERR_ABORT;` |
|   47311 |  6827 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       6 |  6828 | `			return SXERR_SYNTAX;` |
|       - |  6829 | `		}` |
|       - |  6830 | `	}` |
|   47307 |  6831 | `	if( bHandleClosure ){` |
|       - |  6832 | `		ph7_vm_func_closure_env sEnv;` |
|     275 |  6833 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     270 |  6834 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     149 |  6835 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      23 |  6836 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6837 | `				/* Closure,record environment variable */` |
|      23 |  6838 | `				pGen->pIn++;` |
|      23 |  6839 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6840 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6841 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6842 | `						return SXERR_ABORT;` |
|       - |  6843 | `					}` |
|     ! 0 |  6844 | `				}` |
|      23 |  6845 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6846 | `				/* Compile until we hit the first closing parenthesis */` |
|      45 |  6847 | `				while( pGen->pIn < pGen->pEnd ){` |
|      45 |  6848 | `					int iFlagsLocal = 0;` |
|      45 |  6849 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      23 |  6850 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      23 |  6851 | `						break;` |
|       - |  6852 | `					}` |
|      27 |  6853 | `					nLineLocal = pGen->pIn->nLine;` |
|      27 |  6854 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6855 | `						/* Pass by reference,record that */` |
|     ! 0 |  6856 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6857 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6858 | `							);` |
|     ! 0 |  6859 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6860 | `						pGen->pIn++;` |
|     ! 0 |  6861 | `					}` |
|      22 |  6862 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      27 |  6863 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6864 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6865 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6866 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6867 | `								return SXERR_ABORT;` |
|       - |  6868 | `							}` |
|       - |  6869 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6870 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6871 | `								pGen->pIn++;` |
|     ! 0 |  6872 | `							}` |
|     ! 0 |  6873 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6874 | `								pGen->pIn++;` |
|     ! 0 |  6875 | `							}` |
|     ! 0 |  6876 | `							break;` |
|       - |  6877 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6878 | `					}else{` |
|       - |  6879 | `						SyString *pNameLocal;` |
|       - |  6880 | `						char *zDup;` |
|       - |  6881 | `						/* Duplicate variable name */` |
|      27 |  6882 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      27 |  6883 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      27 |  6884 | `						if( zDup ){` |
|       - |  6885 | `							/* Zero the structure */` |
|      27 |  6886 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      27 |  6887 | `							sEnv.iFlags = iFlagsLocal;` |
|      27 |  6888 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      27 |  6889 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      27 |  6890 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6891 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6892 | `									got_this = 1;` |
|     ! 0 |  6893 | `							}` |
|       - |  6894 | `							/* Save imported variable */` |
|      27 |  6895 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      16 |  6896 | `						}else{` |
|     ! 0 |  6897 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6898 | `							 return SXERR_ABORT;` |
|       - |  6899 | `						}` |
|       - |  6900 | `					}` |
|      27 |  6901 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      33 |  6902 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6903 | `						/* Ignore trailing commas */` |
|       7 |  6904 | `						pGen->pIn++;` |
|       1 |  6905 | `					}` |
|       5 |  6906 | `				}` |
|      23 |  6907 | `				if( !got_this ){` |
|       - |  6908 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6909 | `					 * available to the closure environment.` |
|       - |  6910 | `					 */` |
|      23 |  6911 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      23 |  6912 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      23 |  6913 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      23 |  6914 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      23 |  6915 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 |  6916 | `				}` |
|      23 |  6917 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6918 | `					/* Mark as closure */` |
|      23 |  6919 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       9 |  6920 | `				}` |
|       9 |  6921 | `		}` |
|     135 |  6922 | `	}` |
|       - |  6923 | `	/* Compile the body */` |
|   47307 |  6924 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   47307 |  6925 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6926 | `		return SXERR_ABORT;` |
|       - |  6927 | `	}` |
|   47307 |  6928 | `	if( ppFunc ){` |
|     275 |  6929 | `		*ppFunc = pFunc;` |
|     135 |  6930 | `	}` |
|   47307 |  6931 | `	rc = SXRET_OK;` |
|   47307 |  6932 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6933 | `		/* Finally register the function */` |
|   47289 |  6934 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   23642 |  6935 | `	}` |
|   47307 |  6936 | `	if( rc == SXRET_OK ){` |
|   47307 |  6937 | `		return SXRET_OK;` |
|       - |  6938 | `	}` |
|       - |  6939 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6940 | `OutOfMem:` |
|       - |  6941 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6942 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6943 | `	 */` |
|     ! 0 |  6944 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6945 | `	return SXERR_ABORT;` |
|   23661 |  6946 |  |
|       - |  6947 | `/*` |
|       - |  6948 | ` * Compile a standard PHP function.` |
|       - |  6949 | ` *  Refer to the block-comment above for more information.` |
|       - |  6950 | ` */` |
|   47050 |  6951 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6952 |  |
|       - |  6953 | `	SyString *pName;` |
|       - |  6954 | `	sxi32 iFlags;` |
|       - |  6955 | `	sxu32 nLine;` |
|       - |  6956 | `	sxi32 rc;` |
|       - |  6957 |  |
|   47055 |  6958 | `	nLine = pGen->pIn->nLine;` |
|   47055 |  6959 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   47055 |  6960 | `	iFlags = 0;` |
|   47055 |  6961 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6962 | `		/* Return by reference,remember that */` |
|       7 |  6963 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6964 | `		/* Jump the '&' token */` |
|       7 |  6965 | `		pGen->pIn++;` |
|       3 |  6966 | `	}` |
|   47055 |  6967 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6968 | `		/* Invalid function name */` |
|       7 |  6969 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       7 |  6970 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6971 | `			return SXERR_ABORT;` |
|       - |  6972 | `		}` |
|       - |  6973 | `		/* Sychronize with the next semi-colon or braces*/` |
|      21 |  6974 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      15 |  6975 | `			pGen->pIn++;` |
|       1 |  6976 | `		}` |
|       7 |  6977 | `		return SXRET_OK;` |
|       - |  6978 | `	}` |
|   47049 |  6979 | `	pName = &pGen->pIn->sData;` |
|   47049 |  6980 | `	nLine = pGen->pIn->nLine;` |
|       - |  6981 | `	/* Jump the function name */` |
|   47049 |  6982 | `	pGen->pIn++;` |
|   47049 |  6983 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6984 | `		/* Syntax error */` |
|       3 |  6985 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6986 | `		if( rc == SXERR_ABORT ){` |
|       - |  6987 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6988 | `			return SXERR_ABORT;` |
|       - |  6989 | `		}` |
|       - |  6990 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6991 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6992 | `			pGen->pIn++;` |
|     ! 0 |  6993 | `		}` |
|       3 |  6994 | `		return SXRET_OK;` |
|       - |  6995 | `	}` |
|       - |  6996 | `	/* Compile function body */` |
|   47047 |  6997 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   47047 |  6998 | `	return rc;` |
|   23530 |  6999 |  |
|       - |  7000 | `/*` |
|       - |  7001 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7002 | ` * According to the PHP language reference manual` |
|       - |  7003 | ` *  Visibility:` |
|       - |  7004 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7005 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7006 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7007 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7008 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7009 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7010 | ` */` |
|  327330 |  7011 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7012 |  |
|  327335 |  7013 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   21339 |  7014 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  306001 |  7015 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   46051 |  7016 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7017 | `	}` |
|       - |  7018 | `	/* Assume public by default */` |
|  259955 |  7019 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  163670 |  7020 |  |
|       - |  7021 | `/*` |
|       - |  7022 | ` * Compile a class constant.` |
|       - |  7023 | ` * According to the PHP language reference manual` |
|       - |  7024 | ` *  Class Constants` |
|       - |  7025 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7026 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7027 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7028 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7029 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7030 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7031 | ` * Symisc eXtension.` |
|       - |  7032 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7033 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7034 | ` *  Example:` |
|       - |  7035 | ` *   class Test{` |
|       - |  7036 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7037 | ` *   };` |
|       - |  7038 | ` *   var_dump(TEST::MyConst);` |
|       - |  7039 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7040 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7041 | ` */` |
|       - |  7042 | `/*` |
|       - |  7043 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7044 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7045 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7046 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7047 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7048 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7049 | ` */` |
|      78 |  7050 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7051 |  |
|       - |  7052 | `	SyToken *p0, *p1;` |
|      83 |  7053 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7054 | `		return 0;` |
|       - |  7055 | `	}` |
|      83 |  7056 | `	p0 = pGen->pIn;` |
|       - |  7057 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      83 |  7058 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7059 | `		return 1;` |
|       - |  7060 | `	}` |
|      83 |  7061 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7062 | `		return 1;` |
|       - |  7063 | `	}` |
|       - |  7064 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7065 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7066 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      79 |  7067 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      79 |  7068 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      79 |  7069 | `		if( p1 ){` |
|      79 |  7070 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      24 |  7071 | `				return 1;` |
|       - |  7072 | `			}` |
|      59 |  7073 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7074 | `				return 1;` |
|       - |  7075 | `			}` |
|      25 |  7076 | `		}` |
|      25 |  7077 | `	}` |
|      55 |  7078 | `	return 0;` |
|      44 |  7079 |  |
|       - |  7080 | `/*` |
|       - |  7081 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7082 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7083 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7084 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7085 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7086 | ` * share the same backing.` |
|       - |  7087 | ` */` |
|     206 |  7088 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7089 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7090 |  |
|     211 |  7091 | `	pAttr->nType = nType;` |
|     211 |  7092 | `	pAttr->sClass = *pClass;` |
|     211 |  7093 | `	pAttr->sTypeName = *pTypeName;` |
|     211 |  7094 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7095 | `		sxu32 i;` |
|      66 |  7096 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      46 |  7097 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      46 |  7098 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      25 |  7099 | `		}` |
|      10 |  7100 | `	}` |
|     211 |  7101 |  |
|      78 |  7102 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7103 |  |
|      83 |  7104 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7105 | `	SySet *pInstrContainer;` |
|       - |  7106 | `	ph7_class_attr *pCons;` |
|       - |  7107 | `	SyString *pName;` |
|       - |  7108 | `	sxi32 rc;` |
|      83 |  7109 | `	sxu32 nType = 0;` |
|       - |  7110 | `	SyString sTypeClass;` |
|       - |  7111 | `	SyString sTypeText;` |
|       - |  7112 | `	SySet aUnionAlts;` |
|      83 |  7113 | `	sxi32 iTypeFlags = 0;` |
|      83 |  7114 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      83 |  7115 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      83 |  7116 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7117 | `	/* Extract visibility level */` |
|      83 |  7118 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7119 | `	/* Mark as constant */` |
|      83 |  7120 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      83 |  7121 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7122 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7123 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|      97 |  7124 | `	if( GenStateClassConstHasType(pGen) ){` |
|      46 |  7125 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      28 |  7126 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7127 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7128 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7129 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7130 | `		 * and success paths release. */` |
|      32 |  7131 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7132 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7133 | `			goto Synchronize;` |
|      32 |  7134 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7135 | `			return SXERR_ABORT;` |
|      32 |  7136 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7137 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7138 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7139 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7140 | `				return SXERR_ABORT;` |
|       - |  7141 | `			}` |
|     ! 0 |  7142 | `			goto Synchronize;` |
|       - |  7143 | `		}` |
|      32 |  7144 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      14 |  7145 | `	}` |
|      39 |  7146 | `loop:` |
|      85 |  7147 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7148 | `		/* Invalid constant name */` |
|     ! 0 |  7149 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7150 | `		if( rc == SXERR_ABORT ){` |
|       - |  7151 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7152 | `			return SXERR_ABORT;` |
|       - |  7153 | `		}` |
|     ! 0 |  7154 | `		goto Synchronize;` |
|       - |  7155 | `	}` |
|       - |  7156 | `	/* Peek constant name */` |
|      85 |  7157 | `	pName = &pGen->pIn->sData;` |
|       - |  7158 | `	/* Make sure the constant name isn't reserved */` |
|      85 |  7159 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7160 | `		/* Reserved constant name */` |
|     ! 0 |  7161 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7162 | `		if( rc == SXERR_ABORT ){` |
|       - |  7163 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7164 | `			return SXERR_ABORT;` |
|       - |  7165 | `		}` |
|     ! 0 |  7166 | `		goto Synchronize;` |
|       - |  7167 | `	}` |
|       - |  7168 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      85 |  7169 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      46 |  7170 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      28 |  7171 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      14 |  7172 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      32 |  7173 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7174 | `			return SXERR_ABORT;` |
|      32 |  7175 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7176 | `			goto Synchronize;` |
|       - |  7177 | `		}` |
|      13 |  7178 | `	}` |
|       - |  7179 | `	/* Advance the stream cursor */` |
|      83 |  7180 | `	pGen->pIn++;` |
|      83 |  7181 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7182 | `		/* Invalid declaration */` |
|     ! 0 |  7183 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7184 | `		if( rc == SXERR_ABORT ){` |
|       - |  7185 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7186 | `			return SXERR_ABORT;` |
|       - |  7187 | `		}` |
|     ! 0 |  7188 | `		goto Synchronize;` |
|       - |  7189 | `	}` |
|      83 |  7190 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7191 | `	/* Allocate a new class attribute */` |
|      83 |  7192 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      83 |  7193 | `	if( pCons == 0 ){` |
|     ! 0 |  7194 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7195 | `		return SXERR_ABORT;` |
|       - |  7196 | `	}` |
|      83 |  7197 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      29 |  7198 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      13 |  7199 | `	}` |
|       - |  7200 | `	/* Swap bytecode container */` |
|      83 |  7201 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      83 |  7202 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7203 | `	/* Compile constant value.` |
|       - |  7204 | `	 */` |
|      83 |  7205 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      83 |  7206 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7207 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7208 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7209 | `			return SXERR_ABORT;` |
|       - |  7210 | `		}` |
|       1 |  7211 | `	}` |
|       - |  7212 | `	/* Emit the done instruction */` |
|      83 |  7213 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      83 |  7214 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      83 |  7215 | `	if( rc == SXERR_ABORT ){` |
|       - |  7216 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7217 | `		return SXERR_ABORT;` |
|       - |  7218 | `	}` |
|       - |  7219 | `	/* All done,install the constant */` |
|      83 |  7220 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      83 |  7221 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7222 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7223 | `		return SXERR_ABORT;` |
|       - |  7224 | `	}` |
|      83 |  7225 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7226 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7227 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7228 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7229 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7230 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7231 | `				pTok--;` |
|     ! 0 |  7232 | `			}` |
|     ! 0 |  7233 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7234 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7235 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7236 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7237 | `				return SXERR_ABORT;` |
|       - |  7238 | `			}` |
|     ! 0 |  7239 | `		}else{` |
|       3 |  7240 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7241 | `				goto loop;` |
|       - |  7242 | `			}` |
|       - |  7243 | `		}` |
|     ! 0 |  7244 | `	}` |
|      81 |  7245 | `	SySetRelease(&aUnionAlts);` |
|      81 |  7246 | `	return SXRET_OK;` |
|       1 |  7247 | `Synchronize:` |
|       3 |  7248 | `	SySetRelease(&aUnionAlts);` |
|       - |  7249 | `	/* Synchronize with the first semi-colon */` |
|       9 |  7250 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  7251 | `		pGen->pIn++;` |
|       1 |  7252 | `	}` |
|       3 |  7253 | `	return SXERR_CORRUPT;` |
|      44 |  7254 |  |
|       - |  7255 | `/*` |
|       - |  7256 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7257 | ` * According to the PHP language reference manual` |
|       - |  7258 | ` *  Properties` |
|       - |  7259 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7260 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7261 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7262 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7263 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7264 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7265 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7266 | ` * Symisc eXtension.` |
|       - |  7267 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7268 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7269 | ` *  Example:` |
|       - |  7270 | ` *   class Test{` |
|       - |  7271 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7272 | ` *   };` |
|       - |  7273 | ` *   var_dump(TEST::myVar);` |
|       - |  7274 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7275 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7276 | ` */` |
|       - |  7277 | `/*` |
|       - |  7278 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7279 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7280 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7281 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7282 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7283 | ` */` |
|  163860 |  7284 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7285 |  |
|  163865 |  7286 | `	SyToken *p = pStart;` |
|  163865 |  7287 | `	int bFirst = 1;` |
|  163865 |  7288 | `	if( p >= pEnd ) return 0;` |
|       - |  7289 | ``	/* Optional nullable `?` shorthand. */`` |
|  163865 |  7290 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      18 |  7291 | `		p++;` |
|      18 |  7292 | `		if( p >= pEnd ) return 0;` |
|       8 |  7293 | `	}` |
|       - |  7294 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7295 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7296 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7297 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   81930 |  7298 | `	for(;;){` |
|  163883 |  7299 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7300 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7301 | `			p++;` |
|       9 |  7302 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7303 | `			if( p >= pEnd ) return 0;` |
|       3 |  7304 | `			p++; /* skip ')' */` |
|       2 |  7305 | `		}else{` |
|       - |  7306 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7307 | ``			 * then any `&`-joined intersection members. */`` |
|  163881 |  7308 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  163881 |  7309 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7310 | `				return 0;` |
|       - |  7311 | `			}` |
|       - |  7312 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7313 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7314 | `			 * may still appear at the initial dispatch site). */` |
|  163881 |  7315 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  163835 |  7316 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  163830 |  7317 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3758 |  7318 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  163681 |  7319 | `					return 0;` |
|       - |  7320 | `				}` |
|      77 |  7321 | `			}` |
|     205 |  7322 | `			p++;` |
|     207 |  7323 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7324 | `				p += 2;` |
|       1 |  7325 | `			}` |
|     303 |  7326 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     208 |  7327 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7328 | `				p++; /* skip '&' */` |
|       3 |  7329 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7330 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7331 | `				p++;` |
|       3 |  7332 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7333 | `					p += 2;` |
|     ! 0 |  7334 | `				}` |
|       1 |  7335 | `			}` |
|       - |  7336 | `		}` |
|     207 |  7337 | `		bFirst = 0;` |
|     202 |  7338 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7339 | `			&& p->sData.zString[0] == '\|' ){` |
|      22 |  7340 | ``			p++; /* next `\|`-separated part */`` |
|      22 |  7341 | `			continue;` |
|       - |  7342 | `		}` |
|     189 |  7343 | `		break;` |
|     ! 0 |  7344 | `	}` |
|     189 |  7345 | `	if( p >= pEnd ) return 0;` |
|     189 |  7346 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   81935 |  7347 |  |
|       - |  7348 |  |
|       - |  7349 | `/*` |
|       - |  7350 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7351 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7352 | ` * if not). Recognized forms:` |
|       - |  7353 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7354 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7355 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7356 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7357 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7358 | ` * on unrecoverable error.` |
|       - |  7359 | ` *` |
|       - |  7360 | ` * When a type is parsed:` |
|       - |  7361 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7362 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7363 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7364 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7365 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7366 | ` */` |
|     184 |  7367 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7368 | `	ph7_gen_state *pGen,` |
|       - |  7369 | `	sxu32 *pnType,` |
|       - |  7370 | `	SyString *pClass,` |
|       - |  7371 | `	sxi32 *piTypeFlags,` |
|       - |  7372 | `	SyString *pTypeText,` |
|       - |  7373 | `	SySet *pAlts` |
|       5 |  7374 | `){` |
|     189 |  7375 | `	sxi32 iFlags = 0;` |
|       - |  7376 | `	sxi32 rc;` |
|     189 |  7377 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7378 | `		return SXRET_OK;` |
|       - |  7379 | `	}` |
|       - |  7380 | `	/* If the first token is '$', there's no type */` |
|     189 |  7381 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7382 | `		return SXRET_OK;` |
|       - |  7383 | `	}` |
|     189 |  7384 | `	rc = GenStateParseUnionTypeDecl(` |
|      92 |  7385 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7386 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7387 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7388 | `		/* bAllowVoid */ 0,` |
|     184 |  7389 | `		pGen->pIn->nLine);` |
|     189 |  7390 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7391 | `		return rc;` |
|       - |  7392 | `	}` |
|       - |  7393 | `	/* Verify next token is '$' (start of property name) */` |
|     189 |  7394 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7395 | `		return SXERR_SYNTAX;` |
|       - |  7396 | `	}` |
|     189 |  7397 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     189 |  7398 | `	return SXRET_OK;` |
|      97 |  7399 |  |
|       - |  7400 |  |
|       - |  7401 | `/*` |
|       - |  7402 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7403 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7404 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7405 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7406 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7407 | ` * by the type parser itself before reaching here.` |
|       - |  7408 | ` *` |
|       - |  7409 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7410 | ` * use in the error message.` |
|       - |  7411 | ` */` |
|     326 |  7412 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7413 | `	sxu32 nType,` |
|       - |  7414 | `	const SyString *pClass,` |
|       - |  7415 | `	const char **pzName,` |
|       - |  7416 | `	sxu32 *pnName)` |
|       5 |  7417 |  |
|       - |  7418 | `	const char *z;` |
|       - |  7419 | `	sxu32 n;` |
|     331 |  7420 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     277 |  7421 | `		return 0;` |
|       - |  7422 | `	}` |
|      59 |  7423 | `	z = pClass->zString;` |
|      59 |  7424 | `	n = pClass->nByte;` |
|      59 |  7425 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7426 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7427 | `	}` |
|       - |  7428 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7429 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7430 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      52 |  7431 | `	return 0;` |
|     168 |  7432 |  |
|       - |  7433 |  |
|       - |  7434 | `/*` |
|       - |  7435 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7436 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7437 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7438 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7439 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7440 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7441 | ` *` |
|       - |  7442 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7443 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7444 | ` */` |
|     268 |  7445 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7446 | `	ph7_gen_state *pGen,` |
|       - |  7447 | `	ph7_class *pClass,` |
|       - |  7448 | `	const SyString *pMemberName,` |
|       - |  7449 | `	sxu32 nType,` |
|       - |  7450 | `	const SyString *pTypeClass,` |
|       - |  7451 | `	const SyString *pTypeText,` |
|       - |  7452 | `	SySet *pUnionAlts,` |
|       - |  7453 | `	const char *zErrFmt,` |
|       - |  7454 | `	sxu32 nLine)` |
|       5 |  7455 |  |
|     273 |  7456 | `	const char *zBad = 0;` |
|     273 |  7457 | `	sxu32 nBad = 0;` |
|       - |  7458 | `	SyString sFallback;` |
|       - |  7459 | `	const SyString *pBad;` |
|       - |  7460 | `	sxi32 rc;` |
|     273 |  7461 | `	int bDisallowed = 0;` |
|     273 |  7462 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7463 | `		bDisallowed = 1;` |
|     271 |  7464 | `	}else if( pUnionAlts ){` |
|       - |  7465 | `		sxu32 i;` |
|      88 |  7466 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      62 |  7467 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      62 |  7468 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7469 | `				bDisallowed = 1;` |
|       3 |  7470 | `				break;` |
|       - |  7471 | `			}` |
|      32 |  7472 | `		}` |
|      14 |  7473 | `	}` |
|     273 |  7474 | `	if( !bDisallowed ){` |
|     267 |  7475 | `		return SXRET_OK;` |
|       - |  7476 | `	}` |
|       - |  7477 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7478 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7479 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7480 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7481 | `		pBad = pTypeText;` |
|       5 |  7482 | `	}else{` |
|     ! 0 |  7483 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7484 | `		pBad = &sFallback;` |
|       - |  7485 | `	}` |
|      11 |  7486 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7487 | `		zErrFmt,` |
|       3 |  7488 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7489 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7490 | `		return SXERR_ABORT;` |
|       - |  7491 | `	}` |
|       8 |  7492 | `	return SXERR_SYNTAX;` |
|     139 |  7493 |  |
|       - |  7494 | `/*` |
|       - |  7495 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7496 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7497 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7498 | ` * than promoted to a lexer keyword.` |
|       - |  7499 | ` */` |
| 1556744 |  7500 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7501 |  |
| 1590231 |  7502 | `	return (pTok->nType & PH7_TK_ID)` |
|  811854 |  7503 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1590226 |  7504 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7505 |  |
|   74952 |  7506 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7507 |  |
|   74957 |  7508 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7509 | `	ph7_class_attr *pAttr;` |
|       - |  7510 | `	SyString *pName;` |
|       - |  7511 | `	sxi32 rc;` |
|   74957 |  7512 | `	sxu32 nType = 0;` |
|       - |  7513 | `	SyString sTypeClass;` |
|       - |  7514 | `	SyString sTypeText;` |
|       - |  7515 | `	SySet aUnionAlts;` |
|   74957 |  7516 | `	sxi32 iTypeFlags = 0;` |
|   74957 |  7517 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   74957 |  7518 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   74957 |  7519 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7520 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7521 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7522 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   74957 |  7523 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7524 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7525 | `	}` |
|       - |  7526 | `	/* Extract visibility level */` |
|   74957 |  7527 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7528 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   75049 |  7529 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     189 |  7530 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     189 |  7531 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7532 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7533 | `			goto Synchronize;` |
|     189 |  7534 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7535 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7536 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7537 | `				&pGen->pIn->sData);` |
|     ! 0 |  7538 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7539 | `				return SXERR_ABORT;` |
|       - |  7540 | `			}` |
|     ! 0 |  7541 | `			goto Synchronize;` |
|     189 |  7542 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7543 | `			return SXERR_ABORT;` |
|       - |  7544 | `		}` |
|      92 |  7545 | `	}` |
|     ! 0 |  7546 | `loop:` |
|   74961 |  7547 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7548 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7549 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7550 | `			return SXERR_ABORT;` |
|       - |  7551 | `		}` |
|     ! 0 |  7552 | `		goto Synchronize;` |
|       - |  7553 | `	}` |
|   74961 |  7554 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   74961 |  7555 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7556 | `		/* Invalid attribute name */` |
|     ! 0 |  7557 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7558 | `		if( rc == SXERR_ABORT ){` |
|       - |  7559 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7560 | `			return SXERR_ABORT;` |
|       - |  7561 | `		}` |
|     ! 0 |  7562 | `		goto Synchronize;` |
|       - |  7563 | `	}` |
|       - |  7564 | `	/* Peek attribute name */` |
|   74961 |  7565 | `	pName = &pGen->pIn->sData;` |
|       - |  7566 | `	/* Advance the stream cursor */` |
|   74961 |  7567 | `	pGen->pIn++;` |
|   74961 |  7568 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7569 | `		/* Invalid declaration */` |
|       3 |  7570 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7571 | `		if( rc == SXERR_ABORT ){` |
|       - |  7572 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7573 | `			return SXERR_ABORT;` |
|       - |  7574 | `		}` |
|       3 |  7575 | `		goto Synchronize;` |
|       - |  7576 | `	}` |
|       - |  7577 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7578 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   74959 |  7579 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7580 | `		const char *zRoErr = 0;` |
|      39 |  7581 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7582 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7583 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7584 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7585 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7586 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7587 | `		}` |
|      39 |  7588 | `		if( zRoErr ){` |
|      13 |  7589 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7590 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7591 | `				return SXERR_ABORT;` |
|       - |  7592 | `			}` |
|      13 |  7593 | `			goto Synchronize;` |
|       - |  7594 | `		}` |
|      12 |  7595 | `	}` |
|       - |  7596 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7597 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7598 | `	 * by the type parser. */` |
|   74949 |  7599 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     278 |  7600 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7601 | `			&sTypeText,` |
|     182 |  7602 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      91 |  7603 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     187 |  7604 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7605 | `			return SXERR_ABORT;` |
|     187 |  7606 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7607 | `			goto Synchronize;` |
|       - |  7608 | `		}` |
|      91 |  7609 | `	}` |
|       - |  7610 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   74949 |  7611 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7612 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7613 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7614 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7615 | `			return SXERR_ABORT;` |
|       - |  7616 | `		}` |
|       3 |  7617 | `		goto Synchronize;` |
|       - |  7618 | `	}` |
|       - |  7619 | `	/* Allocate a new class attribute */` |
|   74947 |  7620 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   74947 |  7621 | `	if( pAttr == 0 ){` |
|     ! 0 |  7622 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7623 | `		return SXERR_ABORT;` |
|       - |  7624 | `	}` |
|   74947 |  7625 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     185 |  7626 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      90 |  7627 | `	}` |
|   74947 |  7628 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7629 | `		SySet *pInstrContainer;` |
|   21699 |  7630 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7631 | `		/* Swap bytecode container */` |
|   21699 |  7632 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   21699 |  7633 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7634 | `		/* Compile attribute value.` |
|       - |  7635 | `		 */` |
|   21699 |  7636 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   21699 |  7637 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7638 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7639 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7640 | `				return SXERR_ABORT;` |
|       - |  7641 | `			}` |
|     ! 0 |  7642 | `		}` |
|       - |  7643 | `		/* Emit the done instruction */` |
|   21699 |  7644 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   21699 |  7645 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   10847 |  7646 | `	}` |
|       - |  7647 | `	/* All done,install the attribute */` |
|   74947 |  7648 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   74947 |  7649 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7650 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7651 | `		return SXERR_ABORT;` |
|       - |  7652 | `	}` |
|   74947 |  7653 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7654 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7655 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7656 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7657 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7658 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7659 | `				pTok--;` |
|     ! 0 |  7660 | `			}` |
|     ! 0 |  7661 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7662 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7663 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7664 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7665 | `				return SXERR_ABORT;` |
|       - |  7666 | `			}` |
|     ! 0 |  7667 | `		}else{` |
|       5 |  7668 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7669 | `				goto loop;` |
|       - |  7670 | `			}` |
|       - |  7671 | `		}` |
|     ! 0 |  7672 | `	}` |
|   74943 |  7673 | `	SySetRelease(&aUnionAlts);` |
|   74943 |  7674 | `	return SXRET_OK;` |
|       7 |  7675 | `Synchronize:` |
|       - |  7676 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7677 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      16 |  7678 | `		pGen->pIn++;` |
|       2 |  7679 | `	}` |
|      17 |  7680 | `	SySetRelease(&aUnionAlts);` |
|      17 |  7681 | `	return SXERR_CORRUPT;` |
|   37481 |  7682 |  |
|       - |  7683 | `/*` |
|       - |  7684 | ` * Compile a class method.` |
|       - |  7685 | ` *` |
|       - |  7686 | ` * Refer to the official documentation for more information` |
|       - |  7687 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7688 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7689 | ` * overloading and many more.` |
|       - |  7690 | ` */` |
|  252300 |  7691 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7692 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7693 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7694 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7695 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7696 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7697 | `	)` |
|       5 |  7698 |  |
|  252305 |  7699 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7700 | `	ph7_class_method *pMeth;` |
|       - |  7701 | `	sxi32 iFuncFlags;` |
|       - |  7702 | `	SyString *pName;` |
|       - |  7703 | `	SyToken *pEnd;` |
|       - |  7704 | `	sxi32 rc;` |
|       - |  7705 | `	/* Extract visibility level */` |
|  252305 |  7706 | `	iProtection = GetProtectionLevel(iProtection);` |
|  252305 |  7707 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  252305 |  7708 | `	iFuncFlags = 0;` |
|  252305 |  7709 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7710 | `		/* Invalid method name */` |
|     ! 0 |  7711 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7712 | `		if( rc == SXERR_ABORT ){` |
|       - |  7713 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7714 | `			return SXERR_ABORT;` |
|       - |  7715 | `		}` |
|     ! 0 |  7716 | `		goto Synchronize;` |
|       - |  7717 | `	}` |
|  252305 |  7718 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7719 | `		/* Return by reference,remember that */` |
|     ! 0 |  7720 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7721 | `		/* Jump the '&' token */` |
|     ! 0 |  7722 | `		pGen->pIn++;` |
|     ! 0 |  7723 | `	}` |
|  252305 |  7724 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7725 | `		/* Invalid method name */` |
|     ! 0 |  7726 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7727 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7728 | `			return SXERR_ABORT;` |
|       - |  7729 | `		}` |
|     ! 0 |  7730 | `		goto Synchronize;` |
|       - |  7731 | `	}` |
|       - |  7732 | `	/* Peek method name */` |
|  252305 |  7733 | `	pName = &pGen->pIn->sData;` |
|  252305 |  7734 | `	nLine = pGen->pIn->nLine;` |
|       - |  7735 | `	/* Jump the method name */` |
|  252305 |  7736 | `	pGen->pIn++;` |
|  252305 |  7737 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7738 | `		/* Abstract method */` |
|   92047 |  7739 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7740 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7741 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7742 | `				&pClass->sName,pName);` |
|     ! 0 |  7743 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7744 | `				return SXERR_ABORT;` |
|       - |  7745 | `			}` |
|     ! 0 |  7746 | `		}` |
|       - |  7747 | `		/* Assemble method signature only */` |
|   92047 |  7748 | `		doBody = FALSE;` |
|   46021 |  7749 | `	}` |
|  252305 |  7750 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7751 | `		/* Syntax error */` |
|     ! 0 |  7752 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7753 | `		if( rc == SXERR_ABORT ){` |
|       - |  7754 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7755 | `			return SXERR_ABORT;` |
|       - |  7756 | `		}` |
|     ! 0 |  7757 | `		goto Synchronize;` |
|       - |  7758 | `	}` |
|       - |  7759 | `	/* Allocate a new class_method instance */` |
|  252305 |  7760 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  252305 |  7761 | `	if( pMeth == 0 ){` |
|     ! 0 |  7762 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7763 | `		return SXERR_ABORT;` |
|       - |  7764 | `	}` |
|       - |  7765 | `	/* Jump the left parenthesis '(' */` |
|  252305 |  7766 | `	pGen->pIn++;` |
|  252305 |  7767 | `	pEnd = 0; /* cc warning */` |
|       - |  7768 | `	/* Delimit the method signature */` |
|  252305 |  7769 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  252305 |  7770 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7771 | `		/* Syntax error */` |
|       3 |  7772 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7773 | `		if( rc == SXERR_ABORT ){` |
|       - |  7774 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7775 | `			return SXERR_ABORT;` |
|       - |  7776 | `		}` |
|       3 |  7777 | `		goto Synchronize;` |
|       - |  7778 | `	}` |
|       - |  7779 | `	{` |
|  252303 |  7780 | `		int bIsCtor = 0;` |
|  252303 |  7781 | `		int bAbstractCtor = 0;` |
|  252298 |  7782 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  151038 |  7783 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  241609 |  7784 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   21393 |  7785 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7786 | `				bAbstractCtor = 1;` |
|       2 |  7787 | `			}else{` |
|   21391 |  7788 | `				bIsCtor = 1;` |
|       - |  7789 | `			}` |
|   10694 |  7790 | `		}` |
|  252303 |  7791 | `		if( pGen->pIn < pEnd ){` |
|       - |  7792 | `			/* Collect method arguments */` |
|   57051 |  7793 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   57051 |  7794 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7795 | `				return SXERR_ABORT;` |
|       - |  7796 | `			}` |
|   28523 |  7797 | `		}` |
|       - |  7798 | `	}` |
|       - |  7799 | `	/* Point past ')' and parse optional return type ': type' */` |
|  252303 |  7800 | `	pGen->pIn = &pEnd[1];` |
|       - |  7801 | `	{` |
|  252303 |  7802 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  252303 |  7803 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7804 | `			return SXERR_ABORT;` |
|  252303 |  7805 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7806 | `			goto Synchronize;` |
|       - |  7807 | `		}` |
|       - |  7808 | `	}` |
|       - |  7809 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7810 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7811 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7812 | `	{` |
|  252303 |  7813 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7814 | `		sxu32 i;` |
|  344829 |  7815 | `		for( i = 0; i < nArg; i++ ){` |
|   92541 |  7816 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7817 | `			ph7_class_attr *pAttr;` |
|   92541 |  7818 | `			sxi32 iAttrFlags = 0;` |
|       - |  7819 | `			int bArgTyped;` |
|   92541 |  7820 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   92477 |  7821 | `				continue;` |
|       - |  7822 | `			}` |
|       - |  7823 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  7824 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  7825 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      49 |  7826 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      70 |  7827 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      69 |  7828 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7829 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7830 | `					"Cannot declare variadic promoted property");` |
|       3 |  7831 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7832 | `					return SXERR_ABORT;` |
|       - |  7833 | `				}` |
|       3 |  7834 | `				goto Synchronize;` |
|       - |  7835 | `			}` |
|       - |  7836 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7837 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7838 | `			 * appear as an alternative of a union type. */` |
|      67 |  7839 | `			if( bArgTyped ){` |
|      92 |  7840 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      58 |  7841 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      58 |  7842 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      29 |  7843 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      63 |  7844 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7845 | `					return SXERR_ABORT;` |
|      63 |  7846 | `				}else if( rc != SXRET_OK ){` |
|       6 |  7847 | `					goto Synchronize;` |
|       - |  7848 | `				}` |
|      27 |  7849 | `			}` |
|       - |  7850 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      63 |  7851 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7852 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7853 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7854 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7855 | `					return SXERR_ABORT;` |
|       - |  7856 | `				}` |
|       3 |  7857 | `				goto Synchronize;` |
|       - |  7858 | `			}` |
|      61 |  7859 | `			if( bArgTyped ){` |
|      57 |  7860 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      26 |  7861 | `			}` |
|      61 |  7862 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7863 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7864 | `			}` |
|      61 |  7865 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  7866 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  7867 | `			}` |
|      61 |  7868 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  7869 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  7870 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  7871 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  7872 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7873 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  7874 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7875 | `						return SXERR_ABORT;` |
|       - |  7876 | `					}` |
|       3 |  7877 | `					goto Synchronize;` |
|       - |  7878 | `				}` |
|      22 |  7879 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7880 | `			}` |
|      59 |  7881 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      59 |  7882 | `			if( pAttr == 0 ){` |
|     ! 0 |  7883 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7884 | `				return SXERR_ABORT;` |
|       - |  7885 | `			}` |
|      59 |  7886 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      57 |  7887 | `				pAttr->nType = pArg->nType;` |
|      57 |  7888 | `				pAttr->sClass = pArg->sClass;` |
|      57 |  7889 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      57 |  7890 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7891 | `					sxu32 k;` |
|      20 |  7892 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  7893 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  7894 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  7895 | `					}` |
|       3 |  7896 | `				}` |
|      26 |  7897 | `			}` |
|      59 |  7898 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      59 |  7899 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7900 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7901 | `				return SXERR_ABORT;` |
|       - |  7902 | `			}` |
|      32 |  7903 | `		}` |
|       - |  7904 | `	}` |
|  252293 |  7905 | `	if( doBody ){` |
|       - |  7906 | `		/* Compile method body */` |
|  160251 |  7907 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  160251 |  7908 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7909 | `			return SXERR_ABORT;` |
|       - |  7910 | `		}` |
|   80128 |  7911 | `	}else{` |
|       - |  7912 | `		/* Only method signature is allowed */` |
|   92047 |  7913 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7914 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7915 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7916 | `				if( rc == SXERR_ABORT ){` |
|       - |  7917 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7918 | `					return SXERR_ABORT;` |
|       - |  7919 | `				}` |
|     ! 0 |  7920 | `				return SXERR_CORRUPT;` |
|       - |  7921 | `			}` |
|       - |  7922 | `	}` |
|       - |  7923 | `	/* All done,install the method */` |
|  252293 |  7924 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  252293 |  7925 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7926 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7927 | `		return SXERR_ABORT;` |
|       - |  7928 | `	}` |
|  252293 |  7929 | `	return SXRET_OK;` |
|       6 |  7930 | `Synchronize:` |
|       - |  7931 | `	/* Synchronize with the first semi-colon */` |
|      40 |  7932 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  7933 | `		pGen->pIn++;` |
|       4 |  7934 | `	}` |
|      16 |  7935 | `	return SXERR_CORRUPT;` |
|  126155 |  7936 |  |
|       - |  7937 | `/*` |
|       - |  7938 | ` * Compile an object interface.` |
|       - |  7939 | ` *  According to the PHP language reference manual` |
|       - |  7940 | ` *   Object Interfaces:` |
|       - |  7941 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7942 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7943 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7944 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7945 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7946 | ` */` |
|   38996 |  7947 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  7948 |  |
|   39001 |  7949 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7950 | `	ph7_class *pClass,*pBase;` |
|       - |  7951 | `	SyToken *pEnd,*pTmp;` |
|       - |  7952 | `	SyString *pName;` |
|       - |  7953 | `	sxi32 nKwrd;` |
|       - |  7954 | `	sxi32 rc;` |
|       - |  7955 | `	/* Jump the 'interface' keyword */` |
|   39001 |  7956 | `	pGen->pIn++;` |
|       - |  7957 | `	/* Extract interface name */` |
|   39001 |  7958 | `	pName = &pGen->pIn->sData;` |
|       - |  7959 | `	/* Advance the stream cursor */` |
|   39001 |  7960 | `	pGen->pIn++;` |
|       - |  7961 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7962 | `		SyBlob sFQN;` |
|       - |  7963 | `		SyString sFQNStr;` |
|   39001 |  7964 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   39001 |  7965 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   39001 |  7966 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   39001 |  7967 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   39001 |  7968 | `		SyBlobRelease(&sFQN);` |
|       - |  7969 | `	}` |
|   39001 |  7970 | `	if( pClass == 0 ){` |
|     ! 0 |  7971 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7972 | `		return SXERR_ABORT;` |
|       - |  7973 | `	}` |
|       - |  7974 | `	/* Mark as an interface */` |
|   39001 |  7975 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7976 | `	/* Assume no base class is given */` |
|   39001 |  7977 | `	pBase = 0;` |
|   39001 |  7978 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   10627 |  7979 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10627 |  7980 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7981 | `			SyBlob sResolved;` |
|       - |  7982 | `			SyString sBaseName;` |
|       - |  7983 | `			sxu32 nRefLine;` |
|       - |  7984 | `			/* Extract base interface */` |
|   10627 |  7985 | `			pGen->pIn++;` |
|   10627 |  7986 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10627 |  7987 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10627 |  7988 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7989 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7990 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7991 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7992 | `					pName);` |
|     ! 0 |  7993 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7994 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7995 | `					return SXERR_ABORT;` |
|       - |  7996 | `				}` |
|     ! 0 |  7997 | `				return SXRET_OK;` |
|       - |  7998 | `			}` |
|   15938 |  7999 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   10622 |  8000 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10627 |  8001 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8002 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8003 | `			/* Only interfaces is allowed */` |
|   10627 |  8004 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8005 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8006 | `			}` |
|   10627 |  8007 | `			if( pBase == 0 ){` |
|     ! 0 |  8008 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8009 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8010 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8011 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8012 | `					return SXERR_ABORT;` |
|       - |  8013 | `				}` |
|     ! 0 |  8014 | `			}` |
|   10627 |  8015 | `			SyBlobRelease(&sResolved);` |
|    5311 |  8016 | `		}` |
|    5311 |  8017 | `	}` |
|   39001 |  8018 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8019 | `		/* Syntax error */` |
|     ! 0 |  8020 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8021 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8022 | `		if( rc == SXERR_ABORT ){` |
|       - |  8023 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8024 | `			return SXERR_ABORT;` |
|       - |  8025 | `		}` |
|     ! 0 |  8026 | `		return SXRET_OK;` |
|       - |  8027 | `	}` |
|   39001 |  8028 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   39001 |  8029 | `	pEnd = 0; /* cc warning */` |
|       - |  8030 | `	/* Delimit the interface body */` |
|   39001 |  8031 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   39001 |  8032 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8033 | `		/* Syntax error */` |
|     ! 0 |  8034 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8035 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8036 | `		if( rc == SXERR_ABORT ){` |
|       - |  8037 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8038 | `			return SXERR_ABORT;` |
|       - |  8039 | `		}` |
|     ! 0 |  8040 | `		return SXRET_OK;` |
|       - |  8041 | `	}` |
|       - |  8042 | `	/* Swap token stream */` |
|   39001 |  8043 | `	pTmp = pGen->pEnd;` |
|   39001 |  8044 | `	pGen->pEnd = pEnd;` |
|       - |  8045 | `	/* Start the parse process` |
|       - |  8046 | `	 * Note (According to the PHP reference manual):` |
|       - |  8047 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8048 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8049 | `	 */` |
|   65515 |  8050 | `	for(;;){` |
|       - |  8051 | `		/* Jump leading/trailing semi-colons */` |
|  223069 |  8052 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   92039 |  8053 | `			pGen->pIn++;` |
|       5 |  8054 | `		}` |
|  131035 |  8055 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8056 | `			/* End of interface body */` |
|   38999 |  8057 | `			break;` |
|       - |  8058 | `		}` |
|   92041 |  8059 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8060 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8061 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8062 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8063 | `			if( rc == SXERR_ABORT ){` |
|       - |  8064 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8065 | `				return SXERR_ABORT;` |
|       - |  8066 | `			}` |
|     ! 0 |  8067 | `			goto done;` |
|       - |  8068 | `		}` |
|       - |  8069 | `		/* Extract the current keyword */` |
|   92041 |  8070 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   92041 |  8071 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8072 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8073 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8074 | `			const char *zKind = "member";` |
|       3 |  8075 | `			SyString *pMemberName = 0;` |
|       3 |  8076 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8077 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8078 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8079 | `					zKind = "constant";` |
|       3 |  8080 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8081 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8082 | `					}` |
|       1 |  8083 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8084 | `					zKind = "method";` |
|     ! 0 |  8085 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8086 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8087 | `					}` |
|     ! 0 |  8088 | `				}` |
|       1 |  8089 | `			}` |
|       3 |  8090 | `			if( pMemberName ){` |
|       4 |  8091 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8092 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8093 | `			}else{` |
|     ! 0 |  8094 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8095 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8096 | `			}` |
|       3 |  8097 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8098 | `				return SXERR_ABORT;` |
|       - |  8099 | `			}` |
|       3 |  8100 | `			goto done;` |
|       - |  8101 | `		}` |
|   92039 |  8102 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8103 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8104 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8105 | `			if( rc == SXERR_ABORT ){` |
|       - |  8106 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8107 | `				return SXERR_ABORT;` |
|       - |  8108 | `			}` |
|     ! 0 |  8109 | `			goto done;` |
|       - |  8110 | `		}` |
|   92039 |  8111 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8112 | `			/* Advance the stream cursor */` |
|   92029 |  8113 | `			pGen->pIn++;` |
|   92029 |  8114 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8115 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8116 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8117 | `				if( rc == SXERR_ABORT ){` |
|       - |  8118 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8119 | `					return SXERR_ABORT;` |
|       - |  8120 | `				}` |
|     ! 0 |  8121 | `				goto done;` |
|       - |  8122 | `			}` |
|   92029 |  8123 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   92029 |  8124 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8125 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8126 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8127 | `				if( rc == SXERR_ABORT ){` |
|       - |  8128 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8129 | `					return SXERR_ABORT;` |
|       - |  8130 | `				}` |
|     ! 0 |  8131 | `				goto done;` |
|       - |  8132 | `			}` |
|   46012 |  8133 | `		}` |
|   92039 |  8134 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8135 | `			/* Parse constant */` |
|       7 |  8136 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       7 |  8137 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8138 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8139 | `					return SXERR_ABORT;` |
|       - |  8140 | `				}` |
|     ! 0 |  8141 | `				goto done;` |
|       - |  8142 | `			}` |
|       4 |  8143 | `		}else{` |
|   92033 |  8144 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   92033 |  8145 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8146 | `				/* Static method,record that */` |
|   10619 |  8147 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8148 | `				/* Advance the stream cursor */` |
|   10619 |  8149 | `				pGen->pIn++;` |
|   10614 |  8150 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   10619 |  8151 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8152 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8153 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8154 | `						if( rc == SXERR_ABORT ){` |
|       - |  8155 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8156 | `							return SXERR_ABORT;` |
|       - |  8157 | `						}` |
|     ! 0 |  8158 | `						goto done;` |
|       - |  8159 | `				}` |
|    5307 |  8160 | `			}` |
|       - |  8161 | `			/* Process method signature (no body for interface methods) */` |
|   92033 |  8162 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   92033 |  8163 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8164 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8165 | `					return SXERR_ABORT;` |
|       - |  8166 | `				}` |
|     ! 0 |  8167 | `				goto done;` |
|       - |  8168 | `			}` |
|       - |  8169 | `		}` |
|       5 |  8170 | `	}` |
|       - |  8171 | `	/* Install the interface */` |
|   38999 |  8172 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   38999 |  8173 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8174 | `		/* Inherit from the base interface */` |
|   10627 |  8175 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5311 |  8176 | `	}` |
|   38999 |  8177 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8178 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8179 | `		return SXERR_ABORT;` |
|       - |  8180 | `	}` |
|   19497 |  8181 | `done:` |
|       - |  8182 | `	/* Point beyond the interface body */` |
|   39001 |  8183 | `	pGen->pIn  = &pEnd[1];` |
|   39001 |  8184 | `	pGen->pEnd = pTmp;` |
|   39001 |  8185 | `	return PH7_OK;` |
|   19503 |  8186 |  |
|       - |  8187 | `/*` |
|       - |  8188 | ` * Compile a user-defined class.` |
|       - |  8189 | ` * According to the PHP language reference manual` |
|       - |  8190 | ` *  class` |
|       - |  8191 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8192 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8193 | ` *  of the properties and methods belonging to the class.` |
|       - |  8194 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8195 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8196 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8197 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8198 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8199 | ` *  (called "methods").` |
|       - |  8200 | ` */` |
|       - |  8201 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8202 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8203 | `struct TraitUseEntry {` |
|       - |  8204 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8205 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8206 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8207 | `};` |
|       - |  8208 | `/*` |
|       - |  8209 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8210 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8211 | ` */` |
|  100254 |  8212 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8213 |  |
|       - |  8214 | `	ph7_class **apIface;` |
|       - |  8215 | `	sxu32 nIface,i;` |
|       - |  8216 | `	sxi32 rc;` |
|  100259 |  8217 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8218 | `		return SXRET_OK;` |
|       - |  8219 | `	}` |
|  100259 |  8220 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  100259 |  8221 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  192495 |  8222 | `	for(i = 0; i < nIface; i++){` |
|   92241 |  8223 | `		ph7_class *pIface = apIface[i];` |
|       - |  8224 | `		SyHashEntry *pEntry;` |
|   92241 |  8225 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  248363 |  8226 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  156127 |  8227 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8228 | `			ph7_class_method *pImplMeth;` |
|  156127 |  8229 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8230 | `			/* Find the implementing method in the class */` |
|  156127 |  8231 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  156127 |  8232 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8233 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8234 | `			}` |
|       - |  8235 | `			/* Check visibility: interface methods must be implemented as public */` |
|  156113 |  8236 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8237 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8238 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8239 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8240 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8241 | `					return SXERR_ABORT;` |
|       - |  8242 | `				}` |
|       1 |  8243 | `			}` |
|       - |  8244 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8245 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8246 | `			 */` |
|       - |  8247 | `			{` |
|  156113 |  8248 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  156113 |  8249 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  156113 |  8250 | `				int sigError = 0;` |
|  156113 |  8251 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8252 | `					sigError = 1;` |
|  156112 |  8253 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8254 | `					/* Extra parameters must all have default values */` |
|       6 |  8255 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8256 | `					sxu32 k;` |
|       8 |  8257 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8258 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8259 | `							sigError = 1;` |
|       3 |  8260 | `							break;` |
|       - |  8261 | `						}` |
|       2 |  8262 | `					}` |
|       2 |  8263 | `				}` |
|  156113 |  8264 | `				if( sigError ){` |
|       - |  8265 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8266 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8267 | `					sxu32 j;` |
|       6 |  8268 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8269 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8270 | `					/* Build implementing method signature */` |
|       6 |  8271 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8272 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8273 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8274 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8275 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8276 | `					}` |
|       - |  8277 | `					/* Build interface method signature */` |
|       6 |  8278 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8279 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8280 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8281 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8282 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8283 | `					}` |
|       8 |  8284 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8285 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8286 | `						&pClass->sName,pMName,` |
|       4 |  8287 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8288 | `						&pIface->sName,pMName,` |
|       4 |  8289 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8290 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8291 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8292 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8293 | `						return SXERR_ABORT;` |
|       - |  8294 | `					}` |
|       2 |  8295 | `				}` |
|       - |  8296 | `			}` |
|       5 |  8297 | `		}` |
|   46123 |  8298 | `	}` |
|  100259 |  8299 | `	return SXRET_OK;` |
|   50132 |  8300 |  |
|       - |  8301 | `/*` |
|       - |  8302 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8303 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8304 | ` */` |
|  100254 |  8305 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8306 |  |
|       - |  8307 | `	ph7_class_method *pMeth;` |
|       - |  8308 | `	SyHashEntry *pEntry;` |
|       - |  8309 | `	sxu32 nAbstract;` |
|       - |  8310 | `	SyBlob sMsg;` |
|       - |  8311 | `	sxi32 rc;` |
|       - |  8312 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  100259 |  8313 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      33 |  8314 | `		return SXRET_OK;` |
|       - |  8315 | `	}` |
|       - |  8316 | `	/* Count abstract methods */` |
|  100231 |  8317 | `	nAbstract = 0;` |
|  100231 |  8318 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  926241 |  8319 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  826015 |  8320 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  826015 |  8321 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8322 | `			nAbstract++;` |
|       8 |  8323 | `		}` |
|       5 |  8324 | `	}` |
|  100231 |  8325 | `	if( nAbstract == 0 ){` |
|  100217 |  8326 | `		return SXRET_OK;` |
|       - |  8327 | `	}` |
|       - |  8328 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8329 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8330 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8331 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8332 | `		&pClass->sName,nAbstract,` |
|       7 |  8333 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8334 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8335 | `	/* Second pass: list methods with origins */` |
|       - |  8336 | `	{` |
|      18 |  8337 | `		sxu32 nListed = 0;` |
|      18 |  8338 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8339 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8340 | `			ph7_class *pOrigin = 0;` |
|       - |  8341 | `			SyString *pMName;` |
|      22 |  8342 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8343 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8344 | `				continue;` |
|       - |  8345 | `			}` |
|      20 |  8346 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8347 | `			if( nListed > 0 ){` |
|       3 |  8348 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8349 | `			}` |
|       - |  8350 | `			/* Find the origin of this abstract method.` |
|       - |  8351 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8352 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8353 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8354 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8355 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8356 | `			 * class's namespace.` |
|       - |  8357 | `			 */` |
|       - |  8358 | `			{` |
|       - |  8359 | `				ph7_class **apIface;` |
|       - |  8360 | `				ph7_class **apTrait;` |
|       - |  8361 | `				ph7_class *pWalk;` |
|       - |  8362 | `				sxu32 i;` |
|       - |  8363 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8364 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8365 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8366 | `				 */` |
|      20 |  8367 | `				if( pClass->pBase ){` |
|      11 |  8368 | `					pWalk = pClass->pBase;` |
|      19 |  8369 | `					while( pWalk ){` |
|       - |  8370 | `						ph7_class_method *pParentMeth;` |
|      13 |  8371 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8372 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8373 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8374 | `							 * in this class's ancestor chain.` |
|       - |  8375 | `							 */` |
|      13 |  8376 | `							int fromIface = 0;` |
|      13 |  8377 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8378 | `							while( pAnc ){` |
|       - |  8379 | `								ph7_class **apPI;` |
|       - |  8380 | `								sxu32 j;` |
|      15 |  8381 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8382 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8383 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8384 | `										fromIface = 1;` |
|      10 |  8385 | `										break;` |
|       - |  8386 | `									}` |
|     ! 0 |  8387 | `								}` |
|      15 |  8388 | `								if( fromIface ) break;` |
|       6 |  8389 | `								pAnc = pAnc->pBase;` |
|       2 |  8390 | `							}` |
|      13 |  8391 | `							if( !fromIface ){` |
|       3 |  8392 | `								pOrigin = pWalk;` |
|       3 |  8393 | `								break;` |
|       - |  8394 | `							}` |
|       4 |  8395 | `						}` |
|      10 |  8396 | `						pWalk = pWalk->pBase;` |
|       2 |  8397 | `					}` |
|       4 |  8398 | `				}` |
|       - |  8399 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8400 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8401 | `				 */` |
|      20 |  8402 | `				if( !pOrigin ){` |
|      18 |  8403 | `					pWalk = pClass;` |
|      40 |  8404 | `					while( pWalk && !pOrigin ){` |
|      26 |  8405 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8406 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8407 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8408 | `							ph7_class *pDeepest = 0;` |
|      28 |  8409 | `							while( pIface ){` |
|      16 |  8410 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8411 | `									pDeepest = pIface;` |
|       6 |  8412 | `								}` |
|      16 |  8413 | `								pIface = pIface->pBase;` |
|       4 |  8414 | `							}` |
|      16 |  8415 | `							if( pDeepest ){` |
|      16 |  8416 | `								pOrigin = pDeepest;` |
|      16 |  8417 | `								break;` |
|       - |  8418 | `							}` |
|     ! 0 |  8419 | `						}` |
|      26 |  8420 | `						pWalk = pWalk->pBase;` |
|       4 |  8421 | `					}` |
|       7 |  8422 | `				}` |
|       - |  8423 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8424 | `				if( !pOrigin ){` |
|       3 |  8425 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8426 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8427 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8428 | `							pOrigin = pClass;` |
|       3 |  8429 | `							break;` |
|       - |  8430 | `						}` |
|     ! 0 |  8431 | `					}` |
|       1 |  8432 | `				}` |
|       - |  8433 | `			}` |
|      20 |  8434 | `			if( pOrigin ){` |
|      20 |  8435 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8436 | `			}else{` |
|       - |  8437 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8438 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8439 | `			}` |
|      20 |  8440 | `			nListed++;` |
|       4 |  8441 | `		}` |
|       - |  8442 | `	}` |
|      18 |  8443 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8444 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8445 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8446 | `	SyBlobRelease(&sMsg);` |
|      18 |  8447 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8448 | `		return SXERR_ABORT;` |
|       - |  8449 | `	}` |
|      18 |  8450 | `	return SXRET_OK;` |
|   50132 |  8451 |  |
|       - |  8452 | `/*` |
|       - |  8453 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8454 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8455 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8456 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8457 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8458 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8459 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8460 | ` */` |
|   96480 |  8461 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8462 |  |
|   96485 |  8463 | `	int isAbsolute = 0;` |
|   96485 |  8464 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8465 | `	SyBlob sName;` |
|   96485 |  8466 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      95 |  8467 | `		isAbsolute = 1;` |
|      95 |  8468 | `		pGen->pIn++;` |
|      45 |  8469 | `	}` |
|   96485 |  8470 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       8 |  8471 | `		pGen->pIn = pStart;` |
|       8 |  8472 | `		return SXERR_INVALID;` |
|       - |  8473 | `	}` |
|   96479 |  8474 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   96479 |  8475 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   96479 |  8476 | `	pGen->pIn++;` |
|  144729 |  8477 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   48260 |  8478 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8479 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8480 | `		pGen->pIn++;` |
|      13 |  8481 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8482 | `		pGen->pIn++;` |
|       1 |  8483 | `	}` |
|   96479 |  8484 | `	if( isAbsolute ){` |
|      93 |  8485 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      49 |  8486 | `	}else{` |
|       - |  8487 | `		SyString sRaw;` |
|   96391 |  8488 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   96391 |  8489 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8490 | `	}` |
|   96479 |  8491 | `	SyBlobRelease(&sName);` |
|   96479 |  8492 | `	return SXRET_OK;` |
|   48245 |  8493 |  |
|       - |  8494 | `/*` |
|       - |  8495 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8496 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8497 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8498 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8499 | ` * either direction cannot run unbounded.` |
|       - |  8500 | ` */` |
|       - |  8501 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   10784 |  8502 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8503 |  |
|       - |  8504 | `	ph7_class **apParent;` |
|       - |  8505 | `	sxu32 n;` |
|   18067 |  8506 | `	while( pInterface ){` |
|   14369 |  8507 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8508 | `			return FALSE;` |
|       - |  8509 | `		}` |
|   17921 |  8510 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7104 |  8511 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7091 |  8512 | `			return TRUE;` |
|       - |  8513 | `		}` |
|    7283 |  8514 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7283 |  8515 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8516 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8517 | `				return TRUE;` |
|       - |  8518 | `			}` |
|     ! 0 |  8519 | `		}` |
|    7283 |  8520 | `		pInterface = pInterface->pBase;` |
|    7283 |  8521 | `		iDepth++;` |
|       5 |  8522 | `	}` |
|    3703 |  8523 | `	return FALSE;` |
|    5397 |  8524 |  |
|   10784 |  8525 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8526 |  |
|   10789 |  8527 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8528 |  |
|       - |  8529 | `/*` |
|       - |  8530 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8531 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8532 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8533 | ` */` |
|    7086 |  8534 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8535 |  |
|    7095 |  8536 | `	while( pBase ){` |
|      10 |  8537 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8538 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8539 | `			return TRUE;` |
|       - |  8540 | `		}` |
|      10 |  8541 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8542 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8543 | `			return TRUE;` |
|       - |  8544 | `		}` |
|       5 |  8545 | `		pBase = pBase->pBase;` |
|       1 |  8546 | `	}` |
|    7087 |  8547 | `	return FALSE;` |
|    3548 |  8548 |  |
|       - |  8549 | `/*` |
|       - |  8550 | ` * Compile a class declaration, named or anonymous.` |
|       - |  8551 | ` *` |
|       - |  8552 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  8553 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  8554 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  8555 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  8556 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  8557 | ` * implements, body, install) is shared by both paths.` |
|       - |  8558 | ` */` |
|  100284 |  8559 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  8560 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  8561 |  |
|  100289 |  8562 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8563 | `	ph7_class *pClass,*pBase;` |
|       - |  8564 | `	SyToken *pEnd,*pTmp;` |
|       - |  8565 | `	sxi32 iProtection;` |
|       - |  8566 | `	SySet aInterfaces;` |
|       - |  8567 | `	SySet aUseEntries;` |
|       - |  8568 | `	sxi32 iAttrflags;` |
|       - |  8569 | `	SyString *pName;` |
|       - |  8570 | `	sxi32 nKwrd;` |
|       - |  8571 | `	sxi32 rc;` |
|       - |  8572 | `	/* Jump the 'class' keyword */` |
|  100289 |  8573 | `	pGen->pIn++;` |
|  100289 |  8574 | `	if( pAnonName ){` |
|       - |  8575 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  8576 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  8577 | `		 * then use the synthesized name. */` |
|      29 |  8578 | `		*ppArgStart = *ppArgEnd = 0;` |
|      29 |  8579 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  8580 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  8581 | `			*ppArgStart = pGen->pIn;` |
|      10 |  8582 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  8583 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  8584 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  8585 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  8586 | `		}` |
|      29 |  8587 | `		pName = pAnonName;` |
|      29 |  8588 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      16 |  8589 | `	}else{` |
|  100263 |  8590 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8591 | `			/* Syntax error */` |
|     ! 0 |  8592 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8593 | `			if( rc == SXERR_ABORT ){` |
|       - |  8594 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8595 | `				return SXERR_ABORT;` |
|       - |  8596 | `			}` |
|       - |  8597 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8598 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8599 | `				pGen->pIn++;` |
|     ! 0 |  8600 | `			}` |
|     ! 0 |  8601 | `			return SXRET_OK;` |
|       - |  8602 | `		}` |
|       - |  8603 | `		/* Extract class name */` |
|  100263 |  8604 | `		pName = &pGen->pIn->sData;` |
|       - |  8605 | `		/* Advance the stream cursor */` |
|  100263 |  8606 | `		pGen->pIn++;` |
|       - |  8607 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  8608 | `			SyBlob sFQN;` |
|       - |  8609 | `			SyString sFQNStr;` |
|  100263 |  8610 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  100263 |  8611 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  100263 |  8612 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  100263 |  8613 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  100263 |  8614 | `			SyBlobRelease(&sFQN);` |
|       - |  8615 | `		}` |
|       - |  8616 | `	}` |
|  100289 |  8617 | `	if( pClass == 0 ){` |
|     ! 0 |  8618 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8619 | `		return SXERR_ABORT;` |
|       - |  8620 | `	}` |
|       - |  8621 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  100289 |  8622 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  100289 |  8623 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8624 | `	/* Assume a standalone class */` |
|  100289 |  8625 | `	pBase = 0;` |
|  100289 |  8626 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   85245 |  8627 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   85245 |  8628 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8629 | `			SyBlob sResolved;` |
|       - |  8630 | `			SyString sBaseName;` |
|       - |  8631 | `			sxu32 nRefLine;` |
|   74479 |  8632 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   74479 |  8633 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   74479 |  8634 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   74479 |  8635 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8636 | `				SyBlobRelease(&sResolved);` |
|       4 |  8637 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8638 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8639 | `					pName);` |
|       3 |  8640 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8641 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8642 | `					return SXERR_ABORT;` |
|       - |  8643 | `				}` |
|       3 |  8644 | `				return SXRET_OK;` |
|       - |  8645 | `			}` |
|  111713 |  8646 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   74472 |  8647 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   74477 |  8648 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8649 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8650 | `			/* Interfaces are not allowed */` |
|   74477 |  8651 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8652 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8653 | `			}` |
|   74477 |  8654 | `			if( pBase == 0 ){` |
|     ! 0 |  8655 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8656 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8657 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8658 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8659 | `					return SXERR_ABORT;` |
|       - |  8660 | `				}` |
|     ! 0 |  8661 | `			}else{` |
|   74477 |  8662 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8663 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8664 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8665 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8666 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8667 | `						return SXERR_ABORT;` |
|       - |  8668 | `					}` |
|     ! 0 |  8669 | `				}` |
|       - |  8670 | `			}` |
|   74477 |  8671 | `			SyBlobRelease(&sResolved);` |
|   37236 |  8672 | `		}` |
|   85243 |  8673 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8674 | `			ph7_class *pInterface;` |
|       - |  8675 | `			/* Interface implementation */` |
|   10779 |  8676 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5397 |  8677 | `			for(;;){` |
|       - |  8678 | `				SyBlob sResolved;` |
|       - |  8679 | `				SyString sIntName;` |
|       - |  8680 | `				sxu32 nRefLine;` |
|   10789 |  8681 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10789 |  8682 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10789 |  8683 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8684 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8685 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8686 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8687 | `						pName);` |
|     ! 0 |  8688 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8689 | `						return SXERR_ABORT;` |
|       - |  8690 | `					}` |
|     ! 0 |  8691 | `					break;` |
|       - |  8692 | `				}` |
|   21573 |  8693 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   10784 |  8694 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10789 |  8695 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8696 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8697 | `				/* Only interfaces are allowed */` |
|   10789 |  8698 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8699 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8700 | `				}` |
|   10789 |  8701 | `				if( pInterface == 0 ){` |
|     ! 0 |  8702 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8703 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8704 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8705 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8706 | `						return SXERR_ABORT;` |
|       - |  8707 | `					}` |
|     ! 0 |  8708 | `				}else{` |
|       - |  8709 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8710 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8711 | `					 * unless they already extend Exception or Error.` |
|       - |  8712 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8713 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8714 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   10789 |  8715 | `					SyString *pFqn = &pClass->sName;` |
|   10789 |  8716 | `					int bIsExceptionOrError =` |
|    8934 |  8717 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   17949 |  8718 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9022 |  8719 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3552 |  8720 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   14327 |  8721 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   10632 |  8722 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3541 |  8723 | `						!bIsExceptionOrError ){` |
|      12 |  8724 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8725 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8726 | `							&pClass->sName);` |
|       9 |  8727 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8728 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8729 | `							return SXERR_ABORT;` |
|       - |  8730 | `						}` |
|       - |  8731 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8732 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8733 | `					}else{` |
|   10783 |  8734 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8735 | `					}` |
|       - |  8736 | `				}` |
|   10789 |  8737 | `				SyBlobRelease(&sResolved);` |
|   10789 |  8738 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5392 |  8739 | `					break;` |
|       - |  8740 | `				}` |
|      13 |  8741 | `				pGen->pIn++;/* Jump the comma */` |
|       3 |  8742 | `			}` |
|    5387 |  8743 | `		}` |
|   42619 |  8744 | `	}` |
|  100287 |  8745 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8746 | `		/* Syntax error */` |
|     ! 0 |  8747 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8748 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8749 | `		if( rc == SXERR_ABORT ){` |
|       - |  8750 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8751 | `			return SXERR_ABORT;` |
|       - |  8752 | `		}` |
|     ! 0 |  8753 | `		return SXRET_OK;` |
|       - |  8754 | `	}` |
|  100287 |  8755 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  100287 |  8756 | `	pEnd = 0; /* cc warning */` |
|       - |  8757 | `	/* Delimit the class body */` |
|  100287 |  8758 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  100287 |  8759 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8760 | `		/* Syntax error */` |
|     ! 0 |  8761 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8762 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8763 | `		if( rc == SXERR_ABORT ){` |
|       - |  8764 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8765 | `			return SXERR_ABORT;` |
|       - |  8766 | `		}` |
|     ! 0 |  8767 | `		return SXRET_OK;` |
|       - |  8768 | `	}` |
|       - |  8769 | `	/* Swap token stream */` |
|  100287 |  8770 | `	pTmp = pGen->pEnd;` |
|  100287 |  8771 | `	pGen->pEnd = pEnd;` |
|       - |  8772 | `	/* Set the inherited flags */` |
|  100287 |  8773 | `	pClass->iFlags = iFlags;` |
|       - |  8774 | `	/* Start the parse process */` |
|  130274 |  8775 | `	for(;;){` |
|       - |  8776 | `		/* Jump leading/trailing semi-colons */` |
|  410567 |  8777 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   75045 |  8778 | `			pGen->pIn++;` |
|       5 |  8779 | `		}` |
|  335527 |  8780 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8781 | `			/* End of class body */` |
|  100259 |  8782 | `			break;` |
|       - |  8783 | `		}` |
|  235268 |  8784 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  117639 |  8785 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  8786 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8787 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8788 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8789 | `			if( rc == SXERR_ABORT ){` |
|       - |  8790 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8791 | `				return SXERR_ABORT;` |
|       - |  8792 | `			}` |
|     ! 0 |  8793 | `			goto done;` |
|       - |  8794 | `		}` |
|       - |  8795 | `		/* Assume public visibility */` |
|  235273 |  8796 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  235273 |  8797 | `		iAttrflags = 0;` |
|       - |  8798 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  8799 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  8800 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  8801 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  235273 |  8802 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8803 | `			int bMod = 0;` |
|     ! 0 |  8804 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8805 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  8806 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  8807 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  8808 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  8809 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  8810 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  8811 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  8812 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  8813 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  8814 | `			}` |
|     ! 0 |  8815 | `			if( !bMod ){` |
|     ! 0 |  8816 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8817 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8818 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8819 | `						return SXERR_ABORT;` |
|       - |  8820 | `					}` |
|     ! 0 |  8821 | `					goto done;` |
|       - |  8822 | `				}` |
|     ! 0 |  8823 | `				continue;` |
|       - |  8824 | `			}` |
|     ! 0 |  8825 | `		}` |
|  235273 |  8826 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8827 | `			/* Extract the current keyword */` |
|  235273 |  8828 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  235273 |  8829 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8830 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8831 | `				TraitUseEntry sUse;` |
|      53 |  8832 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      53 |  8833 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      53 |  8834 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      32 |  8835 | `				for(;;){` |
|       - |  8836 | `					ph7_class *pTrait;` |
|       - |  8837 | `					SyString *pTraitName;` |
|      61 |  8838 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8839 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8840 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8841 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8842 | `							return SXERR_ABORT;` |
|       - |  8843 | `						}` |
|     ! 0 |  8844 | `						break;` |
|       - |  8845 | `					}` |
|      61 |  8846 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8847 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8848 | `						SyBlob sResolved;` |
|      61 |  8849 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      61 |  8850 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     117 |  8851 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      56 |  8852 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      61 |  8853 | `						SyBlobRelease(&sResolved);` |
|       - |  8854 | `					}` |
|       - |  8855 | `					/* Only traits are allowed */` |
|      61 |  8856 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8857 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8858 | `					}` |
|      61 |  8859 | `					if( pTrait == 0 ){` |
|     ! 0 |  8860 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8861 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8862 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8863 | `							return SXERR_ABORT;` |
|       - |  8864 | `						}` |
|     ! 0 |  8865 | `					}else{` |
|      61 |  8866 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8867 | `					}` |
|      61 |  8868 | `					pGen->pIn++; /* Advance past trait name */` |
|      61 |  8869 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      29 |  8870 | `						break;` |
|       - |  8871 | `					}` |
|      10 |  8872 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  8873 | `				}` |
|       - |  8874 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      53 |  8875 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8876 | `					SyToken *pBlock;` |
|      13 |  8877 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  8878 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  8879 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  8880 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  8881 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  8882 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  8883 | `					}else{` |
|     ! 0 |  8884 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8885 | `					}` |
|       5 |  8886 | `				}` |
|      53 |  8887 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8888 | `				/* The semicolon will be consumed by the outer loop */` |
|      53 |  8889 | `				continue;` |
|       - |  8890 | `			}` |
|  235225 |  8891 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  234963 |  8892 | `				iProtection = nKwrd;` |
|  234963 |  8893 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  8894 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  234963 |  8895 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  8896 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  8897 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  8898 | `				}` |
|  234958 |  8899 | `				if( pGen->pIn >= pGen->pEnd` |
|  234963 |  8900 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  8901 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8902 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8903 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8904 | `					if( rc == SXERR_ABORT ){` |
|       - |  8905 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8906 | `						return SXERR_ABORT;` |
|       - |  8907 | `					}` |
|     ! 0 |  8908 | `					goto done;` |
|       - |  8909 | `				}` |
|  234963 |  8910 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8911 | `					/* Attribute declaration (untyped) */` |
|   74749 |  8912 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   74749 |  8913 | `					if( rc != SXRET_OK ){` |
|       9 |  8914 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8915 | `							return SXERR_ABORT;` |
|       - |  8916 | `						}` |
|       9 |  8917 | `						goto done;` |
|       - |  8918 | `					}` |
|   74743 |  8919 | `					continue;` |
|       - |  8920 | `				}` |
|  160219 |  8921 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8922 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     173 |  8923 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     173 |  8924 | `					if( rc != SXRET_OK ){` |
|       8 |  8925 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8926 | `							return SXERR_ABORT;` |
|       - |  8927 | `						}` |
|       8 |  8928 | `						goto done;` |
|       - |  8929 | `					}` |
|     167 |  8930 | `					continue;` |
|       - |  8931 | `				}` |
|       - |  8932 | `				/* Extract the keyword */` |
|  160051 |  8933 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   80023 |  8934 | `			}` |
|  160313 |  8935 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8936 | `				/* Process constant declaration */` |
|      67 |  8937 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      67 |  8938 | `				if( rc != SXRET_OK ){` |
|       3 |  8939 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8940 | `						return SXERR_ABORT;` |
|       - |  8941 | `					}` |
|       3 |  8942 | `					goto done;` |
|       - |  8943 | `				}` |
|      35 |  8944 | `			}else{` |
|  160251 |  8945 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8946 | `					/* Static method or attribute,record that */` |
|    3597 |  8947 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    3597 |  8948 | `					pGen->pIn++; /* Jump the static keyword */` |
|    3597 |  8949 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8950 | `						/* Extract the keyword */` |
|    3589 |  8951 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    3589 |  8952 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8953 | `							iProtection = nKwrd;` |
|     ! 0 |  8954 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8955 | `						}` |
|    1792 |  8956 | `					}` |
|       - |  8957 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  8958 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  8959 | `					 * than a generic "expecting method" parse error. */` |
|    3597 |  8960 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8961 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8962 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  8963 | `					}` |
|    3592 |  8964 | `					if( pGen->pIn >= pGen->pEnd` |
|    3597 |  8965 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  8966 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8967 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8968 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8969 | `						if( rc == SXERR_ABORT ){` |
|       - |  8970 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8971 | `							return SXERR_ABORT;` |
|       - |  8972 | `						}` |
|     ! 0 |  8973 | `						goto done;` |
|       - |  8974 | `					}` |
|    3597 |  8975 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8976 | `						/* Attribute declaration */` |
|       8 |  8977 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       8 |  8978 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8979 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8980 | `								return SXERR_ABORT;` |
|       - |  8981 | `							}` |
|     ! 0 |  8982 | `							goto done;` |
|       - |  8983 | `						}` |
|       8 |  8984 | `						continue;` |
|       - |  8985 | `					}` |
|    3591 |  8986 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8987 | `						/* Typed static attribute declaration */` |
|      15 |  8988 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  8989 | `						if( rc != SXRET_OK ){` |
|       3 |  8990 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8991 | `								return SXERR_ABORT;` |
|       - |  8992 | `							}` |
|       3 |  8993 | `							goto done;` |
|       - |  8994 | `						}` |
|      13 |  8995 | `						continue;` |
|       - |  8996 | `					}` |
|       - |  8997 | `					/* Extract the keyword */` |
|    3579 |  8998 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  158446 |  8999 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  9000 | `					/* Abstract method,record that */` |
|      12 |  9001 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9002 | `					/* Mark the whole class as abstract */` |
|      12 |  9003 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9004 | `					/* Advance the stream cursor */` |
|      12 |  9005 | `					pGen->pIn++;` |
|      12 |  9006 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  9007 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  9008 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  9009 | `							iProtection = nKwrd;` |
|      10 |  9010 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  9011 | `						}` |
|       5 |  9012 | `					}` |
|      12 |  9013 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  9014 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9015 | `							/* Static method */` |
|     ! 0 |  9016 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9017 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9018 | `					}` |
|      12 |  9019 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  9020 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9021 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9022 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9023 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9024 | `							if( rc == SXERR_ABORT ){` |
|       - |  9025 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9026 | `								return SXERR_ABORT;` |
|       - |  9027 | `							}` |
|     ! 0 |  9028 | `							goto done;` |
|       - |  9029 | `					}` |
|      12 |  9030 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  156654 |  9031 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9032 | `					/* final method ,record that */` |
|      17 |  9033 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  9034 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  9035 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9036 | `						/* Extract the keyword */` |
|      17 |  9037 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  9038 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 |  9039 | `							iProtection = nKwrd;` |
|       8 |  9040 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9041 | `						}` |
|       7 |  9042 | `					}` |
|      17 |  9043 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9044 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9045 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9046 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9047 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9048 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9049 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9050 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9051 | `									return SXERR_ABORT;` |
|       - |  9052 | `								}` |
|     ! 0 |  9053 | `								goto done;` |
|       - |  9054 | `							}` |
|      12 |  9055 | `							continue;` |
|       - |  9056 | `					}` |
|       5 |  9057 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9058 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9059 | `							/* Static method */` |
|     ! 0 |  9060 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9061 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9062 | `					}` |
|       5 |  9063 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9064 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9065 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9066 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9067 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9068 | `							if( rc == SXERR_ABORT ){` |
|       - |  9069 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9070 | `								return SXERR_ABORT;` |
|       - |  9071 | `							}` |
|     ! 0 |  9072 | `							goto done;` |
|       - |  9073 | `					}` |
|       5 |  9074 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9075 | `				}` |
|  160223 |  9076 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9077 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9078 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9079 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9080 | `						if( rc == SXERR_ABORT ){` |
|       - |  9081 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9082 | `							return SXERR_ABORT;` |
|       - |  9083 | `						}` |
|     ! 0 |  9084 | `						goto done;` |
|       - |  9085 | `				}` |
|  160223 |  9086 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9087 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9088 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9089 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9090 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9091 | `						if( rc == SXERR_ABORT ){` |
|       - |  9092 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9093 | `							return SXERR_ABORT;` |
|       - |  9094 | `						}` |
|     ! 0 |  9095 | `						goto done;` |
|       - |  9096 | `					}` |
|       - |  9097 | `					/* Attribute declaration */` |
|       7 |  9098 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9099 | `				}else{` |
|       - |  9100 | `					/* Process method declaration */` |
|  160217 |  9101 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9102 | `				}` |
|  160223 |  9103 | `				if( rc != SXRET_OK ){` |
|      16 |  9104 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9105 | `						return SXERR_ABORT;` |
|       - |  9106 | `					}` |
|      16 |  9107 | `					goto done;` |
|       - |  9108 | `				}` |
|       - |  9109 | `			}` |
|   80138 |  9110 | `		}else{` |
|       - |  9111 | `			/* Attribute declaration */` |
|     ! 0 |  9112 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9113 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9114 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9115 | `					return SXERR_ABORT;` |
|       - |  9116 | `				}` |
|     ! 0 |  9117 | `				goto done;` |
|       - |  9118 | `			}` |
|       - |  9119 | `		}` |
|       5 |  9120 | `	}` |
|       - |  9121 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9122 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9123 | `	 */` |
|       - |  9124 | `	{` |
|       - |  9125 | `		TraitUseEntry *apUse;` |
|       - |  9126 | `		sxu32 nU;` |
|  100259 |  9127 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  100307 |  9128 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      53 |  9129 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      53 |  9130 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      53 |  9131 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      53 |  9132 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9133 | `			sxu32 nT;` |
|      53 |  9134 | `			if( !hasResolution ){` |
|       - |  9135 | `				/* No conflict resolution block: use standard trait application */` |
|      87 |  9136 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      49 |  9137 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      49 |  9138 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9139 | `						break;` |
|       - |  9140 | `					}` |
|      27 |  9141 | `				}` |
|      24 |  9142 | `			}else{` |
|       - |  9143 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9144 | `				 * then use the block to resolve method conflicts.` |
|       - |  9145 | `				 */` |
|       - |  9146 | `				SyToken *pR;` |
|      25 |  9147 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9148 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9149 | `					ph7_class_attr *pAR;` |
|       - |  9150 | `					SyHashEntry *pER;` |
|       - |  9151 | `					SyString *pNR;` |
|      15 |  9152 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9153 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9154 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9155 | `						pNR = &pAR->sName;` |
|     ! 0 |  9156 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9157 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9158 | `						}` |
|     ! 0 |  9159 | `					}` |
|      15 |  9160 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9161 | `				}` |
|       - |  9162 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9163 | `				pR = pUse->pResolvStart;` |
|      27 |  9164 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9165 | `					SyString sTrait,sMethod;` |
|       - |  9166 | `					ph7_class *pSrcTrait;` |
|       - |  9167 | `					ph7_class_method *pMeth;` |
|       - |  9168 | `					sxi32 nRKwrd;` |
|      41 |  9169 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9170 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9171 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9172 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9173 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9174 | `					sMethod = pR->sData;` |
|      17 |  9175 | `					pR++;` |
|      17 |  9176 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9177 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9178 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9179 | `							sTrait = sMethod;` |
|       7 |  9180 | `							pR++;` |
|       7 |  9181 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9182 | `							sMethod = pR->sData;` |
|       7 |  9183 | `							pR++;` |
|       3 |  9184 | `						}` |
|       3 |  9185 | `					}` |
|      17 |  9186 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9187 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9188 | `						continue;` |
|       - |  9189 | `					}` |
|      17 |  9190 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9191 | `					pR++;` |
|      17 |  9192 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9193 | `						pSrcTrait = 0;` |
|       7 |  9194 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9195 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9196 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9197 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9198 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9199 | `								break;` |
|       - |  9200 | `							}` |
|       2 |  9201 | `						}` |
|       5 |  9202 | `						if( pSrcTrait ){` |
|       5 |  9203 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9204 | `							if( pMeth ){` |
|       5 |  9205 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9206 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9207 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9208 | `								}` |
|       2 |  9209 | `							}` |
|       2 |  9210 | `						}` |
|       2 |  9211 | `					}` |
|      35 |  9212 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9213 | `				}` |
|       - |  9214 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9215 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9216 | `					ph7_class_method *pMR;` |
|       - |  9217 | `					SyHashEntry *pER;` |
|       - |  9218 | `					SyString *pNR;` |
|      15 |  9219 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9220 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9221 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9222 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9223 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9224 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9225 | `						}` |
|       3 |  9226 | `					}` |
|       9 |  9227 | `				}` |
|       - |  9228 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9229 | `				pR = pUse->pResolvStart;` |
|      27 |  9230 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9231 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9232 | `					ph7_class *pSrcTrait;` |
|       - |  9233 | `					ph7_class_method *pMeth;` |
|      27 |  9234 | `					int hasQual = 0;` |
|       - |  9235 | `					sxi32 nRKwrd;` |
|      41 |  9236 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9237 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9238 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9239 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9240 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9241 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9242 | `					sMethod = pR->sData;` |
|      17 |  9243 | `					pR++;` |
|      17 |  9244 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9245 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9246 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9247 | `							sTrait = sMethod;` |
|       7 |  9248 | `							hasQual = 1;` |
|       7 |  9249 | `							pR++;` |
|       7 |  9250 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9251 | `							sMethod = pR->sData;` |
|       7 |  9252 | `							pR++;` |
|       3 |  9253 | `						}` |
|       3 |  9254 | `					}` |
|      17 |  9255 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9256 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9257 | `						continue;` |
|       - |  9258 | `					}` |
|      17 |  9259 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9260 | `					pR++;` |
|      17 |  9261 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9262 | `						sxi32 iNewVis = -1;` |
|      13 |  9263 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9264 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9265 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9266 | `								iNewVis = nAK;` |
|       7 |  9267 | `								pR++;` |
|       3 |  9268 | `							}` |
|       3 |  9269 | `						}` |
|      13 |  9270 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9271 | `							sAlias = pR->sData;` |
|      11 |  9272 | `							pR++;` |
|       4 |  9273 | `						}` |
|      13 |  9274 | `						pMeth = 0;` |
|      13 |  9275 | `						if( hasQual ){` |
|       3 |  9276 | `							pSrcTrait = 0;` |
|       5 |  9277 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9278 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9279 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9280 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9281 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9282 | `									break;` |
|       - |  9283 | `								}` |
|       2 |  9284 | `							}` |
|       3 |  9285 | `							if( pSrcTrait ){` |
|       3 |  9286 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9287 | `							}` |
|       2 |  9288 | `						}else{` |
|      10 |  9289 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9290 | `						}` |
|      13 |  9291 | `						if( pMeth ){` |
|      13 |  9292 | `							if( sAlias.nByte > 0 ){` |
|       - |  9293 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9294 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9295 | `								 */` |
|       - |  9296 | `								ph7_class_method *pAlias;` |
|       - |  9297 | `								char *zAliasDup;` |
|      11 |  9298 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9299 | `								if( pAlias ){` |
|      11 |  9300 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9301 | `									if( iNewVis >= 0 ){` |
|       5 |  9302 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9303 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9304 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9305 | `									}` |
|      11 |  9306 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9307 | `									if( zAliasDup ){` |
|      11 |  9308 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9309 | `									}` |
|       7 |  9310 | `								}` |
|       7 |  9311 | `							}else if( iNewVis >= 0 ){` |
|       - |  9312 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9313 | `								ph7_class_method *pCopy;` |
|       3 |  9314 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9315 | `								if( pCopy ){` |
|       3 |  9316 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9317 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9318 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9319 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9320 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9321 | `									/* Replace the method in the class hash */` |
|       3 |  9322 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9323 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9324 | `								}` |
|       1 |  9325 | `							}` |
|       5 |  9326 | `						}` |
|       5 |  9327 | `						SXUNUSED(hasQual);` |
|       5 |  9328 | `					}` |
|      21 |  9329 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9330 | `				}` |
|       - |  9331 | `			}` |
|      53 |  9332 | `			SySetRelease(&pUse->aTraits);` |
|      29 |  9333 | `		}` |
|       - |  9334 | `	}` |
|       - |  9335 | `	/* Install the class */` |
|  100259 |  9336 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  100259 |  9337 | `	if( rc == SXRET_OK ){` |
|       - |  9338 | `		ph7_class **apInterface;` |
|       - |  9339 | `		sxu32 n;` |
|  100259 |  9340 | `		if( pBase ){` |
|       - |  9341 | `			/* Inherit from base class and mark as a subclass */` |
|   74477 |  9342 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   37236 |  9343 | `		}` |
|  100259 |  9344 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  111037 |  9345 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9346 | `			/* Implements one or more interface */` |
|   10783 |  9347 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   10783 |  9348 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9349 | `				break;` |
|       - |  9350 | `			}` |
|    5394 |  9351 | `		}` |
|       - |  9352 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9353 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  100254 |  9354 | `		if( rc == SXRET_OK` |
|  100254 |  9355 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  100259 |  9356 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   81465 |  9357 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9358 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   81465 |  9359 | `			if( pStringable ){` |
|   81465 |  9360 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   81465 |  9361 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9362 | `				sxu32 i;` |
|   81465 |  9363 | `				int bAlready = 0;` |
|   88545 |  9364 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7087 |  9365 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9366 | `						bAlready = 1;` |
|       3 |  9367 | `						break;` |
|       - |  9368 | `					}` |
|    3545 |  9369 | `				}` |
|   81465 |  9370 | `				if( !bAlready ){` |
|   81463 |  9371 | `					PH7_ClassImplement(pClass,pStringable);` |
|   40729 |  9372 | `				}` |
|   40730 |  9373 | `			}` |
|   40730 |  9374 | `		}` |
|       - |  9375 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  100259 |  9376 | `		if( rc == SXRET_OK ){` |
|  100259 |  9377 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  100259 |  9378 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9379 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9380 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9381 | `				return SXERR_ABORT;` |
|       - |  9382 | `			}` |
|   50127 |  9383 | `		}` |
|       - |  9384 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  100259 |  9385 | `		if( rc == SXRET_OK ){` |
|  100259 |  9386 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  100259 |  9387 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9388 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9389 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9390 | `				return SXERR_ABORT;` |
|       - |  9391 | `			}` |
|   50127 |  9392 | `		}` |
|   50127 |  9393 | `	}` |
|  100259 |  9394 | `	SySetRelease(&aUseEntries);` |
|  100259 |  9395 | `	SySetRelease(&aInterfaces);` |
|  100259 |  9396 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9397 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9398 | `		return SXERR_ABORT;` |
|       - |  9399 | `	}` |
|   50127 |  9400 | `done:` |
|       - |  9401 | `	/* Point beyond the class body */` |
|  100287 |  9402 | `	pGen->pIn = &pEnd[1];` |
|  100287 |  9403 | `	pGen->pEnd = pTmp;` |
|  100287 |  9404 | `	return PH7_OK;` |
|   50147 |  9405 |  |
|       - |  9406 | `/* Compile a named class declaration (the common case). */` |
|  100258 |  9407 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9408 |  |
|  100263 |  9409 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9410 |  |
|       - |  9411 | `/*` |
|       - |  9412 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9413 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9414 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9415 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9416 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9417 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9418 | ` */` |
|      26 |  9419 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       3 |  9420 |  |
|       - |  9421 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9422 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9423 | `	SyString sName;` |
|       - |  9424 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9425 | `	ph7_value *pObj;` |
|      29 |  9426 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9427 | `	sxu32 nIdx,nLen;` |
|       - |  9428 | `	sxi32 nArg,rc;` |
|      13 |  9429 | `	SXUNUSED(iCompileFlag);` |
|       - |  9430 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      29 |  9431 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      29 |  9432 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9433 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9434 | `	}` |
|      29 |  9435 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9436 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9437 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9438 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      29 |  9439 | `	pArgStart = pArgEnd = 0;` |
|      29 |  9440 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      29 |  9441 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9442 | `		return rc;` |
|       - |  9443 | `	}` |
|       - |  9444 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9445 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      29 |  9446 | `	nArg = 0;` |
|      29 |  9447 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9448 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9449 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9450 | `		SyToken *pArgNext;` |
|       7 |  9451 | `		pGen->pIn = pArgStart;` |
|       7 |  9452 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9453 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9454 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9455 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 |  9456 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9457 | `					pGen->pIn = pSavedIn;` |
|     ! 0 |  9458 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 |  9459 | `					return SXERR_ABORT;` |
|       - |  9460 | `				}` |
|       7 |  9461 | `				nArg++;` |
|       3 |  9462 | `			}` |
|       7 |  9463 | `			pGen->pIn = &pArgNext[1];` |
|       1 |  9464 | `		}` |
|       7 |  9465 | `		pGen->pIn = pSavedIn;` |
|       7 |  9466 | `		pGen->pEnd = pSavedEnd;` |
|       3 |  9467 | `	}` |
|       - |  9468 | `	/* Load the synthesized class name */` |
|      29 |  9469 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  9470 | `	if( pObj == 0 ){` |
|     ! 0 |  9471 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9472 | `		return SXERR_ABORT;` |
|       - |  9473 | `	}` |
|      29 |  9474 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      29 |  9475 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  9476 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      29 |  9477 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      29 |  9478 | `	return SXRET_OK;` |
|      16 |  9479 |  |
|       - |  9480 | `/*` |
|       - |  9481 | ` * Compile a user-defined abstract class.` |
|       - |  9482 | ` *  According to the PHP language reference manual` |
|       - |  9483 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9484 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9485 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9486 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9487 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9488 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9489 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9490 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9491 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9492 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9493 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9494 | ` *   could differ.` |
|       - |  9495 | ` */` |
|       - |  9496 | `/*` |
|       - |  9497 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9498 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9499 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9500 | ` */` |
|  953492 |  9501 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9502 |  |
|  953497 |  9503 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  635721 |  9504 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  635721 |  9505 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  628627 |  9506 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  314284 |  9507 | `	}` |
|  946349 |  9508 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  946289 |  9509 | `	return FALSE;` |
|  476751 |  9510 |  |
|       - |  9511 | `/*` |
|       - |  9512 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9513 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9514 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9515 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9516 | ` */` |
|  946284 |  9517 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9518 |  |
|  946289 |  9519 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  946289 |  9520 | `	sxi32 iFlags = 0,iFlag;` |
|  953497 |  9521 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7213 |  9522 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9523 | `			pDup = pIn;` |
|       2 |  9524 | `		}` |
|    7213 |  9525 | `		iFlags \|= iFlag;` |
|    7213 |  9526 | `		pIn++;` |
|       5 |  9527 | `	}` |
|  946289 |  9528 | `	*ppIn = pIn;` |
|  946289 |  9529 | `	if( ppDup ){ *ppDup = pDup; }` |
|  946289 |  9530 | `	return iFlags;` |
|       5 |  9531 |  |
|       - |  9532 | `/*` |
|       - |  9533 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9534 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9535 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9536 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9537 | `` * `readonly`) to their existing handlers.`` |
|       - |  9538 | ` */` |
|  942690 |  9539 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9540 |  |
|  942695 |  9541 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  474946 |  9542 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  944489 |  9543 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9544 |  |
|       - |  9545 | `/*` |
|       - |  9546 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9547 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9548 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9549 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9550 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9551 | ` */` |
|    3594 |  9552 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9553 |  |
|       - |  9554 | `	SyToken *pDup;` |
|    3599 |  9555 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9556 | `	sxi32 rc;` |
|    3599 |  9557 | `	if( pDup ){` |
|       4 |  9558 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9559 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9560 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9561 | `			return SXERR_ABORT;` |
|       - |  9562 | `		}` |
|       1 |  9563 | `	}` |
|    3594 |  9564 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1802 |  9565 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9566 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9567 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9568 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9569 | `			return SXERR_ABORT;` |
|       - |  9570 | `		}` |
|       1 |  9571 | `	}` |
|    3599 |  9572 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1802 |  9573 |  |
|       - |  9574 | `/*` |
|       - |  9575 | ` * Compile a user-defined trait.` |
|       - |  9576 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9577 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9578 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9579 | ` */` |
|      60 |  9580 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9581 |  |
|      65 |  9582 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9583 | `	ph7_class *pClass;` |
|       - |  9584 | `	SyToken *pEnd,*pTmp;` |
|       - |  9585 | `	sxi32 iProtection;` |
|       - |  9586 | `	sxi32 iAttrflags;` |
|       - |  9587 | `	SyString *pName;` |
|       - |  9588 | `	sxi32 nKwrd;` |
|       - |  9589 | `	sxi32 rc;` |
|       - |  9590 | `	/* Jump the 'trait' keyword */` |
|      65 |  9591 | `	pGen->pIn++;` |
|      65 |  9592 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9593 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9594 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9595 | `			return SXERR_ABORT;` |
|       - |  9596 | `		}` |
|     ! 0 |  9597 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9598 | `			pGen->pIn++;` |
|     ! 0 |  9599 | `		}` |
|     ! 0 |  9600 | `		return SXRET_OK;` |
|       - |  9601 | `	}` |
|       - |  9602 | `	/* Extract trait name */` |
|      65 |  9603 | `	pName = &pGen->pIn->sData;` |
|      65 |  9604 | `	pGen->pIn++;` |
|       - |  9605 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9606 | `		SyBlob sFQN;` |
|       - |  9607 | `		SyString sFQNStr;` |
|      65 |  9608 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      65 |  9609 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      65 |  9610 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      65 |  9611 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      65 |  9612 | `		SyBlobRelease(&sFQN);` |
|       - |  9613 | `	}` |
|      65 |  9614 | `	if( pClass == 0 ){` |
|     ! 0 |  9615 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9616 | `		return SXERR_ABORT;` |
|       - |  9617 | `	}` |
|       - |  9618 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      65 |  9619 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9620 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9621 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9622 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9623 | `			return SXERR_ABORT;` |
|       - |  9624 | `		}` |
|     ! 0 |  9625 | `		return SXRET_OK;` |
|       - |  9626 | `	}` |
|      65 |  9627 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      65 |  9628 | `	pEnd = 0;` |
|      65 |  9629 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      65 |  9630 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  9631 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  9632 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9633 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9634 | `			return SXERR_ABORT;` |
|       - |  9635 | `		}` |
|     ! 0 |  9636 | `		return SXRET_OK;` |
|       - |  9637 | `	}` |
|       - |  9638 | `	/* Swap token stream */` |
|      65 |  9639 | `	pTmp = pGen->pEnd;` |
|      65 |  9640 | `	pGen->pEnd = pEnd;` |
|       - |  9641 | `	/* Mark as trait */` |
|      65 |  9642 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9643 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      60 |  9644 | `	for(;;){` |
|     169 |  9645 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9646 | `			pGen->pIn++;` |
|       4 |  9647 | `		}` |
|     145 |  9648 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      65 |  9649 | `			break;` |
|       - |  9650 | `		}` |
|      85 |  9651 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9652 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9653 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9654 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9655 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9656 | `				return SXERR_ABORT;` |
|       - |  9657 | `			}` |
|     ! 0 |  9658 | `			goto done;` |
|       - |  9659 | `		}` |
|      85 |  9660 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      85 |  9661 | `		iAttrflags = 0;` |
|      85 |  9662 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      85 |  9663 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  9664 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9665 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9666 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9667 | `				for(;;){` |
|       - |  9668 | `					ph7_class *pUsedTrait;` |
|       - |  9669 | `					SyString *pUsedName;` |
|       5 |  9670 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9671 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9672 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9673 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9674 | `							return SXERR_ABORT;` |
|       - |  9675 | `						}` |
|     ! 0 |  9676 | `						break;` |
|       - |  9677 | `					}` |
|       5 |  9678 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9679 | `					{` |
|       - |  9680 | `						SyBlob sResolved;` |
|       5 |  9681 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9682 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9683 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9684 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9685 | `						SyBlobRelease(&sResolved);` |
|       - |  9686 | `					}` |
|       5 |  9687 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9688 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9689 | `					}` |
|       5 |  9690 | `					if( pUsedTrait == 0 ){` |
|       4 |  9691 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9692 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9693 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9694 | `							return SXERR_ABORT;` |
|       - |  9695 | `						}` |
|       2 |  9696 | `					}else{` |
|       3 |  9697 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9698 | `					}` |
|       5 |  9699 | `					pGen->pIn++;` |
|       5 |  9700 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  9701 | `						break;` |
|       - |  9702 | `					}` |
|     ! 0 |  9703 | `					pGen->pIn++;` |
|     ! 0 |  9704 | `				}` |
|       5 |  9705 | `				continue;` |
|       - |  9706 | `			}` |
|      81 |  9707 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  9708 | `				iProtection = nKwrd;` |
|      73 |  9709 | `				pGen->pIn++;` |
|      68 |  9710 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  9711 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9712 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9713 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9714 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9715 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9716 | `						return SXERR_ABORT;` |
|       - |  9717 | `					}` |
|     ! 0 |  9718 | `					goto done;` |
|       - |  9719 | `				}` |
|      73 |  9720 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  9721 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9722 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9723 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9724 | `							return SXERR_ABORT;` |
|       - |  9725 | `						}` |
|     ! 0 |  9726 | `						goto done;` |
|       - |  9727 | `					}` |
|      12 |  9728 | `					continue;` |
|       - |  9729 | `				}` |
|      63 |  9730 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  9731 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  9732 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9733 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9734 | `							return SXERR_ABORT;` |
|       - |  9735 | `						}` |
|     ! 0 |  9736 | `						goto done;` |
|       - |  9737 | `					}` |
|       5 |  9738 | `					continue;` |
|       - |  9739 | `				}` |
|      58 |  9740 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  9741 | `			}` |
|      66 |  9742 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  9743 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9744 | `					"Traits cannot have constants");` |
|     ! 0 |  9745 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9746 | `					return SXERR_ABORT;` |
|       - |  9747 | `				}` |
|     ! 0 |  9748 | `				goto done;` |
|     ! 0 |  9749 | `			}else{` |
|      66 |  9750 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9751 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9752 | `					pGen->pIn++;` |
|       5 |  9753 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9754 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9755 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9756 | `							iProtection = nKwrd;` |
|     ! 0 |  9757 | `							pGen->pIn++;` |
|     ! 0 |  9758 | `						}` |
|       1 |  9759 | `					}` |
|       4 |  9760 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9761 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9762 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9763 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9764 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9765 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9766 | `							return SXERR_ABORT;` |
|       - |  9767 | `						}` |
|     ! 0 |  9768 | `						goto done;` |
|       - |  9769 | `					}` |
|       5 |  9770 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9771 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9772 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9773 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9774 | `								return SXERR_ABORT;` |
|       - |  9775 | `							}` |
|     ! 0 |  9776 | `							goto done;` |
|       - |  9777 | `						}` |
|       3 |  9778 | `						continue;` |
|       - |  9779 | `					}` |
|       3 |  9780 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9781 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9782 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9783 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9784 | `								return SXERR_ABORT;` |
|       - |  9785 | `							}` |
|     ! 0 |  9786 | `							goto done;` |
|       - |  9787 | `						}` |
|     ! 0 |  9788 | `						continue;` |
|       - |  9789 | `					}` |
|       3 |  9790 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      63 |  9791 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 |  9792 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 |  9793 | `					pGen->pIn++;` |
|       6 |  9794 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 |  9795 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  9796 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  9797 | `							iProtection = nKwrd;` |
|       6 |  9798 | `							pGen->pIn++;` |
|       2 |  9799 | `						}` |
|       2 |  9800 | `					}` |
|       6 |  9801 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9802 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9803 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9804 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9805 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9806 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9807 | `							return SXERR_ABORT;` |
|       - |  9808 | `						}` |
|     ! 0 |  9809 | `						goto done;` |
|       - |  9810 | `					}` |
|       6 |  9811 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9812 | `				}` |
|      64 |  9813 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9814 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9815 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9816 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9817 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9818 | `						return SXERR_ABORT;` |
|       - |  9819 | `					}` |
|     ! 0 |  9820 | `					goto done;` |
|       - |  9821 | `				}` |
|      64 |  9822 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9823 | `					pGen->pIn++;` |
|     ! 0 |  9824 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9825 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9826 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9827 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9828 | `							return SXERR_ABORT;` |
|       - |  9829 | `						}` |
|     ! 0 |  9830 | `						goto done;` |
|       - |  9831 | `					}` |
|     ! 0 |  9832 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9833 | `				}else{` |
|      64 |  9834 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9835 | `				}` |
|      64 |  9836 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9837 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9838 | `						return SXERR_ABORT;` |
|       - |  9839 | `					}` |
|     ! 0 |  9840 | `					goto done;` |
|       - |  9841 | `				}` |
|       - |  9842 | `			}` |
|      34 |  9843 | `		}else{` |
|     ! 0 |  9844 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9845 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9846 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9847 | `					return SXERR_ABORT;` |
|       - |  9848 | `				}` |
|     ! 0 |  9849 | `				goto done;` |
|       - |  9850 | `			}` |
|       - |  9851 | `		}` |
|       4 |  9852 | `	}` |
|       - |  9853 | `	/* Install the trait */` |
|      65 |  9854 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      65 |  9855 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9856 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9857 | `		return SXERR_ABORT;` |
|       - |  9858 | `	}` |
|      30 |  9859 | `done:` |
|       - |  9860 | `	/* Point beyond the trait body */` |
|      65 |  9861 | `	pGen->pIn = &pEnd[1];` |
|      65 |  9862 | `	pGen->pEnd = pTmp;` |
|      65 |  9863 | `	return PH7_OK;` |
|      35 |  9864 |  |
|       - |  9865 | `/*` |
|       - |  9866 | ` * Compile a user-defined class.` |
|       - |  9867 | ` *  According to the PHP language reference manual` |
|       - |  9868 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9869 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9870 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9871 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9872 | ` *   and functions (called "methods").` |
|       - |  9873 | ` */` |
|   96664 |  9874 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 |  9875 |  |
|       - |  9876 | `	sxi32 rc;` |
|   96669 |  9877 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   96669 |  9878 | `	return rc;` |
|       5 |  9879 |  |
|       - |  9880 | `/*` |
|       - |  9881 | ` * Exception handling.` |
|       - |  9882 | ` *  According to the PHP language reference manual` |
|       - |  9883 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9884 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9885 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9886 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9887 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9888 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9889 | ` *    (or re-thrown) within a catch block.` |
|       - |  9890 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9891 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9892 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9893 | ` *    been defined with set_exception_handler().` |
|       - |  9894 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9895 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9896 | ` */` |
|       - |  9897 | `/*` |
|       - |  9898 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9899 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9900 | ` * indicates failure.` |
|       - |  9901 | ` */` |
|   14454 |  9902 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  9903 |  |
|   14459 |  9904 | `	sxi32 rc = SXRET_OK;` |
|   14459 |  9905 | `	if( pRoot->pOp ){` |
|   14449 |  9906 | `		switch( pRoot->pOp->iOp ){` |
|    7222 |  9907 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9908 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9909 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9910 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9911 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9912 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   14449 |  9913 | `			break;` |
|     ! 0 |  9914 | `		default:` |
|       - |  9915 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9916 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9917 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9918 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9919 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9920 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9921 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9922 | `			}` |
|     ! 0 |  9923 | `			break;` |
|       - |  9924 | `		}` |
|    7237 |  9925 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9926 | `		/* Unexpected expression */` |
|     ! 0 |  9927 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9928 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9929 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9930 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9931 | `		}` |
|     ! 0 |  9932 | `	}` |
|   14459 |  9933 | `	return rc;` |
|       5 |  9934 |  |
|       - |  9935 | `/*` |
|       - |  9936 | ` * Compile a 'throw' statement.` |
|       - |  9937 | ` * throw: This is how you trigger an exception.` |
|       - |  9938 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9939 | ` */` |
|   14418 |  9940 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 |  9941 |  |
|   14423 |  9942 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9943 | `	GenBlock *pBlock;` |
|       - |  9944 | `	sxu32 nIdx;` |
|       - |  9945 | `	sxi32 rc;` |
|   14423 |  9946 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9947 | `	/* Compile the expression */` |
|   14423 |  9948 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   14423 |  9949 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9950 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9951 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9952 | `			return SXERR_ABORT;` |
|       - |  9953 | `		}` |
|     ! 0 |  9954 | `		return SXRET_OK;` |
|       - |  9955 | `	}` |
|   14423 |  9956 | `	pBlock = pGen->pCurrent;` |
|       - |  9957 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   57145 |  9958 | `	while(pBlock->pParent){` |
|   57141 |  9959 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   14419 |  9960 | `			break;` |
|       - |  9961 | `		}` |
|       - |  9962 | `		/* Point to the parent block */` |
|   42727 |  9963 | `		pBlock = pBlock->pParent;` |
|       5 |  9964 | `	}` |
|       - |  9965 | `	/* Emit the throw instruction */` |
|   14423 |  9966 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9967 | `	/* Emit the jump */` |
|   14423 |  9968 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   14423 |  9969 | `	return SXRET_OK;` |
|    7214 |  9970 |  |
|       - |  9971 | `/*` |
|       - |  9972 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9973 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9974 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9975 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9976 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9977 | ` */` |
|      36 |  9978 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9979 |  |
|      38 |  9980 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9981 | `	GenBlock *pBlock;` |
|       - |  9982 | `	sxu32 nIdx;` |
|       - |  9983 | `	sxi32 rc;` |
|      18 |  9984 | `	(void)iCompileFlag;` |
|      38 |  9985 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9986 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9987 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9988 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9989 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9990 | `			return SXERR_ABORT;` |
|       - |  9991 | `		}` |
|     ! 0 |  9992 | `		return SXRET_OK;` |
|       - |  9993 | `	}` |
|      38 |  9994 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9995 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9996 | `		return SXERR_ABORT;` |
|       - |  9997 | `	}` |
|      38 |  9998 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9999 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10000 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10001 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10002 | `			return SXERR_ABORT;` |
|       - | 10003 | `		}` |
|     ! 0 | 10004 | `		return SXRET_OK;` |
|       - | 10005 | `	}` |
|       - | 10006 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10007 | `	pBlock = pGen->pCurrent;` |
|      60 | 10008 | `	while( pBlock->pParent ){` |
|      49 | 10009 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10010 | `			break;` |
|       - | 10011 | `		}` |
|      23 | 10012 | `		pBlock = pBlock->pParent;` |
|       1 | 10013 | `	}` |
|      38 | 10014 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10015 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10016 | `	return SXRET_OK;` |
|      20 | 10017 |  |
|       - | 10018 | `/*` |
|       - | 10019 | ` * Compile a 'catch' block.` |
|       - | 10020 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10021 | ` * an object containing the exception information.` |
|       - | 10022 | ` */` |
|     572 | 10023 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10024 |  |
|     577 | 10025 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10026 | `	ph7_exception_block sCatch;` |
|       - | 10027 | `	SySet *pInstrContainer;` |
|       - | 10028 | `	SyString sClassName;` |
|       - | 10029 | `	GenBlock *pCatch;` |
|       - | 10030 | `	SyToken *pToken;` |
|       - | 10031 | `	SyString *pName;` |
|       - | 10032 | `	char *zDup;` |
|       - | 10033 | `	sxi32 rc;` |
|     577 | 10034 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10035 | `	/* Zero the structure */` |
|     577 | 10036 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10037 | `	/* Initialize fields */` |
|     577 | 10038 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     577 | 10039 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     577 | 10040 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10041 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10042 | `			pToken = pGen->pIn;` |
|     ! 0 | 10043 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10044 | `				pToken--;` |
|     ! 0 | 10045 | `			}` |
|     ! 0 | 10046 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10047 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10048 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10049 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10050 | `				return SXERR_ABORT;` |
|       - | 10051 | `			}` |
|     ! 0 | 10052 | `			return SXERR_INVALID;` |
|       - | 10053 | `	}` |
|       - | 10054 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     577 | 10055 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     300 | 10056 | `	for(;;){` |
|       - | 10057 | `		SyBlob sResolved;` |
|     605 | 10058 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     605 | 10059 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10060 | `			SyBlobRelease(&sResolved);` |
|       6 | 10061 | `			pToken = pGen->pIn;` |
|       6 | 10062 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10063 | `				pToken--;` |
|     ! 0 | 10064 | `			}` |
|       8 | 10065 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10066 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10067 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10068 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10069 | `				return SXERR_ABORT;` |
|       - | 10070 | `			}` |
|       6 | 10071 | `			return SXERR_INVALID;` |
|       - | 10072 | `		}` |
|       - | 10073 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10074 | `		 * transient SyBlob allocation. */` |
|     899 | 10075 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     596 | 10076 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     601 | 10077 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     601 | 10078 | `		SyBlobRelease(&sResolved);` |
|     601 | 10079 | `		if( zDup == 0 ){` |
|     ! 0 | 10080 | `			goto Mem;` |
|       - | 10081 | `		}` |
|     601 | 10082 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     601 | 10083 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10084 | `			goto Mem;` |
|       - | 10085 | `		}` |
|       - | 10086 | `		/* Check for '\|' (multi-catch separator) */` |
|     596 | 10087 | `		if( pGen->pIn < pGen->pEnd &&` |
|     596 | 10088 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 10089 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 10090 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 10091 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 10092 | `			continue;` |
|       - | 10093 | `		}` |
|     573 | 10094 | `		break;` |
|     ! 0 | 10095 | `	}` |
|     568 | 10096 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     573 | 10097 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10098 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10099 | `			pToken = pGen->pIn;` |
|     ! 0 | 10100 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10101 | `				pToken--;` |
|     ! 0 | 10102 | `			}` |
|     ! 0 | 10103 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10104 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10105 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10106 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10107 | `				return SXERR_ABORT;` |
|       - | 10108 | `			}` |
|     ! 0 | 10109 | `			return SXERR_INVALID;` |
|       - | 10110 | `	}` |
|     573 | 10111 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10112 | `	/* Duplicate instance name */` |
|     573 | 10113 | `	pName = &pGen->pIn->sData;` |
|     573 | 10114 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     573 | 10115 | `	if( zDup == 0 ){` |
|     ! 0 | 10116 | `		goto Mem;` |
|       - | 10117 | `	}` |
|     573 | 10118 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     573 | 10119 | `	pGen->pIn++;` |
|     573 | 10120 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10121 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10122 | `		pToken = pGen->pIn;` |
|     ! 0 | 10123 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10124 | `			pToken--;` |
|     ! 0 | 10125 | `		}` |
|     ! 0 | 10126 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10127 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10128 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10129 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10130 | `			return SXERR_ABORT;` |
|       - | 10131 | `		}` |
|     ! 0 | 10132 | `		return SXERR_INVALID;` |
|       - | 10133 | `	}` |
|       - | 10134 | `	/* Compile the block */` |
|     573 | 10135 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10136 | `	/* Create the catch block */` |
|     573 | 10137 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     573 | 10138 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10139 | `		return SXERR_ABORT;` |
|       - | 10140 | `	}` |
|       - | 10141 | `	/* Swap bytecode container */` |
|     573 | 10142 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     573 | 10143 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10144 | `	/* Compile the block */` |
|     573 | 10145 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10146 | `	/* Fix forward jumps now the destination is resolved  */` |
|     573 | 10147 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10148 | `	/* Emit the DONE instruction */` |
|     573 | 10149 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10150 | `	/* Leave the block */` |
|     573 | 10151 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10152 | `	/* Restore the default container */` |
|     573 | 10153 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10154 | `	/* Install the catch block */` |
|     573 | 10155 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     573 | 10156 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10157 | `		goto Mem;` |
|       - | 10158 | `	}` |
|     573 | 10159 | `	return SXRET_OK;` |
|     ! 0 | 10160 | `Mem:` |
|     ! 0 | 10161 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10162 | `	return SXERR_ABORT;` |
|     291 | 10163 |  |
|       - | 10164 | `/*` |
|       - | 10165 | ` * Compile a 'try' block.` |
|       - | 10166 | ` * A function using an exception should be in a "try" block.` |
|       - | 10167 | ` * If the exception does not trigger, the code will continue` |
|       - | 10168 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10169 | ` * is "thrown".` |
|       - | 10170 | ` */` |
|     610 | 10171 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10172 |  |
|       - | 10173 | `	ph7_exception *pException;` |
|     615 | 10174 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10175 | `	GenBlock *pTry;` |
|       - | 10176 | `	sxu32 nJmpIdx;` |
|       - | 10177 | `	sxi32 rc;` |
|       - | 10178 | `	/* Create the exception container */` |
|     615 | 10179 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     615 | 10180 | `	if( pException == 0 ){` |
|     ! 0 | 10181 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 10182 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10183 | `		return SXERR_ABORT;` |
|       - | 10184 | `	}` |
|       - | 10185 | `	/* Zero the structure */` |
|     615 | 10186 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 10187 | `	/* Initialize fields */` |
|     615 | 10188 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     615 | 10189 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     615 | 10190 | `	pException->iHasFinally = 0;` |
|     615 | 10191 | `	pException->iFinallyDone = 0;` |
|     615 | 10192 | `	pException->pVm = pGen->pVm;` |
|       - | 10193 | `	/* Create the try block */` |
|     615 | 10194 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     615 | 10195 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10196 | `		return SXERR_ABORT;` |
|       - | 10197 | `	}` |
|       - | 10198 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     615 | 10199 | `	pTry->pUserData = pException;` |
|       - | 10200 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     615 | 10201 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 10202 | `	/* Fix the jump later when the destination is resolved */` |
|     615 | 10203 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     615 | 10204 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 10205 | `	/* Compile the block */` |
|     615 | 10206 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     615 | 10207 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10208 | `		return SXERR_ABORT;` |
|       - | 10209 | `	}` |
|       - | 10210 | `	/* Fix forward jumps now the destination is resolved */` |
|     615 | 10211 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10212 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     615 | 10213 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 10214 | `	/* Leave the block */` |
|     615 | 10215 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10216 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     615 | 10217 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     608 | 10218 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 10219 | `		/* Compile one or more catch blocks */` |
|     568 | 10220 | `		for(;;){` |
|    1136 | 10221 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     919 | 10222 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     287 | 10223 | `					break;` |
|       - | 10224 | `			}` |
|     577 | 10225 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     577 | 10226 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10227 | `				return SXERR_ABORT;` |
|       - | 10228 | `			}` |
|       5 | 10229 | `		}` |
|     282 | 10230 | `	}` |
|       - | 10231 | `	/* Compile optional finally block */` |
|     615 | 10232 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     334 | 10233 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10234 | `		SySet *pInstrContainer;` |
|       - | 10235 | `		GenBlock *pFinBlock;` |
|     107 | 10236 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10237 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     107 | 10238 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     107 | 10239 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10240 | `			return SXERR_ABORT;` |
|       - | 10241 | `		}` |
|       - | 10242 | `		/* Swap bytecode container */` |
|     107 | 10243 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     107 | 10244 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10245 | `		/* Compile the finally body */` |
|     107 | 10246 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     107 | 10247 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10248 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10249 | `			return SXERR_ABORT;` |
|       - | 10250 | `		}` |
|       - | 10251 | `		/* Fix forward jumps now the destination is resolved */` |
|     107 | 10252 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10253 | `		/* Emit DONE to terminate the finally block */` |
|     107 | 10254 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10255 | `		/* Leave the block */` |
|     107 | 10256 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10257 | `		/* Restore the default container */` |
|     107 | 10258 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     107 | 10259 | `		pException->iHasFinally = 1;` |
|      51 | 10260 | `	}` |
|       - | 10261 | `	/* Must have at least one catch or finally */` |
|     615 | 10262 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 | 10263 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10264 | `			"Cannot use try without catch or finally");` |
|       8 | 10265 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10266 | `			return SXERR_ABORT;` |
|       - | 10267 | `		}` |
|       3 | 10268 | `	}` |
|     615 | 10269 | `	return SXRET_OK;` |
|     310 | 10270 |  |
|       - | 10271 | `/*` |
|       - | 10272 | ` * Compile a switch block.` |
|       - | 10273 | ` *  (See block-comment below for more information)` |
|       - | 10274 | ` */` |
|     112 | 10275 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 10276 |  |
|     117 | 10277 | `	sxi32 rc = SXRET_OK;` |
|     117 | 10278 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 10279 | `		/* Unexpected token */` |
|     ! 0 | 10280 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10281 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10282 | `			return SXERR_ABORT;` |
|       - | 10283 | `		}` |
|     ! 0 | 10284 | `		pGen->pIn++;` |
|     ! 0 | 10285 | `	}` |
|     117 | 10286 | `	pGen->pIn++;` |
|       - | 10287 | `	/* First instruction to execute in this block. */` |
|     117 | 10288 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10289 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 10290 | `	 * or the '}' token */` |
|     206 | 10291 | `	for(;;){` |
|     417 | 10292 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10293 | `			/* No more input to process */` |
|     ! 0 | 10294 | `			break;` |
|       - | 10295 | `		}` |
|     417 | 10296 | `		rc = SXRET_OK;` |
|     417 | 10297 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 10298 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 10299 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 10300 | `					/* Unexpected token */` |
|     ! 0 | 10301 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10302 | `						&pGen->pIn->sData);` |
|     ! 0 | 10303 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10304 | `						return SXERR_ABORT;` |
|       - | 10305 | `					}` |
|       - | 10306 | `					/* FALL THROUGH */` |
|     ! 0 | 10307 | `				}` |
|      31 | 10308 | `				rc = SXERR_EOF;` |
|      31 | 10309 | `				break;` |
|       - | 10310 | `			}` |
|      32 | 10311 | `		}else{` |
|       - | 10312 | `			sxi32 nKwrd;` |
|       - | 10313 | `			/* Extract the keyword */` |
|     337 | 10314 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 10315 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 10316 | `				break;` |
|       - | 10317 | `			}` |
|     253 | 10318 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10319 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 10320 | `					/* Unexpected token */` |
|     ! 0 | 10321 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10322 | `						&pGen->pIn->sData);` |
|     ! 0 | 10323 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10324 | `						return SXERR_ABORT;` |
|       - | 10325 | `					}` |
|       - | 10326 | `					/* FALL THROUGH */` |
|     ! 0 | 10327 | `				}` |
|       - | 10328 | `				/* Block compiled */` |
|       3 | 10329 | `				break;` |
|       - | 10330 | `			}` |
|       - | 10331 | `		}` |
|       - | 10332 | `		/* Compile block */` |
|     305 | 10333 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 10334 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10335 | `			return SXERR_ABORT;` |
|       - | 10336 | `		}` |
|       5 | 10337 | `	}` |
|     117 | 10338 | `	return rc;` |
|      61 | 10339 |  |
|       - | 10340 | `/*` |
|       - | 10341 | ` * Compile a case eXpression.` |
|       - | 10342 | ` *  (See block-comment below for more information)` |
|       - | 10343 | ` */` |
|      92 | 10344 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 10345 |  |
|       - | 10346 | `	SySet *pInstrContainer;` |
|       - | 10347 | `	SyToken *pEnd,*pTmp;` |
|      97 | 10348 | `	sxi32 iNest = 0;` |
|       - | 10349 | `	sxi32 rc;` |
|       - | 10350 | `	/* Delimit the expression */` |
|      97 | 10351 | `	pEnd = pGen->pIn;` |
|     197 | 10352 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 10353 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 10354 | `			/* Increment nesting level */` |
|       3 | 10355 | `			iNest++;` |
|     196 | 10356 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 10357 | `			/* Decrement nesting level */` |
|       3 | 10358 | `			iNest--;` |
|     194 | 10359 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 10360 | `			break;` |
|       - | 10361 | `		}` |
|     105 | 10362 | `		pEnd++;` |
|       5 | 10363 | `	}` |
|      97 | 10364 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 10365 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 10366 | `		if( rc == SXERR_ABORT ){` |
|       - | 10367 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10368 | `			return SXERR_ABORT;` |
|       - | 10369 | `		}` |
|     ! 0 | 10370 | `	}` |
|       - | 10371 | `	/* Swap token stream */` |
|      97 | 10372 | `	pTmp = pGen->pEnd;` |
|      97 | 10373 | `	pGen->pEnd = pEnd;` |
|      97 | 10374 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 10375 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 10376 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 10377 | `	/* Emit the done instruction */` |
|      97 | 10378 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 10379 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10380 | `	/* Update token stream */` |
|      97 | 10381 | `	pGen->pIn  = pEnd;` |
|      97 | 10382 | `	pGen->pEnd = pTmp;` |
|      97 | 10383 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10384 | `		return SXERR_ABORT;` |
|       - | 10385 | `	}` |
|      97 | 10386 | `	return SXRET_OK;` |
|      51 | 10387 |  |
|       - | 10388 | `/*` |
|       - | 10389 | ` * Compile the smart switch statement.` |
|       - | 10390 | ` * According to the PHP language reference manual` |
|       - | 10391 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 10392 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 10393 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 10394 | ` *  This is exactly what the switch statement is for.` |
|       - | 10395 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 10396 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 10397 | ` *  of the outer loop, use continue 2.` |
|       - | 10398 | ` *  Note that switch/case does loose comparision.` |
|       - | 10399 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 10400 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 10401 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 10402 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 10403 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 10404 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 10405 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 10406 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 10407 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 10408 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 10409 | ` *  list for the next case.` |
|       - | 10410 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 10411 | ` *  or floating-point numbers and strings.` |
|       - | 10412 | ` */` |
|      28 | 10413 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 10414 |  |
|       - | 10415 | `	GenBlock *pSwitchBlock;` |
|       - | 10416 | `	SyToken *pTmp,*pEnd;` |
|       - | 10417 | `	ph7_switch *pSwitch;` |
|       - | 10418 | `	sxu32 nToken;` |
|       - | 10419 | `	sxu32 nLine;` |
|       - | 10420 | `	sxi32 rc;` |
|      33 | 10421 | `	nLine = pGen->pIn->nLine;` |
|       - | 10422 | `	/* Jump the 'switch' keyword */` |
|      33 | 10423 | `	pGen->pIn++;` |
|      33 | 10424 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 10425 | `		/* Syntax error */` |
|     ! 0 | 10426 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 10427 | `		if( rc == SXERR_ABORT ){` |
|       - | 10428 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10429 | `			return SXERR_ABORT;` |
|       - | 10430 | `		}` |
|     ! 0 | 10431 | `		goto Synchronize;` |
|       - | 10432 | `	}` |
|       - | 10433 | `	/* Jump the left parenthesis '(' */` |
|      33 | 10434 | `	pGen->pIn++;` |
|      33 | 10435 | `	pEnd = 0; /* cc warning */` |
|       - | 10436 | `	/* Create the loop block */` |
|      47 | 10437 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 10438 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 10439 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10440 | `		return SXERR_ABORT;` |
|       - | 10441 | `	}` |
|       - | 10442 | `	/* Delimit the condition */` |
|      33 | 10443 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 10444 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 10445 | `		/* Empty expression */` |
|     ! 0 | 10446 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 10447 | `		if( rc == SXERR_ABORT ){` |
|       - | 10448 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10449 | `			return SXERR_ABORT;` |
|       - | 10450 | `		}` |
|     ! 0 | 10451 | `	}` |
|       - | 10452 | `	/* Swap token streams */` |
|      33 | 10453 | `	pTmp = pGen->pEnd;` |
|      33 | 10454 | `	pGen->pEnd = pEnd;` |
|       - | 10455 | `	/* Compile the expression */` |
|      33 | 10456 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 10457 | `	if( rc == SXERR_ABORT ){` |
|       - | 10458 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 10459 | `		return SXERR_ABORT;` |
|       - | 10460 | `	}` |
|       - | 10461 | `	/* Update token stream */` |
|      33 | 10462 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 10463 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10464 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10465 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10466 | `			return SXERR_ABORT;` |
|       - | 10467 | `		}` |
|     ! 0 | 10468 | `		pGen->pIn++;` |
|     ! 0 | 10469 | `	}` |
|      33 | 10470 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 10471 | `	pGen->pEnd = pTmp;` |
|      33 | 10472 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 10473 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 10474 | `			pTmp = pGen->pIn;` |
|     ! 0 | 10475 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 10476 | `				pTmp--;` |
|     ! 0 | 10477 | `			}` |
|       - | 10478 | `			/* Unexpected token */` |
|     ! 0 | 10479 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 10480 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10481 | `				return SXERR_ABORT;` |
|       - | 10482 | `			}` |
|     ! 0 | 10483 | `			goto Synchronize;` |
|       - | 10484 | `	}` |
|       - | 10485 | `	/* Set the delimiter token */` |
|      33 | 10486 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 10487 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 10488 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 10489 | `	}else{` |
|      31 | 10490 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 10491 | `	}` |
|      33 | 10492 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 10493 | `	/* Create the switch blocks container */` |
|      33 | 10494 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 10495 | `	if( pSwitch == 0 ){` |
|       - | 10496 | `		/* Abort compilation */` |
|     ! 0 | 10497 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10498 | `		return SXERR_ABORT;` |
|       - | 10499 | `	}` |
|       - | 10500 | `	/* Zero the structure */` |
|      33 | 10501 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 10502 | `	/* Initialize fields */` |
|      33 | 10503 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 10504 | `	/* Emit the switch instruction */` |
|      33 | 10505 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 10506 | `	/* Compile case blocks */` |
|     100 | 10507 | `	for(;;){` |
|       - | 10508 | `		sxu32 nKwrd;` |
|     119 | 10509 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10510 | `			/* No more input to process */` |
|     ! 0 | 10511 | `			break;` |
|       - | 10512 | `		}` |
|     119 | 10513 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 10514 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 10515 | `				/* Unexpected token */` |
|     ! 0 | 10516 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10517 | `					&pGen->pIn->sData);` |
|     ! 0 | 10518 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10519 | `					return SXERR_ABORT;` |
|       - | 10520 | `				}` |
|       - | 10521 | `				/* FALL THROUGH */` |
|     ! 0 | 10522 | `			}` |
|       - | 10523 | `			/* Block compiled */` |
|     ! 0 | 10524 | `			break;` |
|       - | 10525 | `		}` |
|       - | 10526 | `		/* Extract the keyword */` |
|     119 | 10527 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 10528 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10529 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 10530 | `				/* Unexpected token */` |
|     ! 0 | 10531 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10532 | `					&pGen->pIn->sData);` |
|     ! 0 | 10533 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10534 | `					return SXERR_ABORT;` |
|       - | 10535 | `				}` |
|       - | 10536 | `				/* FALL THROUGH */` |
|     ! 0 | 10537 | `			}` |
|       - | 10538 | `			/* Block compiled */` |
|       3 | 10539 | `			break;` |
|       - | 10540 | `		}` |
|     117 | 10541 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 10542 | `			/*` |
|       - | 10543 | `			 * Accroding to the PHP language reference manual` |
|       - | 10544 | `			 *  A special case is the default case. This case matches anything` |
|       - | 10545 | `			 *  that wasn't matched by the other cases.` |
|       - | 10546 | `			 */` |
|      25 | 10547 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 10548 | `				/* Default case already compiled */` |
|     ! 0 | 10549 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 10550 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10551 | `					return SXERR_ABORT;` |
|       - | 10552 | `				}` |
|     ! 0 | 10553 | `			}` |
|      25 | 10554 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 10555 | `			/* Compile the default block */` |
|      25 | 10556 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 10557 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10558 | `				return SXERR_ABORT;` |
|      25 | 10559 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 10560 | `				break;` |
|       1 | 10561 | `			}` |
|      98 | 10562 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 10563 | `			ph7_case_expr sCase;` |
|       - | 10564 | `			/* Standard case block */` |
|      97 | 10565 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 10566 | `			/* initialize the structure */` |
|      97 | 10567 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 10568 | `			/* Compile the case expression */` |
|      97 | 10569 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 10570 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10571 | `				return SXERR_ABORT;` |
|       - | 10572 | `			}` |
|       - | 10573 | `			/* Compile the case block */` |
|      97 | 10574 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 10575 | `			/* Insert in the switch container */` |
|      97 | 10576 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 10577 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10578 | `				return SXERR_ABORT;` |
|      97 | 10579 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 10580 | `				break;` |
|       - | 10581 | `			}` |
|      47 | 10582 | `		}else{` |
|       - | 10583 | `			/* Unexpected token */` |
|     ! 0 | 10584 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10585 | `				&pGen->pIn->sData);` |
|     ! 0 | 10586 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10587 | `				return SXERR_ABORT;` |
|       - | 10588 | `			}` |
|     ! 0 | 10589 | `			break;` |
|       - | 10590 | `		}` |
|       5 | 10591 | `	}` |
|       - | 10592 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 10593 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 10594 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10595 | `	/* Release the loop block */` |
|      33 | 10596 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 10597 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 10598 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 10599 | `		pGen->pIn++;` |
|      14 | 10600 | `	}` |
|       - | 10601 | `	/* Statement successfully compiled */` |
|      33 | 10602 | `	return SXRET_OK;` |
|     ! 0 | 10603 | `Synchronize:` |
|       - | 10604 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 10605 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 10606 | `		pGen->pIn++;` |
|     ! 0 | 10607 | `	}` |
|     ! 0 | 10608 | `	return SXRET_OK;` |
|      19 | 10609 |  |
|       - | 10610 | `/*` |
|       - | 10611 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 10612 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 10613 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 10614 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 10615 | ` */` |
|       - | 10616 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 10617 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 10618 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 10619 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 10620 |  |
|       - | 10621 | `/*` |
|       - | 10622 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 10623 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 10624 | ` * patched entries from the pending set.` |
|       - | 10625 | ` */` |
| 2554600 | 10626 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 10627 |  |
| 2554605 | 10628 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 10629 | `	sxu32 nTarget;` |
|       - | 10630 | `	sxu32 *aIdx;` |
|       - | 10631 | `	sxu32 i;` |
| 2554605 | 10632 | `	if( nCur <= nBaseline ){` |
| 2554511 | 10633 | `		return;` |
|       - | 10634 | `	}` |
|      97 | 10635 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      97 | 10636 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     199 | 10637 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     105 | 10638 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     105 | 10639 | `		if( pInstr ){` |
|     105 | 10640 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      51 | 10641 | `		}` |
|      54 | 10642 | `	}` |
|      97 | 10643 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1277305 | 10644 |  |
|       - | 10645 |  |
|       - | 10646 | `/*` |
|       - | 10647 | ` * By-reference out-parameters of builtin functions.` |
|       - | 10648 | ` *` |
|       - | 10649 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 10650 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 10651 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 10652 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 10653 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 10654 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 10655 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 10656 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 10657 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 10658 | ` * creates it" behaviour).` |
|       - | 10659 | ` *` |
|       - | 10660 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 10661 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 10662 | ` */` |
|  423758 | 10663 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 10664 |  |
|       - | 10665 | `	static const struct {` |
|       - | 10666 | `		const char *zName;` |
|       - | 10667 | `		sxu32 nByte;` |
|       - | 10668 | `		sxu32 mask;` |
|       - | 10669 | `	} aByRef[] = {` |
|       - | 10670 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10671 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10672 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10673 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10674 | `	};` |
|       - | 10675 | `	sxu32 i;` |
|  423763 | 10676 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1435 | 10677 | `		return 0;` |
|       - | 10678 | `	}` |
| 2111425 | 10679 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1689160 | 10680 | `		if( pName->nByte == aByRef[i].nByte` |
|  866735 | 10681 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      73 | 10682 | `			return aByRef[i].mask;` |
|       - | 10683 | `		}` |
|  844551 | 10684 | `	}` |
|  422265 | 10685 | `	return 0;` |
|  211884 | 10686 |  |
|       - | 10687 | `/*` |
|       - | 10688 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 10689 | ` *` |
|       - | 10690 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 10691 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 10692 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 10693 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 10694 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 10695 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 10696 | ` */` |
|  423758 | 10697 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 10698 |  |
|       - | 10699 | `	SyToken *p, *pEnd;` |
|  423763 | 10700 | `	pOut->zString = 0;` |
|  423763 | 10701 | `	pOut->nByte = 0;` |
|  423763 | 10702 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 10703 | `		return;` |
|       - | 10704 | `	}` |
|  423763 | 10705 | `	p = pLeft->pStart;` |
|  423763 | 10706 | `	pEnd = pLeft->pEnd;` |
|       - | 10707 | `	/* Optional single leading namespace separator (absolute path). */` |
|  423763 | 10708 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3565 | 10709 | `		p++;` |
|    1780 | 10710 | `	}` |
|  423763 | 10711 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1407 | 10712 | `		return;` |
|       - | 10713 | `	}` |
|       - | 10714 | `	/* Must be a single component: nothing follows the name token. */` |
|  422361 | 10715 | `	if( p + 1 != pEnd ){` |
|      32 | 10716 | `		return;` |
|       - | 10717 | `	}` |
|  422333 | 10718 | `	*pOut = p->sData;` |
|  211884 | 10719 |  |
|       - | 10720 | `/*` |
|       - | 10721 | ` * Generate bytecode for a given expression tree.` |
|       - | 10722 | ` * If something goes wrong while generating bytecode` |
|       - | 10723 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 10724 | ` * this function takes care of generating the appropriate` |
|       - | 10725 | ` * error message.` |
|       - | 10726 | ` */` |
| 3421864 | 10727 | `static sxi32 GenStateEmitExprCode(` |
|       - | 10728 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10729 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 10730 | `	sxi32 iFlags /* Control flags */` |
|       - | 10731 | `	)` |
|       5 | 10732 |  |
|       - | 10733 | `	VmInstr *pInstr;` |
|       - | 10734 | `	sxu32 nJmpIdx;` |
| 3421869 | 10735 | `	sxi32 iP1 = 0;` |
| 3421869 | 10736 | `	sxu32 iP2 = 0;` |
| 3421869 | 10737 | `	void *p3  = 0;` |
|       - | 10738 | `	sxi32 iVmOp;` |
|       - | 10739 | `	sxi32 rc;` |
| 3421869 | 10740 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3421869 | 10741 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 3421869 | 10742 | `	sxu32 nRhsNsBase = 0;` |
| 3421869 | 10743 | `	if( pNode->xCode ){` |
|       - | 10744 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 10745 | `		/* Compile node */` |
| 2124915 | 10746 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2124915 | 10747 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2124915 | 10748 | `		RE_SWAP_DELIMITER(pGen);` |
| 2124915 | 10749 | `		return rc;` |
|       - | 10750 | `	}` |
| 1296959 | 10751 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10752 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10753 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10754 | `		return SXERR_ABORT;` |
|       - | 10755 | `	}` |
| 1296959 | 10756 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1296959 | 10757 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      59 | 10758 | `		sxu32 nJmp = 0;` |
|       - | 10759 | `		sxu32 nNcNsBase;` |
|       - | 10760 | `		VmInstr *pInstrFix;` |
|       - | 10761 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10762 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10763 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10764 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10765 | `		 * stack slot carries a writable nIdx. */` |
|      59 | 10766 | `		if( pNode->pRight ){` |
|      59 | 10767 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10768 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      59 | 10769 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10770 | `				return rc;` |
|       - | 10771 | `			}` |
|      59 | 10772 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10773 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10774 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10775 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10776 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10777 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10778 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10779 | `			 * cascade for the actual write path stays correct. */` |
|      59 | 10780 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      59 | 10781 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      29 | 10782 | `				pInstrFix->iP2 = 3;` |
|      13 | 10783 | `			}` |
|      28 | 10784 | `		}` |
|       - | 10785 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      59 | 10786 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10787 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      59 | 10788 | `		if( pNode->pLeft ){` |
|      59 | 10789 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10790 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      59 | 10791 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10792 | `				return rc;` |
|       - | 10793 | `			}` |
|      59 | 10794 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      28 | 10795 | `		}` |
|       - | 10796 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      59 | 10797 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10798 | `		/* Patch the short-circuit jump to land after the store. */` |
|      59 | 10799 | `		if( nJmp > 0 ){` |
|      59 | 10800 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      59 | 10801 | `			if( pInstrFix ){` |
|      59 | 10802 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10803 | `			}` |
|      28 | 10804 | `		}` |
|      59 | 10805 | `		return SXRET_OK;` |
|       - | 10806 | `	}` |
| 1296903 | 10807 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10808 | `		sxu32 nJz,nJmp;` |
|       - | 10809 | `		sxu32 nTernaryNsBase;` |
|       - | 10810 | `		/* Ternary operator require special handling */` |
|       - | 10811 | `		/* Phase#1: Compile the condition */` |
|    2659 | 10812 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2659 | 10813 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2659 | 10814 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10815 | `			return rc;` |
|       - | 10816 | `		}` |
|       - | 10817 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10818 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10819 | `		 * condition expression, not leak past the ternary. */` |
|    2659 | 10820 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2659 | 10821 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2659 | 10822 | `		if( pNode->pLeft ){` |
|       - | 10823 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10824 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2591 | 10825 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10826 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2591 | 10827 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2591 | 10828 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2591 | 10829 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10830 | `				return rc;` |
|       - | 10831 | `			}` |
|    2591 | 10832 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1298 | 10833 | `		}else{` |
|       - | 10834 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10835 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10836 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10837 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10838 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10839 | `		}` |
|       - | 10840 | `		/* Phase#4: Emit the unconditional jump */` |
|    2659 | 10841 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10842 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2659 | 10843 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2659 | 10844 | `		if( pInstr ){` |
|    2659 | 10845 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1327 | 10846 | `		}` |
|    2659 | 10847 | `		if( !pNode->pLeft ){` |
|       - | 10848 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10849 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10850 | `		}` |
|       - | 10851 | `		/* Phase#6: Compile the 'else' expression */` |
|    2659 | 10852 | `		if( pNode->pRight ){` |
|    2659 | 10853 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2659 | 10854 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2659 | 10855 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10856 | `				return rc;` |
|       - | 10857 | `			}` |
|    2659 | 10858 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1327 | 10859 | `		}` |
|    2659 | 10860 | `		if( nJmp > 0 ){` |
|       - | 10861 | `			/* Phase#7: Fix the unconditional jump */` |
|    2659 | 10862 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2659 | 10863 | `			if( pInstr ){` |
|    2659 | 10864 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1327 | 10865 | `			}` |
|    1327 | 10866 | `		}` |
|       - | 10867 | `		/* All done */` |
|    2659 | 10868 | `		return SXRET_OK;` |
|       - | 10869 | `	}` |
| 1294249 | 10870 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10871 | `	/* Generate code for the left tree */` |
| 1294249 | 10872 | `	if( pNode->pLeft ){` |
| 1294209 | 10873 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1294209 | 10874 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10875 | `			ph7_expr_node **apNode;` |
|  423889 | 10876 | `			int hasSpread = 0;` |
|  423889 | 10877 | `			int hasNamed = 0;` |
|  423889 | 10878 | `			int bAnySpread = 0;` |
|  423889 | 10879 | `			sxu32 byRefMask = 0;` |
|       - | 10880 | `			sxi32 nArgs;` |
|       - | 10881 | `			sxi32 n;` |
|       - | 10882 | `			/* Recurse and generate bytecodes for function arguments */` |
|  423889 | 10883 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  423889 | 10884 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10885 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|       - | 10886 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|       - | 10887 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  423889 | 10888 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      37 | 10889 | `				bFcc = 1;` |
|      37 | 10890 | `				nArgs = 0;` |
|      18 | 10891 | `			}` |
|       - | 10892 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10893 | `			{` |
|  423889 | 10894 | `				int seenNamed = 0;` |
|  839263 | 10895 | `				for( n = 0; n < nArgs; ++n ){` |
|  415381 | 10896 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     190 | 10897 | `						seenNamed = 1;` |
|     190 | 10898 | `						hasNamed = 1;` |
|  415288 | 10899 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      27 | 10900 | `						bAnySpread = 1;` |
|  415183 | 10901 | `					}else if( seenNamed ){` |
|       3 | 10902 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10903 | `							"Cannot use positional argument after named argument");` |
|       3 | 10904 | `						return SXERR_SYNTAX;` |
|       - | 10905 | `					}` |
|  207692 | 10906 | `				}` |
|       - | 10907 | `			}` |
|       - | 10908 | `			/* Read-only load */` |
|  423887 | 10909 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10910 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10911 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10912 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10913 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  423887 | 10914 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  423887 | 10915 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  423882 | 10916 | `				if( pCallName->nByte == 5` |
|  232400 | 10917 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   21535 | 10918 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  413122 | 10919 | `				}else if( pCallName->nByte == 5` |
|  210870 | 10920 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      83 | 10921 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      39 | 10922 | `				}` |
|       - | 10923 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10924 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10925 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10926 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10927 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10928 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  423887 | 10929 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10930 | `					SyString sBuiltin;` |
|  423763 | 10931 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  423763 | 10932 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  211879 | 10933 | `				}` |
|  211941 | 10934 | `			}` |
|  839259 | 10935 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  415377 | 10936 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  415377 | 10937 | `				sxi32 iArgFlags = iFlags & ~EXPR_FLAG_LOAD_IDX_STORE;` |
|       - | 10938 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 10939 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 10940 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 10941 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 10942 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 10943 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  415377 | 10944 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      53 | 10945 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      53 | 10946 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      24 | 10947 | `				}` |
|  415377 | 10948 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  415377 | 10949 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10950 | `					return rc;` |
|       - | 10951 | `				}` |
|       - | 10952 | `				/* Each argument is an independent nullsafe scope. */` |
|  415377 | 10953 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  415377 | 10954 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10955 | `					/* Emit spread opcode to unpack this array argument */` |
|      27 | 10956 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      27 | 10957 | `					hasSpread = 1;` |
|      12 | 10958 | `				}` |
|  207691 | 10959 | `			}` |
|       - | 10960 | `			/* Total number of given arguments */` |
|  423887 | 10961 | `			iP1 = nArgs;` |
|  423887 | 10962 | `			iP2 = hasSpread;` |
|       - | 10963 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10964 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  423887 | 10965 | `			if( hasNamed ){` |
|     103 | 10966 | `				sxu32 nStrBytes = 0;` |
|       - | 10967 | `				char *zBuf;` |
|     301 | 10968 | `				for( n = 0; n < nArgs; ++n ){` |
|     201 | 10969 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     187 | 10970 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      92 | 10971 | `					}` |
|     102 | 10972 | `				}` |
|       - | 10973 | `				{` |
|     103 | 10974 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     103 | 10975 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     100 | 10976 | `					&pGen->pVm->sAllocator, mapSize);` |
|     103 | 10977 | `				if( pMap ){` |
|     103 | 10978 | `					SyZero(pMap, mapSize);` |
|     103 | 10979 | `					pMap->bHasNamed = 1;` |
|     103 | 10980 | `					pMap->nTotal = (sxu32)nArgs;` |
|     103 | 10981 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     103 | 10982 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     301 | 10983 | `					for( n = 0; n < nArgs; ++n ){` |
|     201 | 10984 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     187 | 10985 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     187 | 10986 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     187 | 10987 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     187 | 10988 | `							zBuf += nb;` |
|      92 | 10989 | `						}` |
|       - | 10990 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     102 | 10991 | `					}` |
|     103 | 10992 | `					p3 = (void *)pMap;` |
|      50 | 10993 | `				}` |
|       - | 10994 | `				}` |
|      50 | 10995 | `			}` |
|       - | 10996 | `			/* Remove stale flags now */` |
|  423887 | 10997 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  211941 | 10998 | `		}` |
| 1294207 | 10999 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1294207 | 11000 | `		if( rc != SXRET_OK ){` |
|      34 | 11001 | `			return rc;` |
|       - | 11002 | `		}` |
| 1294177 | 11003 | `		if( !bIsChainOp ){` |
|       - | 11004 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 11005 | `			 * target the end of that LHS chain, which is right here. */` |
|  603099 | 11006 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  301547 | 11007 | `		}` |
| 1294177 | 11008 | `		if( iVmOp == PH7_OP_CALL ){` |
|  423887 | 11009 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  423887 | 11010 | `			if( pInstr ){` |
|  423887 | 11011 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  422453 | 11012 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 11013 | `					sxu32 nQual;` |
|  422453 | 11014 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11015 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 11016 | `					 * so the later NEW handler (if any) can see it. */` |
|  422453 | 11017 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 11018 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 11019 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 11020 | `					 * imports — class imports must NOT affect function` |
|       - | 11021 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11022 | `					 * before NEW; we store the original literal index in the` |
|       - | 11023 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11024 | `					 * the unqualified name and re-qualify with class imports. */` |
|  422453 | 11025 | `					if( bAbsolute ){` |
|    3565 | 11026 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1785 | 11027 | `					}else{` |
|  418893 | 11028 | `						int fromImport = 0;` |
|  418893 | 11029 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  418893 | 11030 | `						pInstr->iP2 = (sxi32)nQual;` |
|  418893 | 11031 | `						if( nQual != nOrig ){` |
|       - | 11032 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11033 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11034 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11035 | `							if( !fromImport ){` |
|       - | 11036 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11037 | `								if( p3 == 0 ){` |
|      67 | 11038 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11039 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11040 | `									if( pMap ){` |
|      67 | 11041 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11042 | `										p3 = (void *)pMap;` |
|      31 | 11043 | `									}` |
|      31 | 11044 | `								}` |
|      67 | 11045 | `								if( p3 ){` |
|      67 | 11046 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11047 | `								}` |
|      31 | 11048 | `							}` |
|      36 | 11049 | `						}` |
|       5 | 11050 | `					}` |
|  212663 | 11051 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11052 | `					/* Method call,flag that */` |
|    1113 | 11053 | `					pInstr->iP2 = 1;` |
|     554 | 11054 | `				}` |
|  211946 | 11055 | `			}` |
| 1082236 | 11056 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11057 | `			ph7_expr_node **apNode;` |
|       - | 11058 | `			sxi32 n;` |
|   91043 | 11059 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11060 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11061 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       - | 11062 | `			/* Recurse and generate bytecodes for array index */` |
|   91043 | 11063 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  164297 | 11064 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   73259 | 11065 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   73259 | 11066 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   73259 | 11067 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11068 | `					return rc;` |
|       - | 11069 | `				}` |
|       - | 11070 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   73259 | 11071 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   36632 | 11072 | `			}` |
|   91043 | 11073 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   73259 | 11074 | `				iP1 = 1; /* Node have an index associated with it */` |
|   36627 | 11075 | `			}` |
|   91043 | 11076 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11077 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     243 | 11078 | `				iP2 = 4;` |
|   90924 | 11079 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11080 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11081 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      54 | 11082 | `				iP2 = 5;` |
|   90780 | 11083 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11084 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11085 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11086 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11087 | `				iP2 = 6;` |
|   90743 | 11088 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11089 | `				/* Create an empty entry when the desired index is not found */` |
|   35869 | 11090 | `				iP2 = 1;` |
|   17937 | 11091 | `			}` |
|  824776 | 11092 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11093 | `			/* POP the left node */` |
|      32 | 11094 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 11095 | `		}` |
|  647086 | 11096 | `	}` |
| 1294217 | 11097 | `	rc = SXRET_OK;` |
| 1294217 | 11098 | `	nJmpIdx = 0;` |
|       - | 11099 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 11100 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 11101 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1294217 | 11102 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     345 | 11103 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     345 | 11104 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     345 | 11105 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     345 | 11106 | `			int isSpecial = 0;` |
|     345 | 11107 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     249 | 11108 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     249 | 11109 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     244 | 11110 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     244 | 11111 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     116 | 11112 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      99 | 11113 | `					isSpecial = 1;` |
|      47 | 11114 | `				}` |
|     146 | 11115 | `			}` |
|     393 | 11116 | `			pInstr->iP1 = 0;` |
|     393 | 11117 | `			if( !isSpecial ){` |
|     203 | 11118 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      99 | 11119 | `			}` |
|       - | 11120 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 11121 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     297 | 11122 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     203 | 11123 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     203 | 11124 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      44 | 11125 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      46 | 11126 | `					return SXRET_OK;` |
|       - | 11127 | `				}` |
|      78 | 11128 | `			}` |
|     125 | 11129 | `		}` |
|     206 | 11130 | `	}` |
|       - | 11131 | `	/* Generate code for the right tree */` |
| 1294137 | 11132 | `	if( pNode->pRight ){` |
|  708547 | 11133 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 11134 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   11111 | 11135 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  702994 | 11136 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 11137 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3721 | 11138 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  695583 | 11139 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 11140 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 11141 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 11142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  693714 | 11143 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 11144 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 11145 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 11146 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 11147 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 11148 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 11149 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     105 | 11150 | `			sxu32 nNsJmp = 0;` |
|     105 | 11151 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     105 | 11152 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  693550 | 11153 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  292709 | 11154 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  146352 | 11155 | `		}` |
|  708547 | 11156 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  708547 | 11157 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  708547 | 11158 | `		if( !bIsChainOp ){` |
|       - | 11159 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 11160 | `			 * operator instruction is emitted. */` |
|  532431 | 11161 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  266213 | 11162 | `		}` |
|  708547 | 11163 | `		if( iVmOp == PH7_OP_STORE ){` |
|  288913 | 11164 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  288882 | 11165 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 11166 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 11167 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 11168 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 11169 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 11170 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 11171 | `				 */` |
|      74 | 11172 | `				iVmOp = 0;` |
|  288878 | 11173 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  288843 | 11174 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11175 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   78309 | 11176 | `					iP2 = 1;` |
|   39157 | 11177 | `				}else{` |
|  210539 | 11178 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11179 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   35797 | 11180 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   35797 | 11181 | `						iP1 = pInstr->iP1;` |
|   17901 | 11182 | `					}else{` |
|  174747 | 11183 | `						p3 = pInstr->p3;` |
|       - | 11184 | `					}` |
|       - | 11185 | `					/* POP the last dynamic load instruction */` |
|  210539 | 11186 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 11187 | `				}` |
|  144424 | 11188 | `			}` |
|  564093 | 11189 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      54 | 11190 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      54 | 11191 | `			if( pInstr ){` |
|      54 | 11192 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11193 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 11194 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 11195 | `					 */` |
|      17 | 11196 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 11197 | `					iP1 = pInstr->iP1;` |
|      17 | 11198 | `					iP2 = pInstr->iP2;` |
|      17 | 11199 | `					p3  = pInstr->p3;` |
|       9 | 11200 | `				}else{` |
|      38 | 11201 | `					p3 = pInstr->p3;` |
|       - | 11202 | `				}` |
|      26 | 11203 | `			}` |
|      26 | 11204 | `		}` |
|  354271 | 11205 | `	}` |
| 1294132 | 11206 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   11427 | 11207 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 11208 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 11209 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      29 | 11210 | `		iVmOp = 0;` |
|      13 | 11211 | `	}` |
| 1294137 | 11212 | `	if( iVmOp > 0 ){` |
| 1293887 | 11213 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   14549 | 11214 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 11215 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   10639 | 11216 | `				iP1 = 1;` |
|    5322 | 11217 | `			}` |
| 1286615 | 11218 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 11219 | `			/* Namespace-qualify the class name for NEW */ {` |
|   22663 | 11220 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   22663 | 11221 | `				VmInstr *pCallInstr = 0;` |
|   22663 | 11222 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   22529 | 11223 | `					pCallInstr = pPeek;` |
|   22529 | 11224 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   11262 | 11225 | `				}` |
|   22663 | 11226 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   22661 | 11227 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11228 | `					sxu32 nLitForClass;` |
|       - | 11229 | `					/* If the CALL handler already qualified the name using` |
|       - | 11230 | `					 * function imports, recover the original unqualified` |
|       - | 11231 | `					 * literal so we can re-qualify with class imports. */` |
|   22661 | 11232 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 11233 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 11234 | `					}else{` |
|   22629 | 11235 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 11236 | `					}` |
|   22661 | 11237 | `					pPeek->iP1 = 0;` |
|   22661 | 11238 | `					if( !bAbsolute ){` |
|   19105 | 11239 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    9555 | 11240 | `					}else{` |
|    3561 | 11241 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 11242 | `					}` |
|   11328 | 11243 | `				}` |
|       - | 11244 | `			}` |
|   22663 | 11245 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   22663 | 11246 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 11247 | `				VmInstr *pPrev;` |
|   22529 | 11248 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   22529 | 11249 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 11250 | `					/* Pop the call instruction, preserve named-arg map */` |
|   22529 | 11251 | `					iP1 = pInstr->iP1;` |
|   22529 | 11252 | `					if( pInstr->p3 ){` |
|      43 | 11253 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 11254 | `					}` |
|   22529 | 11255 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   11262 | 11256 | `				}` |
|   11267 | 11257 | `			}` |
| 1268014 | 11258 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 11259 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 11260 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     185 | 11261 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     185 | 11262 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     185 | 11263 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     185 | 11264 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     185 | 11265 | `				int isSpecialIs = 0;` |
|     185 | 11266 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     181 | 11267 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     181 | 11268 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     176 | 11269 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     181 | 11270 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      89 | 11271 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 11272 | `						isSpecialIs = 1;` |
|       5 | 11273 | `					}` |
|      89 | 11274 | `				}` |
|     187 | 11275 | `				pInstr->iP1 = 0;` |
|     187 | 11276 | `				if( !isSpecialIs && !bAbsolute ){` |
|     165 | 11277 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      80 | 11278 | `				}` |
|      94 | 11279 | `			}` |
| 1256598 | 11280 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 11281 | `			/* Prevent constant expansion for member/property names.` |
|       - | 11282 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 11283 | `			 * should not trigger constant lookup. */` |
|  176121 | 11284 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  176121 | 11285 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  176075 | 11286 | `				pInstr->iP1 = 0;` |
|   88035 | 11287 | `			}` |
|  176121 | 11288 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 11289 | `				/* Static member access,remember that */` |
|     265 | 11290 | `				iP1 = 1;` |
|     265 | 11291 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     265 | 11292 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      40 | 11293 | `					p3 = pInstr->p3;` |
|      40 | 11294 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      18 | 11295 | `				}` |
|     130 | 11296 | `			}` |
|   88058 | 11297 | `		}` |
|       - | 11298 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|       - | 11299 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|       - | 11300 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|       - | 11301 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|       - | 11302 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 1293885 | 11303 | `		if( bFcc ){` |
|      37 | 11304 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|      37 | 11305 | `			iP2 = 0;` |
|      37 | 11306 | `			p3 = 0;` |
|      37 | 11307 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      37 | 11308 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11309 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|       - | 11310 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|       - | 11311 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|       - | 11312 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      21 | 11313 | `				void *pMemberName = pInstr->p3;` |
|      21 | 11314 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      21 | 11315 | `				if( pMemberName ){` |
|       3 | 11316 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       1 | 11317 | `				}` |
|      21 | 11318 | `				iP1 = 2;` |
|      11 | 11319 | `			}else{` |
|      17 | 11320 | `				iP1 = 1;` |
|       - | 11321 | `			}` |
|      18 | 11322 | `		}` |
|       - | 11323 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 11324 | `		 * This is the primary emit path for user-visible calls. */` |
| 1293885 | 11325 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  446509 | 11326 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  223252 | 11327 | `		}` |
|       - | 11328 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1293885 | 11329 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  646940 | 11330 | `	}` |
| 1294135 | 11331 | `	if( nJmpIdx > 0 ){` |
|       - | 11332 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   14951 | 11333 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   14951 | 11334 | `		if( pInstr ){` |
|   14951 | 11335 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    7473 | 11336 | `		}` |
|    7473 | 11337 | `	}` |
| 1294135 | 11338 | `	return rc;` |
| 1710917 | 11339 |  |
|       - | 11340 | `/*` |
|       - | 11341 | ` * Compile a PHP expression.` |
|       - | 11342 | ` * According to the PHP language reference manual:` |
|       - | 11343 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 11344 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 11345 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 11346 | ` *  is "anything that has a value".` |
|       - | 11347 | ` * If something goes wrong while compiling the expression,this` |
|       - | 11348 | ` * function takes care of generating the appropriate error` |
|       - | 11349 | ` * message.` |
|       - | 11350 | ` */` |
|  922640 | 11351 | `static sxi32 PH7_CompileExpr(` |
|       - | 11352 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11353 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 11354 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 11355 | `	)` |
|       5 | 11356 |  |
|       - | 11357 | `	ph7_expr_node *pRoot;` |
|       - | 11358 | `	SySet sExprNode;` |
|       - | 11359 | `	SyToken *pEnd;` |
|       - | 11360 | `	sxi32 nExpr;` |
|       - | 11361 | `	sxi32 iNest;` |
|       - | 11362 | `	sxi32 rc;` |
|       - | 11363 | `	sxu32 nNullsafeBase;` |
|       - | 11364 | `	/* Initialize worker variables */` |
|  922645 | 11365 | `	nExpr = 0;` |
|  922645 | 11366 | `	pRoot = 0;` |
|       - | 11367 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 11368 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  922645 | 11369 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  922645 | 11370 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  922645 | 11371 | `	SySetAlloc(&sExprNode,0x10);` |
|  922645 | 11372 | `	rc = SXRET_OK;` |
|       - | 11373 | `	/* Delimit the expression */` |
|  922645 | 11374 | `	pEnd = pGen->pIn;` |
|  922645 | 11375 | `	iNest = 0;` |
| 6178659 | 11376 | `	while( pEnd < pGen->pEnd ){` |
| 5859465 | 11377 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11378 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     493 | 11379 | `			iNest++;` |
| 5859221 | 11380 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     501 | 11381 | `			iNest--;` |
| 5858729 | 11382 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  603799 | 11383 | `			if( iNest <= 0 ){` |
|  603451 | 11384 | `				break;` |
|       - | 11385 | `			}` |
|     174 | 11386 | `		}` |
| 5256019 | 11387 | `		pEnd++;` |
|       5 | 11388 | `	}` |
|  922645 | 11389 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   21777 | 11390 | `		SyToken *pEnd2 = pGen->pIn;` |
|   21777 | 11391 | `		iNest = 0;` |
|       - | 11392 | `		/* Stop at the first comma */` |
|   43843 | 11393 | `		while( pEnd2 < pEnd ){` |
|   22077 | 11394 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      67 | 11395 | `				iNest++;` |
|   22046 | 11396 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      67 | 11397 | `				iNest--;` |
|   21984 | 11398 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 11399 | `				if( iNest <= 0 ){` |
|       7 | 11400 | `					break;` |
|       - | 11401 | `				}` |
|      23 | 11402 | `			}` |
|   22071 | 11403 | `			pEnd2++;` |
|       5 | 11404 | `		}` |
|   21777 | 11405 | `		if( pEnd2 <pEnd ){` |
|       7 | 11406 | `			pEnd = pEnd2;` |
|       3 | 11407 | `		}` |
|   10886 | 11408 | `	}` |
|  922645 | 11409 | `	if( pEnd > pGen->pIn ){` |
|  922635 | 11410 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 11411 | `		/* Swap delimiter */` |
|  922635 | 11412 | `		pGen->pEnd = pEnd;` |
|       - | 11413 | `		/* Try to get an expression tree */` |
|  922635 | 11414 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  922635 | 11415 | `		if( rc == SXRET_OK && pRoot ){` |
|  922453 | 11416 | `			rc = SXRET_OK;` |
|  922453 | 11417 | `			if( xTreeValidator ){` |
|       - | 11418 | `				/* Call the upper layer validator callback */` |
|   29149 | 11419 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   14572 | 11420 | `			}` |
|  922453 | 11421 | `			if( rc != SXERR_ABORT ){` |
|       - | 11422 | `				/* Generate code for the given tree */` |
|  922453 | 11423 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 11424 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 11425 | `				 * expression so they short-circuit to its end. */` |
|  922453 | 11426 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  461224 | 11427 | `			}` |
|  922453 | 11428 | `			nExpr = 1;` |
|  461224 | 11429 | `		}` |
|       - | 11430 | `		/* Release the whole tree */` |
|  922635 | 11431 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 11432 | `		/* Synchronize token stream */` |
|  922635 | 11433 | `		pGen->pEnd = pTmp;` |
|  922635 | 11434 | `		pGen->pIn  = pEnd;` |
|  922635 | 11435 | `		if( rc == SXERR_ABORT ){` |
|      12 | 11436 | `			SySetRelease(&sExprNode);` |
|      12 | 11437 | `			return SXERR_ABORT;` |
|       - | 11438 | `		}` |
|  461310 | 11439 | `	}` |
|  922635 | 11440 | `	SySetRelease(&sExprNode);` |
|  922635 | 11441 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  461325 | 11442 |  |
|       - | 11443 | `/*` |
|       - | 11444 | ` * Return a pointer to the node construct handler associated` |
|       - | 11445 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 11446 | ` */` |
|  240864 | 11447 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 11448 |  |
|  240869 | 11449 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 11450 | `		/* Numeric literal: Either real or integer */` |
|  124879 | 11451 | `		return PH7_CompileNumLiteral;` |
|  115995 | 11452 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 11453 | `		/* Double quoted string */` |
|   23153 | 11454 | `		return PH7_CompileString;` |
|   92847 | 11455 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 11456 | `		/* Single quoted string */` |
|   92731 | 11457 | `		return PH7_CompileSimpleString;` |
|     120 | 11458 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 11459 | `		/* Heredoc */` |
|      68 | 11460 | `		return PH7_CompileHereDoc;` |
|      56 | 11461 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 11462 | `		/* Nowdoc */` |
|      50 | 11463 | `		return PH7_CompileNowDoc;` |
|       8 | 11464 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 11465 | `		/* Backtick quoted string */` |
|       6 | 11466 | `		return PH7_CompileBacktic;` |
|       - | 11467 | `	}` |
|       3 | 11468 | `	return 0;` |
|  120437 | 11469 |  |
|       - | 11470 | `/*` |
|       - | 11471 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 11472 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 11473 | ` * in write context" parse error.` |
|       - | 11474 | ` */` |
|    6842 | 11475 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 11476 |  |
|       - | 11477 | `	sxi32 rc;` |
|    6847 | 11478 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6845 | 11479 | `		return SXRET_OK;` |
|       - | 11480 | `	}` |
|       5 | 11481 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 11482 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 11483 | `		"Can't use nullsafe operator in write context");` |
|       3 | 11484 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3426 | 11485 |  |
|       - | 11486 | `/*` |
|       - | 11487 | ` * Compile an unset() statement.` |
|       - | 11488 | ` * unset($var, $arr[$key], ...);` |
|       - | 11489 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 11490 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 11491 | ` * parent array before extracting the element to unset.` |
|       - | 11492 | ` */` |
|    2960 | 11493 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 11494 |  |
|    2965 | 11495 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2965 | 11496 | `	sxu32 nIdx = 0;` |
|       - | 11497 | `	SyString sName;` |
|       - | 11498 | `	sxi32 rc;` |
|       - | 11499 | `	/* Jump the 'unset' keyword */` |
|    2965 | 11500 | `	pGen->pIn++;` |
|       - | 11501 | `	/* Save delimiter */` |
|    2965 | 11502 | `	pTmp = pGen->pEnd;` |
|       - | 11503 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2965 | 11504 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2965 | 11505 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 11506 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 11507 | `		SyToken *pClose;` |
|    2965 | 11508 | `		pGen->pIn++;   /* Skip '(' */` |
|    2965 | 11509 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2965 | 11510 | `		pEnd = pClose; /* Stop at ')' */` |
|    1480 | 11511 | `	}` |
|    2965 | 11512 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 11513 | `	/* Resolve the 'unset' builtin name once */` |
|    2965 | 11514 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     363 | 11515 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     363 | 11516 | `		if( pObj == 0 ){` |
|     ! 0 | 11517 | `			return SXERR_ABORT;` |
|       - | 11518 | `		}` |
|     363 | 11519 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     363 | 11520 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     179 | 11521 | `	}` |
|       - | 11522 | `	/* Compile each comma-separated argument */` |
|    9809 | 11523 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6849 | 11524 | `		if( pGen->pIn < pNext ){` |
|    6849 | 11525 | `			pGen->pEnd = pNext;` |
|    6849 | 11526 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 11527 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 11528 | `				GenStateUnsetValidator);` |
|    6849 | 11529 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11530 | `				return SXERR_ABORT;` |
|       - | 11531 | `			}` |
|    6849 | 11532 | `			if( rc != SXERR_EMPTY ){` |
|       - | 11533 | `				/* Emit call for this single argument */` |
|    6847 | 11534 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6847 | 11535 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6847 | 11536 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3421 | 11537 | `			}` |
|    3422 | 11538 | `		}` |
|       - | 11539 | `		/* Jump trailing commas */` |
|   10735 | 11540 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3891 | 11541 | `			pNext++;` |
|       5 | 11542 | `		}` |
|    6849 | 11543 | `		pGen->pIn = pNext;` |
|       5 | 11544 | `	}` |
|       - | 11545 | `	/* Skip past the closing ')' if present */` |
|    2965 | 11546 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2965 | 11547 | `		pGen->pIn++;` |
|    1480 | 11548 | `	}` |
|       - | 11549 | `	/* Restore token stream */` |
|    2965 | 11550 | `	pGen->pEnd = pTmp;` |
|    2965 | 11551 | `	return SXRET_OK;` |
|    1485 | 11552 |  |
|       - | 11553 | `/*` |
|       - | 11554 | ` * PHP Language construct table.` |
|       - | 11555 | ` */` |
|       - | 11556 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 11557 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 11558 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 11559 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 11560 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 11561 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 11562 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 11563 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 11564 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 11565 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 11566 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 11567 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 11568 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 11569 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 11570 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 11571 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 11572 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 11573 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 11574 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 11575 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 11576 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 11577 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 11578 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 11579 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 11580 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 11581 | `};` |
|       - | 11582 | `/*` |
|       - | 11583 | ` * Return a pointer to the statement handler routine associated` |
|       - | 11584 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 11585 | ` */` |
|  621384 | 11586 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 11587 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 11588 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 11589 | `	)` |
|       5 | 11590 |  |
|  621389 | 11591 | `	sxu32 n = 0;` |
| 3231162 | 11592 | `	for(;;){` |
| 6462329 | 11593 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  136009 | 11594 | `			break;` |
|       - | 11595 | `		}` |
| 6326325 | 11596 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  485385 | 11597 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 11598 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 11599 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 11600 | `					/* 'static' (class context),return null */` |
|     ! 0 | 11601 | `					return 0;` |
|       - | 11602 | `				}` |
|     ! 0 | 11603 | `			}` |
|  485380 | 11604 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       6 | 11605 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 11606 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 11607 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 11608 | `				return 0;` |
|       - | 11609 | `			}` |
|       - | 11610 | `			/* Return a pointer to the handler.` |
|       - | 11611 | `			*/` |
|  485385 | 11612 | `			return aLangConstruct[n].xConstruct;` |
|       - | 11613 | `		}` |
| 5840945 | 11614 | `		n++;` |
|       5 | 11615 | `	}` |
|  136009 | 11616 | `	if( pLookahed ){` |
|  136009 | 11617 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   39001 | 11618 | `			return PH7_CompileClassInterface;` |
|   97013 | 11619 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   96669 | 11620 | `			return PH7_CompileClass;` |
|     349 | 11621 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      65 | 11622 | `			return PH7_CompileTrait;` |
|       - | 11623 | `		}` |
|       - | 11624 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 11625 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 11626 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 11627 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     142 | 11628 | `	}` |
|       - | 11629 | `	/* Not a language construct */` |
|     289 | 11630 | `	return 0;` |
|  310697 | 11631 |  |
|       - | 11632 | `/*` |
|       - | 11633 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 11634 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 11635 | ` */` |
|     284 | 11636 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 11637 |  |
|       - | 11638 | `	int rc;` |
|     289 | 11639 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     289 | 11640 | `	if( rc == FALSE ){` |
|     174 | 11641 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     173 | 11642 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 11643 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 11644 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 11645 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 11646 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 11647 | `			*/` |
|       - | 11648 | `			){` |
|     171 | 11649 | `				rc = TRUE;` |
|      83 | 11650 | `		}` |
|      87 | 11651 | `	}` |
|     289 | 11652 | `	return rc;` |
|       5 | 11653 |  |
|       - | 11654 | `/*` |
|       - | 11655 | ` * Compile a PHP chunk.` |
|       - | 11656 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11657 | ` * takes care of generating the appropriate error message.` |
|       - | 11658 | ` */` |
|  742670 | 11659 | `static sxi32 GenStateCompileChunk(` |
|       - | 11660 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11661 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 11662 | `	)` |
|       5 | 11663 |  |
|       - | 11664 | `	ProcLangConstruct xCons;` |
|       - | 11665 | `	sxi32 rc;` |
|  742675 | 11666 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  585589 | 11667 | `	for(;;){` |
|  956929 | 11668 | `		int bStmtIsDeclare = 0;` |
|  956929 | 11669 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11670 | `			/* No more input to process */` |
|   14223 | 11671 | `			break;` |
|       - | 11672 | `		}` |
|       - | 11673 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 11674 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  942711 | 11675 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  624957 | 11676 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  624957 | 11677 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 11678 | `				bStmtIsDeclare = 1;` |
|      20 | 11679 | `			}` |
|  312476 | 11680 | `		}` |
|  942711 | 11681 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 11682 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 11683 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  214229 | 11684 | `			pGen->bStrictTypesLocked = 1;` |
|  107112 | 11685 | `		}` |
|  942711 | 11686 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11687 | `			/* Compile block */` |
|      20 | 11688 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      20 | 11689 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11690 | `				break;` |
|       - | 11691 | `			}` |
|      12 | 11692 | `		}else{` |
|  942695 | 11693 | `			xCons = 0;` |
|  942695 | 11694 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 11695 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 11696 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 11697 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3599 | 11698 | `				xCons = PH7_CompileClassModifiers;` |
|  940898 | 11699 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  621389 | 11700 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 11701 | `				/* Try to extract a language construct handler */` |
|  621389 | 11702 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  621389 | 11703 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 11704 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 11705 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 11706 | `						&pGen->pIn->sData);` |
|       9 | 11707 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11708 | `						break;` |
|       - | 11709 | `					}` |
|       - | 11710 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 11711 | `					 * this erroneous statement.` |
|       - | 11712 | `					 */` |
|       9 | 11713 | `					xCons = PH7_ErrorRecover;` |
|       4 | 11714 | `				}` |
|  628409 | 11715 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   52655 | 11716 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 11717 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 11718 | `				xCons = PH7_CompileLabel;` |
|      56 | 11719 | `			}` |
|  942695 | 11720 | `			if( xCons == 0 ){` |
|       - | 11721 | `				/* Assume an expression an try to compile it */` |
|  317881 | 11722 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  317881 | 11723 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 11724 | `					/* Pop l-value */` |
|  317731 | 11725 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  158863 | 11726 | `				}` |
|  158943 | 11727 | `			}else{` |
|       - | 11728 | `				/* Go compile the sucker */` |
|  624819 | 11729 | `				rc = xCons(&(*pGen));` |
|       - | 11730 | `			}` |
|  942695 | 11731 | `			if( rc == SXERR_ABORT ){` |
|       - | 11732 | `				/* Request to abort compilation */` |
|      12 | 11733 | `				break;` |
|       - | 11734 | `			}` |
|       - | 11735 | `		}` |
|       - | 11736 | `		/* Ignore trailing semi-colons ';' */` |
| 1517423 | 11737 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  574727 | 11738 | `			pGen->pIn++;` |
|       5 | 11739 | `		}` |
|  942701 | 11740 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 11741 | `			/* Compile a single statement and return */` |
|  728447 | 11742 | `			break;` |
|       - | 11743 | `		}` |
|       - | 11744 | `		/* LOOP ONE */` |
|       - | 11745 | `		/* LOOP TWO */` |
|       - | 11746 | `		/* LOOP THREE */` |
|       - | 11747 | `		/* LOOP FOUR */` |
|       5 | 11748 | `	}` |
|       - | 11749 | `	/* Return compilation status */` |
|  742675 | 11750 | `	return rc;` |
|       5 | 11751 |  |
|       - | 11752 | `/*` |
|       - | 11753 | ` * Compile a Raw PHP chunk.` |
|       - | 11754 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11755 | ` * takes care of generating the appropriate error message.` |
|       - | 11756 | ` */` |
|   14230 | 11757 | `static sxi32 PH7_CompilePHP(` |
|       - | 11758 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11759 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 11760 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 11761 | `	)` |
|       5 | 11762 |  |
|   14235 | 11763 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 11764 | `	sxi32 rc;` |
|       - | 11765 | `	/* Reset the token set */` |
|   14235 | 11766 | `	SySetReset(&(*pTokenSet));` |
|       - | 11767 | `	/* Mark as the default token set */` |
|   14235 | 11768 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 11769 | `	/* Advance the stream cursor */` |
|   14235 | 11770 | `	pGen->pRawIn++;` |
|       - | 11771 | `	/* Tokenize the PHP chunk first */` |
|   14235 | 11772 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 11773 | `	/* Point to the head and tail of the token stream. */` |
|   14235 | 11774 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   14235 | 11775 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   14235 | 11776 | `	if( is_expr ){` |
|     ! 0 | 11777 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 11778 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 11779 | `			/* A simple expression,compile it */` |
|     ! 0 | 11780 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 11781 | `		}` |
|       - | 11782 | `		/* Emit the DONE instruction */` |
|     ! 0 | 11783 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 11784 | `		return SXRET_OK;` |
|       - | 11785 | `	}` |
|   14235 | 11786 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 11787 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 11788 | `		/*` |
|       - | 11789 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 11790 | `		 * According to the PHP reference manual:` |
|       - | 11791 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 11792 | `		 *  immediately follow` |
|       - | 11793 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 11794 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 11795 | `		 * Symisc extension:` |
|       - | 11796 | `		 *   This short syntax works with all PHP opening` |
|       - | 11797 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 11798 | `		 *   only short tag.` |
|       - | 11799 | `		 */` |
|       - | 11800 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 11801 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 11802 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11803 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11804 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11805 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11806 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11807 | `		}` |
|       3 | 11808 | `		return SXRET_OK;` |
|       - | 11809 | `	}` |
|       - | 11810 | `	/* Compile the PHP chunk */` |
|   14233 | 11811 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11812 | `	/* Fix exceptions jumps */` |
|   14233 | 11813 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11814 | `	/* Fix gotos now, the jump destination is resolved */` |
|   14233 | 11815 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11816 | `		rc = SXERR_ABORT;` |
|       1 | 11817 | `	}` |
|       - | 11818 | `	/* Reset container */` |
|   14233 | 11819 | `	SySetReset(&pGen->aGoto);` |
|   14233 | 11820 | `	SySetReset(&pGen->aLabel);` |
|   14233 | 11821 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11822 | `	/* Compilation result */` |
|   14233 | 11823 | `	return rc;` |
|    7120 | 11824 |  |
|       - | 11825 | `/*` |
|       - | 11826 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11827 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11828 | ` * This is the only compile interface exported from this file.` |
|       - | 11829 | ` */` |
|   17172 | 11830 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11831 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11832 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11833 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11834 | `	)` |
|       5 | 11835 |  |
|       - | 11836 | `	SySet aPhpToken,aRawToken;` |
|       - | 11837 | `	ph7_gen_state *pCodeGen;` |
|       - | 11838 | `	ph7_value *pRawObj;` |
|       - | 11839 | `	sxu32 nObjIdx;` |
|       - | 11840 | `	sxi32 nRawObj;` |
|       - | 11841 | `	int is_expr;` |
|       - | 11842 | `	sxi8 bSavedStrict;` |
|       - | 11843 | `	sxi8 bSavedStrictLocked;` |
|       - | 11844 | `	sxi32 rc;` |
|   17177 | 11845 | `	if( pScript->nByte < 1 ){` |
|       - | 11846 | `		/* Nothing to compile */` |
|     ! 0 | 11847 | `		return PH7_OK;` |
|       - | 11848 | `	}` |
|       - | 11849 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11850 | `	 * file's flags so include/require restore them on return. */` |
|   17177 | 11851 | `	pCodeGen = &pVm->sCodeGen;` |
|   17177 | 11852 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   17177 | 11853 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   17177 | 11854 | `	pCodeGen->bStrictTypes = 0;` |
|   17177 | 11855 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11856 | `	/* Initialize the tokens containers */` |
|   17177 | 11857 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17177 | 11858 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17177 | 11859 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   17177 | 11860 | `	is_expr = 0;` |
|   17177 | 11861 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11862 | `		SyToken sTmp;` |
|       - | 11863 | `		/* PHP only: -*/` |
|    3611 | 11864 | `		sTmp.nLine = 1;` |
|    3611 | 11865 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3611 | 11866 | `		sTmp.pUserData = 0;` |
|    3611 | 11867 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3611 | 11868 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3611 | 11869 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11870 | `			/* A simple PHP expression */` |
|     ! 0 | 11871 | `			is_expr = 1;` |
|     ! 0 | 11872 | `		}` |
|    1808 | 11873 | `	}else{` |
|       - | 11874 | `		/* Tokenize raw text */` |
|   13571 | 11875 | `		SySetAlloc(&aRawToken,32);` |
|   13571 | 11876 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11877 | `	}` |
|       - | 11878 | `	/* Process high-level tokens */` |
|   17177 | 11879 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   17177 | 11880 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   17177 | 11881 | `	rc = PH7_OK;` |
|   17177 | 11882 | `	if( is_expr ){` |
|       - | 11883 | `		/* Compile the expression */` |
|     ! 0 | 11884 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 11885 | `		goto cleanup;` |
|       - | 11886 | `	}` |
|   17177 | 11887 | `	nObjIdx = 0;` |
|       - | 11888 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 11889 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 11890 | `	 * preventing namespace bleeding across include()d files. */` |
|   17177 | 11891 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 11892 | `	/* Start the compilation process */` |
|   15375 | 11893 | `	for(;;){` |
|   44973 | 11894 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   17165 | 11895 | `			break; /* No more tokens to process */` |
|       - | 11896 | `		}` |
|   27813 | 11897 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 11898 | `			/* Compile the PHP chunk */` |
|   14235 | 11899 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   14235 | 11900 | `			if( rc == SXERR_ABORT ){` |
|      15 | 11901 | `				break;` |
|       - | 11902 | `			}` |
|   14223 | 11903 | `			continue;` |
|       - | 11904 | `		}` |
|       - | 11905 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13583 | 11906 | `		nRawObj = 0;` |
|   27203 | 11907 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 11908 | `			/* Consume the raw chunk without any processing */` |
|   13625 | 11909 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13625 | 11910 | `			if( pRawObj == 0 ){` |
|     ! 0 | 11911 | `				rc = SXERR_MEM;` |
|     ! 0 | 11912 | `				break;` |
|       - | 11913 | `			}` |
|       - | 11914 | `			/* Mark as constant and emit the load constant instruction */` |
|   13625 | 11915 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13625 | 11916 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13625 | 11917 | `			++nRawObj;` |
|   13625 | 11918 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 11919 | `		}` |
|   13583 | 11920 | `		if( nRawObj > 0 ){` |
|       - | 11921 | `			/* Emit the consume instruction */` |
|   13583 | 11922 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6789 | 11923 | `		}` |
|    8591 | 11924 | `	}` |
|    8586 | 11925 | `cleanup:` |
|   17177 | 11926 | `	SySetRelease(&aRawToken);` |
|   17177 | 11927 | `	SySetRelease(&aPhpToken);` |
|       - | 11928 | `	/* Restore outer file's strict_types scope */` |
|   17177 | 11929 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   17177 | 11930 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   17177 | 11931 | `	return rc;` |
|    8591 | 11932 |  |
|       - | 11933 | `/*` |
|       - | 11934 | ` * Utility routines.Initialize the code generator.` |
|       - | 11935 | ` */` |
|    3538 | 11936 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 11937 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11938 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11939 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11940 | `	)` |
|       5 | 11941 |  |
|    3543 | 11942 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11943 | `	/* Zero the structure */` |
|    3543 | 11944 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 11945 | `	/* Initial state */` |
|    3543 | 11946 | `	pGen->pVm  = &(*pVm);` |
|    3543 | 11947 | `	pGen->xErr = xErr;` |
|    3543 | 11948 | `	pGen->pErrData = pErrData;` |
|    3543 | 11949 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3543 | 11950 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3543 | 11951 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3543 | 11952 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3543 | 11953 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 11954 | `	/* Error log buffer */` |
|    3543 | 11955 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 11956 | `	/* General purpose working buffer */` |
|    3543 | 11957 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 11958 | `	/* Namespace state */` |
|    3543 | 11959 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3543 | 11960 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3543 | 11961 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3543 | 11962 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11963 | `	/* Create the global scope */` |
|    3543 | 11964 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 11965 | `	/* Point to the global scope */` |
|    3543 | 11966 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3543 | 11967 | `	return SXRET_OK;` |
|       5 | 11968 |  |
|       - | 11969 | `/*` |
|       - | 11970 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 11971 | ` */` |
|   20364 | 11972 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 11973 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11974 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11975 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11976 | `	)` |
|       5 | 11977 |  |
|   20369 | 11978 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11979 | `	GenBlock *pBlock,*pParent;` |
|       - | 11980 | `	/* Reset state */` |
|   20369 | 11981 | `	SySetReset(&pGen->aLabel);` |
|   20369 | 11982 | `	SySetReset(&pGen->aGoto);` |
|   20369 | 11983 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   20369 | 11984 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   20369 | 11985 | `	SyBlobRelease(&pGen->sWorker);` |
|   20369 | 11986 | `	SyBlobRelease(&pGen->sNamespace);` |
|   20369 | 11987 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   20369 | 11988 | `	SyHashRelease(&pGen->hUseImports);` |
|   20369 | 11989 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   20369 | 11990 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   20369 | 11991 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   20369 | 11992 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   20369 | 11993 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11994 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 11995 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 11996 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 11997 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 11998 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 11999 | `	 * number of unique names, which is acceptable. */` |
|       - | 12000 | `	/* Point to the global scope */` |
|   20369 | 12001 | `	pBlock = pGen->pCurrent;` |
|   20369 | 12002 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 12003 | `		pParent = pBlock->pParent;` |
|     ! 0 | 12004 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 12005 | `		pBlock = pParent;` |
|     ! 0 | 12006 | `	}` |
|   20369 | 12007 | `	pGen->xErr = xErr;` |
|   20369 | 12008 | `	pGen->pErrData = pErrData;` |
|   20369 | 12009 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   20369 | 12010 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   20369 | 12011 | `	pGen->pIn = pGen->pEnd = 0;` |
|   20369 | 12012 | `	pGen->nErr = 0;` |
|   20369 | 12013 | `	return SXRET_OK;` |
|       5 | 12014 |  |
|       - | 12015 | `/*` |
|       - | 12016 | ` * Generate a compile-time error message.` |
|       - | 12017 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 12018 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 12019 | ` * abort compilation immediately.` |
|       - | 12020 | ` */` |
|     610 | 12021 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 12022 |  |
|     615 | 12023 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     615 | 12024 | `	const char *zErr = "Error";` |
|       - | 12025 | `	SyString *pFile;` |
|       - | 12026 | `	va_list ap;` |
|       - | 12027 | `	sxi32 rc;` |
|       - | 12028 | `	/* Reset the working buffer */` |
|     615 | 12029 | `	SyBlobReset(pWorker);` |
|       - | 12030 | `	/* Peek the processed file path if available */` |
|     615 | 12031 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     615 | 12032 | `	if( nErrType == E_ERROR ){` |
|       - | 12033 | `		/* Increment the error counter */` |
|     507 | 12034 | `		pGen->nErr++;` |
|     507 | 12035 | `		if( pGen->nErr > 15 ){` |
|       - | 12036 | `			/* Error count limit reached */` |
|       6 | 12037 | `			if( pGen->xErr ){` |
|       6 | 12038 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       6 | 12039 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       6 | 12040 | `				if( pFile ){` |
|       6 | 12041 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 12042 | `				}` |
|       6 | 12043 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       6 | 12044 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       6 | 12045 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12046 | `				}` |
|       2 | 12047 | `			}` |
|       - | 12048 | `			/* Abort immediately */` |
|       6 | 12049 | `			return SXERR_ABORT;` |
|       - | 12050 | `		}` |
|     249 | 12051 | `	}` |
|     611 | 12052 | `	if( pGen->xErr == 0 ){` |
|       - | 12053 | `		/* No available error consumer,return immediately */` |
|       3 | 12054 | `		return SXRET_OK;` |
|       - | 12055 | `	}` |
|     608 | 12056 | `	switch(nErrType){` |
|     500 | 12057 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 12058 | `	case E_WARNING: zErr = "Warning";     break;` |
|      78 | 12059 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      11 | 12060 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12061 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12062 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12063 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12064 | `	default:` |
|     ! 0 | 12065 | `		break;` |
|       - | 12066 | `	}` |
|     608 | 12067 | `	rc = SXRET_OK;` |
|       - | 12068 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     608 | 12069 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     608 | 12070 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     608 | 12071 | `	va_start(ap,zFormat);` |
|     608 | 12072 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     608 | 12073 | `	va_end(ap);` |
|     608 | 12074 | `	if( pFile ){` |
|     608 | 12075 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     302 | 12076 | `	}` |
|       - | 12077 | `	/* Append a new line */` |
|     608 | 12078 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     608 | 12079 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 12080 | `		/* Consume the generated error message */` |
|     608 | 12081 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     302 | 12082 | `	}` |
|     608 | 12083 | `	return rc;` |
|     310 | 12084 |  |
|       - | 12085 |  |
