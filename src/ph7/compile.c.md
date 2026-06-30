# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5689/7061 lines (80.57%)

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
|      59 |   124 | `	return SXERR_NOTFOUND;` |
|      79 |   125 |  |
|       - |   126 | `/*` |
|       - |   127 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|       - |   128 | ` * compiled blocks.` |
|       - |   129 | ` * Return a pointer to that block on success. NULL otherwise.` |
|       - |   130 | ` */` |
|    3822 |   131 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   132 |  |
|    3827 |   133 | `	GenBlock *pBlock = pCurrent;` |
|   10883 |   134 | `	for(;;){` |
|   21771 |   135 | `		if( pBlock->iFlags & iBlockType ){` |
|    3719 |   136 | `			iCount--; /* Decrement nesting level */` |
|    3719 |   137 | `			if( iCount < 1 ){` |
|       - |   138 | `				/* Block meet with the desired criteria */` |
|    3693 |   139 | `				return pBlock;` |
|       - |   140 | `			}` |
|      13 |   141 | `		}` |
|       - |   142 | `		/* Point to the upper block */` |
|   18083 |   143 | `		pBlock = pBlock->pParent;` |
|   18083 |   144 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   145 | `			/* Forbidden */` |
|      72 |   146 | `			break;` |
|       - |   147 | `		}` |
|       5 |   148 | `	}` |
|       - |   149 | `	/* No such block */` |
|     139 |   150 | `	return 0;` |
|    1916 |   151 |  |
|       - |   152 | `/*` |
|       - |   153 | ` * Initialize a freshly allocated block instance.` |
|       - |   154 | ` */` |
|  837384 |   155 | `static void GenStateInitBlock(` |
|       - |   156 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   157 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   158 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   159 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   160 | `	void *pUserData      /* Upper layer private data */` |
|       - |   161 | `	)` |
|       5 |   162 |  |
|       - |   163 | `	/* Initialize block fields */` |
|  837389 |   164 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  837389 |   165 | `	pBlock->pUserData   = pUserData;` |
|  837389 |   166 | `	pBlock->pGen        = pGen;` |
|  837389 |   167 | `	pBlock->iFlags      = iType;` |
|  837389 |   168 | `	pBlock->pParent     = 0;` |
|  837389 |   169 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  837389 |   170 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  837389 |   171 |  |
|       - |   172 | `/*` |
|       - |   173 | ` * Allocate a new block instance.` |
|       - |   174 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   175 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   176 | ` * processing on failure.` |
|       - |   177 | ` */` |
|  833840 |   178 | `static sxi32 GenStateEnterBlock(` |
|       - |   179 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   180 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   181 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   182 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   183 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   184 | `	)` |
|       5 |   185 |  |
|       - |   186 | `	GenBlock *pBlock;` |
|       - |   187 | `	/* Allocate a new block instance */` |
|  833845 |   188 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  833845 |   189 | `	if( pBlock == 0 ){` |
|       - |   190 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   191 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   192 | `		 */` |
|     ! 0 |   193 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   194 | `		/* Abort processing immediately */` |
|     ! 0 |   195 | `		return SXERR_ABORT;` |
|       - |   196 | `	}` |
|       - |   197 | `	/* Zero the structure */` |
|  833845 |   198 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  833845 |   199 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   200 | `	/* Link to the parent block */` |
|  833845 |   201 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   202 | `	/* Mark as the current block */` |
|  833845 |   203 | `	pGen->pCurrent = pBlock;` |
|  833845 |   204 | `	if( ppBlock ){` |
|       - |   205 | `		/* Write a pointer to the new instance */` |
|  405025 |   206 | `		*ppBlock = pBlock;` |
|  202510 |   207 | `	}` |
|  833845 |   208 | `	return SXRET_OK;` |
|  416925 |   209 |  |
|       - |   210 | `/*` |
|       - |   211 | ` * Release block fields without freeing the whole instance.` |
|       - |   212 | ` */` |
|  833832 |   213 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   214 |  |
|  833837 |   215 | `	SySetRelease(&pBlock->aPostContFix);` |
|  833837 |   216 | `	SySetRelease(&pBlock->aJumpFix);` |
|  833837 |   217 |  |
|       - |   218 | `/*` |
|       - |   219 | ` * Release a block.` |
|       - |   220 | ` */` |
|  833832 |   221 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   222 |  |
|  833837 |   223 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  833837 |   224 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   225 | `	/* Free the instance */` |
|  833837 |   226 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  833837 |   227 |  |
|       - |   228 | `/*` |
|       - |   229 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   230 | ` */` |
|  833832 |   231 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   232 |  |
|  833837 |   233 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  833837 |   234 | `	if( pBlock == 0 ){` |
|       - |   235 | `		/* No more block to pop */` |
|     ! 0 |   236 | `		return SXERR_EMPTY;` |
|       - |   237 | `	}` |
|       - |   238 | `	/* Point to the upper block */` |
|  833837 |   239 | `	pGen->pCurrent = pBlock->pParent;` |
|  833837 |   240 | `	if( ppBlock ){` |
|       - |   241 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   242 | `		*ppBlock = pBlock;` |
|     ! 0 |   243 | `	}else{` |
|       - |   244 | `		/* Safely release the block */` |
|  833837 |   245 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   246 | `	}` |
|  833837 |   247 | `	return SXRET_OK;` |
|  416921 |   248 |  |
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
|  240204 |   259 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   260 |  |
|       - |   261 | `	JumpFixup sJumpFix;` |
|       - |   262 | `	sxi32 rc;` |
|       - |   263 | `	/* Init the JumpFixup structure */` |
|  240209 |   264 | `	sJumpFix.nJumpType = nJumpType;` |
|  240209 |   265 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   266 | `	/* Insert in the jump fixup table */` |
|  240209 |   267 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  240209 |   268 | `	return rc;` |
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
|  582142 |   281 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   282 |  |
|       - |   283 | `	JumpFixup *aFix;` |
|       - |   284 | `	VmInstr *pInstr;` |
|       - |   285 | `	sxu32 nFixed;` |
|       - |   286 | `	sxu32 n;` |
|       - |   287 | `	/* Point to the jump fixup table */` |
|  582147 |   288 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   289 | `	/* Fix the desired jumps */` |
| 1051797 |   290 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  469655 |   291 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   292 | `			/* Already fixed */` |
|  185787 |   293 | `			continue;` |
|       - |   294 | `		}` |
|  283873 |   295 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   296 | `			/* Not of our interest */` |
|   43671 |   297 | `			continue;` |
|       - |   298 | `		}` |
|       - |   299 | `		/* Point to the instruction to fix */` |
|  240207 |   300 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  240207 |   301 | `		if( pInstr ){` |
|  240207 |   302 | `			pInstr->iP2 = nJumpDest;` |
|  240207 |   303 | `			nFixed++;` |
|       - |   304 | `			/* Mark as fixed */` |
|  240207 |   305 | `			aFix[n].nJumpType = -1;` |
|  120101 |   306 | `		}` |
|  120106 |   307 | `	}` |
|       - |   308 | `	/* Total number of fixed jumps */` |
|  582147 |   309 | `	return nFixed;` |
|       5 |   310 |  |
|       - |   311 | `/*` |
|       - |   312 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   313 | ` * The goto statement can be used to jump to another section` |
|       - |   314 | ` * in the program.` |
|       - |   315 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   316 | ` * statement for more information.` |
|       - |   317 | ` */` |
|  236376 |   318 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   319 |  |
|       - |   320 | `	JumpFixup *pJump,*aJumps;` |
|       - |   321 | `	Label *pLabel,*aLabel;` |
|       - |   322 | `	VmInstr *pInstr;` |
|       - |   323 | `	sxi32 rc;` |
|       - |   324 | `	sxu32 n;` |
|       - |   325 | `	/* Point to the goto table */` |
|  236381 |   326 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   327 | `	/* Fix */` |
|  236527 |   328 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|      10 |   342 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|      10 |   343 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   344 | `				return SXERR_ABORT;` |
|       - |   345 | `			}` |
|       4 |   346 | `		}` |
|       - |   347 | `		/* Fix the jump now the destination is resolved */` |
|      97 |   348 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      97 |   349 | `		if( pInstr ){` |
|      97 |   350 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   351 | `		}` |
|      51 |   352 | `	}` |
|  236379 |   353 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  236511 |   354 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   355 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   356 | `			/* Emit a warning */` |
|      40 |   357 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   358 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   359 | `		}` |
|      71 |   360 | `	}` |
|  236379 |   361 | `	return SXRET_OK;` |
|  118193 |   362 |  |
|       - |   363 | `/*` |
|       - |   364 | ` * Check if a given token value is installed in the literal table.` |
|       - |   365 | ` */` |
|  762138 |   366 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   367 |  |
|       - |   368 | `	SyHashEntry *pEntry;` |
|  762143 |   369 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  762143 |   370 | `	if( pEntry == 0 ){` |
|  343215 |   371 | `		return SXERR_NOTFOUND;` |
|       - |   372 | `	}` |
|  418933 |   373 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  418933 |   374 | `	return SXRET_OK;` |
|  381074 |   375 |  |
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
|  343210 |   386 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   387 |  |
|  343215 |   388 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  343215 |   389 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  171605 |   390 | `	}` |
|  343215 |   391 | `	return SXRET_OK;` |
|       5 |   392 |  |
|       - |   393 | `/*` |
|       - |   394 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   395 | ` * in the constant table.` |
|       - |   396 | ` */` |
|  124404 |   397 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   398 |  |
|       - |   399 | `	ph7_value *pObj;` |
|  124409 |   400 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   401 | `	/* Reserve a new constant */` |
|  124409 |   402 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  124409 |   403 | `	if( pObj == 0 ){` |
|     ! 0 |   404 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   405 | `		return 0;` |
|       - |   406 | `	}` |
|  124409 |   407 | `	*pIdx = nIdx;` |
|       - |   408 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   409 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   410 | `	 */` |
|  124409 |   411 | `	return pObj;` |
|   62207 |   412 |  |
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
|  475752 |   427 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   428 |  |
|       - |   429 | `	VmCallArgMap *pMap;` |
|  475757 |   430 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   431 | `	if( p3 == 0 ){` |
|      31 |   432 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   433 | `		if( pMap == 0 ) return 0;` |
|      31 |   434 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   435 | `		p3 = (void *)pMap;` |
|      14 |   436 | `	}` |
|      33 |   437 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   438 | `	return p3;` |
|  237881 |   439 |  |
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
|  125068 |   498 | `static int GenStateFindBadNumericSeparator(` |
|       - |   499 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   500 |  |
|  125073 |   501 | `	const char *z = pRaw->zString;` |
|  125073 |   502 | `	sxu32 n = pRaw->nByte;` |
|  125073 |   503 | `	int base = 10;` |
|       - |   504 | `	sxu32 i, start;` |
|  125073 |   505 | `	if( n < 2 ) return 0;` |
|   10385 |   506 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   507 | `		base = 16;` |
|   10350 |   508 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   509 | `		base = 2;` |
|     139 |   510 | `	}` |
|   37533 |   511 | `	for( i = 0; i < n; ++i ){` |
|   27167 |   512 | `		if( z[i] != '_' ) continue;` |
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
|   10371 |   529 | `	return 0;` |
|   62539 |   530 |  |
|       - |   531 | `/*` |
|       - |   532 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   533 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   534 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   535 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   536 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   537 | ` * so callers can bail from the current construct).` |
|       - |   538 | ` */` |
|  125068 |   539 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   540 |  |
|  125073 |   541 | `	const char *zBad = 0;` |
|  125073 |   542 | `	sxu32 nBad = 0;` |
|       - |   543 | `	SyString sBad;` |
|       - |   544 | `	sxi32 rc;` |
|  125073 |   545 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  125059 |   546 | `		return SXRET_OK;` |
|       - |   547 | `	}` |
|      18 |   548 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   549 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   550 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   551 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   552 | `		return SXERR_ABORT;` |
|       - |   553 | `	}` |
|      18 |   554 | `	return SXERR_SYNTAX;` |
|   62539 |   555 |  |
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
|  125054 |   572 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   573 | `	SyMemBackend *pAlloc,` |
|       - |   574 | `	const SyString *pToken,` |
|       - |   575 | `	char *zScratch, sxu32 nScratch,` |
|       - |   576 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   577 |  |
|       - |   578 | `	sxu32 i, j;` |
|  125059 |   579 | `	int hasUnderscore = 0;` |
|       - |   580 | `	char *zBuf;` |
|  125059 |   581 | `	*pzAlloc = 0;` |
|  264829 |   582 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  140027 |   583 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   69890 |   584 | `	}` |
|  125059 |   585 | `	if( !hasUnderscore ){` |
|  124807 |   586 | `		SyStringDupPtr(pOut, pToken);` |
|  124807 |   587 | `		return SXRET_OK;` |
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
|   62532 |   604 |  |
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
|  125040 |   621 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   622 |  |
|  125045 |   623 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  125045 |   624 | `	sxu32 nIdx = 0;` |
|       - |   625 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  125045 |   626 | `	char *zAlloc = 0;` |
|       - |   627 | `	SyString sNum;` |
|       - |   628 | `	sxi32 rc;` |
|   62520 |   629 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  125045 |   630 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  125045 |   631 | `	if( rc != SXRET_OK ){` |
|      14 |   632 | `		return rc;` |
|       - |   633 | `	}` |
|  187550 |   634 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   62515 |   635 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  125035 |   636 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   637 | `		return SXERR_ABORT;` |
|       - |   638 | `	}` |
|  125035 |   639 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   640 | `		ph7_value *pObj;` |
|       - |   641 | `		sxi64 iValue;` |
|  124409 |   642 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  124409 |   643 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  124409 |   644 | `		if( pObj == 0 ){` |
|     ! 0 |   645 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   646 | `			return SXERR_ABORT;` |
|       - |   647 | `		}` |
|  124409 |   648 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   62207 |   649 | `	}else{` |
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
|  125035 |   662 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   663 | `	/* Emit the load constant instruction */` |
|  125035 |   664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   665 | `	/* Node successfully compiled */` |
|  125035 |   666 | `	return SXRET_OK;` |
|   62525 |   667 |  |
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
|   99960 |   679 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   680 |  |
|   99965 |   681 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   682 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   683 | `	ph7_value *pObj;` |
|       - |   684 | `	sxu32 nIdx;` |
|   99965 |   685 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   686 | `	/* Delimit the string */` |
|   99965 |   687 | `	zIn  = pStr->zString;` |
|   99965 |   688 | `	zEnd = &zIn[pStr->nByte];` |
|   99965 |   689 | `	if( zIn >= zEnd ){` |
|       - |   690 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   691 | `		 * rather than reserving a new object each time. */` |
|    7259 |   692 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7259 |   693 | `		return SXRET_OK;` |
|       - |   694 | `	}` |
|   92711 |   695 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   696 | `		/* Already processed,emit the load constant instruction` |
|       - |   697 | `		 * and return.` |
|       - |   698 | `		 */` |
|   35861 |   699 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   35861 |   700 | `		return SXRET_OK;` |
|       - |   701 | `	}` |
|       - |   702 | `	/* Reserve a new constant */` |
|   56855 |   703 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   56855 |   704 | `	if( pObj == 0 ){` |
|     ! 0 |   705 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   706 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   707 | `		return SXERR_ABORT;` |
|       - |   708 | `	}` |
|   56855 |   709 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   710 | `	/* Compile the node */` |
|   56905 |   711 | `	for(;;){` |
|  113815 |   712 | `		if( zIn >= zEnd ){` |
|       - |   713 | `			/* End of input */` |
|   56855 |   714 | `			break;` |
|       - |   715 | `		}` |
|   56965 |   716 | `		zCur = zIn;` |
|  973413 |   717 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  916453 |   718 | `			zIn++;` |
|       5 |   719 | `		}` |
|   56965 |   720 | `		if( zIn > zCur ){` |
|       - |   721 | `			/* Append raw contents*/` |
|   56941 |   722 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   28468 |   723 | `		}` |
|   56965 |   724 | `		zIn++;` |
|   56965 |   725 | `		if( zIn < zEnd ){` |
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
|   56965 |   740 | `		zIn++;` |
|       5 |   741 | `	}` |
|       - |   742 | `	/* Emit the load constant instruction */` |
|   56855 |   743 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   56855 |   744 | `	if( pStr->nByte < 1024 ){` |
|       - |   745 | `		/* Install in the literal table */` |
|   56855 |   746 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   28425 |   747 | `	}` |
|       - |   748 | `	/* Node successfully compiled */` |
|   56855 |   749 | `	return SXRET_OK;` |
|   49985 |   750 |  |
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
|    2254 |   916 | `static sxi32 GenStateProcessStringExpression(` |
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
|    2259 |   927 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   928 | `	/* Preallocate some slots */` |
|    2259 |   929 | `	SySetAlloc(&sToken,0x08);` |
|       - |   930 | `	/* Tokenize the text */` |
|    2259 |   931 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   932 | `	/* Swap delimiter */` |
|    2259 |   933 | `	pTmpIn  = pGen->pIn;` |
|    2259 |   934 | `	pTmpEnd = pGen->pEnd;` |
|    2259 |   935 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2259 |   936 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   937 | `	/* Compile the expression */` |
|    2259 |   938 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   939 | `	/* Restore token stream */` |
|    2259 |   940 | `	pGen->pIn  = pTmpIn;` |
|    2259 |   941 | `	pGen->pEnd = pTmpEnd;` |
|       - |   942 | `	/* Release the token set */` |
|    2259 |   943 | `	SySetRelease(&sToken);` |
|       - |   944 | `	/* Compilation result */` |
|    2259 |   945 | `	return rc;` |
|       5 |   946 |  |
|       - |   947 | `/*` |
|       - |   948 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   949 | ` */` |
|   24976 |   950 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   951 |  |
|       - |   952 | `	ph7_value *pConstObj;` |
|   24981 |   953 | `	sxu32 nIdx = 0;` |
|       - |   954 | `	/* Reserve a new constant */` |
|   24981 |   955 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   24981 |   956 | `	if( pConstObj == 0 ){` |
|     ! 0 |   957 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   958 | `		return 0;` |
|       - |   959 | `	}` |
|   24981 |   960 | `	(*pCount)++;` |
|   24981 |   961 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   962 | `	/* Emit the load constant instruction */` |
|   24981 |   963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   24981 |   964 | `	return pConstObj;` |
|   12493 |   965 |  |
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
|   23496 |  1004 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1005 |  |
|   23501 |  1006 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1007 | `	const char *zIn,*zCur,*zEnd;` |
|   23501 |  1008 | `	ph7_value *pObj = 0;` |
|       - |  1009 | `	sxi32 iCons;` |
|       - |  1010 | `	sxi32 rc;` |
|       - |  1011 | `	/* Delimit the string */` |
|   23501 |  1012 | `	zIn  = pStr->zString;` |
|   23501 |  1013 | `	zEnd = &zIn[pStr->nByte];` |
|   23501 |  1014 | `	if( zIn >= zEnd ){` |
|       - |  1015 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1016 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1017 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1018 | `		 */` |
|     313 |  1019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     313 |  1020 | `		return SXRET_OK;` |
|       - |  1021 | `	}` |
|   23193 |  1022 | `	zCur = 0;` |
|       - |  1023 | `	/* Compile the node */` |
|   23193 |  1024 | `	iCons = 0;` |
|   12721 |  1025 | `	for(;;){` |
|   38039 |  1026 | `		zCur = zIn;` |
|  178085 |  1027 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  142305 |  1028 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1029 | `				break;` |
|  142181 |  1030 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2134 |  1031 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1068 |  1032 | `					break;` |
|       - |  1033 | `			}` |
|  140051 |  1034 | `			zIn++;` |
|       5 |  1035 | `		}` |
|   38039 |  1036 | `		if( zIn > zCur ){` |
|   17689 |  1037 | `			if( pObj == 0 ){` |
|   17215 |  1038 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   17215 |  1039 | `				if( pObj == 0 ){` |
|     ! 0 |  1040 | `					return SXERR_ABORT;` |
|       - |  1041 | `				}` |
|    8605 |  1042 | `			}` |
|   17689 |  1043 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    8842 |  1044 | `		}` |
|   38039 |  1045 | `		if( zIn >= zEnd ){` |
|   23193 |  1046 | `			break;` |
|       - |  1047 | `		}` |
|   14851 |  1048 | `		if( zIn[0] == '\\' ){` |
|   12597 |  1049 | `			const char *zPtr = 0;` |
|       - |  1050 | `			sxu32 n;` |
|   12597 |  1051 | `			zIn++;` |
|   12597 |  1052 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1053 | `				break;` |
|       - |  1054 | `			}` |
|   12597 |  1055 | `			if( pObj == 0 ){` |
|    7771 |  1056 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    7771 |  1057 | `				if( pObj == 0 ){` |
|     ! 0 |  1058 | `					return SXERR_ABORT;` |
|       - |  1059 | `				}` |
|    3883 |  1060 | `			}` |
|   12597 |  1061 | `			n = sizeof(char); /* size of conversion */` |
|   12597 |  1062 | `			switch( zIn[0] ){` |
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
|    5813 |  1083 | `			case 'n':` |
|       - |  1084 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   11631 |  1085 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   11631 |  1086 | `				break;` |
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
|   12597 |  1154 | `			zIn += n;` |
|   12597 |  1155 | `			continue;` |
|       - |  1156 | `		}` |
|    2259 |  1157 | `		if( zIn[0] == '{' ){` |
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
|    2131 |  1191 | `			const char *zExpr = zIn;` |
|       - |  1192 | `			/* Assemble variable name */` |
|    1073 |  1193 | `			for(;;){` |
|       - |  1194 | `				/* Jump leading dollars */` |
|    4277 |  1195 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2131 |  1196 | `					zIn++;` |
|       5 |  1197 | `				}` |
|    1073 |  1198 | `				for(;;){` |
|   11874 |  1199 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8655 |  1200 | `						zIn++;` |
|       5 |  1201 | `					}` |
|    2151 |  1202 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1203 | `						/* UTF-8 stream */` |
|     ! 0 |  1204 | `						zIn++;` |
|     ! 0 |  1205 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1206 | `							zIn++;` |
|     ! 0 |  1207 | `						}` |
|     ! 0 |  1208 | `						continue;` |
|       - |  1209 | `					}` |
|    2151 |  1210 | `					break;` |
|     ! 0 |  1211 | `				}` |
|    2151 |  1212 | `				if( zIn >= zEnd ){` |
|     211 |  1213 | `					break;` |
|       - |  1214 | `				}` |
|    1945 |  1215 | `				if( zIn[0] == '[' ){` |
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
|    1935 |  1233 | `				}else if(zIn[0] == '{' ){` |
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
|    1931 |  1251 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1252 | `					/* Member access operator '->' */` |
|      23 |  1253 | `					zIn += 2;` |
|    1921 |  1254 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1255 | `					/* Static member access operator '::' */` |
|     ! 0 |  1256 | `					zIn += 2;` |
|     ! 0 |  1257 | `				}else{` |
|     958 |  1258 | `					break;` |
|       - |  1259 | `				}` |
|       3 |  1260 | `			}` |
|       - |  1261 | `			/* Process the expression */` |
|    2131 |  1262 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2131 |  1263 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1264 | `				return SXERR_ABORT;` |
|       - |  1265 | `			}` |
|    2131 |  1266 | `			if( rc != SXERR_EMPTY ){` |
|    2129 |  1267 | `				++iCons;` |
|    1062 |  1268 | `			}` |
|       - |  1269 | `		}` |
|       - |  1270 | `		/* Invalidate the previously used constant */` |
|    2259 |  1271 | `		pObj = 0;` |
|       5 |  1272 | `	}/*for(;;)*/` |
|   23193 |  1273 | `	if( iCons > 1 ){` |
|       - |  1274 | `		/* Concatenate all compiled constants */` |
|    1675 |  1275 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     835 |  1276 | `	}` |
|       - |  1277 | `	/* Node successfully compiled */` |
|   23193 |  1278 | `	return SXRET_OK;` |
|   11753 |  1279 |  |
|       - |  1280 | `/*` |
|       - |  1281 | ` * Compile a double quoted string.` |
|       - |  1282 | ` *  See the block-comment above for more information.` |
|       - |  1283 | ` */` |
|   23436 |  1284 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1285 |  |
|       - |  1286 | `	sxi32 rc;` |
|   23441 |  1287 | `	rc = GenStateCompileString(&(*pGen));` |
|   11718 |  1288 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1289 | `	/* Compilation result */` |
|   23441 |  1290 | `	return rc;` |
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
|   21702 |  1334 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   21707 |  1345 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1346 | `	/* Compile the expression*/` |
|   21707 |  1347 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1348 | `	/* Restore token stream */` |
|   21707 |  1349 | `	RE_SWAP_DELIMITER(pGen);` |
|   21707 |  1350 | `	return rc;` |
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
|   24034 |  1391 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1392 |  |
|   24039 |  1393 | `	SyToken *pCur = pStart;` |
|   24039 |  1394 | `	sxi32 iNest = 0;` |
|   67999 |  1395 | `	while( pCur < pEnd ){` |
|   49429 |  1396 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5465 |  1397 | `			return pCur;` |
|       - |  1398 | `		}` |
|       - |  1399 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1400 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1401 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1402 | `		 */` |
|   43969 |  1403 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   43963 |  1464 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     326 |  1465 | `			iNest++;` |
|   43802 |  1466 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1467 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1468 | `			 * parser will shortly detect any syntax error. */` |
|     326 |  1469 | `			iNest--;` |
|     161 |  1470 | `		}` |
|   43963 |  1471 | `		pCur++;` |
|       5 |  1472 | `	}` |
|   18575 |  1473 | `	return pEnd;` |
|   12022 |  1474 |  |
|       - |  1475 | `/*` |
|       - |  1476 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1477 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1478 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1479 | ` */` |
|   31168 |  1480 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1481 |  |
|       - |  1482 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1483 | `	SyToken *pKey,*pCur;` |
|   31173 |  1484 | `	sxi32 iEmitRef = 0;` |
|   31173 |  1485 | `	sxi32 iSpread = 0;` |
|   31173 |  1486 | `	sxi32 nPair = 0;` |
|       - |  1487 | `	sxi32 rc;` |
|   31173 |  1488 | `	xValidator = 0;` |
|   25519 |  1489 | `	for(;;){` |
|       - |  1490 | `		/* Jump leading commas */` |
|   57907 |  1491 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    6869 |  1492 | `			pGen->pIn++;` |
|       5 |  1493 | `		}` |
|   51043 |  1494 | `		pCur = pGen->pIn;` |
|   51043 |  1495 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1496 | `			/* No more entry to process */` |
|   31157 |  1497 | `			break;` |
|       - |  1498 | `		}` |
|   19891 |  1499 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1500 | `			continue;` |
|       - |  1501 | `		}` |
|       - |  1502 | `		/* Compile the key if available */` |
|   19891 |  1503 | `		pKey = pCur;` |
|   19891 |  1504 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   19891 |  1505 | `		rc = SXERR_EMPTY;` |
|   19891 |  1506 | `		if( pCur < pGen->pIn ){` |
|    1641 |  1507 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1508 | `				/* Missing value */` |
|      13 |  1509 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1510 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1511 | `					return SXERR_ABORT;` |
|       - |  1512 | `				}` |
|      13 |  1513 | `				return SXRET_OK;` |
|       - |  1514 | `			}` |
|       - |  1515 | `			/* Compile the expression holding the key */` |
|    1631 |  1516 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1517 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1631 |  1518 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1519 | `				return SXERR_ABORT;` |
|       - |  1520 | `			}` |
|    1631 |  1521 | `			pCur++; /* Jump the '=>' operator */` |
|   19068 |  1522 | `		}else if( pKey == pCur ){` |
|       - |  1523 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1524 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1525 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1526 | `		}else{` |
|       - |  1527 | `			/* Reset back the cursor and point to the entry value */` |
|   18255 |  1528 | `			pCur = pKey;` |
|       - |  1529 | `		}` |
|   19881 |  1530 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1531 | `			/* No available key,load NULL */` |
|   18257 |  1532 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9126 |  1533 | `		}` |
|   19881 |  1534 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1535 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      44 |  1536 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      44 |  1537 | `			iEmitRef = 1;` |
|      44 |  1538 | `			pCur++; /* Jump the '&' token */` |
|      44 |  1539 | `			if( pCur >= pGen->pIn ){` |
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
|   19879 |  1553 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   19879 |  1554 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   19875 |  1567 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   19875 |  1568 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1569 | `			return SXERR_ABORT;` |
|       - |  1570 | `		}` |
|   19875 |  1571 | `		if( iSpread ){` |
|       - |  1572 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1573 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   19844 |  1574 | `		}else if( iEmitRef ){` |
|       - |  1575 | `			/* Emit the load reference instruction */` |
|      40 |  1576 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1577 | `		}` |
|   19875 |  1578 | `		xValidator = 0;` |
|   19875 |  1579 | `		iEmitRef = 0;` |
|   19875 |  1580 | `		iSpread = 0;` |
|   19875 |  1581 | `		nPair++;` |
|       5 |  1582 | `	}` |
|       - |  1583 | `	/* Emit the load map instruction */` |
|   31157 |  1584 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1585 | `	/* Node successfully compiled */` |
|   31157 |  1586 | `	return SXRET_OK;` |
|   15589 |  1587 |  |
|       - |  1588 | `/*` |
|       - |  1589 | ` * Compile the 'array' language construct.` |
|       - |  1590 | ` *	 According to the PHP language reference manual` |
|       - |  1591 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1592 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1593 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1594 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1595 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1596 | ` */` |
|   30160 |  1597 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1598 |  |
|       - |  1599 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   30165 |  1600 | `	pGen->pIn += 2;` |
|   30165 |  1601 | `	pGen->pEnd--;` |
|   15080 |  1602 | `	SXUNUSED(iCompileFlag);` |
|   30165 |  1603 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1604 |  |
|       - |  1605 | `/*` |
|       - |  1606 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1607 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1608 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1609 | ` */` |
|    1008 |  1610 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1611 |  |
|       - |  1612 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    1013 |  1613 | `	pGen->pIn++;` |
|    1013 |  1614 | `	pGen->pEnd--;` |
|     504 |  1615 | `	SXUNUSED(iCompileFlag);` |
|    1013 |  1616 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1617 |  |
|       - |  1618 | `/*` |
|       - |  1619 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1620 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1621 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1622 | ` * error message.` |
|       - |  1623 | ` * See the routine responible of compiling the list language construct` |
|       - |  1624 | ` * for more inforation.` |
|       - |  1625 | ` */` |
|     164 |  1626 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1627 |  |
|     168 |  1628 | `	sxi32 rc = SXRET_OK;` |
|     168 |  1629 | `	if( pRoot->pOp ){` |
|       4 |  1630 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       2 |  1631 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1632 | `				/* Unexpected expression */` |
|     ! 0 |  1633 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1634 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1635 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1636 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1637 | `				}` |
|       1 |  1638 | `		}` |
|     166 |  1639 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1640 | `		/* Unexpected expression */` |
|       6 |  1641 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1642 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1643 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1644 | `			rc = SXERR_INVALID;` |
|       2 |  1645 | `		}` |
|       2 |  1646 | `	}` |
|     168 |  1647 | `	return rc;` |
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
|      28 |  1681 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       2 |  1682 |  |
|       - |  1683 | `	SyToken *pNext;` |
|       - |  1684 | `	sxi32 rc;` |
|      66 |  1685 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - |  1686 | `		SyToken *pArrow,*pTarget;` |
|       - |  1687 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      38 |  1688 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      38 |  1689 | `		pTarget = &pArrow[1];` |
|      38 |  1690 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - |  1691 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - |  1692 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 |  1693 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1694 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1695 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1696 | `		}` |
|       - |  1697 | `		/* DUP the source array (it is on the stack top) */` |
|      38 |  1698 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1699 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      38 |  1700 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      38 |  1701 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1702 | `			return SXERR_ABORT;` |
|       - |  1703 | `		}` |
|       - |  1704 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - |  1705 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|       - |  1706 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|       - |  1707 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|       - |  1708 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|       - |  1709 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|      38 |  1710 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|      38 |  1711 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      34 |  1712 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      18 |  1713 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - |  1714 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - |  1715 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - |  1716 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 |  1717 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 |  1718 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 |  1719 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 |  1720 | `			pGen->pIn = pTarget;` |
|       5 |  1721 | `			pGen->pEnd = pNext;` |
|       5 |  1722 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 |  1723 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 |  1724 | `			pGen->pIn = pSavedIn;` |
|       5 |  1725 | `			pGen->pEnd = pSavedEnd;` |
|       5 |  1726 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1727 | `				return SXERR_ABORT;` |
|       - |  1728 | `			}` |
|       5 |  1729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 |  1730 | `		}else{` |
|       - |  1731 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|       - |  1732 | `			 * is already on the stack as the value; compiling the target appends` |
|       - |  1733 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|       - |  1734 | `			 * assignment does. */` |
|       - |  1735 | `			VmInstr *pInstr;` |
|      34 |  1736 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|      34 |  1737 | `			sxi32 iP1 = 0, iP2 = 0;` |
|      34 |  1738 | `			void *p3 = 0;` |
|      34 |  1739 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|       - |  1740 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      34 |  1741 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  1742 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1743 | `			}` |
|      34 |  1744 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      34 |  1745 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       3 |  1746 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|      33 |  1747 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 |  1748 | `					iVmOp = PH7_OP_STORE_IDX;` |
|       3 |  1749 | `					iP1 = pInstr->iP1;` |
|       3 |  1750 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 |  1751 | `				}else{` |
|      30 |  1752 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|      30 |  1753 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  1754 | `				}` |
|      16 |  1755 | `			}` |
|      34 |  1756 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - |  1757 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|       - |  1758 | `			 * source array is back on top for the next entry. */` |
|      34 |  1759 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       - |  1760 | `		}` |
|      38 |  1761 | `		pGen->pIn = &pNext[1];` |
|       2 |  1762 | `	}` |
|      30 |  1763 | `	return SXRET_OK;` |
|      16 |  1764 |  |
|       - |  1765 | `/*` |
|       - |  1766 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1767 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1768 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1769 | ` */` |
|     104 |  1770 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       4 |  1771 |  |
|       - |  1772 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1773 | `	SyToken *pNext;` |
|       - |  1774 | `	SyToken *pClassifyIn;` |
|     108 |  1775 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - |  1776 | `	sxi32 nExpr;` |
|       - |  1777 | `	sxi32 rc;` |
|       - |  1778 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - |  1779 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - |  1780 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - |  1781 | `	 * list. */` |
|     108 |  1782 | `	pClassifyIn = pGen->pIn;` |
|     302 |  1783 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     198 |  1784 | `		if( pGen->pIn >= pNext ){` |
|      13 |  1785 | `			nEmpty++;` |
|     192 |  1786 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      38 |  1787 | `			nKeyed++;` |
|      20 |  1788 | `		}else{` |
|     150 |  1789 | `			nPositional++;` |
|       - |  1790 | `		}` |
|     198 |  1791 | `		pGen->pIn = &pNext[1];` |
|       4 |  1792 | `	}` |
|     108 |  1793 | `	pGen->pIn = pClassifyIn;` |
|     108 |  1794 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 |  1795 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1796 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1797 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1798 | `	}` |
|     108 |  1799 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 |  1800 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1801 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 |  1802 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1803 | `	}` |
|     108 |  1804 | `	if( nKeyed > 0 ){` |
|      30 |  1805 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - |  1806 | `	}` |
|      80 |  1807 | `	nExpr = 0;` |
|      80 |  1808 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     238 |  1809 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     162 |  1810 | `		if( pGen->pIn < pNext ){` |
|       - |  1811 | `			/* Check for nested list() */` |
|     150 |  1812 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1813 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1814 | `				/* Record this nested list for post-processing */` |
|       3 |  1815 | `				SyToken *pListEnd = 0;` |
|       3 |  1816 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1817 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1818 | `				}` |
|       3 |  1819 | `				if( pListEnd ){` |
|       - |  1820 | `					struct NestedListEntry sEntry;` |
|       3 |  1821 | `					sEntry.nIndex = nExpr;` |
|       3 |  1822 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1823 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1824 | `					sEntry.isShort = 0;` |
|       3 |  1825 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1826 | `				}` |
|       - |  1827 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1828 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     149 |  1829 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1830 | `				/* Nested short destructuring [...] */` |
|      13 |  1831 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1832 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1833 | `				if( pBracketEnd ){` |
|       - |  1834 | `					struct NestedListEntry sEntry;` |
|      13 |  1835 | `					sEntry.nIndex = nExpr;` |
|      13 |  1836 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1837 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1838 | `					sEntry.isShort = 1;` |
|      13 |  1839 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1840 | `				}` |
|       - |  1841 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1842 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1843 | `			}else{` |
|       - |  1844 | `				/* Compile the expression holding the variable */` |
|     136 |  1845 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     136 |  1846 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1847 | `					SySetRelease(&sNested);` |
|     ! 0 |  1848 | `					return SXRET_OK;` |
|       - |  1849 | `				}` |
|       - |  1850 | `			}` |
|      77 |  1851 | `		}else{` |
|       - |  1852 | `			/* Empty entry,load NULL */` |
|      13 |  1853 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1854 | `		}` |
|     162 |  1855 | `		nExpr++;` |
|       - |  1856 | `		/* Advance the stream cursor */` |
|     162 |  1857 | `		pGen->pIn = &pNext[1];` |
|       4 |  1858 | `	}` |
|       - |  1859 | `	/* Emit the LOAD_LIST instruction */` |
|      80 |  1860 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1861 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1862 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1863 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1864 | `	 */` |
|      80 |  1865 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1866 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1867 | `		sxu32 i;` |
|      27 |  1868 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1869 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1870 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1871 | `			ph7_value *pIdx;` |
|       - |  1872 | `			sxu32 nConstIdx;` |
|       - |  1873 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1874 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1875 | `			/* Push the integer index for this nested entry */` |
|      15 |  1876 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1877 | `			if( pIdx == 0 ){` |
|     ! 0 |  1878 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1879 | `				SySetRelease(&sNested);` |
|     ! 0 |  1880 | `				return SXERR_ABORT;` |
|       - |  1881 | `			}` |
|      15 |  1882 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1883 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1884 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1885 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1886 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1887 | `			 */` |
|      15 |  1888 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1889 | `			/* Recursively compile the inner list */` |
|      15 |  1890 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1891 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1892 | `			if( apNested[i].isShort ){` |
|      13 |  1893 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1894 | `			}else{` |
|       3 |  1895 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1896 | `			}` |
|      15 |  1897 | `			pGen->pIn = pSavedIn;` |
|      15 |  1898 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1899 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1900 | `				SySetRelease(&sNested);` |
|     ! 0 |  1901 | `				return SXERR_ABORT;` |
|       - |  1902 | `			}` |
|       - |  1903 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1904 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1905 | `		}` |
|       6 |  1906 | `	}` |
|      80 |  1907 | `	SySetRelease(&sNested);` |
|       - |  1908 | `	/* Node successfully compiled */` |
|      80 |  1909 | `	return SXRET_OK;` |
|      56 |  1910 |  |
|      34 |  1911 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1912 |  |
|       - |  1913 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      36 |  1914 | `	pGen->pIn += 2;` |
|      36 |  1915 | `	pGen->pEnd--;` |
|      17 |  1916 | `	SXUNUSED(iCompileFlag);` |
|      36 |  1917 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1918 |  |
|      70 |  1919 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  1920 |  |
|       - |  1921 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      74 |  1922 | `	pGen->pIn++;` |
|      74 |  1923 | `	pGen->pEnd--;` |
|      35 |  1924 | `	SXUNUSED(iCompileFlag);` |
|      74 |  1925 | `	return GenStateCompileListBody(pGen);` |
|       4 |  1926 |  |
|       - |  1927 | `/* Forward declarations */` |
|       - |  1928 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  1929 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  1930 | `static int GenStateIsReadonly(SyToken *pTok);` |
|       - |  1931 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|       - |  1932 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|       - |  1933 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1934 | `/*` |
|       - |  1935 | ` * Compile an annoynmous function or a closure.` |
|       - |  1936 | ` * According to the PHP language reference` |
|       - |  1937 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1938 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1939 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1940 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1941 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1942 | ` *  Example Anonymous function variable assignment example` |
|       - |  1943 | ` * <?php` |
|       - |  1944 | ` * $greet = function($name)` |
|       - |  1945 | ` * {` |
|       - |  1946 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1947 | ` * };` |
|       - |  1948 | ` * $greet('World');` |
|       - |  1949 | ` * $greet('PHP');` |
|       - |  1950 | ` * ?>` |
|       - |  1951 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1952 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1953 | ` */` |
|     292 |  1954 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1955 |  |
|       - |  1956 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1957 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1958 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1959 | `							  * one thread is allowed to compile the script.` |
|       - |  1960 | `						      */` |
|       - |  1961 | `	SyString sName;` |
|       - |  1962 | `	sxu32 nLen;` |
|       - |  1963 | `	sxi32 rc;` |
|     146 |  1964 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1965 |  |
|     297 |  1966 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     297 |  1967 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1968 | `		pGen->pIn++;` |
|     ! 0 |  1969 | `	}` |
|       - |  1970 | `	/* Generate a unique name */` |
|     297 |  1971 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1972 | `	/* Make sure the generated name is unique */` |
|     297 |  1973 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1974 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1975 | `	}` |
|     297 |  1976 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  1977 | `	/* Compile the lambda body */` |
|     297 |  1978 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     297 |  1979 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1980 | `		return SXERR_ABORT;` |
|       - |  1981 | `	}` |
|       - |  1982 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  1983 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  1984 | `	 * the handler wraps either in a Closure instance. */` |
|     297 |  1985 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  1986 | `	/* Node successfully compiled */` |
|     297 |  1987 | `	return SXRET_OK;` |
|     151 |  1988 |  |
|       - |  1989 | `/*` |
|       - |  1990 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1991 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1992 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1993 | ` */` |
|     166 |  1994 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  1995 | `	ph7_gen_state *pGen,` |
|       - |  1996 | `	ph7_vm_func *pFunc,` |
|       - |  1997 | `	const char *zName,` |
|       - |  1998 | `	sxu32 nByte,` |
|       - |  1999 | `	SyString *aShadow,` |
|       - |  2000 | `	sxu32 nShadow)` |
|       2 |  2001 |  |
|       - |  2002 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2003 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2004 | `	sxu32 n, nEnv;` |
|       - |  2005 | `	char *zDup;` |
|     168 |  2006 | `	if( nByte == 0 ){` |
|     ! 0 |  2007 | `		return SXRET_OK;` |
|       - |  2008 | `	}` |
|     166 |  2009 | `	if( nByte == sizeof("this")-1` |
|      89 |  2010 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2011 | `		return SXRET_OK;` |
|       - |  2012 | `	}` |
|     202 |  2013 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     148 |  2014 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     145 |  2015 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     114 |  2016 | `			return SXRET_OK;` |
|       - |  2017 | `		}` |
|      19 |  2018 | `	}` |
|      53 |  2019 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  2020 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  2021 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  2022 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  2023 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  2024 | `			return SXRET_OK;` |
|       - |  2025 | `		}` |
|      15 |  2026 | `	}` |
|      53 |  2027 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  2028 | `	if( zDup == 0 ){` |
|     ! 0 |  2029 | `		return SXERR_ABORT;` |
|       - |  2030 | `	}` |
|      53 |  2031 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  2032 | `	sEnv.iFlags = 0;` |
|      53 |  2033 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  2034 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  2035 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  2036 | `	return SXRET_OK;` |
|      85 |  2037 |  |
|       - |  2038 | `/*` |
|       - |  2039 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2040 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2041 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2042 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2043 | ` */` |
|      20 |  2044 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2045 | `	ph7_gen_state *pGen,` |
|       - |  2046 | `	ph7_vm_func *pFunc,` |
|       - |  2047 | `	const char *zIn,` |
|       - |  2048 | `	const char *zEnd,` |
|       - |  2049 | `	SyString *aShadow,` |
|       - |  2050 | `	sxu32 nShadow)` |
|       1 |  2051 |  |
|       - |  2052 | `	sxi32 rc;` |
|     181 |  2053 | `	while( zIn < zEnd ){` |
|     161 |  2054 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  2055 | `			zIn++;` |
|     ! 0 |  2056 | `			if( zIn < zEnd ){` |
|     ! 0 |  2057 | `				zIn++;` |
|     ! 0 |  2058 | `			}` |
|     ! 0 |  2059 | `			continue;` |
|       - |  2060 | `		}` |
|     160 |  2061 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  2062 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  2063 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2064 | `			const char *zName;` |
|      13 |  2065 | `			zIn++; /* skip '$' */` |
|      13 |  2066 | `			zName = zIn;` |
|      39 |  2067 | `			while( zIn < zEnd ){` |
|      35 |  2068 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  2069 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2070 | `					zIn++;` |
|     ! 0 |  2071 | `					while( zIn < zEnd` |
|     ! 0 |  2072 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2073 | `						zIn++;` |
|     ! 0 |  2074 | `					}` |
|     ! 0 |  2075 | `					continue;` |
|       - |  2076 | `				}` |
|      35 |  2077 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  2078 | `					break;` |
|       - |  2079 | `				}` |
|      27 |  2080 | `				zIn++;` |
|       1 |  2081 | `			}` |
|      13 |  2082 | `			if( zIn > zName ){` |
|      19 |  2083 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  2084 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  2085 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2086 | `					return SXERR_ABORT;` |
|       - |  2087 | `				}` |
|       6 |  2088 | `			}` |
|      13 |  2089 | `			continue;` |
|       - |  2090 | `		}` |
|     149 |  2091 | `		zIn++;` |
|       1 |  2092 | `	}` |
|      21 |  2093 | `	return SXRET_OK;` |
|      11 |  2094 |  |
|       - |  2095 | `/*` |
|       - |  2096 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  2097 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  2098 | ` *   - plain $<id> pairs` |
|       - |  2099 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  2100 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  2101 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  2102 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  2103 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  2104 | ` *     are never mistakenly captured.` |
|       - |  2105 | ` */` |
|     162 |  2106 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2107 | `	ph7_gen_state *pGen,` |
|       - |  2108 | `	ph7_vm_func *pFunc,` |
|       - |  2109 | `	SyToken *pStart,` |
|       - |  2110 | `	SyToken *pEnd,` |
|       - |  2111 | `	SyString *aShadow,` |
|       - |  2112 | `	sxu32 nShadow)` |
|       2 |  2113 |  |
|     164 |  2114 | `	SyToken *pScan = pStart;` |
|       - |  2115 | `	sxi32 rc;` |
|     584 |  2116 | `	while( pScan < pEnd ){` |
|     422 |  2117 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      31 |  2118 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|      10 |  2119 | `				pScan->sData.zString,` |
|      20 |  2120 | `				pScan->sData.zString + pScan->sData.nByte,` |
|      10 |  2121 | `				aShadow,nShadow);` |
|      21 |  2122 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2123 | `				return SXERR_ABORT;` |
|       - |  2124 | `			}` |
|      21 |  2125 | `			pScan++;` |
|      21 |  2126 | `			continue;` |
|       - |  2127 | `		}` |
|     402 |  2128 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  2129 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  2130 | `			SyToken *pFnKw = pScan;` |
|      20 |  2131 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2132 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  2133 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2134 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2135 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2136 | `			}` |
|      21 |  2137 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2138 | `				SyToken *pInnerSigStart;` |
|       - |  2139 | `				SyToken *pInnerSigEnd;` |
|       - |  2140 | `				SyToken *pInnerBodyEnd;` |
|       - |  2141 | `				SyString *aInnerShadow;` |
|       - |  2142 | `				sxu32 nInnerShadow;` |
|       - |  2143 | `				sxu32 nInnerParamMax;` |
|       - |  2144 | `				SyToken *p;` |
|       - |  2145 | `				int iNestInner;` |
|      19 |  2146 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2147 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2148 | `					pScan++;` |
|     ! 0 |  2149 | `				}` |
|      19 |  2150 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2151 | `					pScan++;` |
|     ! 0 |  2152 | `					continue;` |
|       - |  2153 | `				}` |
|      19 |  2154 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2155 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2156 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2157 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2158 | `					pScan = pEnd;` |
|     ! 0 |  2159 | `					continue;` |
|       - |  2160 | `				}` |
|       - |  2161 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2162 | `				nInnerParamMax = 0;` |
|      57 |  2163 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2164 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2165 | `						nInnerParamMax++;` |
|       6 |  2166 | `					}` |
|      20 |  2167 | `				}` |
|      19 |  2168 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2169 | `					&pGen->pVm->sAllocator,` |
|      18 |  2170 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2171 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2172 | `					return SXERR_ABORT;` |
|       - |  2173 | `				}` |
|      19 |  2174 | `				nInnerShadow = 0;` |
|      25 |  2175 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2176 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2177 | `				}` |
|      57 |  2178 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2179 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2180 | `						continue;` |
|       - |  2181 | `					}` |
|      13 |  2182 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2183 | `						break;` |
|       - |  2184 | `					}` |
|      13 |  2185 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2186 | `						continue;` |
|       - |  2187 | `					}` |
|      13 |  2188 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2189 | `				}` |
|      19 |  2190 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2191 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2192 | `					pScan++;` |
|     ! 0 |  2193 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2194 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2195 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2196 | `						pScan++;` |
|     ! 0 |  2197 | `					}` |
|     ! 0 |  2198 | `					if( pScan < pEnd` |
|     ! 0 |  2199 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2200 | `						pScan++;` |
|     ! 0 |  2201 | `					}` |
|     ! 0 |  2202 | `				}` |
|      19 |  2203 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2204 | `					pScan++; /* past '=>' */` |
|       9 |  2205 | `				}` |
|      19 |  2206 | `				pInnerBodyEnd = pScan;` |
|      19 |  2207 | `				iNestInner = 0;` |
|     131 |  2208 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2209 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2210 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2211 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2212 | `						break;` |
|       - |  2213 | `					}` |
|     113 |  2214 | `					if( pInnerBodyEnd->nType &` |
|       - |  2215 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2216 | `						iNestInner++;` |
|     112 |  2217 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2218 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2219 | `						iNestInner--;` |
|       1 |  2220 | `					}` |
|     113 |  2221 | `					pInnerBodyEnd++;` |
|       1 |  2222 | `				}` |
|       - |  2223 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2224 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2225 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2226 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2227 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2228 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2229 | `				 *` |
|       - |  2230 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2231 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2232 | `				 * range after the '=' sign. */` |
|       - |  2233 | `				{` |
|      19 |  2234 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2235 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2236 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2237 | `						SyToken *pEq = 0;` |
|      13 |  2238 | `						int iNestArg = 0;` |
|      49 |  2239 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2240 | `							if( iNestArg == 0` |
|      39 |  2241 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2242 | `								break;` |
|       - |  2243 | `							}` |
|      37 |  2244 | `							if( pArgEnd->nType &` |
|       - |  2245 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2246 | `								iNestArg++;` |
|      37 |  2247 | `							}else if( pArgEnd->nType &` |
|       - |  2248 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2249 | `								iNestArg--;` |
|     ! 0 |  2250 | `							}` |
|      36 |  2251 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2252 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2253 | `								pEq = pArgEnd;` |
|       3 |  2254 | `							}` |
|      37 |  2255 | `							pArgEnd++;` |
|       1 |  2256 | `						}` |
|      13 |  2257 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2258 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2259 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2260 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2261 | `								return SXERR_ABORT;` |
|       - |  2262 | `							}` |
|       3 |  2263 | `						}` |
|      13 |  2264 | `						pArgStart = pArgEnd;` |
|      12 |  2265 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2266 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2267 | `							pArgStart++;` |
|       1 |  2268 | `						}` |
|       1 |  2269 | `					}` |
|       - |  2270 | `				}` |
|      28 |  2271 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2272 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2273 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2274 | `					return SXERR_ABORT;` |
|       - |  2275 | `				}` |
|      19 |  2276 | `				pScan = pInnerBodyEnd;` |
|      19 |  2277 | `				continue;` |
|       - |  2278 | `			}` |
|       1 |  2279 | `		}` |
|     384 |  2280 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     230 |  2281 | `			pScan++;` |
|     230 |  2282 | `			continue;` |
|       - |  2283 | `		}` |
|       - |  2284 | `		{` |
|       - |  2285 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     156 |  2286 | `			SyToken *pDollar = pScan;` |
|     231 |  2287 | `			while( &pDollar[1] < pEnd` |
|     156 |  2288 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2289 | `				pDollar++;` |
|     ! 0 |  2290 | `			}` |
|     156 |  2291 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2292 | `				break;` |
|       - |  2293 | `			}` |
|     156 |  2294 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2295 | `				pScan = pDollar + 1;` |
|     ! 0 |  2296 | `				continue;` |
|       - |  2297 | `			}` |
|     233 |  2298 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     154 |  2299 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      77 |  2300 | `				aShadow,nShadow);` |
|     156 |  2301 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2302 | `				return SXERR_ABORT;` |
|       - |  2303 | `			}` |
|     156 |  2304 | `			pScan = pDollar + 2;` |
|       - |  2305 | `		}` |
|       2 |  2306 | `	}` |
|     164 |  2307 | `	return SXRET_OK;` |
|      83 |  2308 |  |
|       - |  2309 | `/*` |
|       - |  2310 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2311 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2312 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2313 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2314 | ` * $this is also made available.` |
|       - |  2315 | ` */` |
|     144 |  2316 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2317 |  |
|       - |  2318 | `	ph7_vm_func *pFunc;` |
|       - |  2319 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2320 | `	GenBlock *pBlock;` |
|       - |  2321 | `	SySet *pInstrContainer;` |
|       - |  2322 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2323 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2324 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2325 | `	SyToken *pSavedEnd;` |
|       - |  2326 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2327 | `	char zName[512];` |
|       - |  2328 | `	static int iCnt = 1;` |
|       - |  2329 | `	char *zDup;` |
|       - |  2330 | `	sxu32 nLen;` |
|       - |  2331 | `	sxu32 nLine;` |
|     148 |  2332 | `	sxi32 iFlags = 0;` |
|     148 |  2333 | `	int bStatic = 0;` |
|       - |  2334 | `	sxi32 rc;` |
|       - |  2335 | `	sxu32 n;` |
|      72 |  2336 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2337 |  |
|     148 |  2338 | `	nLine = pGen->pIn->nLine;` |
|       - |  2339 | `	/* Optional 'static' prefix */` |
|     144 |  2340 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     148 |  2341 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2342 | `		bStatic = 1;` |
|       3 |  2343 | `		pGen->pIn++;` |
|       1 |  2344 | `	}` |
|       - |  2345 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     144 |  2346 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     148 |  2347 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2348 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2349 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2350 | `		return SXERR_SYNTAX;` |
|       - |  2351 | `	}` |
|     148 |  2352 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2353 | `	/* Optional '&' — return by reference */` |
|     148 |  2354 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2355 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2356 | `		pGen->pIn++;` |
|     ! 0 |  2357 | `	}` |
|       - |  2358 | `	/* Expect '(' */` |
|     148 |  2359 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2360 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2361 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2362 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2363 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2364 | `		}else{` |
|     ! 0 |  2365 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2366 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2367 | `		}` |
|       3 |  2368 | `		return SXERR_SYNTAX;` |
|       - |  2369 | `	}` |
|     146 |  2370 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2371 | `	/* Delimit the parameter list */` |
|     146 |  2372 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     146 |  2373 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2374 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2375 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2376 | `		return SXERR_SYNTAX;` |
|       - |  2377 | `	}` |
|       - |  2378 | `	/* Allocate the function state */` |
|     143 |  2379 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     143 |  2380 | `	if( pFunc == 0 ){` |
|     ! 0 |  2381 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2382 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2383 | `		return SXERR_ABORT;` |
|       - |  2384 | `	}` |
|       - |  2385 | `	/* Generate a unique lambda name */` |
|     143 |  2386 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     245 |  2387 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     104 |  2388 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2389 | `	}` |
|     143 |  2390 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     143 |  2391 | `	if( zDup == 0 ){` |
|     ! 0 |  2392 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2393 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2394 | `		return SXERR_ABORT;` |
|       - |  2395 | `	}` |
|     143 |  2396 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2397 | `	/* Collect function arguments */` |
|     143 |  2398 | `	if( pGen->pIn < pSigEnd ){` |
|     101 |  2399 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     101 |  2400 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2401 | `			return SXERR_ABORT;` |
|       - |  2402 | `		}` |
|      49 |  2403 | `	}` |
|       - |  2404 | `	/* Point past ')' and parse optional return type */` |
|     143 |  2405 | `	pGen->pIn = &pSigEnd[1];` |
|     143 |  2406 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     143 |  2407 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2408 | `		return SXERR_ABORT;` |
|     143 |  2409 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2410 | `		return SXERR_SYNTAX;` |
|       - |  2411 | `	}` |
|       - |  2412 | `	/* Expect '=>' */` |
|     143 |  2413 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2414 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2415 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2416 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2417 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2418 | `		}else{` |
|     ! 0 |  2419 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2420 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2421 | `		}` |
|       3 |  2422 | `		return SXERR_SYNTAX;` |
|       - |  2423 | `	}` |
|     140 |  2424 | `	pGen->pIn++; /* Jump '=>' */` |
|     140 |  2425 | `	pBodyStart = pGen->pIn;` |
|     140 |  2426 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2427 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2428 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2429 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2430 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     140 |  2431 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2432 | `	{` |
|     140 |  2433 | `		SyString *aShadow = 0;` |
|     140 |  2434 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     140 |  2435 | `		if( nShadow > 0 ){` |
|      98 |  2436 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      96 |  2437 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      98 |  2438 | `			if( aShadow == 0 ){` |
|     ! 0 |  2439 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2440 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2441 | `				return SXERR_ABORT;` |
|       - |  2442 | `			}` |
|     216 |  2443 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     120 |  2444 | `				aShadow[n] = aArgs[n].sName;` |
|      61 |  2445 | `			}` |
|      48 |  2446 | `		}` |
|     209 |  2447 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      69 |  2448 | `			aShadow,nShadow);` |
|     140 |  2449 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2450 | `			return SXERR_ABORT;` |
|       - |  2451 | `		}` |
|       - |  2452 | `	}` |
|       - |  2453 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2454 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2455 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2456 | `	 * $this. */` |
|     140 |  2457 | `	if( !bStatic ){` |
|       - |  2458 | `		char *zThisDup;` |
|     138 |  2459 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     138 |  2460 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2461 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2462 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2463 | `			return SXERR_ABORT;` |
|       - |  2464 | `		}` |
|     138 |  2465 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     138 |  2466 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     138 |  2467 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     138 |  2468 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     138 |  2469 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      68 |  2470 | `	}` |
|       - |  2471 | `	/* Arrow functions are always closures */` |
|     140 |  2472 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2473 | `	/* Compile the body expression as an implicit return */` |
|     209 |  2474 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      69 |  2475 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     140 |  2476 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2477 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2478 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2479 | `		return SXERR_ABORT;` |
|       - |  2480 | `	}` |
|     140 |  2481 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     140 |  2482 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     140 |  2483 | `	pSavedEnd = pGen->pEnd;` |
|     140 |  2484 | `	pGen->pIn = pBodyStart;` |
|     140 |  2485 | `	pGen->pEnd = pBodyEnd;` |
|     140 |  2486 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     140 |  2487 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2488 | `		return SXERR_ABORT;` |
|       - |  2489 | `	}` |
|       - |  2490 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2491 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2492 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2493 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     140 |  2494 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     140 |  2495 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     140 |  2496 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     140 |  2497 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     140 |  2498 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2499 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     140 |  2500 | `	pGen->pIn = pBodyEnd;` |
|     140 |  2501 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2502 | `	/* Emit the load-closure instruction */` |
|     140 |  2503 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     140 |  2504 | `	return SXRET_OK;` |
|      76 |  2505 |  |
|       - |  2506 | `/*` |
|       - |  2507 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2508 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2509 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2510 | ` * expression's value.` |
|       - |  2511 | ` */` |
|     346 |  2512 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2513 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2514 |  |
|       - |  2515 | `	SySet *pInstrContainer;` |
|       - |  2516 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2517 | `	GenBlock *pArmBlock;` |
|       - |  2518 | `	sxi32 rc;` |
|     349 |  2519 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2520 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2521 | `	pGen->pIn  = pStart;` |
|     349 |  2522 | `	pGen->pEnd = pStop;` |
|     349 |  2523 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2524 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2525 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2526 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2527 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2528 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2529 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2530 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2531 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2532 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2533 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2534 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2535 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2536 | `		return SXERR_ABORT;` |
|       - |  2537 | `	}` |
|     349 |  2538 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2539 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2540 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2541 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2542 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2543 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2544 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2545 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2546 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2547 | `		return SXERR_ABORT;` |
|       - |  2548 | `	}` |
|     349 |  2549 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2550 | `		return SXERR_EMPTY;` |
|       - |  2551 | `	}` |
|     349 |  2552 | `	return SXRET_OK;` |
|     176 |  2553 |  |
|       - |  2554 | `/*` |
|       - |  2555 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2556 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2557 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2558 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2559 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2560 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2561 | ` */` |
|       - |  2562 | `/*` |
|       - |  2563 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2564 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2565 | ` * caller can bail out of the current expression.` |
|       - |  2566 | ` */` |
|       2 |  2567 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2568 |  |
|       - |  2569 | `	va_list ap;` |
|       - |  2570 | `	sxi32 rc;` |
|       - |  2571 | `	SyBlob sMsg;` |
|       3 |  2572 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2573 | `	va_start(ap,zFmt);` |
|       3 |  2574 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2575 | `	va_end(ap);` |
|       3 |  2576 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2577 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2578 | `	SyBlobRelease(&sMsg);` |
|       3 |  2579 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2580 | `		return SXERR_ABORT;` |
|       - |  2581 | `	}` |
|       3 |  2582 | `	return SXERR_SYNTAX;` |
|       2 |  2583 |  |
|       - |  2584 | `/*` |
|       - |  2585 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2586 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2587 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2588 | ` */` |
|     348 |  2589 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2590 |  |
|     352 |  2591 | `	SyToken *pCur = pStart;` |
|     352 |  2592 | `	int iNest = 0;` |
|     814 |  2593 | `	while( pCur < pEnd ){` |
|     780 |  2594 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2595 | `			iNest++;` |
|     774 |  2596 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2597 | `			iNest--;` |
|     762 |  2598 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2599 | `			return pCur;` |
|       - |  2600 | `		}` |
|     466 |  2601 | `		pCur++;` |
|       4 |  2602 | `	}` |
|      37 |  2603 | `	return pEnd;` |
|     178 |  2604 |  |
|      70 |  2605 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2606 |  |
|       - |  2607 | `	ph7_match *pMatch;` |
|       - |  2608 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2609 | `	int bHasDefault = 0;` |
|       - |  2610 | `	sxu32 nLine;` |
|       - |  2611 | `	sxi32 rc;` |
|      35 |  2612 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2613 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2614 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2615 | `	/* Expect '(' */` |
|      75 |  2616 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2617 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2618 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2619 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2620 | `	}` |
|      75 |  2621 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2622 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2623 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2624 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2625 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2626 | `	}` |
|      75 |  2627 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2628 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2629 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2630 | `	}` |
|       - |  2631 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2632 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2633 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2634 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2635 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2636 | `		return SXERR_ABORT;` |
|       - |  2637 | `	}` |
|      75 |  2638 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2639 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2640 | `	/* Expect '{' */` |
|      75 |  2641 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2642 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2643 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2644 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2645 | `	}` |
|      75 |  2646 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2647 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2648 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2649 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2650 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2651 | `	}` |
|       - |  2652 | `	/* Allocate ph7_match container */` |
|      75 |  2653 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2654 | `	if( pMatch == 0 ){` |
|     ! 0 |  2655 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2656 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2657 | `		return SXERR_ABORT;` |
|       - |  2658 | `	}` |
|      75 |  2659 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2660 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2661 | `	/* Iterate arms */` |
|     253 |  2662 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2663 | `		ph7_match_arm sArm;` |
|       - |  2664 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2665 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2666 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2667 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2668 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2669 | `		/* 'default' arm? */` |
|     182 |  2670 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2671 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2672 | `			if( bHasDefault ){` |
|       3 |  2673 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2674 | `					"Match expressions may only contain one default arm");` |
|       4 |  2675 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2676 | `			}` |
|      20 |  2677 | `			sArm.bDefault = 1;` |
|      20 |  2678 | `			bHasDefault = 1;` |
|      20 |  2679 | `			pGen->pIn++;` |
|      20 |  2680 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2681 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2682 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2683 | `			}` |
|      20 |  2684 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2685 | `		}else{` |
|       - |  2686 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2687 | `			pCondStart = pGen->pIn;` |
|     166 |  2688 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2689 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2690 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2691 | `				SySet sCondBc;` |
|       9 |  2692 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2693 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2694 | `						"syntax error, empty match condition expression");` |
|       - |  2695 | `				}` |
|       9 |  2696 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2697 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2698 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2699 | `					return SXERR_ABORT;` |
|       - |  2700 | `				}` |
|       9 |  2701 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2702 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2703 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2704 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2705 | `			}` |
|     166 |  2706 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2707 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2708 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2709 | `			}` |
|     163 |  2710 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2711 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2712 | `					"syntax error, empty match condition expression");` |
|       - |  2713 | `			}` |
|       - |  2714 | `			{` |
|       - |  2715 | `				SySet sCondBc;` |
|     163 |  2716 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2717 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2718 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2719 | `					return SXERR_ABORT;` |
|       - |  2720 | `				}` |
|     163 |  2721 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2722 | `			}` |
|     163 |  2723 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2724 | `		}` |
|       - |  2725 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2726 | `		pResStart = pGen->pIn;` |
|     181 |  2727 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2728 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2729 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2730 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2731 | `		}` |
|     181 |  2732 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2733 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2734 | `			return SXERR_ABORT;` |
|       - |  2735 | `		}` |
|     181 |  2736 | `		pGen->pIn = pResEnd;` |
|     181 |  2737 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2738 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2739 | `		}` |
|     181 |  2740 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2741 | `	}` |
|      69 |  2742 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2743 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2744 | `	return SXRET_OK;` |
|      40 |  2745 |  |
|       - |  2746 | `/*` |
|       - |  2747 | ` * Compile a backtick quoted string.` |
|       - |  2748 | ` */` |
|       4 |  2749 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2750 |  |
|       - |  2751 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2752 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2753 | `	 */` |
|       8 |  2754 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2755 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2756 | `		ph7_lib_version()` |
|       - |  2757 | `		);` |
|       - |  2758 | `	/* Load NULL */` |
|       6 |  2759 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2760 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2761 | `	/* Node successfully compiled */` |
|       6 |  2762 | `	return SXRET_OK;` |
|       2 |  2763 |  |
|       - |  2764 | `/*` |
|       - |  2765 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2766 | ` * construct.` |
|       - |  2767 | ` */` |
|      80 |  2768 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2769 |  |
|       - |  2770 | `	SyString *pName;` |
|       - |  2771 | `	sxu32 nKeyID;` |
|       - |  2772 | `	sxi32 rc;` |
|       - |  2773 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      85 |  2774 | `	pName = &pGen->pIn->sData;` |
|      85 |  2775 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  2776 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      85 |  2777 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2778 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2779 | `		/* Compile arguments one after one */` |
|       9 |  2780 | `		pTmp = pGen->pEnd;` |
|       - |  2781 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2782 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2783 | `		 *  mean that the following expression is valid:` |
|       - |  2784 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2785 | `		 */` |
|       9 |  2786 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2787 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2788 | `			if( pGen->pIn < pNext ){` |
|       9 |  2789 | `				pGen->pEnd = pNext;` |
|       9 |  2790 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2791 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2792 | `					return SXERR_ABORT;` |
|       - |  2793 | `				}` |
|       9 |  2794 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2795 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2796 | `					 * without the overhead of a function call.` |
|       - |  2797 | `					 * This is a very powerful optimization that improve` |
|       - |  2798 | `					 * performance greatly.` |
|       - |  2799 | `					 */` |
|       9 |  2800 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2801 | `				}` |
|       4 |  2802 | `			}` |
|       - |  2803 | `			/* Jump trailing commas */` |
|       9 |  2804 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2805 | `				pNext++;` |
|     ! 0 |  2806 | `			}` |
|       9 |  2807 | `			pGen->pIn = pNext;` |
|       1 |  2808 | `		}` |
|       - |  2809 | `		/* Restore token stream */` |
|       9 |  2810 | `		pGen->pEnd = pTmp;` |
|       5 |  2811 | `	}else{` |
|      77 |  2812 | `		sxi32 nArg = 0;` |
|      77 |  2813 | `		sxu32 nIdx = 0;` |
|      77 |  2814 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      77 |  2815 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2816 | `			return SXERR_ABORT;` |
|      77 |  2817 | `		}else if(rc != SXERR_EMPTY ){` |
|      77 |  2818 | `			nArg = 1;` |
|      36 |  2819 | `		}` |
|      77 |  2820 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2821 | `			ph7_value *pObj;` |
|       - |  2822 | `			/* Emit the call instruction */` |
|      29 |  2823 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  2824 | `			if( pObj == 0 ){` |
|     ! 0 |  2825 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2826 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2827 | `				return SXERR_ABORT;` |
|       - |  2828 | `			}` |
|      29 |  2829 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2830 | `			/* Install in the literal table */` |
|      29 |  2831 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      12 |  2832 | `		}` |
|       - |  2833 | `		/* Emit the call instruction */` |
|      77 |  2834 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      77 |  2835 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2836 | `	}` |
|       - |  2837 | `	/* Node successfully compiled */` |
|      85 |  2838 | `	return SXRET_OK;` |
|      45 |  2839 |  |
|       - |  2840 | `/*` |
|       - |  2841 | ` * Compile a node holding a variable declaration.` |
|       - |  2842 | ` * According to the PHP language reference` |
|       - |  2843 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2844 | ` *  The variable name is case-sensitive.` |
|       - |  2845 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2846 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2847 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2848 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2849 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2850 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2851 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2852 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2853 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2854 | ` *  the chapter on Expressions.` |
|       - |  2855 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2856 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2857 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2858 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2859 | ` *  is being assigned (the source variable).` |
|       - |  2860 | ` */` |
| 1134434 |  2861 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2862 |  |
| 1134439 |  2863 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2864 | `	sxi32 iVv;` |
|       - |  2865 | `	sxi32 iP1;` |
|       - |  2866 | `	void *p3;` |
|       - |  2867 | `	sxi32 rc;` |
| 1134439 |  2868 | `	iVv = -1; /* Variable variable counter */` |
| 2268885 |  2869 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1134451 |  2870 | `		pGen->pIn++;` |
| 1134451 |  2871 | `		iVv++;` |
|       5 |  2872 | `	}` |
| 1134439 |  2873 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2874 | `		/* Invalid variable name */` |
|     ! 0 |  2875 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2876 | `		if( rc == SXERR_ABORT ){` |
|       - |  2877 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2878 | `			return SXERR_ABORT;` |
|       - |  2879 | `		}` |
|     ! 0 |  2880 | `		return SXRET_OK;` |
|       - |  2881 | `	}` |
| 1134439 |  2882 | `	p3  = 0;` |
| 1134439 |  2883 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2884 | `		/* Dynamic variable creation */` |
|      19 |  2885 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  2886 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  2887 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2888 | `			/* Empty expression */` |
|       3 |  2889 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2890 | `			return SXRET_OK;` |
|       - |  2891 | `		}` |
|       - |  2892 | `		/* Compile the expression holding the variable name */` |
|      16 |  2893 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2894 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2895 | `			return SXERR_ABORT;` |
|      16 |  2896 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2897 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2898 | `			return SXRET_OK;` |
|       - |  2899 | `		}` |
|       7 |  2900 | `	}else{` |
|       - |  2901 | `		SyHashEntry *pEntry;` |
|       - |  2902 | `		SyString *pName;` |
| 1134423 |  2903 | `		char *zName = 0;` |
|       - |  2904 | `		/* Extract variable name */` |
| 1134423 |  2905 | `		pName = &pGen->pIn->sData;` |
|       - |  2906 | `		/* Advance the stream cursor */` |
| 1134423 |  2907 | `		pGen->pIn++;` |
| 1134423 |  2908 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1134423 |  2909 | `		if( pEntry == 0 ){` |
|       - |  2910 | `			/* Duplicate name */` |
|  163215 |  2911 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  163215 |  2912 | `			if( zName == 0 ){` |
|     ! 0 |  2913 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2914 | `				return SXERR_ABORT;` |
|       - |  2915 | `			}` |
|       - |  2916 | `			/* Install in the hashtable */` |
|  163215 |  2917 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   81610 |  2918 | `		}else{` |
|       - |  2919 | `			/* Name already available */` |
|  971213 |  2920 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2921 | `		}` |
| 1134423 |  2922 | `		p3 = (void *)zName;` |
|       - |  2923 | `	}` |
| 1134435 |  2924 | `	iP1 = 0;` |
| 1134435 |  2925 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  442743 |  2926 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2927 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  442725 |  2928 | `			iP1 = 1;` |
|  221360 |  2929 | `		}` |
|  221369 |  2930 | `	}` |
|       - |  2931 | `	/* Emit the load instruction */` |
| 1134435 |  2932 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1134447 |  2933 | `	while( iVv > 0 ){` |
|      13 |  2934 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2935 | `		iVv--;` |
|       1 |  2936 | `	}` |
|       - |  2937 | `	/* Node successfully compiled */` |
| 1134435 |  2938 | `	return SXRET_OK;` |
|  567222 |  2939 |  |
|       - |  2940 | `/*` |
|       - |  2941 | ` * Load a literal.` |
|       - |  2942 | ` */` |
|  781834 |  2943 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2944 |  |
|  781839 |  2945 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2946 | `	ph7_value *pObj;` |
|       - |  2947 | `	SyString *pStr;` |
|       - |  2948 | `	sxu32 nIdx;` |
|       - |  2949 | `	/* Extract token value */` |
|  781839 |  2950 | `	pStr = &pToken->sData;` |
|       - |  2951 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  781839 |  2952 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  165693 |  2953 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2954 | `			/* NULL constant are always indexed at 0 */` |
|   61003 |  2955 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   61003 |  2956 | `			return SXRET_OK;` |
|  104695 |  2957 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2958 | `			/* TRUE constant are always indexed at 1 */` |
|     755 |  2959 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     755 |  2960 | `			return SXRET_OK;` |
|       5 |  2961 | `		}` |
|  721094 |  2962 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  105946 |  2963 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2964 | `			/* FALSE constant are always indexed at 2 */` |
|   46763 |  2965 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   46763 |  2966 | `			return SXRET_OK;` |
|  624903 |  2967 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  111020 |  2968 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2969 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   10643 |  2970 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   10643 |  2971 | `			if( pObj == 0 ){` |
|     ! 0 |  2972 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2973 | `				return SXERR_ABORT;` |
|       - |  2974 | `			}` |
|   10643 |  2975 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2976 | `			/* Emit the load constant instruction */` |
|   10643 |  2977 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   10643 |  2978 | `			return SXRET_OK;` |
|  576691 |  2979 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   35872 |  2980 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2981 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  2982 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  2983 | `			if( pObj == 0 ){` |
|     ! 0 |  2984 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2985 | `				return SXERR_ABORT;` |
|       - |  2986 | `			}` |
|       7 |  2987 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2988 | `				SyString sNs;` |
|       7 |  2989 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  2990 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  2991 | `			}else{` |
|     ! 0 |  2992 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2993 | `			}` |
|       7 |  2994 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  2995 | `			return SXRET_OK;` |
|  566220 |  2996 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   24546 |  2997 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  568326 |  2998 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   19178 |  2999 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  3000 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  3001 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  3002 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  3003 | `				/* Point to the upper block */` |
|      11 |  3004 | `				pBlock = pBlock->pParent;` |
|       1 |  3005 | `			}` |
|      11 |  3006 | `			if( pBlock == 0 ){` |
|       - |  3007 | `				/* Called in the global scope,load NULL */` |
|       5 |  3008 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  3009 | `			}else{` |
|       - |  3010 | `				/* Extract the target function/method */` |
|       7 |  3011 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  3012 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  3013 | `					/* Not a class method,Load null */` |
|       3 |  3014 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3015 | `				}else{` |
|       5 |  3016 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  3017 | `					if( pObj == 0 ){` |
|     ! 0 |  3018 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3019 | `						return SXERR_ABORT;` |
|       - |  3020 | `					}` |
|       5 |  3021 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  3022 | `					/* Emit the load constant instruction */` |
|       5 |  3023 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  3024 | `				}` |
|       - |  3025 | `			}` |
|      11 |  3026 | `			return SXRET_OK;` |
|       - |  3027 | `	}` |
|       - |  3028 | `	/* Query literal table */` |
|  662679 |  3029 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3030 | `		ph7_value *pLitObj;` |
|       - |  3031 | `		/* Unknown literal,install it in the literal table */` |
|  282347 |  3032 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  282347 |  3033 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3034 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3035 | `			return SXERR_ABORT;` |
|       - |  3036 | `		}` |
|  282347 |  3037 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  282347 |  3038 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  141171 |  3039 | `	}` |
|       - |  3040 | `	/* Emit the load constant instruction */` |
|  662679 |  3041 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  662679 |  3042 | `	return SXRET_OK;` |
|  390922 |  3043 |  |
|       - |  3044 | `/*` |
|       - |  3045 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3046 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3047 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3048 | ` * Otherwise, load the simple literal directly.` |
|       - |  3049 | ` */` |
|  785418 |  3050 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3051 |  |
|       - |  3052 | `	sxi32 rc;` |
|  785423 |  3053 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3054 | `		return SXRET_OK;` |
|       - |  3055 | `	}` |
|       - |  3056 | `	/* Check if this is a multi-token namespace path */` |
|  785423 |  3057 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3058 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3589 |  3059 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3589 |  3060 | `		int isAbsolute = 0;` |
|    3589 |  3061 | `		SyBlobReset(pWorker);` |
|       - |  3062 | `		/* Check for leading backslash (absolute path) */` |
|    3589 |  3063 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3587 |  3064 | `			isAbsolute = 1;` |
|    3587 |  3065 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1791 |  3066 | `		}` |
|       - |  3067 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3589 |  3068 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3069 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3070 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3071 | `		}` |
|       - |  3072 | `		/* Collect all path components */` |
|    3685 |  3073 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    3685 |  3074 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      53 |  3075 | `				SyBlobAppend(pWorker,"\\",1);` |
|      29 |  3076 | `			}else{` |
|    3637 |  3077 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3078 | `			}` |
|    3685 |  3079 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3589 |  3080 | `				pGen->pIn++;` |
|    3589 |  3081 | `				break;` |
|       - |  3082 | `			}` |
|     101 |  3083 | `			pGen->pIn++;` |
|       5 |  3084 | `		}` |
|    3589 |  3085 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3086 | `			ph7_value *pObj;` |
|       - |  3087 | `			SyString sPath;` |
|       - |  3088 | `			sxu32 nIdx;` |
|    3589 |  3089 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3090 | `			/* Install in the literal table */` |
|    3589 |  3091 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3565 |  3092 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3565 |  3093 | `				if( pObj == 0 ){` |
|     ! 0 |  3094 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3095 | `					return SXERR_ABORT;` |
|       - |  3096 | `				}` |
|    3565 |  3097 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3565 |  3098 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1780 |  3099 | `			}` |
|       - |  3100 | `			/* Emit the load constant instruction.` |
|       - |  3101 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3102 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5381 |  3103 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1792 |  3104 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1792 |  3105 | `				nIdx,0,0);` |
|    3589 |  3106 | `			return SXRET_OK;` |
|       - |  3107 | `		}` |
|     ! 0 |  3108 | `	}` |
|       - |  3109 | `	/* Single-token literal: load directly */` |
|  781839 |  3110 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  781839 |  3111 | `	return rc;` |
|  392714 |  3112 |  |
|       - |  3113 | `/*` |
|       - |  3114 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3115 | ` */` |
|       - |  3116 | `/*` |
|       - |  3117 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - |  3118 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - |  3119 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - |  3120 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - |  3121 | ` */` |
|     ! 0 |  3122 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 |  3123 |  |
|     ! 0 |  3124 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 |  3125 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - |  3126 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 |  3127 | `	return SXERR_SYNTAX;` |
|     ! 0 |  3128 |  |
|  785418 |  3129 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3130 |  |
|       - |  3131 | `	sxi32 rc;` |
|  785423 |  3132 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  785423 |  3133 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3134 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3135 | `		return rc;` |
|       - |  3136 | `	}` |
|       - |  3137 | `	/* Node successfully compiled */` |
|  785423 |  3138 | `	return SXRET_OK;` |
|  392714 |  3139 |  |
|       - |  3140 | `/*` |
|       - |  3141 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3142 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3143 | ` */` |
|       8 |  3144 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3145 |  |
|       - |  3146 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3147 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3148 | `		pGen->pIn++;` |
|       1 |  3149 | `	}` |
|       9 |  3150 | `	return SXRET_OK;` |
|       1 |  3151 |  |
|       - |  3152 | `/*` |
|       - |  3153 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3154 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3155 | ` */` |
|     106 |  3156 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3157 |  |
|     111 |  3158 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      30 |  3159 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3160 | `			return TRUE;` |
|      28 |  3161 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3162 | `			return TRUE;` |
|       2 |  3163 | `		}` |
|      95 |  3164 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3165 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3166 | `			return TRUE;` |
|       - |  3167 | `		}` |
|     ! 0 |  3168 | `	}` |
|       - |  3169 | `	/* Not a reserved constant */` |
|     103 |  3170 | `	return FALSE;` |
|      58 |  3171 |  |
|       - |  3172 | `/*` |
|       - |  3173 | ` * Compile the 'const' statement.` |
|       - |  3174 | ` * According to the PHP language reference` |
|       - |  3175 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3176 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3177 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3178 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3179 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3180 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3181 | ` *  Syntax` |
|       - |  3182 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3183 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3184 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3185 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3186 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3187 | ` *  to get a list of all defined constants.` |
|       - |  3188 | ` *` |
|       - |  3189 | ` * Symisc eXtension.` |
|       - |  3190 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3191 | ` *  would allow only simple scalar value.` |
|       - |  3192 | ` *  Example` |
|       - |  3193 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3194 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3195 | ` */` |
|      32 |  3196 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3197 |  |
|       - |  3198 | `	SySet *pConsCode,*pInstrContainer;` |
|      37 |  3199 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3200 | `	SyString *pName;` |
|       - |  3201 | `	sxi32 rc;` |
|      37 |  3202 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      37 |  3203 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3204 | `		/* Invalid constant name */` |
|       8 |  3205 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       8 |  3206 | `		if( rc == SXERR_ABORT ){` |
|       - |  3207 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3208 | `			return SXERR_ABORT;` |
|       - |  3209 | `		}` |
|       8 |  3210 | `		goto Synchronize;` |
|       - |  3211 | `	}` |
|       - |  3212 | `	/* Peek constant name */` |
|      31 |  3213 | `	pName = &pGen->pIn->sData;` |
|       - |  3214 | `	/* Make sure the constant name isn't reserved */` |
|      31 |  3215 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3216 | `		/* Reserved constant */` |
|      10 |  3217 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3218 | `		if( rc == SXERR_ABORT ){` |
|       - |  3219 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3220 | `			return SXERR_ABORT;` |
|       - |  3221 | `		}` |
|      10 |  3222 | `		goto Synchronize;` |
|       - |  3223 | `	}` |
|      21 |  3224 | `	pGen->pIn++;` |
|      21 |  3225 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3226 | `		/* Invalid statement*/` |
|       6 |  3227 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3228 | `		if( rc == SXERR_ABORT ){` |
|       - |  3229 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3230 | `			return SXERR_ABORT;` |
|       - |  3231 | `		}` |
|       6 |  3232 | `		goto Synchronize;` |
|       - |  3233 | `	}` |
|      15 |  3234 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3235 | `	/* Allocate a new constant value container */` |
|      15 |  3236 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3237 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3238 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3239 | `		return SXERR_ABORT;` |
|       - |  3240 | `	}` |
|      15 |  3241 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3242 | `	/* Swap bytecode container */` |
|      15 |  3243 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3244 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3245 | `	/* Compile constant value */` |
|      15 |  3246 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3247 | `	/* Emit the done instruction */` |
|      15 |  3248 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3249 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3250 | `	if( rc == SXERR_ABORT ){` |
|       - |  3251 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3252 | `		return SXERR_ABORT;` |
|       - |  3253 | `	}` |
|      15 |  3254 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3255 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3256 | `	{` |
|       - |  3257 | `		SyBlob sFQN;` |
|       - |  3258 | `		SyString sFQNStr;` |
|      15 |  3259 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3260 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3261 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3262 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3263 | `		SyBlobRelease(&sFQN);` |
|       - |  3264 | `	}` |
|      15 |  3265 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3266 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3267 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3268 | `	}` |
|      15 |  3269 | `	return SXRET_OK;` |
|       9 |  3270 | `Synchronize:` |
|       - |  3271 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3272 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      41 |  3273 | `		pGen->pIn++;` |
|       3 |  3274 | `	}` |
|      22 |  3275 | `	return SXRET_OK;` |
|      21 |  3276 |  |
|       - |  3277 | `/*` |
|       - |  3278 | ` * Compile the 'continue' statement.` |
|       - |  3279 | ` * According to the PHP language reference` |
|       - |  3280 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3281 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3282 | ` *  iteration.` |
|       - |  3283 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3284 | ` *  the purposes of continue.` |
|       - |  3285 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3286 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3287 | ` *  Note:` |
|       - |  3288 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3289 | ` */` |
|       - |  3290 | `/*` |
|       - |  3291 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3292 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3293 | ` * break/continue crosses a try boundary.` |
|       - |  3294 | ` *` |
|       - |  3295 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3296 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3297 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3298 | ` */` |
|    3684 |  3299 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3300 |  |
|    3689 |  3301 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   21617 |  3302 | `	while( pBlock && pBlock != pTarget ){` |
|   17933 |  3303 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3304 | `			if( pBlock->pUserData ){` |
|       - |  3305 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3306 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3307 | `			}else{` |
|       - |  3308 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3309 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3310 | `				 * exception context from a sub-execution.` |
|       - |  3311 | `				 */` |
|     ! 0 |  3312 | `				break;` |
|       - |  3313 | `			}` |
|       1 |  3314 | `		}` |
|   17933 |  3315 | `		pBlock = pBlock->pParent;` |
|       5 |  3316 | `	}` |
|    3689 |  3317 |  |
|    3588 |  3318 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3319 |  |
|       - |  3320 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3321 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3322 | `	sxu32 nLineLocal;` |
|       - |  3323 | `	sxi32 rc;` |
|    3593 |  3324 | `	nLineLocal = pGen->pIn->nLine;` |
|    3593 |  3325 | `	iLevel = 0;` |
|       - |  3326 | `	/* Jump the 'continue' keyword */` |
|    3593 |  3327 | `	pGen->pIn++;` |
|    3593 |  3328 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3329 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3330 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3331 | `		 */` |
|       - |  3332 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3333 | `		char *zAlloc = 0;` |
|       - |  3334 | `		SyString sNum;` |
|      17 |  3335 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3336 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3337 | `			return SXERR_ABORT;` |
|       - |  3338 | `		}` |
|      17 |  3339 | `		if( rc == SXRET_OK ){` |
|      20 |  3340 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3341 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3342 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3343 | `				return SXERR_ABORT;` |
|       - |  3344 | `			}` |
|      14 |  3345 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3346 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3347 | `		}` |
|      17 |  3348 | `		if( iLevel < 2 ){` |
|       3 |  3349 | `			iLevel = 0;` |
|       1 |  3350 | `		}` |
|      17 |  3351 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3352 | `	}` |
|       - |  3353 | `	/* Point to the target loop */` |
|    3593 |  3354 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3593 |  3355 | `	if( pLoop == 0 ){` |
|       - |  3356 | `		/* Illegal continue */` |
|      12 |  3357 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      12 |  3358 | `		if( rc == SXERR_ABORT ){` |
|       - |  3359 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3360 | `			return SXERR_ABORT;` |
|       - |  3361 | `		}` |
|       7 |  3362 | `	}else{` |
|    3583 |  3363 | `		sxu32 nInstrIdx = 0;` |
|       - |  3364 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3583 |  3365 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3583 |  3366 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3367 | `			/* According to the PHP language reference manual` |
|       - |  3368 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3369 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3370 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3371 | `			 */` |
|       5 |  3372 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3373 | `			if( rc == SXRET_OK ){` |
|       5 |  3374 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3375 | `			}` |
|       3 |  3376 | `		}else{` |
|       - |  3377 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3579 |  3378 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3579 |  3379 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3380 | `				JumpFixup sJumpFix;` |
|       - |  3381 | `				/* Post-continue */` |
|      14 |  3382 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3383 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3384 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3385 | `			}` |
|       - |  3386 | `		}` |
|       - |  3387 | `	}` |
|    3593 |  3388 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3389 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3390 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3391 | `	}` |
|       - |  3392 | `	/* Statement successfully compiled */` |
|    3593 |  3393 | `	return SXRET_OK;` |
|    1799 |  3394 |  |
|       - |  3395 | `/*` |
|       - |  3396 | ` * Compile the 'break' statement.` |
|       - |  3397 | ` * According to the PHP language reference` |
|       - |  3398 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3399 | ` *  structure.` |
|       - |  3400 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3401 | ` *  enclosing structures are to be broken out of.` |
|       - |  3402 | ` */` |
|     122 |  3403 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3404 |  |
|       - |  3405 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3406 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3407 | `	sxi32 rc;` |
|     127 |  3408 | `	iLevel = 0;` |
|       - |  3409 | `	/* Jump the 'break' keyword */` |
|     127 |  3410 | `	pGen->pIn++;` |
|     127 |  3411 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3412 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3413 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3414 | `		 */` |
|       - |  3415 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      18 |  3416 | `		char *zAlloc = 0;` |
|       - |  3417 | `		SyString sNum;` |
|      18 |  3418 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      18 |  3419 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3420 | `			return SXERR_ABORT;` |
|       - |  3421 | `		}` |
|      18 |  3422 | `		if( rc == SXRET_OK ){` |
|      21 |  3423 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3424 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3425 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3426 | `				return SXERR_ABORT;` |
|       - |  3427 | `			}` |
|      15 |  3428 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3429 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3430 | `		}` |
|      18 |  3431 | `		if( iLevel < 2 ){` |
|       3 |  3432 | `			iLevel = 0;` |
|       1 |  3433 | `		}` |
|      18 |  3434 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3435 | `	}` |
|       - |  3436 | `	/* Extract the target loop */` |
|     127 |  3437 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     127 |  3438 | `	if( pLoop == 0 ){` |
|       - |  3439 | `		/* Illegal break */` |
|      18 |  3440 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      18 |  3441 | `		if( rc == SXERR_ABORT ){` |
|       - |  3442 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3443 | `			return SXERR_ABORT;` |
|       - |  3444 | `		}` |
|      10 |  3445 | `	}else{` |
|       - |  3446 | `		sxu32 nInstrIdx;` |
|       - |  3447 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|     111 |  3448 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|     111 |  3449 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|     111 |  3450 | `		if( rc == SXRET_OK ){` |
|       - |  3451 | `			/* Fix the jump later when the jump destination is resolved */` |
|     111 |  3452 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      53 |  3453 | `		}` |
|       - |  3454 | `	}` |
|     127 |  3455 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3456 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3457 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3458 | `	}` |
|       - |  3459 | `	/* Statement successfully compiled */` |
|     127 |  3460 | `	return SXRET_OK;` |
|      66 |  3461 |  |
|       - |  3462 | `/*` |
|       - |  3463 | ` * Compile or record a label.` |
|       - |  3464 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3465 | ` * Example` |
|       - |  3466 | ` *  goto LABEL;` |
|       - |  3467 | ` *   echo 'Foo';` |
|       - |  3468 | ` *  LABEL:` |
|       - |  3469 | ` *   echo 'Bar';` |
|       - |  3470 | ` */` |
|     112 |  3471 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3472 |  |
|       - |  3473 | `	GenBlock *pBlock;` |
|       - |  3474 | `	Label sLabel;` |
|       - |  3475 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3476 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3477 | `	if( pBlock ){` |
|       - |  3478 | `		sxi32 rc;` |
|       8 |  3479 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3480 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3481 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3482 | `			return SXERR_ABORT;` |
|       - |  3483 | `		}` |
|       4 |  3484 | `	}else{` |
|     113 |  3485 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3486 | `		char *zDup;` |
|       - |  3487 | `		/* Initialize label fields */` |
|     113 |  3488 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3489 | `		/* Duplicate label name */` |
|     113 |  3490 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3491 | `		if( zDup == 0 ){` |
|     ! 0 |  3492 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3493 | `			return SXERR_ABORT;` |
|       - |  3494 | `		}` |
|     113 |  3495 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3496 | `		sLabel.bRef  = FALSE;` |
|     113 |  3497 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3498 | `		pBlock = pGen->pCurrent;` |
|     221 |  3499 | `		while( pBlock ){` |
|     133 |  3500 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 |  3501 | `				break;` |
|       - |  3502 | `			}` |
|       - |  3503 | `			/* Point to the upper block */` |
|     113 |  3504 | `			pBlock = pBlock->pParent;` |
|       5 |  3505 | `		}` |
|     113 |  3506 | `		if( pBlock ){` |
|      22 |  3507 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 |  3508 | `		}else{` |
|      93 |  3509 | `			sLabel.pFunc = 0;` |
|       - |  3510 | `		}` |
|       - |  3511 | `		/* Insert in label set */` |
|     113 |  3512 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3513 | `	}` |
|     117 |  3514 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3515 | `	return SXRET_OK;` |
|      61 |  3516 |  |
|       - |  3517 | `/*` |
|       - |  3518 | ` * Compile the so hated 'goto' statement.` |
|       - |  3519 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3520 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3521 | ` * a compiler it has to do this.` |
|       - |  3522 | ` * According to the PHP language reference manual` |
|       - |  3523 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3524 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3525 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3526 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3527 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3528 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3529 | ` *   of a multi-level break` |
|       - |  3530 | ` */` |
|     152 |  3531 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3532 |  |
|       - |  3533 | `	JumpFixup sJump;` |
|       - |  3534 | `	sxi32 rc;` |
|     157 |  3535 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3536 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3537 | `		/* Missing label */` |
|     ! 0 |  3538 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3539 | `		if( rc == SXERR_ABORT ){` |
|       - |  3540 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3541 | `			return SXERR_ABORT;` |
|       - |  3542 | `		}` |
|     ! 0 |  3543 | `		return SXRET_OK;` |
|       - |  3544 | `	}` |
|     157 |  3545 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  3546 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3547 | `		if( rc == SXERR_ABORT ){` |
|       - |  3548 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3549 | `			return SXERR_ABORT;` |
|       - |  3550 | `		}` |
|       3 |  3551 | `	}else{` |
|     153 |  3552 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3553 | `		GenBlock *pBlock;` |
|       - |  3554 | `		char *zDup;` |
|       - |  3555 | `		/* Prepare the jump destination */` |
|     153 |  3556 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3557 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3558 | `		/* Duplicate label name */` |
|     153 |  3559 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3560 | `		if( zDup == 0 ){` |
|     ! 0 |  3561 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3562 | `			return SXERR_ABORT;` |
|       - |  3563 | `		}` |
|     153 |  3564 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3565 | `		pBlock = pGen->pCurrent;` |
|     315 |  3566 | `		while( pBlock ){` |
|     199 |  3567 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      37 |  3568 | `				break;` |
|       - |  3569 | `			}` |
|       - |  3570 | `			/* Point to the upper block */` |
|     167 |  3571 | `			pBlock = pBlock->pParent;` |
|       5 |  3572 | `		}` |
|     153 |  3573 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       9 |  3574 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       9 |  3575 | `			if( rc == SXERR_ABORT ){` |
|       - |  3576 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3577 | `				return SXERR_ABORT;` |
|       - |  3578 | `			}` |
|       3 |  3579 | `		}` |
|     153 |  3580 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      30 |  3581 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      17 |  3582 | `		}else{` |
|     127 |  3583 | `			sJump.pFunc = 0;` |
|       - |  3584 | `		}` |
|       - |  3585 | `		/* Emit the unconditional jump */` |
|     153 |  3586 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3587 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3588 | `		}` |
|       - |  3589 | `	}` |
|     157 |  3590 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3591 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3592 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3593 | `	}` |
|       - |  3594 | `	/* Statement successfully compiled */` |
|     157 |  3595 | `	return SXRET_OK;` |
|      81 |  3596 |  |
|       - |  3597 | `/*` |
|       - |  3598 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3599 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3600 | ` * failure.` |
|       - |  3601 | ` */` |
|      20 |  3602 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  3603 |  |
|       - |  3604 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3605 | `	sxu32 nRawObj;` |
|      10 |  3606 | `	sxu32 nObjIdx;` |
|       - |  3607 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3608 | `	 * a PHP block.` |
|       - |  3609 | `	 */` |
|      10 |  3610 | `Consume:` |
|      21 |  3611 | `	nRawObj = nObjIdx = 0;` |
|      21 |  3612 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3613 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3614 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3615 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3616 | `			return SXERR_ABORT;` |
|       - |  3617 | `		}` |
|       - |  3618 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3619 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3620 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3621 | `		++nRawObj;` |
|     ! 0 |  3622 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3623 | `	}` |
|      21 |  3624 | `	if( nRawObj > 0 ){` |
|       - |  3625 | `		/* Emit the consume instruction */` |
|     ! 0 |  3626 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3627 | `	}` |
|      21 |  3628 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3629 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3630 | `		/* Reset the token set */` |
|     ! 0 |  3631 | `		SySetReset(pTokenSet);` |
|       - |  3632 | `		/* Tokenize input */` |
|     ! 0 |  3633 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3634 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3635 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3636 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3637 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3638 | `		/* Advance the stream cursor */` |
|     ! 0 |  3639 | `		pGen->pRawIn++;` |
|       - |  3640 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3641 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3642 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3643 | `			sxi32 rc;` |
|       - |  3644 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3645 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3646 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3647 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3648 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3649 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3650 | `				return SXERR_ABORT;` |
|     ! 0 |  3651 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3652 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3653 | `			}` |
|     ! 0 |  3654 | `			goto Consume;` |
|       - |  3655 | `		}` |
|     ! 0 |  3656 | `	}else{` |
|       - |  3657 | `		/* No more chunks to process */` |
|      21 |  3658 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  3659 | `		return SXERR_EOF;` |
|       - |  3660 | `	}` |
|     ! 0 |  3661 | `	return SXRET_OK;` |
|      11 |  3662 |  |
|       - |  3663 | `/*` |
|       - |  3664 | ` * Compile a PHP block.` |
|       - |  3665 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3666 | ` * optionally delimited by braces {}.` |
|       - |  3667 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3668 | ` * and this function takes care of generating the appropriate error` |
|       - |  3669 | ` * message.` |
|       - |  3670 | ` */` |
|  430504 |  3671 | `static sxi32 PH7_CompileBlock(` |
|       - |  3672 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3673 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3674 | `	)` |
|       5 |  3675 |  |
|       - |  3676 | `	sxi32 rc;` |
|       - |  3677 | `	sxu32 nLine;` |
|  430509 |  3678 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  428825 |  3679 | `		nLine = pGen->pIn->nLine;` |
|  428825 |  3680 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  428825 |  3681 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3682 | `			return SXERR_ABORT;` |
|       - |  3683 | `		}` |
|  428825 |  3684 | `		pGen->pIn++;` |
|       - |  3685 | `		/* Compile until we hit the closing braces '}' */` |
|  587285 |  3686 | `		for(;;){` |
| 1174575 |  3687 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  3688 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  3689 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3690 | `			 	   return SXERR_ABORT;` |
|       - |  3691 | `				}` |
|      21 |  3692 | `				if( rc == SXERR_EOF ){` |
|       - |  3693 | `					/* No more token to process. Missing closing braces */` |
|      21 |  3694 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 |  3695 | `					break;` |
|       - |  3696 | `				}` |
|     ! 0 |  3697 | `			}` |
| 1174555 |  3698 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3699 | `				/* Closing braces found,break immediately*/` |
|  428805 |  3700 | `				pGen->pIn++;` |
|  428805 |  3701 | `				break;` |
|       - |  3702 | `			}` |
|       - |  3703 | `			/* Compile a single statement */` |
|  745755 |  3704 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  745755 |  3705 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3706 | `				return SXERR_ABORT;` |
|       - |  3707 | `			}` |
|       5 |  3708 | `		}` |
|  428825 |  3709 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  216099 |  3710 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3711 | `		pGen->pIn++;` |
|     ! 0 |  3712 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3713 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3714 | `			return SXERR_ABORT;` |
|       - |  3715 | `		}` |
|       - |  3716 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3717 | `		for(;;){` |
|     ! 0 |  3718 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3719 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3720 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3721 | `			 	   return SXERR_ABORT;` |
|       - |  3722 | `				}` |
|     ! 0 |  3723 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3724 | `					/* No more token to process */` |
|     ! 0 |  3725 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3726 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3727 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3728 | `					}` |
|     ! 0 |  3729 | `					break;` |
|       - |  3730 | `				}` |
|     ! 0 |  3731 | `			}` |
|     ! 0 |  3732 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3733 | `				sxi32 nKwrd;` |
|       - |  3734 | `				/* Keyword found */` |
|     ! 0 |  3735 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3736 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3737 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3738 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3739 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3740 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3741 | `						}` |
|     ! 0 |  3742 | `						break;` |
|       - |  3743 | `				}` |
|     ! 0 |  3744 | `			}` |
|       - |  3745 | `			/* Compile a single statement */` |
|     ! 0 |  3746 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3747 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3748 | `				return SXERR_ABORT;` |
|       - |  3749 | `			}` |
|     ! 0 |  3750 | `		}` |
|     ! 0 |  3751 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3752 | `	}else{` |
|       - |  3753 | `		/* Compile a single statement */` |
|    1689 |  3754 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1689 |  3755 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3756 | `			return SXERR_ABORT;` |
|       - |  3757 | `		}` |
|       - |  3758 | `	}` |
|       - |  3759 | `	/* Jump trailing semi-colons ';' */` |
|  430509 |  3760 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3761 | `		pGen->pIn++;` |
|     ! 0 |  3762 | `	}` |
|  430509 |  3763 | `	return SXRET_OK;` |
|  215257 |  3764 |  |
|       - |  3765 | `/*` |
|       - |  3766 | ` * Compile the gentle 'while' statement.` |
|       - |  3767 | ` * According to the PHP language reference` |
|       - |  3768 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3769 | ` *  The basic form of a while statement is:` |
|       - |  3770 | ` *  while (expr)` |
|       - |  3771 | ` *   statement` |
|       - |  3772 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3773 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3774 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3775 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3776 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3777 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3778 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3779 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3780 | ` *  while (expr):` |
|       - |  3781 | ` *    statement` |
|       - |  3782 | ` *   endwhile;` |
|       - |  3783 | ` */` |
|   14296 |  3784 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3785 |  |
|   14301 |  3786 | `	GenBlock *pWhileBlock = 0;` |
|   14301 |  3787 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3788 | `	sxu32 nFalseJump;` |
|       - |  3789 | `	sxu32 nLine;` |
|       - |  3790 | `	sxi32 rc;` |
|   14301 |  3791 | `	nLine = pGen->pIn->nLine;` |
|       - |  3792 | `	/* Jump the 'while' keyword */` |
|   14301 |  3793 | `	pGen->pIn++;` |
|   14301 |  3794 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3795 | `		/* Syntax error */` |
|     ! 0 |  3796 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3797 | `		if( rc == SXERR_ABORT ){` |
|       - |  3798 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3799 | `			return SXERR_ABORT;` |
|       - |  3800 | `		}` |
|     ! 0 |  3801 | `		goto Synchronize;` |
|       - |  3802 | `	}` |
|       - |  3803 | `	/* Jump the left parenthesis '(' */` |
|   14301 |  3804 | `	pGen->pIn++;` |
|       - |  3805 | `	/* Create the loop block */` |
|   14301 |  3806 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   14301 |  3807 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3808 | `		return SXERR_ABORT;` |
|       - |  3809 | `	}` |
|       - |  3810 | `	/* Delimit the condition */` |
|   14301 |  3811 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14301 |  3812 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3813 | `		/* Empty expression */` |
|       3 |  3814 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3815 | `		if( rc == SXERR_ABORT ){` |
|       - |  3816 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3817 | `			return SXERR_ABORT;` |
|       - |  3818 | `		}` |
|       1 |  3819 | `	}` |
|       - |  3820 | `	/* Swap token streams */` |
|   14301 |  3821 | `	pTmp = pGen->pEnd;` |
|   14301 |  3822 | `	pGen->pEnd = pEnd;` |
|       - |  3823 | `	/* Compile the expression */` |
|   14301 |  3824 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14301 |  3825 | `	if( rc == SXERR_ABORT ){` |
|       - |  3826 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3827 | `		return SXERR_ABORT;` |
|       - |  3828 | `	}` |
|       - |  3829 | `	/* Update token stream */` |
|   14301 |  3830 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3831 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3832 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3833 | `			return SXERR_ABORT;` |
|       - |  3834 | `		}` |
|     ! 0 |  3835 | `		pGen->pIn++;` |
|     ! 0 |  3836 | `	}` |
|       - |  3837 | `	/* Synchronize pointers */` |
|   14301 |  3838 | `	pGen->pIn  = &pEnd[1];` |
|   14301 |  3839 | `	pGen->pEnd = pTmp;` |
|       - |  3840 | `	/* Emit the false jump */` |
|   14301 |  3841 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3842 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14301 |  3843 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3844 | `	/* Compile the loop body */` |
|   14301 |  3845 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   14301 |  3846 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3847 | `		return SXERR_ABORT;` |
|       - |  3848 | `	}` |
|       - |  3849 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14301 |  3850 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3851 | `	/* Fix all jumps now the destination is resolved */` |
|   14301 |  3852 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3853 | `	/* Release the loop block */` |
|   14301 |  3854 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3855 | `	/* Statement successfully compiled */` |
|   14301 |  3856 | `	return SXRET_OK;` |
|     ! 0 |  3857 | `Synchronize:` |
|       - |  3858 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3859 | `	 * compiling this erroneous block.` |
|       - |  3860 | `	 */` |
|     ! 0 |  3861 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3862 | `		pGen->pIn++;` |
|     ! 0 |  3863 | `	}` |
|     ! 0 |  3864 | `	return SXRET_OK;` |
|    7153 |  3865 |  |
|       - |  3866 | `/*` |
|       - |  3867 | ` * Compile the ugly do..while() statement.` |
|       - |  3868 | ` * According to the PHP language reference` |
|       - |  3869 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3870 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3871 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3872 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3873 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3874 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3875 | ` *  would end immediately).` |
|       - |  3876 | ` *  There is just one syntax for do-while loops:` |
|       - |  3877 | ` *  <?php` |
|       - |  3878 | ` *  $i = 0;` |
|       - |  3879 | ` *  do {` |
|       - |  3880 | ` *   echo $i;` |
|       - |  3881 | ` *  } while ($i > 0);` |
|       - |  3882 | ` * ?>` |
|       - |  3883 | ` */` |
|       2 |  3884 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3885 |  |
|       3 |  3886 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3887 | `	GenBlock *pDoBlock = 0;` |
|       - |  3888 | `	sxu32 nLine;` |
|       - |  3889 | `	sxi32 rc;` |
|       3 |  3890 | `	nLine = pGen->pIn->nLine;` |
|       - |  3891 | `	/* Jump the 'do' keyword */` |
|       3 |  3892 | `	pGen->pIn++;` |
|       - |  3893 | `	/* Create the loop block */` |
|       3 |  3894 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3895 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3896 | `		return SXERR_ABORT;` |
|       - |  3897 | `	}` |
|       - |  3898 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3899 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3900 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3901 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3902 | `		return SXERR_ABORT;` |
|       - |  3903 | `	}` |
|       3 |  3904 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3905 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3906 | `	}` |
|       3 |  3907 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3908 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3909 | `			/* Missing 'while' statement */` |
|       3 |  3910 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3911 | `			if( rc == SXERR_ABORT ){` |
|       - |  3912 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3913 | `				return SXERR_ABORT;` |
|       - |  3914 | `			}` |
|       3 |  3915 | `			goto Synchronize;` |
|       - |  3916 | `	}` |
|       - |  3917 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3918 | `	pGen->pIn++;` |
|     ! 0 |  3919 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3920 | `		/* Syntax error */` |
|     ! 0 |  3921 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3922 | `		if( rc == SXERR_ABORT ){` |
|       - |  3923 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3924 | `			return SXERR_ABORT;` |
|       - |  3925 | `		}` |
|     ! 0 |  3926 | `		goto Synchronize;` |
|       - |  3927 | `	}` |
|       - |  3928 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3929 | `	pGen->pIn++;` |
|       - |  3930 | `	/* Delimit the condition */` |
|     ! 0 |  3931 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3932 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3933 | `		/* Empty expression */` |
|     ! 0 |  3934 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3935 | `		if( rc == SXERR_ABORT ){` |
|       - |  3936 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3937 | `			return SXERR_ABORT;` |
|       - |  3938 | `		}` |
|     ! 0 |  3939 | `		goto Synchronize;` |
|       - |  3940 | `	}` |
|       - |  3941 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3942 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3943 | `		JumpFixup *aPost;` |
|       - |  3944 | `		VmInstr *pInstr;` |
|       - |  3945 | `		sxu32 nJumpDest;` |
|       - |  3946 | `		sxu32 n;` |
|     ! 0 |  3947 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3948 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3949 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3950 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3951 | `			if( pInstr ){` |
|       - |  3952 | `				/* Fix */` |
|     ! 0 |  3953 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3954 | `			}` |
|     ! 0 |  3955 | `		}` |
|     ! 0 |  3956 | `	}` |
|       - |  3957 | `	/* Swap token streams */` |
|     ! 0 |  3958 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3959 | `	pGen->pEnd = pEnd;` |
|       - |  3960 | `	/* Compile the expression */` |
|     ! 0 |  3961 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3962 | `	if( rc == SXERR_ABORT ){` |
|       - |  3963 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3964 | `		return SXERR_ABORT;` |
|       - |  3965 | `	}` |
|       - |  3966 | `	/* Update token stream */` |
|     ! 0 |  3967 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3968 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3969 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3970 | `			return SXERR_ABORT;` |
|       - |  3971 | `		}` |
|     ! 0 |  3972 | `		pGen->pIn++;` |
|     ! 0 |  3973 | `	}` |
|     ! 0 |  3974 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3975 | `	pGen->pEnd = pTmp;` |
|       - |  3976 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3977 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3978 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3979 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3980 | `	/* Release the loop block */` |
|     ! 0 |  3981 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3982 | `	/* Statement successfully compiled */` |
|     ! 0 |  3983 | `	return SXRET_OK;` |
|       1 |  3984 | `Synchronize:` |
|       - |  3985 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3986 | `	 * compiling this erroneous block.` |
|       - |  3987 | `	 */` |
|       3 |  3988 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3989 | `		pGen->pIn++;` |
|     ! 0 |  3990 | `	}` |
|       3 |  3991 | `	return SXRET_OK;` |
|       2 |  3992 |  |
|       - |  3993 | `/*` |
|       - |  3994 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3995 | ` * According to the PHP language reference` |
|       - |  3996 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3997 | ` *  The syntax of a for loop is:` |
|       - |  3998 | ` *  for (expr1; expr2; expr3)` |
|       - |  3999 | ` *   statement` |
|       - |  4000 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  4001 | ` *  the beginning of the loop.` |
|       - |  4002 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  4003 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  4004 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  4005 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  4006 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  4007 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  4008 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  4009 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  4010 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  4011 | ` *  of using the for truth expression.` |
|       - |  4012 | ` */` |
|   14296 |  4013 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4014 |  |
|   14301 |  4015 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   14301 |  4016 | `	GenBlock *pForBlock = 0;` |
|       - |  4017 | `	sxu32 nFalseJump;` |
|       - |  4018 | `	sxu32 nLine;` |
|       - |  4019 | `	sxi32 rc;` |
|   14301 |  4020 | `	nLine = pGen->pIn->nLine;` |
|       - |  4021 | `	/* Jump the 'for' keyword */` |
|   14301 |  4022 | `	pGen->pIn++;` |
|   14301 |  4023 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4024 | `		/* Syntax error */` |
|     ! 0 |  4025 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4026 | `		if( rc == SXERR_ABORT ){` |
|       - |  4027 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4028 | `			return SXERR_ABORT;` |
|       - |  4029 | `		}` |
|     ! 0 |  4030 | `		return SXRET_OK;` |
|       - |  4031 | `	}` |
|       - |  4032 | `	/* Jump the left parenthesis '(' */` |
|   14301 |  4033 | `	pGen->pIn++;` |
|       - |  4034 | `	/* Delimit the init-expr;condition;post-expr */` |
|   14301 |  4035 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14301 |  4036 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4037 | `		/* Empty expression */` |
|     ! 0 |  4038 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4039 | `		if( rc == SXERR_ABORT ){` |
|       - |  4040 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4041 | `			return SXERR_ABORT;` |
|       - |  4042 | `		}` |
|       - |  4043 | `		/* Synchronize */` |
|     ! 0 |  4044 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4045 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4046 | `			pGen->pIn++;` |
|     ! 0 |  4047 | `		}` |
|     ! 0 |  4048 | `		return SXRET_OK;` |
|       - |  4049 | `	}` |
|       - |  4050 | `	/* Swap token streams */` |
|   14301 |  4051 | `	pTmp = pGen->pEnd;` |
|   14301 |  4052 | `	pGen->pEnd = pEnd;` |
|       - |  4053 | `	/* Compile initialization expressions if available */` |
|   14301 |  4054 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4055 | `	/* Pop operand lvalues */` |
|   14301 |  4056 | `	if( rc == SXERR_ABORT ){` |
|       - |  4057 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4058 | `		return SXERR_ABORT;` |
|   14301 |  4059 | `	}else if( rc != SXERR_EMPTY ){` |
|   14299 |  4060 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7147 |  4061 | `	}` |
|   14301 |  4062 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4063 | `		/* Syntax error */` |
|     ! 0 |  4064 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4065 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4066 | `		if( rc == SXERR_ABORT ){` |
|       - |  4067 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4068 | `			return SXERR_ABORT;` |
|       - |  4069 | `		}` |
|     ! 0 |  4070 | `		return SXRET_OK;` |
|       - |  4071 | `	}` |
|       - |  4072 | `	/* Jump the trailing ';' */` |
|   14301 |  4073 | `	pGen->pIn++;` |
|       - |  4074 | `	/* Create the loop block */` |
|   14301 |  4075 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   14301 |  4076 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4077 | `		return SXERR_ABORT;` |
|       - |  4078 | `	}` |
|       - |  4079 | `	/* Deffer continue jumps */` |
|   14301 |  4080 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4081 | `	/* Compile the condition */` |
|   14301 |  4082 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14301 |  4083 | `	if( rc == SXERR_ABORT ){` |
|       - |  4084 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4085 | `		return SXERR_ABORT;` |
|   14301 |  4086 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4087 | `		/* Emit the false jump */` |
|   14299 |  4088 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4089 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14299 |  4090 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7147 |  4091 | `	}` |
|   14301 |  4092 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4093 | `		/* Syntax error */` |
|       6 |  4094 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4095 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4096 | `		if( rc == SXERR_ABORT ){` |
|       - |  4097 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4098 | `			return SXERR_ABORT;` |
|       - |  4099 | `		}` |
|       6 |  4100 | `		return SXRET_OK;` |
|       - |  4101 | `	}` |
|       - |  4102 | `	/* Jump the trailing ';' */` |
|   14297 |  4103 | `	pGen->pIn++;` |
|       - |  4104 | `	/* Save the post condition stream */` |
|   14297 |  4105 | `	pPostStart = pGen->pIn;` |
|       - |  4106 | `	/* Compile the loop body */` |
|   14297 |  4107 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   14297 |  4108 | `	pGen->pEnd = pTmp;` |
|   14297 |  4109 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   14297 |  4110 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4111 | `		return SXERR_ABORT;` |
|       - |  4112 | `	}` |
|       - |  4113 | `	/* Fix post-continue jumps */` |
|   14297 |  4114 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4115 | `		JumpFixup *aPost;` |
|       - |  4116 | `		VmInstr *pInstr;` |
|       - |  4117 | `		sxu32 nJumpDest;` |
|       - |  4118 | `		sxu32 n;` |
|      14 |  4119 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4120 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4121 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4122 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4123 | `			if( pInstr ){` |
|       - |  4124 | `				/* Fix jump */` |
|      14 |  4125 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4126 | `			}` |
|       8 |  4127 | `		}` |
|       6 |  4128 | `	}` |
|       - |  4129 | `	/* compile the post-expressions if available */` |
|   14297 |  4130 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4131 | `		pPostStart++;` |
|     ! 0 |  4132 | `	}` |
|   14297 |  4133 | `	if( pPostStart < pEnd ){` |
|       - |  4134 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   14297 |  4135 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   14297 |  4136 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14297 |  4137 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4138 | `			/* Syntax error */` |
|     ! 0 |  4139 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4140 | `			if( rc == SXERR_ABORT ){` |
|       - |  4141 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4142 | `				return SXERR_ABORT;` |
|       - |  4143 | `			}` |
|     ! 0 |  4144 | `			return SXRET_OK;` |
|       - |  4145 | `		}` |
|   14297 |  4146 | `		RE_SWAP_DELIMITER(pGen);` |
|   14297 |  4147 | `		if( rc == SXERR_ABORT ){` |
|       - |  4148 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4149 | `			return SXERR_ABORT;` |
|   14297 |  4150 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4151 | `			/* Pop operand lvalue */` |
|   14297 |  4152 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7146 |  4153 | `		}` |
|    7146 |  4154 | `	}` |
|       - |  4155 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14297 |  4156 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4157 | `	/* Fix all jumps now the destination is resolved */` |
|   14297 |  4158 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4159 | `	/* Release the loop block */` |
|   14297 |  4160 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4161 | `	/* Statement successfully compiled */` |
|   14297 |  4162 | `	return SXRET_OK;` |
|    7153 |  4163 |  |
|       - |  4164 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4165 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4166 | ` * are allowed.` |
|       - |  4167 | ` */` |
|    7670 |  4168 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4169 |  |
|    7675 |  4170 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7675 |  4171 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4172 | `		/* Unexpected expression */` |
|     ! 0 |  4173 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4174 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4175 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4176 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4177 | `		}` |
|     ! 0 |  4178 | `	}` |
|    7675 |  4179 | `	return rc;` |
|       5 |  4180 |  |
|       - |  4181 | `/*` |
|       - |  4182 | ` * Compile the 'foreach' statement.` |
|       - |  4183 | ` * According to the PHP language reference` |
|       - |  4184 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4185 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4186 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4187 | ` *  is a minor but useful extension of the first:` |
|       - |  4188 | ` *  foreach (array_expression as $value)` |
|       - |  4189 | ` *    statement` |
|       - |  4190 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4191 | ` *   statement` |
|       - |  4192 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4193 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4194 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4195 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4196 | ` *  to the variable $key on each loop.` |
|       - |  4197 | ` *  Note:` |
|       - |  4198 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4199 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4200 | ` *  Note:` |
|       - |  4201 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4202 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4203 | ` *  or after the foreach without resetting it.` |
|       - |  4204 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4205 | ` *  of copying the value.` |
|       - |  4206 | ` */` |
|    3930 |  4207 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4208 |  |
|    3935 |  4209 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3935 |  4210 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3935 |  4211 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4212 | `	ph7_foreach_info *pInfo;` |
|       - |  4213 | `	sxu32 nFalseJump;` |
|       - |  4214 | `	VmInstr *pInstr;` |
|       - |  4215 | `	sxu32 nLine;` |
|       - |  4216 | `	sxi32 rc;` |
|    3935 |  4217 | `	nLine = pGen->pIn->nLine;` |
|       - |  4218 | `	/* Jump the 'foreach' keyword */` |
|    3935 |  4219 | `	pGen->pIn++;` |
|    3935 |  4220 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4221 | `		/* Syntax error */` |
|     ! 0 |  4222 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4223 | `		if( rc == SXERR_ABORT ){` |
|       - |  4224 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4225 | `			return SXERR_ABORT;` |
|       - |  4226 | `		}` |
|     ! 0 |  4227 | `		goto Synchronize;` |
|       - |  4228 | `	}` |
|       - |  4229 | `	/* Jump the left parenthesis '(' */` |
|    3935 |  4230 | `	pGen->pIn++;` |
|       - |  4231 | `	/* Create the loop block */` |
|    3935 |  4232 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3935 |  4233 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4234 | `		return SXERR_ABORT;` |
|       - |  4235 | `	}` |
|       - |  4236 | `	/* Delimit the expression */` |
|    3935 |  4237 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3935 |  4238 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4239 | `		/* Empty expression */` |
|     ! 0 |  4240 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4241 | `		if( rc == SXERR_ABORT ){` |
|       - |  4242 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4243 | `			return SXERR_ABORT;` |
|       - |  4244 | `		}` |
|       - |  4245 | `		/* Synchronize */` |
|     ! 0 |  4246 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4247 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4248 | `			pGen->pIn++;` |
|     ! 0 |  4249 | `		}` |
|     ! 0 |  4250 | `		return SXRET_OK;` |
|       - |  4251 | `	}` |
|       - |  4252 | `	/* Compile the array expression */` |
|    3935 |  4253 | `	pCur = pGen->pIn;` |
|   27003 |  4254 | `	while( pCur < pEnd ){` |
|   27003 |  4255 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3949 |  4256 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3949 |  4257 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4258 | `				/* Break with the first 'as' found */` |
|    3935 |  4259 | `				break;` |
|       - |  4260 | `			}` |
|       7 |  4261 | `		}` |
|       - |  4262 | `		/* Advance the stream cursor */` |
|   23073 |  4263 | `		pCur++;` |
|       5 |  4264 | `	}` |
|    3935 |  4265 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4266 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4267 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4268 | `		if( rc == SXERR_ABORT ){` |
|       - |  4269 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4270 | `			return SXERR_ABORT;` |
|       - |  4271 | `		}` |
|     ! 0 |  4272 | `		goto Synchronize;` |
|       - |  4273 | `	}` |
|       - |  4274 | `	/* Swap token streams */` |
|    3935 |  4275 | `	pTmp = pGen->pEnd;` |
|    3935 |  4276 | `	pGen->pEnd = pCur;` |
|    3935 |  4277 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3935 |  4278 | `	if( rc == SXERR_ABORT ){` |
|       - |  4279 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4280 | `		return SXERR_ABORT;` |
|       - |  4281 | `	}` |
|       - |  4282 | `	/* Update token stream */` |
|    3935 |  4283 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4284 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4285 | `		if( rc == SXERR_ABORT ){` |
|       - |  4286 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4287 | `			return SXERR_ABORT;` |
|       - |  4288 | `		}` |
|     ! 0 |  4289 | `		pGen->pIn++;` |
|     ! 0 |  4290 | `	}` |
|    3935 |  4291 | `	pCur++; /* Jump the 'as' keyword */` |
|    3935 |  4292 | `	pGen->pIn = pCur;` |
|    3935 |  4293 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4294 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4295 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4296 | `			return SXERR_ABORT;` |
|       - |  4297 | `		}` |
|     ! 0 |  4298 | `	}` |
|       - |  4299 | `	/* Create the foreach context */` |
|    3935 |  4300 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3935 |  4301 | `	if( pInfo == 0 ){` |
|     ! 0 |  4302 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4303 | `		return SXERR_ABORT;` |
|       - |  4304 | `	}` |
|       - |  4305 | `	/* Zero the structure */` |
|    3935 |  4306 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4307 | `	/* Initialize structure fields */` |
|    3935 |  4308 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4309 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4310 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4311 | `	 * '=>'. */` |
|    3935 |  4312 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    3935 |  4313 | `	if( pCur < pEnd ){` |
|       - |  4314 | `		/* Compile the expression holding the key name */` |
|    3757 |  4315 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4316 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4317 | `			if( rc == SXERR_ABORT ){` |
|       - |  4318 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4319 | `				return SXERR_ABORT;` |
|       - |  4320 | `			}` |
|     ! 0 |  4321 | `		}else{` |
|    3757 |  4322 | `			pGen->pEnd = pCur;` |
|    3757 |  4323 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3757 |  4324 | `			if( rc == SXERR_ABORT ){` |
|       - |  4325 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4326 | `				return SXERR_ABORT;` |
|       - |  4327 | `			}` |
|    3757 |  4328 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3757 |  4329 | `			if( pInstr->p3 ){` |
|       - |  4330 | `				/* Record key name */` |
|    3757 |  4331 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1876 |  4332 | `			}` |
|    3757 |  4333 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4334 | `		}` |
|    3757 |  4335 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1876 |  4336 | `	}` |
|    3935 |  4337 | `	pGen->pEnd = pEnd;` |
|    3935 |  4338 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4339 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4340 | `		if( rc == SXERR_ABORT ){` |
|       - |  4341 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4342 | `			return SXERR_ABORT;` |
|       - |  4343 | `		}` |
|     ! 0 |  4344 | `		goto Synchronize;` |
|       - |  4345 | `	}` |
|    3935 |  4346 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4347 | `		pGen->pIn++;` |
|       - |  4348 | `		/* Pass by reference  */` |
|      11 |  4349 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4350 | `	}` |
|       - |  4351 | `	/* Check if the value target is list() */` |
|    3935 |  4352 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4353 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4354 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4355 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4356 | `		 */` |
|       - |  4357 | `		static int iForeachListCnt = 0;` |
|       - |  4358 | `		char zTmp[128];` |
|       - |  4359 | `		sxu32 nLen;` |
|       - |  4360 | `		char *zDup;` |
|      10 |  4361 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4362 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4363 | `		if( zDup == 0 ){` |
|     ! 0 |  4364 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4365 | `			return SXERR_ABORT;` |
|       - |  4366 | `		}` |
|      10 |  4367 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4368 | `		/* Save list() token boundaries */` |
|      10 |  4369 | `		pListStart = pGen->pIn;` |
|       - |  4370 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4371 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4372 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4373 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4374 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4375 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4376 | `				return SXERR_ABORT;` |
|       - |  4377 | `			}` |
|       3 |  4378 | `			goto Synchronize;` |
|       - |  4379 | `		}` |
|       7 |  4380 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4381 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4382 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4383 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4384 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4385 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4386 | `				return SXERR_ABORT;` |
|       - |  4387 | `			}` |
|     ! 0 |  4388 | `			goto Synchronize;` |
|       - |  4389 | `		}` |
|       7 |  4390 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4391 | `		pListEnd = pGen->pIn;` |
|       7 |  4392 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3930 |  4393 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4394 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4395 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4396 | `		 */` |
|       - |  4397 | `		static int iForeachShortListCnt = 0;` |
|       - |  4398 | `		char zTmp[128];` |
|       - |  4399 | `		sxu32 nLen;` |
|       - |  4400 | `		char *zDup;` |
|       5 |  4401 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       5 |  4402 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       5 |  4403 | `		if( zDup == 0 ){` |
|     ! 0 |  4404 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4405 | `			return SXERR_ABORT;` |
|       - |  4406 | `		}` |
|       5 |  4407 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4408 | `		/* Save [...] token boundaries */` |
|       5 |  4409 | `		pListStart = pGen->pIn;` |
|       - |  4410 | `		/* Advance past [...] */` |
|       5 |  4411 | `		pGen->pIn++; /* Jump '[' */` |
|       5 |  4412 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       5 |  4413 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4414 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4415 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4416 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4417 | `				return SXERR_ABORT;` |
|       - |  4418 | `			}` |
|     ! 0 |  4419 | `			goto Synchronize;` |
|       - |  4420 | `		}` |
|       5 |  4421 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       5 |  4422 | `		pListEnd = pGen->pIn;` |
|       5 |  4423 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       3 |  4424 | `	}else{` |
|       - |  4425 | `		/* Compile the expression holding the value name */` |
|    3923 |  4426 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3923 |  4427 | `		if( rc == SXERR_ABORT ){` |
|       - |  4428 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4429 | `			return SXERR_ABORT;` |
|       - |  4430 | `		}` |
|    3923 |  4431 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3923 |  4432 | `		if( pInstr->p3 ){` |
|       - |  4433 | `			/* Record value name */` |
|    3923 |  4434 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1959 |  4435 | `		}` |
|       - |  4436 | `	}` |
|       - |  4437 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3933 |  4438 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4439 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3933 |  4440 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4441 | `	/* Record the first instruction to execute */` |
|    3933 |  4442 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4443 | `	/* Emit the FOREACH_STEP instruction */` |
|    3933 |  4444 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4445 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3933 |  4446 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4447 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3933 |  4448 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4449 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4450 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4451 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4452 | `		 */` |
|      11 |  4453 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4454 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4455 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4456 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4457 | `		 */` |
|      11 |  4458 | `		pSavedIn = pGen->pIn;` |
|      11 |  4459 | `		pSavedEnd = pGen->pEnd;` |
|      11 |  4460 | `		pGen->pIn = pListStart;` |
|      11 |  4461 | `		pGen->pEnd = pListEnd;` |
|      11 |  4462 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       5 |  4463 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       3 |  4464 | `		}else{` |
|       7 |  4465 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4466 | `		}` |
|      11 |  4467 | `		pGen->pIn = pSavedIn;` |
|      11 |  4468 | `		pGen->pEnd = pSavedEnd;` |
|      11 |  4469 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4470 | `			return SXERR_ABORT;` |
|       - |  4471 | `		}` |
|       - |  4472 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      11 |  4473 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       5 |  4474 | `	}` |
|       - |  4475 | `	/* Compile the loop body */` |
|    3933 |  4476 | `	pGen->pIn = &pEnd[1];` |
|    3933 |  4477 | `	pGen->pEnd = pTmp;` |
|    3933 |  4478 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3933 |  4479 | `	if( rc == SXERR_ABORT ){` |
|       - |  4480 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4481 | `		return SXERR_ABORT;` |
|       - |  4482 | `	}` |
|       - |  4483 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3933 |  4484 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4485 | `	/* Fix all jumps now the destination is resolved */` |
|    3933 |  4486 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4487 | `	/* Release the loop block */` |
|    3933 |  4488 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4489 | `	/* Statement successfully compiled */` |
|    3933 |  4490 | `	return SXRET_OK;` |
|       1 |  4491 | `Synchronize:` |
|       - |  4492 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4493 | `	 * compiling this erroneous block.` |
|       - |  4494 | `	 */` |
|       3 |  4495 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4496 | `		pGen->pIn++;` |
|     ! 0 |  4497 | `	}` |
|       3 |  4498 | `	return SXRET_OK;` |
|    1970 |  4499 |  |
|       - |  4500 | `/*` |
|       - |  4501 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4502 | ` * According to the PHP language reference` |
|       - |  4503 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4504 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4505 | ` *  that is similar to that of C:` |
|       - |  4506 | ` *  if (expr)` |
|       - |  4507 | ` *   statement` |
|       - |  4508 | ` *  else construct:` |
|       - |  4509 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4510 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4511 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4512 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4513 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4514 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4515 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4516 | ` *  elseif` |
|       - |  4517 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4518 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4519 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4520 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4521 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4522 | ` *   <?php` |
|       - |  4523 | ` *    if ($a > $b) {` |
|       - |  4524 | ` *     echo "a is bigger than b";` |
|       - |  4525 | ` *    } elseif ($a == $b) {` |
|       - |  4526 | ` *     echo "a is equal to b";` |
|       - |  4527 | ` *    } else {` |
|       - |  4528 | ` *     echo "a is smaller than b";` |
|       - |  4529 | ` *    }` |
|       - |  4530 | ` *    ?>` |
|       - |  4531 | ` */` |
|  148584 |  4532 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4533 |  |
|  148589 |  4534 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  148589 |  4535 | `	GenBlock *pCondBlock = 0;` |
|       - |  4536 | `	sxu32 nJumpIdx;` |
|       - |  4537 | `	sxu32 nKeyID;` |
|       - |  4538 | `	sxi32 rc;` |
|       - |  4539 | `	/* Jump the 'if' keyword */` |
|  148589 |  4540 | `	pGen->pIn++;` |
|  148589 |  4541 | `	pToken = pGen->pIn;` |
|       - |  4542 | `	/* Create the conditional block */` |
|  148589 |  4543 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  148589 |  4544 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4545 | `		return SXERR_ABORT;` |
|       - |  4546 | `	}` |
|       - |  4547 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   81437 |  4548 | `	for(;;){` |
|  162879 |  4549 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4550 | `			/* Syntax error */` |
|     ! 0 |  4551 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4552 | `				pToken--;` |
|     ! 0 |  4553 | `			}` |
|     ! 0 |  4554 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4555 | `			if( rc == SXERR_ABORT ){` |
|       - |  4556 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4557 | `				return SXERR_ABORT;` |
|       - |  4558 | `			}` |
|     ! 0 |  4559 | `			goto Synchronize;` |
|       - |  4560 | `		}` |
|       - |  4561 | `		/* Jump the left parenthesis '(' */` |
|  162879 |  4562 | `		pToken++;` |
|       - |  4563 | `		/* Delimit the condition */` |
|  162879 |  4564 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  162879 |  4565 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4566 | `			/* Syntax error */` |
|     ! 0 |  4567 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4568 | `				pToken--;` |
|     ! 0 |  4569 | `			}` |
|     ! 0 |  4570 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4571 | `			if( rc == SXERR_ABORT ){` |
|       - |  4572 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4573 | `				return SXERR_ABORT;` |
|       - |  4574 | `			}` |
|     ! 0 |  4575 | `			goto Synchronize;` |
|       - |  4576 | `		}` |
|       - |  4577 | `		/* Swap token streams */` |
|  162879 |  4578 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4579 | `		/* Compile the condition */` |
|  162879 |  4580 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4581 | `		/* Update token stream */` |
|  162879 |  4582 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4583 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4584 | `			pGen->pIn++;` |
|     ! 0 |  4585 | `		}` |
|  162879 |  4586 | `		pGen->pIn  = &pEnd[1];` |
|  162879 |  4587 | `		pGen->pEnd = pTmp;` |
|  162879 |  4588 | `		if( rc == SXERR_ABORT ){` |
|       - |  4589 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4590 | `			return SXERR_ABORT;` |
|       - |  4591 | `		}` |
|       - |  4592 | `		/* Emit the false jump */` |
|  162879 |  4593 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4594 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  162879 |  4595 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4596 | `		/* Compile the body */` |
|  162879 |  4597 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  162879 |  4598 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4599 | `			return SXERR_ABORT;` |
|       - |  4600 | `		}` |
|  162879 |  4601 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   45404 |  4602 | `			break;` |
|       - |  4603 | `		}` |
|       - |  4604 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   72081 |  4605 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   72081 |  4606 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   46395 |  4607 | `			break;` |
|       - |  4608 | `		}` |
|       - |  4609 | `		/* Emit the unconditional jump */` |
|   25691 |  4610 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4611 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   25691 |  4612 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   25691 |  4613 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   18489 |  4614 | `			pToken = &pGen->pIn[1];` |
|   18489 |  4615 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7140 |  4616 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5703 |  4617 | `					break;` |
|       - |  4618 | `			}` |
|    7093 |  4619 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3544 |  4620 | `		}` |
|   14295 |  4621 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4622 | `		/* Synchronize cursors */` |
|   14295 |  4623 | `		pToken = pGen->pIn;` |
|       - |  4624 | `		/* Fix the false jump */` |
|   14295 |  4625 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4626 | `	} /* For(;;) */` |
|       - |  4627 | `	/* Fix the false jump */` |
|  148589 |  4628 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  148589 |  4629 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   57786 |  4630 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4631 | `			/* Compile the else block */` |
|   11401 |  4632 | `			pGen->pIn++;` |
|   11401 |  4633 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11401 |  4634 | `			if( rc == SXERR_ABORT ){` |
|       - |  4635 |  |
|     ! 0 |  4636 | `				return SXERR_ABORT;` |
|       - |  4637 | `			}` |
|    5698 |  4638 | `	}` |
|  148589 |  4639 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4640 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  148589 |  4641 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4642 | `	/* Release the conditional block */` |
|  148589 |  4643 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4644 | `	/* Statement successfully compiled */` |
|  148589 |  4645 | `	return SXRET_OK;` |
|     ! 0 |  4646 | `Synchronize:` |
|       - |  4647 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4648 | `	 */` |
|     ! 0 |  4649 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4650 | `		pGen->pIn++;` |
|     ! 0 |  4651 | `	}` |
|     ! 0 |  4652 | `	return SXRET_OK;` |
|   74297 |  4653 |  |
|       - |  4654 | `/*` |
|       - |  4655 | ` * Compile the global construct.` |
|       - |  4656 | ` * According to the PHP language reference` |
|       - |  4657 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4658 | ` *  to be used in that function.` |
|       - |  4659 | ` *  Example #1 Using global` |
|       - |  4660 | ` *  <?php` |
|       - |  4661 | ` *   $a = 1;` |
|       - |  4662 | ` *   $b = 2;` |
|       - |  4663 | ` *   function Sum()` |
|       - |  4664 | ` *   {` |
|       - |  4665 | ` *    global $a, $b;` |
|       - |  4666 | ` *    $b = $a + $b;` |
|       - |  4667 | ` *   }` |
|       - |  4668 | ` *   Sum();` |
|       - |  4669 | ` *   echo $b;` |
|       - |  4670 | ` *  ?>` |
|       - |  4671 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4672 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4673 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4674 | ` */` |
|      36 |  4675 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4676 |  |
|      41 |  4677 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4678 | `	sxi32 nExpr;` |
|       - |  4679 | `	sxi32 rc;` |
|       - |  4680 | `	/* Jump the 'global' keyword */` |
|      41 |  4681 | `	pGen->pIn++;` |
|      41 |  4682 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4683 | `		/* Nothing to process */` |
|     ! 0 |  4684 | `		return SXRET_OK;` |
|       - |  4685 | `	}` |
|      41 |  4686 | `	pTmp = pGen->pEnd;` |
|      41 |  4687 | `	nExpr = 0;` |
|      87 |  4688 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4689 | `		if( pGen->pIn < pNext ){` |
|      51 |  4690 | `			pGen->pEnd = pNext;` |
|      51 |  4691 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4692 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4693 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4694 | `					return SXERR_ABORT;` |
|       - |  4695 | `				}` |
|     ! 0 |  4696 | `			}else{` |
|      51 |  4697 | `				pGen->pIn++;` |
|      51 |  4698 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4699 | `					/* Emit a warning */` |
|     ! 0 |  4700 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4701 | `				}else{` |
|      51 |  4702 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4703 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4704 | `						return SXERR_ABORT;` |
|      51 |  4705 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4706 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4707 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4708 | `							/* Variable name, not a constant */` |
|      51 |  4709 | `							pLast->iP1 = 0;` |
|      23 |  4710 | `						}` |
|      51 |  4711 | `						nExpr++;` |
|      23 |  4712 | `					}` |
|       - |  4713 | `				}` |
|       - |  4714 | `			}` |
|      23 |  4715 | `		}` |
|       - |  4716 | `		/* Next expression in the stream */` |
|      51 |  4717 | `		pGen->pIn = pNext;` |
|       - |  4718 | `		/* Jump trailing commas */` |
|      61 |  4719 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4720 | `			pGen->pIn++;` |
|       5 |  4721 | `		}` |
|       5 |  4722 | `	}` |
|       - |  4723 | `	/* Restore token stream */` |
|      41 |  4724 | `	pGen->pEnd = pTmp;` |
|      41 |  4725 | `	if( nExpr > 0 ){` |
|       - |  4726 | `		/* Emit the uplink instruction */` |
|      41 |  4727 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4728 | `	}` |
|      41 |  4729 | `	return SXRET_OK;` |
|      23 |  4730 |  |
|       - |  4731 | `/*` |
|       - |  4732 | ` * Compile the return statement.` |
|       - |  4733 | ` * According to the PHP language reference` |
|       - |  4734 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4735 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4736 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4737 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4738 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4739 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4740 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4741 | ` *  from within the main script file, then script execution end.` |
|       - |  4742 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4743 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4744 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4745 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4746 | ` */` |
|  235330 |  4747 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4748 |  |
|  235335 |  4749 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4750 | `	sxi32 rc;` |
|       - |  4751 | `	/* Jump the 'return' keyword */` |
|  235335 |  4752 | `	pGen->pIn++;` |
|  235335 |  4753 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4754 | `		/* Compile the expression */` |
|  235307 |  4755 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  235307 |  4756 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4757 | `			return SXERR_ABORT;` |
|  235307 |  4758 | `		}else if(rc != SXERR_EMPTY ){` |
|  235307 |  4759 | `			nRet = 1;` |
|  117651 |  4760 | `		}` |
|  117651 |  4761 | `	}` |
|       - |  4762 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4763 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4764 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4765 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4766 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  235335 |  4767 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  235335 |  4768 | `	return SXRET_OK;` |
|  117670 |  4769 |  |
|       - |  4770 | `/*` |
|       - |  4771 | ` * Compile a yield expression.` |
|       - |  4772 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4773 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4774 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4775 | ` */` |
|     170 |  4776 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4777 |  |
|       - |  4778 | `	SyToken *pTmp, *pSplit;` |
|     175 |  4779 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     175 |  4780 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4781 | `	sxi32 rc;` |
|      85 |  4782 | `	(void)iCompileFlag;` |
|       - |  4783 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     175 |  4784 | `	pGen->pIn++;` |
|       - |  4785 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4786 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4787 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4788 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4789 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     170 |  4790 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     102 |  4791 | `		&& pGen->pIn->sData.nByte == 4` |
|      41 |  4792 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      40 |  4793 | `		pGen->pIn++; /* Skip 'from' */` |
|      40 |  4794 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      40 |  4795 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4796 | `			return SXERR_ABORT;` |
|       - |  4797 | `		}` |
|      40 |  4798 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  4799 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  4800 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  4801 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  4802 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4803 | `				return SXERR_ABORT;` |
|       - |  4804 | `			}` |
|     ! 0 |  4805 | `		}` |
|      40 |  4806 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      40 |  4807 | `		return SXRET_OK;` |
|       - |  4808 | `	}` |
|     139 |  4809 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4810 | `		/* Bare yield — no value */` |
|       3 |  4811 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  4812 | `		return SXRET_OK;` |
|       - |  4813 | `	}` |
|       - |  4814 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     137 |  4815 | `	pSplit = 0;` |
|       - |  4816 | `	{` |
|     137 |  4817 | `		SyToken *pCur = pGen->pIn;` |
|     137 |  4818 | `		sxi32 nNest = 0;` |
|     285 |  4819 | `		while( pCur < pGen->pEnd ){` |
|     167 |  4820 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4821 | `				nNest++;` |
|     167 |  4822 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4823 | `				nNest--;` |
|     167 |  4824 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4825 | `				pSplit = pCur;` |
|      16 |  4826 | `				break;` |
|       - |  4827 | `			}` |
|     153 |  4828 | `			pCur++;` |
|       5 |  4829 | `		}` |
|       - |  4830 | `	}` |
|     137 |  4831 | `	pTmp = pGen->pEnd;` |
|     137 |  4832 | `	if( pSplit ){` |
|       - |  4833 | `		/* yield $key => $value */` |
|      16 |  4834 | `		pGen->pEnd = pSplit;` |
|      16 |  4835 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4836 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4837 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4838 | `		pGen->pEnd = pTmp;` |
|      16 |  4839 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4840 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4841 | `		iP1 = 1;` |
|      16 |  4842 | `		iP2 = 1;` |
|       9 |  4843 | `	}else{` |
|       - |  4844 | `		/* yield $value */` |
|     123 |  4845 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     123 |  4846 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     123 |  4847 | `		if( rc != SXERR_EMPTY ){` |
|     123 |  4848 | `			iP1 = 1;` |
|      59 |  4849 | `		}` |
|       - |  4850 | `	}` |
|     137 |  4851 | `	pGen->pEnd = pTmp;` |
|     137 |  4852 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     137 |  4853 | `	return SXRET_OK;` |
|      90 |  4854 |  |
|       - |  4855 | `/*` |
|       - |  4856 | ` * Compile the die/exit language construct.` |
|       - |  4857 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4858 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4859 | ` */` |
|     120 |  4860 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4861 |  |
|     125 |  4862 | `	sxi32 nExpr = 0;` |
|       - |  4863 | `	sxi32 rc;` |
|       - |  4864 | `	/* Jump the die/exit keyword */` |
|     125 |  4865 | `	pGen->pIn++;` |
|     125 |  4866 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4867 | `		/* Compile the expression */` |
|     125 |  4868 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4869 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4870 | `			return SXERR_ABORT;` |
|     125 |  4871 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4872 | `			nExpr = 1;` |
|      60 |  4873 | `		}` |
|      60 |  4874 | `	}` |
|       - |  4875 | `	/* Emit the HALT instruction */` |
|     125 |  4876 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4877 | `	return SXRET_OK;` |
|      65 |  4878 |  |
|       - |  4879 | `/*` |
|       - |  4880 | ` * Compile the 'echo' language construct.` |
|       - |  4881 | ` */` |
|   14640 |  4882 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4883 |  |
|   14645 |  4884 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4885 | `	sxi32 rc;` |
|       - |  4886 | `	/* Jump the 'echo' keyword */` |
|   14645 |  4887 | `	pGen->pIn++;` |
|       - |  4888 | `	/* Compile arguments one after one */` |
|   14645 |  4889 | `	pTmp = pGen->pEnd;` |
|   32333 |  4890 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   17693 |  4891 | `		if( pGen->pIn < pNext ){` |
|   17693 |  4892 | `			pGen->pEnd = pNext;` |
|   17693 |  4893 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   17693 |  4894 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4895 | `				return SXERR_ABORT;` |
|   17693 |  4896 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4897 | `				/* Emit the consume instruction */` |
|   17669 |  4898 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    8832 |  4899 | `			}` |
|    8844 |  4900 | `		}` |
|       - |  4901 | `		/* Jump trailing commas */` |
|   20741 |  4902 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    3053 |  4903 | `			pNext++;` |
|       5 |  4904 | `		}` |
|   17693 |  4905 | `		pGen->pIn = pNext;` |
|       5 |  4906 | `	}` |
|       - |  4907 | `	/* Restore token stream */` |
|   14645 |  4908 | `	pGen->pEnd = pTmp;` |
|   14645 |  4909 | `	return SXRET_OK;` |
|    7325 |  4910 |  |
|       - |  4911 | `/*` |
|       - |  4912 | ` * Compile the static statement.` |
|       - |  4913 | ` * According to the PHP language reference` |
|       - |  4914 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4915 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4916 | ` *  when program execution leaves this scope.` |
|       - |  4917 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4918 | ` * Symisc eXtension.` |
|       - |  4919 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4920 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4921 | ` *  Example` |
|       - |  4922 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4923 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4924 | ` */` |
|       6 |  4925 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       2 |  4926 |  |
|       - |  4927 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4928 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4929 | `	GenBlock *pBlock;` |
|       - |  4930 | `	SyString *pName;` |
|       - |  4931 | `	char *zDup;` |
|       - |  4932 | `	sxu32 nLine;` |
|       - |  4933 | `	sxi32 rc;` |
|       - |  4934 | `	/* Jump the static keyword */` |
|       8 |  4935 | `	nLine = pGen->pIn->nLine;` |
|       8 |  4936 | `	pGen->pIn++;` |
|       - |  4937 | `	/* Extract the enclosing function if any */` |
|       8 |  4938 | `	pBlock = pGen->pCurrent;` |
|      14 |  4939 | `	while( pBlock ){` |
|      14 |  4940 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       8 |  4941 | `			break;` |
|       - |  4942 | `		}` |
|       - |  4943 | `		/* Point to the upper block */` |
|       8 |  4944 | `		pBlock = pBlock->pParent;` |
|       2 |  4945 | `	}` |
|       8 |  4946 | `	if( pBlock == 0 ){` |
|       - |  4947 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4948 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4949 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4950 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4951 | `				return SXERR_ABORT;` |
|       - |  4952 | `			}` |
|     ! 0 |  4953 | `			goto Synchronize;` |
|       - |  4954 | `		}` |
|       - |  4955 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4956 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4957 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4958 | `			return SXERR_ABORT;` |
|     ! 0 |  4959 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4960 | `			/* Emit the POP instruction */` |
|     ! 0 |  4961 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4962 | `		}` |
|     ! 0 |  4963 | `		return SXRET_OK;` |
|       - |  4964 | `	}` |
|       8 |  4965 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4966 | `	/* Make sure we are dealing with a valid statement */` |
|       8 |  4967 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       4 |  4968 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4969 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4970 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4971 | `				return SXERR_ABORT;` |
|       - |  4972 | `			}` |
|       3 |  4973 | `			goto Synchronize;` |
|       - |  4974 | `	}` |
|       5 |  4975 | `	pGen->pIn++;` |
|       - |  4976 | `	/* Extract variable name */` |
|       5 |  4977 | `	pName = &pGen->pIn->sData;` |
|       5 |  4978 | `	pGen->pIn++; /* Jump the var name */` |
|       5 |  4979 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4980 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4981 | `		goto Synchronize;` |
|       - |  4982 | `	}` |
|       - |  4983 | `	/* Initialize the structure describing the static variable */` |
|       5 |  4984 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       5 |  4985 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4986 | `	/* Duplicate variable name */` |
|       5 |  4987 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       5 |  4988 | `	if( zDup == 0 ){` |
|     ! 0 |  4989 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4990 | `		return SXERR_ABORT;` |
|       - |  4991 | `	}` |
|       5 |  4992 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4993 | `	/* Check if we have an expression to compile */` |
|       5 |  4994 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4995 | `		SySet *pInstrContainer;` |
|       - |  4996 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4997 | `		 * Static variable can take any complex expression including function` |
|       - |  4998 | `		 * call as their initialization value.` |
|       - |  4999 | `		 * Example:` |
|       - |  5000 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  5001 | `		 */` |
|       5 |  5002 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5003 | `		/* Swap bytecode container */` |
|       5 |  5004 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       5 |  5005 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5006 | `		/* Compile the expression */` |
|       5 |  5007 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5008 | `		/* Emit the done instruction */` |
|       5 |  5009 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5010 | `		/* Restore default bytecode container */` |
|       5 |  5011 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       2 |  5012 | `	}` |
|       - |  5013 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       5 |  5014 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       5 |  5015 | `	return SXRET_OK;` |
|       1 |  5016 | `Synchronize:` |
|       - |  5017 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5018 | `	 * statement.` |
|       - |  5019 | `	 */` |
|       5 |  5020 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5021 | `		pGen->pIn++;` |
|       1 |  5022 | `	}` |
|       3 |  5023 | `	return SXRET_OK;` |
|       5 |  5024 |  |
|       - |  5025 | `/*` |
|       - |  5026 | ` * Compile the var statement.` |
|       - |  5027 | ` * Symisc Extension:` |
|       - |  5028 | ` *      var statement can be used outside of a class definition.` |
|       - |  5029 | ` */` |
|       4 |  5030 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5031 |  |
|       - |  5032 | `	sxu32 nLine;` |
|       - |  5033 | `	sxi32 rc;` |
|       5 |  5034 | `	nLine = pGen->pIn->nLine;` |
|       - |  5035 | `	/* Jump the 'var' keyword */` |
|       5 |  5036 | `	pGen->pIn++;` |
|       5 |  5037 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5038 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5039 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5040 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5041 | `			pGen->pIn++;` |
|     ! 0 |  5042 | `		}` |
|     ! 0 |  5043 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5044 | `			return SXERR_ABORT;` |
|       - |  5045 | `		}` |
|     ! 0 |  5046 | `	}else{` |
|       - |  5047 | `		/* Compile the expression */` |
|       5 |  5048 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5049 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5050 | `			return SXERR_ABORT;` |
|       5 |  5051 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5052 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5053 | `		}` |
|       - |  5054 | `	}` |
|       5 |  5055 | `	return SXRET_OK;` |
|       3 |  5056 |  |
|       - |  5057 | `/*` |
|       - |  5058 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5059 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5060 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5061 | ` */` |
|       - |  5062 | `/*` |
|       - |  5063 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5064 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5065 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5066 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5067 | ` *` |
|       - |  5068 | ` * Resolution order:` |
|       - |  5069 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5070 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5071 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5072 | ` *` |
|       - |  5073 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5074 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5075 | ` * Returns the (possibly new) literal index.` |
|       - |  5076 | ` */` |
|  457022 |  5077 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5078 |  |
|       - |  5079 | `	ph7_value *pLit;` |
|       - |  5080 | `	const char *zLit;` |
|       - |  5081 | `	SyString sQualified;` |
|       - |  5082 | `	sxu32 nLit;` |
|       - |  5083 | `	sxu32 k;` |
|       - |  5084 | `	sxu32 nNewIdx;` |
|       - |  5085 | `	int hasNsSep;` |
|       - |  5086 | `	SyHashEntry *pImport;` |
|       - |  5087 | `	ph7_value *pNew;` |
|  457027 |  5088 | `	if( pFromImport ){` |
|  437445 |  5089 | `		*pFromImport = 0;` |
|  218720 |  5090 | `	}` |
|  457027 |  5091 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  457027 |  5092 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5093 | `		return nOrigIdx;` |
|       - |  5094 | `	}` |
|  457027 |  5095 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  457027 |  5096 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5097 | `	/* Skip if already qualified (contains backslash) */` |
|  457027 |  5098 | `	hasNsSep = 0;` |
| 5048447 |  5099 | `	for( k = 0; k < nLit; k++ ){` |
| 4591433 |  5100 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2295715 |  5101 | `	}` |
|  457027 |  5102 | `	if( hasNsSep ){` |
|      10 |  5103 | `		return nOrigIdx;` |
|       - |  5104 | `	}` |
|       - |  5105 | `	/* Check use imports first (works even outside namespaces) */` |
|  457019 |  5106 | `	SyBlobReset(&pGen->sWorker);` |
|  457019 |  5107 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  457019 |  5108 | `	if( pImport ){` |
|      41 |  5109 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5110 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5111 | `		if( pFromImport ){` |
|      18 |  5112 | `			*pFromImport = 1;` |
|       8 |  5113 | `		}` |
|      23 |  5114 | `	}else{` |
|  456983 |  5115 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  456893 |  5116 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5117 | `		}` |
|       - |  5118 | `		/* Prepend current namespace */` |
|      95 |  5119 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5120 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5121 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5122 | `	}` |
|       - |  5123 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5124 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5125 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5126 | `		return nNewIdx; /* Already interned */` |
|       - |  5127 | `	}` |
|      79 |  5128 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5129 | `	if( pNew == 0 ){` |
|     ! 0 |  5130 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5131 | `	}` |
|      79 |  5132 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5133 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5134 | `	return nNewIdx;` |
|  228516 |  5135 |  |
|       - |  5136 | `/*` |
|       - |  5137 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5138 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5139 | ` */` |
|   96608 |  5140 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5141 |  |
|       - |  5142 | `	SyHashEntry *pImport;` |
|       - |  5143 | `	/* Check use imports first */` |
|   96613 |  5144 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   96613 |  5145 | `	if( pImport ){` |
|      14 |  5146 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      14 |  5147 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      14 |  5148 | `		return;` |
|       - |  5149 | `	}` |
|       - |  5150 | `	/* Prepend current namespace if active */` |
|   96601 |  5151 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5152 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5153 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5154 | `	}` |
|   96601 |  5155 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   48309 |  5156 |  |
|       - |  5157 | `/*` |
|       - |  5158 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5159 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5160 | ` * The caller must release pOut when done.` |
|       - |  5161 | ` */` |
|  139586 |  5162 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5163 |  |
|  139591 |  5164 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5165 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5166 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5167 | `	}` |
|  139591 |  5168 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  139591 |  5169 |  |
|       - |  5170 | `/*` |
|       - |  5171 | ` * Compile a namespace statement` |
|       - |  5172 | ` * According to the PHP language reference manual` |
|       - |  5173 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5174 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5175 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5176 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5177 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5178 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5179 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5180 | ` *  programming world.` |
|       - |  5181 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5182 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5183 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5184 | ` *  classes/functions/constants.` |
|       - |  5185 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5186 | ` *  readability of source code.` |
|       - |  5187 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5188 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5189 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5190 | ` *       class MyClass {}` |
|       - |  5191 | ` *       function myfunction() {}` |
|       - |  5192 | ` *       const MYCONST = 1;` |
|       - |  5193 | ` *       $a = new MyClass;` |
|       - |  5194 | ` *       $c = new \my\name\MyClass;` |
|       - |  5195 | ` *       $a = strlen('hi');` |
|       - |  5196 | ` *       $d = namespace\MYCONST;` |
|       - |  5197 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5198 | ` *       echo constant($d);` |
|       - |  5199 | ` * NOTE` |
|       - |  5200 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5201 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5202 | ` */` |
|       - |  5203 | `/*` |
|       - |  5204 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5205 | ` */` |
|      14 |  5206 | `static const char * TokenTypeName(sxu32 nType)` |
|       3 |  5207 |  |
|      17 |  5208 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      10 |  5209 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      10 |  5210 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      10 |  5211 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      10 |  5212 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      10 |  5213 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5214 | `	return "token";` |
|      10 |  5215 |  |
|     106 |  5216 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5217 |  |
|       - |  5218 | `	sxu32 nLine;` |
|       - |  5219 | `	sxi32 rc;` |
|     111 |  5220 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5221 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5222 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5223 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5224 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5225 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5226 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5227 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5228 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5229 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5230 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5231 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5232 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5233 | `		return SXRET_OK;` |
|       - |  5234 | `	}` |
|     111 |  5235 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5236 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5237 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5238 | `		return SXRET_OK;` |
|       - |  5239 | `	}` |
|     111 |  5240 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5241 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5242 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5243 | `		return SXRET_OK;` |
|       - |  5244 | `	}` |
|       - |  5245 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5246 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5247 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5248 | `			/* Append backslash separator */` |
|      27 |  5249 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5250 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5251 | `			}` |
|      16 |  5252 | `		}else{` |
|       - |  5253 | `			/* Append identifier */` |
|     131 |  5254 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5255 | `		}` |
|     153 |  5256 | `		pGen->pIn++;` |
|       5 |  5257 | `	}` |
|       - |  5258 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5259 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5260 | `	{` |
|     111 |  5261 | `		char *zNsDup = 0;` |
|     111 |  5262 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5263 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5264 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5265 | `		}` |
|     111 |  5266 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5267 | `	}` |
|     111 |  5268 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5269 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5270 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5271 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5272 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5273 | `			return SXERR_ABORT;` |
|       - |  5274 | `		}` |
|       2 |  5275 | `	}` |
|     111 |  5276 | `	return SXRET_OK;` |
|      58 |  5277 |  |
|       - |  5278 | `/*` |
|       - |  5279 | ` * Compile the 'use' statement` |
|       - |  5280 | ` * According to the PHP language reference manual` |
|       - |  5281 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5282 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5283 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5284 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5285 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5286 | ` *  a function or constant is not supported.` |
|       - |  5287 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5288 | ` * NOTE` |
|       - |  5289 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5290 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5291 | ` */` |
|      68 |  5292 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5293 |  |
|       - |  5294 | `	sxu32 nLine;` |
|       - |  5295 | `	sxi32 rc;` |
|       - |  5296 | `	SyBlob sPath;` |
|       - |  5297 | `	SyString sAlias;` |
|       - |  5298 | `	SyToken *pLast;` |
|       - |  5299 | `	char *zDup;` |
|       - |  5300 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5301 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5302 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5303 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5304 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5305 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5306 | `	iUseType = 0;` |
|      73 |  5307 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5308 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5309 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5310 | `			iUseType = 1;` |
|      16 |  5311 | `			pGen->pIn++;` |
|      23 |  5312 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5313 | `			iUseType = 2;` |
|      16 |  5314 | `			pGen->pIn++;` |
|       7 |  5315 | `		}` |
|      14 |  5316 | `	}` |
|       - |  5317 | `	/* Select target hash tables based on import type */` |
|      73 |  5318 | `	switch( iUseType ){` |
|       7 |  5319 | `		case 1:` |
|      16 |  5320 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5321 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5322 | `			break;` |
|       7 |  5323 | `		case 2:` |
|      16 |  5324 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5325 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5326 | `			break;` |
|      20 |  5327 | `		default:` |
|      45 |  5328 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5329 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5330 | `			break;` |
|       - |  5331 | `	}` |
|      73 |  5332 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5333 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5334 | `	for(;;){` |
|      75 |  5335 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5336 | `			break;` |
|       - |  5337 | `		}` |
|      75 |  5338 | `		SyBlobReset(&sPath);` |
|      75 |  5339 | `		pLast = 0;` |
|       - |  5340 | `		/* Collect the full namespace path */` |
|     261 |  5341 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5342 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5343 | `				pLast = pGen->pIn;` |
|     131 |  5344 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5345 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5346 | `				}` |
|     131 |  5347 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5348 | `			}` |
|     191 |  5349 | `			pGen->pIn++;` |
|       5 |  5350 | `		}` |
|      75 |  5351 | `		if( pLast == 0 ){` |
|       - |  5352 | `			/* Empty path */` |
|       5 |  5353 | `			break;` |
|       - |  5354 | `		}` |
|       - |  5355 | `		/* Default alias is the last component of the path */` |
|      71 |  5356 | `		sAlias = pLast->sData;` |
|       - |  5357 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5358 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5359 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      19 |  5360 | `			pGen->pIn++; /* Jump 'as' */` |
|      19 |  5361 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      19 |  5362 | `				sAlias = pGen->pIn->sData;` |
|      19 |  5363 | `				pGen->pIn++;` |
|       8 |  5364 | `			}` |
|       8 |  5365 | `		}` |
|       - |  5366 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5367 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5368 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5369 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5370 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5371 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5372 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5373 | `				return SXERR_ABORT;` |
|       - |  5374 | `			}` |
|       2 |  5375 | `		}` |
|       - |  5376 | `		/* Register the import: alias -> FQN.` |
|       - |  5377 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5378 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5379 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5380 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5381 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5382 | `		if( zDup ){` |
|      71 |  5383 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5384 | `			if( pVmHash ){` |
|       - |  5385 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5386 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5387 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5388 | `				if( zAliasDup ){` |
|      43 |  5389 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5390 | `				}` |
|      19 |  5391 | `			}` |
|      71 |  5392 | `			if( iUseType == 2 ){` |
|       - |  5393 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5394 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5395 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5396 | `				if( zAliasDup ){` |
|       - |  5397 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5398 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5399 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5400 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5401 | `					if( azPair ){` |
|      16 |  5402 | `						azPair[0] = zAliasDup;` |
|      16 |  5403 | `						azPair[1] = zDup;` |
|      16 |  5404 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5405 | `					}` |
|       7 |  5406 | `				}` |
|       7 |  5407 | `			}` |
|      33 |  5408 | `		}` |
|       - |  5409 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5410 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5411 | `			pGen->pIn++;` |
|       2 |  5412 | `		}else{` |
|      37 |  5413 | `			break;` |
|       - |  5414 | `		}` |
|       1 |  5415 | `	}` |
|      73 |  5416 | `	SyBlobRelease(&sPath);` |
|      73 |  5417 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5418 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5419 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5420 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5421 | `			return SXERR_ABORT;` |
|       - |  5422 | `		}` |
|       1 |  5423 | `	}` |
|      73 |  5424 | `	return SXRET_OK;` |
|      39 |  5425 |  |
|       - |  5426 | `/*` |
|       - |  5427 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5428 | ` *` |
|       - |  5429 | ` * According to the PHP language reference manual.` |
|       - |  5430 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5431 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5432 | ` *  declare (directive)` |
|       - |  5433 | ` *   statement` |
|       - |  5434 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5435 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5436 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5437 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5438 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5439 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5440 | ` * <?php` |
|       - |  5441 | ` * // these are the same:` |
|       - |  5442 | ` * // you can use this:` |
|       - |  5443 | ` * declare(ticks=1) {` |
|       - |  5444 | ` *   // entire script here` |
|       - |  5445 | ` * }` |
|       - |  5446 | ` * // or you can use this:` |
|       - |  5447 | ` * declare(ticks=1);` |
|       - |  5448 | ` * // entire script here` |
|       - |  5449 | ` * ?>` |
|       - |  5450 | ` *` |
|       - |  5451 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5452 | ` */` |
|       - |  5453 | `/*` |
|       - |  5454 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5455 | ` */` |
|      68 |  5456 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5457 |  |
|     103 |  5458 | `	return SyStringLength(pName) == nWant` |
|      68 |  5459 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5460 |  |
|       - |  5461 |  |
|      40 |  5462 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5463 |  |
|      45 |  5464 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5465 | `	SyToken *pBodyEnd = 0;` |
|       - |  5466 | `	SyToken *pBodyStart;` |
|       - |  5467 | `	SyToken *pCursor;` |
|       - |  5468 | `	int bHasStrictTypes;` |
|       - |  5469 | `	int bBlockForm;` |
|       - |  5470 | `	int bPlacementOk;` |
|       - |  5471 | `	sxi32 rc;` |
|      45 |  5472 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5473 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5474 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5475 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5476 | `			return SXERR_ABORT;` |
|       - |  5477 | `		}` |
|       6 |  5478 | `		goto Synchro;` |
|       - |  5479 | `	}` |
|      41 |  5480 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5481 | `	pBodyStart = pGen->pIn;` |
|       - |  5482 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5483 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5484 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5485 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5486 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5487 | `			return SXERR_ABORT;` |
|       - |  5488 | `		}` |
|     ! 0 |  5489 | `		return SXRET_OK;` |
|       - |  5490 | `	}` |
|       - |  5491 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5492 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5493 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5494 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5495 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5496 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5497 | `			return SXERR_ABORT;` |
|       - |  5498 | `		}` |
|     ! 0 |  5499 | `	}` |
|      41 |  5500 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5501 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5502 | `	bHasStrictTypes = 0;` |
|       - |  5503 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5504 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5505 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5506 | `	pCursor = pBodyStart;` |
|      53 |  5507 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5508 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5509 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5510 | `				bHasStrictTypes = 1;` |
|      37 |  5511 | `				break;` |
|       - |  5512 | `			}` |
|       2 |  5513 | `		}` |
|      14 |  5514 | `		pCursor++;` |
|       2 |  5515 | `	}` |
|      41 |  5516 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5517 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5518 | `			"strict_types declaration must not use block mode");` |
|       3 |  5519 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5520 | `		return SXRET_OK;` |
|       - |  5521 | `	}` |
|      39 |  5522 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5523 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5524 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5525 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5526 | `		return SXRET_OK;` |
|       - |  5527 | `	}` |
|       - |  5528 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5529 | `	pCursor = pBodyStart;` |
|      65 |  5530 | `	while( pCursor < pBodyEnd ){` |
|       - |  5531 | `		SyToken *pNameTok;` |
|       - |  5532 | `		SyToken *pEqTok;` |
|       - |  5533 | `		SyToken *pValTok;` |
|       - |  5534 | `		SyString *pDirName;` |
|       - |  5535 | `		int bIsStrict;` |
|       - |  5536 | `		int iStrictValue;` |
|      37 |  5537 | `		pNameTok = pCursor;` |
|      37 |  5538 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5539 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5540 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5541 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5542 | `			return SXRET_OK;` |
|       - |  5543 | `		}` |
|      37 |  5544 | `		pEqTok = pNameTok + 1;` |
|      37 |  5545 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5546 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5547 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5548 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5549 | `			return SXRET_OK;` |
|       - |  5550 | `		}` |
|      37 |  5551 | `		pValTok = pEqTok + 1;` |
|      37 |  5552 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5553 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5554 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5555 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5556 | `			return SXRET_OK;` |
|       - |  5557 | `		}` |
|      37 |  5558 | `		pDirName = &pNameTok->sData;` |
|      37 |  5559 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5560 | `		if( bIsStrict ){` |
|       - |  5561 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5562 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5563 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5564 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5565 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5566 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5567 | `				return SXRET_OK;` |
|       - |  5568 | `			}` |
|      33 |  5569 | `			iStrictValue = -1;` |
|      33 |  5570 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5571 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5572 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5573 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5574 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5575 | `			}` |
|      33 |  5576 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5577 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5578 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5579 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5580 | `				return SXRET_OK;` |
|       - |  5581 | `			}` |
|      30 |  5582 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5583 | `		}else{` |
|       - |  5584 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5585 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5586 | `			 * behavior don't regress. */` |
|       8 |  5587 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5588 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5589 | `				ph7_lib_version()` |
|       - |  5590 | `				);` |
|       - |  5591 | `		}` |
|      35 |  5592 | `		pCursor = pValTok + 1;` |
|       - |  5593 | `		/* Consume separating comma (or end). */` |
|      35 |  5594 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5595 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5596 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5597 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5598 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5599 | `				return SXRET_OK;` |
|       - |  5600 | `			}` |
|       3 |  5601 | `			pCursor++;` |
|       1 |  5602 | `		}` |
|       5 |  5603 | `	}` |
|       - |  5604 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5605 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5606 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5607 | `	return SXRET_OK;` |
|       2 |  5608 | `Synchro:` |
|       - |  5609 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5610 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5611 | `		pGen->pIn++;` |
|       2 |  5612 | `	}` |
|       6 |  5613 | `	return SXRET_OK;` |
|      25 |  5614 |  |
|       - |  5615 | `/*` |
|       - |  5616 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5617 | ` * as follows:` |
|       - |  5618 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5619 | ` * {` |
|       - |  5620 | ` *   return "Making a cup of $type.\n";` |
|       - |  5621 | ` * }` |
|       - |  5622 | ` * Symisc eXtension.` |
|       - |  5623 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5624 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5625 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5626 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5627 | ` *      {` |
|       - |  5628 | ` *       var_dump($a);` |
|       - |  5629 | ` *      }` |
|       - |  5630 | ` *     //call test without args` |
|       - |  5631 | ` *      test();` |
|       - |  5632 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5633 | ` *      Example:` |
|       - |  5634 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5635 | ` * 3 -) Function overloading!!` |
|       - |  5636 | ` *      Example:` |
|       - |  5637 | ` *      function foo($a) {` |
|       - |  5638 | ` *   	  return $a.PHP_EOL;` |
|       - |  5639 | ` *	    }` |
|       - |  5640 | ` *	    function foo($a, $b) {` |
|       - |  5641 | ` *   	  return $a + $b;` |
|       - |  5642 | ` *	    }` |
|       - |  5643 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5644 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5645 | ` *      // Same arg` |
|       - |  5646 | ` *	   function foo(string $a)` |
|       - |  5647 | ` *	   {` |
|       - |  5648 | ` *	     echo "a is a string\n";` |
|       - |  5649 | ` *	     var_dump($a);` |
|       - |  5650 | ` *	   }` |
|       - |  5651 | ` *	  function foo(int $a)` |
|       - |  5652 | ` *	  {` |
|       - |  5653 | ` *	    echo "a is integer\n";` |
|       - |  5654 | ` *	    var_dump($a);` |
|       - |  5655 | ` *	  }` |
|       - |  5656 | ` *	  function foo(array $a)` |
|       - |  5657 | ` *	  {` |
|       - |  5658 | ` * 	    echo "a is an array\n";` |
|       - |  5659 | ` * 	    var_dump($a);` |
|       - |  5660 | ` *	  }` |
|       - |  5661 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5662 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5663 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5664 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5665 | ` * introduced by the PH7 engine.` |
|       - |  5666 | ` */` |
|   74464 |  5667 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5668 |  |
|       - |  5669 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5670 | `	SySet *pInstrContainer;` |
|       - |  5671 | `	sxi32 rc;` |
|       - |  5672 | `	/* Swap token stream */` |
|   74469 |  5673 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   74469 |  5674 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   74469 |  5675 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5676 | `	/* Compile the expression holding the argument value */` |
|   74469 |  5677 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5678 | `	/* Emit the done instruction */` |
|   74469 |  5679 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   74469 |  5680 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   74469 |  5681 | `	RE_SWAP_DELIMITER(pGen);` |
|   74469 |  5682 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5683 | `		return SXERR_ABORT;` |
|       - |  5684 | `	}` |
|   74469 |  5685 | `	return SXRET_OK;` |
|   37237 |  5686 |  |
|       - |  5687 | `/*` |
|       - |  5688 | ` * Collect function arguments one after one.` |
|       - |  5689 | ` * According to the PHP language reference manual.` |
|       - |  5690 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5691 | ` * list of expressions.` |
|       - |  5692 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5693 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5694 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5695 | ` * for more information.` |
|       - |  5696 | ` * Example #1 Passing arrays to functions` |
|       - |  5697 | ` * <?php` |
|       - |  5698 | ` * function takes_array($input)` |
|       - |  5699 | ` * {` |
|       - |  5700 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5701 | ` * }` |
|       - |  5702 | ` * ?>` |
|       - |  5703 | ` * Making arguments be passed by reference` |
|       - |  5704 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5705 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5706 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5707 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5708 | ` * to the argument name in the function definition:` |
|       - |  5709 | ` * Example #2 Passing function parameters by reference` |
|       - |  5710 | ` * <?php` |
|       - |  5711 | ` * function add_some_extra(&$string)` |
|       - |  5712 | ` * {` |
|       - |  5713 | ` *   $string .= 'and something extra.';` |
|       - |  5714 | ` * }` |
|       - |  5715 | ` * $str = 'This is a string, ';` |
|       - |  5716 | ` * add_some_extra($str);` |
|       - |  5717 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5718 | ` * ?>` |
|       - |  5719 | ` *` |
|       - |  5720 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5721 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5722 | ` * on these extension.` |
|       - |  5723 | ` */` |
|  104144 |  5724 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5725 |  |
|       - |  5726 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5727 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5728 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5729 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5730 | `	sxi32 rc;` |
|       - |  5731 |  |
|  104149 |  5732 | `	pIn = pGen->pIn;` |
|  104149 |  5733 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5734 | `	/* Process arguments one after one */` |
|  134637 |  5735 | `	for(;;){` |
|  269279 |  5736 | `		if( pIn >= pEnd ){` |
|       - |  5737 | `			/* No more arguments to process */` |
|  104135 |  5738 | `			break;` |
|       - |  5739 | `		}` |
|  165149 |  5740 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  165149 |  5741 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  165149 |  5742 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  165149 |  5743 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5744 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5745 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5746 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5747 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5748 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5749 | `		{` |
|  165149 |  5750 | `			int bReadonly = 0, bVisSeen = 0;` |
|  165149 |  5751 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  165149 |  5752 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5753 | `				bReadonly = 1;` |
|       3 |  5754 | `				pIn++;` |
|       1 |  5755 | `			}` |
|  165149 |  5756 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   64021 |  5757 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   64021 |  5758 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      71 |  5759 | `					bVisSeen = 1;` |
|      71 |  5760 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      95 |  5761 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      31 |  5762 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      71 |  5763 | `					pIn++;` |
|      71 |  5764 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5765 | `						bReadonly = 1;` |
|      16 |  5766 | `						pIn++;` |
|       6 |  5767 | `					}` |
|      33 |  5768 | `				}` |
|   32008 |  5769 | `			}` |
|  165149 |  5770 | `			if( bVisSeen \|\| bReadonly ){` |
|      73 |  5771 | `				if( !bCtorCtx ){` |
|       6 |  5772 | `					if( bAbstractCtx ){` |
|       3 |  5773 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5774 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5775 | `					}else{` |
|       3 |  5776 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5777 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5778 | `					}` |
|       6 |  5779 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5780 | `						return SXERR_ABORT;` |
|       - |  5781 | `					}` |
|       6 |  5782 | `					return SXERR_SYNTAX;` |
|       - |  5783 | `				}` |
|      69 |  5784 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      69 |  5785 | `				sArg.iPromoteVis = iVis;` |
|      69 |  5786 | `				if( bReadonly ){` |
|      18 |  5787 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5788 | `				}` |
|      32 |  5789 | `			}` |
|       - |  5790 | `		}` |
|       - |  5791 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  165140 |  5792 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  125269 |  5793 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   83618 |  5794 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   78265 |  5795 | `			sxu32 nLineLocal = pIn->nLine;` |
|   78265 |  5796 | `			sxi32 iTFlags = 0;` |
|   78265 |  5797 | `			pGen->pIn = pIn;` |
|   78265 |  5798 | `			rc = GenStateParseUnionTypeDecl(` |
|   39130 |  5799 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   39130 |  5800 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5801 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5802 | `				/* bAllowVoid */ 0,` |
|   39130 |  5803 | `						nLineLocal);` |
|   78265 |  5804 | `			pIn = pGen->pIn;` |
|   78265 |  5805 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5806 | `				return SXERR_ABORT;` |
|   78265 |  5807 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5808 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5809 | `				return SXERR_SYNTAX;` |
|   78263 |  5810 | `			}else if( rc == SXERR_SYNTAX ){` |
|       9 |  5811 | `				if( pIn < pEnd ){` |
|      12 |  5812 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5813 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       3 |  5814 | `						&pIn->sData);` |
|       6 |  5815 | `				}else{` |
|     ! 0 |  5816 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5817 | `						"syntax error, unexpected end of file");` |
|       - |  5818 | `				}` |
|       9 |  5819 | `				return SXERR_SYNTAX;` |
|       - |  5820 | `			}` |
|   78257 |  5821 | `			sArg.iFlags \|= iTFlags;` |
|   39126 |  5822 | `		}` |
|  165137 |  5823 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5824 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5825 | `			return rc;` |
|       - |  5826 | `		}` |
|  165137 |  5827 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5828 | `			/* Pass by reference,record that */` |
|    3577 |  5829 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3577 |  5830 | `			pIn++;` |
|    1786 |  5831 | `		}` |
|  165137 |  5832 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5833 | `			/* Variadic parameter: ...$args */` |
|    3593 |  5834 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    3593 |  5835 | `			pIn++;` |
|    1794 |  5836 | `		}` |
|  165137 |  5837 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5838 | `			/* Invalid argument */` |
|     ! 0 |  5839 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5840 | `			return rc;` |
|       - |  5841 | `		}` |
|  165137 |  5842 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5843 | `		/* Copy argument name */` |
|  165137 |  5844 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  165137 |  5845 | `		if( zDup == 0 ){` |
|     ! 0 |  5846 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5847 | `			return SXERR_ABORT;` |
|       - |  5848 | `		}` |
|  165137 |  5849 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  165137 |  5850 | `		pIn++;` |
|  165137 |  5851 | `		if( pIn < pEnd ){` |
|  100027 |  5852 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5853 | `				SyToken *pDefend;` |
|   74471 |  5854 | `				sxi32 iNest = 0;` |
|   74471 |  5855 | `				pIn++; /* Jump the equal sign */` |
|   74471 |  5856 | `				pDefend = pIn;` |
|       - |  5857 | `				/* Process the default value associated with this argument */` |
|  156023 |  5858 | `				while( pDefend < pEnd ){` |
|  117007 |  5859 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   35455 |  5860 | `						break;` |
|       - |  5861 | `					}` |
|   81557 |  5862 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5863 | `						/* Increment nesting level */` |
|    3549 |  5864 | `						iNest++;` |
|   79785 |  5865 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5866 | `						/* Decrement nesting level */` |
|    3549 |  5867 | `						iNest--;` |
|    1772 |  5868 | `					}` |
|   81557 |  5869 | `					pDefend++;` |
|       5 |  5870 | `				}` |
|   74471 |  5871 | `				if( pIn >= pDefend ){` |
|       3 |  5872 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5873 | `					return rc;` |
|       - |  5874 | `				}` |
|       - |  5875 | `				/* Process default value */` |
|   74469 |  5876 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   74469 |  5877 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5878 | `					return rc;` |
|       - |  5879 | `				}` |
|       - |  5880 | `				/* Point beyond the default value */` |
|   74469 |  5881 | `				pIn = pDefend;` |
|   37232 |  5882 | `			}` |
|  100025 |  5883 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5884 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5885 | `				return rc;` |
|       - |  5886 | `			}` |
|  100025 |  5887 | `			pIn++; /* Jump the trailing comma */` |
|   50010 |  5888 | `		}` |
|       - |  5889 | `		/* Append argument signature */` |
|  165135 |  5890 | `		if( sArg.nType > 0 ){` |
|   78203 |  5891 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5892 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   14221 |  5893 | `				int marker = 'o';` |
|   14221 |  5894 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   14221 |  5895 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7113 |  5896 | `			}else{` |
|       - |  5897 | `				int c;` |
|   63987 |  5898 | `				c = 'n'; /* cc warning */` |
|       - |  5899 | `				/* Type leading character */` |
|   63987 |  5900 | `				switch(sArg.nType){` |
|       3 |  5901 | `				case MEMOBJ_HASHMAP:` |
|       - |  5902 | `					/* Hashmap aka 'array' */` |
|       7 |  5903 | `					c = 'h';` |
|       7 |  5904 | `					break;` |
|    8916 |  5905 | `				case MEMOBJ_INT:` |
|       - |  5906 | `					/* Integer */` |
|   17837 |  5907 | `					c = 'i';` |
|   17837 |  5908 | `					break;` |
|       2 |  5909 | `				case MEMOBJ_BOOL:` |
|       - |  5910 | `					/* Bool */` |
|       5 |  5911 | `					c = 'b';` |
|       5 |  5912 | `					break;` |
|       2 |  5913 | `				case MEMOBJ_REAL:` |
|       - |  5914 | `					/* Float */` |
|       5 |  5915 | `					c = 'f';` |
|       5 |  5916 | `					break;` |
|   23060 |  5917 | `				case MEMOBJ_STRING:` |
|       - |  5918 | `					/* String */` |
|   46125 |  5919 | `					c = 's';` |
|   46125 |  5920 | `					break;` |
|       7 |  5921 | `				case MEMOBJ_OBJ:` |
|       - |  5922 | `					/* Object */` |
|      16 |  5923 | `					c = 'o';` |
|      14 |  5924 | `					break;` |
|       1 |  5925 | `				default:` |
|       2 |  5926 | `					break;` |
|       - |  5927 | `				}` |
|   63987 |  5928 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5929 | `			}` |
|   39104 |  5930 | `		}else{` |
|       - |  5931 | `			/* No type is associated with this parameter which mean` |
|       - |  5932 | `			 * that this function is not condidate for overloading.` |
|       - |  5933 | `			 */` |
|   86937 |  5934 | `			SyBlobRelease(&sSig);` |
|       - |  5935 | `		}` |
|       - |  5936 | `		/* Save in the argument set */` |
|  165135 |  5937 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5938 | `	}` |
|  104135 |  5939 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5940 | `		/* Save function signature */` |
|   49817 |  5941 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   24906 |  5942 | `	}` |
|  104135 |  5943 | `	return SXRET_OK;` |
|   52077 |  5944 |  |
|       - |  5945 | `/*` |
|       - |  5946 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5947 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5948 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5949 | ` */` |
|  222120 |  5950 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5951 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5952 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5953 | `	)` |
|       5 |  5954 |  |
|       - |  5955 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5956 | `	GenBlock *pBlock;` |
|       - |  5957 | `	sxu32 nGotoOfft;` |
|       - |  5958 | `	sxi32 rc;` |
|       - |  5959 | `	/* Attach the new function */` |
|  222125 |  5960 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  222125 |  5961 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5962 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5963 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5964 | `		return SXERR_ABORT;` |
|       - |  5965 | `	}` |
|  222125 |  5966 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5967 | `	/* Swap bytecode containers */` |
|  222125 |  5968 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  222125 |  5969 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5970 | `	/* Emit constructor property promotion prologue:` |
|       - |  5971 | `	 *   $this->NAME = $NAME;` |
|       - |  5972 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5973 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5974 | `	{` |
|  222125 |  5975 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5976 | `		sxu32 i;` |
|  358761 |  5977 | `		for( i = 0; i < nArg; i++ ){` |
|  136641 |  5978 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5979 | `			char *zSrc;` |
|       - |  5980 | `			sxu32 nSrc,nName;` |
|       - |  5981 | `			SySet sToken;` |
|       - |  5982 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5983 | `			sxi32 rcPromote;` |
|  136641 |  5984 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  136587 |  5985 | `				continue;` |
|       - |  5986 | `			}` |
|       - |  5987 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5988 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5989 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5990 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5991 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      59 |  5992 | `			nName = SyStringLength(&pArg->sName);` |
|      59 |  5993 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      59 |  5994 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      59 |  5995 | `			if( zSrc == 0 ){` |
|     ! 0 |  5996 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5997 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5998 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5999 | `				return SXERR_ABORT;` |
|       - |  6000 | `			}` |
|       - |  6001 | `			{` |
|      59 |  6002 | `				char *z = zSrc;` |
|      59 |  6003 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      59 |  6004 | `				z += sizeof("$this->")-1;` |
|      59 |  6005 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6006 | `				z += nName;` |
|      59 |  6007 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      59 |  6008 | `				z += sizeof(" = $")-1;` |
|      59 |  6009 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6010 | `				z += nName;` |
|      59 |  6011 | `				*z = 0;` |
|       - |  6012 | `			}` |
|      59 |  6013 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      59 |  6014 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      59 |  6015 | `			pTmpIn = pGen->pIn;` |
|      59 |  6016 | `			pTmpEnd = pGen->pEnd;` |
|      59 |  6017 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      59 |  6018 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      59 |  6019 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      59 |  6020 | `			pGen->pIn = pTmpIn;` |
|      59 |  6021 | `			pGen->pEnd = pTmpEnd;` |
|      59 |  6022 | `			SySetRelease(&sToken);` |
|      59 |  6023 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6024 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6025 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6026 | `				return SXERR_ABORT;` |
|       - |  6027 | `			}` |
|       - |  6028 | `			/* Discard the assignment result — this is a statement expression. */` |
|      59 |  6029 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      32 |  6030 | `		}` |
|       - |  6031 | `	}` |
|       - |  6032 | `	/* Compile the body */` |
|  222125 |  6033 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  6034 | `	/* Fix exception jumps now the destination is resolved */` |
|  222125 |  6035 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6036 | `	/* Emit the final return if not yet done */` |
|  222125 |  6037 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6038 | `	/* Fix gotos jumps now the destination is resolved */` |
|  222125 |  6039 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6040 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6041 | `	}` |
|  222125 |  6042 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6043 | `	/* Restore the default container */` |
|  222125 |  6044 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6045 | `	/* Leave function block */` |
|  222125 |  6046 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  222125 |  6047 | `	if( rc == SXERR_ABORT ){` |
|       - |  6048 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6049 | `		return SXERR_ABORT;` |
|       - |  6050 | `	}` |
|       - |  6051 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6052 | `	{` |
|  222125 |  6053 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6054 | `		sxu32 i;` |
| 4363009 |  6055 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4140989 |  6056 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     105 |  6057 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     105 |  6058 | `				break;` |
|       - |  6059 | `			}` |
| 2070447 |  6060 | `		}` |
|       - |  6061 | `	}` |
|       - |  6062 | `	/* All done, function body compiled */` |
|  222125 |  6063 | `	return SXRET_OK;` |
|  111065 |  6064 |  |
|       - |  6065 | `/*` |
|       - |  6066 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6067 | ` * According to the PHP language reference manual.` |
|       - |  6068 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6069 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6070 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6071 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6072 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6073 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6074 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6075 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6076 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6077 | ` *` |
|       - |  6078 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6079 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6080 | ` * on these extension.` |
|       - |  6081 | ` */` |
|       - |  6082 | `/*` |
|       - |  6083 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6084 | ` */` |
|     484 |  6085 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6086 |  |
|       - |  6087 | `	sxu32 i;` |
|    1337 |  6088 | `	for( i = 0; i < n; i++ ){` |
|    1149 |  6089 | `		int a = zA[i], b = zB[i];` |
|    1149 |  6090 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1149 |  6091 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1149 |  6092 | `		if( a != b ) return a - b;` |
|     429 |  6093 | `	}` |
|     193 |  6094 | `	return 0;` |
|     247 |  6095 |  |
|       - |  6096 | `/*` |
|       - |  6097 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6098 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6099 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6100 | ` */` |
|       - |  6101 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6102 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6103 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6104 |  |
|       - |  6105 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6106 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6107 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6108 |  |
|       - |  6109 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6110 | `struct PhlTypeAtom {` |
|       - |  6111 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6112 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6113 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6114 | `	sxu32 nCanon;` |
|       - |  6115 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6116 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6117 | `};` |
|       - |  6118 |  |
|       - |  6119 | `/*` |
|       - |  6120 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6121 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6122 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6123 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6124 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6125 | ` * already be consumed by the caller.` |
|       - |  6126 | ` */` |
|   79096 |  6127 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6128 |  |
|   79101 |  6129 | `	SyToken *pIn = pGen->pIn;` |
|   79101 |  6130 | `	SyZero(pOut, sizeof(*pOut));` |
|   79101 |  6131 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   79101 |  6132 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6133 | `		return SXERR_SYNTAX;` |
|       - |  6134 | `	}` |
|       - |  6135 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   79101 |  6136 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6137 | `		pIn++;` |
|       8 |  6138 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6139 | `			return SXERR_SYNTAX;` |
|       - |  6140 | `		}` |
|       3 |  6141 | `	}` |
|   79101 |  6142 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6143 | `		return SXERR_SYNTAX;` |
|       - |  6144 | `	}` |
|   79101 |  6145 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   64519 |  6146 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   64519 |  6147 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      32 |  6148 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   64505 |  6149 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      71 |  6150 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   64458 |  6151 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   18085 |  6152 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   55385 |  6153 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   46283 |  6154 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   23206 |  6155 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      33 |  6156 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      53 |  6157 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6158 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      25 |  6159 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       7 |  6160 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      11 |  6161 | `			pOut->nType = SXU32_HIGH;` |
|      11 |  6162 | `			pOut->sClass = pIn->sData;` |
|       7 |  6163 | `		}else{` |
|       3 |  6164 | `			return SXERR_SYNTAX;` |
|       - |  6165 | `		}` |
|   64517 |  6166 | `		pIn++;` |
|   32261 |  6167 | `	}else{` |
|       - |  6168 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6169 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   14587 |  6170 | `		SyString *pT = &pIn->sData;` |
|   14587 |  6171 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6172 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6173 | `			pIn++;` |
|   14573 |  6174 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     157 |  6175 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     157 |  6176 | `			pIn++;` |
|   14483 |  6177 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  6178 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  6179 | `			pIn++;` |
|       2 |  6180 | `		}else{` |
|       - |  6181 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   14405 |  6182 | `			SyToken *pFirst = pIn;` |
|   14405 |  6183 | `			SyToken *pLast = pIn;` |
|   14405 |  6184 | `			pOut->nType = SXU32_HIGH;` |
|   14405 |  6185 | `			pOut->sClass = pIn->sData;` |
|   14405 |  6186 | `			pIn++;` |
|   21603 |  6187 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   14408 |  6188 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6189 | `				pLast = &pIn[1];` |
|       3 |  6190 | `				pIn += 2;` |
|       1 |  6191 | `			}` |
|   14405 |  6192 | `			if( pLast != pFirst ){` |
|       3 |  6193 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6194 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6195 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6196 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6197 | `			}` |
|       - |  6198 | `		}` |
|       - |  6199 | `	}` |
|   79099 |  6200 | `	pGen->pIn = pIn;` |
|   79099 |  6201 | `	return SXRET_OK;` |
|   39553 |  6202 |  |
|       - |  6203 |  |
|       - |  6204 | `/*` |
|       - |  6205 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6206 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6207 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6208 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6209 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6210 | ` */` |
|   78942 |  6211 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6212 |  |
|       - |  6213 | `	int i;` |
|   78947 |  6214 | `	int nNonNull = 0;` |
|   78947 |  6215 | `	int bAnyIntersection = 0;` |
|       - |  6216 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   78947 |  6217 | `	sxu32 nMaxGroup = 0;` |
| 2605091 |  6218 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  158023 |  6219 | `	for( i = 0; i < nAtoms; i++ ){` |
|   79081 |  6220 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   79053 |  6221 | `			nNonNull++;` |
|   79053 |  6222 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   79053 |  6223 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   79053 |  6224 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   39524 |  6225 | `			}` |
|   39524 |  6226 | `		}` |
|   39543 |  6227 | `	}` |
|  157989 |  6228 | `	for( i = 0; i < nAtoms; i++ ){` |
|   79063 |  6229 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      19 |  6230 | `			bAnyIntersection = 1;` |
|      19 |  6231 | `			break;` |
|       - |  6232 | `		}` |
|   39526 |  6233 | `	}` |
|   78947 |  6234 | `	if( bAnyIntersection ){` |
|       - |  6235 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6236 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6237 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      19 |  6238 | `		sxu32 g, nGroups = 0;` |
|      19 |  6239 | `		int bFirstGroup = 1;` |
|      39 |  6240 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      39 |  6241 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      23 |  6242 | `			int bFirstMember = 1;` |
|       - |  6243 | `			int bWrap;` |
|      23 |  6244 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6245 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6246 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6247 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6248 | `			 * parens, matching PHP's canonical text. */` |
|      31 |  6249 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      23 |  6250 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      23 |  6251 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      71 |  6252 | `			for( i = 0; i < nAtoms; i++ ){` |
|      51 |  6253 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      39 |  6254 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      39 |  6255 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      37 |  6256 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      20 |  6257 | `				}else{` |
|       3 |  6258 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6259 | `				}` |
|      39 |  6260 | `				bFirstMember = 0;` |
|      21 |  6261 | `			}` |
|      23 |  6262 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      23 |  6263 | `			bFirstGroup = 0;` |
|      13 |  6264 | `		}` |
|      19 |  6265 | `		if( bNullable ){` |
|     ! 0 |  6266 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6267 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6268 | `		}` |
|      57 |  6269 | `		return;` |
|       - |  6270 | `	}` |
|   78931 |  6271 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6272 | `		/* Shorthand: ?T */` |
|      81 |  6273 | `		for( i = 0; i < nAtoms; i++ ){` |
|      81 |  6274 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      81 |  6275 | `			SyBlobAppend(pBlob, "?", 1);` |
|      81 |  6276 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      22 |  6277 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      13 |  6278 | `			}else{` |
|      63 |  6279 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6280 | `			}` |
|      81 |  6281 | `			return;` |
|     ! 0 |  6282 | `		}` |
|     ! 0 |  6283 | `	}` |
|       - |  6284 | `	{` |
|   78855 |  6285 | `		int bFirst = 1;` |
|       - |  6286 | `		/* 1) Classes in declaration order */` |
|  157807 |  6287 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78957 |  6288 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   14361 |  6289 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   14361 |  6290 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   14361 |  6291 | `				bFirst = 0;` |
|    7178 |  6292 | `			}` |
|   39481 |  6293 | `		}` |
|       - |  6294 | `		/* 2) Built-ins in canonical order */` |
|       - |  6295 | `		{` |
|       - |  6296 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6297 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6298 | `			int k;` |
|  551955 |  6299 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  882279 |  6300 | `				for( i = 0; i < nAtoms; i++ ){` |
|  473609 |  6301 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   64435 |  6302 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   64435 |  6303 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   64435 |  6304 | `						bFirst = 0;` |
|   64435 |  6305 | `						break;` |
|       - |  6306 | `					}` |
|  204592 |  6307 | `				}` |
|  236555 |  6308 | `			}` |
|       - |  6309 | `		}` |
|       - |  6310 | `		/* 3) null suffix */` |
|   78855 |  6311 | `		if( bNullable ){` |
|      20 |  6312 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      20 |  6313 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6314 | `		}` |
|       - |  6315 | `	}` |
|   39476 |  6316 |  |
|       - |  6317 |  |
|       - |  6318 | `/*` |
|       - |  6319 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6320 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6321 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6322 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6323 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6324 | ` * whether it was parenthesized.` |
|       - |  6325 | ` *` |
|       - |  6326 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6327 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6328 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6329 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6330 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6331 | ` */` |
|   79078 |  6332 | `static sxi32 GenStateParsePart(` |
|       - |  6333 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6334 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6335 |  |
|       - |  6336 | `	sxi32 rc;` |
|   79083 |  6337 | `	int nMembers = 0;` |
|   79083 |  6338 | `	int bParen = 0;` |
|   79083 |  6339 | `	*pnMembers = 0;` |
|   79083 |  6340 | `	*pbParen = 0;` |
|   79083 |  6341 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6342 | `		bParen = 1;` |
|       6 |  6343 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6344 | `	}` |
|   39539 |  6345 | `	for(;;){` |
|   79101 |  6346 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6347 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6348 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6349 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6350 | `		}` |
|   79101 |  6351 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   79101 |  6352 | `		if( rc != SXRET_OK ){` |
|       3 |  6353 | `			return rc;` |
|       - |  6354 | `		}` |
|   79099 |  6355 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   79099 |  6356 | `		(*pnAtoms)++;` |
|   79099 |  6357 | `		nMembers++;` |
|       - |  6358 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   79099 |  6359 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      24 |  6360 | `			SyToken *pNext = &pGen->pIn[1];` |
|      20 |  6361 | `			if( pNext < pGen->pEnd` |
|      24 |  6362 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      22 |  6363 | `				pGen->pIn++; /* skip '&' */` |
|      22 |  6364 | `				continue;` |
|       - |  6365 | `			}` |
|       1 |  6366 | `		}` |
|   79081 |  6367 | `		break;` |
|     ! 0 |  6368 | `	}` |
|   79081 |  6369 | `	if( bParen ){` |
|       6 |  6370 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6371 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6372 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6373 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6374 | `		}` |
|       6 |  6375 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6376 | `		if( nMembers < 2 ){` |
|     ! 0 |  6377 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6378 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6379 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6380 | `		}` |
|       2 |  6381 | `	}` |
|   79081 |  6382 | `	*pnMembers = nMembers;` |
|   79081 |  6383 | `	*pbParen = bParen;` |
|   79081 |  6384 | `	return SXRET_OK;` |
|   39544 |  6385 |  |
|       - |  6386 |  |
|       - |  6387 | `/*` |
|       - |  6388 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6389 | ` *` |
|       - |  6390 | ` * Outputs:` |
|       - |  6391 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6392 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6393 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6394 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6395 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6396 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6397 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6398 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6399 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6400 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6401 | ` *` |
|       - |  6402 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6403 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6404 | ` */` |
|   78954 |  6405 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6406 | `	ph7_gen_state *pGen,` |
|       - |  6407 | `	sxu32 *pnType,` |
|       - |  6408 | `	SyString *pClass,` |
|       - |  6409 | `	SySet *pAlts,` |
|       - |  6410 | `	sxi32 *piTypeFlags,` |
|       - |  6411 | `	SyString *pTypeText,` |
|       - |  6412 | `	int iNullableFlag,` |
|       - |  6413 | `	int iUnionFlag,` |
|       - |  6414 | `	int bAllowVoid,` |
|       - |  6415 | `	sxu32 nLine` |
|       5 |  6416 | `){` |
|       - |  6417 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   78959 |  6418 | `	int nAtoms = 0;` |
|   78959 |  6419 | `	int bShortNullable = 0;` |
|   78959 |  6420 | `	int bExplicitNull = 0;` |
|       - |  6421 | `	sxi32 rc;` |
|   78959 |  6422 | `	*pnType = 0;` |
|   78959 |  6423 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   78959 |  6424 | `	*piTypeFlags = 0;` |
|   78959 |  6425 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6426 |  |
|   78959 |  6427 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6428 | `		return SXRET_OK;` |
|       - |  6429 | `	}` |
|       - |  6430 | ``	/* Optional `?` shorthand prefix */`` |
|   78954 |  6431 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      71 |  6432 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      71 |  6433 | `		bShortNullable = 1;` |
|      71 |  6434 | `		pGen->pIn++;` |
|      71 |  6435 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6436 | `			return SXERR_SYNTAX;` |
|       - |  6437 | `		}` |
|      33 |  6438 | `	}` |
|       - |  6439 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6440 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6441 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6442 | `	{` |
|       - |  6443 | `		int nMembers, bParen;` |
|   78959 |  6444 | `		sxu32 iGroup = 0;` |
|   78959 |  6445 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   78959 |  6446 | `		if( rc != SXRET_OK ){` |
|       4 |  6447 | `			return rc;` |
|       - |  6448 | `		}` |
|       - |  6449 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6450 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6451 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6452 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6453 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  118616 |  6454 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   79145 |  6455 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     131 |  6456 | `			if( bShortNullable ){` |
|       - |  6457 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6458 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6459 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6460 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6461 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6462 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6463 | `			}` |
|     129 |  6464 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6465 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6466 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6467 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6468 | `			}` |
|     129 |  6469 | ``			pGen->pIn++; /* skip `\|` */`` |
|     129 |  6470 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     129 |  6471 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6472 | `				return rc;` |
|       - |  6473 | `			}` |
|       5 |  6474 | `		}` |
|   78955 |  6475 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6476 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6477 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6478 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6479 | `		}` |
|       - |  6480 | `	}` |
|       - |  6481 | `	/* Validation pass.` |
|       - |  6482 | `	 *` |
|       - |  6483 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6484 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6485 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6486 | `	 */` |
|       - |  6487 | `	{` |
|       - |  6488 | `		int i, j;` |
|   78955 |  6489 | `		int bHasNonNull = 0;` |
|   78955 |  6490 | `		int bAnyIntersection = 0;` |
|       - |  6491 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6492 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6493 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2605355 |  6494 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  158047 |  6495 | `		for( i = 0; i < nAtoms; i++ ){` |
|   79097 |  6496 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   39551 |  6497 | `		}` |
|  158009 |  6498 | `		for( i = 0; i < nAtoms; i++ ){` |
|   79077 |  6499 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   39532 |  6500 | `		}` |
|       - |  6501 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6502 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   78955 |  6503 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6504 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6505 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6506 | `			return SXERR_SYNTAX;` |
|       - |  6507 | `		}` |
|  158037 |  6508 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6509 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6510 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6511 | ``			 * `true`/`false` in an intersection). */`` |
|   79095 |  6512 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      38 |  6513 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      38 |  6514 | `				if( bClassLike ){` |
|      35 |  6515 | `					SyString *pC = &aAtoms[i].sClass;` |
|      32 |  6516 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      32 |  6517 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      32 |  6518 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      35 |  6519 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6520 | `						bClassLike = 0;` |
|     ! 0 |  6521 | `					}` |
|      16 |  6522 | `				}` |
|      38 |  6523 | `				if( !bClassLike ){` |
|       - |  6524 | `					const char *zName; sxu32 nName;` |
|       3 |  6525 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6526 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6527 | `					}else{` |
|       3 |  6528 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6529 | `					}` |
|       4 |  6530 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6531 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6532 | `						(int)nName, zName);` |
|       3 |  6533 | `					return SXERR_SYNTAX;` |
|       - |  6534 | `				}` |
|      16 |  6535 | `			}` |
|   79093 |  6536 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     157 |  6537 | `				if( nAtoms > 1 ){` |
|       3 |  6538 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6539 | `						"Void can only be used as a standalone type");` |
|       3 |  6540 | `					return SXERR_SYNTAX;` |
|       - |  6541 | `				}` |
|     155 |  6542 | `				if( !bAllowVoid ){` |
|     ! 0 |  6543 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6544 | `						"void cannot be used here");` |
|     ! 0 |  6545 | `					return SXERR_SYNTAX;` |
|       - |  6546 | `				}` |
|     155 |  6547 | `				if( bShortNullable ){` |
|     ! 0 |  6548 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6549 | `						"Void type cannot be nullable");` |
|     ! 0 |  6550 | `					return SXERR_SYNTAX;` |
|       - |  6551 | `				}` |
|      75 |  6552 | `			}` |
|   79091 |  6553 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6554 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6555 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6556 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6557 | `				 * does not return), and folding them would mislead any` |
|       - |  6558 | `				 * future return-enforcement work. */` |
|       3 |  6559 | `				if( nAtoms > 1 ){` |
|       3 |  6560 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6561 | `						"never can only be used as a standalone type");` |
|       3 |  6562 | `					return SXERR_SYNTAX;` |
|       - |  6563 | `				}` |
|     ! 0 |  6564 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6565 | `					"never type is not yet implemented");` |
|     ! 0 |  6566 | `				return SXERR_SYNTAX;` |
|       - |  6567 | `			}` |
|   79089 |  6568 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  6569 | `				bExplicitNull = 1;` |
|      18 |  6570 | `			}else{` |
|   79061 |  6571 | `				bHasNonNull = 1;` |
|       - |  6572 | `			}` |
|       - |  6573 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  6574 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  6575 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  6576 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  6577 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   79269 |  6578 | `			for( j = 0; j < i; j++ ){` |
|     187 |  6579 | `				int bDup = 0;` |
|     187 |  6580 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     359 |  6581 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     182 |  6582 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     187 |  6583 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     179 |  6584 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      40 |  6585 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      34 |  6586 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      37 |  6587 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      16 |  6588 | `								aAtoms[j].sClass.zString,` |
|      32 |  6589 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6590 | `							bDup = 1;` |
|     ! 0 |  6591 | `						}` |
|      21 |  6592 | `					}else{` |
|       3 |  6593 | `						bDup = 1;` |
|       - |  6594 | `					}` |
|      18 |  6595 | `				}` |
|     179 |  6596 | `				if( bDup ){` |
|       - |  6597 | `					const char *zName;` |
|       - |  6598 | `					sxu32 nName;` |
|       3 |  6599 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6600 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6601 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6602 | `					}else{` |
|       3 |  6603 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6604 | `						nName = aAtoms[i].nCanon;` |
|       - |  6605 | `					}` |
|       4 |  6606 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6607 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6608 | `					return SXERR_SYNTAX;` |
|       - |  6609 | `				}` |
|      91 |  6610 | `			}` |
|   39546 |  6611 | `		}` |
|   78947 |  6612 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6613 | `			if( bShortNullable ){` |
|       - |  6614 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6615 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6616 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6617 | `				return SXERR_SYNTAX;` |
|       - |  6618 | `			}` |
|       - |  6619 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6620 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6621 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6622 | `			 * atom, so set it here. */` |
|       7 |  6623 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6624 | `		}` |
|       - |  6625 | `	}` |
|       - |  6626 | `	/* Compute nullability flag */` |
|   78947 |  6627 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      97 |  6628 | `		*piTypeFlags \|= iNullableFlag;` |
|      46 |  6629 | `	}` |
|       - |  6630 | `	/* Build canonical type text */` |
|   78947 |  6631 | `	if( pTypeText ){` |
|       - |  6632 | `		SyBlob sBlob;` |
|   78947 |  6633 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  118386 |  6634 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   39471 |  6635 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   78947 |  6636 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  118193 |  6637 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   78792 |  6638 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   78797 |  6639 | `			if( zDup ){` |
|   78797 |  6640 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   39396 |  6641 | `			}` |
|   39396 |  6642 | `		}` |
|   78947 |  6643 | `		SyBlobRelease(&sBlob);` |
|   39471 |  6644 | `	}` |
|       - |  6645 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6646 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6647 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6648 | `	{` |
|   78947 |  6649 | `		int nNonNull = 0;` |
|   78947 |  6650 | `		int iNonNullIdx = -1;` |
|       - |  6651 | `		int i;` |
|  158023 |  6652 | `		for( i = 0; i < nAtoms; i++ ){` |
|   79081 |  6653 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   79053 |  6654 | `				nNonNull++;` |
|   79053 |  6655 | `				iNonNullIdx = i;` |
|   39524 |  6656 | `			}` |
|   39543 |  6657 | `		}` |
|   78947 |  6658 | `		if( nNonNull <= 1 ){` |
|       - |  6659 | `			/* Fast path: store as single type. */` |
|   78855 |  6660 | `			if( iNonNullIdx >= 0 ){` |
|   78849 |  6661 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   78849 |  6662 | `				if( pA->nType == SXU32_HIGH ){` |
|   21506 |  6663 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7167 |  6664 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   14339 |  6665 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   14339 |  6666 | `					*pnType = SXU32_HIGH;` |
|   14339 |  6667 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   71682 |  6668 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     155 |  6669 | `					*pnType = MEMOBJ_VOID;` |
|      80 |  6670 | `				}else{` |
|       - |  6671 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6672 | `					 * pass above rejects it as not-yet-implemented. */` |
|   64365 |  6673 | `					*pnType = pA->nType;` |
|       - |  6674 | `				}` |
|   39422 |  6675 | `			}` |
|   39430 |  6676 | `		}else{` |
|       - |  6677 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      97 |  6678 | `			*piTypeFlags \|= iUnionFlag;` |
|     311 |  6679 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6680 | `				ph7_type_alt sAlt;` |
|     219 |  6681 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     209 |  6682 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     209 |  6683 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     209 |  6684 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     116 |  6685 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      37 |  6686 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      79 |  6687 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      79 |  6688 | `					sAlt.nType = SXU32_HIGH;` |
|      79 |  6689 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      42 |  6690 | `				}else{` |
|     135 |  6691 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  6692 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6693 | `				}` |
|     209 |  6694 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     107 |  6695 | `			}` |
|       - |  6696 | `		}` |
|       - |  6697 | `	}` |
|   78947 |  6698 | `	return SXRET_OK;` |
|   39482 |  6699 |  |
|       - |  6700 |  |
|       - |  6701 | `/*` |
|       - |  6702 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6703 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6704 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6705 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6706 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6707 | `` *          and union types `: T\|U`.`` |
|       - |  6708 | ` */` |
|  314472 |  6709 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6710 |  |
|  314477 |  6711 | `	sxi32 iFlags = 0;` |
|       - |  6712 | `	sxi32 rc;` |
|       - |  6713 | `	sxu32 nLine;` |
|  314477 |  6714 | `	pFunc->nReturnType = 0;` |
|  314477 |  6715 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  314477 |  6716 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  314477 |  6717 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  313995 |  6718 | `		return SXRET_OK;` |
|       - |  6719 | `	}` |
|     487 |  6720 | `	pGen->pIn++; /* Skip ':' */` |
|     487 |  6721 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6722 | `		return SXRET_OK;` |
|       - |  6723 | `	}` |
|     487 |  6724 | `	nLine = pGen->pIn->nLine;` |
|     487 |  6725 | `	rc = GenStateParseUnionTypeDecl(` |
|     241 |  6726 | `		pGen,` |
|     241 |  6727 | `		&pFunc->nReturnType,` |
|     241 |  6728 | `		&pFunc->sReturnClass,` |
|     241 |  6729 | `		&pFunc->aReturnUnion,` |
|       - |  6730 | `		&iFlags,` |
|     241 |  6731 | `		&pFunc->sReturnTypeName,` |
|       - |  6732 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  6733 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  6734 | `		/* iUnionFlag */ 0,` |
|       - |  6735 | `		/* bAllowVoid */ 1,` |
|     241 |  6736 | `		nLine);` |
|     487 |  6737 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6738 | `		return SXERR_ABORT;` |
|       - |  6739 | `	}` |
|     487 |  6740 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6741 | `		/* Error already reported */` |
|     ! 0 |  6742 | `		return SXERR_SYNTAX;` |
|       - |  6743 | `	}` |
|     487 |  6744 | `	if( rc == SXERR_SYNTAX ){` |
|       6 |  6745 | `		if( pGen->pIn < pGen->pEnd ){` |
|       8 |  6746 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6747 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6748 | `				&pGen->pIn->sData);` |
|       4 |  6749 | `		}else{` |
|     ! 0 |  6750 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6751 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6752 | `		}` |
|       6 |  6753 | `		return SXERR_SYNTAX;` |
|       - |  6754 | `	}` |
|     483 |  6755 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     483 |  6756 | `	return SXRET_OK;` |
|  157241 |  6757 |  |
|       - |  6758 |  |
|   47412 |  6759 | `static sxi32 GenStateCompileFunc(` |
|       - |  6760 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6761 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6762 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6763 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6764 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6765 | `	)` |
|       5 |  6766 |  |
|       - |  6767 | `	ph7_vm_func *pFunc;` |
|       - |  6768 | `	SyToken *pEnd;` |
|       - |  6769 | `	sxu32 nLine;` |
|       - |  6770 | `	char *zName;` |
|       - |  6771 | `	sxi32 rc;` |
|       - |  6772 | `	/* Extract line number */` |
|   47417 |  6773 | `	nLine = pGen->pIn->nLine;` |
|       - |  6774 | `	/* Jump the left parenthesis '(' */` |
|   47417 |  6775 | `	pGen->pIn++;` |
|       - |  6776 | `	/* Delimit the function signature */` |
|   47417 |  6777 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   47417 |  6778 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6779 | `		/* Syntax error */` |
|       8 |  6780 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       8 |  6781 | `		if( rc == SXERR_ABORT ){` |
|       - |  6782 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6783 | `			return SXERR_ABORT;` |
|       - |  6784 | `		}` |
|       8 |  6785 | `		pGen->pIn = pGen->pEnd;` |
|       8 |  6786 | `		return SXRET_OK;` |
|       - |  6787 | `	}` |
|       - |  6788 | `	/* Create the function state */` |
|   47411 |  6789 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   47411 |  6790 | `	if( pFunc == 0 ){` |
|     ! 0 |  6791 | `		goto OutOfMem;` |
|       - |  6792 | `	}` |
|       - |  6793 | `	/* Build the function name, prepending namespace if active */` |
|   47418 |  6794 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6795 | `		SyBlob sFQN;` |
|       - |  6796 | `		sxu32 nLen;` |
|      16 |  6797 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6798 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6799 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6800 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6801 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6802 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6803 | `		SyBlobRelease(&sFQN);` |
|      16 |  6804 | `		if( zName == 0 ){` |
|     ! 0 |  6805 | `			goto OutOfMem;` |
|       - |  6806 | `		}` |
|      16 |  6807 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6808 | `	}else{` |
|   47397 |  6809 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   47397 |  6810 | `		if( zName == 0 ){` |
|     ! 0 |  6811 | `			goto OutOfMem;` |
|       - |  6812 | `		}` |
|   47397 |  6813 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6814 | `	}` |
|   47411 |  6815 | `	if( pGen->pIn < pEnd ){` |
|       - |  6816 | `		/* Collect function arguments */` |
|   32709 |  6817 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   32709 |  6818 | `		if( rc == SXERR_ABORT ){` |
|       - |  6819 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6820 | `			return SXERR_ABORT;` |
|       - |  6821 | `		}` |
|   16352 |  6822 | `	}` |
|       - |  6823 | `	/* Point past ')' and parse optional return type ': type' */` |
|   47411 |  6824 | `	pGen->pIn = &pEnd[1];` |
|       - |  6825 | `	{` |
|   47411 |  6826 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   47411 |  6827 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6828 | `			return SXERR_ABORT;` |
|   47411 |  6829 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       6 |  6830 | `			return SXERR_SYNTAX;` |
|       - |  6831 | `		}` |
|       - |  6832 | `	}` |
|   47407 |  6833 | `	if( bHandleClosure ){` |
|       - |  6834 | `		ph7_vm_func_closure_env sEnv;` |
|     297 |  6835 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     292 |  6836 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     160 |  6837 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      23 |  6838 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6839 | `				/* Closure,record environment variable */` |
|      23 |  6840 | `				pGen->pIn++;` |
|      23 |  6841 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6842 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6843 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6844 | `						return SXERR_ABORT;` |
|       - |  6845 | `					}` |
|     ! 0 |  6846 | `				}` |
|      23 |  6847 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6848 | `				/* Compile until we hit the first closing parenthesis */` |
|      45 |  6849 | `				while( pGen->pIn < pGen->pEnd ){` |
|      45 |  6850 | `					int iFlagsLocal = 0;` |
|      45 |  6851 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      23 |  6852 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      23 |  6853 | `						break;` |
|       - |  6854 | `					}` |
|      27 |  6855 | `					nLineLocal = pGen->pIn->nLine;` |
|      27 |  6856 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6857 | `						/* Pass by reference,record that */` |
|     ! 0 |  6858 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6859 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6860 | `							);` |
|     ! 0 |  6861 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6862 | `						pGen->pIn++;` |
|     ! 0 |  6863 | `					}` |
|      22 |  6864 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      27 |  6865 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6866 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6867 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6868 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6869 | `								return SXERR_ABORT;` |
|       - |  6870 | `							}` |
|       - |  6871 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6872 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6873 | `								pGen->pIn++;` |
|     ! 0 |  6874 | `							}` |
|     ! 0 |  6875 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6876 | `								pGen->pIn++;` |
|     ! 0 |  6877 | `							}` |
|     ! 0 |  6878 | `							break;` |
|       - |  6879 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6880 | `					}else{` |
|       - |  6881 | `						SyString *pNameLocal;` |
|       - |  6882 | `						char *zDup;` |
|       - |  6883 | `						/* Duplicate variable name */` |
|      27 |  6884 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      27 |  6885 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      27 |  6886 | `						if( zDup ){` |
|       - |  6887 | `							/* Zero the structure */` |
|      27 |  6888 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      27 |  6889 | `							sEnv.iFlags = iFlagsLocal;` |
|      27 |  6890 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      27 |  6891 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      27 |  6892 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6893 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6894 | `									got_this = 1;` |
|     ! 0 |  6895 | `							}` |
|       - |  6896 | `							/* Save imported variable */` |
|      27 |  6897 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      16 |  6898 | `						}else{` |
|     ! 0 |  6899 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6900 | `							 return SXERR_ABORT;` |
|       - |  6901 | `						}` |
|       - |  6902 | `					}` |
|      27 |  6903 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      33 |  6904 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6905 | `						/* Ignore trailing commas */` |
|       7 |  6906 | `						pGen->pIn++;` |
|       1 |  6907 | `					}` |
|       5 |  6908 | `				}` |
|      23 |  6909 | `				if( !got_this ){` |
|       - |  6910 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6911 | `					 * available to the closure environment.` |
|       - |  6912 | `					 */` |
|      23 |  6913 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      23 |  6914 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      23 |  6915 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      23 |  6916 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      23 |  6917 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 |  6918 | `				}` |
|      23 |  6919 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6920 | `					/* Mark as closure */` |
|      23 |  6921 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       9 |  6922 | `				}` |
|       9 |  6923 | `		}` |
|     146 |  6924 | `	}` |
|       - |  6925 | `	/* Compile the body */` |
|   47407 |  6926 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   47407 |  6927 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6928 | `		return SXERR_ABORT;` |
|       - |  6929 | `	}` |
|   47407 |  6930 | `	if( ppFunc ){` |
|     297 |  6931 | `		*ppFunc = pFunc;` |
|     146 |  6932 | `	}` |
|   47407 |  6933 | `	rc = SXRET_OK;` |
|   47407 |  6934 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6935 | `		/* Finally register the function */` |
|   47389 |  6936 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   23692 |  6937 | `	}` |
|   47407 |  6938 | `	if( rc == SXRET_OK ){` |
|   47407 |  6939 | `		return SXRET_OK;` |
|       - |  6940 | `	}` |
|       - |  6941 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6942 | `OutOfMem:` |
|       - |  6943 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6944 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6945 | `	 */` |
|     ! 0 |  6946 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6947 | `	return SXERR_ABORT;` |
|   23711 |  6948 |  |
|       - |  6949 | `/*` |
|       - |  6950 | ` * Compile a standard PHP function.` |
|       - |  6951 | ` *  Refer to the block-comment above for more information.` |
|       - |  6952 | ` */` |
|   47128 |  6953 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6954 |  |
|       - |  6955 | `	SyString *pName;` |
|       - |  6956 | `	sxi32 iFlags;` |
|       - |  6957 | `	sxu32 nLine;` |
|       - |  6958 | `	sxi32 rc;` |
|       - |  6959 |  |
|   47133 |  6960 | `	nLine = pGen->pIn->nLine;` |
|   47133 |  6961 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   47133 |  6962 | `	iFlags = 0;` |
|   47133 |  6963 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6964 | `		/* Return by reference,remember that */` |
|       7 |  6965 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6966 | `		/* Jump the '&' token */` |
|       7 |  6967 | `		pGen->pIn++;` |
|       3 |  6968 | `	}` |
|   47133 |  6969 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6970 | `		/* Invalid function name */` |
|       8 |  6971 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       8 |  6972 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6973 | `			return SXERR_ABORT;` |
|       - |  6974 | `		}` |
|       - |  6975 | `		/* Sychronize with the next semi-colon or braces*/` |
|      22 |  6976 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      16 |  6977 | `			pGen->pIn++;` |
|       2 |  6978 | `		}` |
|       8 |  6979 | `		return SXRET_OK;` |
|       - |  6980 | `	}` |
|   47127 |  6981 | `	pName = &pGen->pIn->sData;` |
|   47127 |  6982 | `	nLine = pGen->pIn->nLine;` |
|       - |  6983 | `	/* Jump the function name */` |
|   47127 |  6984 | `	pGen->pIn++;` |
|   47127 |  6985 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6986 | `		/* Syntax error */` |
|       3 |  6987 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6988 | `		if( rc == SXERR_ABORT ){` |
|       - |  6989 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6990 | `			return SXERR_ABORT;` |
|       - |  6991 | `		}` |
|       - |  6992 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6993 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6994 | `			pGen->pIn++;` |
|     ! 0 |  6995 | `		}` |
|       3 |  6996 | `		return SXRET_OK;` |
|       - |  6997 | `	}` |
|       - |  6998 | `	/* Compile function body */` |
|   47125 |  6999 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   47125 |  7000 | `	return rc;` |
|   23569 |  7001 |  |
|       - |  7002 | `/*` |
|       - |  7003 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7004 | ` * According to the PHP language reference manual` |
|       - |  7005 | ` *  Visibility:` |
|       - |  7006 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7007 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7008 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7009 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7010 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7011 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7012 | ` */` |
|  342114 |  7013 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7014 |  |
|  342119 |  7015 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   21379 |  7016 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  320745 |  7017 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   46131 |  7018 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7019 | `	}` |
|       - |  7020 | `	/* Assume public by default */` |
|  274619 |  7021 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  171062 |  7022 |  |
|       - |  7023 | `/*` |
|       - |  7024 | ` * Compile a class constant.` |
|       - |  7025 | ` * According to the PHP language reference manual` |
|       - |  7026 | ` *  Class Constants` |
|       - |  7027 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7028 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7029 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7030 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7031 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7032 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7033 | ` * Symisc eXtension.` |
|       - |  7034 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7035 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7036 | ` *  Example:` |
|       - |  7037 | ` *   class Test{` |
|       - |  7038 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7039 | ` *   };` |
|       - |  7040 | ` *   var_dump(TEST::MyConst);` |
|       - |  7041 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7042 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7043 | ` */` |
|       - |  7044 | `/*` |
|       - |  7045 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7046 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7047 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7048 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7049 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7050 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7051 | ` */` |
|      78 |  7052 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7053 |  |
|       - |  7054 | `	SyToken *p0, *p1;` |
|      83 |  7055 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7056 | `		return 0;` |
|       - |  7057 | `	}` |
|      83 |  7058 | `	p0 = pGen->pIn;` |
|       - |  7059 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      83 |  7060 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7061 | `		return 1;` |
|       - |  7062 | `	}` |
|      83 |  7063 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7064 | `		return 1;` |
|       - |  7065 | `	}` |
|       - |  7066 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7067 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7068 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      79 |  7069 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      79 |  7070 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      79 |  7071 | `		if( p1 ){` |
|      79 |  7072 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      24 |  7073 | `				return 1;` |
|       - |  7074 | `			}` |
|      59 |  7075 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7076 | `				return 1;` |
|       - |  7077 | `			}` |
|      25 |  7078 | `		}` |
|      25 |  7079 | `	}` |
|      55 |  7080 | `	return 0;` |
|      44 |  7081 |  |
|       - |  7082 | `/*` |
|       - |  7083 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7084 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7085 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7086 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7087 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7088 | ` * share the same backing.` |
|       - |  7089 | ` */` |
|     206 |  7090 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7091 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7092 |  |
|     211 |  7093 | `	pAttr->nType = nType;` |
|     211 |  7094 | `	pAttr->sClass = *pClass;` |
|     211 |  7095 | `	pAttr->sTypeName = *pTypeName;` |
|     211 |  7096 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7097 | `		sxu32 i;` |
|      66 |  7098 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      46 |  7099 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      46 |  7100 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      25 |  7101 | `		}` |
|      10 |  7102 | `	}` |
|     211 |  7103 |  |
|      78 |  7104 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7105 |  |
|      83 |  7106 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7107 | `	SySet *pInstrContainer;` |
|       - |  7108 | `	ph7_class_attr *pCons;` |
|       - |  7109 | `	SyString *pName;` |
|       - |  7110 | `	sxi32 rc;` |
|      83 |  7111 | `	sxu32 nType = 0;` |
|       - |  7112 | `	SyString sTypeClass;` |
|       - |  7113 | `	SyString sTypeText;` |
|       - |  7114 | `	SySet aUnionAlts;` |
|      83 |  7115 | `	sxi32 iTypeFlags = 0;` |
|      83 |  7116 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      83 |  7117 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      83 |  7118 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7119 | `	/* Extract visibility level */` |
|      83 |  7120 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7121 | `	/* Mark as constant */` |
|      83 |  7122 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      83 |  7123 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7124 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7125 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|      97 |  7126 | `	if( GenStateClassConstHasType(pGen) ){` |
|      46 |  7127 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      28 |  7128 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7129 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7130 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7131 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7132 | `		 * and success paths release. */` |
|      32 |  7133 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7134 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7135 | `			goto Synchronize;` |
|      32 |  7136 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7137 | `			return SXERR_ABORT;` |
|      32 |  7138 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7139 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7140 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7141 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7142 | `				return SXERR_ABORT;` |
|       - |  7143 | `			}` |
|     ! 0 |  7144 | `			goto Synchronize;` |
|       - |  7145 | `		}` |
|      32 |  7146 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      14 |  7147 | `	}` |
|      39 |  7148 | `loop:` |
|      85 |  7149 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7150 | `		/* Invalid constant name */` |
|     ! 0 |  7151 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7152 | `		if( rc == SXERR_ABORT ){` |
|       - |  7153 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7154 | `			return SXERR_ABORT;` |
|       - |  7155 | `		}` |
|     ! 0 |  7156 | `		goto Synchronize;` |
|       - |  7157 | `	}` |
|       - |  7158 | `	/* Peek constant name */` |
|      85 |  7159 | `	pName = &pGen->pIn->sData;` |
|       - |  7160 | `	/* Make sure the constant name isn't reserved */` |
|      85 |  7161 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7162 | `		/* Reserved constant name */` |
|     ! 0 |  7163 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7164 | `		if( rc == SXERR_ABORT ){` |
|       - |  7165 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7166 | `			return SXERR_ABORT;` |
|       - |  7167 | `		}` |
|     ! 0 |  7168 | `		goto Synchronize;` |
|       - |  7169 | `	}` |
|       - |  7170 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      85 |  7171 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      46 |  7172 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      28 |  7173 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      14 |  7174 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      32 |  7175 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7176 | `			return SXERR_ABORT;` |
|      32 |  7177 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7178 | `			goto Synchronize;` |
|       - |  7179 | `		}` |
|      13 |  7180 | `	}` |
|       - |  7181 | `	/* Advance the stream cursor */` |
|      83 |  7182 | `	pGen->pIn++;` |
|      83 |  7183 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7184 | `		/* Invalid declaration */` |
|     ! 0 |  7185 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7186 | `		if( rc == SXERR_ABORT ){` |
|       - |  7187 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7188 | `			return SXERR_ABORT;` |
|       - |  7189 | `		}` |
|     ! 0 |  7190 | `		goto Synchronize;` |
|       - |  7191 | `	}` |
|      83 |  7192 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7193 | `	/* Allocate a new class attribute */` |
|      83 |  7194 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      83 |  7195 | `	if( pCons == 0 ){` |
|     ! 0 |  7196 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7197 | `		return SXERR_ABORT;` |
|       - |  7198 | `	}` |
|      83 |  7199 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      29 |  7200 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      13 |  7201 | `	}` |
|       - |  7202 | `	/* Swap bytecode container */` |
|      83 |  7203 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      83 |  7204 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7205 | `	/* Compile constant value.` |
|       - |  7206 | `	 */` |
|      83 |  7207 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      83 |  7208 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7209 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7210 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7211 | `			return SXERR_ABORT;` |
|       - |  7212 | `		}` |
|       1 |  7213 | `	}` |
|       - |  7214 | `	/* Emit the done instruction */` |
|      83 |  7215 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      83 |  7216 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      83 |  7217 | `	if( rc == SXERR_ABORT ){` |
|       - |  7218 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7219 | `		return SXERR_ABORT;` |
|       - |  7220 | `	}` |
|       - |  7221 | `	/* All done,install the constant */` |
|      83 |  7222 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      83 |  7223 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7224 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7225 | `		return SXERR_ABORT;` |
|       - |  7226 | `	}` |
|      83 |  7227 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7228 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7229 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7230 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7231 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7232 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7233 | `				pTok--;` |
|     ! 0 |  7234 | `			}` |
|     ! 0 |  7235 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7236 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7237 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7238 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7239 | `				return SXERR_ABORT;` |
|       - |  7240 | `			}` |
|     ! 0 |  7241 | `		}else{` |
|       3 |  7242 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7243 | `				goto loop;` |
|       - |  7244 | `			}` |
|       - |  7245 | `		}` |
|     ! 0 |  7246 | `	}` |
|      81 |  7247 | `	SySetRelease(&aUnionAlts);` |
|      81 |  7248 | `	return SXRET_OK;` |
|       1 |  7249 | `Synchronize:` |
|       3 |  7250 | `	SySetRelease(&aUnionAlts);` |
|       - |  7251 | `	/* Synchronize with the first semi-colon */` |
|       9 |  7252 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  7253 | `		pGen->pIn++;` |
|       1 |  7254 | `	}` |
|       3 |  7255 | `	return SXERR_CORRUPT;` |
|      44 |  7256 |  |
|       - |  7257 | `/*` |
|       - |  7258 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7259 | ` * According to the PHP language reference manual` |
|       - |  7260 | ` *  Properties` |
|       - |  7261 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7262 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7263 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7264 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7265 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7266 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7267 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7268 | ` * Symisc eXtension.` |
|       - |  7269 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7270 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7271 | ` *  Example:` |
|       - |  7272 | ` *   class Test{` |
|       - |  7273 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7274 | ` *   };` |
|       - |  7275 | ` *   var_dump(TEST::myVar);` |
|       - |  7276 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7277 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7278 | ` */` |
|       - |  7279 | `/*` |
|       - |  7280 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7281 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7282 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7283 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7284 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7285 | ` */` |
|  185406 |  7286 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7287 |  |
|  185411 |  7288 | `	SyToken *p = pStart;` |
|  185411 |  7289 | `	int bFirst = 1;` |
|  185411 |  7290 | `	if( p >= pEnd ) return 0;` |
|       - |  7291 | ``	/* Optional nullable `?` shorthand. */`` |
|  185411 |  7292 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      18 |  7293 | `		p++;` |
|      18 |  7294 | `		if( p >= pEnd ) return 0;` |
|       8 |  7295 | `	}` |
|       - |  7296 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7297 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7298 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7299 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   92703 |  7300 | `	for(;;){` |
|  185429 |  7301 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7302 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7303 | `			p++;` |
|       9 |  7304 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7305 | `			if( p >= pEnd ) return 0;` |
|       3 |  7306 | `			p++; /* skip ')' */` |
|       2 |  7307 | `		}else{` |
|       - |  7308 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7309 | ``			 * then any `&`-joined intersection members. */`` |
|  185427 |  7310 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  185427 |  7311 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7312 | `				return 0;` |
|       - |  7313 | `			}` |
|       - |  7314 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7315 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7316 | `			 * may still appear at the initial dispatch site). */` |
|  185427 |  7317 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  185381 |  7318 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  185376 |  7319 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   10852 |  7320 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  185227 |  7321 | `					return 0;` |
|       - |  7322 | `				}` |
|      77 |  7323 | `			}` |
|     205 |  7324 | `			p++;` |
|     207 |  7325 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7326 | `				p += 2;` |
|       1 |  7327 | `			}` |
|     303 |  7328 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     208 |  7329 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7330 | `				p++; /* skip '&' */` |
|       3 |  7331 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7332 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7333 | `				p++;` |
|       3 |  7334 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7335 | `					p += 2;` |
|     ! 0 |  7336 | `				}` |
|       1 |  7337 | `			}` |
|       - |  7338 | `		}` |
|     207 |  7339 | `		bFirst = 0;` |
|     202 |  7340 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7341 | `			&& p->sData.zString[0] == '\|' ){` |
|      22 |  7342 | ``			p++; /* next `\|`-separated part */`` |
|      22 |  7343 | `			continue;` |
|       - |  7344 | `		}` |
|     189 |  7345 | `		break;` |
|     ! 0 |  7346 | `	}` |
|     189 |  7347 | `	if( p >= pEnd ) return 0;` |
|     189 |  7348 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   92708 |  7349 |  |
|       - |  7350 |  |
|       - |  7351 | `/*` |
|       - |  7352 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7353 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7354 | ` * if not). Recognized forms:` |
|       - |  7355 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7356 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7357 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7358 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7359 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7360 | ` * on unrecoverable error.` |
|       - |  7361 | ` *` |
|       - |  7362 | ` * When a type is parsed:` |
|       - |  7363 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7364 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7365 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7366 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7367 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7368 | ` */` |
|     184 |  7369 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7370 | `	ph7_gen_state *pGen,` |
|       - |  7371 | `	sxu32 *pnType,` |
|       - |  7372 | `	SyString *pClass,` |
|       - |  7373 | `	sxi32 *piTypeFlags,` |
|       - |  7374 | `	SyString *pTypeText,` |
|       - |  7375 | `	SySet *pAlts` |
|       5 |  7376 | `){` |
|     189 |  7377 | `	sxi32 iFlags = 0;` |
|       - |  7378 | `	sxi32 rc;` |
|     189 |  7379 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7380 | `		return SXRET_OK;` |
|       - |  7381 | `	}` |
|       - |  7382 | `	/* If the first token is '$', there's no type */` |
|     189 |  7383 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7384 | `		return SXRET_OK;` |
|       - |  7385 | `	}` |
|     189 |  7386 | `	rc = GenStateParseUnionTypeDecl(` |
|      92 |  7387 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7388 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7389 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7390 | `		/* bAllowVoid */ 0,` |
|     184 |  7391 | `		pGen->pIn->nLine);` |
|     189 |  7392 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7393 | `		return rc;` |
|       - |  7394 | `	}` |
|       - |  7395 | `	/* Verify next token is '$' (start of property name) */` |
|     189 |  7396 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7397 | `		return SXERR_SYNTAX;` |
|       - |  7398 | `	}` |
|     189 |  7399 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     189 |  7400 | `	return SXRET_OK;` |
|      97 |  7401 |  |
|       - |  7402 |  |
|       - |  7403 | `/*` |
|       - |  7404 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7405 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7406 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7407 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7408 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7409 | ` * by the type parser itself before reaching here.` |
|       - |  7410 | ` *` |
|       - |  7411 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7412 | ` * use in the error message.` |
|       - |  7413 | ` */` |
|     326 |  7414 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7415 | `	sxu32 nType,` |
|       - |  7416 | `	const SyString *pClass,` |
|       - |  7417 | `	const char **pzName,` |
|       - |  7418 | `	sxu32 *pnName)` |
|       5 |  7419 |  |
|       - |  7420 | `	const char *z;` |
|       - |  7421 | `	sxu32 n;` |
|     331 |  7422 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     277 |  7423 | `		return 0;` |
|       - |  7424 | `	}` |
|      59 |  7425 | `	z = pClass->zString;` |
|      59 |  7426 | `	n = pClass->nByte;` |
|      59 |  7427 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7428 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7429 | `	}` |
|       - |  7430 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7431 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7432 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      52 |  7433 | `	return 0;` |
|     168 |  7434 |  |
|       - |  7435 |  |
|       - |  7436 | `/*` |
|       - |  7437 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7438 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7439 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7440 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7441 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7442 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7443 | ` *` |
|       - |  7444 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7445 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7446 | ` */` |
|     268 |  7447 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7448 | `	ph7_gen_state *pGen,` |
|       - |  7449 | `	ph7_class *pClass,` |
|       - |  7450 | `	const SyString *pMemberName,` |
|       - |  7451 | `	sxu32 nType,` |
|       - |  7452 | `	const SyString *pTypeClass,` |
|       - |  7453 | `	const SyString *pTypeText,` |
|       - |  7454 | `	SySet *pUnionAlts,` |
|       - |  7455 | `	const char *zErrFmt,` |
|       - |  7456 | `	sxu32 nLine)` |
|       5 |  7457 |  |
|     273 |  7458 | `	const char *zBad = 0;` |
|     273 |  7459 | `	sxu32 nBad = 0;` |
|       - |  7460 | `	SyString sFallback;` |
|       - |  7461 | `	const SyString *pBad;` |
|       - |  7462 | `	sxi32 rc;` |
|     273 |  7463 | `	int bDisallowed = 0;` |
|     273 |  7464 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7465 | `		bDisallowed = 1;` |
|     271 |  7466 | `	}else if( pUnionAlts ){` |
|       - |  7467 | `		sxu32 i;` |
|      88 |  7468 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      62 |  7469 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      62 |  7470 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7471 | `				bDisallowed = 1;` |
|       3 |  7472 | `				break;` |
|       - |  7473 | `			}` |
|      32 |  7474 | `		}` |
|      14 |  7475 | `	}` |
|     273 |  7476 | `	if( !bDisallowed ){` |
|     267 |  7477 | `		return SXRET_OK;` |
|       - |  7478 | `	}` |
|       - |  7479 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7480 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7481 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7482 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7483 | `		pBad = pTypeText;` |
|       5 |  7484 | `	}else{` |
|     ! 0 |  7485 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7486 | `		pBad = &sFallback;` |
|       - |  7487 | `	}` |
|      11 |  7488 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7489 | `		zErrFmt,` |
|       3 |  7490 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7491 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7492 | `		return SXERR_ABORT;` |
|       - |  7493 | `	}` |
|       8 |  7494 | `	return SXERR_SYNTAX;` |
|     139 |  7495 |  |
|       - |  7496 | `/*` |
|       - |  7497 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7498 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7499 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7500 | ` * than promoted to a lexer keyword.` |
|       - |  7501 | ` */` |
| 1641336 |  7502 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7503 |  |
| 1674877 |  7504 | `	return (pTok->nType & PH7_TK_ID)` |
|  854204 |  7505 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1674872 |  7506 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7507 |  |
|   75108 |  7508 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7509 |  |
|   75113 |  7510 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7511 | `	ph7_class_attr *pAttr;` |
|       - |  7512 | `	SyString *pName;` |
|       - |  7513 | `	sxi32 rc;` |
|   75113 |  7514 | `	sxu32 nType = 0;` |
|       - |  7515 | `	SyString sTypeClass;` |
|       - |  7516 | `	SyString sTypeText;` |
|       - |  7517 | `	SySet aUnionAlts;` |
|   75113 |  7518 | `	sxi32 iTypeFlags = 0;` |
|   75113 |  7519 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   75113 |  7520 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   75113 |  7521 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7522 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7523 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7524 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   75113 |  7525 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7526 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7527 | `	}` |
|       - |  7528 | `	/* Extract visibility level */` |
|   75113 |  7529 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7530 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   75205 |  7531 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     189 |  7532 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     189 |  7533 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7534 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7535 | `			goto Synchronize;` |
|     189 |  7536 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7537 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7538 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7539 | `				&pGen->pIn->sData);` |
|     ! 0 |  7540 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7541 | `				return SXERR_ABORT;` |
|       - |  7542 | `			}` |
|     ! 0 |  7543 | `			goto Synchronize;` |
|     189 |  7544 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7545 | `			return SXERR_ABORT;` |
|       - |  7546 | `		}` |
|      92 |  7547 | `	}` |
|     ! 0 |  7548 | `loop:` |
|   75117 |  7549 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7550 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7551 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7552 | `			return SXERR_ABORT;` |
|       - |  7553 | `		}` |
|     ! 0 |  7554 | `		goto Synchronize;` |
|       - |  7555 | `	}` |
|   75117 |  7556 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   75117 |  7557 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7558 | `		/* Invalid attribute name */` |
|     ! 0 |  7559 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7560 | `		if( rc == SXERR_ABORT ){` |
|       - |  7561 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7562 | `			return SXERR_ABORT;` |
|       - |  7563 | `		}` |
|     ! 0 |  7564 | `		goto Synchronize;` |
|       - |  7565 | `	}` |
|       - |  7566 | `	/* Peek attribute name */` |
|   75117 |  7567 | `	pName = &pGen->pIn->sData;` |
|       - |  7568 | `	/* Advance the stream cursor */` |
|   75117 |  7569 | `	pGen->pIn++;` |
|   75117 |  7570 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7571 | `		/* Invalid declaration */` |
|       3 |  7572 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7573 | `		if( rc == SXERR_ABORT ){` |
|       - |  7574 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7575 | `			return SXERR_ABORT;` |
|       - |  7576 | `		}` |
|       3 |  7577 | `		goto Synchronize;` |
|       - |  7578 | `	}` |
|       - |  7579 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7580 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   75115 |  7581 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7582 | `		const char *zRoErr = 0;` |
|      39 |  7583 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7584 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7585 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7586 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7587 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7588 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7589 | `		}` |
|      39 |  7590 | `		if( zRoErr ){` |
|      13 |  7591 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7592 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7593 | `				return SXERR_ABORT;` |
|       - |  7594 | `			}` |
|      13 |  7595 | `			goto Synchronize;` |
|       - |  7596 | `		}` |
|      12 |  7597 | `	}` |
|       - |  7598 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7599 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7600 | `	 * by the type parser. */` |
|   75105 |  7601 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     278 |  7602 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7603 | `			&sTypeText,` |
|     182 |  7604 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      91 |  7605 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     187 |  7606 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7607 | `			return SXERR_ABORT;` |
|     187 |  7608 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7609 | `			goto Synchronize;` |
|       - |  7610 | `		}` |
|      91 |  7611 | `	}` |
|       - |  7612 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   75105 |  7613 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7614 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7615 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7616 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7617 | `			return SXERR_ABORT;` |
|       - |  7618 | `		}` |
|       3 |  7619 | `		goto Synchronize;` |
|       - |  7620 | `	}` |
|       - |  7621 | `	/* Allocate a new class attribute */` |
|   75103 |  7622 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   75103 |  7623 | `	if( pAttr == 0 ){` |
|     ! 0 |  7624 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7625 | `		return SXERR_ABORT;` |
|       - |  7626 | `	}` |
|   75103 |  7627 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     185 |  7628 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      90 |  7629 | `	}` |
|   75103 |  7630 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7631 | `		SySet *pInstrContainer;` |
|   21759 |  7632 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7633 | `		/* Swap bytecode container */` |
|   21759 |  7634 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   21759 |  7635 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7636 | `		/* Compile attribute value.` |
|       - |  7637 | `		 */` |
|   21759 |  7638 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   21759 |  7639 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7640 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7641 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7642 | `				return SXERR_ABORT;` |
|       - |  7643 | `			}` |
|     ! 0 |  7644 | `		}` |
|       - |  7645 | `		/* Emit the done instruction */` |
|   21759 |  7646 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   21759 |  7647 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   10877 |  7648 | `	}` |
|       - |  7649 | `	/* All done,install the attribute */` |
|   75103 |  7650 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   75103 |  7651 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7652 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7653 | `		return SXERR_ABORT;` |
|       - |  7654 | `	}` |
|   75103 |  7655 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7656 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7657 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7658 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7659 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7660 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7661 | `				pTok--;` |
|     ! 0 |  7662 | `			}` |
|     ! 0 |  7663 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7664 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7665 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7666 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7667 | `				return SXERR_ABORT;` |
|       - |  7668 | `			}` |
|     ! 0 |  7669 | `		}else{` |
|       5 |  7670 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7671 | `				goto loop;` |
|       - |  7672 | `			}` |
|       - |  7673 | `		}` |
|     ! 0 |  7674 | `	}` |
|   75099 |  7675 | `	SySetRelease(&aUnionAlts);` |
|   75099 |  7676 | `	return SXRET_OK;` |
|       7 |  7677 | `Synchronize:` |
|       - |  7678 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7679 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      16 |  7680 | `		pGen->pIn++;` |
|       2 |  7681 | `	}` |
|      17 |  7682 | `	SySetRelease(&aUnionAlts);` |
|      17 |  7683 | `	return SXERR_CORRUPT;` |
|   37559 |  7684 |  |
|       - |  7685 | `/*` |
|       - |  7686 | ` * Compile a class method.` |
|       - |  7687 | ` *` |
|       - |  7688 | ` * Refer to the official documentation for more information` |
|       - |  7689 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7690 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7691 | ` * overloading and many more.` |
|       - |  7692 | ` */` |
|  266928 |  7693 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7694 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7695 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7696 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7697 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7698 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7699 | `	)` |
|       5 |  7700 |  |
|  266933 |  7701 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7702 | `	ph7_class_method *pMeth;` |
|       - |  7703 | `	sxi32 iFuncFlags;` |
|       - |  7704 | `	SyString *pName;` |
|       - |  7705 | `	SyToken *pEnd;` |
|       - |  7706 | `	sxi32 rc;` |
|       - |  7707 | `	/* Extract visibility level */` |
|  266933 |  7708 | `	iProtection = GetProtectionLevel(iProtection);` |
|  266933 |  7709 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  266933 |  7710 | `	iFuncFlags = 0;` |
|  266933 |  7711 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7712 | `		/* Invalid method name */` |
|     ! 0 |  7713 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7714 | `		if( rc == SXERR_ABORT ){` |
|       - |  7715 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7716 | `			return SXERR_ABORT;` |
|       - |  7717 | `		}` |
|     ! 0 |  7718 | `		goto Synchronize;` |
|       - |  7719 | `	}` |
|  266933 |  7720 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7721 | `		/* Return by reference,remember that */` |
|     ! 0 |  7722 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7723 | `		/* Jump the '&' token */` |
|     ! 0 |  7724 | `		pGen->pIn++;` |
|     ! 0 |  7725 | `	}` |
|  266933 |  7726 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7727 | `		/* Invalid method name */` |
|     ! 0 |  7728 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7729 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7730 | `			return SXERR_ABORT;` |
|       - |  7731 | `		}` |
|     ! 0 |  7732 | `		goto Synchronize;` |
|       - |  7733 | `	}` |
|       - |  7734 | `	/* Peek method name */` |
|  266933 |  7735 | `	pName = &pGen->pIn->sData;` |
|  266933 |  7736 | `	nLine = pGen->pIn->nLine;` |
|       - |  7737 | `	/* Jump the method name */` |
|  266933 |  7738 | `	pGen->pIn++;` |
|  266933 |  7739 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7740 | `		/* Abstract method */` |
|   92203 |  7741 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7742 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7743 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7744 | `				&pClass->sName,pName);` |
|     ! 0 |  7745 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7746 | `				return SXERR_ABORT;` |
|       - |  7747 | `			}` |
|     ! 0 |  7748 | `		}` |
|       - |  7749 | `		/* Assemble method signature only */` |
|   92203 |  7750 | `		doBody = FALSE;` |
|   46099 |  7751 | `	}` |
|  266933 |  7752 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7753 | `		/* Syntax error */` |
|     ! 0 |  7754 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7755 | `		if( rc == SXERR_ABORT ){` |
|       - |  7756 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7757 | `			return SXERR_ABORT;` |
|       - |  7758 | `		}` |
|     ! 0 |  7759 | `		goto Synchronize;` |
|       - |  7760 | `	}` |
|       - |  7761 | `	/* Allocate a new class_method instance */` |
|  266933 |  7762 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  266933 |  7763 | `	if( pMeth == 0 ){` |
|     ! 0 |  7764 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7765 | `		return SXERR_ABORT;` |
|       - |  7766 | `	}` |
|       - |  7767 | `	/* Jump the left parenthesis '(' */` |
|  266933 |  7768 | `	pGen->pIn++;` |
|  266933 |  7769 | `	pEnd = 0; /* cc warning */` |
|       - |  7770 | `	/* Delimit the method signature */` |
|  266933 |  7771 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  266933 |  7772 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7773 | `		/* Syntax error */` |
|       3 |  7774 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7775 | `		if( rc == SXERR_ABORT ){` |
|       - |  7776 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7777 | `			return SXERR_ABORT;` |
|       - |  7778 | `		}` |
|       3 |  7779 | `		goto Synchronize;` |
|       - |  7780 | `	}` |
|       - |  7781 | `	{` |
|  266931 |  7782 | `		int bIsCtor = 0;` |
|  266931 |  7783 | `		int bAbstractCtor = 0;` |
|  266926 |  7784 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  158397 |  7785 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  256217 |  7786 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   21433 |  7787 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7788 | `				bAbstractCtor = 1;` |
|       2 |  7789 | `			}else{` |
|   21431 |  7790 | `				bIsCtor = 1;` |
|       - |  7791 | `			}` |
|   10714 |  7792 | `		}` |
|  266931 |  7793 | `		if( pGen->pIn < pEnd ){` |
|       - |  7794 | `			/* Collect method arguments */` |
|   71347 |  7795 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   71347 |  7796 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7797 | `				return SXERR_ABORT;` |
|       - |  7798 | `			}` |
|   35671 |  7799 | `		}` |
|       - |  7800 | `	}` |
|       - |  7801 | `	/* Point past ')' and parse optional return type ': type' */` |
|  266931 |  7802 | `	pGen->pIn = &pEnd[1];` |
|       - |  7803 | `	{` |
|  266931 |  7804 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  266931 |  7805 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7806 | `			return SXERR_ABORT;` |
|  266931 |  7807 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7808 | `			goto Synchronize;` |
|       - |  7809 | `		}` |
|       - |  7810 | `	}` |
|       - |  7811 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7812 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7813 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7814 | `	{` |
|  266931 |  7815 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7816 | `		sxu32 i;` |
|  387999 |  7817 | `		for( i = 0; i < nArg; i++ ){` |
|  121083 |  7818 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7819 | `			ph7_class_attr *pAttr;` |
|  121083 |  7820 | `			sxi32 iAttrFlags = 0;` |
|       - |  7821 | `			int bArgTyped;` |
|  121083 |  7822 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  121019 |  7823 | `				continue;` |
|       - |  7824 | `			}` |
|       - |  7825 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  7826 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  7827 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      49 |  7828 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      70 |  7829 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      69 |  7830 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7831 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7832 | `					"Cannot declare variadic promoted property");` |
|       3 |  7833 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7834 | `					return SXERR_ABORT;` |
|       - |  7835 | `				}` |
|       3 |  7836 | `				goto Synchronize;` |
|       - |  7837 | `			}` |
|       - |  7838 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7839 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7840 | `			 * appear as an alternative of a union type. */` |
|      67 |  7841 | `			if( bArgTyped ){` |
|      92 |  7842 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      58 |  7843 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      58 |  7844 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      29 |  7845 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      63 |  7846 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7847 | `					return SXERR_ABORT;` |
|      63 |  7848 | `				}else if( rc != SXRET_OK ){` |
|       6 |  7849 | `					goto Synchronize;` |
|       - |  7850 | `				}` |
|      27 |  7851 | `			}` |
|       - |  7852 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      63 |  7853 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7854 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7855 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7856 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7857 | `					return SXERR_ABORT;` |
|       - |  7858 | `				}` |
|       3 |  7859 | `				goto Synchronize;` |
|       - |  7860 | `			}` |
|      61 |  7861 | `			if( bArgTyped ){` |
|      57 |  7862 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      26 |  7863 | `			}` |
|      61 |  7864 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7865 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7866 | `			}` |
|      61 |  7867 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  7868 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  7869 | `			}` |
|      61 |  7870 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  7871 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  7872 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  7873 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  7874 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7875 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  7876 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7877 | `						return SXERR_ABORT;` |
|       - |  7878 | `					}` |
|       3 |  7879 | `					goto Synchronize;` |
|       - |  7880 | `				}` |
|      22 |  7881 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7882 | `			}` |
|      59 |  7883 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      59 |  7884 | `			if( pAttr == 0 ){` |
|     ! 0 |  7885 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7886 | `				return SXERR_ABORT;` |
|       - |  7887 | `			}` |
|      59 |  7888 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      57 |  7889 | `				pAttr->nType = pArg->nType;` |
|      57 |  7890 | `				pAttr->sClass = pArg->sClass;` |
|      57 |  7891 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      57 |  7892 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7893 | `					sxu32 k;` |
|      20 |  7894 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  7895 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  7896 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  7897 | `					}` |
|       3 |  7898 | `				}` |
|      26 |  7899 | `			}` |
|      59 |  7900 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      59 |  7901 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7902 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7903 | `				return SXERR_ABORT;` |
|       - |  7904 | `			}` |
|      32 |  7905 | `		}` |
|       - |  7906 | `	}` |
|  266921 |  7907 | `	if( doBody ){` |
|       - |  7908 | `		/* Compile method body */` |
|  174723 |  7909 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  174723 |  7910 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7911 | `			return SXERR_ABORT;` |
|       - |  7912 | `		}` |
|   87364 |  7913 | `	}else{` |
|       - |  7914 | `		/* Only method signature is allowed */` |
|   92203 |  7915 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7916 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7917 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7918 | `				if( rc == SXERR_ABORT ){` |
|       - |  7919 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7920 | `					return SXERR_ABORT;` |
|       - |  7921 | `				}` |
|     ! 0 |  7922 | `				return SXERR_CORRUPT;` |
|       - |  7923 | `			}` |
|       - |  7924 | `	}` |
|       - |  7925 | `	/* All done,install the method */` |
|  266921 |  7926 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  266921 |  7927 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7928 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7929 | `		return SXERR_ABORT;` |
|       - |  7930 | `	}` |
|  266921 |  7931 | `	return SXRET_OK;` |
|       6 |  7932 | `Synchronize:` |
|       - |  7933 | `	/* Synchronize with the first semi-colon */` |
|      40 |  7934 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  7935 | `		pGen->pIn++;` |
|       4 |  7936 | `	}` |
|      16 |  7937 | `	return SXERR_CORRUPT;` |
|  133469 |  7938 |  |
|       - |  7939 | `/*` |
|       - |  7940 | ` * Compile an object interface.` |
|       - |  7941 | ` *  According to the PHP language reference manual` |
|       - |  7942 | ` *   Object Interfaces:` |
|       - |  7943 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7944 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7945 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7946 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7947 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7948 | ` */` |
|   39062 |  7949 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  7950 |  |
|   39067 |  7951 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7952 | `	ph7_class *pClass,*pBase;` |
|       - |  7953 | `	SyToken *pEnd,*pTmp;` |
|       - |  7954 | `	SyString *pName;` |
|       - |  7955 | `	sxi32 nKwrd;` |
|       - |  7956 | `	sxi32 rc;` |
|       - |  7957 | `	/* Jump the 'interface' keyword */` |
|   39067 |  7958 | `	pGen->pIn++;` |
|       - |  7959 | `	/* Extract interface name */` |
|   39067 |  7960 | `	pName = &pGen->pIn->sData;` |
|       - |  7961 | `	/* Advance the stream cursor */` |
|   39067 |  7962 | `	pGen->pIn++;` |
|       - |  7963 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7964 | `		SyBlob sFQN;` |
|       - |  7965 | `		SyString sFQNStr;` |
|   39067 |  7966 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   39067 |  7967 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   39067 |  7968 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   39067 |  7969 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   39067 |  7970 | `		SyBlobRelease(&sFQN);` |
|       - |  7971 | `	}` |
|   39067 |  7972 | `	if( pClass == 0 ){` |
|     ! 0 |  7973 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7974 | `		return SXERR_ABORT;` |
|       - |  7975 | `	}` |
|       - |  7976 | `	/* Mark as an interface */` |
|   39067 |  7977 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7978 | `	/* Assume no base class is given */` |
|   39067 |  7979 | `	pBase = 0;` |
|   39067 |  7980 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   10645 |  7981 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10645 |  7982 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7983 | `			SyBlob sResolved;` |
|       - |  7984 | `			SyString sBaseName;` |
|       - |  7985 | `			sxu32 nRefLine;` |
|       - |  7986 | `			/* Extract base interface */` |
|   10645 |  7987 | `			pGen->pIn++;` |
|   10645 |  7988 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10645 |  7989 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10645 |  7990 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7991 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7992 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7993 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7994 | `					pName);` |
|     ! 0 |  7995 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7996 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7997 | `					return SXERR_ABORT;` |
|       - |  7998 | `				}` |
|     ! 0 |  7999 | `				return SXRET_OK;` |
|       - |  8000 | `			}` |
|   15965 |  8001 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   10640 |  8002 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10645 |  8003 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8004 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8005 | `			/* Only interfaces is allowed */` |
|   10645 |  8006 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8007 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8008 | `			}` |
|   10645 |  8009 | `			if( pBase == 0 ){` |
|     ! 0 |  8010 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8011 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8012 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8013 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8014 | `					return SXERR_ABORT;` |
|       - |  8015 | `				}` |
|     ! 0 |  8016 | `			}` |
|   10645 |  8017 | `			SyBlobRelease(&sResolved);` |
|    5320 |  8018 | `		}` |
|    5320 |  8019 | `	}` |
|   39067 |  8020 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8021 | `		/* Syntax error */` |
|     ! 0 |  8022 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8023 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8024 | `		if( rc == SXERR_ABORT ){` |
|       - |  8025 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8026 | `			return SXERR_ABORT;` |
|       - |  8027 | `		}` |
|     ! 0 |  8028 | `		return SXRET_OK;` |
|       - |  8029 | `	}` |
|   39067 |  8030 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   39067 |  8031 | `	pEnd = 0; /* cc warning */` |
|       - |  8032 | `	/* Delimit the interface body */` |
|   39067 |  8033 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   39067 |  8034 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8035 | `		/* Syntax error */` |
|     ! 0 |  8036 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8037 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8038 | `		if( rc == SXERR_ABORT ){` |
|       - |  8039 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8040 | `			return SXERR_ABORT;` |
|       - |  8041 | `		}` |
|     ! 0 |  8042 | `		return SXRET_OK;` |
|       - |  8043 | `	}` |
|       - |  8044 | `	/* Swap token stream */` |
|   39067 |  8045 | `	pTmp = pGen->pEnd;` |
|   39067 |  8046 | `	pGen->pEnd = pEnd;` |
|       - |  8047 | `	/* Start the parse process` |
|       - |  8048 | `	 * Note (According to the PHP reference manual):` |
|       - |  8049 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8050 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8051 | `	 */` |
|   65626 |  8052 | `	for(;;){` |
|       - |  8053 | `		/* Jump leading/trailing semi-colons */` |
|  223447 |  8054 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   92195 |  8055 | `			pGen->pIn++;` |
|       5 |  8056 | `		}` |
|  131257 |  8057 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8058 | `			/* End of interface body */` |
|   39065 |  8059 | `			break;` |
|       - |  8060 | `		}` |
|   92197 |  8061 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8062 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8063 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8064 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8065 | `			if( rc == SXERR_ABORT ){` |
|       - |  8066 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8067 | `				return SXERR_ABORT;` |
|       - |  8068 | `			}` |
|     ! 0 |  8069 | `			goto done;` |
|       - |  8070 | `		}` |
|       - |  8071 | `		/* Extract the current keyword */` |
|   92197 |  8072 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   92197 |  8073 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8074 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8075 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8076 | `			const char *zKind = "member";` |
|       3 |  8077 | `			SyString *pMemberName = 0;` |
|       3 |  8078 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8079 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8080 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8081 | `					zKind = "constant";` |
|       3 |  8082 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8083 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8084 | `					}` |
|       1 |  8085 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8086 | `					zKind = "method";` |
|     ! 0 |  8087 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8088 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8089 | `					}` |
|     ! 0 |  8090 | `				}` |
|       1 |  8091 | `			}` |
|       3 |  8092 | `			if( pMemberName ){` |
|       4 |  8093 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8094 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8095 | `			}else{` |
|     ! 0 |  8096 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8097 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8098 | `			}` |
|       3 |  8099 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8100 | `				return SXERR_ABORT;` |
|       - |  8101 | `			}` |
|       3 |  8102 | `			goto done;` |
|       - |  8103 | `		}` |
|   92195 |  8104 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8105 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8106 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8107 | `			if( rc == SXERR_ABORT ){` |
|       - |  8108 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8109 | `				return SXERR_ABORT;` |
|       - |  8110 | `			}` |
|     ! 0 |  8111 | `			goto done;` |
|       - |  8112 | `		}` |
|   92195 |  8113 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8114 | `			/* Advance the stream cursor */` |
|   92185 |  8115 | `			pGen->pIn++;` |
|   92185 |  8116 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8117 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8118 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8119 | `				if( rc == SXERR_ABORT ){` |
|       - |  8120 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8121 | `					return SXERR_ABORT;` |
|       - |  8122 | `				}` |
|     ! 0 |  8123 | `				goto done;` |
|       - |  8124 | `			}` |
|   92185 |  8125 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   92185 |  8126 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8127 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8128 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8129 | `				if( rc == SXERR_ABORT ){` |
|       - |  8130 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8131 | `					return SXERR_ABORT;` |
|       - |  8132 | `				}` |
|     ! 0 |  8133 | `				goto done;` |
|       - |  8134 | `			}` |
|   46090 |  8135 | `		}` |
|   92195 |  8136 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8137 | `			/* Parse constant */` |
|       7 |  8138 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       7 |  8139 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8140 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8141 | `					return SXERR_ABORT;` |
|       - |  8142 | `				}` |
|     ! 0 |  8143 | `				goto done;` |
|       - |  8144 | `			}` |
|       4 |  8145 | `		}else{` |
|   92189 |  8146 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   92189 |  8147 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8148 | `				/* Static method,record that */` |
|   10637 |  8149 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8150 | `				/* Advance the stream cursor */` |
|   10637 |  8151 | `				pGen->pIn++;` |
|   10632 |  8152 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   10637 |  8153 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8154 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8155 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8156 | `						if( rc == SXERR_ABORT ){` |
|       - |  8157 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8158 | `							return SXERR_ABORT;` |
|       - |  8159 | `						}` |
|     ! 0 |  8160 | `						goto done;` |
|       - |  8161 | `				}` |
|    5316 |  8162 | `			}` |
|       - |  8163 | `			/* Process method signature (no body for interface methods) */` |
|   92189 |  8164 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   92189 |  8165 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8166 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8167 | `					return SXERR_ABORT;` |
|       - |  8168 | `				}` |
|     ! 0 |  8169 | `				goto done;` |
|       - |  8170 | `			}` |
|       - |  8171 | `		}` |
|       5 |  8172 | `	}` |
|       - |  8173 | `	/* Install the interface */` |
|   39065 |  8174 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   39065 |  8175 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8176 | `		/* Inherit from the base interface */` |
|   10645 |  8177 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5320 |  8178 | `	}` |
|   39065 |  8179 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8180 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8181 | `		return SXERR_ABORT;` |
|       - |  8182 | `	}` |
|   19530 |  8183 | `done:` |
|       - |  8184 | `	/* Point beyond the interface body */` |
|   39067 |  8185 | `	pGen->pIn  = &pEnd[1];` |
|   39067 |  8186 | `	pGen->pEnd = pTmp;` |
|   39067 |  8187 | `	return PH7_OK;` |
|   19536 |  8188 |  |
|       - |  8189 | `/*` |
|       - |  8190 | ` * Compile a user-defined class.` |
|       - |  8191 | ` * According to the PHP language reference manual` |
|       - |  8192 | ` *  class` |
|       - |  8193 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8194 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8195 | ` *  of the properties and methods belonging to the class.` |
|       - |  8196 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8197 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8198 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8199 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8200 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8201 | ` *  (called "methods").` |
|       - |  8202 | ` */` |
|       - |  8203 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8204 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8205 | `struct TraitUseEntry {` |
|       - |  8206 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8207 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8208 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8209 | `};` |
|       - |  8210 | `/*` |
|       - |  8211 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8212 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8213 | ` */` |
|  100446 |  8214 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8215 |  |
|       - |  8216 | `	ph7_class **apIface;` |
|       - |  8217 | `	sxu32 nIface,i;` |
|       - |  8218 | `	sxi32 rc;` |
|  100451 |  8219 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8220 | `		return SXRET_OK;` |
|       - |  8221 | `	}` |
|  100451 |  8222 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  100451 |  8223 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  192843 |  8224 | `	for(i = 0; i < nIface; i++){` |
|   92397 |  8225 | `		ph7_class *pIface = apIface[i];` |
|       - |  8226 | `		SyHashEntry *pEntry;` |
|   92397 |  8227 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  248783 |  8228 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  156391 |  8229 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8230 | `			ph7_class_method *pImplMeth;` |
|  156391 |  8231 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8232 | `			/* Find the implementing method in the class */` |
|  156391 |  8233 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  156391 |  8234 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8235 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8236 | `			}` |
|       - |  8237 | `			/* Check visibility: interface methods must be implemented as public */` |
|  156377 |  8238 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8239 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8240 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8241 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8242 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8243 | `					return SXERR_ABORT;` |
|       - |  8244 | `				}` |
|       1 |  8245 | `			}` |
|       - |  8246 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8247 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8248 | `			 */` |
|       - |  8249 | `			{` |
|  156377 |  8250 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  156377 |  8251 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  156377 |  8252 | `				int sigError = 0;` |
|  156377 |  8253 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8254 | `					sigError = 1;` |
|  156376 |  8255 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8256 | `					/* Extra parameters must all have default values */` |
|       6 |  8257 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8258 | `					sxu32 k;` |
|       8 |  8259 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8260 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8261 | `							sigError = 1;` |
|       3 |  8262 | `							break;` |
|       - |  8263 | `						}` |
|       2 |  8264 | `					}` |
|       2 |  8265 | `				}` |
|  156377 |  8266 | `				if( sigError ){` |
|       - |  8267 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8268 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8269 | `					sxu32 j;` |
|       6 |  8270 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8271 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8272 | `					/* Build implementing method signature */` |
|       6 |  8273 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8274 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8275 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8276 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8277 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8278 | `					}` |
|       - |  8279 | `					/* Build interface method signature */` |
|       6 |  8280 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8281 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8282 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8283 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8284 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8285 | `					}` |
|       8 |  8286 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8287 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8288 | `						&pClass->sName,pMName,` |
|       4 |  8289 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8290 | `						&pIface->sName,pMName,` |
|       4 |  8291 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8292 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8293 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8294 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8295 | `						return SXERR_ABORT;` |
|       - |  8296 | `					}` |
|       2 |  8297 | `				}` |
|       - |  8298 | `			}` |
|       5 |  8299 | `		}` |
|   46201 |  8300 | `	}` |
|  100451 |  8301 | `	return SXRET_OK;` |
|   50228 |  8302 |  |
|       - |  8303 | `/*` |
|       - |  8304 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8305 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8306 | ` */` |
|  100446 |  8307 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8308 |  |
|       - |  8309 | `	ph7_class_method *pMeth;` |
|       - |  8310 | `	SyHashEntry *pEntry;` |
|       - |  8311 | `	sxu32 nAbstract;` |
|       - |  8312 | `	SyBlob sMsg;` |
|       - |  8313 | `	sxi32 rc;` |
|       - |  8314 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  100451 |  8315 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      33 |  8316 | `		return SXRET_OK;` |
|       - |  8317 | `	}` |
|       - |  8318 | `	/* Count abstract methods */` |
|  100423 |  8319 | `	nAbstract = 0;` |
|  100423 |  8320 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  942033 |  8321 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  841615 |  8322 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  841615 |  8323 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8324 | `			nAbstract++;` |
|       8 |  8325 | `		}` |
|       5 |  8326 | `	}` |
|  100423 |  8327 | `	if( nAbstract == 0 ){` |
|  100409 |  8328 | `		return SXRET_OK;` |
|       - |  8329 | `	}` |
|       - |  8330 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8331 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8332 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8333 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8334 | `		&pClass->sName,nAbstract,` |
|       7 |  8335 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8336 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8337 | `	/* Second pass: list methods with origins */` |
|       - |  8338 | `	{` |
|      18 |  8339 | `		sxu32 nListed = 0;` |
|      18 |  8340 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8341 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8342 | `			ph7_class *pOrigin = 0;` |
|       - |  8343 | `			SyString *pMName;` |
|      22 |  8344 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8345 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8346 | `				continue;` |
|       - |  8347 | `			}` |
|      20 |  8348 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8349 | `			if( nListed > 0 ){` |
|       3 |  8350 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8351 | `			}` |
|       - |  8352 | `			/* Find the origin of this abstract method.` |
|       - |  8353 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8354 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8355 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8356 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8357 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8358 | `			 * class's namespace.` |
|       - |  8359 | `			 */` |
|       - |  8360 | `			{` |
|       - |  8361 | `				ph7_class **apIface;` |
|       - |  8362 | `				ph7_class **apTrait;` |
|       - |  8363 | `				ph7_class *pWalk;` |
|       - |  8364 | `				sxu32 i;` |
|       - |  8365 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8366 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8367 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8368 | `				 */` |
|      20 |  8369 | `				if( pClass->pBase ){` |
|      11 |  8370 | `					pWalk = pClass->pBase;` |
|      19 |  8371 | `					while( pWalk ){` |
|       - |  8372 | `						ph7_class_method *pParentMeth;` |
|      13 |  8373 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8374 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8375 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8376 | `							 * in this class's ancestor chain.` |
|       - |  8377 | `							 */` |
|      13 |  8378 | `							int fromIface = 0;` |
|      13 |  8379 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8380 | `							while( pAnc ){` |
|       - |  8381 | `								ph7_class **apPI;` |
|       - |  8382 | `								sxu32 j;` |
|      15 |  8383 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8384 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8385 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8386 | `										fromIface = 1;` |
|      10 |  8387 | `										break;` |
|       - |  8388 | `									}` |
|     ! 0 |  8389 | `								}` |
|      15 |  8390 | `								if( fromIface ) break;` |
|       6 |  8391 | `								pAnc = pAnc->pBase;` |
|       2 |  8392 | `							}` |
|      13 |  8393 | `							if( !fromIface ){` |
|       3 |  8394 | `								pOrigin = pWalk;` |
|       3 |  8395 | `								break;` |
|       - |  8396 | `							}` |
|       4 |  8397 | `						}` |
|      10 |  8398 | `						pWalk = pWalk->pBase;` |
|       2 |  8399 | `					}` |
|       4 |  8400 | `				}` |
|       - |  8401 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8402 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8403 | `				 */` |
|      20 |  8404 | `				if( !pOrigin ){` |
|      18 |  8405 | `					pWalk = pClass;` |
|      40 |  8406 | `					while( pWalk && !pOrigin ){` |
|      26 |  8407 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8408 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8409 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8410 | `							ph7_class *pDeepest = 0;` |
|      28 |  8411 | `							while( pIface ){` |
|      16 |  8412 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8413 | `									pDeepest = pIface;` |
|       6 |  8414 | `								}` |
|      16 |  8415 | `								pIface = pIface->pBase;` |
|       4 |  8416 | `							}` |
|      16 |  8417 | `							if( pDeepest ){` |
|      16 |  8418 | `								pOrigin = pDeepest;` |
|      16 |  8419 | `								break;` |
|       - |  8420 | `							}` |
|     ! 0 |  8421 | `						}` |
|      26 |  8422 | `						pWalk = pWalk->pBase;` |
|       4 |  8423 | `					}` |
|       7 |  8424 | `				}` |
|       - |  8425 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8426 | `				if( !pOrigin ){` |
|       3 |  8427 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8428 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8429 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8430 | `							pOrigin = pClass;` |
|       3 |  8431 | `							break;` |
|       - |  8432 | `						}` |
|     ! 0 |  8433 | `					}` |
|       1 |  8434 | `				}` |
|       - |  8435 | `			}` |
|      20 |  8436 | `			if( pOrigin ){` |
|      20 |  8437 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8438 | `			}else{` |
|       - |  8439 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8440 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8441 | `			}` |
|      20 |  8442 | `			nListed++;` |
|       4 |  8443 | `		}` |
|       - |  8444 | `	}` |
|      18 |  8445 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8446 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8447 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8448 | `	SyBlobRelease(&sMsg);` |
|      18 |  8449 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8450 | `		return SXERR_ABORT;` |
|       - |  8451 | `	}` |
|      18 |  8452 | `	return SXRET_OK;` |
|   50228 |  8453 |  |
|       - |  8454 | `/*` |
|       - |  8455 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8456 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8457 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8458 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8459 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8460 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8461 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8462 | ` */` |
|   96642 |  8463 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8464 |  |
|   96647 |  8465 | `	int isAbsolute = 0;` |
|   96647 |  8466 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8467 | `	SyBlob sName;` |
|   96647 |  8468 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      95 |  8469 | `		isAbsolute = 1;` |
|      95 |  8470 | `		pGen->pIn++;` |
|      45 |  8471 | `	}` |
|   96647 |  8472 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       8 |  8473 | `		pGen->pIn = pStart;` |
|       8 |  8474 | `		return SXERR_INVALID;` |
|       - |  8475 | `	}` |
|   96641 |  8476 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   96641 |  8477 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   96641 |  8478 | `	pGen->pIn++;` |
|  144972 |  8479 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   48341 |  8480 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8481 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8482 | `		pGen->pIn++;` |
|      13 |  8483 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8484 | `		pGen->pIn++;` |
|       1 |  8485 | `	}` |
|   96641 |  8486 | `	if( isAbsolute ){` |
|      93 |  8487 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      49 |  8488 | `	}else{` |
|       - |  8489 | `		SyString sRaw;` |
|   96553 |  8490 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   96553 |  8491 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8492 | `	}` |
|   96641 |  8493 | `	SyBlobRelease(&sName);` |
|   96641 |  8494 | `	return SXRET_OK;` |
|   48326 |  8495 |  |
|       - |  8496 | `/*` |
|       - |  8497 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8498 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8499 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8500 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8501 | ` * either direction cannot run unbounded.` |
|       - |  8502 | ` */` |
|       - |  8503 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   10802 |  8504 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8505 |  |
|       - |  8506 | `	ph7_class **apParent;` |
|       - |  8507 | `	sxu32 n;` |
|   18097 |  8508 | `	while( pInterface ){` |
|   14393 |  8509 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8510 | `			return FALSE;` |
|       - |  8511 | `		}` |
|   17951 |  8512 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7116 |  8513 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7103 |  8514 | `			return TRUE;` |
|       - |  8515 | `		}` |
|    7295 |  8516 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7295 |  8517 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8518 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8519 | `				return TRUE;` |
|       - |  8520 | `			}` |
|     ! 0 |  8521 | `		}` |
|    7295 |  8522 | `		pInterface = pInterface->pBase;` |
|    7295 |  8523 | `		iDepth++;` |
|       5 |  8524 | `	}` |
|    3709 |  8525 | `	return FALSE;` |
|    5406 |  8526 |  |
|   10802 |  8527 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8528 |  |
|   10807 |  8529 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8530 |  |
|       - |  8531 | `/*` |
|       - |  8532 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8533 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8534 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8535 | ` */` |
|    7098 |  8536 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8537 |  |
|    7107 |  8538 | `	while( pBase ){` |
|      10 |  8539 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8540 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8541 | `			return TRUE;` |
|       - |  8542 | `		}` |
|      10 |  8543 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8544 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8545 | `			return TRUE;` |
|       - |  8546 | `		}` |
|       5 |  8547 | `		pBase = pBase->pBase;` |
|       1 |  8548 | `	}` |
|    7099 |  8549 | `	return FALSE;` |
|    3554 |  8550 |  |
|       - |  8551 | `/*` |
|       - |  8552 | ` * Compile a class declaration, named or anonymous.` |
|       - |  8553 | ` *` |
|       - |  8554 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  8555 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  8556 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  8557 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  8558 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  8559 | ` * implements, body, install) is shared by both paths.` |
|       - |  8560 | ` */` |
|  100476 |  8561 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  8562 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  8563 |  |
|  100481 |  8564 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8565 | `	ph7_class *pClass,*pBase;` |
|       - |  8566 | `	SyToken *pEnd,*pTmp;` |
|       - |  8567 | `	sxi32 iProtection;` |
|       - |  8568 | `	SySet aInterfaces;` |
|       - |  8569 | `	SySet aUseEntries;` |
|       - |  8570 | `	sxi32 iAttrflags;` |
|       - |  8571 | `	SyString *pName;` |
|       - |  8572 | `	sxi32 nKwrd;` |
|       - |  8573 | `	sxi32 rc;` |
|       - |  8574 | `	/* Jump the 'class' keyword */` |
|  100481 |  8575 | `	pGen->pIn++;` |
|  100481 |  8576 | `	if( pAnonName ){` |
|       - |  8577 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  8578 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  8579 | `		 * then use the synthesized name. */` |
|      29 |  8580 | `		*ppArgStart = *ppArgEnd = 0;` |
|      29 |  8581 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  8582 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  8583 | `			*ppArgStart = pGen->pIn;` |
|      10 |  8584 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  8585 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  8586 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  8587 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  8588 | `		}` |
|      29 |  8589 | `		pName = pAnonName;` |
|      29 |  8590 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      16 |  8591 | `	}else{` |
|  100455 |  8592 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8593 | `			/* Syntax error */` |
|     ! 0 |  8594 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8595 | `			if( rc == SXERR_ABORT ){` |
|       - |  8596 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8597 | `				return SXERR_ABORT;` |
|       - |  8598 | `			}` |
|       - |  8599 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8600 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8601 | `				pGen->pIn++;` |
|     ! 0 |  8602 | `			}` |
|     ! 0 |  8603 | `			return SXRET_OK;` |
|       - |  8604 | `		}` |
|       - |  8605 | `		/* Extract class name */` |
|  100455 |  8606 | `		pName = &pGen->pIn->sData;` |
|       - |  8607 | `		/* Advance the stream cursor */` |
|  100455 |  8608 | `		pGen->pIn++;` |
|       - |  8609 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  8610 | `			SyBlob sFQN;` |
|       - |  8611 | `			SyString sFQNStr;` |
|  100455 |  8612 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  100455 |  8613 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  100455 |  8614 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  100455 |  8615 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  100455 |  8616 | `			SyBlobRelease(&sFQN);` |
|       - |  8617 | `		}` |
|       - |  8618 | `	}` |
|  100481 |  8619 | `	if( pClass == 0 ){` |
|     ! 0 |  8620 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8621 | `		return SXERR_ABORT;` |
|       - |  8622 | `	}` |
|       - |  8623 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  100481 |  8624 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  100481 |  8625 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8626 | `	/* Assume a standalone class */` |
|  100481 |  8627 | `	pBase = 0;` |
|  100481 |  8628 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   85389 |  8629 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   85389 |  8630 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8631 | `			SyBlob sResolved;` |
|       - |  8632 | `			SyString sBaseName;` |
|       - |  8633 | `			sxu32 nRefLine;` |
|   74605 |  8634 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   74605 |  8635 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   74605 |  8636 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   74605 |  8637 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8638 | `				SyBlobRelease(&sResolved);` |
|       4 |  8639 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8640 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8641 | `					pName);` |
|       3 |  8642 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8643 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8644 | `					return SXERR_ABORT;` |
|       - |  8645 | `				}` |
|       3 |  8646 | `				return SXRET_OK;` |
|       - |  8647 | `			}` |
|  111902 |  8648 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   74598 |  8649 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   74603 |  8650 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8651 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8652 | `			/* Interfaces are not allowed */` |
|   74603 |  8653 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8654 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8655 | `			}` |
|   74603 |  8656 | `			if( pBase == 0 ){` |
|     ! 0 |  8657 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8658 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8659 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8660 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8661 | `					return SXERR_ABORT;` |
|       - |  8662 | `				}` |
|     ! 0 |  8663 | `			}else{` |
|   74603 |  8664 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8665 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8666 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8667 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8668 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8669 | `						return SXERR_ABORT;` |
|       - |  8670 | `					}` |
|     ! 0 |  8671 | `				}` |
|       - |  8672 | `			}` |
|   74603 |  8673 | `			SyBlobRelease(&sResolved);` |
|   37299 |  8674 | `		}` |
|   85387 |  8675 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8676 | `			ph7_class *pInterface;` |
|       - |  8677 | `			/* Interface implementation */` |
|   10797 |  8678 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5406 |  8679 | `			for(;;){` |
|       - |  8680 | `				SyBlob sResolved;` |
|       - |  8681 | `				SyString sIntName;` |
|       - |  8682 | `				sxu32 nRefLine;` |
|   10807 |  8683 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10807 |  8684 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10807 |  8685 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8686 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8687 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8688 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8689 | `						pName);` |
|     ! 0 |  8690 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8691 | `						return SXERR_ABORT;` |
|       - |  8692 | `					}` |
|     ! 0 |  8693 | `					break;` |
|       - |  8694 | `				}` |
|   21609 |  8695 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   10802 |  8696 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10807 |  8697 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8698 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8699 | `				/* Only interfaces are allowed */` |
|   10807 |  8700 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8701 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8702 | `				}` |
|   10807 |  8703 | `				if( pInterface == 0 ){` |
|     ! 0 |  8704 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8705 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8706 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8707 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8708 | `						return SXERR_ABORT;` |
|       - |  8709 | `					}` |
|     ! 0 |  8710 | `				}else{` |
|       - |  8711 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8712 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8713 | `					 * unless they already extend Exception or Error.` |
|       - |  8714 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8715 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8716 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   10807 |  8717 | `					SyString *pFqn = &pClass->sName;` |
|   10807 |  8718 | `					int bIsExceptionOrError =` |
|    8949 |  8719 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   17979 |  8720 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9037 |  8721 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3558 |  8722 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   14351 |  8723 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   10650 |  8724 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3547 |  8725 | `						!bIsExceptionOrError ){` |
|      12 |  8726 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8727 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8728 | `							&pClass->sName);` |
|       9 |  8729 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8730 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8731 | `							return SXERR_ABORT;` |
|       - |  8732 | `						}` |
|       - |  8733 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8734 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8735 | `					}else{` |
|   10801 |  8736 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8737 | `					}` |
|       - |  8738 | `				}` |
|   10807 |  8739 | `				SyBlobRelease(&sResolved);` |
|   10807 |  8740 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5401 |  8741 | `					break;` |
|       - |  8742 | `				}` |
|      13 |  8743 | `				pGen->pIn++;/* Jump the comma */` |
|       3 |  8744 | `			}` |
|    5396 |  8745 | `		}` |
|   42691 |  8746 | `	}` |
|  100479 |  8747 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8748 | `		/* Syntax error */` |
|     ! 0 |  8749 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8750 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8751 | `		if( rc == SXERR_ABORT ){` |
|       - |  8752 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8753 | `			return SXERR_ABORT;` |
|       - |  8754 | `		}` |
|     ! 0 |  8755 | `		return SXRET_OK;` |
|       - |  8756 | `	}` |
|  100479 |  8757 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  100479 |  8758 | `	pEnd = 0; /* cc warning */` |
|       - |  8759 | `	/* Delimit the class body */` |
|  100479 |  8760 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  100479 |  8761 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8762 | `		/* Syntax error */` |
|     ! 0 |  8763 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8764 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8765 | `		if( rc == SXERR_ABORT ){` |
|       - |  8766 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8767 | `			return SXERR_ABORT;` |
|       - |  8768 | `		}` |
|     ! 0 |  8769 | `		return SXRET_OK;` |
|       - |  8770 | `	}` |
|       - |  8771 | `	/* Swap token stream */` |
|  100479 |  8772 | `	pTmp = pGen->pEnd;` |
|  100479 |  8773 | `	pGen->pEnd = pEnd;` |
|       - |  8774 | `	/* Set the inherited flags */` |
|  100479 |  8775 | `	pClass->iFlags = iFlags;` |
|       - |  8776 | `	/* Start the parse process */` |
|  137606 |  8777 | `	for(;;){` |
|       - |  8778 | `		/* Jump leading/trailing semi-colons */` |
|  425543 |  8779 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   75201 |  8780 | `			pGen->pIn++;` |
|       5 |  8781 | `		}` |
|  350347 |  8782 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8783 | `			/* End of class body */` |
|  100451 |  8784 | `			break;` |
|       - |  8785 | `		}` |
|  249896 |  8786 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  124953 |  8787 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  8788 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8789 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8790 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8791 | `			if( rc == SXERR_ABORT ){` |
|       - |  8792 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8793 | `				return SXERR_ABORT;` |
|       - |  8794 | `			}` |
|     ! 0 |  8795 | `			goto done;` |
|       - |  8796 | `		}` |
|       - |  8797 | `		/* Assume public visibility */` |
|  249901 |  8798 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  249901 |  8799 | `		iAttrflags = 0;` |
|       - |  8800 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  8801 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  8802 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  8803 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  249901 |  8804 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8805 | `			int bMod = 0;` |
|     ! 0 |  8806 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8807 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  8808 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  8809 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  8810 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  8811 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  8812 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  8813 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  8814 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  8815 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  8816 | `			}` |
|     ! 0 |  8817 | `			if( !bMod ){` |
|     ! 0 |  8818 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8819 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8820 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8821 | `						return SXERR_ABORT;` |
|       - |  8822 | `					}` |
|     ! 0 |  8823 | `					goto done;` |
|       - |  8824 | `				}` |
|     ! 0 |  8825 | `				continue;` |
|       - |  8826 | `			}` |
|     ! 0 |  8827 | `		}` |
|  249901 |  8828 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8829 | `			/* Extract the current keyword */` |
|  249901 |  8830 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  249901 |  8831 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8832 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8833 | `				TraitUseEntry sUse;` |
|      53 |  8834 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      53 |  8835 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      53 |  8836 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      32 |  8837 | `				for(;;){` |
|       - |  8838 | `					ph7_class *pTrait;` |
|       - |  8839 | `					SyString *pTraitName;` |
|      61 |  8840 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8841 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8842 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8843 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8844 | `							return SXERR_ABORT;` |
|       - |  8845 | `						}` |
|     ! 0 |  8846 | `						break;` |
|       - |  8847 | `					}` |
|      61 |  8848 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8849 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8850 | `						SyBlob sResolved;` |
|      61 |  8851 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      61 |  8852 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     117 |  8853 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      56 |  8854 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      61 |  8855 | `						SyBlobRelease(&sResolved);` |
|       - |  8856 | `					}` |
|       - |  8857 | `					/* Only traits are allowed */` |
|      61 |  8858 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8859 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8860 | `					}` |
|      61 |  8861 | `					if( pTrait == 0 ){` |
|     ! 0 |  8862 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8863 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8864 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8865 | `							return SXERR_ABORT;` |
|       - |  8866 | `						}` |
|     ! 0 |  8867 | `					}else{` |
|      61 |  8868 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8869 | `					}` |
|      61 |  8870 | `					pGen->pIn++; /* Advance past trait name */` |
|      61 |  8871 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      29 |  8872 | `						break;` |
|       - |  8873 | `					}` |
|      10 |  8874 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  8875 | `				}` |
|       - |  8876 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      53 |  8877 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8878 | `					SyToken *pBlock;` |
|      13 |  8879 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  8880 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  8881 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  8882 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  8883 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  8884 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  8885 | `					}else{` |
|     ! 0 |  8886 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8887 | `					}` |
|       5 |  8888 | `				}` |
|      53 |  8889 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8890 | `				/* The semicolon will be consumed by the outer loop */` |
|      53 |  8891 | `				continue;` |
|       - |  8892 | `			}` |
|  249853 |  8893 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  249565 |  8894 | `				iProtection = nKwrd;` |
|  249565 |  8895 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  8896 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  249565 |  8897 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  8898 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  8899 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  8900 | `				}` |
|  249560 |  8901 | `				if( pGen->pIn >= pGen->pEnd` |
|  249565 |  8902 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  8903 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8904 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8905 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8906 | `					if( rc == SXERR_ABORT ){` |
|       - |  8907 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8908 | `						return SXERR_ABORT;` |
|       - |  8909 | `					}` |
|     ! 0 |  8910 | `					goto done;` |
|       - |  8911 | `				}` |
|  249565 |  8912 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8913 | `					/* Attribute declaration (untyped) */` |
|   74905 |  8914 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   74905 |  8915 | `					if( rc != SXRET_OK ){` |
|       9 |  8916 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8917 | `							return SXERR_ABORT;` |
|       - |  8918 | `						}` |
|       9 |  8919 | `						goto done;` |
|       - |  8920 | `					}` |
|   74899 |  8921 | `					continue;` |
|       - |  8922 | `				}` |
|  174665 |  8923 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8924 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     173 |  8925 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     173 |  8926 | `					if( rc != SXRET_OK ){` |
|       8 |  8927 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8928 | `							return SXERR_ABORT;` |
|       - |  8929 | `						}` |
|       8 |  8930 | `						goto done;` |
|       - |  8931 | `					}` |
|     167 |  8932 | `					continue;` |
|       - |  8933 | `				}` |
|       - |  8934 | `				/* Extract the keyword */` |
|  174497 |  8935 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   87246 |  8936 | `			}` |
|  174785 |  8937 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8938 | `				/* Process constant declaration */` |
|      67 |  8939 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      67 |  8940 | `				if( rc != SXRET_OK ){` |
|       3 |  8941 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8942 | `						return SXERR_ABORT;` |
|       - |  8943 | `					}` |
|       3 |  8944 | `					goto done;` |
|       - |  8945 | `				}` |
|      35 |  8946 | `			}else{` |
|  174723 |  8947 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8948 | `					/* Static method or attribute,record that */` |
|   10697 |  8949 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   10697 |  8950 | `					pGen->pIn++; /* Jump the static keyword */` |
|   10697 |  8951 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8952 | `						/* Extract the keyword */` |
|   10689 |  8953 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10689 |  8954 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8955 | `							iProtection = nKwrd;` |
|     ! 0 |  8956 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8957 | `						}` |
|    5342 |  8958 | `					}` |
|       - |  8959 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  8960 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  8961 | `					 * than a generic "expecting method" parse error. */` |
|   10697 |  8962 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8963 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8964 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  8965 | `					}` |
|   10692 |  8966 | `					if( pGen->pIn >= pGen->pEnd` |
|   10697 |  8967 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  8968 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8969 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8970 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8971 | `						if( rc == SXERR_ABORT ){` |
|       - |  8972 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8973 | `							return SXERR_ABORT;` |
|       - |  8974 | `						}` |
|     ! 0 |  8975 | `						goto done;` |
|       - |  8976 | `					}` |
|   10697 |  8977 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8978 | `						/* Attribute declaration */` |
|       8 |  8979 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       8 |  8980 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8981 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8982 | `								return SXERR_ABORT;` |
|       - |  8983 | `							}` |
|     ! 0 |  8984 | `							goto done;` |
|       - |  8985 | `						}` |
|       8 |  8986 | `						continue;` |
|       - |  8987 | `					}` |
|   10691 |  8988 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8989 | `						/* Typed static attribute declaration */` |
|      15 |  8990 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  8991 | `						if( rc != SXRET_OK ){` |
|       3 |  8992 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8993 | `								return SXERR_ABORT;` |
|       - |  8994 | `							}` |
|       3 |  8995 | `							goto done;` |
|       - |  8996 | `						}` |
|      13 |  8997 | `						continue;` |
|       - |  8998 | `					}` |
|       - |  8999 | `					/* Extract the keyword */` |
|   10679 |  9000 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  169368 |  9001 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  9002 | `					/* Abstract method,record that */` |
|      12 |  9003 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9004 | `					/* Mark the whole class as abstract */` |
|      12 |  9005 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9006 | `					/* Advance the stream cursor */` |
|      12 |  9007 | `					pGen->pIn++;` |
|      12 |  9008 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  9009 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  9010 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  9011 | `							iProtection = nKwrd;` |
|      10 |  9012 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  9013 | `						}` |
|       5 |  9014 | `					}` |
|      12 |  9015 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  9016 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9017 | `							/* Static method */` |
|     ! 0 |  9018 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9019 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9020 | `					}` |
|      12 |  9021 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  9022 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9023 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9024 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9025 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9026 | `							if( rc == SXERR_ABORT ){` |
|       - |  9027 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9028 | `								return SXERR_ABORT;` |
|       - |  9029 | `							}` |
|     ! 0 |  9030 | `							goto done;` |
|       - |  9031 | `					}` |
|      12 |  9032 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  164026 |  9033 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9034 | `					/* final method ,record that */` |
|      17 |  9035 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  9036 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  9037 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9038 | `						/* Extract the keyword */` |
|      17 |  9039 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  9040 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  9041 | `							iProtection = nKwrd;` |
|       9 |  9042 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9043 | `						}` |
|       7 |  9044 | `					}` |
|      17 |  9045 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9046 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9047 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9048 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9049 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9050 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9051 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9052 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9053 | `									return SXERR_ABORT;` |
|       - |  9054 | `								}` |
|     ! 0 |  9055 | `								goto done;` |
|       - |  9056 | `							}` |
|      12 |  9057 | `							continue;` |
|       - |  9058 | `					}` |
|       6 |  9059 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9060 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9061 | `							/* Static method */` |
|     ! 0 |  9062 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9063 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9064 | `					}` |
|       6 |  9065 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9066 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9067 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9068 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9069 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9070 | `							if( rc == SXERR_ABORT ){` |
|       - |  9071 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9072 | `								return SXERR_ABORT;` |
|       - |  9073 | `							}` |
|     ! 0 |  9074 | `							goto done;` |
|       - |  9075 | `					}` |
|       6 |  9076 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9077 | `				}` |
|  174695 |  9078 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9079 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9080 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9081 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9082 | `						if( rc == SXERR_ABORT ){` |
|       - |  9083 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9084 | `							return SXERR_ABORT;` |
|       - |  9085 | `						}` |
|     ! 0 |  9086 | `						goto done;` |
|       - |  9087 | `				}` |
|  174695 |  9088 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9089 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9090 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9091 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9092 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9093 | `						if( rc == SXERR_ABORT ){` |
|       - |  9094 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9095 | `							return SXERR_ABORT;` |
|       - |  9096 | `						}` |
|     ! 0 |  9097 | `						goto done;` |
|       - |  9098 | `					}` |
|       - |  9099 | `					/* Attribute declaration */` |
|       7 |  9100 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9101 | `				}else{` |
|       - |  9102 | `					/* Process method declaration */` |
|  174689 |  9103 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9104 | `				}` |
|  174695 |  9105 | `				if( rc != SXRET_OK ){` |
|      16 |  9106 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9107 | `						return SXERR_ABORT;` |
|       - |  9108 | `					}` |
|      16 |  9109 | `					goto done;` |
|       - |  9110 | `				}` |
|       - |  9111 | `			}` |
|   87374 |  9112 | `		}else{` |
|       - |  9113 | `			/* Attribute declaration */` |
|     ! 0 |  9114 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9115 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9116 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9117 | `					return SXERR_ABORT;` |
|       - |  9118 | `				}` |
|     ! 0 |  9119 | `				goto done;` |
|       - |  9120 | `			}` |
|       - |  9121 | `		}` |
|       5 |  9122 | `	}` |
|       - |  9123 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9124 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9125 | `	 */` |
|       - |  9126 | `	{` |
|       - |  9127 | `		TraitUseEntry *apUse;` |
|       - |  9128 | `		sxu32 nU;` |
|  100451 |  9129 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  100499 |  9130 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      53 |  9131 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      53 |  9132 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      53 |  9133 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      53 |  9134 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9135 | `			sxu32 nT;` |
|      53 |  9136 | `			if( !hasResolution ){` |
|       - |  9137 | `				/* No conflict resolution block: use standard trait application */` |
|      87 |  9138 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      49 |  9139 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      49 |  9140 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9141 | `						break;` |
|       - |  9142 | `					}` |
|      27 |  9143 | `				}` |
|      24 |  9144 | `			}else{` |
|       - |  9145 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9146 | `				 * then use the block to resolve method conflicts.` |
|       - |  9147 | `				 */` |
|       - |  9148 | `				SyToken *pR;` |
|      25 |  9149 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9150 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9151 | `					ph7_class_attr *pAR;` |
|       - |  9152 | `					SyHashEntry *pER;` |
|       - |  9153 | `					SyString *pNR;` |
|      15 |  9154 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9155 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9156 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9157 | `						pNR = &pAR->sName;` |
|     ! 0 |  9158 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9159 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9160 | `						}` |
|     ! 0 |  9161 | `					}` |
|      15 |  9162 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9163 | `				}` |
|       - |  9164 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9165 | `				pR = pUse->pResolvStart;` |
|      27 |  9166 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9167 | `					SyString sTrait,sMethod;` |
|       - |  9168 | `					ph7_class *pSrcTrait;` |
|       - |  9169 | `					ph7_class_method *pMeth;` |
|       - |  9170 | `					sxi32 nRKwrd;` |
|      41 |  9171 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9172 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9173 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9174 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9175 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9176 | `					sMethod = pR->sData;` |
|      17 |  9177 | `					pR++;` |
|      17 |  9178 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9179 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9180 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9181 | `							sTrait = sMethod;` |
|       7 |  9182 | `							pR++;` |
|       7 |  9183 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9184 | `							sMethod = pR->sData;` |
|       7 |  9185 | `							pR++;` |
|       3 |  9186 | `						}` |
|       3 |  9187 | `					}` |
|      17 |  9188 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9189 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9190 | `						continue;` |
|       - |  9191 | `					}` |
|      17 |  9192 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9193 | `					pR++;` |
|      17 |  9194 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9195 | `						pSrcTrait = 0;` |
|       7 |  9196 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9197 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9198 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9199 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9200 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9201 | `								break;` |
|       - |  9202 | `							}` |
|       2 |  9203 | `						}` |
|       5 |  9204 | `						if( pSrcTrait ){` |
|       5 |  9205 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9206 | `							if( pMeth ){` |
|       5 |  9207 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9208 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9209 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9210 | `								}` |
|       2 |  9211 | `							}` |
|       2 |  9212 | `						}` |
|       2 |  9213 | `					}` |
|      35 |  9214 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9215 | `				}` |
|       - |  9216 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9217 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9218 | `					ph7_class_method *pMR;` |
|       - |  9219 | `					SyHashEntry *pER;` |
|       - |  9220 | `					SyString *pNR;` |
|      15 |  9221 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9222 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9223 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9224 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9225 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9226 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9227 | `						}` |
|       3 |  9228 | `					}` |
|       9 |  9229 | `				}` |
|       - |  9230 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9231 | `				pR = pUse->pResolvStart;` |
|      27 |  9232 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9233 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9234 | `					ph7_class *pSrcTrait;` |
|       - |  9235 | `					ph7_class_method *pMeth;` |
|      27 |  9236 | `					int hasQual = 0;` |
|       - |  9237 | `					sxi32 nRKwrd;` |
|      41 |  9238 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9239 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9240 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9241 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9242 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9243 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9244 | `					sMethod = pR->sData;` |
|      17 |  9245 | `					pR++;` |
|      17 |  9246 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9247 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9248 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9249 | `							sTrait = sMethod;` |
|       7 |  9250 | `							hasQual = 1;` |
|       7 |  9251 | `							pR++;` |
|       7 |  9252 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9253 | `							sMethod = pR->sData;` |
|       7 |  9254 | `							pR++;` |
|       3 |  9255 | `						}` |
|       3 |  9256 | `					}` |
|      17 |  9257 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9258 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9259 | `						continue;` |
|       - |  9260 | `					}` |
|      17 |  9261 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9262 | `					pR++;` |
|      17 |  9263 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9264 | `						sxi32 iNewVis = -1;` |
|      13 |  9265 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9266 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9267 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9268 | `								iNewVis = nAK;` |
|       7 |  9269 | `								pR++;` |
|       3 |  9270 | `							}` |
|       3 |  9271 | `						}` |
|      13 |  9272 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9273 | `							sAlias = pR->sData;` |
|      11 |  9274 | `							pR++;` |
|       4 |  9275 | `						}` |
|      13 |  9276 | `						pMeth = 0;` |
|      13 |  9277 | `						if( hasQual ){` |
|       3 |  9278 | `							pSrcTrait = 0;` |
|       5 |  9279 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9280 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9281 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9282 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9283 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9284 | `									break;` |
|       - |  9285 | `								}` |
|       2 |  9286 | `							}` |
|       3 |  9287 | `							if( pSrcTrait ){` |
|       3 |  9288 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9289 | `							}` |
|       2 |  9290 | `						}else{` |
|      10 |  9291 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9292 | `						}` |
|      13 |  9293 | `						if( pMeth ){` |
|      13 |  9294 | `							if( sAlias.nByte > 0 ){` |
|       - |  9295 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9296 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9297 | `								 */` |
|       - |  9298 | `								ph7_class_method *pAlias;` |
|       - |  9299 | `								char *zAliasDup;` |
|      11 |  9300 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9301 | `								if( pAlias ){` |
|      11 |  9302 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9303 | `									if( iNewVis >= 0 ){` |
|       5 |  9304 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9305 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9306 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9307 | `									}` |
|      11 |  9308 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9309 | `									if( zAliasDup ){` |
|      11 |  9310 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9311 | `									}` |
|       7 |  9312 | `								}` |
|       7 |  9313 | `							}else if( iNewVis >= 0 ){` |
|       - |  9314 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9315 | `								ph7_class_method *pCopy;` |
|       3 |  9316 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9317 | `								if( pCopy ){` |
|       3 |  9318 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9319 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9320 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9321 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9322 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9323 | `									/* Replace the method in the class hash */` |
|       3 |  9324 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9325 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9326 | `								}` |
|       1 |  9327 | `							}` |
|       5 |  9328 | `						}` |
|       5 |  9329 | `						SXUNUSED(hasQual);` |
|       5 |  9330 | `					}` |
|      21 |  9331 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9332 | `				}` |
|       - |  9333 | `			}` |
|      53 |  9334 | `			SySetRelease(&pUse->aTraits);` |
|      29 |  9335 | `		}` |
|       - |  9336 | `	}` |
|       - |  9337 | `	/* Install the class */` |
|  100451 |  9338 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  100451 |  9339 | `	if( rc == SXRET_OK ){` |
|       - |  9340 | `		ph7_class **apInterface;` |
|       - |  9341 | `		sxu32 n;` |
|  100451 |  9342 | `		if( pBase ){` |
|       - |  9343 | `			/* Inherit from base class and mark as a subclass */` |
|   74603 |  9344 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   37299 |  9345 | `		}` |
|  100451 |  9346 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  111247 |  9347 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9348 | `			/* Implements one or more interface */` |
|   10801 |  9349 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   10801 |  9350 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9351 | `				break;` |
|       - |  9352 | `			}` |
|    5403 |  9353 | `		}` |
|       - |  9354 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9355 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  100446 |  9356 | `		if( rc == SXRET_OK` |
|  100446 |  9357 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  100451 |  9358 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   81603 |  9359 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9360 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   81603 |  9361 | `			if( pStringable ){` |
|   81603 |  9362 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   81603 |  9363 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9364 | `				sxu32 i;` |
|   81603 |  9365 | `				int bAlready = 0;` |
|   88695 |  9366 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7099 |  9367 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9368 | `						bAlready = 1;` |
|       3 |  9369 | `						break;` |
|       - |  9370 | `					}` |
|    3551 |  9371 | `				}` |
|   81603 |  9372 | `				if( !bAlready ){` |
|   81601 |  9373 | `					PH7_ClassImplement(pClass,pStringable);` |
|   40798 |  9374 | `				}` |
|   40799 |  9375 | `			}` |
|   40799 |  9376 | `		}` |
|       - |  9377 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  100451 |  9378 | `		if( rc == SXRET_OK ){` |
|  100451 |  9379 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  100451 |  9380 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9381 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9382 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9383 | `				return SXERR_ABORT;` |
|       - |  9384 | `			}` |
|   50223 |  9385 | `		}` |
|       - |  9386 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  100451 |  9387 | `		if( rc == SXRET_OK ){` |
|  100451 |  9388 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  100451 |  9389 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9390 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9391 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9392 | `				return SXERR_ABORT;` |
|       - |  9393 | `			}` |
|   50223 |  9394 | `		}` |
|   50223 |  9395 | `	}` |
|  100451 |  9396 | `	SySetRelease(&aUseEntries);` |
|  100451 |  9397 | `	SySetRelease(&aInterfaces);` |
|  100451 |  9398 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9399 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9400 | `		return SXERR_ABORT;` |
|       - |  9401 | `	}` |
|   50223 |  9402 | `done:` |
|       - |  9403 | `	/* Point beyond the class body */` |
|  100479 |  9404 | `	pGen->pIn = &pEnd[1];` |
|  100479 |  9405 | `	pGen->pEnd = pTmp;` |
|  100479 |  9406 | `	return PH7_OK;` |
|   50243 |  9407 |  |
|       - |  9408 | `/* Compile a named class declaration (the common case). */` |
|  100450 |  9409 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9410 |  |
|  100455 |  9411 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9412 |  |
|       - |  9413 | `/*` |
|       - |  9414 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9415 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9416 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9417 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9418 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9419 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9420 | ` */` |
|      26 |  9421 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       3 |  9422 |  |
|       - |  9423 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9424 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9425 | `	SyString sName;` |
|       - |  9426 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9427 | `	ph7_value *pObj;` |
|      29 |  9428 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9429 | `	sxu32 nIdx,nLen;` |
|       - |  9430 | `	sxi32 nArg,rc;` |
|      13 |  9431 | `	SXUNUSED(iCompileFlag);` |
|       - |  9432 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      29 |  9433 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      29 |  9434 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9435 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9436 | `	}` |
|      29 |  9437 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9438 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9439 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9440 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      29 |  9441 | `	pArgStart = pArgEnd = 0;` |
|      29 |  9442 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      29 |  9443 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9444 | `		return rc;` |
|       - |  9445 | `	}` |
|       - |  9446 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9447 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      29 |  9448 | `	nArg = 0;` |
|      29 |  9449 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9450 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9451 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9452 | `		SyToken *pArgNext;` |
|       7 |  9453 | `		pGen->pIn = pArgStart;` |
|       7 |  9454 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9455 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9456 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9457 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 |  9458 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9459 | `					pGen->pIn = pSavedIn;` |
|     ! 0 |  9460 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 |  9461 | `					return SXERR_ABORT;` |
|       - |  9462 | `				}` |
|       7 |  9463 | `				nArg++;` |
|       3 |  9464 | `			}` |
|       7 |  9465 | `			pGen->pIn = &pArgNext[1];` |
|       1 |  9466 | `		}` |
|       7 |  9467 | `		pGen->pIn = pSavedIn;` |
|       7 |  9468 | `		pGen->pEnd = pSavedEnd;` |
|       3 |  9469 | `	}` |
|       - |  9470 | `	/* Load the synthesized class name */` |
|      29 |  9471 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  9472 | `	if( pObj == 0 ){` |
|     ! 0 |  9473 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9474 | `		return SXERR_ABORT;` |
|       - |  9475 | `	}` |
|      29 |  9476 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      29 |  9477 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  9478 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      29 |  9479 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      29 |  9480 | `	return SXRET_OK;` |
|      16 |  9481 |  |
|       - |  9482 | `/*` |
|       - |  9483 | ` * Compile a user-defined abstract class.` |
|       - |  9484 | ` *  According to the PHP language reference manual` |
|       - |  9485 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9486 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9487 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9488 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9489 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9490 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9491 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9492 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9493 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9494 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9495 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9496 | ` *   could differ.` |
|       - |  9497 | ` */` |
|       - |  9498 | `/*` |
|       - |  9499 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9500 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9501 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9502 | ` */` |
|  973138 |  9503 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9504 |  |
|  973143 |  9505 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  651153 |  9506 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  651153 |  9507 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  644047 |  9508 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  321994 |  9509 | `	}` |
|  965983 |  9510 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  965923 |  9511 | `	return FALSE;` |
|  486574 |  9512 |  |
|       - |  9513 | `/*` |
|       - |  9514 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9515 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9516 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9517 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9518 | ` */` |
|  965918 |  9519 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9520 |  |
|  965923 |  9521 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  965923 |  9522 | `	sxi32 iFlags = 0,iFlag;` |
|  973143 |  9523 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7225 |  9524 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9525 | `			pDup = pIn;` |
|       2 |  9526 | `		}` |
|    7225 |  9527 | `		iFlags \|= iFlag;` |
|    7225 |  9528 | `		pIn++;` |
|       5 |  9529 | `	}` |
|  965923 |  9530 | `	*ppIn = pIn;` |
|  965923 |  9531 | `	if( ppDup ){ *ppDup = pDup; }` |
|  965923 |  9532 | `	return iFlags;` |
|       5 |  9533 |  |
|       - |  9534 | `/*` |
|       - |  9535 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9536 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9537 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9538 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9539 | `` * `readonly`) to their existing handlers.`` |
|       - |  9540 | ` */` |
|  962318 |  9541 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9542 |  |
|  962323 |  9543 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  484766 |  9544 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  964120 |  9545 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9546 |  |
|       - |  9547 | `/*` |
|       - |  9548 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9549 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9550 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9551 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9552 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9553 | ` */` |
|    3600 |  9554 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9555 |  |
|       - |  9556 | `	SyToken *pDup;` |
|    3605 |  9557 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9558 | `	sxi32 rc;` |
|    3605 |  9559 | `	if( pDup ){` |
|       4 |  9560 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9561 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9562 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9563 | `			return SXERR_ABORT;` |
|       - |  9564 | `		}` |
|       1 |  9565 | `	}` |
|    3600 |  9566 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1805 |  9567 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9568 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9569 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9570 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9571 | `			return SXERR_ABORT;` |
|       - |  9572 | `		}` |
|       1 |  9573 | `	}` |
|    3605 |  9574 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1805 |  9575 |  |
|       - |  9576 | `/*` |
|       - |  9577 | ` * Compile a user-defined trait.` |
|       - |  9578 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9579 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9580 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9581 | ` */` |
|      60 |  9582 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9583 |  |
|      65 |  9584 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9585 | `	ph7_class *pClass;` |
|       - |  9586 | `	SyToken *pEnd,*pTmp;` |
|       - |  9587 | `	sxi32 iProtection;` |
|       - |  9588 | `	sxi32 iAttrflags;` |
|       - |  9589 | `	SyString *pName;` |
|       - |  9590 | `	sxi32 nKwrd;` |
|       - |  9591 | `	sxi32 rc;` |
|       - |  9592 | `	/* Jump the 'trait' keyword */` |
|      65 |  9593 | `	pGen->pIn++;` |
|      65 |  9594 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9595 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9596 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9597 | `			return SXERR_ABORT;` |
|       - |  9598 | `		}` |
|     ! 0 |  9599 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9600 | `			pGen->pIn++;` |
|     ! 0 |  9601 | `		}` |
|     ! 0 |  9602 | `		return SXRET_OK;` |
|       - |  9603 | `	}` |
|       - |  9604 | `	/* Extract trait name */` |
|      65 |  9605 | `	pName = &pGen->pIn->sData;` |
|      65 |  9606 | `	pGen->pIn++;` |
|       - |  9607 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9608 | `		SyBlob sFQN;` |
|       - |  9609 | `		SyString sFQNStr;` |
|      65 |  9610 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      65 |  9611 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      65 |  9612 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      65 |  9613 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      65 |  9614 | `		SyBlobRelease(&sFQN);` |
|       - |  9615 | `	}` |
|      65 |  9616 | `	if( pClass == 0 ){` |
|     ! 0 |  9617 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9618 | `		return SXERR_ABORT;` |
|       - |  9619 | `	}` |
|       - |  9620 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      65 |  9621 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9622 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9623 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9624 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9625 | `			return SXERR_ABORT;` |
|       - |  9626 | `		}` |
|     ! 0 |  9627 | `		return SXRET_OK;` |
|       - |  9628 | `	}` |
|      65 |  9629 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      65 |  9630 | `	pEnd = 0;` |
|      65 |  9631 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      65 |  9632 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  9633 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  9634 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9635 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9636 | `			return SXERR_ABORT;` |
|       - |  9637 | `		}` |
|     ! 0 |  9638 | `		return SXRET_OK;` |
|       - |  9639 | `	}` |
|       - |  9640 | `	/* Swap token stream */` |
|      65 |  9641 | `	pTmp = pGen->pEnd;` |
|      65 |  9642 | `	pGen->pEnd = pEnd;` |
|       - |  9643 | `	/* Mark as trait */` |
|      65 |  9644 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9645 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      60 |  9646 | `	for(;;){` |
|     169 |  9647 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9648 | `			pGen->pIn++;` |
|       4 |  9649 | `		}` |
|     145 |  9650 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      65 |  9651 | `			break;` |
|       - |  9652 | `		}` |
|      85 |  9653 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9654 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9655 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9656 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9657 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9658 | `				return SXERR_ABORT;` |
|       - |  9659 | `			}` |
|     ! 0 |  9660 | `			goto done;` |
|       - |  9661 | `		}` |
|      85 |  9662 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      85 |  9663 | `		iAttrflags = 0;` |
|      85 |  9664 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      85 |  9665 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  9666 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9667 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9668 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9669 | `				for(;;){` |
|       - |  9670 | `					ph7_class *pUsedTrait;` |
|       - |  9671 | `					SyString *pUsedName;` |
|       5 |  9672 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9673 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9674 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9675 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9676 | `							return SXERR_ABORT;` |
|       - |  9677 | `						}` |
|     ! 0 |  9678 | `						break;` |
|       - |  9679 | `					}` |
|       5 |  9680 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9681 | `					{` |
|       - |  9682 | `						SyBlob sResolved;` |
|       5 |  9683 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9684 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9685 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9686 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9687 | `						SyBlobRelease(&sResolved);` |
|       - |  9688 | `					}` |
|       5 |  9689 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9690 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9691 | `					}` |
|       5 |  9692 | `					if( pUsedTrait == 0 ){` |
|       4 |  9693 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9694 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9695 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9696 | `							return SXERR_ABORT;` |
|       - |  9697 | `						}` |
|       2 |  9698 | `					}else{` |
|       3 |  9699 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9700 | `					}` |
|       5 |  9701 | `					pGen->pIn++;` |
|       5 |  9702 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  9703 | `						break;` |
|       - |  9704 | `					}` |
|     ! 0 |  9705 | `					pGen->pIn++;` |
|     ! 0 |  9706 | `				}` |
|       5 |  9707 | `				continue;` |
|       - |  9708 | `			}` |
|      81 |  9709 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  9710 | `				iProtection = nKwrd;` |
|      73 |  9711 | `				pGen->pIn++;` |
|      68 |  9712 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  9713 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9714 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9715 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9716 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9717 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9718 | `						return SXERR_ABORT;` |
|       - |  9719 | `					}` |
|     ! 0 |  9720 | `					goto done;` |
|       - |  9721 | `				}` |
|      73 |  9722 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  9723 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9724 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9725 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9726 | `							return SXERR_ABORT;` |
|       - |  9727 | `						}` |
|     ! 0 |  9728 | `						goto done;` |
|       - |  9729 | `					}` |
|      12 |  9730 | `					continue;` |
|       - |  9731 | `				}` |
|      63 |  9732 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  9733 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  9734 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9735 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9736 | `							return SXERR_ABORT;` |
|       - |  9737 | `						}` |
|     ! 0 |  9738 | `						goto done;` |
|       - |  9739 | `					}` |
|       5 |  9740 | `					continue;` |
|       - |  9741 | `				}` |
|      58 |  9742 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  9743 | `			}` |
|      66 |  9744 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  9745 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9746 | `					"Traits cannot have constants");` |
|     ! 0 |  9747 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9748 | `					return SXERR_ABORT;` |
|       - |  9749 | `				}` |
|     ! 0 |  9750 | `				goto done;` |
|     ! 0 |  9751 | `			}else{` |
|      66 |  9752 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9753 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9754 | `					pGen->pIn++;` |
|       5 |  9755 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9756 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9757 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9758 | `							iProtection = nKwrd;` |
|     ! 0 |  9759 | `							pGen->pIn++;` |
|     ! 0 |  9760 | `						}` |
|       1 |  9761 | `					}` |
|       4 |  9762 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9763 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9764 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9765 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9766 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9767 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9768 | `							return SXERR_ABORT;` |
|       - |  9769 | `						}` |
|     ! 0 |  9770 | `						goto done;` |
|       - |  9771 | `					}` |
|       5 |  9772 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9773 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9774 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9775 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9776 | `								return SXERR_ABORT;` |
|       - |  9777 | `							}` |
|     ! 0 |  9778 | `							goto done;` |
|       - |  9779 | `						}` |
|       3 |  9780 | `						continue;` |
|       - |  9781 | `					}` |
|       3 |  9782 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9783 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9784 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9785 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9786 | `								return SXERR_ABORT;` |
|       - |  9787 | `							}` |
|     ! 0 |  9788 | `							goto done;` |
|       - |  9789 | `						}` |
|     ! 0 |  9790 | `						continue;` |
|       - |  9791 | `					}` |
|       3 |  9792 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      63 |  9793 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 |  9794 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 |  9795 | `					pGen->pIn++;` |
|       6 |  9796 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 |  9797 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  9798 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  9799 | `							iProtection = nKwrd;` |
|       6 |  9800 | `							pGen->pIn++;` |
|       2 |  9801 | `						}` |
|       2 |  9802 | `					}` |
|       6 |  9803 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9804 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9805 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9806 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9807 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9808 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9809 | `							return SXERR_ABORT;` |
|       - |  9810 | `						}` |
|     ! 0 |  9811 | `						goto done;` |
|       - |  9812 | `					}` |
|       6 |  9813 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9814 | `				}` |
|      64 |  9815 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9816 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9817 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9818 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9819 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9820 | `						return SXERR_ABORT;` |
|       - |  9821 | `					}` |
|     ! 0 |  9822 | `					goto done;` |
|       - |  9823 | `				}` |
|      64 |  9824 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9825 | `					pGen->pIn++;` |
|     ! 0 |  9826 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9827 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9828 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9829 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9830 | `							return SXERR_ABORT;` |
|       - |  9831 | `						}` |
|     ! 0 |  9832 | `						goto done;` |
|       - |  9833 | `					}` |
|     ! 0 |  9834 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9835 | `				}else{` |
|      64 |  9836 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9837 | `				}` |
|      64 |  9838 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9839 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9840 | `						return SXERR_ABORT;` |
|       - |  9841 | `					}` |
|     ! 0 |  9842 | `					goto done;` |
|       - |  9843 | `				}` |
|       - |  9844 | `			}` |
|      34 |  9845 | `		}else{` |
|     ! 0 |  9846 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9847 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9848 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9849 | `					return SXERR_ABORT;` |
|       - |  9850 | `				}` |
|     ! 0 |  9851 | `				goto done;` |
|       - |  9852 | `			}` |
|       - |  9853 | `		}` |
|       4 |  9854 | `	}` |
|       - |  9855 | `	/* Install the trait */` |
|      65 |  9856 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      65 |  9857 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9858 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9859 | `		return SXERR_ABORT;` |
|       - |  9860 | `	}` |
|      30 |  9861 | `done:` |
|       - |  9862 | `	/* Point beyond the trait body */` |
|      65 |  9863 | `	pGen->pIn = &pEnd[1];` |
|      65 |  9864 | `	pGen->pEnd = pTmp;` |
|      65 |  9865 | `	return PH7_OK;` |
|      35 |  9866 |  |
|       - |  9867 | `/*` |
|       - |  9868 | ` * Compile a user-defined class.` |
|       - |  9869 | ` *  According to the PHP language reference manual` |
|       - |  9870 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9871 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9872 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9873 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9874 | ` *   and functions (called "methods").` |
|       - |  9875 | ` */` |
|   96850 |  9876 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 |  9877 |  |
|       - |  9878 | `	sxi32 rc;` |
|   96855 |  9879 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   96855 |  9880 | `	return rc;` |
|       5 |  9881 |  |
|       - |  9882 | `/*` |
|       - |  9883 | ` * Exception handling.` |
|       - |  9884 | ` *  According to the PHP language reference manual` |
|       - |  9885 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9886 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9887 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9888 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9889 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9890 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9891 | ` *    (or re-thrown) within a catch block.` |
|       - |  9892 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9893 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9894 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9895 | ` *    been defined with set_exception_handler().` |
|       - |  9896 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9897 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9898 | ` */` |
|       - |  9899 | `/*` |
|       - |  9900 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9901 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9902 | ` * indicates failure.` |
|       - |  9903 | ` */` |
|   14478 |  9904 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  9905 |  |
|   14483 |  9906 | `	sxi32 rc = SXRET_OK;` |
|   14483 |  9907 | `	if( pRoot->pOp ){` |
|   14473 |  9908 | `		switch( pRoot->pOp->iOp ){` |
|    7234 |  9909 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9910 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9911 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9912 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9913 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9914 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   14473 |  9915 | `			break;` |
|     ! 0 |  9916 | `		default:` |
|       - |  9917 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9918 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9919 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9920 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9921 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9922 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9923 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9924 | `			}` |
|     ! 0 |  9925 | `			break;` |
|       - |  9926 | `		}` |
|    7249 |  9927 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9928 | `		/* Unexpected expression */` |
|     ! 0 |  9929 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9930 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9931 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9932 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9933 | `		}` |
|     ! 0 |  9934 | `	}` |
|   14483 |  9935 | `	return rc;` |
|       5 |  9936 |  |
|       - |  9937 | `/*` |
|       - |  9938 | ` * Compile a 'throw' statement.` |
|       - |  9939 | ` * throw: This is how you trigger an exception.` |
|       - |  9940 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9941 | ` */` |
|   14442 |  9942 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 |  9943 |  |
|   14447 |  9944 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9945 | `	GenBlock *pBlock;` |
|       - |  9946 | `	sxu32 nIdx;` |
|       - |  9947 | `	sxi32 rc;` |
|   14447 |  9948 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9949 | `	/* Compile the expression */` |
|   14447 |  9950 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   14447 |  9951 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9952 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9953 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9954 | `			return SXERR_ABORT;` |
|       - |  9955 | `		}` |
|     ! 0 |  9956 | `		return SXRET_OK;` |
|       - |  9957 | `	}` |
|   14447 |  9958 | `	pBlock = pGen->pCurrent;` |
|       - |  9959 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   57241 |  9960 | `	while(pBlock->pParent){` |
|   57237 |  9961 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   14443 |  9962 | `			break;` |
|       - |  9963 | `		}` |
|       - |  9964 | `		/* Point to the parent block */` |
|   42799 |  9965 | `		pBlock = pBlock->pParent;` |
|       5 |  9966 | `	}` |
|       - |  9967 | `	/* Emit the throw instruction */` |
|   14447 |  9968 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9969 | `	/* Emit the jump */` |
|   14447 |  9970 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   14447 |  9971 | `	return SXRET_OK;` |
|    7226 |  9972 |  |
|       - |  9973 | `/*` |
|       - |  9974 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9975 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9976 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9977 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9978 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9979 | ` */` |
|      36 |  9980 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9981 |  |
|      38 |  9982 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9983 | `	GenBlock *pBlock;` |
|       - |  9984 | `	sxu32 nIdx;` |
|       - |  9985 | `	sxi32 rc;` |
|      18 |  9986 | `	(void)iCompileFlag;` |
|      38 |  9987 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9988 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9989 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9990 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9991 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9992 | `			return SXERR_ABORT;` |
|       - |  9993 | `		}` |
|     ! 0 |  9994 | `		return SXRET_OK;` |
|       - |  9995 | `	}` |
|      38 |  9996 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9997 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9998 | `		return SXERR_ABORT;` |
|       - |  9999 | `	}` |
|      38 | 10000 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10001 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10002 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10003 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10004 | `			return SXERR_ABORT;` |
|       - | 10005 | `		}` |
|     ! 0 | 10006 | `		return SXRET_OK;` |
|       - | 10007 | `	}` |
|       - | 10008 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10009 | `	pBlock = pGen->pCurrent;` |
|      60 | 10010 | `	while( pBlock->pParent ){` |
|      49 | 10011 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10012 | `			break;` |
|       - | 10013 | `		}` |
|      23 | 10014 | `		pBlock = pBlock->pParent;` |
|       1 | 10015 | `	}` |
|      38 | 10016 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10017 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10018 | `	return SXRET_OK;` |
|      20 | 10019 |  |
|       - | 10020 | `/*` |
|       - | 10021 | ` * Compile a 'catch' block.` |
|       - | 10022 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10023 | ` * an object containing the exception information.` |
|       - | 10024 | ` */` |
|     572 | 10025 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10026 |  |
|     577 | 10027 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10028 | `	ph7_exception_block sCatch;` |
|       - | 10029 | `	SySet *pInstrContainer;` |
|       - | 10030 | `	SyString sClassName;` |
|       - | 10031 | `	GenBlock *pCatch;` |
|       - | 10032 | `	SyToken *pToken;` |
|       - | 10033 | `	SyString *pName;` |
|       - | 10034 | `	char *zDup;` |
|       - | 10035 | `	sxi32 rc;` |
|     577 | 10036 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10037 | `	/* Zero the structure */` |
|     577 | 10038 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10039 | `	/* Initialize fields */` |
|     577 | 10040 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     577 | 10041 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     577 | 10042 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10043 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10044 | `			pToken = pGen->pIn;` |
|     ! 0 | 10045 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10046 | `				pToken--;` |
|     ! 0 | 10047 | `			}` |
|     ! 0 | 10048 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10049 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10050 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10051 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10052 | `				return SXERR_ABORT;` |
|       - | 10053 | `			}` |
|     ! 0 | 10054 | `			return SXERR_INVALID;` |
|       - | 10055 | `	}` |
|       - | 10056 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     577 | 10057 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     300 | 10058 | `	for(;;){` |
|       - | 10059 | `		SyBlob sResolved;` |
|     605 | 10060 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     605 | 10061 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10062 | `			SyBlobRelease(&sResolved);` |
|       6 | 10063 | `			pToken = pGen->pIn;` |
|       6 | 10064 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10065 | `				pToken--;` |
|     ! 0 | 10066 | `			}` |
|       8 | 10067 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10068 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10069 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10070 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10071 | `				return SXERR_ABORT;` |
|       - | 10072 | `			}` |
|       6 | 10073 | `			return SXERR_INVALID;` |
|       - | 10074 | `		}` |
|       - | 10075 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10076 | `		 * transient SyBlob allocation. */` |
|     899 | 10077 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     596 | 10078 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     601 | 10079 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     601 | 10080 | `		SyBlobRelease(&sResolved);` |
|     601 | 10081 | `		if( zDup == 0 ){` |
|     ! 0 | 10082 | `			goto Mem;` |
|       - | 10083 | `		}` |
|     601 | 10084 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     601 | 10085 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10086 | `			goto Mem;` |
|       - | 10087 | `		}` |
|       - | 10088 | `		/* Check for '\|' (multi-catch separator) */` |
|     596 | 10089 | `		if( pGen->pIn < pGen->pEnd &&` |
|     596 | 10090 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 10091 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 10092 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 10093 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 10094 | `			continue;` |
|       - | 10095 | `		}` |
|     573 | 10096 | `		break;` |
|     ! 0 | 10097 | `	}` |
|     568 | 10098 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     573 | 10099 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10100 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10101 | `			pToken = pGen->pIn;` |
|     ! 0 | 10102 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10103 | `				pToken--;` |
|     ! 0 | 10104 | `			}` |
|     ! 0 | 10105 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10106 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10107 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10108 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10109 | `				return SXERR_ABORT;` |
|       - | 10110 | `			}` |
|     ! 0 | 10111 | `			return SXERR_INVALID;` |
|       - | 10112 | `	}` |
|     573 | 10113 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10114 | `	/* Duplicate instance name */` |
|     573 | 10115 | `	pName = &pGen->pIn->sData;` |
|     573 | 10116 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     573 | 10117 | `	if( zDup == 0 ){` |
|     ! 0 | 10118 | `		goto Mem;` |
|       - | 10119 | `	}` |
|     573 | 10120 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     573 | 10121 | `	pGen->pIn++;` |
|     573 | 10122 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10123 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10124 | `		pToken = pGen->pIn;` |
|     ! 0 | 10125 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10126 | `			pToken--;` |
|     ! 0 | 10127 | `		}` |
|     ! 0 | 10128 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10129 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10130 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10131 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10132 | `			return SXERR_ABORT;` |
|       - | 10133 | `		}` |
|     ! 0 | 10134 | `		return SXERR_INVALID;` |
|       - | 10135 | `	}` |
|       - | 10136 | `	/* Compile the block */` |
|     573 | 10137 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10138 | `	/* Create the catch block */` |
|     573 | 10139 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     573 | 10140 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10141 | `		return SXERR_ABORT;` |
|       - | 10142 | `	}` |
|       - | 10143 | `	/* Swap bytecode container */` |
|     573 | 10144 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     573 | 10145 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10146 | `	/* Compile the block */` |
|     573 | 10147 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10148 | `	/* Fix forward jumps now the destination is resolved  */` |
|     573 | 10149 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10150 | `	/* Emit the DONE instruction */` |
|     573 | 10151 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10152 | `	/* Leave the block */` |
|     573 | 10153 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10154 | `	/* Restore the default container */` |
|     573 | 10155 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10156 | `	/* Install the catch block */` |
|     573 | 10157 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     573 | 10158 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10159 | `		goto Mem;` |
|       - | 10160 | `	}` |
|     573 | 10161 | `	return SXRET_OK;` |
|     ! 0 | 10162 | `Mem:` |
|     ! 0 | 10163 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10164 | `	return SXERR_ABORT;` |
|     291 | 10165 |  |
|       - | 10166 | `/*` |
|       - | 10167 | ` * Compile a 'try' block.` |
|       - | 10168 | ` * A function using an exception should be in a "try" block.` |
|       - | 10169 | ` * If the exception does not trigger, the code will continue` |
|       - | 10170 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10171 | ` * is "thrown".` |
|       - | 10172 | ` */` |
|     610 | 10173 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10174 |  |
|       - | 10175 | `	ph7_exception *pException;` |
|     615 | 10176 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10177 | `	GenBlock *pTry;` |
|       - | 10178 | `	sxu32 nJmpIdx;` |
|       - | 10179 | `	sxi32 rc;` |
|       - | 10180 | `	/* Create the exception container */` |
|     615 | 10181 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     615 | 10182 | `	if( pException == 0 ){` |
|     ! 0 | 10183 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 10184 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10185 | `		return SXERR_ABORT;` |
|       - | 10186 | `	}` |
|       - | 10187 | `	/* Zero the structure */` |
|     615 | 10188 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 10189 | `	/* Initialize fields */` |
|     615 | 10190 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     615 | 10191 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     615 | 10192 | `	pException->iHasFinally = 0;` |
|     615 | 10193 | `	pException->iFinallyDone = 0;` |
|     615 | 10194 | `	pException->pVm = pGen->pVm;` |
|       - | 10195 | `	/* Create the try block */` |
|     615 | 10196 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     615 | 10197 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10198 | `		return SXERR_ABORT;` |
|       - | 10199 | `	}` |
|       - | 10200 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     615 | 10201 | `	pTry->pUserData = pException;` |
|       - | 10202 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     615 | 10203 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 10204 | `	/* Fix the jump later when the destination is resolved */` |
|     615 | 10205 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     615 | 10206 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 10207 | `	/* Compile the block */` |
|     615 | 10208 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     615 | 10209 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10210 | `		return SXERR_ABORT;` |
|       - | 10211 | `	}` |
|       - | 10212 | `	/* Fix forward jumps now the destination is resolved */` |
|     615 | 10213 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10214 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     615 | 10215 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 10216 | `	/* Leave the block */` |
|     615 | 10217 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10218 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     615 | 10219 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     608 | 10220 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 10221 | `		/* Compile one or more catch blocks */` |
|     568 | 10222 | `		for(;;){` |
|    1136 | 10223 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     919 | 10224 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     287 | 10225 | `					break;` |
|       - | 10226 | `			}` |
|     577 | 10227 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     577 | 10228 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10229 | `				return SXERR_ABORT;` |
|       - | 10230 | `			}` |
|       5 | 10231 | `		}` |
|     282 | 10232 | `	}` |
|       - | 10233 | `	/* Compile optional finally block */` |
|     615 | 10234 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     334 | 10235 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10236 | `		SySet *pInstrContainer;` |
|       - | 10237 | `		GenBlock *pFinBlock;` |
|     107 | 10238 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10239 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     107 | 10240 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     107 | 10241 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10242 | `			return SXERR_ABORT;` |
|       - | 10243 | `		}` |
|       - | 10244 | `		/* Swap bytecode container */` |
|     107 | 10245 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     107 | 10246 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10247 | `		/* Compile the finally body */` |
|     107 | 10248 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     107 | 10249 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10250 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10251 | `			return SXERR_ABORT;` |
|       - | 10252 | `		}` |
|       - | 10253 | `		/* Fix forward jumps now the destination is resolved */` |
|     107 | 10254 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10255 | `		/* Emit DONE to terminate the finally block */` |
|     107 | 10256 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10257 | `		/* Leave the block */` |
|     107 | 10258 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10259 | `		/* Restore the default container */` |
|     107 | 10260 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     107 | 10261 | `		pException->iHasFinally = 1;` |
|      51 | 10262 | `	}` |
|       - | 10263 | `	/* Must have at least one catch or finally */` |
|     615 | 10264 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 | 10265 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10266 | `			"Cannot use try without catch or finally");` |
|       8 | 10267 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10268 | `			return SXERR_ABORT;` |
|       - | 10269 | `		}` |
|       3 | 10270 | `	}` |
|     615 | 10271 | `	return SXRET_OK;` |
|     310 | 10272 |  |
|       - | 10273 | `/*` |
|       - | 10274 | ` * Compile a switch block.` |
|       - | 10275 | ` *  (See block-comment below for more information)` |
|       - | 10276 | ` */` |
|     112 | 10277 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 10278 |  |
|     117 | 10279 | `	sxi32 rc = SXRET_OK;` |
|     117 | 10280 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 10281 | `		/* Unexpected token */` |
|     ! 0 | 10282 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10283 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10284 | `			return SXERR_ABORT;` |
|       - | 10285 | `		}` |
|     ! 0 | 10286 | `		pGen->pIn++;` |
|     ! 0 | 10287 | `	}` |
|     117 | 10288 | `	pGen->pIn++;` |
|       - | 10289 | `	/* First instruction to execute in this block. */` |
|     117 | 10290 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10291 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 10292 | `	 * or the '}' token */` |
|     206 | 10293 | `	for(;;){` |
|     417 | 10294 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10295 | `			/* No more input to process */` |
|     ! 0 | 10296 | `			break;` |
|       - | 10297 | `		}` |
|     417 | 10298 | `		rc = SXRET_OK;` |
|     417 | 10299 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 10300 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 10301 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 10302 | `					/* Unexpected token */` |
|     ! 0 | 10303 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10304 | `						&pGen->pIn->sData);` |
|     ! 0 | 10305 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10306 | `						return SXERR_ABORT;` |
|       - | 10307 | `					}` |
|       - | 10308 | `					/* FALL THROUGH */` |
|     ! 0 | 10309 | `				}` |
|      31 | 10310 | `				rc = SXERR_EOF;` |
|      31 | 10311 | `				break;` |
|       - | 10312 | `			}` |
|      32 | 10313 | `		}else{` |
|       - | 10314 | `			sxi32 nKwrd;` |
|       - | 10315 | `			/* Extract the keyword */` |
|     337 | 10316 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 10317 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 10318 | `				break;` |
|       - | 10319 | `			}` |
|     253 | 10320 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10321 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 10322 | `					/* Unexpected token */` |
|     ! 0 | 10323 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10324 | `						&pGen->pIn->sData);` |
|     ! 0 | 10325 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10326 | `						return SXERR_ABORT;` |
|       - | 10327 | `					}` |
|       - | 10328 | `					/* FALL THROUGH */` |
|     ! 0 | 10329 | `				}` |
|       - | 10330 | `				/* Block compiled */` |
|       3 | 10331 | `				break;` |
|       - | 10332 | `			}` |
|       - | 10333 | `		}` |
|       - | 10334 | `		/* Compile block */` |
|     305 | 10335 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 10336 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10337 | `			return SXERR_ABORT;` |
|       - | 10338 | `		}` |
|       5 | 10339 | `	}` |
|     117 | 10340 | `	return rc;` |
|      61 | 10341 |  |
|       - | 10342 | `/*` |
|       - | 10343 | ` * Compile a case eXpression.` |
|       - | 10344 | ` *  (See block-comment below for more information)` |
|       - | 10345 | ` */` |
|      92 | 10346 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 10347 |  |
|       - | 10348 | `	SySet *pInstrContainer;` |
|       - | 10349 | `	SyToken *pEnd,*pTmp;` |
|      97 | 10350 | `	sxi32 iNest = 0;` |
|       - | 10351 | `	sxi32 rc;` |
|       - | 10352 | `	/* Delimit the expression */` |
|      97 | 10353 | `	pEnd = pGen->pIn;` |
|     197 | 10354 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 10355 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 10356 | `			/* Increment nesting level */` |
|       3 | 10357 | `			iNest++;` |
|     196 | 10358 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 10359 | `			/* Decrement nesting level */` |
|       3 | 10360 | `			iNest--;` |
|     194 | 10361 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 10362 | `			break;` |
|       - | 10363 | `		}` |
|     105 | 10364 | `		pEnd++;` |
|       5 | 10365 | `	}` |
|      97 | 10366 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 10367 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 10368 | `		if( rc == SXERR_ABORT ){` |
|       - | 10369 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10370 | `			return SXERR_ABORT;` |
|       - | 10371 | `		}` |
|     ! 0 | 10372 | `	}` |
|       - | 10373 | `	/* Swap token stream */` |
|      97 | 10374 | `	pTmp = pGen->pEnd;` |
|      97 | 10375 | `	pGen->pEnd = pEnd;` |
|      97 | 10376 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 10377 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 10378 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 10379 | `	/* Emit the done instruction */` |
|      97 | 10380 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 10381 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10382 | `	/* Update token stream */` |
|      97 | 10383 | `	pGen->pIn  = pEnd;` |
|      97 | 10384 | `	pGen->pEnd = pTmp;` |
|      97 | 10385 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10386 | `		return SXERR_ABORT;` |
|       - | 10387 | `	}` |
|      97 | 10388 | `	return SXRET_OK;` |
|      51 | 10389 |  |
|       - | 10390 | `/*` |
|       - | 10391 | ` * Compile the smart switch statement.` |
|       - | 10392 | ` * According to the PHP language reference manual` |
|       - | 10393 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 10394 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 10395 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 10396 | ` *  This is exactly what the switch statement is for.` |
|       - | 10397 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 10398 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 10399 | ` *  of the outer loop, use continue 2.` |
|       - | 10400 | ` *  Note that switch/case does loose comparision.` |
|       - | 10401 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 10402 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 10403 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 10404 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 10405 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 10406 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 10407 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 10408 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 10409 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 10410 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 10411 | ` *  list for the next case.` |
|       - | 10412 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 10413 | ` *  or floating-point numbers and strings.` |
|       - | 10414 | ` */` |
|      28 | 10415 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 10416 |  |
|       - | 10417 | `	GenBlock *pSwitchBlock;` |
|       - | 10418 | `	SyToken *pTmp,*pEnd;` |
|       - | 10419 | `	ph7_switch *pSwitch;` |
|       - | 10420 | `	sxu32 nToken;` |
|       - | 10421 | `	sxu32 nLine;` |
|       - | 10422 | `	sxi32 rc;` |
|      33 | 10423 | `	nLine = pGen->pIn->nLine;` |
|       - | 10424 | `	/* Jump the 'switch' keyword */` |
|      33 | 10425 | `	pGen->pIn++;` |
|      33 | 10426 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 10427 | `		/* Syntax error */` |
|     ! 0 | 10428 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 10429 | `		if( rc == SXERR_ABORT ){` |
|       - | 10430 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10431 | `			return SXERR_ABORT;` |
|       - | 10432 | `		}` |
|     ! 0 | 10433 | `		goto Synchronize;` |
|       - | 10434 | `	}` |
|       - | 10435 | `	/* Jump the left parenthesis '(' */` |
|      33 | 10436 | `	pGen->pIn++;` |
|      33 | 10437 | `	pEnd = 0; /* cc warning */` |
|       - | 10438 | `	/* Create the loop block */` |
|      47 | 10439 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 10440 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 10441 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10442 | `		return SXERR_ABORT;` |
|       - | 10443 | `	}` |
|       - | 10444 | `	/* Delimit the condition */` |
|      33 | 10445 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 10446 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 10447 | `		/* Empty expression */` |
|     ! 0 | 10448 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 10449 | `		if( rc == SXERR_ABORT ){` |
|       - | 10450 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10451 | `			return SXERR_ABORT;` |
|       - | 10452 | `		}` |
|     ! 0 | 10453 | `	}` |
|       - | 10454 | `	/* Swap token streams */` |
|      33 | 10455 | `	pTmp = pGen->pEnd;` |
|      33 | 10456 | `	pGen->pEnd = pEnd;` |
|       - | 10457 | `	/* Compile the expression */` |
|      33 | 10458 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 10459 | `	if( rc == SXERR_ABORT ){` |
|       - | 10460 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 10461 | `		return SXERR_ABORT;` |
|       - | 10462 | `	}` |
|       - | 10463 | `	/* Update token stream */` |
|      33 | 10464 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 10465 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10466 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10467 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10468 | `			return SXERR_ABORT;` |
|       - | 10469 | `		}` |
|     ! 0 | 10470 | `		pGen->pIn++;` |
|     ! 0 | 10471 | `	}` |
|      33 | 10472 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 10473 | `	pGen->pEnd = pTmp;` |
|      33 | 10474 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 10475 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 10476 | `			pTmp = pGen->pIn;` |
|     ! 0 | 10477 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 10478 | `				pTmp--;` |
|     ! 0 | 10479 | `			}` |
|       - | 10480 | `			/* Unexpected token */` |
|     ! 0 | 10481 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 10482 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10483 | `				return SXERR_ABORT;` |
|       - | 10484 | `			}` |
|     ! 0 | 10485 | `			goto Synchronize;` |
|       - | 10486 | `	}` |
|       - | 10487 | `	/* Set the delimiter token */` |
|      33 | 10488 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 10489 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 10490 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 10491 | `	}else{` |
|      31 | 10492 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 10493 | `	}` |
|      33 | 10494 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 10495 | `	/* Create the switch blocks container */` |
|      33 | 10496 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 10497 | `	if( pSwitch == 0 ){` |
|       - | 10498 | `		/* Abort compilation */` |
|     ! 0 | 10499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10500 | `		return SXERR_ABORT;` |
|       - | 10501 | `	}` |
|       - | 10502 | `	/* Zero the structure */` |
|      33 | 10503 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 10504 | `	/* Initialize fields */` |
|      33 | 10505 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 10506 | `	/* Emit the switch instruction */` |
|      33 | 10507 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 10508 | `	/* Compile case blocks */` |
|     100 | 10509 | `	for(;;){` |
|       - | 10510 | `		sxu32 nKwrd;` |
|     119 | 10511 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10512 | `			/* No more input to process */` |
|     ! 0 | 10513 | `			break;` |
|       - | 10514 | `		}` |
|     119 | 10515 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 10516 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 10517 | `				/* Unexpected token */` |
|     ! 0 | 10518 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10519 | `					&pGen->pIn->sData);` |
|     ! 0 | 10520 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10521 | `					return SXERR_ABORT;` |
|       - | 10522 | `				}` |
|       - | 10523 | `				/* FALL THROUGH */` |
|     ! 0 | 10524 | `			}` |
|       - | 10525 | `			/* Block compiled */` |
|     ! 0 | 10526 | `			break;` |
|       - | 10527 | `		}` |
|       - | 10528 | `		/* Extract the keyword */` |
|     119 | 10529 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 10530 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10531 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 10532 | `				/* Unexpected token */` |
|     ! 0 | 10533 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10534 | `					&pGen->pIn->sData);` |
|     ! 0 | 10535 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10536 | `					return SXERR_ABORT;` |
|       - | 10537 | `				}` |
|       - | 10538 | `				/* FALL THROUGH */` |
|     ! 0 | 10539 | `			}` |
|       - | 10540 | `			/* Block compiled */` |
|       3 | 10541 | `			break;` |
|       - | 10542 | `		}` |
|     117 | 10543 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 10544 | `			/*` |
|       - | 10545 | `			 * Accroding to the PHP language reference manual` |
|       - | 10546 | `			 *  A special case is the default case. This case matches anything` |
|       - | 10547 | `			 *  that wasn't matched by the other cases.` |
|       - | 10548 | `			 */` |
|      25 | 10549 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 10550 | `				/* Default case already compiled */` |
|     ! 0 | 10551 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 10552 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10553 | `					return SXERR_ABORT;` |
|       - | 10554 | `				}` |
|     ! 0 | 10555 | `			}` |
|      25 | 10556 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 10557 | `			/* Compile the default block */` |
|      25 | 10558 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 10559 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10560 | `				return SXERR_ABORT;` |
|      25 | 10561 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 10562 | `				break;` |
|       1 | 10563 | `			}` |
|      98 | 10564 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 10565 | `			ph7_case_expr sCase;` |
|       - | 10566 | `			/* Standard case block */` |
|      97 | 10567 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 10568 | `			/* initialize the structure */` |
|      97 | 10569 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 10570 | `			/* Compile the case expression */` |
|      97 | 10571 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 10572 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10573 | `				return SXERR_ABORT;` |
|       - | 10574 | `			}` |
|       - | 10575 | `			/* Compile the case block */` |
|      97 | 10576 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 10577 | `			/* Insert in the switch container */` |
|      97 | 10578 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 10579 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10580 | `				return SXERR_ABORT;` |
|      97 | 10581 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 10582 | `				break;` |
|       - | 10583 | `			}` |
|      47 | 10584 | `		}else{` |
|       - | 10585 | `			/* Unexpected token */` |
|     ! 0 | 10586 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10587 | `				&pGen->pIn->sData);` |
|     ! 0 | 10588 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10589 | `				return SXERR_ABORT;` |
|       - | 10590 | `			}` |
|     ! 0 | 10591 | `			break;` |
|       - | 10592 | `		}` |
|       5 | 10593 | `	}` |
|       - | 10594 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 10595 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 10596 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10597 | `	/* Release the loop block */` |
|      33 | 10598 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 10599 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 10600 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 10601 | `		pGen->pIn++;` |
|      14 | 10602 | `	}` |
|       - | 10603 | `	/* Statement successfully compiled */` |
|      33 | 10604 | `	return SXRET_OK;` |
|     ! 0 | 10605 | `Synchronize:` |
|       - | 10606 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 10607 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 10608 | `		pGen->pIn++;` |
|     ! 0 | 10609 | `	}` |
|     ! 0 | 10610 | `	return SXRET_OK;` |
|      19 | 10611 |  |
|       - | 10612 | `/*` |
|       - | 10613 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 10614 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 10615 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 10616 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 10617 | ` */` |
|       - | 10618 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 10619 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 10620 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 10621 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 10622 |  |
|       - | 10623 | `/*` |
|       - | 10624 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 10625 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 10626 | ` * patched entries from the pending set.` |
|       - | 10627 | ` */` |
| 2634500 | 10628 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 10629 |  |
| 2634505 | 10630 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 10631 | `	sxu32 nTarget;` |
|       - | 10632 | `	sxu32 *aIdx;` |
|       - | 10633 | `	sxu32 i;` |
| 2634505 | 10634 | `	if( nCur <= nBaseline ){` |
| 2634411 | 10635 | `		return;` |
|       - | 10636 | `	}` |
|      97 | 10637 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      97 | 10638 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     199 | 10639 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     105 | 10640 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     105 | 10641 | `		if( pInstr ){` |
|     105 | 10642 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      51 | 10643 | `		}` |
|      54 | 10644 | `	}` |
|      97 | 10645 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1317255 | 10646 |  |
|       - | 10647 |  |
|       - | 10648 | `/*` |
|       - | 10649 | ` * By-reference out-parameters of builtin functions.` |
|       - | 10650 | ` *` |
|       - | 10651 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 10652 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 10653 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 10654 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 10655 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 10656 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 10657 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 10658 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 10659 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 10660 | ` * creates it" behaviour).` |
|       - | 10661 | ` *` |
|       - | 10662 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 10663 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 10664 | ` */` |
|  442418 | 10665 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 10666 |  |
|       - | 10667 | `	static const struct {` |
|       - | 10668 | `		const char *zName;` |
|       - | 10669 | `		sxu32 nByte;` |
|       - | 10670 | `		sxu32 mask;` |
|       - | 10671 | `	} aByRef[] = {` |
|       - | 10672 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10673 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10674 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10675 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10676 | `	};` |
|       - | 10677 | `	sxu32 i;` |
|  442423 | 10678 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1537 | 10679 | `		return 0;` |
|       - | 10680 | `	}` |
| 2204215 | 10681 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1763392 | 10682 | `		if( pName->nByte == aByRef[i].nByte` |
|  903911 | 10683 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      73 | 10684 | `			return aByRef[i].mask;` |
|       - | 10685 | `		}` |
|  881667 | 10686 | `	}` |
|  440823 | 10687 | `	return 0;` |
|  221214 | 10688 |  |
|       - | 10689 | `/*` |
|       - | 10690 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 10691 | ` *` |
|       - | 10692 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 10693 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 10694 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 10695 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 10696 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 10697 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 10698 | ` */` |
|  442418 | 10699 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 10700 |  |
|       - | 10701 | `	SyToken *p, *pEnd;` |
|  442423 | 10702 | `	pOut->zString = 0;` |
|  442423 | 10703 | `	pOut->nByte = 0;` |
|  442423 | 10704 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 10705 | `		return;` |
|       - | 10706 | `	}` |
|  442423 | 10707 | `	p = pLeft->pStart;` |
|  442423 | 10708 | `	pEnd = pLeft->pEnd;` |
|       - | 10709 | `	/* Optional single leading namespace separator (absolute path). */` |
|  442423 | 10710 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3571 | 10711 | `		p++;` |
|    1783 | 10712 | `	}` |
|  442423 | 10713 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1509 | 10714 | `		return;` |
|       - | 10715 | `	}` |
|       - | 10716 | `	/* Must be a single component: nothing follows the name token. */` |
|  440919 | 10717 | `	if( p + 1 != pEnd ){` |
|      32 | 10718 | `		return;` |
|       - | 10719 | `	}` |
|  440891 | 10720 | `	*pOut = p->sData;` |
|  221214 | 10721 |  |
|       - | 10722 | `/*` |
|       - | 10723 | ` * Generate bytecode for a given expression tree.` |
|       - | 10724 | ` * If something goes wrong while generating bytecode` |
|       - | 10725 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 10726 | ` * this function takes care of generating the appropriate` |
|       - | 10727 | ` * error message.` |
|       - | 10728 | ` */` |
| 3525040 | 10729 | `static sxi32 GenStateEmitExprCode(` |
|       - | 10730 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10731 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 10732 | `	sxi32 iFlags /* Control flags */` |
|       - | 10733 | `	)` |
|       5 | 10734 |  |
|       - | 10735 | `	VmInstr *pInstr;` |
|       - | 10736 | `	sxu32 nJmpIdx;` |
| 3525045 | 10737 | `	sxi32 iP1 = 0;` |
| 3525045 | 10738 | `	sxu32 iP2 = 0;` |
| 3525045 | 10739 | `	void *p3  = 0;` |
|       - | 10740 | `	sxi32 iVmOp;` |
|       - | 10741 | `	sxi32 rc;` |
| 3525045 | 10742 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3525045 | 10743 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 3525045 | 10744 | `	sxu32 nRhsNsBase = 0;` |
| 3525045 | 10745 | `	if( pNode->xCode ){` |
|       - | 10746 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 10747 | `		/* Compile node */` |
| 2200469 | 10748 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2200469 | 10749 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2200469 | 10750 | `		RE_SWAP_DELIMITER(pGen);` |
| 2200469 | 10751 | `		return rc;` |
|       - | 10752 | `	}` |
| 1324581 | 10753 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10754 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10755 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10756 | `		return SXERR_ABORT;` |
|       - | 10757 | `	}` |
| 1324581 | 10758 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1324581 | 10759 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      59 | 10760 | `		sxu32 nJmp = 0;` |
|       - | 10761 | `		sxu32 nNcNsBase;` |
|       - | 10762 | `		VmInstr *pInstrFix;` |
|       - | 10763 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10764 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10765 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10766 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10767 | `		 * stack slot carries a writable nIdx. */` |
|      59 | 10768 | `		if( pNode->pRight ){` |
|      59 | 10769 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10770 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      59 | 10771 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10772 | `				return rc;` |
|       - | 10773 | `			}` |
|      59 | 10774 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10775 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10776 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10777 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10778 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10779 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10780 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10781 | `			 * cascade for the actual write path stays correct. */` |
|      59 | 10782 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      59 | 10783 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      29 | 10784 | `				pInstrFix->iP2 = 3;` |
|      13 | 10785 | `			}` |
|      28 | 10786 | `		}` |
|       - | 10787 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      59 | 10788 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10789 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      59 | 10790 | `		if( pNode->pLeft ){` |
|      59 | 10791 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10792 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      59 | 10793 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10794 | `				return rc;` |
|       - | 10795 | `			}` |
|      59 | 10796 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      28 | 10797 | `		}` |
|       - | 10798 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      59 | 10799 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10800 | `		/* Patch the short-circuit jump to land after the store. */` |
|      59 | 10801 | `		if( nJmp > 0 ){` |
|      59 | 10802 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      59 | 10803 | `			if( pInstrFix ){` |
|      59 | 10804 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10805 | `			}` |
|      28 | 10806 | `		}` |
|      59 | 10807 | `		return SXRET_OK;` |
|       - | 10808 | `	}` |
| 1324525 | 10809 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10810 | `		sxu32 nJz,nJmp;` |
|       - | 10811 | `		sxu32 nTernaryNsBase;` |
|       - | 10812 | `		/* Ternary operator require special handling */` |
|       - | 10813 | `		/* Phase#1: Compile the condition */` |
|    2665 | 10814 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2665 | 10815 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2665 | 10816 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10817 | `			return rc;` |
|       - | 10818 | `		}` |
|       - | 10819 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10820 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10821 | `		 * condition expression, not leak past the ternary. */` |
|    2665 | 10822 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2665 | 10823 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2665 | 10824 | `		if( pNode->pLeft ){` |
|       - | 10825 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10826 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2597 | 10827 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10828 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2597 | 10829 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2597 | 10830 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2597 | 10831 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10832 | `				return rc;` |
|       - | 10833 | `			}` |
|    2597 | 10834 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1301 | 10835 | `		}else{` |
|       - | 10836 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10837 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10838 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10839 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10840 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10841 | `		}` |
|       - | 10842 | `		/* Phase#4: Emit the unconditional jump */` |
|    2665 | 10843 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10844 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2665 | 10845 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2665 | 10846 | `		if( pInstr ){` |
|    2665 | 10847 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1330 | 10848 | `		}` |
|    2665 | 10849 | `		if( !pNode->pLeft ){` |
|       - | 10850 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10851 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10852 | `		}` |
|       - | 10853 | `		/* Phase#6: Compile the 'else' expression */` |
|    2665 | 10854 | `		if( pNode->pRight ){` |
|    2665 | 10855 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2665 | 10856 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2665 | 10857 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10858 | `				return rc;` |
|       - | 10859 | `			}` |
|    2665 | 10860 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1330 | 10861 | `		}` |
|    2665 | 10862 | `		if( nJmp > 0 ){` |
|       - | 10863 | `			/* Phase#7: Fix the unconditional jump */` |
|    2665 | 10864 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2665 | 10865 | `			if( pInstr ){` |
|    2665 | 10866 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1330 | 10867 | `			}` |
|    1330 | 10868 | `		}` |
|       - | 10869 | `		/* All done */` |
|    2665 | 10870 | `		return SXRET_OK;` |
|       - | 10871 | `	}` |
| 1321865 | 10872 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10873 | `	/* Generate code for the left tree */` |
| 1321865 | 10874 | `	if( pNode->pLeft ){` |
| 1321825 | 10875 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1321825 | 10876 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10877 | `			ph7_expr_node **apNode;` |
|  446107 | 10878 | `			int hasSpread = 0;` |
|  446107 | 10879 | `			int hasNamed = 0;` |
|  446107 | 10880 | `			int bAnySpread = 0;` |
|  446107 | 10881 | `			sxu32 byRefMask = 0;` |
|       - | 10882 | `			sxi32 nArgs;` |
|       - | 10883 | `			sxi32 n;` |
|       - | 10884 | `			/* Recurse and generate bytecodes for function arguments */` |
|  446107 | 10885 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  446107 | 10886 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10887 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|       - | 10888 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|       - | 10889 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  446107 | 10890 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      65 | 10891 | `				bFcc = 1;` |
|      65 | 10892 | `				nArgs = 0;` |
|      32 | 10893 | `			}` |
|       - | 10894 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10895 | `			{` |
|  446107 | 10896 | `				int seenNamed = 0;` |
|  904957 | 10897 | `				for( n = 0; n < nArgs; ++n ){` |
|  458857 | 10898 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     214 | 10899 | `						seenNamed = 1;` |
|     214 | 10900 | `						hasNamed = 1;` |
|  458752 | 10901 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|    3573 | 10902 | `						bAnySpread = 1;` |
|  456863 | 10903 | `					}else if( seenNamed ){` |
|       3 | 10904 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10905 | `							"Cannot use positional argument after named argument");` |
|       3 | 10906 | `						return SXERR_SYNTAX;` |
|       - | 10907 | `					}` |
|  229430 | 10908 | `				}` |
|       - | 10909 | `			}` |
|       - | 10910 | `			/* Read-only load */` |
|  446105 | 10911 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10912 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10913 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10914 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10915 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  446105 | 10916 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  446105 | 10917 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  446100 | 10918 | `				if( pCallName->nByte == 5` |
|  243558 | 10919 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   21595 | 10920 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  435310 | 10921 | `				}else if( pCallName->nByte == 5` |
|  221968 | 10922 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      89 | 10923 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      42 | 10924 | `				}` |
|       - | 10925 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10926 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10927 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10928 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10929 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10930 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  446105 | 10931 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10932 | `					SyString sBuiltin;` |
|  442423 | 10933 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  442423 | 10934 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  221209 | 10935 | `				}` |
|  223050 | 10936 | `			}` |
|  904953 | 10937 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  458853 | 10938 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  458853 | 10939 | `				sxi32 iArgFlags = iFlags & ~EXPR_FLAG_LOAD_IDX_STORE;` |
|       - | 10940 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 10941 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 10942 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 10943 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 10944 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 10945 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  458853 | 10946 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      53 | 10947 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      53 | 10948 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      24 | 10949 | `				}` |
|  458853 | 10950 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  458853 | 10951 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10952 | `					return rc;` |
|       - | 10953 | `				}` |
|       - | 10954 | `				/* Each argument is an independent nullsafe scope. */` |
|  458853 | 10955 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  458853 | 10956 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10957 | `					/* Emit spread opcode to unpack this array argument */` |
|    3573 | 10958 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|    3573 | 10959 | `					hasSpread = 1;` |
|    1784 | 10960 | `				}` |
|  229429 | 10961 | `			}` |
|       - | 10962 | `			/* Total number of given arguments */` |
|  446105 | 10963 | `			iP1 = nArgs;` |
|  446105 | 10964 | `			iP2 = hasSpread;` |
|       - | 10965 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10966 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  446105 | 10967 | `			if( hasNamed ){` |
|     117 | 10968 | `				sxu32 nStrBytes = 0;` |
|       - | 10969 | `				char *zBuf;` |
|     343 | 10970 | `				for( n = 0; n < nArgs; ++n ){` |
|     229 | 10971 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     211 | 10972 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|     104 | 10973 | `					}` |
|     116 | 10974 | `				}` |
|       - | 10975 | `				{` |
|     117 | 10976 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     117 | 10977 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     114 | 10978 | `					&pGen->pVm->sAllocator, mapSize);` |
|     117 | 10979 | `				if( pMap ){` |
|     117 | 10980 | `					SyZero(pMap, mapSize);` |
|     117 | 10981 | `					pMap->bHasNamed = 1;` |
|     117 | 10982 | `					pMap->nTotal = (sxu32)nArgs;` |
|     117 | 10983 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     117 | 10984 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     343 | 10985 | `					for( n = 0; n < nArgs; ++n ){` |
|     229 | 10986 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     211 | 10987 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     211 | 10988 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     211 | 10989 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     211 | 10990 | `							zBuf += nb;` |
|     104 | 10991 | `						}` |
|       - | 10992 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     116 | 10993 | `					}` |
|     117 | 10994 | `					p3 = (void *)pMap;` |
|      57 | 10995 | `				}` |
|       - | 10996 | `				}` |
|      57 | 10997 | `			}` |
|       - | 10998 | `			/* Remove stale flags now */` |
|  446105 | 10999 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  223050 | 11000 | `		}` |
|       - | 11001 | `		{` |
|       - | 11002 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|       - | 11003 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|       - | 11004 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|       - | 11005 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|       - | 11006 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|       - | 11007 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|       - | 11008 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|       - | 11009 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 1321823 | 11010 | `			sxi32 iLeftFlags = iFlags;` |
| 1321818 | 11011 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 1009716 | 11012 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  348832 | 11013 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  341026 | 11014 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   15805 | 11015 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|    7900 | 11016 | `			}` |
| 1321823 | 11017 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|       - | 11018 | `		}` |
| 1321823 | 11019 | `		if( rc != SXRET_OK ){` |
|      34 | 11020 | `			return rc;` |
|       - | 11021 | `		}` |
| 1321793 | 11022 | `		if( !bIsChainOp ){` |
|       - | 11023 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 11024 | `			 * target the end of that LHS chain, which is right here. */` |
|  607891 | 11025 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  303943 | 11026 | `		}` |
| 1321793 | 11027 | `		if( iVmOp == PH7_OP_CALL ){` |
|  446105 | 11028 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  446105 | 11029 | `			if( pInstr ){` |
|  446105 | 11030 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  441011 | 11031 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 11032 | `					sxu32 nQual;` |
|  441011 | 11033 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11034 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 11035 | `					 * so the later NEW handler (if any) can see it. */` |
|  441011 | 11036 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 11037 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 11038 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 11039 | `					 * imports — class imports must NOT affect function` |
|       - | 11040 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11041 | `					 * before NEW; we store the original literal index in the` |
|       - | 11042 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11043 | `					 * the unqualified name and re-qualify with class imports. */` |
|  441011 | 11044 | `					if( bAbsolute ){` |
|    3571 | 11045 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1788 | 11046 | `					}else{` |
|  437445 | 11047 | `						int fromImport = 0;` |
|  437445 | 11048 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  437445 | 11049 | `						pInstr->iP2 = (sxi32)nQual;` |
|  437445 | 11050 | `						if( nQual != nOrig ){` |
|       - | 11051 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11052 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11053 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11054 | `							if( !fromImport ){` |
|       - | 11055 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11056 | `								if( p3 == 0 ){` |
|      67 | 11057 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11058 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11059 | `									if( pMap ){` |
|      67 | 11060 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11061 | `										p3 = (void *)pMap;` |
|      31 | 11062 | `									}` |
|      31 | 11063 | `								}` |
|      67 | 11064 | `								if( p3 ){` |
|      67 | 11065 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11066 | `								}` |
|      31 | 11067 | `							}` |
|      36 | 11068 | `						}` |
|       5 | 11069 | `					}` |
|  225602 | 11070 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11071 | `					/* Method call,flag that */` |
|    1157 | 11072 | `					pInstr->iP2 = 1;` |
|     576 | 11073 | `				}` |
|  223055 | 11074 | `			}` |
| 1098743 | 11075 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11076 | `			ph7_expr_node **apNode;` |
|       - | 11077 | `			sxi32 n;` |
|   91195 | 11078 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11079 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11080 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       - | 11081 | `			/* Recurse and generate bytecodes for array index */` |
|   91195 | 11082 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  164571 | 11083 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   73381 | 11084 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   73381 | 11085 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   73381 | 11086 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11087 | `					return rc;` |
|       - | 11088 | `				}` |
|       - | 11089 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   73381 | 11090 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   36693 | 11091 | `			}` |
|   91195 | 11092 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   73381 | 11093 | `				iP1 = 1; /* Node have an index associated with it */` |
|   36688 | 11094 | `			}` |
|   91195 | 11095 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11096 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     243 | 11097 | `				iP2 = 4;` |
|   91076 | 11098 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11099 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11100 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      56 | 11101 | `				iP2 = 5;` |
|   90931 | 11102 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11103 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11104 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11105 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11106 | `				iP2 = 6;` |
|   90893 | 11107 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11108 | `				/* Create an empty entry when the desired index is not found */` |
|   35929 | 11109 | `				iP2 = 1;` |
|   17967 | 11110 | `			}` |
|  830098 | 11111 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11112 | `			/* POP the left node */` |
|      32 | 11113 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 11114 | `		}` |
|  660894 | 11115 | `	}` |
| 1321833 | 11116 | `	rc = SXRET_OK;` |
| 1321833 | 11117 | `	nJmpIdx = 0;` |
|       - | 11118 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 11119 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 11120 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1321833 | 11121 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     361 | 11122 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     361 | 11123 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     361 | 11124 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     361 | 11125 | `			int isSpecial = 0;` |
|     361 | 11126 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     265 | 11127 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     265 | 11128 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     260 | 11129 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     260 | 11130 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     124 | 11131 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      99 | 11132 | `					isSpecial = 1;` |
|      47 | 11133 | `				}` |
|     154 | 11134 | `			}` |
|     409 | 11135 | `			pInstr->iP1 = 0;` |
|     409 | 11136 | `			if( !isSpecial ){` |
|     219 | 11137 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     107 | 11138 | `			}` |
|       - | 11139 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 11140 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     313 | 11141 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     219 | 11142 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     219 | 11143 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      46 | 11144 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      48 | 11145 | `					return SXRET_OK;` |
|       - | 11146 | `				}` |
|      85 | 11147 | `			}` |
|     132 | 11148 | `		}` |
|     213 | 11149 | `	}` |
|       - | 11150 | `	/* Generate code for the right tree */` |
| 1321751 | 11151 | `	if( pNode->pRight ){` |
|  713609 | 11152 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 11153 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   11131 | 11154 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  708046 | 11155 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 11156 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3727 | 11157 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  700622 | 11158 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 11159 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 11160 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 11161 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  698750 | 11162 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 11163 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 11164 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 11165 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 11166 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 11167 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 11168 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     105 | 11169 | `			sxu32 nNsJmp = 0;` |
|     105 | 11170 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     105 | 11171 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  698586 | 11172 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  296887 | 11173 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  148441 | 11174 | `		}` |
|  713609 | 11175 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  713609 | 11176 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  713609 | 11177 | `		if( !bIsChainOp ){` |
|       - | 11178 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 11179 | `			 * operator instruction is emitted. */` |
|  537041 | 11180 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  268518 | 11181 | `		}` |
|  713609 | 11182 | `		if( iVmOp == PH7_OP_STORE ){` |
|  293085 | 11183 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  293054 | 11184 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 11185 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 11186 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 11187 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 11188 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 11189 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 11190 | `				 */` |
|      80 | 11191 | `				iVmOp = 0;` |
|  293047 | 11192 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  293009 | 11193 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11194 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   78475 | 11195 | `					iP2 = 1;` |
|   39240 | 11196 | `				}else{` |
|  214539 | 11197 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11198 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   35857 | 11199 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   35857 | 11200 | `						iP1 = pInstr->iP1;` |
|   17931 | 11201 | `					}else{` |
|  178687 | 11202 | `						p3 = pInstr->p3;` |
|       - | 11203 | `					}` |
|       - | 11204 | `					/* POP the last dynamic load instruction */` |
|  214539 | 11205 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 11206 | `				}` |
|  146507 | 11207 | `			}` |
|  567069 | 11208 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      54 | 11209 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      54 | 11210 | `			if( pInstr ){` |
|      54 | 11211 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11212 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 11213 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 11214 | `					 */` |
|      17 | 11215 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 11216 | `					iP1 = pInstr->iP1;` |
|      17 | 11217 | `					iP2 = pInstr->iP2;` |
|      17 | 11218 | `					p3  = pInstr->p3;` |
|       9 | 11219 | `				}else{` |
|      38 | 11220 | `					p3 = pInstr->p3;` |
|       - | 11221 | `				}` |
|      26 | 11222 | `			}` |
|      26 | 11223 | `		}` |
|  356802 | 11224 | `	}` |
| 1321746 | 11225 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   11484 | 11226 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 11227 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 11228 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      29 | 11229 | `		iVmOp = 0;` |
|      13 | 11230 | `	}` |
| 1321751 | 11231 | `	if( iVmOp > 0 ){` |
| 1321495 | 11232 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   14573 | 11233 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 11234 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   10657 | 11235 | `				iP1 = 1;` |
|    5331 | 11236 | `			}` |
| 1314211 | 11237 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 11238 | `			/* Namespace-qualify the class name for NEW */ {` |
|   22761 | 11239 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   22761 | 11240 | `				VmInstr *pCallInstr = 0;` |
|   22761 | 11241 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   22611 | 11242 | `					pCallInstr = pPeek;` |
|   22611 | 11243 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   11303 | 11244 | `				}` |
|   22761 | 11245 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   22759 | 11246 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11247 | `					sxu32 nLitForClass;` |
|       - | 11248 | `					/* If the CALL handler already qualified the name using` |
|       - | 11249 | `					 * function imports, recover the original unqualified` |
|       - | 11250 | `					 * literal so we can re-qualify with class imports. */` |
|   22759 | 11251 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 11252 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 11253 | `					}else{` |
|   22727 | 11254 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 11255 | `					}` |
|   22759 | 11256 | `					pPeek->iP1 = 0;` |
|   22759 | 11257 | `					if( !bAbsolute ){` |
|   19197 | 11258 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    9601 | 11259 | `					}else{` |
|    3567 | 11260 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 11261 | `					}` |
|   11377 | 11262 | `				}` |
|       - | 11263 | `			}` |
|   22761 | 11264 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   22761 | 11265 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 11266 | `				VmInstr *pPrev;` |
|   22611 | 11267 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   22611 | 11268 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 11269 | `					/* Pop the call instruction, preserve named-arg map */` |
|   22611 | 11270 | `					iP1 = pInstr->iP1;` |
|   22611 | 11271 | `					if( pInstr->p3 ){` |
|      43 | 11272 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 11273 | `					}` |
|   22611 | 11274 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   11303 | 11275 | `				}` |
|   11308 | 11276 | `			}` |
| 1295549 | 11277 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 11278 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 11279 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     201 | 11280 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     201 | 11281 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     201 | 11282 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     201 | 11283 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     201 | 11284 | `				int isSpecialIs = 0;` |
|     201 | 11285 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     197 | 11286 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     197 | 11287 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     192 | 11288 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     197 | 11289 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      97 | 11290 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 11291 | `						isSpecialIs = 1;` |
|       5 | 11292 | `					}` |
|      97 | 11293 | `				}` |
|     203 | 11294 | `				pInstr->iP1 = 0;` |
|     203 | 11295 | `				if( !isSpecialIs && !bAbsolute ){` |
|     181 | 11296 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      88 | 11297 | `				}` |
|     102 | 11298 | `			}` |
| 1284076 | 11299 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 11300 | `			/* Prevent constant expansion for member/property names.` |
|       - | 11301 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 11302 | `			 * should not trigger constant lookup. */` |
|  176573 | 11303 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  176573 | 11304 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  176527 | 11305 | `				pInstr->iP1 = 0;` |
|   88261 | 11306 | `			}` |
|  176573 | 11307 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 11308 | `				/* Static member access,remember that */` |
|     279 | 11309 | `				iP1 = 1;` |
|     279 | 11310 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     279 | 11311 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      40 | 11312 | `					p3 = pInstr->p3;` |
|      40 | 11313 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      18 | 11314 | `				}` |
|     137 | 11315 | `			}` |
|       - | 11316 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|       - | 11317 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|       - | 11318 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|       - | 11319 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  176573 | 11320 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  176573 | 11321 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|      24 | 11322 | `					iP2 = PH7_MEMBER_UNSET;` |
|  176562 | 11323 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      75 | 11324 | `					iP2 = PH7_MEMBER_ISSET;` |
|  176516 | 11325 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|      11 | 11326 | `					iP2 = PH7_MEMBER_EMPTY;` |
|       5 | 11327 | `				}` |
|   88284 | 11328 | `			}` |
|   88284 | 11329 | `		}` |
|       - | 11330 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|       - | 11331 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|       - | 11332 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|       - | 11333 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|       - | 11334 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 1321493 | 11335 | `		if( bFcc ){` |
|      65 | 11336 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|      65 | 11337 | `			iP2 = 0;` |
|      65 | 11338 | `			p3 = 0;` |
|      65 | 11339 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11340 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11341 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|       - | 11342 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|       - | 11343 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|       - | 11344 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      31 | 11345 | `				void *pMemberName = pInstr->p3;` |
|      31 | 11346 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      31 | 11347 | `				if( pMemberName ){` |
|       3 | 11348 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       1 | 11349 | `				}` |
|      31 | 11350 | `				iP1 = 2;` |
|      16 | 11351 | `			}else{` |
|      35 | 11352 | `				iP1 = 1;` |
|       - | 11353 | `			}` |
|      32 | 11354 | `		}` |
|       - | 11355 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 11356 | `		 * This is the primary emit path for user-visible calls. */` |
| 1321493 | 11357 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  468797 | 11358 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  234396 | 11359 | `		}` |
|       - | 11360 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1321493 | 11361 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  660744 | 11362 | `	}` |
| 1321749 | 11363 | `	if( nJmpIdx > 0 ){` |
|       - | 11364 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   14977 | 11365 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   14977 | 11366 | `		if( pInstr ){` |
|   14977 | 11367 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    7486 | 11368 | `		}` |
|    7486 | 11369 | `	}` |
| 1321749 | 11370 | `	return rc;` |
| 1762505 | 11371 |  |
|       - | 11372 | `/*` |
|       - | 11373 | ` * Compile a PHP expression.` |
|       - | 11374 | ` * According to the PHP language reference manual:` |
|       - | 11375 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 11376 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 11377 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 11378 | ` *  is "anything that has a value".` |
|       - | 11379 | ` * If something goes wrong while compiling the expression,this` |
|       - | 11380 | ` * function takes care of generating the appropriate error` |
|       - | 11381 | ` * message.` |
|       - | 11382 | ` */` |
|  949522 | 11383 | `static sxi32 PH7_CompileExpr(` |
|       - | 11384 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11385 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 11386 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 11387 | `	)` |
|       5 | 11388 |  |
|       - | 11389 | `	ph7_expr_node *pRoot;` |
|       - | 11390 | `	SySet sExprNode;` |
|       - | 11391 | `	SyToken *pEnd;` |
|       - | 11392 | `	sxi32 nExpr;` |
|       - | 11393 | `	sxi32 iNest;` |
|       - | 11394 | `	sxi32 rc;` |
|       - | 11395 | `	sxu32 nNullsafeBase;` |
|       - | 11396 | `	/* Initialize worker variables */` |
|  949527 | 11397 | `	nExpr = 0;` |
|  949527 | 11398 | `	pRoot = 0;` |
|       - | 11399 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 11400 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  949527 | 11401 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  949527 | 11402 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  949527 | 11403 | `	SySetAlloc(&sExprNode,0x10);` |
|  949527 | 11404 | `	rc = SXRET_OK;` |
|       - | 11405 | `	/* Delimit the expression */` |
|  949527 | 11406 | `	pEnd = pGen->pIn;` |
|  949527 | 11407 | `	iNest = 0;` |
| 6405065 | 11408 | `	while( pEnd < pGen->pEnd ){` |
| 6077927 | 11409 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11410 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     515 | 11411 | `			iNest++;` |
| 6077672 | 11412 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     523 | 11413 | `			iNest--;` |
| 6077158 | 11414 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  622759 | 11415 | `			if( iNest <= 0 ){` |
|  622389 | 11416 | `				break;` |
|       - | 11417 | `			}` |
|     185 | 11418 | `		}` |
| 5455543 | 11419 | `		pEnd++;` |
|       5 | 11420 | `	}` |
|  949527 | 11421 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   21837 | 11422 | `		SyToken *pEnd2 = pGen->pIn;` |
|   21837 | 11423 | `		iNest = 0;` |
|       - | 11424 | `		/* Stop at the first comma */` |
|   43963 | 11425 | `		while( pEnd2 < pEnd ){` |
|   22137 | 11426 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      67 | 11427 | `				iNest++;` |
|   22106 | 11428 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      67 | 11429 | `				iNest--;` |
|   22044 | 11430 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 11431 | `				if( iNest <= 0 ){` |
|       7 | 11432 | `					break;` |
|       - | 11433 | `				}` |
|      23 | 11434 | `			}` |
|   22131 | 11435 | `			pEnd2++;` |
|       5 | 11436 | `		}` |
|   21837 | 11437 | `		if( pEnd2 <pEnd ){` |
|       7 | 11438 | `			pEnd = pEnd2;` |
|       3 | 11439 | `		}` |
|   10916 | 11440 | `	}` |
|  949527 | 11441 | `	if( pEnd > pGen->pIn ){` |
|  949517 | 11442 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 11443 | `		/* Swap delimiter */` |
|  949517 | 11444 | `		pGen->pEnd = pEnd;` |
|       - | 11445 | `		/* Try to get an expression tree */` |
|  949517 | 11446 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  949517 | 11447 | `		if( rc == SXRET_OK && pRoot ){` |
|  949335 | 11448 | `			rc = SXRET_OK;` |
|  949335 | 11449 | `			if( xTreeValidator ){` |
|       - | 11450 | `				/* Call the upper layer validator callback */` |
|   29215 | 11451 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   14605 | 11452 | `			}` |
|  949335 | 11453 | `			if( rc != SXERR_ABORT ){` |
|       - | 11454 | `				/* Generate code for the given tree */` |
|  949335 | 11455 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 11456 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 11457 | `				 * expression so they short-circuit to its end. */` |
|  949335 | 11458 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  474665 | 11459 | `			}` |
|  949335 | 11460 | `			nExpr = 1;` |
|  474665 | 11461 | `		}` |
|       - | 11462 | `		/* Release the whole tree */` |
|  949517 | 11463 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 11464 | `		/* Synchronize token stream */` |
|  949517 | 11465 | `		pGen->pEnd = pTmp;` |
|  949517 | 11466 | `		pGen->pIn  = pEnd;` |
|  949517 | 11467 | `		if( rc == SXERR_ABORT ){` |
|      12 | 11468 | `			SySetRelease(&sExprNode);` |
|      12 | 11469 | `			return SXERR_ABORT;` |
|       - | 11470 | `		}` |
|  474751 | 11471 | `	}` |
|  949517 | 11472 | `	SySetRelease(&sExprNode);` |
|  949517 | 11473 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  474766 | 11474 |  |
|       - | 11475 | `/*` |
|       - | 11476 | ` * Return a pointer to the node construct handler associated` |
|       - | 11477 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 11478 | ` */` |
|  248648 | 11479 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 11480 |  |
|  248653 | 11481 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 11482 | `		/* Numeric literal: Either real or integer */` |
|  125135 | 11483 | `		return PH7_CompileNumLiteral;` |
|  123523 | 11484 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 11485 | `		/* Double quoted string */` |
|   23447 | 11486 | `		return PH7_CompileString;` |
|  100081 | 11487 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 11488 | `		/* Single quoted string */` |
|   99965 | 11489 | `		return PH7_CompileSimpleString;` |
|     120 | 11490 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 11491 | `		/* Heredoc */` |
|      68 | 11492 | `		return PH7_CompileHereDoc;` |
|      56 | 11493 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 11494 | `		/* Nowdoc */` |
|      50 | 11495 | `		return PH7_CompileNowDoc;` |
|       8 | 11496 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 11497 | `		/* Backtick quoted string */` |
|       6 | 11498 | `		return PH7_CompileBacktic;` |
|       - | 11499 | `	}` |
|       3 | 11500 | `	return 0;` |
|  124329 | 11501 |  |
|       - | 11502 | `/*` |
|       - | 11503 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 11504 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 11505 | ` * in write context" parse error.` |
|       - | 11506 | ` */` |
|    6862 | 11507 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 11508 |  |
|       - | 11509 | `	sxi32 rc;` |
|    6867 | 11510 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6865 | 11511 | `		return SXRET_OK;` |
|       - | 11512 | `	}` |
|       5 | 11513 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 11514 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 11515 | `		"Can't use nullsafe operator in write context");` |
|       3 | 11516 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3436 | 11517 |  |
|       - | 11518 | `/*` |
|       - | 11519 | ` * Compile an unset() statement.` |
|       - | 11520 | ` * unset($var, $arr[$key], ...);` |
|       - | 11521 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 11522 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 11523 | ` * parent array before extracting the element to unset.` |
|       - | 11524 | ` */` |
|    2976 | 11525 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 11526 |  |
|    2981 | 11527 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2981 | 11528 | `	sxu32 nIdx = 0;` |
|       - | 11529 | `	SyString sName;` |
|       - | 11530 | `	sxi32 rc;` |
|       - | 11531 | `	/* Jump the 'unset' keyword */` |
|    2981 | 11532 | `	pGen->pIn++;` |
|       - | 11533 | `	/* Save delimiter */` |
|    2981 | 11534 | `	pTmp = pGen->pEnd;` |
|       - | 11535 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2981 | 11536 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2981 | 11537 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 11538 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 11539 | `		SyToken *pClose;` |
|    2981 | 11540 | `		pGen->pIn++;   /* Skip '(' */` |
|    2981 | 11541 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2981 | 11542 | `		pEnd = pClose; /* Stop at ')' */` |
|    1488 | 11543 | `	}` |
|    2981 | 11544 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 11545 | `	/* Resolve the 'unset' builtin name once */` |
|    2981 | 11546 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     365 | 11547 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     365 | 11548 | `		if( pObj == 0 ){` |
|     ! 0 | 11549 | `			return SXERR_ABORT;` |
|       - | 11550 | `		}` |
|     365 | 11551 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     365 | 11552 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     180 | 11553 | `	}` |
|       - | 11554 | `	/* Compile each comma-separated argument */` |
|    9845 | 11555 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6869 | 11556 | `		if( pGen->pIn < pNext ){` |
|    6869 | 11557 | `			pGen->pEnd = pNext;` |
|    6869 | 11558 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 11559 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 11560 | `				GenStateUnsetValidator);` |
|    6869 | 11561 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11562 | `				return SXERR_ABORT;` |
|       - | 11563 | `			}` |
|    6869 | 11564 | `			if( rc != SXERR_EMPTY ){` |
|       - | 11565 | `				/* Emit call for this single argument */` |
|    6867 | 11566 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6867 | 11567 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6867 | 11568 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3431 | 11569 | `			}` |
|    3432 | 11570 | `		}` |
|       - | 11571 | `		/* Jump trailing commas */` |
|   10759 | 11572 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3895 | 11573 | `			pNext++;` |
|       5 | 11574 | `		}` |
|    6869 | 11575 | `		pGen->pIn = pNext;` |
|       5 | 11576 | `	}` |
|       - | 11577 | `	/* Skip past the closing ')' if present */` |
|    2981 | 11578 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2981 | 11579 | `		pGen->pIn++;` |
|    1488 | 11580 | `	}` |
|       - | 11581 | `	/* Restore token stream */` |
|    2981 | 11582 | `	pGen->pEnd = pTmp;` |
|    2981 | 11583 | `	return SXRET_OK;` |
|    1493 | 11584 |  |
|       - | 11585 | `/*` |
|       - | 11586 | ` * PHP Language construct table.` |
|       - | 11587 | ` */` |
|       - | 11588 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 11589 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 11590 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 11591 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 11592 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 11593 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 11594 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 11595 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 11596 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 11597 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 11598 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 11599 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 11600 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 11601 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 11602 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 11603 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 11604 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 11605 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 11606 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 11607 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 11608 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 11609 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 11610 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 11611 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 11612 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 11613 | `};` |
|       - | 11614 | `/*` |
|       - | 11615 | ` * Return a pointer to the statement handler routine associated` |
|       - | 11616 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 11617 | ` */` |
|  636792 | 11618 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 11619 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 11620 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 11621 | `	)` |
|       5 | 11622 |  |
|  636797 | 11623 | `	sxu32 n = 0;` |
| 3301047 | 11624 | `	for(;;){` |
| 6602099 | 11625 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  136261 | 11626 | `			break;` |
|       - | 11627 | `		}` |
| 6465843 | 11628 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  500541 | 11629 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 11630 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 11631 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 11632 | `					/* 'static' (class context),return null */` |
|     ! 0 | 11633 | `					return 0;` |
|       - | 11634 | `				}` |
|     ! 0 | 11635 | `			}` |
|  500536 | 11636 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       6 | 11637 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 11638 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 11639 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 11640 | `				return 0;` |
|       - | 11641 | `			}` |
|       - | 11642 | `			/* Return a pointer to the handler.` |
|       - | 11643 | `			*/` |
|  500541 | 11644 | `			return aLangConstruct[n].xConstruct;` |
|       - | 11645 | `		}` |
| 5965307 | 11646 | `		n++;` |
|       5 | 11647 | `	}` |
|  136261 | 11648 | `	if( pLookahed ){` |
|  136261 | 11649 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   39067 | 11650 | `			return PH7_CompileClassInterface;` |
|   97199 | 11651 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   96855 | 11652 | `			return PH7_CompileClass;` |
|     349 | 11653 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      65 | 11654 | `			return PH7_CompileTrait;` |
|       - | 11655 | `		}` |
|       - | 11656 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 11657 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 11658 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 11659 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     142 | 11660 | `	}` |
|       - | 11661 | `	/* Not a language construct */` |
|     289 | 11662 | `	return 0;` |
|  318401 | 11663 |  |
|       - | 11664 | `/*` |
|       - | 11665 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 11666 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 11667 | ` */` |
|     284 | 11668 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 11669 |  |
|       - | 11670 | `	int rc;` |
|     289 | 11671 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     289 | 11672 | `	if( rc == FALSE ){` |
|     174 | 11673 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     173 | 11674 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 11675 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 11676 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 11677 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 11678 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 11679 | `			*/` |
|       - | 11680 | `			){` |
|     171 | 11681 | `				rc = TRUE;` |
|      83 | 11682 | `		}` |
|      87 | 11683 | `	}` |
|     289 | 11684 | `	return rc;` |
|       5 | 11685 |  |
|       - | 11686 | `/*` |
|       - | 11687 | ` * Compile a PHP chunk.` |
|       - | 11688 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11689 | ` * takes care of generating the appropriate error message.` |
|       - | 11690 | ` */` |
|  761690 | 11691 | `static sxi32 GenStateCompileChunk(` |
|       - | 11692 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11693 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 11694 | `	)` |
|       5 | 11695 |  |
|       - | 11696 | `	ProcLangConstruct xCons;` |
|       - | 11697 | `	sxi32 rc;` |
|  761695 | 11698 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  595735 | 11699 | `	for(;;){` |
|  976585 | 11700 | `		int bStmtIsDeclare = 0;` |
|  976585 | 11701 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11702 | `			/* No more input to process */` |
|   14251 | 11703 | `			break;` |
|       - | 11704 | `		}` |
|       - | 11705 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 11706 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  962339 | 11707 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  640371 | 11708 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  640371 | 11709 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 11710 | `				bStmtIsDeclare = 1;` |
|      20 | 11711 | `			}` |
|  320183 | 11712 | `		}` |
|  962339 | 11713 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 11714 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 11715 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  214865 | 11716 | `			pGen->bStrictTypesLocked = 1;` |
|  107430 | 11717 | `		}` |
|  962339 | 11718 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11719 | `			/* Compile block */` |
|      21 | 11720 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      21 | 11721 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11722 | `				break;` |
|       - | 11723 | `			}` |
|      13 | 11724 | `		}else{` |
|  962323 | 11725 | `			xCons = 0;` |
|  962323 | 11726 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 11727 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 11728 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 11729 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3605 | 11730 | `				xCons = PH7_CompileClassModifiers;` |
|  960523 | 11731 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  636797 | 11732 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 11733 | `				/* Try to extract a language construct handler */` |
|  636797 | 11734 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  636797 | 11735 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 11736 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 11737 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 11738 | `						&pGen->pIn->sData);` |
|       9 | 11739 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11740 | `						break;` |
|       - | 11741 | `					}` |
|       - | 11742 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 11743 | `					 * this erroneous statement.` |
|       - | 11744 | `					 */` |
|       9 | 11745 | `					xCons = PH7_ErrorRecover;` |
|       4 | 11746 | `				}` |
|  640327 | 11747 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   52739 | 11748 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 11749 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 11750 | `				xCons = PH7_CompileLabel;` |
|      56 | 11751 | `			}` |
|  962323 | 11752 | `			if( xCons == 0 ){` |
|       - | 11753 | `				/* Assume an expression an try to compile it */` |
|  322095 | 11754 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  322095 | 11755 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 11756 | `					/* Pop l-value */` |
|  321945 | 11757 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  160970 | 11758 | `				}` |
|  161050 | 11759 | `			}else{` |
|       - | 11760 | `				/* Go compile the sucker */` |
|  640233 | 11761 | `				rc = xCons(&(*pGen));` |
|       - | 11762 | `			}` |
|  962323 | 11763 | `			if( rc == SXERR_ABORT ){` |
|       - | 11764 | `				/* Request to abort compilation */` |
|      12 | 11765 | `				break;` |
|       - | 11766 | `			}` |
|       - | 11767 | `		}` |
|       - | 11768 | `		/* Ignore trailing semi-colons ';' */` |
| 1556039 | 11769 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  593715 | 11770 | `			pGen->pIn++;` |
|       5 | 11771 | `		}` |
|  962329 | 11772 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 11773 | `			/* Compile a single statement and return */` |
|  747439 | 11774 | `			break;` |
|       - | 11775 | `		}` |
|       - | 11776 | `		/* LOOP ONE */` |
|       - | 11777 | `		/* LOOP TWO */` |
|       - | 11778 | `		/* LOOP THREE */` |
|       - | 11779 | `		/* LOOP FOUR */` |
|       5 | 11780 | `	}` |
|       - | 11781 | `	/* Return compilation status */` |
|  761695 | 11782 | `	return rc;` |
|       5 | 11783 |  |
|       - | 11784 | `/*` |
|       - | 11785 | ` * Compile a Raw PHP chunk.` |
|       - | 11786 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11787 | ` * takes care of generating the appropriate error message.` |
|       - | 11788 | ` */` |
|   14258 | 11789 | `static sxi32 PH7_CompilePHP(` |
|       - | 11790 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11791 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 11792 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 11793 | `	)` |
|       5 | 11794 |  |
|   14263 | 11795 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 11796 | `	sxi32 rc;` |
|       - | 11797 | `	/* Reset the token set */` |
|   14263 | 11798 | `	SySetReset(&(*pTokenSet));` |
|       - | 11799 | `	/* Mark as the default token set */` |
|   14263 | 11800 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 11801 | `	/* Advance the stream cursor */` |
|   14263 | 11802 | `	pGen->pRawIn++;` |
|       - | 11803 | `	/* Tokenize the PHP chunk first */` |
|   14263 | 11804 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 11805 | `	/* Point to the head and tail of the token stream. */` |
|   14263 | 11806 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   14263 | 11807 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   14263 | 11808 | `	if( is_expr ){` |
|     ! 0 | 11809 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 11810 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 11811 | `			/* A simple expression,compile it */` |
|     ! 0 | 11812 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 11813 | `		}` |
|       - | 11814 | `		/* Emit the DONE instruction */` |
|     ! 0 | 11815 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 11816 | `		return SXRET_OK;` |
|       - | 11817 | `	}` |
|   14263 | 11818 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 11819 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 11820 | `		/*` |
|       - | 11821 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 11822 | `		 * According to the PHP reference manual:` |
|       - | 11823 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 11824 | `		 *  immediately follow` |
|       - | 11825 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 11826 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 11827 | `		 * Symisc extension:` |
|       - | 11828 | `		 *   This short syntax works with all PHP opening` |
|       - | 11829 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 11830 | `		 *   only short tag.` |
|       - | 11831 | `		 */` |
|       - | 11832 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 11833 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 11834 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11835 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11836 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11837 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11838 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11839 | `		}` |
|       3 | 11840 | `		return SXRET_OK;` |
|       - | 11841 | `	}` |
|       - | 11842 | `	/* Compile the PHP chunk */` |
|   14261 | 11843 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11844 | `	/* Fix exceptions jumps */` |
|   14261 | 11845 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11846 | `	/* Fix gotos now, the jump destination is resolved */` |
|   14261 | 11847 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11848 | `		rc = SXERR_ABORT;` |
|       1 | 11849 | `	}` |
|       - | 11850 | `	/* Reset container */` |
|   14261 | 11851 | `	SySetReset(&pGen->aGoto);` |
|   14261 | 11852 | `	SySetReset(&pGen->aLabel);` |
|   14261 | 11853 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11854 | `	/* Compilation result */` |
|   14261 | 11855 | `	return rc;` |
|    7134 | 11856 |  |
|       - | 11857 | `/*` |
|       - | 11858 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11859 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11860 | ` * This is the only compile interface exported from this file.` |
|       - | 11861 | ` */` |
|   17216 | 11862 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11863 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11864 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11865 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11866 | `	)` |
|       5 | 11867 |  |
|       - | 11868 | `	SySet aPhpToken,aRawToken;` |
|       - | 11869 | `	ph7_gen_state *pCodeGen;` |
|       - | 11870 | `	ph7_value *pRawObj;` |
|       - | 11871 | `	sxu32 nObjIdx;` |
|       - | 11872 | `	sxi32 nRawObj;` |
|       - | 11873 | `	int is_expr;` |
|       - | 11874 | `	sxi8 bSavedStrict;` |
|       - | 11875 | `	sxi8 bSavedStrictLocked;` |
|       - | 11876 | `	sxi32 rc;` |
|   17221 | 11877 | `	if( pScript->nByte < 1 ){` |
|       - | 11878 | `		/* Nothing to compile */` |
|     ! 0 | 11879 | `		return PH7_OK;` |
|       - | 11880 | `	}` |
|       - | 11881 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11882 | `	 * file's flags so include/require restore them on return. */` |
|   17221 | 11883 | `	pCodeGen = &pVm->sCodeGen;` |
|   17221 | 11884 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   17221 | 11885 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   17221 | 11886 | `	pCodeGen->bStrictTypes = 0;` |
|   17221 | 11887 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11888 | `	/* Initialize the tokens containers */` |
|   17221 | 11889 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17221 | 11890 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17221 | 11891 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   17221 | 11892 | `	is_expr = 0;` |
|   17221 | 11893 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11894 | `		SyToken sTmp;` |
|       - | 11895 | `		/* PHP only: -*/` |
|    3617 | 11896 | `		sTmp.nLine = 1;` |
|    3617 | 11897 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3617 | 11898 | `		sTmp.pUserData = 0;` |
|    3617 | 11899 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3617 | 11900 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3617 | 11901 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11902 | `			/* A simple PHP expression */` |
|     ! 0 | 11903 | `			is_expr = 1;` |
|     ! 0 | 11904 | `		}` |
|    1811 | 11905 | `	}else{` |
|       - | 11906 | `		/* Tokenize raw text */` |
|   13609 | 11907 | `		SySetAlloc(&aRawToken,32);` |
|   13609 | 11908 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11909 | `	}` |
|       - | 11910 | `	/* Process high-level tokens */` |
|   17221 | 11911 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   17221 | 11912 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   17221 | 11913 | `	rc = PH7_OK;` |
|   17221 | 11914 | `	if( is_expr ){` |
|       - | 11915 | `		/* Compile the expression */` |
|     ! 0 | 11916 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 11917 | `		goto cleanup;` |
|       - | 11918 | `	}` |
|   17221 | 11919 | `	nObjIdx = 0;` |
|       - | 11920 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 11921 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 11922 | `	 * preventing namespace bleeding across include()d files. */` |
|   17221 | 11923 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 11924 | `	/* Start the compilation process */` |
|   15416 | 11925 | `	for(;;){` |
|   45083 | 11926 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   17209 | 11927 | `			break; /* No more tokens to process */` |
|       - | 11928 | `		}` |
|   27879 | 11929 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 11930 | `			/* Compile the PHP chunk */` |
|   14263 | 11931 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   14263 | 11932 | `			if( rc == SXERR_ABORT ){` |
|      15 | 11933 | `				break;` |
|       - | 11934 | `			}` |
|   14251 | 11935 | `			continue;` |
|       - | 11936 | `		}` |
|       - | 11937 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13621 | 11938 | `		nRawObj = 0;` |
|   27279 | 11939 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 11940 | `			/* Consume the raw chunk without any processing */` |
|   13663 | 11941 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13663 | 11942 | `			if( pRawObj == 0 ){` |
|     ! 0 | 11943 | `				rc = SXERR_MEM;` |
|     ! 0 | 11944 | `				break;` |
|       - | 11945 | `			}` |
|       - | 11946 | `			/* Mark as constant and emit the load constant instruction */` |
|   13663 | 11947 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13663 | 11948 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13663 | 11949 | `			++nRawObj;` |
|   13663 | 11950 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 11951 | `		}` |
|   13621 | 11952 | `		if( nRawObj > 0 ){` |
|       - | 11953 | `			/* Emit the consume instruction */` |
|   13621 | 11954 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6808 | 11955 | `		}` |
|    8613 | 11956 | `	}` |
|    8608 | 11957 | `cleanup:` |
|   17221 | 11958 | `	SySetRelease(&aRawToken);` |
|   17221 | 11959 | `	SySetRelease(&aPhpToken);` |
|       - | 11960 | `	/* Restore outer file's strict_types scope */` |
|   17221 | 11961 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   17221 | 11962 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   17221 | 11963 | `	return rc;` |
|    8613 | 11964 |  |
|       - | 11965 | `/*` |
|       - | 11966 | ` * Utility routines.Initialize the code generator.` |
|       - | 11967 | ` */` |
|    3544 | 11968 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 11969 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11970 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11971 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11972 | `	)` |
|       5 | 11973 |  |
|    3549 | 11974 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11975 | `	/* Zero the structure */` |
|    3549 | 11976 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 11977 | `	/* Initial state */` |
|    3549 | 11978 | `	pGen->pVm  = &(*pVm);` |
|    3549 | 11979 | `	pGen->xErr = xErr;` |
|    3549 | 11980 | `	pGen->pErrData = pErrData;` |
|    3549 | 11981 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3549 | 11982 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3549 | 11983 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3549 | 11984 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3549 | 11985 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 11986 | `	/* Error log buffer */` |
|    3549 | 11987 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 11988 | `	/* General purpose working buffer */` |
|    3549 | 11989 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 11990 | `	/* Namespace state */` |
|    3549 | 11991 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3549 | 11992 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3549 | 11993 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3549 | 11994 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11995 | `	/* Create the global scope */` |
|    3549 | 11996 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 11997 | `	/* Point to the global scope */` |
|    3549 | 11998 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3549 | 11999 | `	return SXRET_OK;` |
|       5 | 12000 |  |
|       - | 12001 | `/*` |
|       - | 12002 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 12003 | ` */` |
|   20414 | 12004 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 12005 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12006 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12007 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12008 | `	)` |
|       5 | 12009 |  |
|   20419 | 12010 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12011 | `	GenBlock *pBlock,*pParent;` |
|       - | 12012 | `	/* Reset state */` |
|   20419 | 12013 | `	SySetReset(&pGen->aLabel);` |
|   20419 | 12014 | `	SySetReset(&pGen->aGoto);` |
|   20419 | 12015 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   20419 | 12016 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   20419 | 12017 | `	SyBlobRelease(&pGen->sWorker);` |
|   20419 | 12018 | `	SyBlobRelease(&pGen->sNamespace);` |
|   20419 | 12019 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   20419 | 12020 | `	SyHashRelease(&pGen->hUseImports);` |
|   20419 | 12021 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   20419 | 12022 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   20419 | 12023 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   20419 | 12024 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   20419 | 12025 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12026 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 12027 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 12028 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 12029 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 12030 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 12031 | `	 * number of unique names, which is acceptable. */` |
|       - | 12032 | `	/* Point to the global scope */` |
|   20419 | 12033 | `	pBlock = pGen->pCurrent;` |
|   20419 | 12034 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 12035 | `		pParent = pBlock->pParent;` |
|     ! 0 | 12036 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 12037 | `		pBlock = pParent;` |
|     ! 0 | 12038 | `	}` |
|   20419 | 12039 | `	pGen->xErr = xErr;` |
|   20419 | 12040 | `	pGen->pErrData = pErrData;` |
|   20419 | 12041 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   20419 | 12042 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   20419 | 12043 | `	pGen->pIn = pGen->pEnd = 0;` |
|   20419 | 12044 | `	pGen->nErr = 0;` |
|   20419 | 12045 | `	return SXRET_OK;` |
|       5 | 12046 |  |
|       - | 12047 | `/*` |
|       - | 12048 | ` * Generate a compile-time error message.` |
|       - | 12049 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 12050 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 12051 | ` * abort compilation immediately.` |
|       - | 12052 | ` */` |
|     610 | 12053 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 12054 |  |
|     615 | 12055 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     615 | 12056 | `	const char *zErr = "Error";` |
|       - | 12057 | `	SyString *pFile;` |
|       - | 12058 | `	va_list ap;` |
|       - | 12059 | `	sxi32 rc;` |
|       - | 12060 | `	/* Reset the working buffer */` |
|     615 | 12061 | `	SyBlobReset(pWorker);` |
|       - | 12062 | `	/* Peek the processed file path if available */` |
|     615 | 12063 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     615 | 12064 | `	if( nErrType == E_ERROR ){` |
|       - | 12065 | `		/* Increment the error counter */` |
|     507 | 12066 | `		pGen->nErr++;` |
|     507 | 12067 | `		if( pGen->nErr > 15 ){` |
|       - | 12068 | `			/* Error count limit reached */` |
|       6 | 12069 | `			if( pGen->xErr ){` |
|       6 | 12070 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       6 | 12071 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       6 | 12072 | `				if( pFile ){` |
|       6 | 12073 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 12074 | `				}` |
|       6 | 12075 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       6 | 12076 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       6 | 12077 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12078 | `				}` |
|       2 | 12079 | `			}` |
|       - | 12080 | `			/* Abort immediately */` |
|       6 | 12081 | `			return SXERR_ABORT;` |
|       - | 12082 | `		}` |
|     249 | 12083 | `	}` |
|     611 | 12084 | `	if( pGen->xErr == 0 ){` |
|       - | 12085 | `		/* No available error consumer,return immediately */` |
|       3 | 12086 | `		return SXRET_OK;` |
|       - | 12087 | `	}` |
|     608 | 12088 | `	switch(nErrType){` |
|     500 | 12089 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 12090 | `	case E_WARNING: zErr = "Warning";     break;` |
|      78 | 12091 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      11 | 12092 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12093 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12094 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12095 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12096 | `	default:` |
|     ! 0 | 12097 | `		break;` |
|       - | 12098 | `	}` |
|     608 | 12099 | `	rc = SXRET_OK;` |
|       - | 12100 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     608 | 12101 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     608 | 12102 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     608 | 12103 | `	va_start(ap,zFormat);` |
|     608 | 12104 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     608 | 12105 | `	va_end(ap);` |
|     608 | 12106 | `	if( pFile ){` |
|     608 | 12107 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     302 | 12108 | `	}` |
|       - | 12109 | `	/* Append a new line */` |
|     608 | 12110 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     608 | 12111 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 12112 | `		/* Consume the generated error message */` |
|     608 | 12113 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     302 | 12114 | `	}` |
|     608 | 12115 | `	return rc;` |
|     310 | 12116 |  |
|       - | 12117 |  |
