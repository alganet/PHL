# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5399/6741 lines (80.09%)

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
|  780412 |   155 | `static void GenStateInitBlock(` |
|       - |   156 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   157 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   158 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   159 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   160 | `	void *pUserData      /* Upper layer private data */` |
|       - |   161 | `	)` |
|       5 |   162 |  |
|       - |   163 | `	/* Initialize block fields */` |
|  780417 |   164 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  780417 |   165 | `	pBlock->pUserData   = pUserData;` |
|  780417 |   166 | `	pBlock->pGen        = pGen;` |
|  780417 |   167 | `	pBlock->iFlags      = iType;` |
|  780417 |   168 | `	pBlock->pParent     = 0;` |
|  780417 |   169 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  780417 |   170 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  780417 |   171 |  |
|       - |   172 | `/*` |
|       - |   173 | ` * Allocate a new block instance.` |
|       - |   174 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   175 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   176 | ` * processing on failure.` |
|       - |   177 | ` */` |
|  777106 |   178 | `static sxi32 GenStateEnterBlock(` |
|       - |   179 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   180 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   181 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   182 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   183 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   184 | `	)` |
|       5 |   185 |  |
|       - |   186 | `	GenBlock *pBlock;` |
|       - |   187 | `	/* Allocate a new block instance */` |
|  777111 |   188 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  777111 |   189 | `	if( pBlock == 0 ){` |
|       - |   190 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   191 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   192 | `		 */` |
|     ! 0 |   193 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   194 | `		/* Abort processing immediately */` |
|     ! 0 |   195 | `		return SXERR_ABORT;` |
|       - |   196 | `	}` |
|       - |   197 | `	/* Zero the structure */` |
|  777111 |   198 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  777111 |   199 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   200 | `	/* Link to the parent block */` |
|  777111 |   201 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   202 | `	/* Mark as the current block */` |
|  777111 |   203 | `	pGen->pCurrent = pBlock;` |
|  777111 |   204 | `	if( ppBlock ){` |
|       - |   205 | `		/* Write a pointer to the new instance */` |
|  377459 |   206 | `		*ppBlock = pBlock;` |
|  188727 |   207 | `	}` |
|  777111 |   208 | `	return SXRET_OK;` |
|  388558 |   209 |  |
|       - |   210 | `/*` |
|       - |   211 | ` * Release block fields without freeing the whole instance.` |
|       - |   212 | ` */` |
|  777098 |   213 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   214 |  |
|  777103 |   215 | `	SySetRelease(&pBlock->aPostContFix);` |
|  777103 |   216 | `	SySetRelease(&pBlock->aJumpFix);` |
|  777103 |   217 |  |
|       - |   218 | `/*` |
|       - |   219 | ` * Release a block.` |
|       - |   220 | ` */` |
|  777098 |   221 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   222 |  |
|  777103 |   223 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  777103 |   224 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   225 | `	/* Free the instance */` |
|  777103 |   226 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  777103 |   227 |  |
|       - |   228 | `/*` |
|       - |   229 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   230 | ` */` |
|  777098 |   231 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   232 |  |
|  777103 |   233 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  777103 |   234 | `	if( pBlock == 0 ){` |
|       - |   235 | `		/* No more block to pop */` |
|     ! 0 |   236 | `		return SXERR_EMPTY;` |
|       - |   237 | `	}` |
|       - |   238 | `	/* Point to the upper block */` |
|  777103 |   239 | `	pGen->pCurrent = pBlock->pParent;` |
|  777103 |   240 | `	if( ppBlock ){` |
|       - |   241 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   242 | `		*ppBlock = pBlock;` |
|     ! 0 |   243 | `	}else{` |
|       - |   244 | `		/* Safely release the block */` |
|  777103 |   245 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   246 | `	}` |
|  777103 |   247 | `	return SXRET_OK;` |
|  388554 |   248 |  |
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
|  220820 |   259 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   260 |  |
|       - |   261 | `	JumpFixup sJumpFix;` |
|       - |   262 | `	sxi32 rc;` |
|       - |   263 | `	/* Init the JumpFixup structure */` |
|  220825 |   264 | `	sJumpFix.nJumpType = nJumpType;` |
|  220825 |   265 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   266 | `	/* Insert in the jump fixup table */` |
|  220825 |   267 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  220825 |   268 | `	return rc;` |
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
|  543276 |   281 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   282 |  |
|       - |   283 | `	JumpFixup *aFix;` |
|       - |   284 | `	VmInstr *pInstr;` |
|       - |   285 | `	sxu32 nFixed;` |
|       - |   286 | `	sxu32 n;` |
|       - |   287 | `	/* Point to the jump fixup table */` |
|  543281 |   288 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   289 | `	/* Fix the desired jumps */` |
|  978577 |   290 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  435301 |   291 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   292 | `			/* Already fixed */` |
|  173673 |   293 | `			continue;` |
|       - |   294 | `		}` |
|  261633 |   295 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   296 | `			/* Not of our interest */` |
|   40815 |   297 | `			continue;` |
|       - |   298 | `		}` |
|       - |   299 | `		/* Point to the instruction to fix */` |
|  220823 |   300 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  220823 |   301 | `		if( pInstr ){` |
|  220823 |   302 | `			pInstr->iP2 = nJumpDest;` |
|  220823 |   303 | `			nFixed++;` |
|       - |   304 | `			/* Mark as fixed */` |
|  220823 |   305 | `			aFix[n].nJumpType = -1;` |
|  110409 |   306 | `		}` |
|  110414 |   307 | `	}` |
|       - |   308 | `	/* Total number of fixed jumps */` |
|  543281 |   309 | `	return nFixed;` |
|       5 |   310 |  |
|       - |   311 | `/*` |
|       - |   312 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   313 | ` * The goto statement can be used to jump to another section` |
|       - |   314 | ` * in the program.` |
|       - |   315 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   316 | ` * statement for more information.` |
|       - |   317 | ` */` |
|  220634 |   318 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   319 |  |
|       - |   320 | `	JumpFixup *pJump,*aJumps;` |
|       - |   321 | `	Label *pLabel,*aLabel;` |
|       - |   322 | `	VmInstr *pInstr;` |
|       - |   323 | `	sxi32 rc;` |
|       - |   324 | `	sxu32 n;` |
|       - |   325 | `	/* Point to the goto table */` |
|  220639 |   326 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   327 | `	/* Fix */` |
|  220785 |   328 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  220637 |   353 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  220769 |   354 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   355 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   356 | `			/* Emit a warning */` |
|      40 |   357 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   358 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   359 | `		}` |
|      71 |   360 | `	}` |
|  220637 |   361 | `	return SXRET_OK;` |
|  110322 |   362 |  |
|       - |   363 | `/*` |
|       - |   364 | ` * Check if a given token value is installed in the literal table.` |
|       - |   365 | ` */` |
|  698022 |   366 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   367 |  |
|       - |   368 | `	SyHashEntry *pEntry;` |
|  698027 |   369 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  698027 |   370 | `	if( pEntry == 0 ){` |
|  303493 |   371 | `		return SXERR_NOTFOUND;` |
|       - |   372 | `	}` |
|  394539 |   373 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  394539 |   374 | `	return SXRET_OK;` |
|  349016 |   375 |  |
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
|  303488 |   386 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   387 |  |
|  303493 |   388 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  303493 |   389 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  151744 |   390 | `	}` |
|  303493 |   391 | `	return SXRET_OK;` |
|       5 |   392 |  |
|       - |   393 | `/*` |
|       - |   394 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   395 | ` * in the constant table.` |
|       - |   396 | ` */` |
|  116252 |   397 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   398 |  |
|       - |   399 | `	ph7_value *pObj;` |
|  116257 |   400 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   401 | `	/* Reserve a new constant */` |
|  116257 |   402 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  116257 |   403 | `	if( pObj == 0 ){` |
|     ! 0 |   404 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   405 | `		return 0;` |
|       - |   406 | `	}` |
|  116257 |   407 | `	*pIdx = nIdx;` |
|       - |   408 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   409 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   410 | `	 */` |
|  116257 |   411 | `	return pObj;` |
|   58131 |   412 |  |
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
|  417382 |   427 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   428 |  |
|       - |   429 | `	VmCallArgMap *pMap;` |
|  417387 |   430 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   431 | `	if( p3 == 0 ){` |
|      31 |   432 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   433 | `		if( pMap == 0 ) return 0;` |
|      31 |   434 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   435 | `		p3 = (void *)pMap;` |
|      14 |   436 | `	}` |
|      33 |   437 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   438 | `	return p3;` |
|  208696 |   439 |  |
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
|  116898 |   498 | `static int GenStateFindBadNumericSeparator(` |
|       - |   499 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   500 |  |
|  116903 |   501 | `	const char *z = pRaw->zString;` |
|  116903 |   502 | `	sxu32 n = pRaw->nByte;` |
|  116903 |   503 | `	int base = 10;` |
|       - |   504 | `	sxu32 i, start;` |
|  116903 |   505 | `	if( n < 2 ) return 0;` |
|    9827 |   506 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   507 | `		base = 16;` |
|    9792 |   508 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   509 | `		base = 2;` |
|     139 |   510 | `	}` |
|   35821 |   511 | `	for( i = 0; i < n; ++i ){` |
|   26013 |   512 | `		if( z[i] != '_' ) continue;` |
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
|    9813 |   529 | `	return 0;` |
|   58454 |   530 |  |
|       - |   531 | `/*` |
|       - |   532 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   533 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   534 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   535 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   536 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   537 | ` * so callers can bail from the current construct).` |
|       - |   538 | ` */` |
|  116898 |   539 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   540 |  |
|  116903 |   541 | `	const char *zBad = 0;` |
|  116903 |   542 | `	sxu32 nBad = 0;` |
|       - |   543 | `	SyString sBad;` |
|       - |   544 | `	sxi32 rc;` |
|  116903 |   545 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  116889 |   546 | `		return SXRET_OK;` |
|       - |   547 | `	}` |
|      18 |   548 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   549 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   550 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   551 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   552 | `		return SXERR_ABORT;` |
|       - |   553 | `	}` |
|      18 |   554 | `	return SXERR_SYNTAX;` |
|   58454 |   555 |  |
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
|  116884 |   572 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   573 | `	SyMemBackend *pAlloc,` |
|       - |   574 | `	const SyString *pToken,` |
|       - |   575 | `	char *zScratch, sxu32 nScratch,` |
|       - |   576 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   577 |  |
|       - |   578 | `	sxu32 i, j;` |
|  116889 |   579 | `	int hasUnderscore = 0;` |
|       - |   580 | `	char *zBuf;` |
|  116889 |   581 | `	*pzAlloc = 0;` |
|  247893 |   582 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  131261 |   583 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   65507 |   584 | `	}` |
|  116889 |   585 | `	if( !hasUnderscore ){` |
|  116637 |   586 | `		SyStringDupPtr(pOut, pToken);` |
|  116637 |   587 | `		return SXRET_OK;` |
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
|   58447 |   604 |  |
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
|  116870 |   621 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   622 |  |
|  116875 |   623 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  116875 |   624 | `	sxu32 nIdx = 0;` |
|       - |   625 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  116875 |   626 | `	char *zAlloc = 0;` |
|       - |   627 | `	SyString sNum;` |
|       - |   628 | `	sxi32 rc;` |
|   58435 |   629 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  116875 |   630 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  116875 |   631 | `	if( rc != SXRET_OK ){` |
|      14 |   632 | `		return rc;` |
|       - |   633 | `	}` |
|  175295 |   634 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   58430 |   635 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  116865 |   636 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   637 | `		return SXERR_ABORT;` |
|       - |   638 | `	}` |
|  116865 |   639 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   640 | `		ph7_value *pObj;` |
|       - |   641 | `		sxi64 iValue;` |
|  116257 |   642 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  116257 |   643 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  116257 |   644 | `		if( pObj == 0 ){` |
|     ! 0 |   645 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   646 | `			return SXERR_ABORT;` |
|       - |   647 | `		}` |
|  116257 |   648 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   58131 |   649 | `	}else{` |
|       - |   650 | `		/* Real number */` |
|       - |   651 | `		ph7_value *pObj;` |
|       - |   652 | `		/* Reserve a new constant */` |
|     613 |   653 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     613 |   654 | `		if( pObj == 0 ){` |
|     ! 0 |   655 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   656 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   657 | `			return SXERR_ABORT;` |
|       - |   658 | `		}` |
|     613 |   659 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     613 |   660 | `		PH7_MemObjToReal(pObj);` |
|       - |   661 | `	}` |
|  116865 |   662 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   663 | `	/* Emit the load constant instruction */` |
|  116865 |   664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   665 | `	/* Node successfully compiled */` |
|  116865 |   666 | `	return SXRET_OK;` |
|   58440 |   667 |  |
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
|   83810 |   679 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   680 |  |
|   83815 |   681 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   682 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   683 | `	ph7_value *pObj;` |
|       - |   684 | `	sxu32 nIdx;` |
|   83815 |   685 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   686 | `	/* Delimit the string */` |
|   83815 |   687 | `	zIn  = pStr->zString;` |
|   83815 |   688 | `	zEnd = &zIn[pStr->nByte];` |
|   83815 |   689 | `	if( zIn >= zEnd ){` |
|       - |   690 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   691 | `		 * rather than reserving a new object each time. */` |
|    6783 |   692 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    6783 |   693 | `		return SXRET_OK;` |
|       - |   694 | `	}` |
|   77037 |   695 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   696 | `		/* Already processed,emit the load constant instruction` |
|       - |   697 | `		 * and return.` |
|       - |   698 | `		 */` |
|   30535 |   699 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   30535 |   700 | `		return SXRET_OK;` |
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
|   41910 |   750 |  |
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
|    2130 |   916 | `static sxi32 GenStateProcessStringExpression(` |
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
|    2135 |   927 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   928 | `	/* Preallocate some slots */` |
|    2135 |   929 | `	SySetAlloc(&sToken,0x08);` |
|       - |   930 | `	/* Tokenize the text */` |
|    2135 |   931 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   932 | `	/* Swap delimiter */` |
|    2135 |   933 | `	pTmpIn  = pGen->pIn;` |
|    2135 |   934 | `	pTmpEnd = pGen->pEnd;` |
|    2135 |   935 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2135 |   936 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   937 | `	/* Compile the expression */` |
|    2135 |   938 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   939 | `	/* Restore token stream */` |
|    2135 |   940 | `	pGen->pIn  = pTmpIn;` |
|    2135 |   941 | `	pGen->pEnd = pTmpEnd;` |
|       - |   942 | `	/* Release the token set */` |
|    2135 |   943 | `	SySetRelease(&sToken);` |
|       - |   944 | `	/* Compilation result */` |
|    2135 |   945 | `	return rc;` |
|       5 |   946 |  |
|       - |   947 | `/*` |
|       - |   948 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   949 | ` */` |
|   23408 |   950 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   951 |  |
|       - |   952 | `	ph7_value *pConstObj;` |
|   23413 |   953 | `	sxu32 nIdx = 0;` |
|       - |   954 | `	/* Reserve a new constant */` |
|   23413 |   955 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   23413 |   956 | `	if( pConstObj == 0 ){` |
|     ! 0 |   957 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   958 | `		return 0;` |
|       - |   959 | `	}` |
|   23413 |   960 | `	(*pCount)++;` |
|   23413 |   961 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   962 | `	/* Emit the load constant instruction */` |
|   23413 |   963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   23413 |   964 | `	return pConstObj;` |
|   11709 |   965 |  |
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
|   21970 |  1004 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1005 |  |
|   21975 |  1006 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1007 | `	const char *zIn,*zCur,*zEnd;` |
|   21975 |  1008 | `	ph7_value *pObj = 0;` |
|       - |  1009 | `	sxi32 iCons;` |
|       - |  1010 | `	sxi32 rc;` |
|       - |  1011 | `	/* Delimit the string */` |
|   21975 |  1012 | `	zIn  = pStr->zString;` |
|   21975 |  1013 | `	zEnd = &zIn[pStr->nByte];` |
|   21975 |  1014 | `	if( zIn >= zEnd ){` |
|       - |  1015 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1016 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1017 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1018 | `		 */` |
|     309 |  1019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     309 |  1020 | `		return SXRET_OK;` |
|       - |  1021 | `	}` |
|   21671 |  1022 | `	zCur = 0;` |
|       - |  1023 | `	/* Compile the node */` |
|   21671 |  1024 | `	iCons = 0;` |
|   11898 |  1025 | `	for(;;){` |
|   35639 |  1026 | `		zCur = zIn;` |
|  172513 |  1027 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  139009 |  1028 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1029 | `				break;` |
|  138885 |  1030 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2010 |  1031 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1006 |  1032 | `					break;` |
|       - |  1033 | `			}` |
|  136879 |  1034 | `			zIn++;` |
|       5 |  1035 | `		}` |
|   35639 |  1036 | `		if( zIn > zCur ){` |
|   16753 |  1037 | `			if( pObj == 0 ){` |
|   16285 |  1038 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   16285 |  1039 | `				if( pObj == 0 ){` |
|     ! 0 |  1040 | `					return SXERR_ABORT;` |
|       - |  1041 | `				}` |
|    8140 |  1042 | `			}` |
|   16753 |  1043 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    8374 |  1044 | `		}` |
|   35639 |  1045 | `		if( zIn >= zEnd ){` |
|   21671 |  1046 | `			break;` |
|       - |  1047 | `		}` |
|   13973 |  1048 | `		if( zIn[0] == '\\' ){` |
|   11843 |  1049 | `			const char *zPtr = 0;` |
|       - |  1050 | `			sxu32 n;` |
|   11843 |  1051 | `			zIn++;` |
|   11843 |  1052 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1053 | `				break;` |
|       - |  1054 | `			}` |
|   11843 |  1055 | `			if( pObj == 0 ){` |
|    7133 |  1056 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    7133 |  1057 | `				if( pObj == 0 ){` |
|     ! 0 |  1058 | `					return SXERR_ABORT;` |
|       - |  1059 | `				}` |
|    3564 |  1060 | `			}` |
|   11843 |  1061 | `			n = sizeof(char); /* size of conversion */` |
|   11843 |  1062 | `			switch( zIn[0] ){` |
|       7 |  1063 | `			case '$':` |
|       - |  1064 | `				/* Dollar sign */` |
|      15 |  1065 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|      15 |  1066 | `				break;` |
|      48 |  1067 | `			case '\\':` |
|       - |  1068 | `				/* A literal backslash */` |
|     100 |  1069 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     100 |  1070 | `				break;` |
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
|    5438 |  1083 | `			case 'n':` |
|       - |  1084 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   10881 |  1085 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   10881 |  1086 | `				break;` |
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
|       9 |  1107 | `			case '0':` |
|       - |  1108 | `				/* NUL byte */` |
|      19 |  1109 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      19 |  1110 | `				break;` |
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
|   11843 |  1154 | `			zIn += n;` |
|   11843 |  1155 | `			continue;` |
|       - |  1156 | `		}` |
|    2135 |  1157 | `		if( zIn[0] == '{' ){` |
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
|    2007 |  1191 | `			const char *zExpr = zIn;` |
|       - |  1192 | `			/* Assemble variable name */` |
|    1010 |  1193 | `			for(;;){` |
|       - |  1194 | `				/* Jump leading dollars */` |
|    4027 |  1195 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2007 |  1196 | `					zIn++;` |
|       5 |  1197 | `				}` |
|    1010 |  1198 | `				for(;;){` |
|   11463 |  1199 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8433 |  1200 | `						zIn++;` |
|       5 |  1201 | `					}` |
|    2025 |  1202 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1203 | `						/* UTF-8 stream */` |
|     ! 0 |  1204 | `						zIn++;` |
|     ! 0 |  1205 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1206 | `							zIn++;` |
|     ! 0 |  1207 | `						}` |
|     ! 0 |  1208 | `						continue;` |
|       - |  1209 | `					}` |
|    2025 |  1210 | `					break;` |
|     ! 0 |  1211 | `				}` |
|    2025 |  1212 | `				if( zIn >= zEnd ){` |
|     172 |  1213 | `					break;` |
|       - |  1214 | `				}` |
|    1857 |  1215 | `				if( zIn[0] == '[' ){` |
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
|    1847 |  1233 | `				}else if(zIn[0] == '{' ){` |
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
|    1843 |  1251 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1252 | `					/* Member access operator '->' */` |
|      21 |  1253 | `					zIn += 2;` |
|    1834 |  1254 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1255 | `					/* Static member access operator '::' */` |
|     ! 0 |  1256 | `					zIn += 2;` |
|     ! 0 |  1257 | `				}else{` |
|     915 |  1258 | `					break;` |
|       - |  1259 | `				}` |
|       3 |  1260 | `			}` |
|       - |  1261 | `			/* Process the expression */` |
|    2007 |  1262 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2007 |  1263 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1264 | `				return SXERR_ABORT;` |
|       - |  1265 | `			}` |
|    2007 |  1266 | `			if( rc != SXERR_EMPTY ){` |
|    2005 |  1267 | `				++iCons;` |
|    1000 |  1268 | `			}` |
|       - |  1269 | `		}` |
|       - |  1270 | `		/* Invalidate the previously used constant */` |
|    2135 |  1271 | `		pObj = 0;` |
|       5 |  1272 | `	}/*for(;;)*/` |
|   21671 |  1273 | `	if( iCons > 1 ){` |
|       - |  1274 | `		/* Concatenate all compiled constants */` |
|    1593 |  1275 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     794 |  1276 | `	}` |
|       - |  1277 | `	/* Node successfully compiled */` |
|   21671 |  1278 | `	return SXRET_OK;` |
|   10990 |  1279 |  |
|       - |  1280 | `/*` |
|       - |  1281 | ` * Compile a double quoted string.` |
|       - |  1282 | ` *  See the block-comment above for more information.` |
|       - |  1283 | ` */` |
|   21910 |  1284 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1285 |  |
|       - |  1286 | `	sxi32 rc;` |
|   21915 |  1287 | `	rc = GenStateCompileString(&(*pGen));` |
|   10955 |  1288 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1289 | `	/* Compilation result */` |
|   21915 |  1290 | `	return rc;` |
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
|   20220 |  1334 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   20225 |  1345 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1346 | `	/* Compile the expression*/` |
|   20225 |  1347 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1348 | `	/* Restore token stream */` |
|   20225 |  1349 | `	RE_SWAP_DELIMITER(pGen);` |
|   20225 |  1350 | `	return rc;` |
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
|   29102 |  1389 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1390 |  |
|       - |  1391 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1392 | `	SyToken *pKey,*pCur;` |
|   29107 |  1393 | `	sxi32 iEmitRef = 0;` |
|   29107 |  1394 | `	sxi32 iSpread = 0;` |
|   29107 |  1395 | `	sxi32 nPair = 0;` |
|       - |  1396 | `	sxi32 iNest;` |
|       - |  1397 | `	sxi32 rc;` |
|   29107 |  1398 | `	xValidator = 0;` |
|   23839 |  1399 | `	for(;;){` |
|       - |  1400 | `		/* Jump leading commas */` |
|   54111 |  1401 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    6433 |  1402 | `			pGen->pIn++;` |
|       5 |  1403 | `		}` |
|   47683 |  1404 | `		pCur = pGen->pIn;` |
|   47683 |  1405 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1406 | `			/* No more entry to process */` |
|   29091 |  1407 | `			break;` |
|       - |  1408 | `		}` |
|   18597 |  1409 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1410 | `			continue;` |
|       - |  1411 | `		}` |
|       - |  1412 | `		/* Compile the key if available */` |
|   18597 |  1413 | `		pKey = pCur;` |
|   18597 |  1414 | `		iNest = 0;` |
|   51869 |  1415 | `		while( pCur < pGen->pIn ){` |
|   34807 |  1416 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1531 |  1417 | `				break;` |
|       - |  1418 | `			}` |
|       - |  1419 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1420 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1421 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1422 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1423 | `			 */` |
|   33281 |  1424 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   33275 |  1488 | `			if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     286 |  1489 | `				iNest++;` |
|   33134 |  1490 | `			}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1491 | `				/* Don't worry about mismatched brackets here,the expression` |
|       - |  1492 | `				 * parser will shortly detect any syntax error.` |
|       - |  1493 | `				 */` |
|     286 |  1494 | `				iNest--;` |
|     141 |  1495 | `			}` |
|   33275 |  1496 | `			pCur++;` |
|       5 |  1497 | `		}` |
|   18597 |  1498 | `		rc = SXERR_EMPTY;` |
|   18597 |  1499 | `		if( pCur < pGen->pIn ){` |
|    1531 |  1500 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1501 | `				/* Missing value */` |
|      13 |  1502 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1503 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1504 | `					return SXERR_ABORT;` |
|       - |  1505 | `				}` |
|      13 |  1506 | `				return SXRET_OK;` |
|       - |  1507 | `			}` |
|       - |  1508 | `			/* Compile the expression holding the key */` |
|    1521 |  1509 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1510 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1521 |  1511 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1512 | `				return SXERR_ABORT;` |
|       - |  1513 | `			}` |
|    1521 |  1514 | `			pCur++; /* Jump the '=>' operator */` |
|   17829 |  1515 | `		}else if( pKey == pCur ){` |
|       - |  1516 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1517 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1518 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1519 | `		}else{` |
|       - |  1520 | `			/* Reset back the cursor and point to the entry value */` |
|   17071 |  1521 | `			pCur = pKey;` |
|       - |  1522 | `		}` |
|   18587 |  1523 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1524 | `			/* No available key,load NULL */` |
|   17073 |  1525 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    8534 |  1526 | `		}` |
|   18587 |  1527 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1528 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      45 |  1529 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      45 |  1530 | `			iEmitRef = 1;` |
|      45 |  1531 | `			pCur++; /* Jump the '&' token */` |
|      45 |  1532 | `			if( pCur >= pGen->pIn ){` |
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
|   18585 |  1546 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   18585 |  1547 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   18581 |  1560 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   18581 |  1561 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1562 | `			return SXERR_ABORT;` |
|       - |  1563 | `		}` |
|   18581 |  1564 | `		if( iSpread ){` |
|       - |  1565 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1566 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   18550 |  1567 | `		}else if( iEmitRef ){` |
|       - |  1568 | `			/* Emit the load reference instruction */` |
|      40 |  1569 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1570 | `		}` |
|   18581 |  1571 | `		xValidator = 0;` |
|   18581 |  1572 | `		iEmitRef = 0;` |
|   18581 |  1573 | `		iSpread = 0;` |
|   18581 |  1574 | `		nPair++;` |
|       5 |  1575 | `	}` |
|       - |  1576 | `	/* Emit the load map instruction */` |
|   29091 |  1577 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1578 | `	/* Node successfully compiled */` |
|   29091 |  1579 | `	return SXRET_OK;` |
|   14556 |  1580 |  |
|       - |  1581 | `/*` |
|       - |  1582 | ` * Compile the 'array' language construct.` |
|       - |  1583 | ` *	 According to the PHP language reference manual` |
|       - |  1584 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1585 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1586 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1587 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1588 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1589 | ` */` |
|   28252 |  1590 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1591 |  |
|       - |  1592 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   28257 |  1593 | `	pGen->pIn += 2;` |
|   28257 |  1594 | `	pGen->pEnd--;` |
|   14126 |  1595 | `	SXUNUSED(iCompileFlag);` |
|   28257 |  1596 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1597 |  |
|       - |  1598 | `/*` |
|       - |  1599 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1600 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1601 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1602 | ` */` |
|     850 |  1603 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1604 |  |
|       - |  1605 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     855 |  1606 | `	pGen->pIn++;` |
|     855 |  1607 | `	pGen->pEnd--;` |
|     425 |  1608 | `	SXUNUSED(iCompileFlag);` |
|     855 |  1609 | `	return GenStateCompileArrayBody(pGen);` |
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
|       - |  1797 | `static int GenStateIsReadonly(SyToken *pTok);` |
|       - |  1798 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|       - |  1799 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|       - |  1800 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1801 | `/*` |
|       - |  1802 | ` * Compile an annoynmous function or a closure.` |
|       - |  1803 | ` * According to the PHP language reference` |
|       - |  1804 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1805 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1806 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1807 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1808 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1809 | ` *  Example Anonymous function variable assignment example` |
|       - |  1810 | ` * <?php` |
|       - |  1811 | ` * $greet = function($name)` |
|       - |  1812 | ` * {` |
|       - |  1813 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1814 | ` * };` |
|       - |  1815 | ` * $greet('World');` |
|       - |  1816 | ` * $greet('PHP');` |
|       - |  1817 | ` * ?>` |
|       - |  1818 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1819 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1820 | ` */` |
|     248 |  1821 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1822 |  |
|       - |  1823 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1824 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1825 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1826 | `							  * one thread is allowed to compile the script.` |
|       - |  1827 | `						      */` |
|       - |  1828 | `	ph7_value *pObj;` |
|       - |  1829 | `	SyString sName;` |
|       - |  1830 | `	sxu32 nIdx;` |
|       - |  1831 | `	sxu32 nLen;` |
|       - |  1832 | `	sxi32 rc;` |
|       - |  1833 |  |
|     253 |  1834 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     253 |  1835 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1836 | `		pGen->pIn++;` |
|     ! 0 |  1837 | `	}` |
|       - |  1838 | `	/* Reserve a constant for the lambda */` |
|     253 |  1839 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     253 |  1840 | `	if( pObj == 0 ){` |
|     ! 0 |  1841 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1842 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1843 | `		return SXERR_ABORT;` |
|       - |  1844 | `	}` |
|       - |  1845 | `	/* Generate a unique name */` |
|     253 |  1846 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1847 | `	/* Make sure the generated name is unique */` |
|     253 |  1848 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1849 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1850 | `	}` |
|     253 |  1851 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     253 |  1852 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - |  1853 | `	/* Compile the lambda body */` |
|     253 |  1854 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     253 |  1855 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1856 | `		return SXERR_ABORT;` |
|       - |  1857 | `	}` |
|     253 |  1858 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - |  1859 | `		/* Emit the load closure instruction */` |
|      21 |  1860 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|      13 |  1861 | `	}else{` |
|       - |  1862 | `		/* Emit the load constant instruction */` |
|     237 |  1863 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1864 | `	}` |
|       - |  1865 | `	/* Node successfully compiled */` |
|     253 |  1866 | `	return SXRET_OK;` |
|     129 |  1867 |  |
|       - |  1868 | `/*` |
|       - |  1869 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1870 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1871 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1872 | ` */` |
|     150 |  1873 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  1874 | `	ph7_gen_state *pGen,` |
|       - |  1875 | `	ph7_vm_func *pFunc,` |
|       - |  1876 | `	const char *zName,` |
|       - |  1877 | `	sxu32 nByte,` |
|       - |  1878 | `	SyString *aShadow,` |
|       - |  1879 | `	sxu32 nShadow)` |
|       2 |  1880 |  |
|       - |  1881 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  1882 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  1883 | `	sxu32 n, nEnv;` |
|       - |  1884 | `	char *zDup;` |
|     152 |  1885 | `	if( nByte == 0 ){` |
|     ! 0 |  1886 | `		return SXRET_OK;` |
|       - |  1887 | `	}` |
|     150 |  1888 | `	if( nByte == sizeof("this")-1` |
|      81 |  1889 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  1890 | `		return SXRET_OK;` |
|       - |  1891 | `	}` |
|     182 |  1892 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     128 |  1893 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     125 |  1894 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      98 |  1895 | `			return SXRET_OK;` |
|       - |  1896 | `		}` |
|      17 |  1897 | `	}` |
|      53 |  1898 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  1899 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  1900 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  1901 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  1902 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  1903 | `			return SXRET_OK;` |
|       - |  1904 | `		}` |
|      15 |  1905 | `	}` |
|      53 |  1906 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  1907 | `	if( zDup == 0 ){` |
|     ! 0 |  1908 | `		return SXERR_ABORT;` |
|       - |  1909 | `	}` |
|      53 |  1910 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  1911 | `	sEnv.iFlags = 0;` |
|      53 |  1912 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  1913 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  1914 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  1915 | `	return SXRET_OK;` |
|      77 |  1916 |  |
|       - |  1917 | `/*` |
|       - |  1918 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  1919 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  1920 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  1921 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  1922 | ` */` |
|      14 |  1923 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  1924 | `	ph7_gen_state *pGen,` |
|       - |  1925 | `	ph7_vm_func *pFunc,` |
|       - |  1926 | `	const char *zIn,` |
|       - |  1927 | `	const char *zEnd,` |
|       - |  1928 | `	SyString *aShadow,` |
|       - |  1929 | `	sxu32 nShadow)` |
|       1 |  1930 |  |
|       - |  1931 | `	sxi32 rc;` |
|     159 |  1932 | `	while( zIn < zEnd ){` |
|     145 |  1933 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  1934 | `			zIn++;` |
|     ! 0 |  1935 | `			if( zIn < zEnd ){` |
|     ! 0 |  1936 | `				zIn++;` |
|     ! 0 |  1937 | `			}` |
|     ! 0 |  1938 | `			continue;` |
|       - |  1939 | `		}` |
|     144 |  1940 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  1941 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  1942 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  1943 | `			const char *zName;` |
|      13 |  1944 | `			zIn++; /* skip '$' */` |
|      13 |  1945 | `			zName = zIn;` |
|      39 |  1946 | `			while( zIn < zEnd ){` |
|      35 |  1947 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  1948 | `				if( c >= 0xc0 ){` |
|     ! 0 |  1949 | `					zIn++;` |
|     ! 0 |  1950 | `					while( zIn < zEnd` |
|     ! 0 |  1951 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1952 | `						zIn++;` |
|     ! 0 |  1953 | `					}` |
|     ! 0 |  1954 | `					continue;` |
|       - |  1955 | `				}` |
|      35 |  1956 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  1957 | `					break;` |
|       - |  1958 | `				}` |
|      27 |  1959 | `				zIn++;` |
|       1 |  1960 | `			}` |
|      13 |  1961 | `			if( zIn > zName ){` |
|      19 |  1962 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  1963 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  1964 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1965 | `					return SXERR_ABORT;` |
|       - |  1966 | `				}` |
|       6 |  1967 | `			}` |
|      13 |  1968 | `			continue;` |
|       - |  1969 | `		}` |
|     133 |  1970 | `		zIn++;` |
|       1 |  1971 | `	}` |
|      15 |  1972 | `	return SXRET_OK;` |
|       8 |  1973 |  |
|       - |  1974 | `/*` |
|       - |  1975 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  1976 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  1977 | ` *   - plain $<id> pairs` |
|       - |  1978 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  1979 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  1980 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  1981 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  1982 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  1983 | ` *     are never mistakenly captured.` |
|       - |  1984 | ` */` |
|     136 |  1985 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  1986 | `	ph7_gen_state *pGen,` |
|       - |  1987 | `	ph7_vm_func *pFunc,` |
|       - |  1988 | `	SyToken *pStart,` |
|       - |  1989 | `	SyToken *pEnd,` |
|       - |  1990 | `	SyString *aShadow,` |
|       - |  1991 | `	sxu32 nShadow)` |
|       2 |  1992 |  |
|     138 |  1993 | `	SyToken *pScan = pStart;` |
|       - |  1994 | `	sxi32 rc;` |
|     512 |  1995 | `	while( pScan < pEnd ){` |
|     376 |  1996 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 |  1997 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 |  1998 | `				pScan->sData.zString,` |
|      14 |  1999 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 |  2000 | `				aShadow,nShadow);` |
|      15 |  2001 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2002 | `				return SXERR_ABORT;` |
|       - |  2003 | `			}` |
|      15 |  2004 | `			pScan++;` |
|      15 |  2005 | `			continue;` |
|       - |  2006 | `		}` |
|     362 |  2007 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  2008 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  2009 | `			SyToken *pFnKw = pScan;` |
|      20 |  2010 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2011 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  2012 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2013 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2014 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2015 | `			}` |
|      21 |  2016 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2017 | `				SyToken *pInnerSigStart;` |
|       - |  2018 | `				SyToken *pInnerSigEnd;` |
|       - |  2019 | `				SyToken *pInnerBodyEnd;` |
|       - |  2020 | `				SyString *aInnerShadow;` |
|       - |  2021 | `				sxu32 nInnerShadow;` |
|       - |  2022 | `				sxu32 nInnerParamMax;` |
|       - |  2023 | `				SyToken *p;` |
|       - |  2024 | `				int iNestInner;` |
|      19 |  2025 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2026 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2027 | `					pScan++;` |
|     ! 0 |  2028 | `				}` |
|      19 |  2029 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2030 | `					pScan++;` |
|     ! 0 |  2031 | `					continue;` |
|       - |  2032 | `				}` |
|      19 |  2033 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2034 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2035 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2036 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2037 | `					pScan = pEnd;` |
|     ! 0 |  2038 | `					continue;` |
|       - |  2039 | `				}` |
|       - |  2040 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2041 | `				nInnerParamMax = 0;` |
|      57 |  2042 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2043 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2044 | `						nInnerParamMax++;` |
|       6 |  2045 | `					}` |
|      20 |  2046 | `				}` |
|      19 |  2047 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2048 | `					&pGen->pVm->sAllocator,` |
|      18 |  2049 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2050 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2051 | `					return SXERR_ABORT;` |
|       - |  2052 | `				}` |
|      19 |  2053 | `				nInnerShadow = 0;` |
|      25 |  2054 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2055 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2056 | `				}` |
|      57 |  2057 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2058 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2059 | `						continue;` |
|       - |  2060 | `					}` |
|      13 |  2061 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2062 | `						break;` |
|       - |  2063 | `					}` |
|      13 |  2064 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2065 | `						continue;` |
|       - |  2066 | `					}` |
|      13 |  2067 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2068 | `				}` |
|      19 |  2069 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2070 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2071 | `					pScan++;` |
|     ! 0 |  2072 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2073 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2074 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2075 | `						pScan++;` |
|     ! 0 |  2076 | `					}` |
|     ! 0 |  2077 | `					if( pScan < pEnd` |
|     ! 0 |  2078 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2079 | `						pScan++;` |
|     ! 0 |  2080 | `					}` |
|     ! 0 |  2081 | `				}` |
|      19 |  2082 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2083 | `					pScan++; /* past '=>' */` |
|       9 |  2084 | `				}` |
|      19 |  2085 | `				pInnerBodyEnd = pScan;` |
|      19 |  2086 | `				iNestInner = 0;` |
|     131 |  2087 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2088 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2089 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2090 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2091 | `						break;` |
|       - |  2092 | `					}` |
|     113 |  2093 | `					if( pInnerBodyEnd->nType &` |
|       - |  2094 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2095 | `						iNestInner++;` |
|     112 |  2096 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2097 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2098 | `						iNestInner--;` |
|       1 |  2099 | `					}` |
|     113 |  2100 | `					pInnerBodyEnd++;` |
|       1 |  2101 | `				}` |
|       - |  2102 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2103 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2104 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2105 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2106 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2107 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2108 | `				 *` |
|       - |  2109 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2110 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2111 | `				 * range after the '=' sign. */` |
|       - |  2112 | `				{` |
|      19 |  2113 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2114 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2115 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2116 | `						SyToken *pEq = 0;` |
|      13 |  2117 | `						int iNestArg = 0;` |
|      49 |  2118 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2119 | `							if( iNestArg == 0` |
|      39 |  2120 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2121 | `								break;` |
|       - |  2122 | `							}` |
|      37 |  2123 | `							if( pArgEnd->nType &` |
|       - |  2124 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2125 | `								iNestArg++;` |
|      37 |  2126 | `							}else if( pArgEnd->nType &` |
|       - |  2127 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2128 | `								iNestArg--;` |
|     ! 0 |  2129 | `							}` |
|      36 |  2130 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2131 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2132 | `								pEq = pArgEnd;` |
|       3 |  2133 | `							}` |
|      37 |  2134 | `							pArgEnd++;` |
|       1 |  2135 | `						}` |
|      13 |  2136 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2137 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2138 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2139 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2140 | `								return SXERR_ABORT;` |
|       - |  2141 | `							}` |
|       3 |  2142 | `						}` |
|      13 |  2143 | `						pArgStart = pArgEnd;` |
|      12 |  2144 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2145 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2146 | `							pArgStart++;` |
|       1 |  2147 | `						}` |
|       1 |  2148 | `					}` |
|       - |  2149 | `				}` |
|      28 |  2150 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2151 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2152 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2153 | `					return SXERR_ABORT;` |
|       - |  2154 | `				}` |
|      19 |  2155 | `				pScan = pInnerBodyEnd;` |
|      19 |  2156 | `				continue;` |
|       - |  2157 | `			}` |
|       1 |  2158 | `		}` |
|     344 |  2159 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     206 |  2160 | `			pScan++;` |
|     206 |  2161 | `			continue;` |
|       - |  2162 | `		}` |
|       - |  2163 | `		{` |
|       - |  2164 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     140 |  2165 | `			SyToken *pDollar = pScan;` |
|     207 |  2166 | `			while( &pDollar[1] < pEnd` |
|     140 |  2167 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2168 | `				pDollar++;` |
|     ! 0 |  2169 | `			}` |
|     140 |  2170 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2171 | `				break;` |
|       - |  2172 | `			}` |
|     140 |  2173 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2174 | `				pScan = pDollar + 1;` |
|     ! 0 |  2175 | `				continue;` |
|       - |  2176 | `			}` |
|     209 |  2177 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     138 |  2178 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      69 |  2179 | `				aShadow,nShadow);` |
|     140 |  2180 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2181 | `				return SXERR_ABORT;` |
|       - |  2182 | `			}` |
|     140 |  2183 | `			pScan = pDollar + 2;` |
|       - |  2184 | `		}` |
|       2 |  2185 | `	}` |
|     138 |  2186 | `	return SXRET_OK;` |
|      70 |  2187 |  |
|       - |  2188 | `/*` |
|       - |  2189 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2190 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2191 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2192 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2193 | ` * $this is also made available.` |
|       - |  2194 | ` */` |
|     118 |  2195 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2196 |  |
|       - |  2197 | `	ph7_vm_func *pFunc;` |
|       - |  2198 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2199 | `	GenBlock *pBlock;` |
|       - |  2200 | `	SySet *pInstrContainer;` |
|       - |  2201 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2202 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2203 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2204 | `	SyToken *pSavedEnd;` |
|       - |  2205 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2206 | `	char zName[512];` |
|       - |  2207 | `	static int iCnt = 1;` |
|       - |  2208 | `	char *zDup;` |
|       - |  2209 | `	sxu32 nLen;` |
|       - |  2210 | `	sxu32 nLine;` |
|     122 |  2211 | `	sxi32 iFlags = 0;` |
|     122 |  2212 | `	int bStatic = 0;` |
|       - |  2213 | `	sxi32 rc;` |
|       - |  2214 | `	sxu32 n;` |
|      59 |  2215 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2216 |  |
|     122 |  2217 | `	nLine = pGen->pIn->nLine;` |
|       - |  2218 | `	/* Optional 'static' prefix */` |
|     118 |  2219 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     122 |  2220 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2221 | `		bStatic = 1;` |
|       3 |  2222 | `		pGen->pIn++;` |
|       1 |  2223 | `	}` |
|       - |  2224 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     118 |  2225 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     122 |  2226 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2227 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2228 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2229 | `		return SXERR_SYNTAX;` |
|       - |  2230 | `	}` |
|     122 |  2231 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2232 | `	/* Optional '&' — return by reference */` |
|     122 |  2233 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2234 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2235 | `		pGen->pIn++;` |
|     ! 0 |  2236 | `	}` |
|       - |  2237 | `	/* Expect '(' */` |
|     122 |  2238 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2239 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2240 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2241 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2242 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2243 | `		}else{` |
|     ! 0 |  2244 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2245 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2246 | `		}` |
|       3 |  2247 | `		return SXERR_SYNTAX;` |
|       - |  2248 | `	}` |
|     119 |  2249 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2250 | `	/* Delimit the parameter list */` |
|     119 |  2251 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     119 |  2252 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2253 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2254 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2255 | `		return SXERR_SYNTAX;` |
|       - |  2256 | `	}` |
|       - |  2257 | `	/* Allocate the function state */` |
|     117 |  2258 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     117 |  2259 | `	if( pFunc == 0 ){` |
|     ! 0 |  2260 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2261 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2262 | `		return SXERR_ABORT;` |
|       - |  2263 | `	}` |
|       - |  2264 | `	/* Generate a unique lambda name */` |
|     117 |  2265 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     217 |  2266 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     102 |  2267 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2268 | `	}` |
|     117 |  2269 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     117 |  2270 | `	if( zDup == 0 ){` |
|     ! 0 |  2271 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2272 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2273 | `		return SXERR_ABORT;` |
|       - |  2274 | `	}` |
|     117 |  2275 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2276 | `	/* Collect function arguments */` |
|     117 |  2277 | `	if( pGen->pIn < pSigEnd ){` |
|      87 |  2278 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      87 |  2279 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2280 | `			return SXERR_ABORT;` |
|       - |  2281 | `		}` |
|      42 |  2282 | `	}` |
|       - |  2283 | `	/* Point past ')' and parse optional return type */` |
|     117 |  2284 | `	pGen->pIn = &pSigEnd[1];` |
|     117 |  2285 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     117 |  2286 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2287 | `		return SXERR_ABORT;` |
|     117 |  2288 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2289 | `		return SXERR_SYNTAX;` |
|       - |  2290 | `	}` |
|       - |  2291 | `	/* Expect '=>' */` |
|     117 |  2292 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2293 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2294 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2295 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2296 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2297 | `		}else{` |
|     ! 0 |  2298 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2299 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2300 | `		}` |
|       3 |  2301 | `		return SXERR_SYNTAX;` |
|       - |  2302 | `	}` |
|     114 |  2303 | `	pGen->pIn++; /* Jump '=>' */` |
|     114 |  2304 | `	pBodyStart = pGen->pIn;` |
|     114 |  2305 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2306 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2307 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2308 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2309 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     114 |  2310 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2311 | `	{` |
|     114 |  2312 | `		SyString *aShadow = 0;` |
|     114 |  2313 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     114 |  2314 | `		if( nShadow > 0 ){` |
|      84 |  2315 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      82 |  2316 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      84 |  2317 | `			if( aShadow == 0 ){` |
|     ! 0 |  2318 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2319 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2320 | `				return SXERR_ABORT;` |
|       - |  2321 | `			}` |
|     184 |  2322 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     102 |  2323 | `				aShadow[n] = aArgs[n].sName;` |
|      52 |  2324 | `			}` |
|      41 |  2325 | `		}` |
|     170 |  2326 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      56 |  2327 | `			aShadow,nShadow);` |
|     114 |  2328 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2329 | `			return SXERR_ABORT;` |
|       - |  2330 | `		}` |
|       - |  2331 | `	}` |
|       - |  2332 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2333 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2334 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2335 | `	 * $this. */` |
|     114 |  2336 | `	if( !bStatic ){` |
|       - |  2337 | `		char *zThisDup;` |
|     112 |  2338 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     112 |  2339 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2340 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2341 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2342 | `			return SXERR_ABORT;` |
|       - |  2343 | `		}` |
|     112 |  2344 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     112 |  2345 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     112 |  2346 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     112 |  2347 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     112 |  2348 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      55 |  2349 | `	}` |
|       - |  2350 | `	/* Arrow functions are always closures */` |
|     114 |  2351 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2352 | `	/* Compile the body expression as an implicit return */` |
|     170 |  2353 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      56 |  2354 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     114 |  2355 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2356 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2357 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2358 | `		return SXERR_ABORT;` |
|       - |  2359 | `	}` |
|     114 |  2360 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     114 |  2361 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     114 |  2362 | `	pSavedEnd = pGen->pEnd;` |
|     114 |  2363 | `	pGen->pIn = pBodyStart;` |
|     114 |  2364 | `	pGen->pEnd = pBodyEnd;` |
|     114 |  2365 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     114 |  2366 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2367 | `		return SXERR_ABORT;` |
|       - |  2368 | `	}` |
|       - |  2369 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2370 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2371 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2372 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     114 |  2373 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     114 |  2374 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     114 |  2375 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     114 |  2376 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     114 |  2377 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2378 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     114 |  2379 | `	pGen->pIn = pBodyEnd;` |
|     114 |  2380 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2381 | `	/* Emit the load-closure instruction */` |
|     114 |  2382 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     114 |  2383 | `	return SXRET_OK;` |
|      63 |  2384 |  |
|       - |  2385 | `/*` |
|       - |  2386 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2387 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2388 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2389 | ` * expression's value.` |
|       - |  2390 | ` */` |
|     346 |  2391 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2392 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2393 |  |
|       - |  2394 | `	SySet *pInstrContainer;` |
|       - |  2395 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2396 | `	GenBlock *pArmBlock;` |
|       - |  2397 | `	sxi32 rc;` |
|     349 |  2398 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2399 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2400 | `	pGen->pIn  = pStart;` |
|     349 |  2401 | `	pGen->pEnd = pStop;` |
|     349 |  2402 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2403 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2404 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2405 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2406 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2407 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2408 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2409 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2410 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2411 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2412 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2413 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2414 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2415 | `		return SXERR_ABORT;` |
|       - |  2416 | `	}` |
|     349 |  2417 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2418 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2419 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2420 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2421 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2422 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2423 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2424 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2425 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2426 | `		return SXERR_ABORT;` |
|       - |  2427 | `	}` |
|     349 |  2428 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2429 | `		return SXERR_EMPTY;` |
|       - |  2430 | `	}` |
|     349 |  2431 | `	return SXRET_OK;` |
|     176 |  2432 |  |
|       - |  2433 | `/*` |
|       - |  2434 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2435 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2436 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2437 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2438 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2439 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2440 | ` */` |
|       - |  2441 | `/*` |
|       - |  2442 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2443 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2444 | ` * caller can bail out of the current expression.` |
|       - |  2445 | ` */` |
|       2 |  2446 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2447 |  |
|       - |  2448 | `	va_list ap;` |
|       - |  2449 | `	sxi32 rc;` |
|       - |  2450 | `	SyBlob sMsg;` |
|       3 |  2451 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2452 | `	va_start(ap,zFmt);` |
|       3 |  2453 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2454 | `	va_end(ap);` |
|       3 |  2455 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2456 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2457 | `	SyBlobRelease(&sMsg);` |
|       3 |  2458 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2459 | `		return SXERR_ABORT;` |
|       - |  2460 | `	}` |
|       3 |  2461 | `	return SXERR_SYNTAX;` |
|       2 |  2462 |  |
|       - |  2463 | `/*` |
|       - |  2464 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2465 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2466 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2467 | ` */` |
|     348 |  2468 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2469 |  |
|     352 |  2470 | `	SyToken *pCur = pStart;` |
|     352 |  2471 | `	int iNest = 0;` |
|     814 |  2472 | `	while( pCur < pEnd ){` |
|     780 |  2473 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2474 | `			iNest++;` |
|     774 |  2475 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2476 | `			iNest--;` |
|     762 |  2477 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2478 | `			return pCur;` |
|       - |  2479 | `		}` |
|     466 |  2480 | `		pCur++;` |
|       4 |  2481 | `	}` |
|      37 |  2482 | `	return pEnd;` |
|     178 |  2483 |  |
|      70 |  2484 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2485 |  |
|       - |  2486 | `	ph7_match *pMatch;` |
|       - |  2487 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2488 | `	int bHasDefault = 0;` |
|       - |  2489 | `	sxu32 nLine;` |
|       - |  2490 | `	sxi32 rc;` |
|      35 |  2491 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2492 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2493 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2494 | `	/* Expect '(' */` |
|      75 |  2495 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2496 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2497 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2498 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2499 | `	}` |
|      75 |  2500 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2501 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2502 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2503 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2504 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2505 | `	}` |
|      75 |  2506 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2507 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2508 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2509 | `	}` |
|       - |  2510 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2511 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2512 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2513 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2514 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2515 | `		return SXERR_ABORT;` |
|       - |  2516 | `	}` |
|      75 |  2517 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2518 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2519 | `	/* Expect '{' */` |
|      75 |  2520 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2521 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2522 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2523 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2524 | `	}` |
|      75 |  2525 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2526 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2527 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2528 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2529 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2530 | `	}` |
|       - |  2531 | `	/* Allocate ph7_match container */` |
|      75 |  2532 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2533 | `	if( pMatch == 0 ){` |
|     ! 0 |  2534 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2535 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2536 | `		return SXERR_ABORT;` |
|       - |  2537 | `	}` |
|      75 |  2538 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2539 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2540 | `	/* Iterate arms */` |
|     253 |  2541 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2542 | `		ph7_match_arm sArm;` |
|       - |  2543 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2544 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2545 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2546 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2547 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2548 | `		/* 'default' arm? */` |
|     182 |  2549 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2550 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2551 | `			if( bHasDefault ){` |
|       3 |  2552 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2553 | `					"Match expressions may only contain one default arm");` |
|       4 |  2554 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2555 | `			}` |
|      20 |  2556 | `			sArm.bDefault = 1;` |
|      20 |  2557 | `			bHasDefault = 1;` |
|      20 |  2558 | `			pGen->pIn++;` |
|      20 |  2559 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2560 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2561 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2562 | `			}` |
|      20 |  2563 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2564 | `		}else{` |
|       - |  2565 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2566 | `			pCondStart = pGen->pIn;` |
|     166 |  2567 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2568 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2569 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2570 | `				SySet sCondBc;` |
|       9 |  2571 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2572 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2573 | `						"syntax error, empty match condition expression");` |
|       - |  2574 | `				}` |
|       9 |  2575 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2576 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2577 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2578 | `					return SXERR_ABORT;` |
|       - |  2579 | `				}` |
|       9 |  2580 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2581 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2582 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2583 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2584 | `			}` |
|     166 |  2585 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2586 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2587 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2588 | `			}` |
|     163 |  2589 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2590 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2591 | `					"syntax error, empty match condition expression");` |
|       - |  2592 | `			}` |
|       - |  2593 | `			{` |
|       - |  2594 | `				SySet sCondBc;` |
|     163 |  2595 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2596 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2597 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2598 | `					return SXERR_ABORT;` |
|       - |  2599 | `				}` |
|     163 |  2600 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2601 | `			}` |
|     163 |  2602 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2603 | `		}` |
|       - |  2604 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2605 | `		pResStart = pGen->pIn;` |
|     181 |  2606 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2607 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2608 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2609 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2610 | `		}` |
|     181 |  2611 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2612 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2613 | `			return SXERR_ABORT;` |
|       - |  2614 | `		}` |
|     181 |  2615 | `		pGen->pIn = pResEnd;` |
|     181 |  2616 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2617 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2618 | `		}` |
|     181 |  2619 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2620 | `	}` |
|      69 |  2621 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2622 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2623 | `	return SXRET_OK;` |
|      40 |  2624 |  |
|       - |  2625 | `/*` |
|       - |  2626 | ` * Compile a backtick quoted string.` |
|       - |  2627 | ` */` |
|       4 |  2628 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2629 |  |
|       - |  2630 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2631 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2632 | `	 */` |
|       8 |  2633 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2634 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2635 | `		ph7_lib_version()` |
|       - |  2636 | `		);` |
|       - |  2637 | `	/* Load NULL */` |
|       6 |  2638 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2639 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2640 | `	/* Node successfully compiled */` |
|       6 |  2641 | `	return SXRET_OK;` |
|       2 |  2642 |  |
|       - |  2643 | `/*` |
|       - |  2644 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2645 | ` * construct.` |
|       - |  2646 | ` */` |
|      80 |  2647 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2648 |  |
|       - |  2649 | `	SyString *pName;` |
|       - |  2650 | `	sxu32 nKeyID;` |
|       - |  2651 | `	sxi32 rc;` |
|       - |  2652 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      85 |  2653 | `	pName = &pGen->pIn->sData;` |
|      85 |  2654 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  2655 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      85 |  2656 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2657 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2658 | `		/* Compile arguments one after one */` |
|       9 |  2659 | `		pTmp = pGen->pEnd;` |
|       - |  2660 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2661 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2662 | `		 *  mean that the following expression is valid:` |
|       - |  2663 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2664 | `		 */` |
|       9 |  2665 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2666 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2667 | `			if( pGen->pIn < pNext ){` |
|       9 |  2668 | `				pGen->pEnd = pNext;` |
|       9 |  2669 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2670 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2671 | `					return SXERR_ABORT;` |
|       - |  2672 | `				}` |
|       9 |  2673 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2674 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2675 | `					 * without the overhead of a function call.` |
|       - |  2676 | `					 * This is a very powerful optimization that improve` |
|       - |  2677 | `					 * performance greatly.` |
|       - |  2678 | `					 */` |
|       9 |  2679 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2680 | `				}` |
|       4 |  2681 | `			}` |
|       - |  2682 | `			/* Jump trailing commas */` |
|       9 |  2683 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2684 | `				pNext++;` |
|     ! 0 |  2685 | `			}` |
|       9 |  2686 | `			pGen->pIn = pNext;` |
|       1 |  2687 | `		}` |
|       - |  2688 | `		/* Restore token stream */` |
|       9 |  2689 | `		pGen->pEnd = pTmp;` |
|       5 |  2690 | `	}else{` |
|      77 |  2691 | `		sxi32 nArg = 0;` |
|      77 |  2692 | `		sxu32 nIdx = 0;` |
|      77 |  2693 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      77 |  2694 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2695 | `			return SXERR_ABORT;` |
|      77 |  2696 | `		}else if(rc != SXERR_EMPTY ){` |
|      77 |  2697 | `			nArg = 1;` |
|      36 |  2698 | `		}` |
|      77 |  2699 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2700 | `			ph7_value *pObj;` |
|       - |  2701 | `			/* Emit the call instruction */` |
|      29 |  2702 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  2703 | `			if( pObj == 0 ){` |
|     ! 0 |  2704 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2705 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2706 | `				return SXERR_ABORT;` |
|       - |  2707 | `			}` |
|      29 |  2708 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2709 | `			/* Install in the literal table */` |
|      29 |  2710 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      12 |  2711 | `		}` |
|       - |  2712 | `		/* Emit the call instruction */` |
|      77 |  2713 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      77 |  2714 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2715 | `	}` |
|       - |  2716 | `	/* Node successfully compiled */` |
|      85 |  2717 | `	return SXRET_OK;` |
|      45 |  2718 |  |
|       - |  2719 | `/*` |
|       - |  2720 | ` * Compile a node holding a variable declaration.` |
|       - |  2721 | ` * According to the PHP language reference` |
|       - |  2722 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2723 | ` *  The variable name is case-sensitive.` |
|       - |  2724 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2725 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2726 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2727 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2728 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2729 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2730 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2731 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2732 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2733 | ` *  the chapter on Expressions.` |
|       - |  2734 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2735 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2736 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2737 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2738 | ` *  is being assigned (the source variable).` |
|       - |  2739 | ` */` |
| 1035784 |  2740 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2741 |  |
| 1035789 |  2742 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2743 | `	sxi32 iVv;` |
|       - |  2744 | `	sxi32 iP1;` |
|       - |  2745 | `	void *p3;` |
|       - |  2746 | `	sxi32 rc;` |
| 1035789 |  2747 | `	iVv = -1; /* Variable variable counter */` |
| 2071585 |  2748 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1035801 |  2749 | `		pGen->pIn++;` |
| 1035801 |  2750 | `		iVv++;` |
|       5 |  2751 | `	}` |
| 1035789 |  2752 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2753 | `		/* Invalid variable name */` |
|     ! 0 |  2754 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2755 | `		if( rc == SXERR_ABORT ){` |
|       - |  2756 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2757 | `			return SXERR_ABORT;` |
|       - |  2758 | `		}` |
|     ! 0 |  2759 | `		return SXRET_OK;` |
|       - |  2760 | `	}` |
| 1035789 |  2761 | `	p3  = 0;` |
| 1035789 |  2762 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2763 | `		/* Dynamic variable creation */` |
|      19 |  2764 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  2765 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  2766 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2767 | `			/* Empty expression */` |
|       3 |  2768 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2769 | `			return SXRET_OK;` |
|       - |  2770 | `		}` |
|       - |  2771 | `		/* Compile the expression holding the variable name */` |
|      16 |  2772 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2773 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2774 | `			return SXERR_ABORT;` |
|      16 |  2775 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2776 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2777 | `			return SXRET_OK;` |
|       - |  2778 | `		}` |
|       7 |  2779 | `	}else{` |
|       - |  2780 | `		SyHashEntry *pEntry;` |
|       - |  2781 | `		SyString *pName;` |
| 1035773 |  2782 | `		char *zName = 0;` |
|       - |  2783 | `		/* Extract variable name */` |
| 1035773 |  2784 | `		pName = &pGen->pIn->sData;` |
|       - |  2785 | `		/* Advance the stream cursor */` |
| 1035773 |  2786 | `		pGen->pIn++;` |
| 1035773 |  2787 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1035773 |  2788 | `		if( pEntry == 0 ){` |
|       - |  2789 | `			/* Duplicate name */` |
|  139025 |  2790 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  139025 |  2791 | `			if( zName == 0 ){` |
|     ! 0 |  2792 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2793 | `				return SXERR_ABORT;` |
|       - |  2794 | `			}` |
|       - |  2795 | `			/* Install in the hashtable */` |
|  139025 |  2796 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   69515 |  2797 | `		}else{` |
|       - |  2798 | `			/* Name already available */` |
|  896753 |  2799 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2800 | `		}` |
| 1035773 |  2801 | `		p3 = (void *)zName;` |
|       - |  2802 | `	}` |
| 1035785 |  2803 | `	iP1 = 0;` |
| 1035785 |  2804 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  377303 |  2805 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2806 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  377285 |  2807 | `			iP1 = 1;` |
|  188640 |  2808 | `		}` |
|  188649 |  2809 | `	}` |
|       - |  2810 | `	/* Emit the load instruction */` |
| 1035785 |  2811 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1035797 |  2812 | `	while( iVv > 0 ){` |
|      13 |  2813 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2814 | `		iVv--;` |
|       1 |  2815 | `	}` |
|       - |  2816 | `	/* Node successfully compiled */` |
| 1035785 |  2817 | `	return SXRET_OK;` |
|  517897 |  2818 |  |
|       - |  2819 | `/*` |
|       - |  2820 | ` * Load a literal.` |
|       - |  2821 | ` */` |
|  728986 |  2822 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2823 |  |
|  728991 |  2824 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2825 | `	ph7_value *pObj;` |
|       - |  2826 | `	SyString *pStr;` |
|       - |  2827 | `	sxu32 nIdx;` |
|       - |  2828 | `	/* Extract token value */` |
|  728991 |  2829 | `	pStr = &pToken->sData;` |
|       - |  2830 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  728991 |  2831 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  154507 |  2832 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2833 | `			/* NULL constant are always indexed at 0 */` |
|   56923 |  2834 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   56923 |  2835 | `			return SXRET_OK;` |
|   97589 |  2836 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2837 | `			/* TRUE constant are always indexed at 1 */` |
|     647 |  2838 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     647 |  2839 | `			return SXRET_OK;` |
|       5 |  2840 | `		}` |
|  680683 |  2841 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  115446 |  2842 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2843 | `			/* FALSE constant are always indexed at 2 */` |
|   43649 |  2844 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   43649 |  2845 | `			return SXRET_OK;` |
|  582631 |  2846 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  103572 |  2847 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2848 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    9929 |  2849 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    9929 |  2850 | `			if( pObj == 0 ){` |
|     ! 0 |  2851 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2852 | `				return SXERR_ABORT;` |
|       - |  2853 | `			}` |
|    9929 |  2854 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2855 | `			/* Emit the load constant instruction */` |
|    9929 |  2856 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    9929 |  2857 | `			return SXRET_OK;` |
|  537655 |  2858 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   33468 |  2859 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2860 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       8 |  2861 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       8 |  2862 | `			if( pObj == 0 ){` |
|     ! 0 |  2863 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2864 | `				return SXERR_ABORT;` |
|       - |  2865 | `			}` |
|       8 |  2866 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2867 | `				SyString sNs;` |
|       8 |  2868 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  2869 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       5 |  2870 | `			}else{` |
|     ! 0 |  2871 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2872 | `			}` |
|       8 |  2873 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       8 |  2874 | `			return SXRET_OK;` |
|  536761 |  2875 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   13983 |  2876 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  529765 |  2877 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   17724 |  2878 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  2879 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  2880 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  2881 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  2882 | `				/* Point to the upper block */` |
|      11 |  2883 | `				pBlock = pBlock->pParent;` |
|       1 |  2884 | `			}` |
|      11 |  2885 | `			if( pBlock == 0 ){` |
|       - |  2886 | `				/* Called in the global scope,load NULL */` |
|       5 |  2887 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  2888 | `			}else{` |
|       - |  2889 | `				/* Extract the target function/method */` |
|       7 |  2890 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  2891 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  2892 | `					/* Not a class method,Load null */` |
|       3 |  2893 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2894 | `				}else{` |
|       5 |  2895 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  2896 | `					if( pObj == 0 ){` |
|     ! 0 |  2897 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2898 | `						return SXERR_ABORT;` |
|       - |  2899 | `					}` |
|       5 |  2900 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  2901 | `					/* Emit the load constant instruction */` |
|       5 |  2902 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  2903 | `				}` |
|       - |  2904 | `			}` |
|      11 |  2905 | `			return SXRET_OK;` |
|       - |  2906 | `	}` |
|       - |  2907 | `	/* Query literal table */` |
|  617847 |  2908 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2909 | `		ph7_value *pLitObj;` |
|       - |  2910 | `		/* Unknown literal,install it in the literal table */` |
|  256519 |  2911 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  256519 |  2912 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2913 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2914 | `			return SXERR_ABORT;` |
|       - |  2915 | `		}` |
|  256519 |  2916 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  256519 |  2917 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  128257 |  2918 | `	}` |
|       - |  2919 | `	/* Emit the load constant instruction */` |
|  617847 |  2920 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  617847 |  2921 | `	return SXRET_OK;` |
|  364498 |  2922 |  |
|       - |  2923 | `/*` |
|       - |  2924 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2925 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2926 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2927 | ` * Otherwise, load the simple literal directly.` |
|       - |  2928 | ` */` |
|  729024 |  2929 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  2930 |  |
|       - |  2931 | `	sxi32 rc;` |
|  729029 |  2932 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2933 | `		return SXRET_OK;` |
|       - |  2934 | `	}` |
|       - |  2935 | `	/* Check if this is a multi-token namespace path */` |
|  729029 |  2936 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  2937 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      43 |  2938 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      43 |  2939 | `		int isAbsolute = 0;` |
|      43 |  2940 | `		SyBlobReset(pWorker);` |
|       - |  2941 | `		/* Check for leading backslash (absolute path) */` |
|      43 |  2942 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      41 |  2943 | `			isAbsolute = 1;` |
|      41 |  2944 | `			pGen->pIn++; /* Skip leading backslash */` |
|      18 |  2945 | `		}` |
|       - |  2946 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      43 |  2947 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  2948 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  2949 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  2950 | `		}` |
|       - |  2951 | `		/* Collect all path components */` |
|     139 |  2952 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     139 |  2953 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      53 |  2954 | `				SyBlobAppend(pWorker,"\\",1);` |
|      29 |  2955 | `			}else{` |
|      91 |  2956 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  2957 | `			}` |
|     139 |  2958 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      43 |  2959 | `				pGen->pIn++;` |
|      43 |  2960 | `				break;` |
|       - |  2961 | `			}` |
|     101 |  2962 | `			pGen->pIn++;` |
|       5 |  2963 | `		}` |
|      43 |  2964 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  2965 | `			ph7_value *pObj;` |
|       - |  2966 | `			SyString sPath;` |
|       - |  2967 | `			sxu32 nIdx;` |
|      43 |  2968 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  2969 | `			/* Install in the literal table */` |
|      43 |  2970 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      21 |  2971 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      21 |  2972 | `				if( pObj == 0 ){` |
|     ! 0 |  2973 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2974 | `					return SXERR_ABORT;` |
|       - |  2975 | `				}` |
|      21 |  2976 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      21 |  2977 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 |  2978 | `			}` |
|       - |  2979 | `			/* Emit the load constant instruction.` |
|       - |  2980 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  2981 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      62 |  2982 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      19 |  2983 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      19 |  2984 | `				nIdx,0,0);` |
|      43 |  2985 | `			return SXRET_OK;` |
|       - |  2986 | `		}` |
|     ! 0 |  2987 | `	}` |
|       - |  2988 | `	/* Single-token literal: load directly */` |
|  728991 |  2989 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  728991 |  2990 | `	return rc;` |
|  364517 |  2991 |  |
|       - |  2992 | `/*` |
|       - |  2993 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2994 | ` */` |
|  729024 |  2995 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2996 |  |
|       - |  2997 | `	sxi32 rc;` |
|  729029 |  2998 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  729029 |  2999 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3000 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3001 | `		return rc;` |
|       - |  3002 | `	}` |
|       - |  3003 | `	/* Node successfully compiled */` |
|  729029 |  3004 | `	return SXRET_OK;` |
|  364517 |  3005 |  |
|       - |  3006 | `/*` |
|       - |  3007 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3008 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3009 | ` */` |
|       8 |  3010 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3011 |  |
|       - |  3012 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3013 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3014 | `		pGen->pIn++;` |
|       1 |  3015 | `	}` |
|       9 |  3016 | `	return SXRET_OK;` |
|       1 |  3017 |  |
|       - |  3018 | `/*` |
|       - |  3019 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3020 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3021 | ` */` |
|     104 |  3022 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3023 |  |
|     109 |  3024 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      29 |  3025 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3026 | `			return TRUE;` |
|      27 |  3027 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3028 | `			return TRUE;` |
|       2 |  3029 | `		}` |
|      93 |  3030 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3031 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3032 | `			return TRUE;` |
|       - |  3033 | `		}` |
|     ! 0 |  3034 | `	}` |
|       - |  3035 | `	/* Not a reserved constant */` |
|     101 |  3036 | `	return FALSE;` |
|      57 |  3037 |  |
|       - |  3038 | `/*` |
|       - |  3039 | ` * Compile the 'const' statement.` |
|       - |  3040 | ` * According to the PHP language reference` |
|       - |  3041 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3042 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3043 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3044 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3045 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3046 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3047 | ` *  Syntax` |
|       - |  3048 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3049 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3050 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3051 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3052 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3053 | ` *  to get a list of all defined constants.` |
|       - |  3054 | ` *` |
|       - |  3055 | ` * Symisc eXtension.` |
|       - |  3056 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3057 | ` *  would allow only simple scalar value.` |
|       - |  3058 | ` *  Example` |
|       - |  3059 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3060 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3061 | ` */` |
|      32 |  3062 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3063 |  |
|       - |  3064 | `	SySet *pConsCode,*pInstrContainer;` |
|      37 |  3065 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3066 | `	SyString *pName;` |
|       - |  3067 | `	sxi32 rc;` |
|      37 |  3068 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      37 |  3069 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3070 | `		/* Invalid constant name */` |
|       9 |  3071 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       9 |  3072 | `		if( rc == SXERR_ABORT ){` |
|       - |  3073 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3074 | `			return SXERR_ABORT;` |
|       - |  3075 | `		}` |
|       9 |  3076 | `		goto Synchronize;` |
|       - |  3077 | `	}` |
|       - |  3078 | `	/* Peek constant name */` |
|      30 |  3079 | `	pName = &pGen->pIn->sData;` |
|       - |  3080 | `	/* Make sure the constant name isn't reserved */` |
|      30 |  3081 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3082 | `		/* Reserved constant */` |
|      10 |  3083 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3084 | `		if( rc == SXERR_ABORT ){` |
|       - |  3085 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3086 | `			return SXERR_ABORT;` |
|       - |  3087 | `		}` |
|      10 |  3088 | `		goto Synchronize;` |
|       - |  3089 | `	}` |
|      21 |  3090 | `	pGen->pIn++;` |
|      21 |  3091 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3092 | `		/* Invalid statement*/` |
|       6 |  3093 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3094 | `		if( rc == SXERR_ABORT ){` |
|       - |  3095 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3096 | `			return SXERR_ABORT;` |
|       - |  3097 | `		}` |
|       6 |  3098 | `		goto Synchronize;` |
|       - |  3099 | `	}` |
|      15 |  3100 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3101 | `	/* Allocate a new constant value container */` |
|      15 |  3102 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3103 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3104 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3105 | `		return SXERR_ABORT;` |
|       - |  3106 | `	}` |
|      15 |  3107 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3108 | `	/* Swap bytecode container */` |
|      15 |  3109 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3110 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3111 | `	/* Compile constant value */` |
|      15 |  3112 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3113 | `	/* Emit the done instruction */` |
|      15 |  3114 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3115 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3116 | `	if( rc == SXERR_ABORT ){` |
|       - |  3117 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3118 | `		return SXERR_ABORT;` |
|       - |  3119 | `	}` |
|      15 |  3120 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3121 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3122 | `	{` |
|       - |  3123 | `		SyBlob sFQN;` |
|       - |  3124 | `		SyString sFQNStr;` |
|      15 |  3125 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3126 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3127 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3128 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3129 | `		SyBlobRelease(&sFQN);` |
|       - |  3130 | `	}` |
|      15 |  3131 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3132 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3133 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3134 | `	}` |
|      15 |  3135 | `	return SXRET_OK;` |
|       9 |  3136 | `Synchronize:` |
|       - |  3137 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3138 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      42 |  3139 | `		pGen->pIn++;` |
|       4 |  3140 | `	}` |
|      22 |  3141 | `	return SXRET_OK;` |
|      21 |  3142 |  |
|       - |  3143 | `/*` |
|       - |  3144 | ` * Compile the 'continue' statement.` |
|       - |  3145 | ` * According to the PHP language reference` |
|       - |  3146 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3147 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3148 | ` *  iteration.` |
|       - |  3149 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3150 | ` *  the purposes of continue.` |
|       - |  3151 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3152 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3153 | ` *  Note:` |
|       - |  3154 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3155 | ` */` |
|       - |  3156 | `/*` |
|       - |  3157 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3158 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3159 | ` * break/continue crosses a try boundary.` |
|       - |  3160 | ` *` |
|       - |  3161 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3162 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3163 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3164 | ` */` |
|    3446 |  3165 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3166 |  |
|    3451 |  3167 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   20189 |  3168 | `	while( pBlock && pBlock != pTarget ){` |
|   16743 |  3169 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3170 | `			if( pBlock->pUserData ){` |
|       - |  3171 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3172 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3173 | `			}else{` |
|       - |  3174 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3175 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3176 | `				 * exception context from a sub-execution.` |
|       - |  3177 | `				 */` |
|     ! 0 |  3178 | `				break;` |
|       - |  3179 | `			}` |
|       1 |  3180 | `		}` |
|   16743 |  3181 | `		pBlock = pBlock->pParent;` |
|       5 |  3182 | `	}` |
|    3451 |  3183 |  |
|    3350 |  3184 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3185 |  |
|       - |  3186 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3187 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3188 | `	sxu32 nLineLocal;` |
|       - |  3189 | `	sxi32 rc;` |
|    3355 |  3190 | `	nLineLocal = pGen->pIn->nLine;` |
|    3355 |  3191 | `	iLevel = 0;` |
|       - |  3192 | `	/* Jump the 'continue' keyword */` |
|    3355 |  3193 | `	pGen->pIn++;` |
|    3355 |  3194 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3195 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3196 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3197 | `		 */` |
|       - |  3198 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3199 | `		char *zAlloc = 0;` |
|       - |  3200 | `		SyString sNum;` |
|      17 |  3201 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3202 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3203 | `			return SXERR_ABORT;` |
|       - |  3204 | `		}` |
|      17 |  3205 | `		if( rc == SXRET_OK ){` |
|      20 |  3206 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3207 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3208 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3209 | `				return SXERR_ABORT;` |
|       - |  3210 | `			}` |
|      14 |  3211 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3212 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3213 | `		}` |
|      17 |  3214 | `		if( iLevel < 2 ){` |
|       3 |  3215 | `			iLevel = 0;` |
|       1 |  3216 | `		}` |
|      17 |  3217 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3218 | `	}` |
|       - |  3219 | `	/* Point to the target loop */` |
|    3355 |  3220 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3355 |  3221 | `	if( pLoop == 0 ){` |
|       - |  3222 | `		/* Illegal continue */` |
|      12 |  3223 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      12 |  3224 | `		if( rc == SXERR_ABORT ){` |
|       - |  3225 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3226 | `			return SXERR_ABORT;` |
|       - |  3227 | `		}` |
|       7 |  3228 | `	}else{` |
|    3345 |  3229 | `		sxu32 nInstrIdx = 0;` |
|       - |  3230 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3345 |  3231 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3345 |  3232 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3233 | `			/* According to the PHP language reference manual` |
|       - |  3234 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3235 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3236 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3237 | `			 */` |
|       5 |  3238 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3239 | `			if( rc == SXRET_OK ){` |
|       5 |  3240 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3241 | `			}` |
|       3 |  3242 | `		}else{` |
|       - |  3243 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3341 |  3244 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3341 |  3245 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3246 | `				JumpFixup sJumpFix;` |
|       - |  3247 | `				/* Post-continue */` |
|      14 |  3248 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3249 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3250 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3251 | `			}` |
|       - |  3252 | `		}` |
|       - |  3253 | `	}` |
|    3355 |  3254 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3255 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3256 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3257 | `	}` |
|       - |  3258 | `	/* Statement successfully compiled */` |
|    3355 |  3259 | `	return SXRET_OK;` |
|    1680 |  3260 |  |
|       - |  3261 | `/*` |
|       - |  3262 | ` * Compile the 'break' statement.` |
|       - |  3263 | ` * According to the PHP language reference` |
|       - |  3264 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3265 | ` *  structure.` |
|       - |  3266 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3267 | ` *  enclosing structures are to be broken out of.` |
|       - |  3268 | ` */` |
|     122 |  3269 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3270 |  |
|       - |  3271 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3272 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3273 | `	sxi32 rc;` |
|     127 |  3274 | `	iLevel = 0;` |
|       - |  3275 | `	/* Jump the 'break' keyword */` |
|     127 |  3276 | `	pGen->pIn++;` |
|     127 |  3277 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3278 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3279 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3280 | `		 */` |
|       - |  3281 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3282 | `		char *zAlloc = 0;` |
|       - |  3283 | `		SyString sNum;` |
|      17 |  3284 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3285 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3286 | `			return SXERR_ABORT;` |
|       - |  3287 | `		}` |
|      17 |  3288 | `		if( rc == SXRET_OK ){` |
|      20 |  3289 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3290 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3291 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3292 | `				return SXERR_ABORT;` |
|       - |  3293 | `			}` |
|      14 |  3294 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3295 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3296 | `		}` |
|      17 |  3297 | `		if( iLevel < 2 ){` |
|       3 |  3298 | `			iLevel = 0;` |
|       1 |  3299 | `		}` |
|      17 |  3300 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3301 | `	}` |
|       - |  3302 | `	/* Extract the target loop */` |
|     127 |  3303 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     127 |  3304 | `	if( pLoop == 0 ){` |
|       - |  3305 | `		/* Illegal break */` |
|      19 |  3306 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      19 |  3307 | `		if( rc == SXERR_ABORT ){` |
|       - |  3308 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3309 | `			return SXERR_ABORT;` |
|       - |  3310 | `		}` |
|      11 |  3311 | `	}else{` |
|       - |  3312 | `		sxu32 nInstrIdx;` |
|       - |  3313 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|     111 |  3314 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|     111 |  3315 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|     111 |  3316 | `		if( rc == SXRET_OK ){` |
|       - |  3317 | `			/* Fix the jump later when the jump destination is resolved */` |
|     111 |  3318 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      53 |  3319 | `		}` |
|       - |  3320 | `	}` |
|     127 |  3321 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3322 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3323 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3324 | `	}` |
|       - |  3325 | `	/* Statement successfully compiled */` |
|     127 |  3326 | `	return SXRET_OK;` |
|      66 |  3327 |  |
|       - |  3328 | `/*` |
|       - |  3329 | ` * Compile or record a label.` |
|       - |  3330 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3331 | ` * Example` |
|       - |  3332 | ` *  goto LABEL;` |
|       - |  3333 | ` *   echo 'Foo';` |
|       - |  3334 | ` *  LABEL:` |
|       - |  3335 | ` *   echo 'Bar';` |
|       - |  3336 | ` */` |
|     112 |  3337 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3338 |  |
|       - |  3339 | `	GenBlock *pBlock;` |
|       - |  3340 | `	Label sLabel;` |
|       - |  3341 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3342 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3343 | `	if( pBlock ){` |
|       - |  3344 | `		sxi32 rc;` |
|       8 |  3345 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3346 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3347 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3348 | `			return SXERR_ABORT;` |
|       - |  3349 | `		}` |
|       4 |  3350 | `	}else{` |
|     113 |  3351 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3352 | `		char *zDup;` |
|       - |  3353 | `		/* Initialize label fields */` |
|     113 |  3354 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3355 | `		/* Duplicate label name */` |
|     113 |  3356 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3357 | `		if( zDup == 0 ){` |
|     ! 0 |  3358 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3359 | `			return SXERR_ABORT;` |
|       - |  3360 | `		}` |
|     113 |  3361 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3362 | `		sLabel.bRef  = FALSE;` |
|     113 |  3363 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3364 | `		pBlock = pGen->pCurrent;` |
|     221 |  3365 | `		while( pBlock ){` |
|     133 |  3366 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      23 |  3367 | `				break;` |
|       - |  3368 | `			}` |
|       - |  3369 | `			/* Point to the upper block */` |
|     113 |  3370 | `			pBlock = pBlock->pParent;` |
|       5 |  3371 | `		}` |
|     113 |  3372 | `		if( pBlock ){` |
|      23 |  3373 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      13 |  3374 | `		}else{` |
|      93 |  3375 | `			sLabel.pFunc = 0;` |
|       - |  3376 | `		}` |
|       - |  3377 | `		/* Insert in label set */` |
|     113 |  3378 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3379 | `	}` |
|     117 |  3380 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3381 | `	return SXRET_OK;` |
|      61 |  3382 |  |
|       - |  3383 | `/*` |
|       - |  3384 | ` * Compile the so hated 'goto' statement.` |
|       - |  3385 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3386 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3387 | ` * a compiler it has to do this.` |
|       - |  3388 | ` * According to the PHP language reference manual` |
|       - |  3389 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3390 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3391 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3392 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3393 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3394 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3395 | ` *   of a multi-level break` |
|       - |  3396 | ` */` |
|     152 |  3397 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3398 |  |
|       - |  3399 | `	JumpFixup sJump;` |
|       - |  3400 | `	sxi32 rc;` |
|     157 |  3401 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3402 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3403 | `		/* Missing label */` |
|     ! 0 |  3404 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3405 | `		if( rc == SXERR_ABORT ){` |
|       - |  3406 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3407 | `			return SXERR_ABORT;` |
|       - |  3408 | `		}` |
|     ! 0 |  3409 | `		return SXRET_OK;` |
|       - |  3410 | `	}` |
|     157 |  3411 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  3412 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       6 |  3413 | `		if( rc == SXERR_ABORT ){` |
|       - |  3414 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3415 | `			return SXERR_ABORT;` |
|       - |  3416 | `		}` |
|       4 |  3417 | `	}else{` |
|     153 |  3418 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3419 | `		GenBlock *pBlock;` |
|       - |  3420 | `		char *zDup;` |
|       - |  3421 | `		/* Prepare the jump destination */` |
|     153 |  3422 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3423 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3424 | `		/* Duplicate label name */` |
|     153 |  3425 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3426 | `		if( zDup == 0 ){` |
|     ! 0 |  3427 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3428 | `			return SXERR_ABORT;` |
|       - |  3429 | `		}` |
|     153 |  3430 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3431 | `		pBlock = pGen->pCurrent;` |
|     315 |  3432 | `		while( pBlock ){` |
|     199 |  3433 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      37 |  3434 | `				break;` |
|       - |  3435 | `			}` |
|       - |  3436 | `			/* Point to the upper block */` |
|     167 |  3437 | `			pBlock = pBlock->pParent;` |
|       5 |  3438 | `		}` |
|     153 |  3439 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       9 |  3440 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       9 |  3441 | `			if( rc == SXERR_ABORT ){` |
|       - |  3442 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3443 | `				return SXERR_ABORT;` |
|       - |  3444 | `			}` |
|       3 |  3445 | `		}` |
|     153 |  3446 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      30 |  3447 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      17 |  3448 | `		}else{` |
|     127 |  3449 | `			sJump.pFunc = 0;` |
|       - |  3450 | `		}` |
|       - |  3451 | `		/* Emit the unconditional jump */` |
|     153 |  3452 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3453 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3454 | `		}` |
|       - |  3455 | `	}` |
|     157 |  3456 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3457 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3458 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3459 | `	}` |
|       - |  3460 | `	/* Statement successfully compiled */` |
|     157 |  3461 | `	return SXRET_OK;` |
|      81 |  3462 |  |
|       - |  3463 | `/*` |
|       - |  3464 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3465 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3466 | ` * failure.` |
|       - |  3467 | ` */` |
|      20 |  3468 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  3469 |  |
|       - |  3470 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3471 | `	sxu32 nRawObj;` |
|      10 |  3472 | `	sxu32 nObjIdx;` |
|       - |  3473 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3474 | `	 * a PHP block.` |
|       - |  3475 | `	 */` |
|      10 |  3476 | `Consume:` |
|      22 |  3477 | `	nRawObj = nObjIdx = 0;` |
|      22 |  3478 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3479 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3480 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3481 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3482 | `			return SXERR_ABORT;` |
|       - |  3483 | `		}` |
|       - |  3484 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3485 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3487 | `		++nRawObj;` |
|     ! 0 |  3488 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3489 | `	}` |
|      22 |  3490 | `	if( nRawObj > 0 ){` |
|       - |  3491 | `		/* Emit the consume instruction */` |
|     ! 0 |  3492 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3493 | `	}` |
|      22 |  3494 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3495 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3496 | `		/* Reset the token set */` |
|     ! 0 |  3497 | `		SySetReset(pTokenSet);` |
|       - |  3498 | `		/* Tokenize input */` |
|     ! 0 |  3499 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3500 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3501 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3502 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3503 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3504 | `		/* Advance the stream cursor */` |
|     ! 0 |  3505 | `		pGen->pRawIn++;` |
|       - |  3506 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3507 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3508 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3509 | `			sxi32 rc;` |
|       - |  3510 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3511 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3512 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3513 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3514 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3515 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3516 | `				return SXERR_ABORT;` |
|     ! 0 |  3517 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3518 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3519 | `			}` |
|     ! 0 |  3520 | `			goto Consume;` |
|       - |  3521 | `		}` |
|     ! 0 |  3522 | `	}else{` |
|       - |  3523 | `		/* No more chunks to process */` |
|      22 |  3524 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  3525 | `		return SXERR_EOF;` |
|       - |  3526 | `	}` |
|     ! 0 |  3527 | `	return SXRET_OK;` |
|      12 |  3528 |  |
|       - |  3529 | `/*` |
|       - |  3530 | ` * Compile a PHP block.` |
|       - |  3531 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3532 | ` * optionally delimited by braces {}.` |
|       - |  3533 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3534 | ` * and this function takes care of generating the appropriate error` |
|       - |  3535 | ` * message.` |
|       - |  3536 | ` */` |
|  401298 |  3537 | `static sxi32 PH7_CompileBlock(` |
|       - |  3538 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3539 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3540 | `	)` |
|       5 |  3541 |  |
|       - |  3542 | `	sxi32 rc;` |
|       - |  3543 | `	sxu32 nLine;` |
|  401303 |  3544 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  399657 |  3545 | `		nLine = pGen->pIn->nLine;` |
|  399657 |  3546 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  399657 |  3547 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3548 | `			return SXERR_ABORT;` |
|       - |  3549 | `		}` |
|  399657 |  3550 | `		pGen->pIn++;` |
|       - |  3551 | `		/* Compile until we hit the closing braces '}' */` |
|  545847 |  3552 | `		for(;;){` |
| 1091699 |  3553 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  3554 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  3555 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3556 | `			 	   return SXERR_ABORT;` |
|       - |  3557 | `				}` |
|      22 |  3558 | `				if( rc == SXERR_EOF ){` |
|       - |  3559 | `					/* No more token to process. Missing closing braces */` |
|      22 |  3560 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      22 |  3561 | `					break;` |
|       - |  3562 | `				}` |
|     ! 0 |  3563 | `			}` |
| 1091679 |  3564 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3565 | `				/* Closing braces found,break immediately*/` |
|  399637 |  3566 | `				pGen->pIn++;` |
|  399637 |  3567 | `				break;` |
|       - |  3568 | `			}` |
|       - |  3569 | `			/* Compile a single statement */` |
|  692047 |  3570 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  692047 |  3571 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3572 | `				return SXERR_ABORT;` |
|       - |  3573 | `			}` |
|       5 |  3574 | `		}` |
|  399657 |  3575 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  201477 |  3576 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3577 | `		pGen->pIn++;` |
|     ! 0 |  3578 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3579 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3580 | `			return SXERR_ABORT;` |
|       - |  3581 | `		}` |
|       - |  3582 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3583 | `		for(;;){` |
|     ! 0 |  3584 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3585 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3586 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3587 | `			 	   return SXERR_ABORT;` |
|       - |  3588 | `				}` |
|     ! 0 |  3589 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3590 | `					/* No more token to process */` |
|     ! 0 |  3591 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3592 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3593 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3594 | `					}` |
|     ! 0 |  3595 | `					break;` |
|       - |  3596 | `				}` |
|     ! 0 |  3597 | `			}` |
|     ! 0 |  3598 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3599 | `				sxi32 nKwrd;` |
|       - |  3600 | `				/* Keyword found */` |
|     ! 0 |  3601 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3602 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3603 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3604 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3605 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3606 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3607 | `						}` |
|     ! 0 |  3608 | `						break;` |
|       - |  3609 | `				}` |
|     ! 0 |  3610 | `			}` |
|       - |  3611 | `			/* Compile a single statement */` |
|     ! 0 |  3612 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3613 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3614 | `				return SXERR_ABORT;` |
|       - |  3615 | `			}` |
|     ! 0 |  3616 | `		}` |
|     ! 0 |  3617 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3618 | `	}else{` |
|       - |  3619 | `		/* Compile a single statement */` |
|    1651 |  3620 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1651 |  3621 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3622 | `			return SXERR_ABORT;` |
|       - |  3623 | `		}` |
|       - |  3624 | `	}` |
|       - |  3625 | `	/* Jump trailing semi-colons ';' */` |
|  401303 |  3626 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3627 | `		pGen->pIn++;` |
|     ! 0 |  3628 | `	}` |
|  401303 |  3629 | `	return SXRET_OK;` |
|  200654 |  3630 |  |
|       - |  3631 | `/*` |
|       - |  3632 | ` * Compile the gentle 'while' statement.` |
|       - |  3633 | ` * According to the PHP language reference` |
|       - |  3634 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3635 | ` *  The basic form of a while statement is:` |
|       - |  3636 | ` *  while (expr)` |
|       - |  3637 | ` *   statement` |
|       - |  3638 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3639 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3640 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3641 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3642 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3643 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3644 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3645 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3646 | ` *  while (expr):` |
|       - |  3647 | ` *    statement` |
|       - |  3648 | ` *   endwhile;` |
|       - |  3649 | ` */` |
|   13344 |  3650 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3651 |  |
|   13349 |  3652 | `	GenBlock *pWhileBlock = 0;` |
|   13349 |  3653 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3654 | `	sxu32 nFalseJump;` |
|       - |  3655 | `	sxu32 nLine;` |
|       - |  3656 | `	sxi32 rc;` |
|   13349 |  3657 | `	nLine = pGen->pIn->nLine;` |
|       - |  3658 | `	/* Jump the 'while' keyword */` |
|   13349 |  3659 | `	pGen->pIn++;` |
|   13349 |  3660 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3661 | `		/* Syntax error */` |
|     ! 0 |  3662 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3663 | `		if( rc == SXERR_ABORT ){` |
|       - |  3664 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3665 | `			return SXERR_ABORT;` |
|       - |  3666 | `		}` |
|     ! 0 |  3667 | `		goto Synchronize;` |
|       - |  3668 | `	}` |
|       - |  3669 | `	/* Jump the left parenthesis '(' */` |
|   13349 |  3670 | `	pGen->pIn++;` |
|       - |  3671 | `	/* Create the loop block */` |
|   13349 |  3672 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   13349 |  3673 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3674 | `		return SXERR_ABORT;` |
|       - |  3675 | `	}` |
|       - |  3676 | `	/* Delimit the condition */` |
|   13349 |  3677 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   13349 |  3678 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3679 | `		/* Empty expression */` |
|       3 |  3680 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3681 | `		if( rc == SXERR_ABORT ){` |
|       - |  3682 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3683 | `			return SXERR_ABORT;` |
|       - |  3684 | `		}` |
|       1 |  3685 | `	}` |
|       - |  3686 | `	/* Swap token streams */` |
|   13349 |  3687 | `	pTmp = pGen->pEnd;` |
|   13349 |  3688 | `	pGen->pEnd = pEnd;` |
|       - |  3689 | `	/* Compile the expression */` |
|   13349 |  3690 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   13349 |  3691 | `	if( rc == SXERR_ABORT ){` |
|       - |  3692 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3693 | `		return SXERR_ABORT;` |
|       - |  3694 | `	}` |
|       - |  3695 | `	/* Update token stream */` |
|   13349 |  3696 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3697 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3698 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3699 | `			return SXERR_ABORT;` |
|       - |  3700 | `		}` |
|     ! 0 |  3701 | `		pGen->pIn++;` |
|     ! 0 |  3702 | `	}` |
|       - |  3703 | `	/* Synchronize pointers */` |
|   13349 |  3704 | `	pGen->pIn  = &pEnd[1];` |
|   13349 |  3705 | `	pGen->pEnd = pTmp;` |
|       - |  3706 | `	/* Emit the false jump */` |
|   13349 |  3707 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3708 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   13349 |  3709 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3710 | `	/* Compile the loop body */` |
|   13349 |  3711 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   13349 |  3712 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3713 | `		return SXERR_ABORT;` |
|       - |  3714 | `	}` |
|       - |  3715 | `	/* Emit the unconditional jump to the start of the loop */` |
|   13349 |  3716 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3717 | `	/* Fix all jumps now the destination is resolved */` |
|   13349 |  3718 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3719 | `	/* Release the loop block */` |
|   13349 |  3720 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3721 | `	/* Statement successfully compiled */` |
|   13349 |  3722 | `	return SXRET_OK;` |
|     ! 0 |  3723 | `Synchronize:` |
|       - |  3724 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3725 | `	 * compiling this erroneous block.` |
|       - |  3726 | `	 */` |
|     ! 0 |  3727 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3728 | `		pGen->pIn++;` |
|     ! 0 |  3729 | `	}` |
|     ! 0 |  3730 | `	return SXRET_OK;` |
|    6677 |  3731 |  |
|       - |  3732 | `/*` |
|       - |  3733 | ` * Compile the ugly do..while() statement.` |
|       - |  3734 | ` * According to the PHP language reference` |
|       - |  3735 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3736 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3737 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3738 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3739 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3740 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3741 | ` *  would end immediately).` |
|       - |  3742 | ` *  There is just one syntax for do-while loops:` |
|       - |  3743 | ` *  <?php` |
|       - |  3744 | ` *  $i = 0;` |
|       - |  3745 | ` *  do {` |
|       - |  3746 | ` *   echo $i;` |
|       - |  3747 | ` *  } while ($i > 0);` |
|       - |  3748 | ` * ?>` |
|       - |  3749 | ` */` |
|       2 |  3750 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3751 |  |
|       3 |  3752 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3753 | `	GenBlock *pDoBlock = 0;` |
|       - |  3754 | `	sxu32 nLine;` |
|       - |  3755 | `	sxi32 rc;` |
|       3 |  3756 | `	nLine = pGen->pIn->nLine;` |
|       - |  3757 | `	/* Jump the 'do' keyword */` |
|       3 |  3758 | `	pGen->pIn++;` |
|       - |  3759 | `	/* Create the loop block */` |
|       3 |  3760 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3761 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3762 | `		return SXERR_ABORT;` |
|       - |  3763 | `	}` |
|       - |  3764 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3765 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3766 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3767 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3768 | `		return SXERR_ABORT;` |
|       - |  3769 | `	}` |
|       3 |  3770 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3771 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3772 | `	}` |
|       3 |  3773 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3774 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3775 | `			/* Missing 'while' statement */` |
|       3 |  3776 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3777 | `			if( rc == SXERR_ABORT ){` |
|       - |  3778 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3779 | `				return SXERR_ABORT;` |
|       - |  3780 | `			}` |
|       3 |  3781 | `			goto Synchronize;` |
|       - |  3782 | `	}` |
|       - |  3783 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3784 | `	pGen->pIn++;` |
|     ! 0 |  3785 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3786 | `		/* Syntax error */` |
|     ! 0 |  3787 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3788 | `		if( rc == SXERR_ABORT ){` |
|       - |  3789 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3790 | `			return SXERR_ABORT;` |
|       - |  3791 | `		}` |
|     ! 0 |  3792 | `		goto Synchronize;` |
|       - |  3793 | `	}` |
|       - |  3794 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3795 | `	pGen->pIn++;` |
|       - |  3796 | `	/* Delimit the condition */` |
|     ! 0 |  3797 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3798 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3799 | `		/* Empty expression */` |
|     ! 0 |  3800 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3801 | `		if( rc == SXERR_ABORT ){` |
|       - |  3802 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3803 | `			return SXERR_ABORT;` |
|       - |  3804 | `		}` |
|     ! 0 |  3805 | `		goto Synchronize;` |
|       - |  3806 | `	}` |
|       - |  3807 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3808 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3809 | `		JumpFixup *aPost;` |
|       - |  3810 | `		VmInstr *pInstr;` |
|       - |  3811 | `		sxu32 nJumpDest;` |
|       - |  3812 | `		sxu32 n;` |
|     ! 0 |  3813 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3814 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3815 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3816 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3817 | `			if( pInstr ){` |
|       - |  3818 | `				/* Fix */` |
|     ! 0 |  3819 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3820 | `			}` |
|     ! 0 |  3821 | `		}` |
|     ! 0 |  3822 | `	}` |
|       - |  3823 | `	/* Swap token streams */` |
|     ! 0 |  3824 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3825 | `	pGen->pEnd = pEnd;` |
|       - |  3826 | `	/* Compile the expression */` |
|     ! 0 |  3827 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3828 | `	if( rc == SXERR_ABORT ){` |
|       - |  3829 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3830 | `		return SXERR_ABORT;` |
|       - |  3831 | `	}` |
|       - |  3832 | `	/* Update token stream */` |
|     ! 0 |  3833 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3834 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3835 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3836 | `			return SXERR_ABORT;` |
|       - |  3837 | `		}` |
|     ! 0 |  3838 | `		pGen->pIn++;` |
|     ! 0 |  3839 | `	}` |
|     ! 0 |  3840 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3841 | `	pGen->pEnd = pTmp;` |
|       - |  3842 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3843 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3844 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3845 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3846 | `	/* Release the loop block */` |
|     ! 0 |  3847 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3848 | `	/* Statement successfully compiled */` |
|     ! 0 |  3849 | `	return SXRET_OK;` |
|       1 |  3850 | `Synchronize:` |
|       - |  3851 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3852 | `	 * compiling this erroneous block.` |
|       - |  3853 | `	 */` |
|       3 |  3854 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3855 | `		pGen->pIn++;` |
|     ! 0 |  3856 | `	}` |
|       3 |  3857 | `	return SXRET_OK;` |
|       2 |  3858 |  |
|       - |  3859 | `/*` |
|       - |  3860 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3861 | ` * According to the PHP language reference` |
|       - |  3862 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3863 | ` *  The syntax of a for loop is:` |
|       - |  3864 | ` *  for (expr1; expr2; expr3)` |
|       - |  3865 | ` *   statement` |
|       - |  3866 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3867 | ` *  the beginning of the loop.` |
|       - |  3868 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  3869 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  3870 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  3871 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  3872 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  3873 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  3874 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  3875 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  3876 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  3877 | ` *  of using the for truth expression.` |
|       - |  3878 | ` */` |
|   13344 |  3879 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  3880 |  |
|   13349 |  3881 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   13349 |  3882 | `	GenBlock *pForBlock = 0;` |
|       - |  3883 | `	sxu32 nFalseJump;` |
|       - |  3884 | `	sxu32 nLine;` |
|       - |  3885 | `	sxi32 rc;` |
|   13349 |  3886 | `	nLine = pGen->pIn->nLine;` |
|       - |  3887 | `	/* Jump the 'for' keyword */` |
|   13349 |  3888 | `	pGen->pIn++;` |
|   13349 |  3889 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3890 | `		/* Syntax error */` |
|     ! 0 |  3891 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  3892 | `		if( rc == SXERR_ABORT ){` |
|       - |  3893 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3894 | `			return SXERR_ABORT;` |
|       - |  3895 | `		}` |
|     ! 0 |  3896 | `		return SXRET_OK;` |
|       - |  3897 | `	}` |
|       - |  3898 | `	/* Jump the left parenthesis '(' */` |
|   13349 |  3899 | `	pGen->pIn++;` |
|       - |  3900 | `	/* Delimit the init-expr;condition;post-expr */` |
|   13349 |  3901 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   13349 |  3902 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3903 | `		/* Empty expression */` |
|     ! 0 |  3904 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  3905 | `		if( rc == SXERR_ABORT ){` |
|       - |  3906 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3907 | `			return SXERR_ABORT;` |
|       - |  3908 | `		}` |
|       - |  3909 | `		/* Synchronize */` |
|     ! 0 |  3910 | `		pGen->pIn = pEnd;` |
|     ! 0 |  3911 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3912 | `			pGen->pIn++;` |
|     ! 0 |  3913 | `		}` |
|     ! 0 |  3914 | `		return SXRET_OK;` |
|       - |  3915 | `	}` |
|       - |  3916 | `	/* Swap token streams */` |
|   13349 |  3917 | `	pTmp = pGen->pEnd;` |
|   13349 |  3918 | `	pGen->pEnd = pEnd;` |
|       - |  3919 | `	/* Compile initialization expressions if available */` |
|   13349 |  3920 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3921 | `	/* Pop operand lvalues */` |
|   13349 |  3922 | `	if( rc == SXERR_ABORT ){` |
|       - |  3923 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3924 | `		return SXERR_ABORT;` |
|   13349 |  3925 | `	}else if( rc != SXERR_EMPTY ){` |
|   13347 |  3926 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6671 |  3927 | `	}` |
|   13349 |  3928 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3929 | `		/* Syntax error */` |
|     ! 0 |  3930 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3931 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  3932 | `		if( rc == SXERR_ABORT ){` |
|       - |  3933 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3934 | `			return SXERR_ABORT;` |
|       - |  3935 | `		}` |
|     ! 0 |  3936 | `		return SXRET_OK;` |
|       - |  3937 | `	}` |
|       - |  3938 | `	/* Jump the trailing ';' */` |
|   13349 |  3939 | `	pGen->pIn++;` |
|       - |  3940 | `	/* Create the loop block */` |
|   13349 |  3941 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   13349 |  3942 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3943 | `		return SXERR_ABORT;` |
|       - |  3944 | `	}` |
|       - |  3945 | `	/* Deffer continue jumps */` |
|   13349 |  3946 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  3947 | `	/* Compile the condition */` |
|   13349 |  3948 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   13349 |  3949 | `	if( rc == SXERR_ABORT ){` |
|       - |  3950 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3951 | `		return SXERR_ABORT;` |
|   13349 |  3952 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  3953 | `		/* Emit the false jump */` |
|   13347 |  3954 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3955 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   13347 |  3956 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    6671 |  3957 | `	}` |
|   13349 |  3958 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3959 | `		/* Syntax error */` |
|       6 |  3960 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3961 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  3962 | `		if( rc == SXERR_ABORT ){` |
|       - |  3963 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3964 | `			return SXERR_ABORT;` |
|       - |  3965 | `		}` |
|       6 |  3966 | `		return SXRET_OK;` |
|       - |  3967 | `	}` |
|       - |  3968 | `	/* Jump the trailing ';' */` |
|   13345 |  3969 | `	pGen->pIn++;` |
|       - |  3970 | `	/* Save the post condition stream */` |
|   13345 |  3971 | `	pPostStart = pGen->pIn;` |
|       - |  3972 | `	/* Compile the loop body */` |
|   13345 |  3973 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   13345 |  3974 | `	pGen->pEnd = pTmp;` |
|   13345 |  3975 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   13345 |  3976 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3977 | `		return SXERR_ABORT;` |
|       - |  3978 | `	}` |
|       - |  3979 | `	/* Fix post-continue jumps */` |
|   13345 |  3980 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  3981 | `		JumpFixup *aPost;` |
|       - |  3982 | `		VmInstr *pInstr;` |
|       - |  3983 | `		sxu32 nJumpDest;` |
|       - |  3984 | `		sxu32 n;` |
|      14 |  3985 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  3986 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  3987 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  3988 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  3989 | `			if( pInstr ){` |
|       - |  3990 | `				/* Fix jump */` |
|      14 |  3991 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  3992 | `			}` |
|       8 |  3993 | `		}` |
|       6 |  3994 | `	}` |
|       - |  3995 | `	/* compile the post-expressions if available */` |
|   13345 |  3996 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3997 | `		pPostStart++;` |
|     ! 0 |  3998 | `	}` |
|   13345 |  3999 | `	if( pPostStart < pEnd ){` |
|       - |  4000 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   13345 |  4001 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   13345 |  4002 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   13345 |  4003 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4004 | `			/* Syntax error */` |
|     ! 0 |  4005 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4006 | `			if( rc == SXERR_ABORT ){` |
|       - |  4007 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4008 | `				return SXERR_ABORT;` |
|       - |  4009 | `			}` |
|     ! 0 |  4010 | `			return SXRET_OK;` |
|       - |  4011 | `		}` |
|   13345 |  4012 | `		RE_SWAP_DELIMITER(pGen);` |
|   13345 |  4013 | `		if( rc == SXERR_ABORT ){` |
|       - |  4014 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4015 | `			return SXERR_ABORT;` |
|   13345 |  4016 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4017 | `			/* Pop operand lvalue */` |
|   13345 |  4018 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6670 |  4019 | `		}` |
|    6670 |  4020 | `	}` |
|       - |  4021 | `	/* Emit the unconditional jump to the start of the loop */` |
|   13345 |  4022 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4023 | `	/* Fix all jumps now the destination is resolved */` |
|   13345 |  4024 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4025 | `	/* Release the loop block */` |
|   13345 |  4026 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4027 | `	/* Statement successfully compiled */` |
|   13345 |  4028 | `	return SXRET_OK;` |
|    6677 |  4029 |  |
|       - |  4030 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4031 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4032 | ` * are allowed.` |
|       - |  4033 | ` */` |
|    7150 |  4034 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4035 |  |
|    7155 |  4036 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7155 |  4037 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4038 | `		/* Unexpected expression */` |
|     ! 0 |  4039 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4040 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4041 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4042 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4043 | `		}` |
|     ! 0 |  4044 | `	}` |
|    7155 |  4045 | `	return rc;` |
|       5 |  4046 |  |
|       - |  4047 | `/*` |
|       - |  4048 | ` * Compile the 'foreach' statement.` |
|       - |  4049 | ` * According to the PHP language reference` |
|       - |  4050 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4051 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4052 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4053 | ` *  is a minor but useful extension of the first:` |
|       - |  4054 | ` *  foreach (array_expression as $value)` |
|       - |  4055 | ` *    statement` |
|       - |  4056 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4057 | ` *   statement` |
|       - |  4058 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4059 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4060 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4061 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4062 | ` *  to the variable $key on each loop.` |
|       - |  4063 | ` *  Note:` |
|       - |  4064 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4065 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4066 | ` *  Note:` |
|       - |  4067 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4068 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4069 | ` *  or after the foreach without resetting it.` |
|       - |  4070 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4071 | ` *  of copying the value.` |
|       - |  4072 | ` */` |
|    3658 |  4073 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4074 |  |
|    3663 |  4075 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3663 |  4076 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3663 |  4077 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4078 | `	ph7_foreach_info *pInfo;` |
|       - |  4079 | `	sxu32 nFalseJump;` |
|       - |  4080 | `	VmInstr *pInstr;` |
|       - |  4081 | `	sxu32 nLine;` |
|       - |  4082 | `	sxi32 rc;` |
|    3663 |  4083 | `	nLine = pGen->pIn->nLine;` |
|       - |  4084 | `	/* Jump the 'foreach' keyword */` |
|    3663 |  4085 | `	pGen->pIn++;` |
|    3663 |  4086 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4087 | `		/* Syntax error */` |
|     ! 0 |  4088 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4089 | `		if( rc == SXERR_ABORT ){` |
|       - |  4090 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4091 | `			return SXERR_ABORT;` |
|       - |  4092 | `		}` |
|     ! 0 |  4093 | `		goto Synchronize;` |
|       - |  4094 | `	}` |
|       - |  4095 | `	/* Jump the left parenthesis '(' */` |
|    3663 |  4096 | `	pGen->pIn++;` |
|       - |  4097 | `	/* Create the loop block */` |
|    3663 |  4098 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3663 |  4099 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4100 | `		return SXERR_ABORT;` |
|       - |  4101 | `	}` |
|       - |  4102 | `	/* Delimit the expression */` |
|    3663 |  4103 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3663 |  4104 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4105 | `		/* Empty expression */` |
|     ! 0 |  4106 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4107 | `		if( rc == SXERR_ABORT ){` |
|       - |  4108 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4109 | `			return SXERR_ABORT;` |
|       - |  4110 | `		}` |
|       - |  4111 | `		/* Synchronize */` |
|     ! 0 |  4112 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4113 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4114 | `			pGen->pIn++;` |
|     ! 0 |  4115 | `		}` |
|     ! 0 |  4116 | `		return SXRET_OK;` |
|       - |  4117 | `	}` |
|       - |  4118 | `	/* Compile the array expression */` |
|    3663 |  4119 | `	pCur = pGen->pIn;` |
|   25097 |  4120 | `	while( pCur < pEnd ){` |
|   25097 |  4121 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3677 |  4122 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3677 |  4123 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4124 | `				/* Break with the first 'as' found */` |
|    3663 |  4125 | `				break;` |
|       - |  4126 | `			}` |
|       7 |  4127 | `		}` |
|       - |  4128 | `		/* Advance the stream cursor */` |
|   21439 |  4129 | `		pCur++;` |
|       5 |  4130 | `	}` |
|    3663 |  4131 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4132 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4133 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4134 | `		if( rc == SXERR_ABORT ){` |
|       - |  4135 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4136 | `			return SXERR_ABORT;` |
|       - |  4137 | `		}` |
|     ! 0 |  4138 | `		goto Synchronize;` |
|       - |  4139 | `	}` |
|       - |  4140 | `	/* Swap token streams */` |
|    3663 |  4141 | `	pTmp = pGen->pEnd;` |
|    3663 |  4142 | `	pGen->pEnd = pCur;` |
|    3663 |  4143 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3663 |  4144 | `	if( rc == SXERR_ABORT ){` |
|       - |  4145 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4146 | `		return SXERR_ABORT;` |
|       - |  4147 | `	}` |
|       - |  4148 | `	/* Update token stream */` |
|    3663 |  4149 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4150 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4151 | `		if( rc == SXERR_ABORT ){` |
|       - |  4152 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4153 | `			return SXERR_ABORT;` |
|       - |  4154 | `		}` |
|     ! 0 |  4155 | `		pGen->pIn++;` |
|     ! 0 |  4156 | `	}` |
|    3663 |  4157 | `	pCur++; /* Jump the 'as' keyword */` |
|    3663 |  4158 | `	pGen->pIn = pCur;` |
|    3663 |  4159 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4160 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4161 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4162 | `			return SXERR_ABORT;` |
|       - |  4163 | `		}` |
|     ! 0 |  4164 | `	}` |
|       - |  4165 | `	/* Create the foreach context */` |
|    3663 |  4166 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3663 |  4167 | `	if( pInfo == 0 ){` |
|     ! 0 |  4168 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4169 | `		return SXERR_ABORT;` |
|       - |  4170 | `	}` |
|       - |  4171 | `	/* Zero the structure */` |
|    3663 |  4172 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4173 | `	/* Initialize structure fields */` |
|    3663 |  4174 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4175 | `	/* Check if we have a key field */` |
|   11029 |  4176 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    7371 |  4177 | `		pCur++;` |
|       5 |  4178 | `	}` |
|    3663 |  4179 | `	if( pCur < pEnd ){` |
|       - |  4180 | `		/* Compile the expression holding the key name */` |
|    3507 |  4181 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4182 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4183 | `			if( rc == SXERR_ABORT ){` |
|       - |  4184 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4185 | `				return SXERR_ABORT;` |
|       - |  4186 | `			}` |
|     ! 0 |  4187 | `		}else{` |
|    3507 |  4188 | `			pGen->pEnd = pCur;` |
|    3507 |  4189 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3507 |  4190 | `			if( rc == SXERR_ABORT ){` |
|       - |  4191 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4192 | `				return SXERR_ABORT;` |
|       - |  4193 | `			}` |
|    3507 |  4194 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3507 |  4195 | `			if( pInstr->p3 ){` |
|       - |  4196 | `				/* Record key name */` |
|    3507 |  4197 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1751 |  4198 | `			}` |
|    3507 |  4199 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4200 | `		}` |
|    3507 |  4201 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1751 |  4202 | `	}` |
|    3663 |  4203 | `	pGen->pEnd = pEnd;` |
|    3663 |  4204 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4205 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4206 | `		if( rc == SXERR_ABORT ){` |
|       - |  4207 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4208 | `			return SXERR_ABORT;` |
|       - |  4209 | `		}` |
|     ! 0 |  4210 | `		goto Synchronize;` |
|       - |  4211 | `	}` |
|    3663 |  4212 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4213 | `		pGen->pIn++;` |
|       - |  4214 | `		/* Pass by reference  */` |
|      11 |  4215 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4216 | `	}` |
|       - |  4217 | `	/* Check if the value target is list() */` |
|    3663 |  4218 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4219 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4220 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4221 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4222 | `		 */` |
|       - |  4223 | `		static int iForeachListCnt = 0;` |
|       - |  4224 | `		char zTmp[128];` |
|       - |  4225 | `		sxu32 nLen;` |
|       - |  4226 | `		char *zDup;` |
|      10 |  4227 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4228 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4229 | `		if( zDup == 0 ){` |
|     ! 0 |  4230 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4231 | `			return SXERR_ABORT;` |
|       - |  4232 | `		}` |
|      10 |  4233 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4234 | `		/* Save list() token boundaries */` |
|      10 |  4235 | `		pListStart = pGen->pIn;` |
|       - |  4236 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4237 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4238 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4239 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4240 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4241 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4242 | `				return SXERR_ABORT;` |
|       - |  4243 | `			}` |
|       3 |  4244 | `			goto Synchronize;` |
|       - |  4245 | `		}` |
|       7 |  4246 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4247 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4248 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4249 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4250 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4251 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4252 | `				return SXERR_ABORT;` |
|       - |  4253 | `			}` |
|     ! 0 |  4254 | `			goto Synchronize;` |
|       - |  4255 | `		}` |
|       7 |  4256 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4257 | `		pListEnd = pGen->pIn;` |
|       7 |  4258 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3658 |  4259 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4260 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4261 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4262 | `		 */` |
|       - |  4263 | `		static int iForeachShortListCnt = 0;` |
|       - |  4264 | `		char zTmp[128];` |
|       - |  4265 | `		sxu32 nLen;` |
|       - |  4266 | `		char *zDup;` |
|       3 |  4267 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 |  4268 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 |  4269 | `		if( zDup == 0 ){` |
|     ! 0 |  4270 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4271 | `			return SXERR_ABORT;` |
|       - |  4272 | `		}` |
|       3 |  4273 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4274 | `		/* Save [...] token boundaries */` |
|       3 |  4275 | `		pListStart = pGen->pIn;` |
|       - |  4276 | `		/* Advance past [...] */` |
|       3 |  4277 | `		pGen->pIn++; /* Jump '[' */` |
|       3 |  4278 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 |  4279 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4280 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4281 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4282 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4283 | `				return SXERR_ABORT;` |
|       - |  4284 | `			}` |
|     ! 0 |  4285 | `			goto Synchronize;` |
|       - |  4286 | `		}` |
|       3 |  4287 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 |  4288 | `		pListEnd = pGen->pIn;` |
|       3 |  4289 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 |  4290 | `	}else{` |
|       - |  4291 | `		/* Compile the expression holding the value name */` |
|    3653 |  4292 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3653 |  4293 | `		if( rc == SXERR_ABORT ){` |
|       - |  4294 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4295 | `			return SXERR_ABORT;` |
|       - |  4296 | `		}` |
|    3653 |  4297 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3653 |  4298 | `		if( pInstr->p3 ){` |
|       - |  4299 | `			/* Record value name */` |
|    3653 |  4300 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1824 |  4301 | `		}` |
|       - |  4302 | `	}` |
|       - |  4303 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3661 |  4304 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4305 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3661 |  4306 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4307 | `	/* Record the first instruction to execute */` |
|    3661 |  4308 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4309 | `	/* Emit the FOREACH_STEP instruction */` |
|    3661 |  4310 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4311 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3661 |  4312 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4313 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3661 |  4314 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4315 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4316 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4317 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4318 | `		 */` |
|       9 |  4319 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4320 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4321 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4322 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4323 | `		 */` |
|       9 |  4324 | `		pSavedIn = pGen->pIn;` |
|       9 |  4325 | `		pSavedEnd = pGen->pEnd;` |
|       9 |  4326 | `		pGen->pIn = pListStart;` |
|       9 |  4327 | `		pGen->pEnd = pListEnd;` |
|       9 |  4328 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 |  4329 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 |  4330 | `		}else{` |
|       7 |  4331 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4332 | `		}` |
|       9 |  4333 | `		pGen->pIn = pSavedIn;` |
|       9 |  4334 | `		pGen->pEnd = pSavedEnd;` |
|       9 |  4335 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4336 | `			return SXERR_ABORT;` |
|       - |  4337 | `		}` |
|       - |  4338 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 |  4339 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 |  4340 | `	}` |
|       - |  4341 | `	/* Compile the loop body */` |
|    3661 |  4342 | `	pGen->pIn = &pEnd[1];` |
|    3661 |  4343 | `	pGen->pEnd = pTmp;` |
|    3661 |  4344 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3661 |  4345 | `	if( rc == SXERR_ABORT ){` |
|       - |  4346 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4347 | `		return SXERR_ABORT;` |
|       - |  4348 | `	}` |
|       - |  4349 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3661 |  4350 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4351 | `	/* Fix all jumps now the destination is resolved */` |
|    3661 |  4352 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4353 | `	/* Release the loop block */` |
|    3661 |  4354 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4355 | `	/* Statement successfully compiled */` |
|    3661 |  4356 | `	return SXRET_OK;` |
|       1 |  4357 | `Synchronize:` |
|       - |  4358 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4359 | `	 * compiling this erroneous block.` |
|       - |  4360 | `	 */` |
|       3 |  4361 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4362 | `		pGen->pIn++;` |
|     ! 0 |  4363 | `	}` |
|       3 |  4364 | `	return SXRET_OK;` |
|    1834 |  4365 |  |
|       - |  4366 | `/*` |
|       - |  4367 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4368 | ` * According to the PHP language reference` |
|       - |  4369 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4370 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4371 | ` *  that is similar to that of C:` |
|       - |  4372 | ` *  if (expr)` |
|       - |  4373 | ` *   statement` |
|       - |  4374 | ` *  else construct:` |
|       - |  4375 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4376 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4377 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4378 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4379 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4380 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4381 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4382 | ` *  elseif` |
|       - |  4383 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4384 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4385 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4386 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4387 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4388 | ` *   <?php` |
|       - |  4389 | ` *    if ($a > $b) {` |
|       - |  4390 | ` *     echo "a is bigger than b";` |
|       - |  4391 | ` *    } elseif ($a == $b) {` |
|       - |  4392 | ` *     echo "a is equal to b";` |
|       - |  4393 | ` *    } else {` |
|       - |  4394 | ` *     echo "a is smaller than b";` |
|       - |  4395 | ` *    }` |
|       - |  4396 | ` *    ?>` |
|       - |  4397 | ` */` |
|  138788 |  4398 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4399 |  |
|  138793 |  4400 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  138793 |  4401 | `	GenBlock *pCondBlock = 0;` |
|       - |  4402 | `	sxu32 nJumpIdx;` |
|       - |  4403 | `	sxu32 nKeyID;` |
|       - |  4404 | `	sxi32 rc;` |
|       - |  4405 | `	/* Jump the 'if' keyword */` |
|  138793 |  4406 | `	pGen->pIn++;` |
|  138793 |  4407 | `	pToken = pGen->pIn;` |
|       - |  4408 | `	/* Create the conditional block */` |
|  138793 |  4409 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  138793 |  4410 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4411 | `		return SXERR_ABORT;` |
|       - |  4412 | `	}` |
|       - |  4413 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   76063 |  4414 | `	for(;;){` |
|  152131 |  4415 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4416 | `			/* Syntax error */` |
|     ! 0 |  4417 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4418 | `				pToken--;` |
|     ! 0 |  4419 | `			}` |
|     ! 0 |  4420 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4421 | `			if( rc == SXERR_ABORT ){` |
|       - |  4422 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4423 | `				return SXERR_ABORT;` |
|       - |  4424 | `			}` |
|     ! 0 |  4425 | `			goto Synchronize;` |
|       - |  4426 | `		}` |
|       - |  4427 | `		/* Jump the left parenthesis '(' */` |
|  152131 |  4428 | `		pToken++;` |
|       - |  4429 | `		/* Delimit the condition */` |
|  152131 |  4430 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  152131 |  4431 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4432 | `			/* Syntax error */` |
|     ! 0 |  4433 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4434 | `				pToken--;` |
|     ! 0 |  4435 | `			}` |
|     ! 0 |  4436 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4437 | `			if( rc == SXERR_ABORT ){` |
|       - |  4438 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4439 | `				return SXERR_ABORT;` |
|       - |  4440 | `			}` |
|     ! 0 |  4441 | `			goto Synchronize;` |
|       - |  4442 | `		}` |
|       - |  4443 | `		/* Swap token streams */` |
|  152131 |  4444 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4445 | `		/* Compile the condition */` |
|  152131 |  4446 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4447 | `		/* Update token stream */` |
|  152131 |  4448 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4449 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4450 | `			pGen->pIn++;` |
|     ! 0 |  4451 | `		}` |
|  152131 |  4452 | `		pGen->pIn  = &pEnd[1];` |
|  152131 |  4453 | `		pGen->pEnd = pTmp;` |
|  152131 |  4454 | `		if( rc == SXERR_ABORT ){` |
|       - |  4455 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4456 | `			return SXERR_ABORT;` |
|       - |  4457 | `		}` |
|       - |  4458 | `		/* Emit the false jump */` |
|  152131 |  4459 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4460 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  152131 |  4461 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4462 | `		/* Compile the body */` |
|  152131 |  4463 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  152131 |  4464 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4465 | `			return SXERR_ABORT;` |
|       - |  4466 | `		}` |
|  152131 |  4467 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   42410 |  4468 | `			break;` |
|       - |  4469 | `		}` |
|       - |  4470 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   67321 |  4471 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   67321 |  4472 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   43301 |  4473 | `			break;` |
|       - |  4474 | `		}` |
|       - |  4475 | `		/* Emit the unconditional jump */` |
|   24025 |  4476 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4477 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   24025 |  4478 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   24025 |  4479 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   17299 |  4480 | `			pToken = &pGen->pIn[1];` |
|   17299 |  4481 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    6664 |  4482 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5346 |  4483 | `					break;` |
|       - |  4484 | `			}` |
|    6617 |  4485 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3306 |  4486 | `		}` |
|   13343 |  4487 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4488 | `		/* Synchronize cursors */` |
|   13343 |  4489 | `		pToken = pGen->pIn;` |
|       - |  4490 | `		/* Fix the false jump */` |
|   13343 |  4491 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4492 | `	} /* For(;;) */` |
|       - |  4493 | `	/* Fix the false jump */` |
|  138793 |  4494 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  138793 |  4495 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   53978 |  4496 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4497 | `			/* Compile the else block */` |
|   10687 |  4498 | `			pGen->pIn++;` |
|   10687 |  4499 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   10687 |  4500 | `			if( rc == SXERR_ABORT ){` |
|       - |  4501 |  |
|     ! 0 |  4502 | `				return SXERR_ABORT;` |
|       - |  4503 | `			}` |
|    5341 |  4504 | `	}` |
|  138793 |  4505 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4506 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  138793 |  4507 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4508 | `	/* Release the conditional block */` |
|  138793 |  4509 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4510 | `	/* Statement successfully compiled */` |
|  138793 |  4511 | `	return SXRET_OK;` |
|     ! 0 |  4512 | `Synchronize:` |
|       - |  4513 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4514 | `	 */` |
|     ! 0 |  4515 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4516 | `		pGen->pIn++;` |
|     ! 0 |  4517 | `	}` |
|     ! 0 |  4518 | `	return SXRET_OK;` |
|   69399 |  4519 |  |
|       - |  4520 | `/*` |
|       - |  4521 | ` * Compile the global construct.` |
|       - |  4522 | ` * According to the PHP language reference` |
|       - |  4523 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4524 | ` *  to be used in that function.` |
|       - |  4525 | ` *  Example #1 Using global` |
|       - |  4526 | ` *  <?php` |
|       - |  4527 | ` *   $a = 1;` |
|       - |  4528 | ` *   $b = 2;` |
|       - |  4529 | ` *   function Sum()` |
|       - |  4530 | ` *   {` |
|       - |  4531 | ` *    global $a, $b;` |
|       - |  4532 | ` *    $b = $a + $b;` |
|       - |  4533 | ` *   }` |
|       - |  4534 | ` *   Sum();` |
|       - |  4535 | ` *   echo $b;` |
|       - |  4536 | ` *  ?>` |
|       - |  4537 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4538 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4539 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4540 | ` */` |
|      36 |  4541 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4542 |  |
|      41 |  4543 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4544 | `	sxi32 nExpr;` |
|       - |  4545 | `	sxi32 rc;` |
|       - |  4546 | `	/* Jump the 'global' keyword */` |
|      41 |  4547 | `	pGen->pIn++;` |
|      41 |  4548 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4549 | `		/* Nothing to process */` |
|     ! 0 |  4550 | `		return SXRET_OK;` |
|       - |  4551 | `	}` |
|      41 |  4552 | `	pTmp = pGen->pEnd;` |
|      41 |  4553 | `	nExpr = 0;` |
|      87 |  4554 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4555 | `		if( pGen->pIn < pNext ){` |
|      51 |  4556 | `			pGen->pEnd = pNext;` |
|      51 |  4557 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4558 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4559 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4560 | `					return SXERR_ABORT;` |
|       - |  4561 | `				}` |
|     ! 0 |  4562 | `			}else{` |
|      51 |  4563 | `				pGen->pIn++;` |
|      51 |  4564 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4565 | `					/* Emit a warning */` |
|     ! 0 |  4566 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4567 | `				}else{` |
|      51 |  4568 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4569 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4570 | `						return SXERR_ABORT;` |
|      51 |  4571 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4572 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4573 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4574 | `							/* Variable name, not a constant */` |
|      51 |  4575 | `							pLast->iP1 = 0;` |
|      23 |  4576 | `						}` |
|      51 |  4577 | `						nExpr++;` |
|      23 |  4578 | `					}` |
|       - |  4579 | `				}` |
|       - |  4580 | `			}` |
|      23 |  4581 | `		}` |
|       - |  4582 | `		/* Next expression in the stream */` |
|      51 |  4583 | `		pGen->pIn = pNext;` |
|       - |  4584 | `		/* Jump trailing commas */` |
|      61 |  4585 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4586 | `			pGen->pIn++;` |
|       5 |  4587 | `		}` |
|       5 |  4588 | `	}` |
|       - |  4589 | `	/* Restore token stream */` |
|      41 |  4590 | `	pGen->pEnd = pTmp;` |
|      41 |  4591 | `	if( nExpr > 0 ){` |
|       - |  4592 | `		/* Emit the uplink instruction */` |
|      41 |  4593 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4594 | `	}` |
|      41 |  4595 | `	return SXRET_OK;` |
|      23 |  4596 |  |
|       - |  4597 | `/*` |
|       - |  4598 | ` * Compile the return statement.` |
|       - |  4599 | ` * According to the PHP language reference` |
|       - |  4600 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4601 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4602 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4603 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4604 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4605 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4606 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4607 | ` *  from within the main script file, then script execution end.` |
|       - |  4608 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4609 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4610 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4611 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4612 | ` */` |
|  219310 |  4613 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4614 |  |
|  219315 |  4615 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4616 | `	sxi32 rc;` |
|       - |  4617 | `	/* Jump the 'return' keyword */` |
|  219315 |  4618 | `	pGen->pIn++;` |
|  219315 |  4619 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4620 | `		/* Compile the expression */` |
|  219289 |  4621 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  219289 |  4622 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4623 | `			return SXERR_ABORT;` |
|  219289 |  4624 | `		}else if(rc != SXERR_EMPTY ){` |
|  219289 |  4625 | `			nRet = 1;` |
|  109642 |  4626 | `		}` |
|  109642 |  4627 | `	}` |
|       - |  4628 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4629 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4630 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4631 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4632 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  219315 |  4633 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  219315 |  4634 | `	return SXRET_OK;` |
|  109660 |  4635 |  |
|       - |  4636 | `/*` |
|       - |  4637 | ` * Compile a yield expression.` |
|       - |  4638 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4639 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4640 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4641 | ` */` |
|      72 |  4642 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4643 |  |
|       - |  4644 | `	SyToken *pTmp, *pSplit;` |
|      77 |  4645 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      77 |  4646 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4647 | `	sxi32 rc;` |
|      36 |  4648 | `	(void)iCompileFlag;` |
|       - |  4649 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      77 |  4650 | `	pGen->pIn++;` |
|       - |  4651 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4652 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      77 |  4653 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4654 | `		/* Bare yield — no value */` |
|     ! 0 |  4655 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4656 | `		return SXRET_OK;` |
|       - |  4657 | `	}` |
|       - |  4658 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      77 |  4659 | `	pSplit = 0;` |
|       - |  4660 | `	{` |
|      77 |  4661 | `		SyToken *pCur = pGen->pIn;` |
|      77 |  4662 | `		sxi32 nNest = 0;` |
|     163 |  4663 | `		while( pCur < pGen->pEnd ){` |
|     105 |  4664 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4665 | `				nNest++;` |
|     105 |  4666 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4667 | `				nNest--;` |
|     105 |  4668 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4669 | `				pSplit = pCur;` |
|      16 |  4670 | `				break;` |
|       - |  4671 | `			}` |
|      91 |  4672 | `			pCur++;` |
|       5 |  4673 | `		}` |
|       - |  4674 | `	}` |
|      77 |  4675 | `	pTmp = pGen->pEnd;` |
|      77 |  4676 | `	if( pSplit ){` |
|       - |  4677 | `		/* yield $key => $value */` |
|      16 |  4678 | `		pGen->pEnd = pSplit;` |
|      16 |  4679 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4680 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4681 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4682 | `		pGen->pEnd = pTmp;` |
|      16 |  4683 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4684 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4685 | `		iP1 = 1;` |
|      16 |  4686 | `		iP2 = 1;` |
|       9 |  4687 | `	}else{` |
|       - |  4688 | `		/* yield $value */` |
|      63 |  4689 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      63 |  4690 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      63 |  4691 | `		if( rc != SXERR_EMPTY ){` |
|      63 |  4692 | `			iP1 = 1;` |
|      29 |  4693 | `		}` |
|       - |  4694 | `	}` |
|      77 |  4695 | `	pGen->pEnd = pTmp;` |
|      77 |  4696 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      77 |  4697 | `	return SXRET_OK;` |
|      41 |  4698 |  |
|       - |  4699 | `/*` |
|       - |  4700 | ` * Compile the die/exit language construct.` |
|       - |  4701 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4702 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4703 | ` */` |
|     120 |  4704 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4705 |  |
|     125 |  4706 | `	sxi32 nExpr = 0;` |
|       - |  4707 | `	sxi32 rc;` |
|       - |  4708 | `	/* Jump the die/exit keyword */` |
|     125 |  4709 | `	pGen->pIn++;` |
|     125 |  4710 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4711 | `		/* Compile the expression */` |
|     125 |  4712 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4713 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4714 | `			return SXERR_ABORT;` |
|     125 |  4715 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4716 | `			nExpr = 1;` |
|      60 |  4717 | `		}` |
|      60 |  4718 | `	}` |
|       - |  4719 | `	/* Emit the HALT instruction */` |
|     125 |  4720 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4721 | `	return SXRET_OK;` |
|      65 |  4722 |  |
|       - |  4723 | `/*` |
|       - |  4724 | ` * Compile the 'echo' language construct.` |
|       - |  4725 | ` */` |
|   13844 |  4726 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4727 |  |
|   13849 |  4728 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4729 | `	sxi32 rc;` |
|       - |  4730 | `	/* Jump the 'echo' keyword */` |
|   13849 |  4731 | `	pGen->pIn++;` |
|       - |  4732 | `	/* Compile arguments one after one */` |
|   13849 |  4733 | `	pTmp = pGen->pEnd;` |
|   30111 |  4734 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   16267 |  4735 | `		if( pGen->pIn < pNext ){` |
|   16267 |  4736 | `			pGen->pEnd = pNext;` |
|   16267 |  4737 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   16267 |  4738 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4739 | `				return SXERR_ABORT;` |
|   16267 |  4740 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4741 | `				/* Emit the consume instruction */` |
|   16243 |  4742 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    8119 |  4743 | `			}` |
|    8131 |  4744 | `		}` |
|       - |  4745 | `		/* Jump trailing commas */` |
|   18685 |  4746 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    2423 |  4747 | `			pNext++;` |
|       5 |  4748 | `		}` |
|   16267 |  4749 | `		pGen->pIn = pNext;` |
|       5 |  4750 | `	}` |
|       - |  4751 | `	/* Restore token stream */` |
|   13849 |  4752 | `	pGen->pEnd = pTmp;` |
|   13849 |  4753 | `	return SXRET_OK;` |
|    6927 |  4754 |  |
|       - |  4755 | `/*` |
|       - |  4756 | ` * Compile the static statement.` |
|       - |  4757 | ` * According to the PHP language reference` |
|       - |  4758 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4759 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4760 | ` *  when program execution leaves this scope.` |
|       - |  4761 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4762 | ` * Symisc eXtension.` |
|       - |  4763 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4764 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4765 | ` *  Example` |
|       - |  4766 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4767 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4768 | ` */` |
|       6 |  4769 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       2 |  4770 |  |
|       - |  4771 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4772 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4773 | `	GenBlock *pBlock;` |
|       - |  4774 | `	SyString *pName;` |
|       - |  4775 | `	char *zDup;` |
|       - |  4776 | `	sxu32 nLine;` |
|       - |  4777 | `	sxi32 rc;` |
|       - |  4778 | `	/* Jump the static keyword */` |
|       8 |  4779 | `	nLine = pGen->pIn->nLine;` |
|       8 |  4780 | `	pGen->pIn++;` |
|       - |  4781 | `	/* Extract the enclosing function if any */` |
|       8 |  4782 | `	pBlock = pGen->pCurrent;` |
|      14 |  4783 | `	while( pBlock ){` |
|      14 |  4784 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       8 |  4785 | `			break;` |
|       - |  4786 | `		}` |
|       - |  4787 | `		/* Point to the upper block */` |
|       8 |  4788 | `		pBlock = pBlock->pParent;` |
|       2 |  4789 | `	}` |
|       8 |  4790 | `	if( pBlock == 0 ){` |
|       - |  4791 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4792 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4793 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4794 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4795 | `				return SXERR_ABORT;` |
|       - |  4796 | `			}` |
|     ! 0 |  4797 | `			goto Synchronize;` |
|       - |  4798 | `		}` |
|       - |  4799 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4800 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4801 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4802 | `			return SXERR_ABORT;` |
|     ! 0 |  4803 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4804 | `			/* Emit the POP instruction */` |
|     ! 0 |  4805 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4806 | `		}` |
|     ! 0 |  4807 | `		return SXRET_OK;` |
|       - |  4808 | `	}` |
|       8 |  4809 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4810 | `	/* Make sure we are dealing with a valid statement */` |
|       8 |  4811 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       4 |  4812 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4813 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4814 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4815 | `				return SXERR_ABORT;` |
|       - |  4816 | `			}` |
|       3 |  4817 | `			goto Synchronize;` |
|       - |  4818 | `	}` |
|       5 |  4819 | `	pGen->pIn++;` |
|       - |  4820 | `	/* Extract variable name */` |
|       5 |  4821 | `	pName = &pGen->pIn->sData;` |
|       5 |  4822 | `	pGen->pIn++; /* Jump the var name */` |
|       5 |  4823 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4824 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4825 | `		goto Synchronize;` |
|       - |  4826 | `	}` |
|       - |  4827 | `	/* Initialize the structure describing the static variable */` |
|       5 |  4828 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       5 |  4829 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4830 | `	/* Duplicate variable name */` |
|       5 |  4831 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       5 |  4832 | `	if( zDup == 0 ){` |
|     ! 0 |  4833 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4834 | `		return SXERR_ABORT;` |
|       - |  4835 | `	}` |
|       5 |  4836 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4837 | `	/* Check if we have an expression to compile */` |
|       5 |  4838 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4839 | `		SySet *pInstrContainer;` |
|       - |  4840 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4841 | `		 * Static variable can take any complex expression including function` |
|       - |  4842 | `		 * call as their initialization value.` |
|       - |  4843 | `		 * Example:` |
|       - |  4844 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4845 | `		 */` |
|       5 |  4846 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4847 | `		/* Swap bytecode container */` |
|       5 |  4848 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       5 |  4849 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4850 | `		/* Compile the expression */` |
|       5 |  4851 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4852 | `		/* Emit the done instruction */` |
|       5 |  4853 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4854 | `		/* Restore default bytecode container */` |
|       5 |  4855 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       2 |  4856 | `	}` |
|       - |  4857 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       5 |  4858 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       5 |  4859 | `	return SXRET_OK;` |
|       1 |  4860 | `Synchronize:` |
|       - |  4861 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  4862 | `	 * statement.` |
|       - |  4863 | `	 */` |
|       5 |  4864 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  4865 | `		pGen->pIn++;` |
|       1 |  4866 | `	}` |
|       3 |  4867 | `	return SXRET_OK;` |
|       5 |  4868 |  |
|       - |  4869 | `/*` |
|       - |  4870 | ` * Compile the var statement.` |
|       - |  4871 | ` * Symisc Extension:` |
|       - |  4872 | ` *      var statement can be used outside of a class definition.` |
|       - |  4873 | ` */` |
|       4 |  4874 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  4875 |  |
|       - |  4876 | `	sxu32 nLine;` |
|       - |  4877 | `	sxi32 rc;` |
|       5 |  4878 | `	nLine = pGen->pIn->nLine;` |
|       - |  4879 | `	/* Jump the 'var' keyword */` |
|       5 |  4880 | `	pGen->pIn++;` |
|       5 |  4881 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  4882 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  4883 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  4884 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  4885 | `			pGen->pIn++;` |
|     ! 0 |  4886 | `		}` |
|     ! 0 |  4887 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4888 | `			return SXERR_ABORT;` |
|       - |  4889 | `		}` |
|     ! 0 |  4890 | `	}else{` |
|       - |  4891 | `		/* Compile the expression */` |
|       5 |  4892 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  4893 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4894 | `			return SXERR_ABORT;` |
|       5 |  4895 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  4896 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  4897 | `		}` |
|       - |  4898 | `	}` |
|       5 |  4899 | `	return SXRET_OK;` |
|       3 |  4900 |  |
|       - |  4901 | `/*` |
|       - |  4902 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  4903 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  4904 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  4905 | ` */` |
|       - |  4906 | `/*` |
|       - |  4907 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  4908 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  4909 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  4910 | ` * qualified name and updates the instruction's operand index.` |
|       - |  4911 | ` *` |
|       - |  4912 | ` * Resolution order:` |
|       - |  4913 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  4914 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  4915 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  4916 | ` *` |
|       - |  4917 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  4918 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  4919 | ` * Returns the (possibly new) literal index.` |
|       - |  4920 | ` */` |
|  409640 |  4921 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  4922 |  |
|       - |  4923 | `	ph7_value *pLit;` |
|       - |  4924 | `	const char *zLit;` |
|       - |  4925 | `	SyString sQualified;` |
|       - |  4926 | `	sxu32 nLit;` |
|       - |  4927 | `	sxu32 k;` |
|       - |  4928 | `	sxu32 nNewIdx;` |
|       - |  4929 | `	int hasNsSep;` |
|       - |  4930 | `	SyHashEntry *pImport;` |
|       - |  4931 | `	ph7_value *pNew;` |
|  409645 |  4932 | `	if( pFromImport ){` |
|  391621 |  4933 | `		*pFromImport = 0;` |
|  195808 |  4934 | `	}` |
|  409645 |  4935 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  409645 |  4936 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4937 | `		return nOrigIdx;` |
|       - |  4938 | `	}` |
|  409645 |  4939 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  409645 |  4940 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4941 | `	/* Skip if already qualified (contains backslash) */` |
|  409645 |  4942 | `	hasNsSep = 0;` |
| 4431757 |  4943 | `	for( k = 0; k < nLit; k++ ){` |
| 4022125 |  4944 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2011061 |  4945 | `	}` |
|  409645 |  4946 | `	if( hasNsSep ){` |
|      11 |  4947 | `		return nOrigIdx;` |
|       - |  4948 | `	}` |
|       - |  4949 | `	/* Check use imports first (works even outside namespaces) */` |
|  409637 |  4950 | `	SyBlobReset(&pGen->sWorker);` |
|  409637 |  4951 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  409637 |  4952 | `	if( pImport ){` |
|      41 |  4953 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  4954 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  4955 | `		if( pFromImport ){` |
|      18 |  4956 | `			*pFromImport = 1;` |
|       8 |  4957 | `		}` |
|      23 |  4958 | `	}else{` |
|  409601 |  4959 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  409511 |  4960 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  4961 | `		}` |
|       - |  4962 | `		/* Prepend current namespace */` |
|      95 |  4963 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  4964 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  4965 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  4966 | `	}` |
|       - |  4967 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  4968 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  4969 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  4970 | `		return nNewIdx; /* Already interned */` |
|       - |  4971 | `	}` |
|      79 |  4972 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  4973 | `	if( pNew == 0 ){` |
|     ! 0 |  4974 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  4975 | `	}` |
|      79 |  4976 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  4977 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  4978 | `	return nNewIdx;` |
|  204825 |  4979 |  |
|       - |  4980 | `/*` |
|       - |  4981 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4982 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4983 | ` */` |
|   90000 |  4984 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  4985 |  |
|       - |  4986 | `	SyHashEntry *pImport;` |
|       - |  4987 | `	/* Check use imports first */` |
|   90005 |  4988 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   90005 |  4989 | `	if( pImport ){` |
|      15 |  4990 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 |  4991 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      15 |  4992 | `		return;` |
|       - |  4993 | `	}` |
|       - |  4994 | `	/* Prepend current namespace if active */` |
|   89993 |  4995 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4996 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4997 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4998 | `	}` |
|   89993 |  4999 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   45005 |  5000 |  |
|       - |  5001 | `/*` |
|       - |  5002 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5003 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5004 | ` * The caller must release pOut when done.` |
|       - |  5005 | ` */` |
|  126800 |  5006 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5007 |  |
|  126805 |  5008 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5009 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5010 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5011 | `	}` |
|  126805 |  5012 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  126805 |  5013 |  |
|       - |  5014 | `/*` |
|       - |  5015 | ` * Compile a namespace statement` |
|       - |  5016 | ` * According to the PHP language reference manual` |
|       - |  5017 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5018 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5019 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5020 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5021 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5022 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5023 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5024 | ` *  programming world.` |
|       - |  5025 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5026 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5027 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5028 | ` *  classes/functions/constants.` |
|       - |  5029 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5030 | ` *  readability of source code.` |
|       - |  5031 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5032 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5033 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5034 | ` *       class MyClass {}` |
|       - |  5035 | ` *       function myfunction() {}` |
|       - |  5036 | ` *       const MYCONST = 1;` |
|       - |  5037 | ` *       $a = new MyClass;` |
|       - |  5038 | ` *       $c = new \my\name\MyClass;` |
|       - |  5039 | ` *       $a = strlen('hi');` |
|       - |  5040 | ` *       $d = namespace\MYCONST;` |
|       - |  5041 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5042 | ` *       echo constant($d);` |
|       - |  5043 | ` * NOTE` |
|       - |  5044 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5045 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5046 | ` */` |
|       - |  5047 | `/*` |
|       - |  5048 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5049 | ` */` |
|      14 |  5050 | `static const char * TokenTypeName(sxu32 nType)` |
|       4 |  5051 |  |
|      18 |  5052 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 |  5053 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 |  5054 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 |  5055 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 |  5056 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 |  5057 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5058 | `	return "token";` |
|      11 |  5059 |  |
|     106 |  5060 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5061 |  |
|       - |  5062 | `	sxu32 nLine;` |
|       - |  5063 | `	sxi32 rc;` |
|     111 |  5064 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5065 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5066 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5067 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5068 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5069 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5070 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5071 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5072 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5073 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5074 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5075 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5076 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5077 | `		return SXRET_OK;` |
|       - |  5078 | `	}` |
|     111 |  5079 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5080 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5081 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5082 | `		return SXRET_OK;` |
|       - |  5083 | `	}` |
|     111 |  5084 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5085 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5086 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5087 | `		return SXRET_OK;` |
|       - |  5088 | `	}` |
|       - |  5089 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5090 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5091 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5092 | `			/* Append backslash separator */` |
|      27 |  5093 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5094 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5095 | `			}` |
|      16 |  5096 | `		}else{` |
|       - |  5097 | `			/* Append identifier */` |
|     131 |  5098 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5099 | `		}` |
|     153 |  5100 | `		pGen->pIn++;` |
|       5 |  5101 | `	}` |
|       - |  5102 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5103 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5104 | `	{` |
|     111 |  5105 | `		char *zNsDup = 0;` |
|     111 |  5106 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5107 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5108 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5109 | `		}` |
|     111 |  5110 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5111 | `	}` |
|     111 |  5112 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5113 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5114 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5115 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5116 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5117 | `			return SXERR_ABORT;` |
|       - |  5118 | `		}` |
|       2 |  5119 | `	}` |
|     111 |  5120 | `	return SXRET_OK;` |
|      58 |  5121 |  |
|       - |  5122 | `/*` |
|       - |  5123 | ` * Compile the 'use' statement` |
|       - |  5124 | ` * According to the PHP language reference manual` |
|       - |  5125 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5126 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5127 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5128 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5129 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5130 | ` *  a function or constant is not supported.` |
|       - |  5131 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5132 | ` * NOTE` |
|       - |  5133 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5134 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5135 | ` */` |
|      68 |  5136 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5137 |  |
|       - |  5138 | `	sxu32 nLine;` |
|       - |  5139 | `	sxi32 rc;` |
|       - |  5140 | `	SyBlob sPath;` |
|       - |  5141 | `	SyString sAlias;` |
|       - |  5142 | `	SyToken *pLast;` |
|       - |  5143 | `	char *zDup;` |
|       - |  5144 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5145 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5146 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5147 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5148 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5149 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5150 | `	iUseType = 0;` |
|      73 |  5151 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5152 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5153 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5154 | `			iUseType = 1;` |
|      16 |  5155 | `			pGen->pIn++;` |
|      23 |  5156 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5157 | `			iUseType = 2;` |
|      16 |  5158 | `			pGen->pIn++;` |
|       7 |  5159 | `		}` |
|      14 |  5160 | `	}` |
|       - |  5161 | `	/* Select target hash tables based on import type */` |
|      73 |  5162 | `	switch( iUseType ){` |
|       7 |  5163 | `		case 1:` |
|      16 |  5164 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5165 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5166 | `			break;` |
|       7 |  5167 | `		case 2:` |
|      16 |  5168 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5169 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5170 | `			break;` |
|      20 |  5171 | `		default:` |
|      45 |  5172 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5173 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5174 | `			break;` |
|       - |  5175 | `	}` |
|      73 |  5176 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5177 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5178 | `	for(;;){` |
|      75 |  5179 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5180 | `			break;` |
|       - |  5181 | `		}` |
|      75 |  5182 | `		SyBlobReset(&sPath);` |
|      75 |  5183 | `		pLast = 0;` |
|       - |  5184 | `		/* Collect the full namespace path */` |
|     261 |  5185 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5186 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5187 | `				pLast = pGen->pIn;` |
|     131 |  5188 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5189 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5190 | `				}` |
|     131 |  5191 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5192 | `			}` |
|     191 |  5193 | `			pGen->pIn++;` |
|       5 |  5194 | `		}` |
|      75 |  5195 | `		if( pLast == 0 ){` |
|       - |  5196 | `			/* Empty path */` |
|       5 |  5197 | `			break;` |
|       - |  5198 | `		}` |
|       - |  5199 | `		/* Default alias is the last component of the path */` |
|      71 |  5200 | `		sAlias = pLast->sData;` |
|       - |  5201 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5202 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5203 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      19 |  5204 | `			pGen->pIn++; /* Jump 'as' */` |
|      19 |  5205 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      19 |  5206 | `				sAlias = pGen->pIn->sData;` |
|      19 |  5207 | `				pGen->pIn++;` |
|       8 |  5208 | `			}` |
|       8 |  5209 | `		}` |
|       - |  5210 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5211 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5212 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5213 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5214 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5215 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5216 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5217 | `				return SXERR_ABORT;` |
|       - |  5218 | `			}` |
|       2 |  5219 | `		}` |
|       - |  5220 | `		/* Register the import: alias -> FQN.` |
|       - |  5221 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5222 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5223 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5224 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5225 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5226 | `		if( zDup ){` |
|      71 |  5227 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5228 | `			if( pVmHash ){` |
|       - |  5229 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5230 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5231 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5232 | `				if( zAliasDup ){` |
|      43 |  5233 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5234 | `				}` |
|      19 |  5235 | `			}` |
|      71 |  5236 | `			if( iUseType == 2 ){` |
|       - |  5237 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5238 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5239 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5240 | `				if( zAliasDup ){` |
|       - |  5241 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5242 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5243 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5244 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5245 | `					if( azPair ){` |
|      16 |  5246 | `						azPair[0] = zAliasDup;` |
|      16 |  5247 | `						azPair[1] = zDup;` |
|      16 |  5248 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5249 | `					}` |
|       7 |  5250 | `				}` |
|       7 |  5251 | `			}` |
|      33 |  5252 | `		}` |
|       - |  5253 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5254 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5255 | `			pGen->pIn++;` |
|       2 |  5256 | `		}else{` |
|      37 |  5257 | `			break;` |
|       - |  5258 | `		}` |
|       1 |  5259 | `	}` |
|      73 |  5260 | `	SyBlobRelease(&sPath);` |
|      73 |  5261 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5262 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5263 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5264 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5265 | `			return SXERR_ABORT;` |
|       - |  5266 | `		}` |
|       1 |  5267 | `	}` |
|      73 |  5268 | `	return SXRET_OK;` |
|      39 |  5269 |  |
|       - |  5270 | `/*` |
|       - |  5271 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5272 | ` *` |
|       - |  5273 | ` * According to the PHP language reference manual.` |
|       - |  5274 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5275 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5276 | ` *  declare (directive)` |
|       - |  5277 | ` *   statement` |
|       - |  5278 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5279 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5280 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5281 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5282 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5283 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5284 | ` * <?php` |
|       - |  5285 | ` * // these are the same:` |
|       - |  5286 | ` * // you can use this:` |
|       - |  5287 | ` * declare(ticks=1) {` |
|       - |  5288 | ` *   // entire script here` |
|       - |  5289 | ` * }` |
|       - |  5290 | ` * // or you can use this:` |
|       - |  5291 | ` * declare(ticks=1);` |
|       - |  5292 | ` * // entire script here` |
|       - |  5293 | ` * ?>` |
|       - |  5294 | ` *` |
|       - |  5295 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5296 | ` */` |
|       - |  5297 | `/*` |
|       - |  5298 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5299 | ` */` |
|      68 |  5300 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5301 |  |
|     103 |  5302 | `	return SyStringLength(pName) == nWant` |
|      68 |  5303 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5304 |  |
|       - |  5305 |  |
|      40 |  5306 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5307 |  |
|      45 |  5308 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5309 | `	SyToken *pBodyEnd = 0;` |
|       - |  5310 | `	SyToken *pBodyStart;` |
|       - |  5311 | `	SyToken *pCursor;` |
|       - |  5312 | `	int bHasStrictTypes;` |
|       - |  5313 | `	int bBlockForm;` |
|       - |  5314 | `	int bPlacementOk;` |
|       - |  5315 | `	sxi32 rc;` |
|      45 |  5316 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5317 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5318 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5319 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5320 | `			return SXERR_ABORT;` |
|       - |  5321 | `		}` |
|       6 |  5322 | `		goto Synchro;` |
|       - |  5323 | `	}` |
|      41 |  5324 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5325 | `	pBodyStart = pGen->pIn;` |
|       - |  5326 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5327 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5328 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5329 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5330 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5331 | `			return SXERR_ABORT;` |
|       - |  5332 | `		}` |
|     ! 0 |  5333 | `		return SXRET_OK;` |
|       - |  5334 | `	}` |
|       - |  5335 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5336 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5337 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5338 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5339 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5340 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5341 | `			return SXERR_ABORT;` |
|       - |  5342 | `		}` |
|     ! 0 |  5343 | `	}` |
|      41 |  5344 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5345 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5346 | `	bHasStrictTypes = 0;` |
|       - |  5347 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5348 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5349 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5350 | `	pCursor = pBodyStart;` |
|      53 |  5351 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5352 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5353 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5354 | `				bHasStrictTypes = 1;` |
|      37 |  5355 | `				break;` |
|       - |  5356 | `			}` |
|       2 |  5357 | `		}` |
|      14 |  5358 | `		pCursor++;` |
|       2 |  5359 | `	}` |
|      41 |  5360 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5361 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5362 | `			"strict_types declaration must not use block mode");` |
|       3 |  5363 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5364 | `		return SXRET_OK;` |
|       - |  5365 | `	}` |
|      39 |  5366 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5367 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5368 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5369 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5370 | `		return SXRET_OK;` |
|       - |  5371 | `	}` |
|       - |  5372 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5373 | `	pCursor = pBodyStart;` |
|      65 |  5374 | `	while( pCursor < pBodyEnd ){` |
|       - |  5375 | `		SyToken *pNameTok;` |
|       - |  5376 | `		SyToken *pEqTok;` |
|       - |  5377 | `		SyToken *pValTok;` |
|       - |  5378 | `		SyString *pDirName;` |
|       - |  5379 | `		int bIsStrict;` |
|       - |  5380 | `		int iStrictValue;` |
|      37 |  5381 | `		pNameTok = pCursor;` |
|      37 |  5382 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5383 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5384 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5385 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5386 | `			return SXRET_OK;` |
|       - |  5387 | `		}` |
|      37 |  5388 | `		pEqTok = pNameTok + 1;` |
|      37 |  5389 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5390 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5391 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5392 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5393 | `			return SXRET_OK;` |
|       - |  5394 | `		}` |
|      37 |  5395 | `		pValTok = pEqTok + 1;` |
|      37 |  5396 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5397 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5398 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5399 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5400 | `			return SXRET_OK;` |
|       - |  5401 | `		}` |
|      37 |  5402 | `		pDirName = &pNameTok->sData;` |
|      37 |  5403 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5404 | `		if( bIsStrict ){` |
|       - |  5405 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5406 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5407 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5408 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5409 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5410 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5411 | `				return SXRET_OK;` |
|       - |  5412 | `			}` |
|      33 |  5413 | `			iStrictValue = -1;` |
|      33 |  5414 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5415 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5416 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5417 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5418 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5419 | `			}` |
|      33 |  5420 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5421 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5422 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5423 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5424 | `				return SXRET_OK;` |
|       - |  5425 | `			}` |
|      30 |  5426 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5427 | `		}else{` |
|       - |  5428 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5429 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5430 | `			 * behavior don't regress. */` |
|       8 |  5431 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5432 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5433 | `				ph7_lib_version()` |
|       - |  5434 | `				);` |
|       - |  5435 | `		}` |
|      35 |  5436 | `		pCursor = pValTok + 1;` |
|       - |  5437 | `		/* Consume separating comma (or end). */` |
|      35 |  5438 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5439 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5440 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5441 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5442 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5443 | `				return SXRET_OK;` |
|       - |  5444 | `			}` |
|       3 |  5445 | `			pCursor++;` |
|       1 |  5446 | `		}` |
|       5 |  5447 | `	}` |
|       - |  5448 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5449 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5450 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5451 | `	return SXRET_OK;` |
|       2 |  5452 | `Synchro:` |
|       - |  5453 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5454 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5455 | `		pGen->pIn++;` |
|       2 |  5456 | `	}` |
|       6 |  5457 | `	return SXRET_OK;` |
|      25 |  5458 |  |
|       - |  5459 | `/*` |
|       - |  5460 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5461 | ` * as follows:` |
|       - |  5462 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5463 | ` * {` |
|       - |  5464 | ` *   return "Making a cup of $type.\n";` |
|       - |  5465 | ` * }` |
|       - |  5466 | ` * Symisc eXtension.` |
|       - |  5467 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5468 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5469 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5470 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5471 | ` *      {` |
|       - |  5472 | ` *       var_dump($a);` |
|       - |  5473 | ` *      }` |
|       - |  5474 | ` *     //call test without args` |
|       - |  5475 | ` *      test();` |
|       - |  5476 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5477 | ` *      Example:` |
|       - |  5478 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5479 | ` * 3 -) Function overloading!!` |
|       - |  5480 | ` *      Example:` |
|       - |  5481 | ` *      function foo($a) {` |
|       - |  5482 | ` *   	  return $a.PHP_EOL;` |
|       - |  5483 | ` *	    }` |
|       - |  5484 | ` *	    function foo($a, $b) {` |
|       - |  5485 | ` *   	  return $a + $b;` |
|       - |  5486 | ` *	    }` |
|       - |  5487 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5488 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5489 | ` *      // Same arg` |
|       - |  5490 | ` *	   function foo(string $a)` |
|       - |  5491 | ` *	   {` |
|       - |  5492 | ` *	     echo "a is a string\n";` |
|       - |  5493 | ` *	     var_dump($a);` |
|       - |  5494 | ` *	   }` |
|       - |  5495 | ` *	  function foo(int $a)` |
|       - |  5496 | ` *	  {` |
|       - |  5497 | ` *	    echo "a is integer\n";` |
|       - |  5498 | ` *	    var_dump($a);` |
|       - |  5499 | ` *	  }` |
|       - |  5500 | ` *	  function foo(array $a)` |
|       - |  5501 | ` *	  {` |
|       - |  5502 | ` * 	    echo "a is an array\n";` |
|       - |  5503 | ` * 	    var_dump($a);` |
|       - |  5504 | ` *	  }` |
|       - |  5505 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5506 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5507 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5508 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5509 | ` * introduced by the PH7 engine.` |
|       - |  5510 | ` */` |
|   62848 |  5511 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5512 |  |
|       - |  5513 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5514 | `	SySet *pInstrContainer;` |
|       - |  5515 | `	sxi32 rc;` |
|       - |  5516 | `	/* Swap token stream */` |
|   62853 |  5517 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   62853 |  5518 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   62853 |  5519 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5520 | `	/* Compile the expression holding the argument value */` |
|   62853 |  5521 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5522 | `	/* Emit the done instruction */` |
|   62853 |  5523 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   62853 |  5524 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   62853 |  5525 | `	RE_SWAP_DELIMITER(pGen);` |
|   62853 |  5526 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5527 | `		return SXERR_ABORT;` |
|       - |  5528 | `	}` |
|   62853 |  5529 | `	return SXRET_OK;` |
|   31429 |  5530 |  |
|       - |  5531 | `/*` |
|       - |  5532 | ` * Collect function arguments one after one.` |
|       - |  5533 | ` * According to the PHP language reference manual.` |
|       - |  5534 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5535 | ` * list of expressions.` |
|       - |  5536 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5537 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5538 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5539 | ` * for more information.` |
|       - |  5540 | ` * Example #1 Passing arrays to functions` |
|       - |  5541 | ` * <?php` |
|       - |  5542 | ` * function takes_array($input)` |
|       - |  5543 | ` * {` |
|       - |  5544 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5545 | ` * }` |
|       - |  5546 | ` * ?>` |
|       - |  5547 | ` * Making arguments be passed by reference` |
|       - |  5548 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5549 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5550 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5551 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5552 | ` * to the argument name in the function definition:` |
|       - |  5553 | ` * Example #2 Passing function parameters by reference` |
|       - |  5554 | ` * <?php` |
|       - |  5555 | ` * function add_some_extra(&$string)` |
|       - |  5556 | ` * {` |
|       - |  5557 | ` *   $string .= 'and something extra.';` |
|       - |  5558 | ` * }` |
|       - |  5559 | ` * $str = 'This is a string, ';` |
|       - |  5560 | ` * add_some_extra($str);` |
|       - |  5561 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5562 | ` * ?>` |
|       - |  5563 | ` *` |
|       - |  5564 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5565 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5566 | ` * on these extension.` |
|       - |  5567 | ` */` |
|   87156 |  5568 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5569 |  |
|       - |  5570 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5571 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5572 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5573 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5574 | `	sxi32 rc;` |
|       - |  5575 |  |
|   87161 |  5576 | `	pIn = pGen->pIn;` |
|   87161 |  5577 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5578 | `	/* Process arguments one after one */` |
|  109001 |  5579 | `	for(;;){` |
|  218007 |  5580 | `		if( pIn >= pEnd ){` |
|       - |  5581 | `			/* No more arguments to process */` |
|   87149 |  5582 | `			break;` |
|       - |  5583 | `		}` |
|  130863 |  5584 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  130863 |  5585 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  130863 |  5586 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  130863 |  5587 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5588 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5589 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5590 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5591 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5592 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5593 | `		{` |
|  130863 |  5594 | `			int bReadonly = 0, bVisSeen = 0;` |
|  130863 |  5595 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  130863 |  5596 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5597 | `				bReadonly = 1;` |
|       3 |  5598 | `				pIn++;` |
|       1 |  5599 | `			}` |
|  130863 |  5600 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   59701 |  5601 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   59701 |  5602 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      65 |  5603 | `					bVisSeen = 1;` |
|      65 |  5604 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      86 |  5605 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      28 |  5606 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      65 |  5607 | `					pIn++;` |
|      65 |  5608 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5609 | `						bReadonly = 1;` |
|      16 |  5610 | `						pIn++;` |
|       6 |  5611 | `					}` |
|      30 |  5612 | `				}` |
|   29848 |  5613 | `			}` |
|  130863 |  5614 | `			if( bVisSeen \|\| bReadonly ){` |
|      67 |  5615 | `				if( !bCtorCtx ){` |
|       6 |  5616 | `					if( bAbstractCtx ){` |
|       3 |  5617 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5618 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5619 | `					}else{` |
|       3 |  5620 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5621 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5622 | `					}` |
|       6 |  5623 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5624 | `						return SXERR_ABORT;` |
|       - |  5625 | `					}` |
|       6 |  5626 | `					return SXERR_SYNTAX;` |
|       - |  5627 | `				}` |
|      63 |  5628 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      63 |  5629 | `				sArg.iPromoteVis = iVis;` |
|      63 |  5630 | `				if( bReadonly ){` |
|      18 |  5631 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5632 | `				}` |
|      29 |  5633 | `			}` |
|       - |  5634 | `		}` |
|       - |  5635 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  165693 |  5636 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  101932 |  5637 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   71349 |  5638 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   69661 |  5639 | `			sxu32 nLineLocal = pIn->nLine;` |
|   69661 |  5640 | `			sxi32 iTFlags = 0;` |
|   69661 |  5641 | `			pGen->pIn = pIn;` |
|   69661 |  5642 | `			rc = GenStateParseUnionTypeDecl(` |
|   34828 |  5643 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   34828 |  5644 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5645 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5646 | `				/* bAllowVoid */ 0,` |
|   34828 |  5647 | `						nLineLocal);` |
|   69661 |  5648 | `			pIn = pGen->pIn;` |
|   69661 |  5649 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5650 | `				return SXERR_ABORT;` |
|   69661 |  5651 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5652 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5653 | `				return SXERR_SYNTAX;` |
|   69659 |  5654 | `			}else if( rc == SXERR_SYNTAX ){` |
|       6 |  5655 | `				if( pIn < pEnd ){` |
|       8 |  5656 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5657 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5658 | `						&pIn->sData);` |
|       4 |  5659 | `				}else{` |
|     ! 0 |  5660 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5661 | `						"syntax error, unexpected end of file");` |
|       - |  5662 | `				}` |
|       6 |  5663 | `				return SXERR_SYNTAX;` |
|       - |  5664 | `			}` |
|   69655 |  5665 | `			sArg.iFlags \|= iTFlags;` |
|   34825 |  5666 | `		}` |
|  130853 |  5667 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5668 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5669 | `			return rc;` |
|       - |  5670 | `		}` |
|  130853 |  5671 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5672 | `			/* Pass by reference,record that */` |
|    3339 |  5673 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3339 |  5674 | `			pIn++;` |
|    1667 |  5675 | `		}` |
|  130853 |  5676 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5677 | `			/* Variadic parameter: ...$args */` |
|      47 |  5678 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      47 |  5679 | `			pIn++;` |
|      21 |  5680 | `		}` |
|  130853 |  5681 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5682 | `			/* Invalid argument */` |
|     ! 0 |  5683 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5684 | `			return rc;` |
|       - |  5685 | `		}` |
|  130853 |  5686 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5687 | `		/* Copy argument name */` |
|  130853 |  5688 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  130853 |  5689 | `		if( zDup == 0 ){` |
|     ! 0 |  5690 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5691 | `			return SXERR_ABORT;` |
|       - |  5692 | `		}` |
|  130853 |  5693 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  130853 |  5694 | `		pIn++;` |
|  130853 |  5695 | `		if( pIn < pEnd ){` |
|   73495 |  5696 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5697 | `				SyToken *pDefend;` |
|   62855 |  5698 | `				sxi32 iNest = 0;` |
|   62855 |  5699 | `				pIn++; /* Jump the equal sign */` |
|   62855 |  5700 | `				pDefend = pIn;` |
|       - |  5701 | `				/* Process the default value associated with this argument */` |
|  132315 |  5702 | `				while( pDefend < pEnd ){` |
|  102533 |  5703 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   33073 |  5704 | `						break;` |
|       - |  5705 | `					}` |
|   69465 |  5706 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5707 | `						/* Increment nesting level */` |
|    3311 |  5708 | `						iNest++;` |
|   67812 |  5709 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5710 | `						/* Decrement nesting level */` |
|    3311 |  5711 | `						iNest--;` |
|    1653 |  5712 | `					}` |
|   69465 |  5713 | `					pDefend++;` |
|       5 |  5714 | `				}` |
|   62855 |  5715 | `				if( pIn >= pDefend ){` |
|       3 |  5716 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5717 | `					return rc;` |
|       - |  5718 | `				}` |
|       - |  5719 | `				/* Process default value */` |
|   62853 |  5720 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   62853 |  5721 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5722 | `					return rc;` |
|       - |  5723 | `				}` |
|       - |  5724 | `				/* Point beyond the default value */` |
|   62853 |  5725 | `				pIn = pDefend;` |
|   31424 |  5726 | `			}` |
|   73493 |  5727 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5728 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5729 | `				return rc;` |
|       - |  5730 | `			}` |
|   73493 |  5731 | `			pIn++; /* Jump the trailing comma */` |
|   36744 |  5732 | `		}` |
|       - |  5733 | `		/* Append argument signature */` |
|  130851 |  5734 | `		if( sArg.nType > 0 ){` |
|   69611 |  5735 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5736 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    9945 |  5737 | `				int marker = 'o';` |
|    9945 |  5738 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    9945 |  5739 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    4975 |  5740 | `			}else{` |
|       - |  5741 | `				int c;` |
|   59671 |  5742 | `				c = 'n'; /* cc warning */` |
|       - |  5743 | `				/* Type leading character */` |
|   59671 |  5744 | `				switch(sArg.nType){` |
|     ! 0 |  5745 | `				case MEMOBJ_HASHMAP:` |
|       - |  5746 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5747 | `					c = 'h';` |
|     ! 0 |  5748 | `					break;` |
|    8310 |  5749 | `				case MEMOBJ_INT:` |
|       - |  5750 | `					/* Integer */` |
|   16625 |  5751 | `					c = 'i';` |
|   16625 |  5752 | `					break;` |
|       1 |  5753 | `				case MEMOBJ_BOOL:` |
|       - |  5754 | `					/* Bool */` |
|       3 |  5755 | `					c = 'b';` |
|       3 |  5756 | `					break;` |
|       2 |  5757 | `				case MEMOBJ_REAL:` |
|       - |  5758 | `					/* Float */` |
|       5 |  5759 | `					c = 'f';` |
|       5 |  5760 | `					break;` |
|   21512 |  5761 | `				case MEMOBJ_STRING:` |
|       - |  5762 | `					/* String */` |
|   43029 |  5763 | `					c = 's';` |
|   43029 |  5764 | `					break;` |
|       7 |  5765 | `				case MEMOBJ_OBJ:` |
|       - |  5766 | `					/* Object */` |
|      16 |  5767 | `					c = 'o';` |
|      14 |  5768 | `					break;` |
|       1 |  5769 | `				default:` |
|       2 |  5770 | `					break;` |
|       - |  5771 | `				}` |
|   59671 |  5772 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5773 | `			}` |
|   34808 |  5774 | `		}else{` |
|       - |  5775 | `			/* No type is associated with this parameter which mean` |
|       - |  5776 | `			 * that this function is not condidate for overloading.` |
|       - |  5777 | `			 */` |
|   61245 |  5778 | `			SyBlobRelease(&sSig);` |
|       - |  5779 | `		}` |
|       - |  5780 | `		/* Save in the argument set */` |
|  130851 |  5781 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5782 | `	}` |
|   87149 |  5783 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5784 | `		/* Save function signature */` |
|   43131 |  5785 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   21563 |  5786 | `	}` |
|   87149 |  5787 | `	return SXRET_OK;` |
|   43583 |  5788 |  |
|       - |  5789 | `/*` |
|       - |  5790 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5791 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5792 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5793 | ` */` |
|  206930 |  5794 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5795 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5796 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5797 | `	)` |
|       5 |  5798 |  |
|       - |  5799 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5800 | `	GenBlock *pBlock;` |
|       - |  5801 | `	sxu32 nGotoOfft;` |
|       - |  5802 | `	sxi32 rc;` |
|       - |  5803 | `	/* Attach the new function */` |
|  206935 |  5804 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  206935 |  5805 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5806 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5807 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5808 | `		return SXERR_ABORT;` |
|       - |  5809 | `	}` |
|  206935 |  5810 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5811 | `	/* Swap bytecode containers */` |
|  206935 |  5812 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  206935 |  5813 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5814 | `	/* Emit constructor property promotion prologue:` |
|       - |  5815 | `	 *   $this->NAME = $NAME;` |
|       - |  5816 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5817 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5818 | `	{` |
|  206935 |  5819 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5820 | `		sxu32 i;` |
|  311209 |  5821 | `		for( i = 0; i < nArg; i++ ){` |
|  104279 |  5822 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5823 | `			char *zSrc;` |
|       - |  5824 | `			sxu32 nSrc,nName;` |
|       - |  5825 | `			SySet sToken;` |
|       - |  5826 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5827 | `			sxi32 rcPromote;` |
|  104279 |  5828 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  104231 |  5829 | `				continue;` |
|       - |  5830 | `			}` |
|       - |  5831 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5832 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5833 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5834 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5835 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      53 |  5836 | `			nName = SyStringLength(&pArg->sName);` |
|      53 |  5837 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      53 |  5838 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      53 |  5839 | `			if( zSrc == 0 ){` |
|     ! 0 |  5840 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5841 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5842 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5843 | `				return SXERR_ABORT;` |
|       - |  5844 | `			}` |
|       - |  5845 | `			{` |
|      53 |  5846 | `				char *z = zSrc;` |
|      53 |  5847 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      53 |  5848 | `				z += sizeof("$this->")-1;` |
|      53 |  5849 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      53 |  5850 | `				z += nName;` |
|      53 |  5851 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      53 |  5852 | `				z += sizeof(" = $")-1;` |
|      53 |  5853 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      53 |  5854 | `				z += nName;` |
|      53 |  5855 | `				*z = 0;` |
|       - |  5856 | `			}` |
|      53 |  5857 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      53 |  5858 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      53 |  5859 | `			pTmpIn = pGen->pIn;` |
|      53 |  5860 | `			pTmpEnd = pGen->pEnd;` |
|      53 |  5861 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      53 |  5862 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      53 |  5863 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      53 |  5864 | `			pGen->pIn = pTmpIn;` |
|      53 |  5865 | `			pGen->pEnd = pTmpEnd;` |
|      53 |  5866 | `			SySetRelease(&sToken);` |
|      53 |  5867 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  5868 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5869 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5870 | `				return SXERR_ABORT;` |
|       - |  5871 | `			}` |
|       - |  5872 | `			/* Discard the assignment result — this is a statement expression. */` |
|      53 |  5873 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      29 |  5874 | `		}` |
|       - |  5875 | `	}` |
|       - |  5876 | `	/* Compile the body */` |
|  206935 |  5877 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5878 | `	/* Fix exception jumps now the destination is resolved */` |
|  206935 |  5879 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5880 | `	/* Emit the final return if not yet done */` |
|  206935 |  5881 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5882 | `	/* Fix gotos jumps now the destination is resolved */` |
|  206935 |  5883 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5884 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5885 | `	}` |
|  206935 |  5886 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5887 | `	/* Restore the default container */` |
|  206935 |  5888 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5889 | `	/* Leave function block */` |
|  206935 |  5890 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  206935 |  5891 | `	if( rc == SXERR_ABORT ){` |
|       - |  5892 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5893 | `		return SXERR_ABORT;` |
|       - |  5894 | `	}` |
|       - |  5895 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5896 | `	{` |
|  206935 |  5897 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5898 | `		sxu32 i;` |
| 4042283 |  5899 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 3835389 |  5900 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      41 |  5901 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      41 |  5902 | `				break;` |
|       - |  5903 | `			}` |
| 1917679 |  5904 | `		}` |
|       - |  5905 | `	}` |
|       - |  5906 | `	/* All done, function body compiled */` |
|  206935 |  5907 | `	return SXRET_OK;` |
|  103470 |  5908 |  |
|       - |  5909 | `/*` |
|       - |  5910 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5911 | ` * According to the PHP language reference manual.` |
|       - |  5912 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5913 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5914 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5915 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5916 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5917 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5918 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5919 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5920 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5921 | ` *` |
|       - |  5922 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5923 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5924 | ` * on these extension.` |
|       - |  5925 | ` */` |
|       - |  5926 | `/*` |
|       - |  5927 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5928 | ` */` |
|     334 |  5929 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  5930 |  |
|       - |  5931 | `	sxu32 i;` |
|     947 |  5932 | `	for( i = 0; i < n; i++ ){` |
|     811 |  5933 | `		int a = zA[i], b = zB[i];` |
|     811 |  5934 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     811 |  5935 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     811 |  5936 | `		if( a != b ) return a - b;` |
|     309 |  5937 | `	}` |
|     141 |  5938 | `	return 0;` |
|     172 |  5939 |  |
|       - |  5940 | `/*` |
|       - |  5941 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5942 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5943 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5944 | ` */` |
|       - |  5945 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5946 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5947 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5948 |  |
|       - |  5949 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5950 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5951 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5952 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5953 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5954 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5955 |  |
|       - |  5956 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5957 | `struct PhlTypeAtom {` |
|       - |  5958 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5959 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5960 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5961 | `	sxu32 nCanon;` |
|       - |  5962 | `};` |
|       - |  5963 |  |
|       - |  5964 | `/*` |
|       - |  5965 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5966 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5967 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5968 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5969 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5970 | ` * already be consumed by the caller.` |
|       - |  5971 | ` */` |
|   70300 |  5972 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  5973 |  |
|   70305 |  5974 | `	SyToken *pIn = pGen->pIn;` |
|   70305 |  5975 | `	SyZero(pOut, sizeof(*pOut));` |
|   70305 |  5976 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   70305 |  5977 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5978 | `		return SXERR_SYNTAX;` |
|       - |  5979 | `	}` |
|       - |  5980 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   70305 |  5981 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5982 | `		pIn++;` |
|       8 |  5983 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5984 | `			return SXERR_SYNTAX;` |
|       - |  5985 | `		}` |
|       3 |  5986 | `	}` |
|   70305 |  5987 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5988 | `		return SXERR_SYNTAX;` |
|       - |  5989 | `	}` |
|   70305 |  5990 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   60139 |  5991 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   60139 |  5992 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      24 |  5993 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   60129 |  5994 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      61 |  5995 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   60091 |  5996 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   16847 |  5997 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   51642 |  5998 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   43163 |  5999 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   21642 |  6000 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      32 |  6001 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      48 |  6002 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      28 |  6003 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      22 |  6004 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       4 |  6005 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  6006 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  6007 | `			pOut->sClass = pIn->sData;` |
|       4 |  6008 | `		}else{` |
|       3 |  6009 | `			return SXERR_SYNTAX;` |
|       - |  6010 | `		}` |
|   60137 |  6011 | `		pIn++;` |
|   30071 |  6012 | `	}else{` |
|       - |  6013 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6014 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   10171 |  6015 | `		SyString *pT = &pIn->sData;` |
|   10171 |  6016 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      18 |  6017 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      18 |  6018 | `			pIn++;` |
|   10163 |  6019 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     117 |  6020 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     117 |  6021 | `			pIn++;` |
|   10099 |  6022 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  6023 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  6024 | `			pIn++;` |
|       2 |  6025 | `		}else{` |
|       - |  6026 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   10041 |  6027 | `			SyToken *pFirst = pIn;` |
|   10041 |  6028 | `			SyToken *pLast = pIn;` |
|   10041 |  6029 | `			pOut->nType = SXU32_HIGH;` |
|   10041 |  6030 | `			pOut->sClass = pIn->sData;` |
|   10041 |  6031 | `			pIn++;` |
|   15057 |  6032 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   10044 |  6033 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6034 | `				pLast = &pIn[1];` |
|       3 |  6035 | `				pIn += 2;` |
|       1 |  6036 | `			}` |
|   10041 |  6037 | `			if( pLast != pFirst ){` |
|       3 |  6038 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6039 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6040 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6041 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6042 | `			}` |
|       - |  6043 | `		}` |
|       - |  6044 | `	}` |
|   70303 |  6045 | `	pGen->pIn = pIn;` |
|   70303 |  6046 | `	return SXRET_OK;` |
|   35155 |  6047 |  |
|       - |  6048 |  |
|       - |  6049 | `/*` |
|       - |  6050 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6051 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6052 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6053 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6054 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6055 | ` */` |
|   70196 |  6056 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6057 |  |
|       - |  6058 | `	int i;` |
|   70201 |  6059 | `	int nNonNull = 0;` |
|  140485 |  6060 | `	for( i = 0; i < nAtoms; i++ ){` |
|   70289 |  6061 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   70273 |  6062 | `			nNonNull++;` |
|   35134 |  6063 | `		}` |
|   35147 |  6064 | `	}` |
|   70201 |  6065 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6066 | `		/* Shorthand: ?T */` |
|      65 |  6067 | `		for( i = 0; i < nAtoms; i++ ){` |
|      65 |  6068 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      65 |  6069 | `			SyBlobAppend(pBlob, "?", 1);` |
|      65 |  6070 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      15 |  6071 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       9 |  6072 | `			}else{` |
|      53 |  6073 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6074 | `			}` |
|      65 |  6075 | `			return;` |
|     ! 0 |  6076 | `		}` |
|     ! 0 |  6077 | `	}` |
|       - |  6078 | `	{` |
|   70139 |  6079 | `		int bFirst = 1;` |
|       - |  6080 | `		/* 1) Classes in declaration order */` |
|  140355 |  6081 | `		for( i = 0; i < nAtoms; i++ ){` |
|   70221 |  6082 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   10033 |  6083 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   10033 |  6084 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   10033 |  6085 | `				bFirst = 0;` |
|    5014 |  6086 | `			}` |
|   35113 |  6087 | `		}` |
|       - |  6088 | `		/* 2) Built-ins in canonical order */` |
|       - |  6089 | `		{` |
|       - |  6090 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6091 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6092 | `			int k;` |
|  490943 |  6093 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  781941 |  6094 | `				for( i = 0; i < nAtoms; i++ ){` |
|  421205 |  6095 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   60073 |  6096 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   60073 |  6097 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   60073 |  6098 | `						bFirst = 0;` |
|   60073 |  6099 | `						break;` |
|       - |  6100 | `					}` |
|  180571 |  6101 | `				}` |
|  210407 |  6102 | `			}` |
|       - |  6103 | `		}` |
|       - |  6104 | `		/* 3) null suffix */` |
|   70139 |  6105 | `		if( bNullable ){` |
|      12 |  6106 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      12 |  6107 | `			SyBlobAppend(pBlob, "null", 4);` |
|       5 |  6108 | `		}` |
|       - |  6109 | `	}` |
|   35103 |  6110 |  |
|       - |  6111 |  |
|       - |  6112 | `/*` |
|       - |  6113 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6114 | ` *` |
|       - |  6115 | ` * Outputs:` |
|       - |  6116 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6117 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6118 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6119 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6120 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6121 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6122 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6123 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6124 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6125 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6126 | ` *` |
|       - |  6127 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6128 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6129 | ` */` |
|   70206 |  6130 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6131 | `	ph7_gen_state *pGen,` |
|       - |  6132 | `	sxu32 *pnType,` |
|       - |  6133 | `	SyString *pClass,` |
|       - |  6134 | `	SySet *pAlts,` |
|       - |  6135 | `	sxi32 *piTypeFlags,` |
|       - |  6136 | `	SyString *pTypeText,` |
|       - |  6137 | `	int iNullableFlag,` |
|       - |  6138 | `	int iUnionFlag,` |
|       - |  6139 | `	int bAllowVoid,` |
|       - |  6140 | `	sxu32 nLine` |
|       5 |  6141 | `){` |
|       - |  6142 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   70211 |  6143 | `	int nAtoms = 0;` |
|   70211 |  6144 | `	int bShortNullable = 0;` |
|   70211 |  6145 | `	int bExplicitNull = 0;` |
|       - |  6146 | `	sxi32 rc;` |
|   70211 |  6147 | `	*pnType = 0;` |
|   70211 |  6148 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   70211 |  6149 | `	*piTypeFlags = 0;` |
|   70211 |  6150 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6151 |  |
|   70211 |  6152 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6153 | `		return SXRET_OK;` |
|       - |  6154 | `	}` |
|       - |  6155 | ``	/* Optional `?` shorthand prefix */`` |
|   70206 |  6156 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      63 |  6157 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      61 |  6158 | `		bShortNullable = 1;` |
|      61 |  6159 | `		pGen->pIn++;` |
|      61 |  6160 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6161 | `			return SXERR_SYNTAX;` |
|       - |  6162 | `		}` |
|      29 |  6163 | `	}` |
|       - |  6164 | `	/* First atom is mandatory */` |
|   70211 |  6165 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   70211 |  6166 | `	if( rc != SXRET_OK ){` |
|       3 |  6167 | `		return rc;` |
|       - |  6168 | `	}` |
|   70209 |  6169 | `	nAtoms = 1;` |
|       - |  6170 | ``	/* Subsequent atoms separated by `\|` */`` |
|  105449 |  6171 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   70352 |  6172 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     101 |  6173 | `		if( bShortNullable ){` |
|       - |  6174 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6175 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6176 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  6177 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6178 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6179 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6180 | `		}` |
|      99 |  6181 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6182 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6183 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6184 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6185 | `		}` |
|      99 |  6186 | ``		pGen->pIn++; /* skip `\|` */`` |
|      99 |  6187 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      99 |  6188 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  6189 | `			return rc;` |
|       - |  6190 | `		}` |
|      99 |  6191 | `		nAtoms++;` |
|       5 |  6192 | `	}` |
|       - |  6193 | `	/* Validation pass.` |
|       - |  6194 | `	 *` |
|       - |  6195 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6196 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6197 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6198 | `	 */` |
|       - |  6199 | `	{` |
|       - |  6200 | `		int i, j;` |
|   70207 |  6201 | `		int bHasNonNull = 0;` |
|  140497 |  6202 | `		for( i = 0; i < nAtoms; i++ ){` |
|   70301 |  6203 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     117 |  6204 | `				if( nAtoms > 1 ){` |
|       3 |  6205 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6206 | `						"Void can only be used as a standalone type");` |
|       3 |  6207 | `					return SXERR_SYNTAX;` |
|       - |  6208 | `				}` |
|     115 |  6209 | `				if( !bAllowVoid ){` |
|     ! 0 |  6210 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6211 | `						"void cannot be used here");` |
|     ! 0 |  6212 | `					return SXERR_SYNTAX;` |
|       - |  6213 | `				}` |
|     115 |  6214 | `				if( bShortNullable ){` |
|     ! 0 |  6215 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6216 | `						"Void type cannot be nullable");` |
|     ! 0 |  6217 | `					return SXERR_SYNTAX;` |
|       - |  6218 | `				}` |
|      55 |  6219 | `			}` |
|   70299 |  6220 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6221 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6222 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6223 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6224 | `				 * does not return), and folding them would mislead any` |
|       - |  6225 | `				 * future return-enforcement work. */` |
|       3 |  6226 | `				if( nAtoms > 1 ){` |
|       3 |  6227 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6228 | `						"never can only be used as a standalone type");` |
|       3 |  6229 | `					return SXERR_SYNTAX;` |
|       - |  6230 | `				}` |
|     ! 0 |  6231 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6232 | `					"never type is not yet implemented");` |
|     ! 0 |  6233 | `				return SXERR_SYNTAX;` |
|       - |  6234 | `			}` |
|   70297 |  6235 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      18 |  6236 | `				bExplicitNull = 1;` |
|      10 |  6237 | `			}else{` |
|   70281 |  6238 | `				bHasNonNull = 1;` |
|       - |  6239 | `			}` |
|       - |  6240 | `			/* Duplicate detection */` |
|   70421 |  6241 | `			for( j = 0; j < i; j++ ){` |
|     131 |  6242 | `				int bDup = 0;` |
|     131 |  6243 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      18 |  6244 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6245 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      15 |  6246 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6247 | `								aAtoms[j].sClass.zString,` |
|      12 |  6248 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6249 | `							bDup = 1;` |
|     ! 0 |  6250 | `						}` |
|       9 |  6251 | `					}else{` |
|       3 |  6252 | `						bDup = 1;` |
|       - |  6253 | `					}` |
|       7 |  6254 | `				}` |
|     131 |  6255 | `				if( bDup ){` |
|       - |  6256 | `					const char *zName;` |
|       - |  6257 | `					sxu32 nName;` |
|       3 |  6258 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6259 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6260 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6261 | `					}else{` |
|       3 |  6262 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6263 | `						nName = aAtoms[i].nCanon;` |
|       - |  6264 | `					}` |
|       4 |  6265 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6266 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6267 | `					return SXERR_SYNTAX;` |
|       - |  6268 | `				}` |
|      67 |  6269 | `			}` |
|   35150 |  6270 | `		}` |
|   70201 |  6271 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6272 | `			if( bShortNullable ){` |
|       - |  6273 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6274 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6275 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6276 | `				return SXERR_SYNTAX;` |
|       - |  6277 | `			}` |
|       - |  6278 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6279 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6280 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6281 | `			 * atom, so set it here. */` |
|       7 |  6282 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6283 | `		}` |
|       - |  6284 | `	}` |
|       - |  6285 | `	/* Compute nullability flag */` |
|   70201 |  6286 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      76 |  6287 | `		*piTypeFlags \|= iNullableFlag;` |
|      36 |  6288 | `	}` |
|       - |  6289 | `	/* Build canonical type text */` |
|   70201 |  6290 | `	if( pTypeText ){` |
|       - |  6291 | `		SyBlob sBlob;` |
|   70201 |  6292 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  105271 |  6293 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   35098 |  6294 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   70201 |  6295 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  105134 |  6296 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   70086 |  6297 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   70091 |  6298 | `			if( zDup ){` |
|   70091 |  6299 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   35043 |  6300 | `			}` |
|   35043 |  6301 | `		}` |
|   70201 |  6302 | `		SyBlobRelease(&sBlob);` |
|   35098 |  6303 | `	}` |
|       - |  6304 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6305 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6306 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6307 | `	{` |
|   70201 |  6308 | `		int nNonNull = 0;` |
|   70201 |  6309 | `		int iNonNullIdx = -1;` |
|       - |  6310 | `		int i;` |
|  140485 |  6311 | `		for( i = 0; i < nAtoms; i++ ){` |
|   70289 |  6312 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   70273 |  6313 | `				nNonNull++;` |
|   70273 |  6314 | `				iNonNullIdx = i;` |
|   35134 |  6315 | `			}` |
|   35147 |  6316 | `		}` |
|   70201 |  6317 | `		if( nNonNull <= 1 ){` |
|       - |  6318 | `			/* Fast path: store as single type. */` |
|   70139 |  6319 | `			if( iNonNullIdx >= 0 ){` |
|   70133 |  6320 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   70133 |  6321 | `				if( pA->nType == SXU32_HIGH ){` |
|   15023 |  6322 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    5006 |  6323 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   10017 |  6324 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   10017 |  6325 | `					*pnType = SXU32_HIGH;` |
|   10017 |  6326 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   65127 |  6327 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     115 |  6328 | `					*pnType = MEMOBJ_VOID;` |
|      60 |  6329 | `				}else{` |
|       - |  6330 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6331 | `					 * pass above rejects it as not-yet-implemented. */` |
|   60011 |  6332 | `					*pnType = pA->nType;` |
|       - |  6333 | `				}` |
|   35064 |  6334 | `			}` |
|   35072 |  6335 | `		}else{` |
|       - |  6336 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      67 |  6337 | `			*piTypeFlags \|= iUnionFlag;` |
|     211 |  6338 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6339 | `				ph7_type_alt sAlt;` |
|     149 |  6340 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     145 |  6341 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     145 |  6342 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      45 |  6343 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      14 |  6344 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      31 |  6345 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      31 |  6346 | `					sAlt.nType = SXU32_HIGH;` |
|      31 |  6347 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      17 |  6348 | `				}else{` |
|     117 |  6349 | `					sAlt.nType = aAtoms[i].nType;` |
|     117 |  6350 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6351 | `				}` |
|     145 |  6352 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      75 |  6353 | `			}` |
|       - |  6354 | `		}` |
|       - |  6355 | `	}` |
|   70201 |  6356 | `	return SXRET_OK;` |
|   35108 |  6357 |  |
|       - |  6358 |  |
|       - |  6359 | `/*` |
|       - |  6360 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6361 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6362 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6363 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6364 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6365 | `` *          and union types `: T\|U`.`` |
|       - |  6366 | ` */` |
|  293066 |  6367 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6368 |  |
|  293071 |  6369 | `	sxi32 iFlags = 0;` |
|       - |  6370 | `	sxi32 rc;` |
|       - |  6371 | `	sxu32 nLine;` |
|  293071 |  6372 | `	pFunc->nReturnType = 0;` |
|  293071 |  6373 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  293071 |  6374 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  293071 |  6375 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  292719 |  6376 | `		return SXRET_OK;` |
|       - |  6377 | `	}` |
|     357 |  6378 | `	pGen->pIn++; /* Skip ':' */` |
|     357 |  6379 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6380 | `		return SXRET_OK;` |
|       - |  6381 | `	}` |
|     357 |  6382 | `	nLine = pGen->pIn->nLine;` |
|     357 |  6383 | `	rc = GenStateParseUnionTypeDecl(` |
|     176 |  6384 | `		pGen,` |
|     176 |  6385 | `		&pFunc->nReturnType,` |
|     176 |  6386 | `		&pFunc->sReturnClass,` |
|     176 |  6387 | `		&pFunc->aReturnUnion,` |
|       - |  6388 | `		&iFlags,` |
|     176 |  6389 | `		&pFunc->sReturnTypeName,` |
|       - |  6390 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6391 | `		/* iUnionFlag */ 0,` |
|       - |  6392 | `		/* bAllowVoid */ 1,` |
|     176 |  6393 | `		nLine);` |
|     176 |  6394 | `	(void)iFlags;` |
|     357 |  6395 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6396 | `		return SXERR_ABORT;` |
|       - |  6397 | `	}` |
|     357 |  6398 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6399 | `		/* Error already reported */` |
|     ! 0 |  6400 | `		return SXERR_SYNTAX;` |
|       - |  6401 | `	}` |
|     357 |  6402 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6403 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6404 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6405 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6406 | `				&pGen->pIn->sData);` |
|       3 |  6407 | `		}else{` |
|     ! 0 |  6408 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6409 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6410 | `		}` |
|       5 |  6411 | `		return SXERR_SYNTAX;` |
|       - |  6412 | `	}` |
|     353 |  6413 | `	return SXRET_OK;` |
|  146538 |  6414 |  |
|       - |  6415 |  |
|   44080 |  6416 | `static sxi32 GenStateCompileFunc(` |
|       - |  6417 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6418 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6419 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6420 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6421 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6422 | `	)` |
|       5 |  6423 |  |
|       - |  6424 | `	ph7_vm_func *pFunc;` |
|       - |  6425 | `	SyToken *pEnd;` |
|       - |  6426 | `	sxu32 nLine;` |
|       - |  6427 | `	char *zName;` |
|       - |  6428 | `	sxi32 rc;` |
|       - |  6429 | `	/* Extract line number */` |
|   44085 |  6430 | `	nLine = pGen->pIn->nLine;` |
|       - |  6431 | `	/* Jump the left parenthesis '(' */` |
|   44085 |  6432 | `	pGen->pIn++;` |
|       - |  6433 | `	/* Delimit the function signature */` |
|   44085 |  6434 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   44085 |  6435 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6436 | `		/* Syntax error */` |
|       9 |  6437 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       9 |  6438 | `		if( rc == SXERR_ABORT ){` |
|       - |  6439 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6440 | `			return SXERR_ABORT;` |
|       - |  6441 | `		}` |
|       9 |  6442 | `		pGen->pIn = pGen->pEnd;` |
|       9 |  6443 | `		return SXRET_OK;` |
|       - |  6444 | `	}` |
|       - |  6445 | `	/* Create the function state */` |
|   44079 |  6446 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   44079 |  6447 | `	if( pFunc == 0 ){` |
|     ! 0 |  6448 | `		goto OutOfMem;` |
|       - |  6449 | `	}` |
|       - |  6450 | `	/* Build the function name, prepending namespace if active */` |
|   44086 |  6451 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6452 | `		SyBlob sFQN;` |
|       - |  6453 | `		sxu32 nLen;` |
|      16 |  6454 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6455 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6456 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6457 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6458 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6459 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6460 | `		SyBlobRelease(&sFQN);` |
|      16 |  6461 | `		if( zName == 0 ){` |
|     ! 0 |  6462 | `			goto OutOfMem;` |
|       - |  6463 | `		}` |
|      16 |  6464 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6465 | `	}else{` |
|   44065 |  6466 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   44065 |  6467 | `		if( zName == 0 ){` |
|     ! 0 |  6468 | `			goto OutOfMem;` |
|       - |  6469 | `		}` |
|   44065 |  6470 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6471 | `	}` |
|   44079 |  6472 | `	if( pGen->pIn < pEnd ){` |
|       - |  6473 | `		/* Collect function arguments */` |
|   30517 |  6474 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   30517 |  6475 | `		if( rc == SXERR_ABORT ){` |
|       - |  6476 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6477 | `			return SXERR_ABORT;` |
|       - |  6478 | `		}` |
|   15256 |  6479 | `	}` |
|       - |  6480 | `	/* Point past ')' and parse optional return type ': type' */` |
|   44079 |  6481 | `	pGen->pIn = &pEnd[1];` |
|       - |  6482 | `	{` |
|   44079 |  6483 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   44079 |  6484 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6485 | `			return SXERR_ABORT;` |
|   44079 |  6486 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6487 | `			return SXERR_SYNTAX;` |
|       - |  6488 | `		}` |
|       - |  6489 | `	}` |
|   44075 |  6490 | `	if( bHandleClosure ){` |
|       - |  6491 | `		ph7_vm_func_closure_env sEnv;` |
|     253 |  6492 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     248 |  6493 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     137 |  6494 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      21 |  6495 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6496 | `				/* Closure,record environment variable */` |
|      21 |  6497 | `				pGen->pIn++;` |
|      21 |  6498 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6499 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6500 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6501 | `						return SXERR_ABORT;` |
|       - |  6502 | `					}` |
|     ! 0 |  6503 | `				}` |
|      21 |  6504 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6505 | `				/* Compile until we hit the first closing parenthesis */` |
|      41 |  6506 | `				while( pGen->pIn < pGen->pEnd ){` |
|      41 |  6507 | `					int iFlagsLocal = 0;` |
|      41 |  6508 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      21 |  6509 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      21 |  6510 | `						break;` |
|       - |  6511 | `					}` |
|      25 |  6512 | `					nLineLocal = pGen->pIn->nLine;` |
|      25 |  6513 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6514 | `						/* Pass by reference,record that */` |
|     ! 0 |  6515 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6516 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6517 | `							);` |
|     ! 0 |  6518 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6519 | `						pGen->pIn++;` |
|     ! 0 |  6520 | `					}` |
|      20 |  6521 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      25 |  6522 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6523 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6524 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6525 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6526 | `								return SXERR_ABORT;` |
|       - |  6527 | `							}` |
|       - |  6528 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6529 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6530 | `								pGen->pIn++;` |
|     ! 0 |  6531 | `							}` |
|     ! 0 |  6532 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6533 | `								pGen->pIn++;` |
|     ! 0 |  6534 | `							}` |
|     ! 0 |  6535 | `							break;` |
|       - |  6536 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6537 | `					}else{` |
|       - |  6538 | `						SyString *pNameLocal;` |
|       - |  6539 | `						char *zDup;` |
|       - |  6540 | `						/* Duplicate variable name */` |
|      25 |  6541 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      25 |  6542 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      25 |  6543 | `						if( zDup ){` |
|       - |  6544 | `							/* Zero the structure */` |
|      25 |  6545 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      25 |  6546 | `							sEnv.iFlags = iFlagsLocal;` |
|      25 |  6547 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      25 |  6548 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      25 |  6549 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6550 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6551 | `									got_this = 1;` |
|     ! 0 |  6552 | `							}` |
|       - |  6553 | `							/* Save imported variable */` |
|      25 |  6554 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      15 |  6555 | `						}else{` |
|     ! 0 |  6556 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6557 | `							 return SXERR_ABORT;` |
|       - |  6558 | `						}` |
|       - |  6559 | `					}` |
|      25 |  6560 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      31 |  6561 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6562 | `						/* Ignore trailing commas */` |
|       7 |  6563 | `						pGen->pIn++;` |
|       1 |  6564 | `					}` |
|       5 |  6565 | `				}` |
|      21 |  6566 | `				if( !got_this ){` |
|       - |  6567 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6568 | `					 * available to the closure environment.` |
|       - |  6569 | `					 */` |
|      21 |  6570 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      21 |  6571 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      21 |  6572 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      21 |  6573 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      21 |  6574 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       8 |  6575 | `				}` |
|      21 |  6576 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6577 | `					/* Mark as closure */` |
|      21 |  6578 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       8 |  6579 | `				}` |
|       8 |  6580 | `		}` |
|     124 |  6581 | `	}` |
|       - |  6582 | `	/* Compile the body */` |
|   44075 |  6583 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   44075 |  6584 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6585 | `		return SXERR_ABORT;` |
|       - |  6586 | `	}` |
|   44075 |  6587 | `	if( ppFunc ){` |
|     253 |  6588 | `		*ppFunc = pFunc;` |
|     124 |  6589 | `	}` |
|   44075 |  6590 | `	rc = SXRET_OK;` |
|   44075 |  6591 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6592 | `		/* Finally register the function */` |
|   44059 |  6593 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   22027 |  6594 | `	}` |
|   44075 |  6595 | `	if( rc == SXRET_OK ){` |
|   44075 |  6596 | `		return SXRET_OK;` |
|       - |  6597 | `	}` |
|       - |  6598 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6599 | `OutOfMem:` |
|       - |  6600 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6601 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6602 | `	 */` |
|     ! 0 |  6603 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6604 | `	return SXERR_ABORT;` |
|   22045 |  6605 |  |
|       - |  6606 | `/*` |
|       - |  6607 | ` * Compile a standard PHP function.` |
|       - |  6608 | ` *  Refer to the block-comment above for more information.` |
|       - |  6609 | ` */` |
|   43840 |  6610 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6611 |  |
|       - |  6612 | `	SyString *pName;` |
|       - |  6613 | `	sxi32 iFlags;` |
|       - |  6614 | `	sxu32 nLine;` |
|       - |  6615 | `	sxi32 rc;` |
|       - |  6616 |  |
|   43845 |  6617 | `	nLine = pGen->pIn->nLine;` |
|   43845 |  6618 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   43845 |  6619 | `	iFlags = 0;` |
|   43845 |  6620 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6621 | `		/* Return by reference,remember that */` |
|       7 |  6622 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6623 | `		/* Jump the '&' token */` |
|       7 |  6624 | `		pGen->pIn++;` |
|       3 |  6625 | `	}` |
|   43845 |  6626 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6627 | `		/* Invalid function name */` |
|       8 |  6628 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       8 |  6629 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6630 | `			return SXERR_ABORT;` |
|       - |  6631 | `		}` |
|       - |  6632 | `		/* Sychronize with the next semi-colon or braces*/` |
|      22 |  6633 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      16 |  6634 | `			pGen->pIn++;` |
|       2 |  6635 | `		}` |
|       8 |  6636 | `		return SXRET_OK;` |
|       - |  6637 | `	}` |
|   43839 |  6638 | `	pName = &pGen->pIn->sData;` |
|   43839 |  6639 | `	nLine = pGen->pIn->nLine;` |
|       - |  6640 | `	/* Jump the function name */` |
|   43839 |  6641 | `	pGen->pIn++;` |
|   43839 |  6642 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6643 | `		/* Syntax error */` |
|       3 |  6644 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6645 | `		if( rc == SXERR_ABORT ){` |
|       - |  6646 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6647 | `			return SXERR_ABORT;` |
|       - |  6648 | `		}` |
|       - |  6649 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6650 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6651 | `			pGen->pIn++;` |
|     ! 0 |  6652 | `		}` |
|       3 |  6653 | `		return SXRET_OK;` |
|       - |  6654 | `	}` |
|       - |  6655 | `	/* Compile function body */` |
|   43837 |  6656 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   43837 |  6657 | `	return rc;` |
|   21925 |  6658 |  |
|       - |  6659 | `/*` |
|       - |  6660 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6661 | ` * According to the PHP language reference manual` |
|       - |  6662 | ` *  Visibility:` |
|       - |  6663 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6664 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6665 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6666 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6667 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6668 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6669 | ` */` |
|  312348 |  6670 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  6671 |  |
|  312353 |  6672 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   10013 |  6673 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  302345 |  6674 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   43031 |  6675 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6676 | `	}` |
|       - |  6677 | `	/* Assume public by default */` |
|  259319 |  6678 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  156179 |  6679 |  |
|       - |  6680 | `/*` |
|       - |  6681 | ` * Compile a class constant.` |
|       - |  6682 | ` * According to the PHP language reference manual` |
|       - |  6683 | ` *  Class Constants` |
|       - |  6684 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6685 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6686 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6687 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6688 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6689 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6690 | ` * Symisc eXtension.` |
|       - |  6691 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6692 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6693 | ` *  Example:` |
|       - |  6694 | ` *   class Test{` |
|       - |  6695 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6696 | ` *   };` |
|       - |  6697 | ` *   var_dump(TEST::MyConst);` |
|       - |  6698 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6699 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6700 | ` */` |
|       - |  6701 | `/*` |
|       - |  6702 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  6703 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  6704 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  6705 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  6706 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  6707 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  6708 | ` */` |
|      76 |  6709 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  6710 |  |
|       - |  6711 | `	SyToken *p0, *p1;` |
|      81 |  6712 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6713 | `		return 0;` |
|       - |  6714 | `	}` |
|      81 |  6715 | `	p0 = pGen->pIn;` |
|       - |  6716 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      81 |  6717 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  6718 | `		return 1;` |
|       - |  6719 | `	}` |
|      81 |  6720 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  6721 | `		return 1;` |
|       - |  6722 | `	}` |
|       - |  6723 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  6724 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  6725 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      77 |  6726 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      77 |  6727 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      77 |  6728 | `		if( p1 ){` |
|      77 |  6729 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      24 |  6730 | `				return 1;` |
|       - |  6731 | `			}` |
|      56 |  6732 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  6733 | `				return 1;` |
|       - |  6734 | `			}` |
|      24 |  6735 | `		}` |
|      24 |  6736 | `	}` |
|      52 |  6737 | `	return 0;` |
|      43 |  6738 |  |
|       - |  6739 | `/*` |
|       - |  6740 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  6741 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  6742 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  6743 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  6744 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  6745 | ` * share the same backing.` |
|       - |  6746 | ` */` |
|     192 |  6747 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  6748 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  6749 |  |
|     197 |  6750 | `	pAttr->nType = nType;` |
|     197 |  6751 | `	pAttr->sClass = *pClass;` |
|     197 |  6752 | `	pAttr->sTypeName = *pTypeName;` |
|     197 |  6753 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  6754 | `		sxu32 i;` |
|      46 |  6755 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      32 |  6756 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      32 |  6757 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      18 |  6758 | `		}` |
|       7 |  6759 | `	}` |
|     197 |  6760 |  |
|      76 |  6761 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  6762 |  |
|      81 |  6763 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6764 | `	SySet *pInstrContainer;` |
|       - |  6765 | `	ph7_class_attr *pCons;` |
|       - |  6766 | `	SyString *pName;` |
|       - |  6767 | `	sxi32 rc;` |
|      81 |  6768 | `	sxu32 nType = 0;` |
|       - |  6769 | `	SyString sTypeClass;` |
|       - |  6770 | `	SyString sTypeText;` |
|       - |  6771 | `	SySet aUnionAlts;` |
|      81 |  6772 | `	sxi32 iTypeFlags = 0;` |
|      81 |  6773 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      81 |  6774 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      81 |  6775 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  6776 | `	/* Extract visibility level */` |
|      81 |  6777 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  6778 | `	/* Mark as constant */` |
|      81 |  6779 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      81 |  6780 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  6781 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  6782 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|      95 |  6783 | `	if( GenStateClassConstHasType(pGen) ){` |
|      46 |  6784 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      28 |  6785 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  6786 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  6787 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  6788 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  6789 | `		 * and success paths release. */` |
|      32 |  6790 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  6791 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  6792 | `			goto Synchronize;` |
|      32 |  6793 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  6794 | `			return SXERR_ABORT;` |
|      32 |  6795 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  6796 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  6797 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  6798 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6799 | `				return SXERR_ABORT;` |
|       - |  6800 | `			}` |
|     ! 0 |  6801 | `			goto Synchronize;` |
|       - |  6802 | `		}` |
|      32 |  6803 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      14 |  6804 | `	}` |
|      38 |  6805 | `loop:` |
|      83 |  6806 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6807 | `		/* Invalid constant name */` |
|     ! 0 |  6808 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6809 | `		if( rc == SXERR_ABORT ){` |
|       - |  6810 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6811 | `			return SXERR_ABORT;` |
|       - |  6812 | `		}` |
|     ! 0 |  6813 | `		goto Synchronize;` |
|       - |  6814 | `	}` |
|       - |  6815 | `	/* Peek constant name */` |
|      83 |  6816 | `	pName = &pGen->pIn->sData;` |
|       - |  6817 | `	/* Make sure the constant name isn't reserved */` |
|      83 |  6818 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6819 | `		/* Reserved constant name */` |
|     ! 0 |  6820 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6821 | `		if( rc == SXERR_ABORT ){` |
|       - |  6822 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6823 | `			return SXERR_ABORT;` |
|       - |  6824 | `		}` |
|     ! 0 |  6825 | `		goto Synchronize;` |
|       - |  6826 | `	}` |
|       - |  6827 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      83 |  6828 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      46 |  6829 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      28 |  6830 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      14 |  6831 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      32 |  6832 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6833 | `			return SXERR_ABORT;` |
|      32 |  6834 | `		}else if( rc != SXRET_OK ){` |
|       3 |  6835 | `			goto Synchronize;` |
|       - |  6836 | `		}` |
|      13 |  6837 | `	}` |
|       - |  6838 | `	/* Advance the stream cursor */` |
|      81 |  6839 | `	pGen->pIn++;` |
|      81 |  6840 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6841 | `		/* Invalid declaration */` |
|     ! 0 |  6842 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6843 | `		if( rc == SXERR_ABORT ){` |
|       - |  6844 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6845 | `			return SXERR_ABORT;` |
|       - |  6846 | `		}` |
|     ! 0 |  6847 | `		goto Synchronize;` |
|       - |  6848 | `	}` |
|      81 |  6849 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6850 | `	/* Allocate a new class attribute */` |
|      81 |  6851 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      81 |  6852 | `	if( pCons == 0 ){` |
|     ! 0 |  6853 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6854 | `		return SXERR_ABORT;` |
|       - |  6855 | `	}` |
|      81 |  6856 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      29 |  6857 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      13 |  6858 | `	}` |
|       - |  6859 | `	/* Swap bytecode container */` |
|      81 |  6860 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      81 |  6861 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6862 | `	/* Compile constant value.` |
|       - |  6863 | `	 */` |
|      81 |  6864 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      81 |  6865 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6866 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6867 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6868 | `			return SXERR_ABORT;` |
|       - |  6869 | `		}` |
|       1 |  6870 | `	}` |
|       - |  6871 | `	/* Emit the done instruction */` |
|      81 |  6872 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      81 |  6873 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      81 |  6874 | `	if( rc == SXERR_ABORT ){` |
|       - |  6875 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6876 | `		return SXERR_ABORT;` |
|       - |  6877 | `	}` |
|       - |  6878 | `	/* All done,install the constant */` |
|      81 |  6879 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      81 |  6880 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6881 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6882 | `		return SXERR_ABORT;` |
|       - |  6883 | `	}` |
|      81 |  6884 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6885 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  6886 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  6887 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6888 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6889 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6890 | `				pTok--;` |
|     ! 0 |  6891 | `			}` |
|     ! 0 |  6892 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6893 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6894 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6895 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6896 | `				return SXERR_ABORT;` |
|       - |  6897 | `			}` |
|     ! 0 |  6898 | `		}else{` |
|       3 |  6899 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  6900 | `				goto loop;` |
|       - |  6901 | `			}` |
|       - |  6902 | `		}` |
|     ! 0 |  6903 | `	}` |
|      79 |  6904 | `	SySetRelease(&aUnionAlts);` |
|      79 |  6905 | `	return SXRET_OK;` |
|       1 |  6906 | `Synchronize:` |
|       3 |  6907 | `	SySetRelease(&aUnionAlts);` |
|       - |  6908 | `	/* Synchronize with the first semi-colon */` |
|       9 |  6909 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  6910 | `		pGen->pIn++;` |
|       1 |  6911 | `	}` |
|       3 |  6912 | `	return SXERR_CORRUPT;` |
|      43 |  6913 |  |
|       - |  6914 | `/*` |
|       - |  6915 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6916 | ` * According to the PHP language reference manual` |
|       - |  6917 | ` *  Properties` |
|       - |  6918 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6919 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6920 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6921 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6922 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6923 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6924 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6925 | ` * Symisc eXtension.` |
|       - |  6926 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6927 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6928 | ` *  Example:` |
|       - |  6929 | ` *   class Test{` |
|       - |  6930 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6931 | ` *   };` |
|       - |  6932 | ` *   var_dump(TEST::myVar);` |
|       - |  6933 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6934 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6935 | ` */` |
|       - |  6936 | `/*` |
|       - |  6937 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6938 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6939 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6940 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6941 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6942 | ` */` |
|  162996 |  6943 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  6944 |  |
|  163001 |  6945 | `	SyToken *p = pStart;` |
|  163001 |  6946 | `	if( p >= pEnd ) return 0;` |
|  163001 |  6947 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      18 |  6948 | `		p++;` |
|      18 |  6949 | `		if( p >= pEnd ) return 0;` |
|       8 |  6950 | `	}` |
|  163001 |  6951 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6952 | `		p++;` |
|       3 |  6953 | `		if( p >= pEnd ) return 0;` |
|       1 |  6954 | `	}` |
|  163001 |  6955 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6956 | `		return 0;` |
|       - |  6957 | `	}` |
|       - |  6958 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6959 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6960 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6961 | `	 * dispatch site. */` |
|  163001 |  6962 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  162979 |  6963 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  163048 |  6964 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3516 |  6965 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  162831 |  6966 | `			return 0;` |
|       - |  6967 | `		}` |
|      74 |  6968 | `	}` |
|     175 |  6969 | `	p++;` |
|       - |  6970 | `	/* Consume optional namespace path */` |
|     177 |  6971 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6972 | `		p += 2;` |
|       1 |  6973 | `	}` |
|       - |  6974 | ``	/* Consume any `\| Type` union alternatives */`` |
|     273 |  6975 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|     108 |  6976 | `		&& p->sData.zString[0] == '\|' ){` |
|      16 |  6977 | `		p++;` |
|      16 |  6978 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      16 |  6979 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      16 |  6980 | `		p++;` |
|      16 |  6981 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6982 | `			p += 2;` |
|     ! 0 |  6983 | `		}` |
|       4 |  6984 | `	}` |
|     175 |  6985 | `	if( p >= pEnd ) return 0;` |
|     175 |  6986 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   81503 |  6987 |  |
|       - |  6988 |  |
|       - |  6989 | `/*` |
|       - |  6990 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6991 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6992 | ` * if not). Recognized forms:` |
|       - |  6993 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6994 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6995 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6996 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6997 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6998 | ` * on unrecoverable error.` |
|       - |  6999 | ` *` |
|       - |  7000 | ` * When a type is parsed:` |
|       - |  7001 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7002 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7003 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7004 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7005 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7006 | ` */` |
|     170 |  7007 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7008 | `	ph7_gen_state *pGen,` |
|       - |  7009 | `	sxu32 *pnType,` |
|       - |  7010 | `	SyString *pClass,` |
|       - |  7011 | `	sxi32 *piTypeFlags,` |
|       - |  7012 | `	SyString *pTypeText,` |
|       - |  7013 | `	SySet *pAlts` |
|       5 |  7014 | `){` |
|     175 |  7015 | `	sxi32 iFlags = 0;` |
|       - |  7016 | `	sxi32 rc;` |
|     175 |  7017 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7018 | `		return SXRET_OK;` |
|       - |  7019 | `	}` |
|       - |  7020 | `	/* If the first token is '$', there's no type */` |
|     175 |  7021 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7022 | `		return SXRET_OK;` |
|       - |  7023 | `	}` |
|     175 |  7024 | `	rc = GenStateParseUnionTypeDecl(` |
|      85 |  7025 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7026 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7027 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7028 | `		/* bAllowVoid */ 0,` |
|     170 |  7029 | `		pGen->pIn->nLine);` |
|     175 |  7030 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7031 | `		return rc;` |
|       - |  7032 | `	}` |
|       - |  7033 | `	/* Verify next token is '$' (start of property name) */` |
|     175 |  7034 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7035 | `		return SXERR_SYNTAX;` |
|       - |  7036 | `	}` |
|     175 |  7037 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     175 |  7038 | `	return SXRET_OK;` |
|      90 |  7039 |  |
|       - |  7040 |  |
|       - |  7041 | `/*` |
|       - |  7042 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7043 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7044 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7045 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7046 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7047 | ` * by the type parser itself before reaching here.` |
|       - |  7048 | ` *` |
|       - |  7049 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7050 | ` * use in the error message.` |
|       - |  7051 | ` */` |
|     284 |  7052 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7053 | `	sxu32 nType,` |
|       - |  7054 | `	const SyString *pClass,` |
|       - |  7055 | `	const char **pzName,` |
|       - |  7056 | `	sxu32 *pnName)` |
|       5 |  7057 |  |
|       - |  7058 | `	const char *z;` |
|       - |  7059 | `	sxu32 n;` |
|     289 |  7060 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     253 |  7061 | `		return 0;` |
|       - |  7062 | `	}` |
|      41 |  7063 | `	z = pClass->zString;` |
|      41 |  7064 | `	n = pClass->nByte;` |
|      41 |  7065 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7066 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7067 | `	}` |
|       - |  7068 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7069 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7070 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      33 |  7071 | `	return 0;` |
|     147 |  7072 |  |
|       - |  7073 |  |
|       - |  7074 | `/*` |
|       - |  7075 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7076 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7077 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7078 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7079 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7080 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7081 | ` *` |
|       - |  7082 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7083 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7084 | ` */` |
|     248 |  7085 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7086 | `	ph7_gen_state *pGen,` |
|       - |  7087 | `	ph7_class *pClass,` |
|       - |  7088 | `	const SyString *pMemberName,` |
|       - |  7089 | `	sxu32 nType,` |
|       - |  7090 | `	const SyString *pTypeClass,` |
|       - |  7091 | `	const SyString *pTypeText,` |
|       - |  7092 | `	SySet *pUnionAlts,` |
|       - |  7093 | `	const char *zErrFmt,` |
|       - |  7094 | `	sxu32 nLine)` |
|       5 |  7095 |  |
|     253 |  7096 | `	const char *zBad = 0;` |
|     253 |  7097 | `	sxu32 nBad = 0;` |
|       - |  7098 | `	SyString sFallback;` |
|       - |  7099 | `	const SyString *pBad;` |
|       - |  7100 | `	sxi32 rc;` |
|     253 |  7101 | `	int bDisallowed = 0;` |
|     253 |  7102 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7103 | `		bDisallowed = 1;` |
|     251 |  7104 | `	}else if( pUnionAlts ){` |
|       - |  7105 | `		sxu32 i;` |
|      56 |  7106 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      40 |  7107 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      40 |  7108 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7109 | `				bDisallowed = 1;` |
|       3 |  7110 | `				break;` |
|       - |  7111 | `			}` |
|      21 |  7112 | `		}` |
|       9 |  7113 | `	}` |
|     253 |  7114 | `	if( !bDisallowed ){` |
|     247 |  7115 | `		return SXRET_OK;` |
|       - |  7116 | `	}` |
|       - |  7117 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7118 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7119 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7120 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7121 | `		pBad = pTypeText;` |
|       5 |  7122 | `	}else{` |
|     ! 0 |  7123 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7124 | `		pBad = &sFallback;` |
|       - |  7125 | `	}` |
|      11 |  7126 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7127 | `		zErrFmt,` |
|       3 |  7128 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7129 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7130 | `		return SXERR_ABORT;` |
|       - |  7131 | `	}` |
|       8 |  7132 | `	return SXERR_SYNTAX;` |
|     129 |  7133 |  |
|       - |  7134 | `/*` |
|       - |  7135 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7136 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7137 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7138 | ` * than promoted to a lexer keyword.` |
|       - |  7139 | ` */` |
| 1474640 |  7140 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7141 |  |
| 1504300 |  7142 | `	return (pTok->nType & PH7_TK_ID)` |
|  766975 |  7143 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1504295 |  7144 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7145 |  |
|   63392 |  7146 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7147 |  |
|   63397 |  7148 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7149 | `	ph7_class_attr *pAttr;` |
|       - |  7150 | `	SyString *pName;` |
|       - |  7151 | `	sxi32 rc;` |
|   63397 |  7152 | `	sxu32 nType = 0;` |
|       - |  7153 | `	SyString sTypeClass;` |
|       - |  7154 | `	SyString sTypeText;` |
|       - |  7155 | `	SySet aUnionAlts;` |
|   63397 |  7156 | `	sxi32 iTypeFlags = 0;` |
|   63397 |  7157 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   63397 |  7158 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   63397 |  7159 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7160 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7161 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7162 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   63397 |  7163 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7164 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7165 | `	}` |
|       - |  7166 | `	/* Extract visibility level */` |
|   63397 |  7167 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7168 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   63482 |  7169 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     175 |  7170 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     175 |  7171 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7172 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7173 | `			goto Synchronize;` |
|     175 |  7174 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7175 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7176 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7177 | `				&pGen->pIn->sData);` |
|     ! 0 |  7178 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7179 | `				return SXERR_ABORT;` |
|       - |  7180 | `			}` |
|     ! 0 |  7181 | `			goto Synchronize;` |
|     175 |  7182 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7183 | `			return SXERR_ABORT;` |
|       - |  7184 | `		}` |
|      85 |  7185 | `	}` |
|     ! 0 |  7186 | `loop:` |
|   63401 |  7187 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7188 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7189 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7190 | `			return SXERR_ABORT;` |
|       - |  7191 | `		}` |
|     ! 0 |  7192 | `		goto Synchronize;` |
|       - |  7193 | `	}` |
|   63401 |  7194 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   63401 |  7195 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7196 | `		/* Invalid attribute name */` |
|     ! 0 |  7197 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7198 | `		if( rc == SXERR_ABORT ){` |
|       - |  7199 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7200 | `			return SXERR_ABORT;` |
|       - |  7201 | `		}` |
|     ! 0 |  7202 | `		goto Synchronize;` |
|       - |  7203 | `	}` |
|       - |  7204 | `	/* Peek attribute name */` |
|   63401 |  7205 | `	pName = &pGen->pIn->sData;` |
|       - |  7206 | `	/* Advance the stream cursor */` |
|   63401 |  7207 | `	pGen->pIn++;` |
|   63401 |  7208 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7209 | `		/* Invalid declaration */` |
|       3 |  7210 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7211 | `		if( rc == SXERR_ABORT ){` |
|       - |  7212 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7213 | `			return SXERR_ABORT;` |
|       - |  7214 | `		}` |
|       3 |  7215 | `		goto Synchronize;` |
|       - |  7216 | `	}` |
|       - |  7217 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7218 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   63399 |  7219 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7220 | `		const char *zRoErr = 0;` |
|      39 |  7221 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7222 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7223 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7224 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7225 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7226 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7227 | `		}` |
|      39 |  7228 | `		if( zRoErr ){` |
|      13 |  7229 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7230 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7231 | `				return SXERR_ABORT;` |
|       - |  7232 | `			}` |
|      13 |  7233 | `			goto Synchronize;` |
|       - |  7234 | `		}` |
|      12 |  7235 | `	}` |
|       - |  7236 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7237 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7238 | `	 * by the type parser. */` |
|   63389 |  7239 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     257 |  7240 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7241 | `			&sTypeText,` |
|     168 |  7242 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      84 |  7243 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     173 |  7244 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7245 | `			return SXERR_ABORT;` |
|     173 |  7246 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7247 | `			goto Synchronize;` |
|       - |  7248 | `		}` |
|      84 |  7249 | `	}` |
|       - |  7250 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   63389 |  7251 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7252 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7253 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7254 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7255 | `			return SXERR_ABORT;` |
|       - |  7256 | `		}` |
|       3 |  7257 | `		goto Synchronize;` |
|       - |  7258 | `	}` |
|       - |  7259 | `	/* Allocate a new class attribute */` |
|   63387 |  7260 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   63387 |  7261 | `	if( pAttr == 0 ){` |
|     ! 0 |  7262 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7263 | `		return SXERR_ABORT;` |
|       - |  7264 | `	}` |
|   63387 |  7265 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     171 |  7266 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      83 |  7267 | `	}` |
|   63387 |  7268 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7269 | `		SySet *pInstrContainer;` |
|   20263 |  7270 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7271 | `		/* Swap bytecode container */` |
|   20263 |  7272 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   20263 |  7273 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7274 | `		/* Compile attribute value.` |
|       - |  7275 | `		 */` |
|   20263 |  7276 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   20263 |  7277 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7278 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7279 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7280 | `				return SXERR_ABORT;` |
|       - |  7281 | `			}` |
|     ! 0 |  7282 | `		}` |
|       - |  7283 | `		/* Emit the done instruction */` |
|   20263 |  7284 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   20263 |  7285 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   10129 |  7286 | `	}` |
|       - |  7287 | `	/* All done,install the attribute */` |
|   63387 |  7288 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   63387 |  7289 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7290 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7291 | `		return SXERR_ABORT;` |
|       - |  7292 | `	}` |
|   63387 |  7293 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7294 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7295 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7296 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7297 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7298 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7299 | `				pTok--;` |
|     ! 0 |  7300 | `			}` |
|     ! 0 |  7301 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7302 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7303 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7304 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7305 | `				return SXERR_ABORT;` |
|       - |  7306 | `			}` |
|     ! 0 |  7307 | `		}else{` |
|       5 |  7308 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7309 | `				goto loop;` |
|       - |  7310 | `			}` |
|       - |  7311 | `		}` |
|     ! 0 |  7312 | `	}` |
|   63383 |  7313 | `	SySetRelease(&aUnionAlts);` |
|   63383 |  7314 | `	return SXRET_OK;` |
|       7 |  7315 | `Synchronize:` |
|       - |  7316 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7317 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      16 |  7318 | `		pGen->pIn++;` |
|       2 |  7319 | `	}` |
|      17 |  7320 | `	SySetRelease(&aUnionAlts);` |
|      17 |  7321 | `	return SXERR_CORRUPT;` |
|   31701 |  7322 |  |
|       - |  7323 | `/*` |
|       - |  7324 | ` * Compile a class method.` |
|       - |  7325 | ` *` |
|       - |  7326 | ` * Refer to the official documentation for more information` |
|       - |  7327 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7328 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7329 | ` * overloading and many more.` |
|       - |  7330 | ` */` |
|  248880 |  7331 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7332 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7333 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7334 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7335 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7336 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7337 | `	)` |
|       5 |  7338 |  |
|  248885 |  7339 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7340 | `	ph7_class_method *pMeth;` |
|       - |  7341 | `	sxi32 iFuncFlags;` |
|       - |  7342 | `	SyString *pName;` |
|       - |  7343 | `	SyToken *pEnd;` |
|       - |  7344 | `	sxi32 rc;` |
|       - |  7345 | `	/* Extract visibility level */` |
|  248885 |  7346 | `	iProtection = GetProtectionLevel(iProtection);` |
|  248885 |  7347 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  248885 |  7348 | `	iFuncFlags = 0;` |
|  248885 |  7349 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7350 | `		/* Invalid method name */` |
|     ! 0 |  7351 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7352 | `		if( rc == SXERR_ABORT ){` |
|       - |  7353 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7354 | `			return SXERR_ABORT;` |
|       - |  7355 | `		}` |
|     ! 0 |  7356 | `		goto Synchronize;` |
|       - |  7357 | `	}` |
|  248885 |  7358 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7359 | `		/* Return by reference,remember that */` |
|     ! 0 |  7360 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7361 | `		/* Jump the '&' token */` |
|     ! 0 |  7362 | `		pGen->pIn++;` |
|     ! 0 |  7363 | `	}` |
|  248885 |  7364 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7365 | `		/* Invalid method name */` |
|     ! 0 |  7366 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7367 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7368 | `			return SXERR_ABORT;` |
|       - |  7369 | `		}` |
|     ! 0 |  7370 | `		goto Synchronize;` |
|       - |  7371 | `	}` |
|       - |  7372 | `	/* Peek method name */` |
|  248885 |  7373 | `	pName = &pGen->pIn->sData;` |
|  248885 |  7374 | `	nLine = pGen->pIn->nLine;` |
|       - |  7375 | `	/* Jump the method name */` |
|  248885 |  7376 | `	pGen->pIn++;` |
|  248885 |  7377 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7378 | `		/* Abstract method */` |
|   86013 |  7379 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7380 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7381 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7382 | `				&pClass->sName,pName);` |
|     ! 0 |  7383 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7384 | `				return SXERR_ABORT;` |
|       - |  7385 | `			}` |
|     ! 0 |  7386 | `		}` |
|       - |  7387 | `		/* Assemble method signature only */` |
|   86013 |  7388 | `		doBody = FALSE;` |
|   43004 |  7389 | `	}` |
|  248885 |  7390 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7391 | `		/* Syntax error */` |
|     ! 0 |  7392 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7393 | `		if( rc == SXERR_ABORT ){` |
|       - |  7394 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7395 | `			return SXERR_ABORT;` |
|       - |  7396 | `		}` |
|     ! 0 |  7397 | `		goto Synchronize;` |
|       - |  7398 | `	}` |
|       - |  7399 | `	/* Allocate a new class_method instance */` |
|  248885 |  7400 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  248885 |  7401 | `	if( pMeth == 0 ){` |
|     ! 0 |  7402 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7403 | `		return SXERR_ABORT;` |
|       - |  7404 | `	}` |
|       - |  7405 | `	/* Jump the left parenthesis '(' */` |
|  248885 |  7406 | `	pGen->pIn++;` |
|  248885 |  7407 | `	pEnd = 0; /* cc warning */` |
|       - |  7408 | `	/* Delimit the method signature */` |
|  248885 |  7409 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  248885 |  7410 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7411 | `		/* Syntax error */` |
|       3 |  7412 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7413 | `		if( rc == SXERR_ABORT ){` |
|       - |  7414 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7415 | `			return SXERR_ABORT;` |
|       - |  7416 | `		}` |
|       3 |  7417 | `		goto Synchronize;` |
|       - |  7418 | `	}` |
|       - |  7419 | `	{` |
|  248883 |  7420 | `		int bIsCtor = 0;` |
|  248883 |  7421 | `		int bAbstractCtor = 0;` |
|  363329 |  7422 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  147687 |  7423 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  238895 |  7424 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   19981 |  7425 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7426 | `				bAbstractCtor = 1;` |
|       2 |  7427 | `			}else{` |
|   19979 |  7428 | `				bIsCtor = 1;` |
|       - |  7429 | `			}` |
|    9988 |  7430 | `		}` |
|  248883 |  7431 | `		if( pGen->pIn < pEnd ){` |
|       - |  7432 | `			/* Collect method arguments */` |
|   56565 |  7433 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   56565 |  7434 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7435 | `				return SXERR_ABORT;` |
|       - |  7436 | `			}` |
|   28280 |  7437 | `		}` |
|       - |  7438 | `	}` |
|       - |  7439 | `	/* Point past ')' and parse optional return type ': type' */` |
|  248883 |  7440 | `	pGen->pIn = &pEnd[1];` |
|       - |  7441 | `	{` |
|  248883 |  7442 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  248883 |  7443 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7444 | `			return SXERR_ABORT;` |
|  248883 |  7445 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7446 | `			goto Synchronize;` |
|       - |  7447 | `		}` |
|       - |  7448 | `	}` |
|       - |  7449 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7450 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7451 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7452 | `	{` |
|  248883 |  7453 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7454 | `		sxu32 i;` |
|  338601 |  7455 | `		for( i = 0; i < nArg; i++ ){` |
|   89733 |  7456 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7457 | `			ph7_class_attr *pAttr;` |
|   89733 |  7458 | `			sxi32 iAttrFlags = 0;` |
|   89733 |  7459 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   89675 |  7460 | `				continue;` |
|       - |  7461 | `			}` |
|      63 |  7462 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7463 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7464 | `					"Cannot declare variadic promoted property");` |
|       3 |  7465 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7466 | `					return SXERR_ABORT;` |
|       - |  7467 | `				}` |
|       3 |  7468 | `				goto Synchronize;` |
|       - |  7469 | `			}` |
|       - |  7470 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7471 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7472 | `			 * appear as an alternative of a union type. */` |
|      56 |  7473 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      13 |  7474 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      83 |  7475 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      52 |  7476 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      52 |  7477 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      26 |  7478 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      57 |  7479 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7480 | `					return SXERR_ABORT;` |
|      57 |  7481 | `				}else if( rc != SXRET_OK ){` |
|       6 |  7482 | `					goto Synchronize;` |
|       - |  7483 | `				}` |
|      24 |  7484 | `			}` |
|       - |  7485 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      57 |  7486 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7487 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7488 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7489 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7490 | `					return SXERR_ABORT;` |
|       - |  7491 | `				}` |
|       3 |  7492 | `				goto Synchronize;` |
|       - |  7493 | `			}` |
|      55 |  7494 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      49 |  7495 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      22 |  7496 | `			}` |
|      55 |  7497 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7498 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7499 | `			}` |
|      55 |  7500 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7501 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7502 | `			}` |
|      55 |  7503 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  7504 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  7505 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  7506 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  7507 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7508 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  7509 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7510 | `						return SXERR_ABORT;` |
|       - |  7511 | `					}` |
|       3 |  7512 | `					goto Synchronize;` |
|       - |  7513 | `				}` |
|      22 |  7514 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7515 | `			}` |
|      53 |  7516 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      53 |  7517 | `			if( pAttr == 0 ){` |
|     ! 0 |  7518 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7519 | `				return SXERR_ABORT;` |
|       - |  7520 | `			}` |
|      53 |  7521 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      49 |  7522 | `				pAttr->nType = pArg->nType;` |
|      49 |  7523 | `				pAttr->sClass = pArg->sClass;` |
|      49 |  7524 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      49 |  7525 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7526 | `					sxu32 k;` |
|     ! 0 |  7527 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7528 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7529 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7530 | `					}` |
|     ! 0 |  7531 | `				}` |
|      22 |  7532 | `			}` |
|      53 |  7533 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      53 |  7534 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7535 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7536 | `				return SXERR_ABORT;` |
|       - |  7537 | `			}` |
|      29 |  7538 | `		}` |
|       - |  7539 | `	}` |
|  248873 |  7540 | `	if( doBody ){` |
|       - |  7541 | `		/* Compile method body */` |
|  162865 |  7542 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  162865 |  7543 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7544 | `			return SXERR_ABORT;` |
|       - |  7545 | `		}` |
|   81435 |  7546 | `	}else{` |
|       - |  7547 | `		/* Only method signature is allowed */` |
|   86013 |  7548 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7549 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7550 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7551 | `				if( rc == SXERR_ABORT ){` |
|       - |  7552 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7553 | `					return SXERR_ABORT;` |
|       - |  7554 | `				}` |
|     ! 0 |  7555 | `				return SXERR_CORRUPT;` |
|       - |  7556 | `			}` |
|       - |  7557 | `	}` |
|       - |  7558 | `	/* All done,install the method */` |
|  248873 |  7559 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  248873 |  7560 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7561 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7562 | `		return SXERR_ABORT;` |
|       - |  7563 | `	}` |
|  248873 |  7564 | `	return SXRET_OK;` |
|       6 |  7565 | `Synchronize:` |
|       - |  7566 | `	/* Synchronize with the first semi-colon */` |
|      40 |  7567 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  7568 | `		pGen->pIn++;` |
|       4 |  7569 | `	}` |
|      16 |  7570 | `	return SXERR_CORRUPT;` |
|  124445 |  7571 |  |
|       - |  7572 | `/*` |
|       - |  7573 | ` * Compile an object interface.` |
|       - |  7574 | ` *  According to the PHP language reference manual` |
|       - |  7575 | ` *   Object Interfaces:` |
|       - |  7576 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7577 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7578 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7579 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7580 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7581 | ` */` |
|   36414 |  7582 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  7583 |  |
|   36419 |  7584 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7585 | `	ph7_class *pClass,*pBase;` |
|       - |  7586 | `	SyToken *pEnd,*pTmp;` |
|       - |  7587 | `	SyString *pName;` |
|       - |  7588 | `	sxi32 nKwrd;` |
|       - |  7589 | `	sxi32 rc;` |
|       - |  7590 | `	/* Jump the 'interface' keyword */` |
|   36419 |  7591 | `	pGen->pIn++;` |
|       - |  7592 | `	/* Extract interface name */` |
|   36419 |  7593 | `	pName = &pGen->pIn->sData;` |
|       - |  7594 | `	/* Advance the stream cursor */` |
|   36419 |  7595 | `	pGen->pIn++;` |
|       - |  7596 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7597 | `		SyBlob sFQN;` |
|       - |  7598 | `		SyString sFQNStr;` |
|   36419 |  7599 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   36419 |  7600 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   36419 |  7601 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   36419 |  7602 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   36419 |  7603 | `		SyBlobRelease(&sFQN);` |
|       - |  7604 | `	}` |
|   36419 |  7605 | `	if( pClass == 0 ){` |
|     ! 0 |  7606 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7607 | `		return SXERR_ABORT;` |
|       - |  7608 | `	}` |
|       - |  7609 | `	/* Mark as an interface */` |
|   36419 |  7610 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7611 | `	/* Assume no base class is given */` |
|   36419 |  7612 | `	pBase = 0;` |
|   36419 |  7613 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    9931 |  7614 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    9931 |  7615 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7616 | `			SyBlob sResolved;` |
|       - |  7617 | `			SyString sBaseName;` |
|       - |  7618 | `			sxu32 nRefLine;` |
|       - |  7619 | `			/* Extract base interface */` |
|    9931 |  7620 | `			pGen->pIn++;` |
|    9931 |  7621 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    9931 |  7622 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    9931 |  7623 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7624 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7625 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7626 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7627 | `					pName);` |
|     ! 0 |  7628 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7629 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7630 | `					return SXERR_ABORT;` |
|       - |  7631 | `				}` |
|     ! 0 |  7632 | `				return SXRET_OK;` |
|       - |  7633 | `			}` |
|   14894 |  7634 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    9926 |  7635 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    9931 |  7636 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7637 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7638 | `			/* Only interfaces is allowed */` |
|    9931 |  7639 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7640 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7641 | `			}` |
|    9931 |  7642 | `			if( pBase == 0 ){` |
|     ! 0 |  7643 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7644 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  7645 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7646 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7647 | `					return SXERR_ABORT;` |
|       - |  7648 | `				}` |
|     ! 0 |  7649 | `			}` |
|    9931 |  7650 | `			SyBlobRelease(&sResolved);` |
|    4963 |  7651 | `		}` |
|    4963 |  7652 | `	}` |
|   36419 |  7653 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7654 | `		/* Syntax error */` |
|     ! 0 |  7655 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7656 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7657 | `		if( rc == SXERR_ABORT ){` |
|       - |  7658 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7659 | `			return SXERR_ABORT;` |
|       - |  7660 | `		}` |
|     ! 0 |  7661 | `		return SXRET_OK;` |
|       - |  7662 | `	}` |
|   36419 |  7663 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   36419 |  7664 | `	pEnd = 0; /* cc warning */` |
|       - |  7665 | `	/* Delimit the interface body */` |
|   36419 |  7666 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   36419 |  7667 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7668 | `		/* Syntax error */` |
|     ! 0 |  7669 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7670 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7671 | `		if( rc == SXERR_ABORT ){` |
|       - |  7672 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7673 | `			return SXERR_ABORT;` |
|       - |  7674 | `		}` |
|     ! 0 |  7675 | `		return SXRET_OK;` |
|       - |  7676 | `	}` |
|       - |  7677 | `	/* Swap token stream */` |
|   36419 |  7678 | `	pTmp = pGen->pEnd;` |
|   36419 |  7679 | `	pGen->pEnd = pEnd;` |
|       - |  7680 | `	/* Start the parse process` |
|       - |  7681 | `	 * Note (According to the PHP reference manual):` |
|       - |  7682 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7683 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7684 | `	 */` |
|   61207 |  7685 | `	for(;;){` |
|       - |  7686 | `		/* Jump leading/trailing semi-colons */` |
|  208419 |  7687 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   86005 |  7688 | `			pGen->pIn++;` |
|       5 |  7689 | `		}` |
|  122419 |  7690 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7691 | `			/* End of interface body */` |
|   36417 |  7692 | `			break;` |
|       - |  7693 | `		}` |
|   86007 |  7694 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7695 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7696 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7697 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7698 | `			if( rc == SXERR_ABORT ){` |
|       - |  7699 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7700 | `				return SXERR_ABORT;` |
|       - |  7701 | `			}` |
|     ! 0 |  7702 | `			goto done;` |
|       - |  7703 | `		}` |
|       - |  7704 | `		/* Extract the current keyword */` |
|   86007 |  7705 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   86007 |  7706 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7707 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7708 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7709 | `			const char *zKind = "member";` |
|       3 |  7710 | `			SyString *pMemberName = 0;` |
|       3 |  7711 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7712 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7713 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7714 | `					zKind = "constant";` |
|       3 |  7715 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7716 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7717 | `					}` |
|       1 |  7718 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7719 | `					zKind = "method";` |
|     ! 0 |  7720 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7721 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7722 | `					}` |
|     ! 0 |  7723 | `				}` |
|       1 |  7724 | `			}` |
|       3 |  7725 | `			if( pMemberName ){` |
|       4 |  7726 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7727 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7728 | `			}else{` |
|     ! 0 |  7729 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7730 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7731 | `			}` |
|       3 |  7732 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7733 | `				return SXERR_ABORT;` |
|       - |  7734 | `			}` |
|       3 |  7735 | `			goto done;` |
|       - |  7736 | `		}` |
|   86005 |  7737 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7738 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7739 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7740 | `			if( rc == SXERR_ABORT ){` |
|       - |  7741 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7742 | `				return SXERR_ABORT;` |
|       - |  7743 | `			}` |
|     ! 0 |  7744 | `			goto done;` |
|       - |  7745 | `		}` |
|   86005 |  7746 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7747 | `			/* Advance the stream cursor */` |
|   85997 |  7748 | `			pGen->pIn++;` |
|   85997 |  7749 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7750 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7751 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7752 | `				if( rc == SXERR_ABORT ){` |
|       - |  7753 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7754 | `					return SXERR_ABORT;` |
|       - |  7755 | `				}` |
|     ! 0 |  7756 | `				goto done;` |
|       - |  7757 | `			}` |
|   85997 |  7758 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   85997 |  7759 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7760 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7761 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7762 | `				if( rc == SXERR_ABORT ){` |
|       - |  7763 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7764 | `					return SXERR_ABORT;` |
|       - |  7765 | `				}` |
|     ! 0 |  7766 | `				goto done;` |
|       - |  7767 | `			}` |
|   42996 |  7768 | `		}` |
|   86005 |  7769 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7770 | `			/* Parse constant */` |
|       7 |  7771 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       7 |  7772 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7773 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7774 | `					return SXERR_ABORT;` |
|       - |  7775 | `				}` |
|     ! 0 |  7776 | `				goto done;` |
|       - |  7777 | `			}` |
|       4 |  7778 | `		}else{` |
|   85999 |  7779 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   85999 |  7780 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7781 | `				/* Static method,record that */` |
|    9923 |  7782 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7783 | `				/* Advance the stream cursor */` |
|    9923 |  7784 | `				pGen->pIn++;` |
|    9918 |  7785 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    9923 |  7786 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7787 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7788 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7789 | `						if( rc == SXERR_ABORT ){` |
|       - |  7790 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7791 | `							return SXERR_ABORT;` |
|       - |  7792 | `						}` |
|     ! 0 |  7793 | `						goto done;` |
|       - |  7794 | `				}` |
|    4959 |  7795 | `			}` |
|       - |  7796 | `			/* Process method signature (no body for interface methods) */` |
|   85999 |  7797 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   85999 |  7798 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7799 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7800 | `					return SXERR_ABORT;` |
|       - |  7801 | `				}` |
|     ! 0 |  7802 | `				goto done;` |
|       - |  7803 | `			}` |
|       - |  7804 | `		}` |
|       5 |  7805 | `	}` |
|       - |  7806 | `	/* Install the interface */` |
|   36417 |  7807 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   36417 |  7808 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7809 | `		/* Inherit from the base interface */` |
|    9931 |  7810 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    4963 |  7811 | `	}` |
|   36417 |  7812 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7813 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7814 | `		return SXERR_ABORT;` |
|       - |  7815 | `	}` |
|   18206 |  7816 | `done:` |
|       - |  7817 | `	/* Point beyond the interface body */` |
|   36419 |  7818 | `	pGen->pIn  = &pEnd[1];` |
|   36419 |  7819 | `	pGen->pEnd = pTmp;` |
|   36419 |  7820 | `	return PH7_OK;` |
|   18212 |  7821 |  |
|       - |  7822 | `/*` |
|       - |  7823 | ` * Compile a user-defined class.` |
|       - |  7824 | ` * According to the PHP language reference manual` |
|       - |  7825 | ` *  class` |
|       - |  7826 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7827 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7828 | ` *  of the properties and methods belonging to the class.` |
|       - |  7829 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7830 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7831 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7832 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7833 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7834 | ` *  (called "methods").` |
|       - |  7835 | ` */` |
|       - |  7836 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7837 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7838 | `struct TraitUseEntry {` |
|       - |  7839 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7840 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7841 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7842 | `};` |
|       - |  7843 | `/*` |
|       - |  7844 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7845 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7846 | ` */` |
|   90286 |  7847 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  7848 |  |
|       - |  7849 | `	ph7_class **apIface;` |
|       - |  7850 | `	sxu32 nIface,i;` |
|       - |  7851 | `	sxi32 rc;` |
|   90291 |  7852 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7853 | `		return SXRET_OK;` |
|       - |  7854 | `	}` |
|   90291 |  7855 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   90291 |  7856 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  179749 |  7857 | `	for(i = 0; i < nIface; i++){` |
|   89463 |  7858 | `		ph7_class *pIface = apIface[i];` |
|       - |  7859 | `		SyHashEntry *pEntry;` |
|   89463 |  7860 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  238635 |  7861 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  149177 |  7862 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7863 | `			ph7_class_method *pImplMeth;` |
|  149177 |  7864 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7865 | `			/* Find the implementing method in the class */` |
|  149177 |  7866 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  149177 |  7867 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  7868 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  7869 | `			}` |
|       - |  7870 | `			/* Check visibility: interface methods must be implemented as public */` |
|  149163 |  7871 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  7872 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7873 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  7874 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  7875 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7876 | `					return SXERR_ABORT;` |
|       - |  7877 | `				}` |
|       1 |  7878 | `			}` |
|       - |  7879 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7880 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7881 | `			 */` |
|       - |  7882 | `			{` |
|  149163 |  7883 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  149163 |  7884 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  149163 |  7885 | `				int sigError = 0;` |
|  149163 |  7886 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7887 | `					sigError = 1;` |
|  149162 |  7888 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7889 | `					/* Extra parameters must all have default values */` |
|       6 |  7890 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7891 | `					sxu32 k;` |
|       8 |  7892 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  7893 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7894 | `							sigError = 1;` |
|       3 |  7895 | `							break;` |
|       - |  7896 | `						}` |
|       2 |  7897 | `					}` |
|       2 |  7898 | `				}` |
|  149163 |  7899 | `				if( sigError ){` |
|       - |  7900 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7901 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7902 | `					sxu32 j;` |
|       6 |  7903 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  7904 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7905 | `					/* Build implementing method signature */` |
|       6 |  7906 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  7907 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  7908 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  7909 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  7910 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  7911 | `					}` |
|       - |  7912 | `					/* Build interface method signature */` |
|       6 |  7913 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  7914 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  7915 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  7916 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  7917 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  7918 | `					}` |
|       8 |  7919 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7920 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7921 | `						&pClass->sName,pMName,` |
|       4 |  7922 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7923 | `						&pIface->sName,pMName,` |
|       4 |  7924 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  7925 | `					SyBlobRelease(&sImplSig);` |
|       6 |  7926 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  7927 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7928 | `						return SXERR_ABORT;` |
|       - |  7929 | `					}` |
|       2 |  7930 | `				}` |
|       - |  7931 | `			}` |
|       5 |  7932 | `		}` |
|   44734 |  7933 | `	}` |
|   90291 |  7934 | `	return SXRET_OK;` |
|   45148 |  7935 |  |
|       - |  7936 | `/*` |
|       - |  7937 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7938 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7939 | ` */` |
|   90286 |  7940 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  7941 |  |
|       - |  7942 | `	ph7_class_method *pMeth;` |
|       - |  7943 | `	SyHashEntry *pEntry;` |
|       - |  7944 | `	sxu32 nAbstract;` |
|       - |  7945 | `	SyBlob sMsg;` |
|       - |  7946 | `	sxi32 rc;` |
|       - |  7947 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   90291 |  7948 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      29 |  7949 | `		return SXRET_OK;` |
|       - |  7950 | `	}` |
|       - |  7951 | `	/* Count abstract methods */` |
|   90267 |  7952 | `	nAbstract = 0;` |
|   90267 |  7953 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  875233 |  7954 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  784971 |  7955 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  784971 |  7956 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  7957 | `			nAbstract++;` |
|       8 |  7958 | `		}` |
|       5 |  7959 | `	}` |
|   90267 |  7960 | `	if( nAbstract == 0 ){` |
|   90253 |  7961 | `		return SXRET_OK;` |
|       - |  7962 | `	}` |
|       - |  7963 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  7964 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  7965 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7966 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7967 | `		&pClass->sName,nAbstract,` |
|       7 |  7968 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7969 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7970 | `	/* Second pass: list methods with origins */` |
|       - |  7971 | `	{` |
|      18 |  7972 | `		sxu32 nListed = 0;` |
|      18 |  7973 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  7974 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  7975 | `			ph7_class *pOrigin = 0;` |
|       - |  7976 | `			SyString *pMName;` |
|      22 |  7977 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  7978 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7979 | `				continue;` |
|       - |  7980 | `			}` |
|      20 |  7981 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  7982 | `			if( nListed > 0 ){` |
|       3 |  7983 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7984 | `			}` |
|       - |  7985 | `			/* Find the origin of this abstract method.` |
|       - |  7986 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7987 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7988 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7989 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7990 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7991 | `			 * class's namespace.` |
|       - |  7992 | `			 */` |
|       - |  7993 | `			{` |
|       - |  7994 | `				ph7_class **apIface;` |
|       - |  7995 | `				ph7_class **apTrait;` |
|       - |  7996 | `				ph7_class *pWalk;` |
|       - |  7997 | `				sxu32 i;` |
|       - |  7998 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7999 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8000 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8001 | `				 */` |
|      20 |  8002 | `				if( pClass->pBase ){` |
|      11 |  8003 | `					pWalk = pClass->pBase;` |
|      19 |  8004 | `					while( pWalk ){` |
|       - |  8005 | `						ph7_class_method *pParentMeth;` |
|      13 |  8006 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8007 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8008 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8009 | `							 * in this class's ancestor chain.` |
|       - |  8010 | `							 */` |
|      13 |  8011 | `							int fromIface = 0;` |
|      13 |  8012 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8013 | `							while( pAnc ){` |
|       - |  8014 | `								ph7_class **apPI;` |
|       - |  8015 | `								sxu32 j;` |
|      15 |  8016 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8017 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8018 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8019 | `										fromIface = 1;` |
|      10 |  8020 | `										break;` |
|       - |  8021 | `									}` |
|     ! 0 |  8022 | `								}` |
|      15 |  8023 | `								if( fromIface ) break;` |
|       6 |  8024 | `								pAnc = pAnc->pBase;` |
|       2 |  8025 | `							}` |
|      13 |  8026 | `							if( !fromIface ){` |
|       3 |  8027 | `								pOrigin = pWalk;` |
|       3 |  8028 | `								break;` |
|       - |  8029 | `							}` |
|       4 |  8030 | `						}` |
|      10 |  8031 | `						pWalk = pWalk->pBase;` |
|       2 |  8032 | `					}` |
|       4 |  8033 | `				}` |
|       - |  8034 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8035 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8036 | `				 */` |
|      20 |  8037 | `				if( !pOrigin ){` |
|      18 |  8038 | `					pWalk = pClass;` |
|      40 |  8039 | `					while( pWalk && !pOrigin ){` |
|      26 |  8040 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8041 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8042 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8043 | `							ph7_class *pDeepest = 0;` |
|      28 |  8044 | `							while( pIface ){` |
|      16 |  8045 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8046 | `									pDeepest = pIface;` |
|       6 |  8047 | `								}` |
|      16 |  8048 | `								pIface = pIface->pBase;` |
|       4 |  8049 | `							}` |
|      16 |  8050 | `							if( pDeepest ){` |
|      16 |  8051 | `								pOrigin = pDeepest;` |
|      16 |  8052 | `								break;` |
|       - |  8053 | `							}` |
|     ! 0 |  8054 | `						}` |
|      26 |  8055 | `						pWalk = pWalk->pBase;` |
|       4 |  8056 | `					}` |
|       7 |  8057 | `				}` |
|       - |  8058 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8059 | `				if( !pOrigin ){` |
|       3 |  8060 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8061 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8062 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8063 | `							pOrigin = pClass;` |
|       3 |  8064 | `							break;` |
|       - |  8065 | `						}` |
|     ! 0 |  8066 | `					}` |
|       1 |  8067 | `				}` |
|       - |  8068 | `			}` |
|      20 |  8069 | `			if( pOrigin ){` |
|      20 |  8070 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8071 | `			}else{` |
|       - |  8072 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8073 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8074 | `			}` |
|      20 |  8075 | `			nListed++;` |
|       4 |  8076 | `		}` |
|       - |  8077 | `	}` |
|      18 |  8078 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8079 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8080 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8081 | `	SyBlobRelease(&sMsg);` |
|      18 |  8082 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8083 | `		return SXERR_ABORT;` |
|       - |  8084 | `	}` |
|      18 |  8085 | `	return SXRET_OK;` |
|   45148 |  8086 |  |
|       - |  8087 | `/*` |
|       - |  8088 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8089 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8090 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8091 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8092 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8093 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8094 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8095 | ` */` |
|   89992 |  8096 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8097 |  |
|   89997 |  8098 | `	int isAbsolute = 0;` |
|   89997 |  8099 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8100 | `	SyBlob sName;` |
|   89997 |  8101 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      47 |  8102 | `		isAbsolute = 1;` |
|      47 |  8103 | `		pGen->pIn++;` |
|      22 |  8104 | `	}` |
|   89997 |  8105 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       9 |  8106 | `		pGen->pIn = pStart;` |
|       9 |  8107 | `		return SXERR_INVALID;` |
|       - |  8108 | `	}` |
|   89991 |  8109 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   89991 |  8110 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   89991 |  8111 | `	pGen->pIn++;` |
|  134997 |  8112 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   45016 |  8113 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8114 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8115 | `		pGen->pIn++;` |
|      13 |  8116 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8117 | `		pGen->pIn++;` |
|       1 |  8118 | `	}` |
|   89991 |  8119 | `	if( isAbsolute ){` |
|      45 |  8120 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      24 |  8121 | `	}else{` |
|       - |  8122 | `		SyString sRaw;` |
|   89949 |  8123 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   89949 |  8124 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8125 | `	}` |
|   89991 |  8126 | `	SyBlobRelease(&sName);` |
|   89991 |  8127 | `	return SXRET_OK;` |
|   45001 |  8128 |  |
|       - |  8129 | `/*` |
|       - |  8130 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8131 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8132 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8133 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8134 | ` * either direction cannot run unbounded.` |
|       - |  8135 | ` */` |
|       - |  8136 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   10040 |  8137 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8138 |  |
|       - |  8139 | `	ph7_class **apParent;` |
|       - |  8140 | `	sxu32 n;` |
|   16801 |  8141 | `	while( pInterface ){` |
|   13383 |  8142 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8143 | `			return FALSE;` |
|       - |  8144 | `		}` |
|   16701 |  8145 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    6636 |  8146 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    6627 |  8147 | `			return TRUE;` |
|       - |  8148 | `		}` |
|    6761 |  8149 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    6761 |  8150 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8151 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8152 | `				return TRUE;` |
|       - |  8153 | `			}` |
|     ! 0 |  8154 | `		}` |
|    6761 |  8155 | `		pInterface = pInterface->pBase;` |
|    6761 |  8156 | `		iDepth++;` |
|       5 |  8157 | `	}` |
|    3423 |  8158 | `	return FALSE;` |
|    5025 |  8159 |  |
|   10040 |  8160 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8161 |  |
|   10045 |  8162 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8163 |  |
|       - |  8164 | `/*` |
|       - |  8165 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8166 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8167 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8168 | ` */` |
|    6622 |  8169 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8170 |  |
|    6631 |  8171 | `	while( pBase ){` |
|      10 |  8172 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8173 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8174 | `			return TRUE;` |
|       - |  8175 | `		}` |
|      10 |  8176 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8177 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8178 | `			return TRUE;` |
|       - |  8179 | `		}` |
|       5 |  8180 | `		pBase = pBase->pBase;` |
|       1 |  8181 | `	}` |
|    6623 |  8182 | `	return FALSE;` |
|    3316 |  8183 |  |
|   90316 |  8184 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  8185 |  |
|   90321 |  8186 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8187 | `	ph7_class *pClass,*pBase;` |
|       - |  8188 | `	SyToken *pEnd,*pTmp;` |
|       - |  8189 | `	sxi32 iProtection;` |
|       - |  8190 | `	SySet aInterfaces;` |
|       - |  8191 | `	SySet aUseEntries;` |
|       - |  8192 | `	sxi32 iAttrflags;` |
|       - |  8193 | `	SyString *pName;` |
|       - |  8194 | `	sxi32 nKwrd;` |
|       - |  8195 | `	sxi32 rc;` |
|       - |  8196 | `	/* Jump the 'class' keyword */` |
|   90321 |  8197 | `	pGen->pIn++;` |
|   90321 |  8198 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8199 | `		/* Syntax error */` |
|     ! 0 |  8200 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8201 | `		if( rc == SXERR_ABORT ){` |
|       - |  8202 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8203 | `			return SXERR_ABORT;` |
|       - |  8204 | `		}` |
|       - |  8205 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8206 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8207 | `			pGen->pIn++;` |
|     ! 0 |  8208 | `		}` |
|     ! 0 |  8209 | `		return SXRET_OK;` |
|       - |  8210 | `	}` |
|       - |  8211 | `	/* Extract class name */` |
|   90321 |  8212 | `	pName = &pGen->pIn->sData;` |
|       - |  8213 | `	/* Advance the stream cursor */` |
|   90321 |  8214 | `	pGen->pIn++;` |
|       - |  8215 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8216 | `		SyBlob sFQN;` |
|       - |  8217 | `		SyString sFQNStr;` |
|   90321 |  8218 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   90321 |  8219 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   90321 |  8220 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   90321 |  8221 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   90321 |  8222 | `		SyBlobRelease(&sFQN);` |
|       - |  8223 | `	}` |
|   90321 |  8224 | `	if( pClass == 0 ){` |
|     ! 0 |  8225 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8226 | `		return SXERR_ABORT;` |
|       - |  8227 | `	}` |
|       - |  8228 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   90321 |  8229 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   90321 |  8230 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8231 | `	/* Assume a standalone class */` |
|   90321 |  8232 | `	pBase = 0;` |
|   90321 |  8233 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   79617 |  8234 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   79617 |  8235 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8236 | `			SyBlob sResolved;` |
|       - |  8237 | `			SyString sBaseName;` |
|       - |  8238 | `			sxu32 nRefLine;` |
|   69583 |  8239 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   69583 |  8240 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   69583 |  8241 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   69583 |  8242 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8243 | `				SyBlobRelease(&sResolved);` |
|       4 |  8244 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8245 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8246 | `					pName);` |
|       3 |  8247 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8248 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8249 | `					return SXERR_ABORT;` |
|       - |  8250 | `				}` |
|       3 |  8251 | `				return SXRET_OK;` |
|       - |  8252 | `			}` |
|  104369 |  8253 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   69576 |  8254 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   69581 |  8255 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8256 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8257 | `			/* Interfaces are not allowed */` |
|   69581 |  8258 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8259 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8260 | `			}` |
|   69581 |  8261 | `			if( pBase == 0 ){` |
|     ! 0 |  8262 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8263 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8264 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8265 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8266 | `					return SXERR_ABORT;` |
|       - |  8267 | `				}` |
|     ! 0 |  8268 | `			}else{` |
|   69581 |  8269 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8270 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8271 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8272 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8273 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8274 | `						return SXERR_ABORT;` |
|       - |  8275 | `					}` |
|     ! 0 |  8276 | `				}` |
|       - |  8277 | `			}` |
|   69581 |  8278 | `			SyBlobRelease(&sResolved);` |
|   34788 |  8279 | `		}` |
|   79615 |  8280 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8281 | `			ph7_class *pInterface;` |
|       - |  8282 | `			/* Interface implementation */` |
|   10045 |  8283 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5020 |  8284 | `			for(;;){` |
|       - |  8285 | `				SyBlob sResolved;` |
|       - |  8286 | `				SyString sIntName;` |
|       - |  8287 | `				sxu32 nRefLine;` |
|   10045 |  8288 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10045 |  8289 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10045 |  8290 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8291 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8292 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8293 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8294 | `						pName);` |
|     ! 0 |  8295 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8296 | `						return SXERR_ABORT;` |
|       - |  8297 | `					}` |
|     ! 0 |  8298 | `					break;` |
|       - |  8299 | `				}` |
|   20085 |  8300 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   10040 |  8301 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10045 |  8302 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8303 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8304 | `				/* Only interfaces are allowed */` |
|   10045 |  8305 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8306 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8307 | `				}` |
|   10045 |  8308 | `				if( pInterface == 0 ){` |
|     ! 0 |  8309 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8310 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8311 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8312 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8313 | `						return SXERR_ABORT;` |
|       - |  8314 | `					}` |
|     ! 0 |  8315 | `				}else{` |
|       - |  8316 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8317 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8318 | `					 * unless they already extend Exception or Error.` |
|       - |  8319 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8320 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8321 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   10045 |  8322 | `					SyString *pFqn = &pClass->sName;` |
|   10045 |  8323 | `					int bIsExceptionOrError =` |
|    8328 |  8324 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   16715 |  8325 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    8392 |  8326 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3316 |  8327 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   16660 |  8328 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    9936 |  8329 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3309 |  8330 | `						!bIsExceptionOrError ){` |
|      12 |  8331 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8332 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8333 | `							&pClass->sName);` |
|       9 |  8334 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8335 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8336 | `							return SXERR_ABORT;` |
|       - |  8337 | `						}` |
|       - |  8338 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8339 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8340 | `					}else{` |
|   10039 |  8341 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8342 | `					}` |
|       - |  8343 | `				}` |
|   10045 |  8344 | `				SyBlobRelease(&sResolved);` |
|   10045 |  8345 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5025 |  8346 | `					break;` |
|       - |  8347 | `				}` |
|     ! 0 |  8348 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  8349 | `			}` |
|    5020 |  8350 | `		}` |
|   39805 |  8351 | `	}` |
|   90319 |  8352 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8353 | `		/* Syntax error */` |
|     ! 0 |  8354 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8355 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8356 | `		if( rc == SXERR_ABORT ){` |
|       - |  8357 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8358 | `			return SXERR_ABORT;` |
|       - |  8359 | `		}` |
|     ! 0 |  8360 | `		return SXRET_OK;` |
|       - |  8361 | `	}` |
|   90319 |  8362 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   90319 |  8363 | `	pEnd = 0; /* cc warning */` |
|       - |  8364 | `	/* Delimit the class body */` |
|   90319 |  8365 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   90319 |  8366 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8367 | `		/* Syntax error */` |
|     ! 0 |  8368 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8369 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8370 | `		if( rc == SXERR_ABORT ){` |
|       - |  8371 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8372 | `			return SXERR_ABORT;` |
|       - |  8373 | `		}` |
|     ! 0 |  8374 | `		return SXRET_OK;` |
|       - |  8375 | `	}` |
|       - |  8376 | `	/* Swap token stream */` |
|   90319 |  8377 | `	pTmp = pGen->pEnd;` |
|   90319 |  8378 | `	pGen->pEnd = pEnd;` |
|       - |  8379 | `	/* Set the inherited flags */` |
|   90319 |  8380 | `	pClass->iFlags = iFlags;` |
|       - |  8381 | `	/* Start the parse process */` |
|  126598 |  8382 | `	for(;;){` |
|       - |  8383 | `		/* Jump leading/trailing semi-colons */` |
|  380087 |  8384 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   63481 |  8385 | `			pGen->pIn++;` |
|       5 |  8386 | `		}` |
|  316611 |  8387 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8388 | `			/* End of class body */` |
|   90291 |  8389 | `			break;` |
|       - |  8390 | `		}` |
|  226320 |  8391 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  113165 |  8392 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  8393 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8394 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8395 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8396 | `			if( rc == SXERR_ABORT ){` |
|       - |  8397 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8398 | `				return SXERR_ABORT;` |
|       - |  8399 | `			}` |
|     ! 0 |  8400 | `			goto done;` |
|       - |  8401 | `		}` |
|       - |  8402 | `		/* Assume public visibility */` |
|  226325 |  8403 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  226325 |  8404 | `		iAttrflags = 0;` |
|       - |  8405 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  8406 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  8407 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  8408 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  226325 |  8409 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8410 | `			int bMod = 0;` |
|     ! 0 |  8411 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8412 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  8413 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  8414 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  8415 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  8416 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  8417 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  8418 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  8419 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  8420 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  8421 | `			}` |
|     ! 0 |  8422 | `			if( !bMod ){` |
|     ! 0 |  8423 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8424 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8425 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8426 | `						return SXERR_ABORT;` |
|       - |  8427 | `					}` |
|     ! 0 |  8428 | `					goto done;` |
|       - |  8429 | `				}` |
|     ! 0 |  8430 | `				continue;` |
|       - |  8431 | `			}` |
|     ! 0 |  8432 | `		}` |
|  226325 |  8433 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8434 | `			/* Extract the current keyword */` |
|  226325 |  8435 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  226325 |  8436 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8437 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8438 | `				TraitUseEntry sUse;` |
|      49 |  8439 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      49 |  8440 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      49 |  8441 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      30 |  8442 | `				for(;;){` |
|       - |  8443 | `					ph7_class *pTrait;` |
|       - |  8444 | `					SyString *pTraitName;` |
|      57 |  8445 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8446 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8447 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8448 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8449 | `							return SXERR_ABORT;` |
|       - |  8450 | `						}` |
|     ! 0 |  8451 | `						break;` |
|       - |  8452 | `					}` |
|      57 |  8453 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8454 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8455 | `						SyBlob sResolved;` |
|      57 |  8456 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      57 |  8457 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     109 |  8458 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      52 |  8459 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      57 |  8460 | `						SyBlobRelease(&sResolved);` |
|       - |  8461 | `					}` |
|       - |  8462 | `					/* Only traits are allowed */` |
|      57 |  8463 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8464 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8465 | `					}` |
|      57 |  8466 | `					if( pTrait == 0 ){` |
|     ! 0 |  8467 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8468 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8469 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8470 | `							return SXERR_ABORT;` |
|       - |  8471 | `						}` |
|     ! 0 |  8472 | `					}else{` |
|      57 |  8473 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8474 | `					}` |
|      57 |  8475 | `					pGen->pIn++; /* Advance past trait name */` |
|      57 |  8476 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      27 |  8477 | `						break;` |
|       - |  8478 | `					}` |
|      10 |  8479 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  8480 | `				}` |
|       - |  8481 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      49 |  8482 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8483 | `					SyToken *pBlock;` |
|      10 |  8484 | `					pGen->pIn++; /* Jump '{' */` |
|      10 |  8485 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      10 |  8486 | `					sUse.pResolvStart = pGen->pIn;` |
|      10 |  8487 | `					sUse.pResolvEnd = pBlock;` |
|      10 |  8488 | `					if( pBlock < pGen->pEnd ){` |
|      10 |  8489 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       6 |  8490 | `					}else{` |
|     ! 0 |  8491 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8492 | `					}` |
|       4 |  8493 | `				}` |
|      49 |  8494 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8495 | `				/* The semicolon will be consumed by the outer loop */` |
|      49 |  8496 | `				continue;` |
|       - |  8497 | `			}` |
|  226281 |  8498 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  222797 |  8499 | `				iProtection = nKwrd;` |
|  222797 |  8500 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  8501 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  222797 |  8502 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  8503 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  8504 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  8505 | `				}` |
|  222792 |  8506 | `				if( pGen->pIn >= pGen->pEnd` |
|  222797 |  8507 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8508 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8509 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8510 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8511 | `					if( rc == SXERR_ABORT ){` |
|       - |  8512 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8513 | `						return SXERR_ABORT;` |
|       - |  8514 | `					}` |
|     ! 0 |  8515 | `					goto done;` |
|       - |  8516 | `				}` |
|  222797 |  8517 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8518 | `					/* Attribute declaration (untyped) */` |
|   63205 |  8519 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   63205 |  8520 | `					if( rc != SXRET_OK ){` |
|       9 |  8521 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8522 | `							return SXERR_ABORT;` |
|       - |  8523 | `						}` |
|       9 |  8524 | `						goto done;` |
|       - |  8525 | `					}` |
|   63199 |  8526 | `					continue;` |
|       - |  8527 | `				}` |
|  159597 |  8528 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8529 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     159 |  8530 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     159 |  8531 | `					if( rc != SXRET_OK ){` |
|       8 |  8532 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8533 | `							return SXERR_ABORT;` |
|       - |  8534 | `						}` |
|       8 |  8535 | `						goto done;` |
|       - |  8536 | `					}` |
|     153 |  8537 | `					continue;` |
|       - |  8538 | `				}` |
|       - |  8539 | `				/* Extract the keyword */` |
|  159443 |  8540 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   79719 |  8541 | `			}` |
|  162927 |  8542 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8543 | `				/* Process constant declaration */` |
|      65 |  8544 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      65 |  8545 | `				if( rc != SXRET_OK ){` |
|       3 |  8546 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8547 | `						return SXERR_ABORT;` |
|       - |  8548 | `					}` |
|       3 |  8549 | `					goto done;` |
|       - |  8550 | `				}` |
|      34 |  8551 | `			}else{` |
|  162867 |  8552 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8553 | `					/* Static method or attribute,record that */` |
|    3353 |  8554 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    3353 |  8555 | `					pGen->pIn++; /* Jump the static keyword */` |
|    3353 |  8556 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8557 | `						/* Extract the keyword */` |
|    3347 |  8558 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    3347 |  8559 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8560 | `							iProtection = nKwrd;` |
|     ! 0 |  8561 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8562 | `						}` |
|    1671 |  8563 | `					}` |
|       - |  8564 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  8565 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  8566 | `					 * than a generic "expecting method" parse error. */` |
|    3353 |  8567 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8568 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8569 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  8570 | `					}` |
|    3348 |  8571 | `					if( pGen->pIn >= pGen->pEnd` |
|    3353 |  8572 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8573 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8574 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8575 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8576 | `						if( rc == SXERR_ABORT ){` |
|       - |  8577 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8578 | `							return SXERR_ABORT;` |
|       - |  8579 | `						}` |
|     ! 0 |  8580 | `						goto done;` |
|       - |  8581 | `					}` |
|    3353 |  8582 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8583 | `						/* Attribute declaration */` |
|       5 |  8584 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8585 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8586 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8587 | `								return SXERR_ABORT;` |
|       - |  8588 | `							}` |
|     ! 0 |  8589 | `							goto done;` |
|       - |  8590 | `						}` |
|       5 |  8591 | `						continue;` |
|       - |  8592 | `					}` |
|    3349 |  8593 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8594 | `						/* Typed static attribute declaration */` |
|      15 |  8595 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  8596 | `						if( rc != SXRET_OK ){` |
|       3 |  8597 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8598 | `								return SXERR_ABORT;` |
|       - |  8599 | `							}` |
|       3 |  8600 | `							goto done;` |
|       - |  8601 | `						}` |
|      13 |  8602 | `						continue;` |
|       - |  8603 | `					}` |
|       - |  8604 | `					/* Extract the keyword */` |
|    3337 |  8605 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  161185 |  8606 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8607 | `					/* Abstract method,record that */` |
|      12 |  8608 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8609 | `					/* Mark the whole class as abstract */` |
|      12 |  8610 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8611 | `					/* Advance the stream cursor */` |
|      12 |  8612 | `					pGen->pIn++;` |
|      12 |  8613 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8614 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8615 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8616 | `							iProtection = nKwrd;` |
|      10 |  8617 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8618 | `						}` |
|       5 |  8619 | `					}` |
|      12 |  8620 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8621 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8622 | `							/* Static method */` |
|     ! 0 |  8623 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8624 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8625 | `					}` |
|      12 |  8626 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8627 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8628 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8629 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8630 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8631 | `							if( rc == SXERR_ABORT ){` |
|       - |  8632 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8633 | `								return SXERR_ABORT;` |
|       - |  8634 | `							}` |
|     ! 0 |  8635 | `							goto done;` |
|       - |  8636 | `					}` |
|      12 |  8637 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  159514 |  8638 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8639 | `					/* final method ,record that */` |
|      17 |  8640 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  8641 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  8642 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8643 | `						/* Extract the keyword */` |
|      17 |  8644 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  8645 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  8646 | `							iProtection = nKwrd;` |
|       9 |  8647 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  8648 | `						}` |
|       7 |  8649 | `					}` |
|      17 |  8650 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  8651 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  8652 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  8653 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  8654 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  8655 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  8656 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  8657 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  8658 | `									return SXERR_ABORT;` |
|       - |  8659 | `								}` |
|     ! 0 |  8660 | `								goto done;` |
|       - |  8661 | `							}` |
|      12 |  8662 | `							continue;` |
|       - |  8663 | `					}` |
|       6 |  8664 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8665 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8666 | `							/* Static method */` |
|     ! 0 |  8667 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8668 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8669 | `					}` |
|       6 |  8670 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8671 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8672 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8673 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8674 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8675 | `							if( rc == SXERR_ABORT ){` |
|       - |  8676 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8677 | `								return SXERR_ABORT;` |
|       - |  8678 | `							}` |
|     ! 0 |  8679 | `							goto done;` |
|       - |  8680 | `					}` |
|       6 |  8681 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8682 | `				}` |
|  162841 |  8683 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8684 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8685 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8686 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8687 | `						if( rc == SXERR_ABORT ){` |
|       - |  8688 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8689 | `							return SXERR_ABORT;` |
|       - |  8690 | `						}` |
|     ! 0 |  8691 | `						goto done;` |
|       - |  8692 | `				}` |
|  162841 |  8693 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8694 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8695 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8696 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8697 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8698 | `						if( rc == SXERR_ABORT ){` |
|       - |  8699 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8700 | `							return SXERR_ABORT;` |
|       - |  8701 | `						}` |
|     ! 0 |  8702 | `						goto done;` |
|       - |  8703 | `					}` |
|       - |  8704 | `					/* Attribute declaration */` |
|       7 |  8705 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8706 | `				}else{` |
|       - |  8707 | `					/* Process method declaration */` |
|  162835 |  8708 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8709 | `				}` |
|  162841 |  8710 | `				if( rc != SXRET_OK ){` |
|      16 |  8711 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8712 | `						return SXERR_ABORT;` |
|       - |  8713 | `					}` |
|      16 |  8714 | `					goto done;` |
|       - |  8715 | `				}` |
|       - |  8716 | `			}` |
|   81446 |  8717 | `		}else{` |
|       - |  8718 | `			/* Attribute declaration */` |
|     ! 0 |  8719 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8720 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8721 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8722 | `					return SXERR_ABORT;` |
|       - |  8723 | `				}` |
|     ! 0 |  8724 | `				goto done;` |
|       - |  8725 | `			}` |
|       - |  8726 | `		}` |
|       5 |  8727 | `	}` |
|       - |  8728 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8729 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8730 | `	 */` |
|       - |  8731 | `	{` |
|       - |  8732 | `		TraitUseEntry *apUse;` |
|       - |  8733 | `		sxu32 nU;` |
|   90291 |  8734 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   90335 |  8735 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      49 |  8736 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      49 |  8737 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      49 |  8738 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      49 |  8739 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8740 | `			sxu32 nT;` |
|      49 |  8741 | `			if( !hasResolution ){` |
|       - |  8742 | `				/* No conflict resolution block: use standard trait application */` |
|      83 |  8743 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      47 |  8744 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      47 |  8745 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8746 | `						break;` |
|       - |  8747 | `					}` |
|      26 |  8748 | `				}` |
|      23 |  8749 | `			}else{` |
|       - |  8750 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8751 | `				 * then use the block to resolve method conflicts.` |
|       - |  8752 | `				 */` |
|       - |  8753 | `				SyToken *pR;` |
|      20 |  8754 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      12 |  8755 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8756 | `					ph7_class_attr *pAR;` |
|       - |  8757 | `					SyHashEntry *pER;` |
|       - |  8758 | `					SyString *pNR;` |
|      12 |  8759 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      17 |  8760 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8761 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8762 | `						pNR = &pAR->sName;` |
|     ! 0 |  8763 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8764 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8765 | `						}` |
|     ! 0 |  8766 | `					}` |
|      12 |  8767 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       7 |  8768 | `				}` |
|       - |  8769 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      10 |  8770 | `				pR = pUse->pResolvStart;` |
|      22 |  8771 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8772 | `					SyString sTrait,sMethod;` |
|       - |  8773 | `					ph7_class *pSrcTrait;` |
|       - |  8774 | `					ph7_class_method *pMeth;` |
|       - |  8775 | `					sxi32 nRKwrd;` |
|      34 |  8776 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      22 |  8777 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      14 |  8778 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      14 |  8779 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      14 |  8780 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      14 |  8781 | `					sMethod = pR->sData;` |
|      14 |  8782 | `					pR++;` |
|      14 |  8783 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8784 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8785 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8786 | `							sTrait = sMethod;` |
|       7 |  8787 | `							pR++;` |
|       7 |  8788 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8789 | `							sMethod = pR->sData;` |
|       7 |  8790 | `							pR++;` |
|       3 |  8791 | `						}` |
|       3 |  8792 | `					}` |
|      14 |  8793 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8794 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8795 | `						continue;` |
|       - |  8796 | `					}` |
|      14 |  8797 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      14 |  8798 | `					pR++;` |
|      14 |  8799 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8800 | `						pSrcTrait = 0;` |
|       7 |  8801 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8802 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8803 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8804 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8805 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8806 | `								break;` |
|       - |  8807 | `							}` |
|       2 |  8808 | `						}` |
|       5 |  8809 | `						if( pSrcTrait ){` |
|       5 |  8810 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8811 | `							if( pMeth ){` |
|       5 |  8812 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8813 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8814 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8815 | `								}` |
|       2 |  8816 | `							}` |
|       2 |  8817 | `						}` |
|       2 |  8818 | `					}` |
|      30 |  8819 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       2 |  8820 | `				}` |
|       - |  8821 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      20 |  8822 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8823 | `					ph7_class_method *pMR;` |
|       - |  8824 | `					SyHashEntry *pER;` |
|       - |  8825 | `					SyString *pNR;` |
|      12 |  8826 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      35 |  8827 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      20 |  8828 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      20 |  8829 | `						pNR = &pMR->sFunc.sName;` |
|      20 |  8830 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8831 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8832 | `						}` |
|       2 |  8833 | `					}` |
|       7 |  8834 | `				}` |
|       - |  8835 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      10 |  8836 | `				pR = pUse->pResolvStart;` |
|      22 |  8837 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8838 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8839 | `					ph7_class *pSrcTrait;` |
|       - |  8840 | `					ph7_class_method *pMeth;` |
|      22 |  8841 | `					int hasQual = 0;` |
|       - |  8842 | `					sxi32 nRKwrd;` |
|      34 |  8843 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      22 |  8844 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      14 |  8845 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      14 |  8846 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      14 |  8847 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      14 |  8848 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      14 |  8849 | `					sMethod = pR->sData;` |
|      14 |  8850 | `					pR++;` |
|      14 |  8851 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8852 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8853 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8854 | `							sTrait = sMethod;` |
|       7 |  8855 | `							hasQual = 1;` |
|       7 |  8856 | `							pR++;` |
|       7 |  8857 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8858 | `							sMethod = pR->sData;` |
|       7 |  8859 | `							pR++;` |
|       3 |  8860 | `						}` |
|       3 |  8861 | `					}` |
|      14 |  8862 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8863 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8864 | `						continue;` |
|       - |  8865 | `					}` |
|      14 |  8866 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      14 |  8867 | `					pR++;` |
|      14 |  8868 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      10 |  8869 | `						sxi32 iNewVis = -1;` |
|      10 |  8870 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  8871 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  8872 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  8873 | `								iNewVis = nAK;` |
|       7 |  8874 | `								pR++;` |
|       3 |  8875 | `							}` |
|       3 |  8876 | `						}` |
|      10 |  8877 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       8 |  8878 | `							sAlias = pR->sData;` |
|       8 |  8879 | `							pR++;` |
|       3 |  8880 | `						}` |
|      10 |  8881 | `						pMeth = 0;` |
|      10 |  8882 | `						if( hasQual ){` |
|       3 |  8883 | `							pSrcTrait = 0;` |
|       5 |  8884 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  8885 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  8886 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  8887 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  8888 | `									pSrcTrait = apTrait[nT];` |
|       3 |  8889 | `									break;` |
|       - |  8890 | `								}` |
|       2 |  8891 | `							}` |
|       3 |  8892 | `							if( pSrcTrait ){` |
|       3 |  8893 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  8894 | `							}` |
|       2 |  8895 | `						}else{` |
|       7 |  8896 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  8897 | `						}` |
|      10 |  8898 | `						if( pMeth ){` |
|      10 |  8899 | `							if( sAlias.nByte > 0 ){` |
|       - |  8900 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  8901 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  8902 | `								 */` |
|       - |  8903 | `								ph7_class_method *pAlias;` |
|       - |  8904 | `								char *zAliasDup;` |
|       8 |  8905 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       8 |  8906 | `								if( pAlias ){` |
|       8 |  8907 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       8 |  8908 | `									if( iNewVis >= 0 ){` |
|       5 |  8909 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8910 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8911 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  8912 | `									}` |
|       8 |  8913 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       8 |  8914 | `									if( zAliasDup ){` |
|       8 |  8915 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  8916 | `									}` |
|       5 |  8917 | `								}` |
|       6 |  8918 | `							}else if( iNewVis >= 0 ){` |
|       - |  8919 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  8920 | `								ph7_class_method *pCopy;` |
|       3 |  8921 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  8922 | `								if( pCopy ){` |
|       3 |  8923 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  8924 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  8925 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8926 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8927 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  8928 | `									/* Replace the method in the class hash */` |
|       3 |  8929 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  8930 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  8931 | `								}` |
|       1 |  8932 | `							}` |
|       4 |  8933 | `						}` |
|       4 |  8934 | `						SXUNUSED(hasQual);` |
|       4 |  8935 | `					}` |
|      18 |  8936 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       2 |  8937 | `				}` |
|       - |  8938 | `			}` |
|      49 |  8939 | `			SySetRelease(&pUse->aTraits);` |
|      27 |  8940 | `		}` |
|       - |  8941 | `	}` |
|       - |  8942 | `	/* Install the class */` |
|   90291 |  8943 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   90291 |  8944 | `	if( rc == SXRET_OK ){` |
|       - |  8945 | `		ph7_class **apInterface;` |
|       - |  8946 | `		sxu32 n;` |
|   90291 |  8947 | `		if( pBase ){` |
|       - |  8948 | `			/* Inherit from base class and mark as a subclass */` |
|   69581 |  8949 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   34788 |  8950 | `		}` |
|   90291 |  8951 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  100325 |  8952 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  8953 | `			/* Implements one or more interface */` |
|   10039 |  8954 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   10039 |  8955 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8956 | `				break;` |
|       - |  8957 | `			}` |
|    5022 |  8958 | `		}` |
|       - |  8959 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  8960 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  135429 |  8961 | `		if( rc == SXRET_OK` |
|   90286 |  8962 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   90291 |  8963 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   79431 |  8964 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  8965 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   79431 |  8966 | `			if( pStringable ){` |
|   79431 |  8967 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   79431 |  8968 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  8969 | `				sxu32 i;` |
|   79431 |  8970 | `				int bAlready = 0;` |
|   86047 |  8971 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    6623 |  8972 | `					if( apImpl[i] == pStringable ){` |
|       3 |  8973 | `						bAlready = 1;` |
|       3 |  8974 | `						break;` |
|       - |  8975 | `					}` |
|    3313 |  8976 | `				}` |
|   79431 |  8977 | `				if( !bAlready ){` |
|   79429 |  8978 | `					PH7_ClassImplement(pClass,pStringable);` |
|   39712 |  8979 | `				}` |
|   39713 |  8980 | `			}` |
|   39713 |  8981 | `		}` |
|       - |  8982 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   90291 |  8983 | `		if( rc == SXRET_OK ){` |
|   90291 |  8984 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   90291 |  8985 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8986 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8987 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8988 | `				return SXERR_ABORT;` |
|       - |  8989 | `			}` |
|   45143 |  8990 | `		}` |
|       - |  8991 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   90291 |  8992 | `		if( rc == SXRET_OK ){` |
|   90291 |  8993 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   90291 |  8994 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8995 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8996 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8997 | `				return SXERR_ABORT;` |
|       - |  8998 | `			}` |
|   45143 |  8999 | `		}` |
|   45143 |  9000 | `	}` |
|   90291 |  9001 | `	SySetRelease(&aUseEntries);` |
|   90291 |  9002 | `	SySetRelease(&aInterfaces);` |
|   90291 |  9003 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9004 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9005 | `		return SXERR_ABORT;` |
|       - |  9006 | `	}` |
|   45143 |  9007 | `done:` |
|       - |  9008 | `	/* Point beyond the class body */` |
|   90319 |  9009 | `	pGen->pIn = &pEnd[1];` |
|   90319 |  9010 | `	pGen->pEnd = pTmp;` |
|   90319 |  9011 | `	return PH7_OK;` |
|   45163 |  9012 |  |
|       - |  9013 | `/*` |
|       - |  9014 | ` * Compile a user-defined abstract class.` |
|       - |  9015 | ` *  According to the PHP language reference manual` |
|       - |  9016 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9017 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9018 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9019 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9020 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9021 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9022 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9023 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9024 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9025 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9026 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9027 | ` *   could differ.` |
|       - |  9028 | ` */` |
|       - |  9029 | `/*` |
|       - |  9030 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9031 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9032 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9033 | ` */` |
|  891326 |  9034 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9035 |  |
|  891331 |  9036 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  590729 |  9037 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  590729 |  9038 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  590711 |  9039 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  295330 |  9040 | `	}` |
|  891267 |  9041 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  891207 |  9042 | `	return FALSE;` |
|  445668 |  9043 |  |
|       - |  9044 | `/*` |
|       - |  9045 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9046 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9047 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9048 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9049 | ` */` |
|  891202 |  9050 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9051 |  |
|  891207 |  9052 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  891207 |  9053 | `	sxi32 iFlags = 0,iFlag;` |
|  891331 |  9054 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     129 |  9055 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9056 | `			pDup = pIn;` |
|       2 |  9057 | `		}` |
|     129 |  9058 | `		iFlags \|= iFlag;` |
|     129 |  9059 | `		pIn++;` |
|       5 |  9060 | `	}` |
|  891207 |  9061 | `	*ppIn = pIn;` |
|  891207 |  9062 | `	if( ppDup ){ *ppDup = pDup; }` |
|  891207 |  9063 | `	return iFlags;` |
|       5 |  9064 |  |
|       - |  9065 | `/*` |
|       - |  9066 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9067 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9068 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9069 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9070 | `` * `readonly`) to their existing handlers.`` |
|       - |  9071 | ` */` |
|  891150 |  9072 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9073 |  |
|  891155 |  9074 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  445634 |  9075 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  891178 |  9076 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9077 |  |
|       - |  9078 | `/*` |
|       - |  9079 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9080 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9081 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9082 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9083 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9084 | ` */` |
|      52 |  9085 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9086 |  |
|       - |  9087 | `	SyToken *pDup;` |
|      57 |  9088 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9089 | `	sxi32 rc;` |
|      57 |  9090 | `	if( pDup ){` |
|       4 |  9091 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9092 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9093 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9094 | `			return SXERR_ABORT;` |
|       - |  9095 | `		}` |
|       1 |  9096 | `	}` |
|      78 |  9097 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|      31 |  9098 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9099 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9100 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9101 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9102 | `			return SXERR_ABORT;` |
|       - |  9103 | `		}` |
|       1 |  9104 | `	}` |
|      57 |  9105 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|      31 |  9106 |  |
|       - |  9107 | `/*` |
|       - |  9108 | ` * Compile a user-defined trait.` |
|       - |  9109 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9110 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9111 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9112 | ` */` |
|      56 |  9113 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9114 |  |
|      61 |  9115 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9116 | `	ph7_class *pClass;` |
|       - |  9117 | `	SyToken *pEnd,*pTmp;` |
|       - |  9118 | `	sxi32 iProtection;` |
|       - |  9119 | `	sxi32 iAttrflags;` |
|       - |  9120 | `	SyString *pName;` |
|       - |  9121 | `	sxi32 nKwrd;` |
|       - |  9122 | `	sxi32 rc;` |
|       - |  9123 | `	/* Jump the 'trait' keyword */` |
|      61 |  9124 | `	pGen->pIn++;` |
|      61 |  9125 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9126 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9127 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9128 | `			return SXERR_ABORT;` |
|       - |  9129 | `		}` |
|     ! 0 |  9130 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9131 | `			pGen->pIn++;` |
|     ! 0 |  9132 | `		}` |
|     ! 0 |  9133 | `		return SXRET_OK;` |
|       - |  9134 | `	}` |
|       - |  9135 | `	/* Extract trait name */` |
|      61 |  9136 | `	pName = &pGen->pIn->sData;` |
|      61 |  9137 | `	pGen->pIn++;` |
|       - |  9138 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9139 | `		SyBlob sFQN;` |
|       - |  9140 | `		SyString sFQNStr;` |
|      61 |  9141 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      61 |  9142 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      61 |  9143 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      61 |  9144 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      61 |  9145 | `		SyBlobRelease(&sFQN);` |
|       - |  9146 | `	}` |
|      61 |  9147 | `	if( pClass == 0 ){` |
|     ! 0 |  9148 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9149 | `		return SXERR_ABORT;` |
|       - |  9150 | `	}` |
|       - |  9151 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      61 |  9152 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9153 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9154 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9155 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9156 | `			return SXERR_ABORT;` |
|       - |  9157 | `		}` |
|     ! 0 |  9158 | `		return SXRET_OK;` |
|       - |  9159 | `	}` |
|      61 |  9160 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      61 |  9161 | `	pEnd = 0;` |
|      61 |  9162 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      61 |  9163 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  9164 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  9165 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9166 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9167 | `			return SXERR_ABORT;` |
|       - |  9168 | `		}` |
|     ! 0 |  9169 | `		return SXRET_OK;` |
|       - |  9170 | `	}` |
|       - |  9171 | `	/* Swap token stream */` |
|      61 |  9172 | `	pTmp = pGen->pEnd;` |
|      61 |  9173 | `	pGen->pEnd = pEnd;` |
|       - |  9174 | `	/* Mark as trait */` |
|      61 |  9175 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9176 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      56 |  9177 | `	for(;;){` |
|     161 |  9178 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9179 | `			pGen->pIn++;` |
|       4 |  9180 | `		}` |
|     137 |  9181 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      61 |  9182 | `			break;` |
|       - |  9183 | `		}` |
|      81 |  9184 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9185 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9186 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9187 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9188 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9189 | `				return SXERR_ABORT;` |
|       - |  9190 | `			}` |
|     ! 0 |  9191 | `			goto done;` |
|       - |  9192 | `		}` |
|      81 |  9193 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      81 |  9194 | `		iAttrflags = 0;` |
|      81 |  9195 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      81 |  9196 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      81 |  9197 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9198 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9199 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9200 | `				for(;;){` |
|       - |  9201 | `					ph7_class *pUsedTrait;` |
|       - |  9202 | `					SyString *pUsedName;` |
|       5 |  9203 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9204 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9205 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9206 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9207 | `							return SXERR_ABORT;` |
|       - |  9208 | `						}` |
|     ! 0 |  9209 | `						break;` |
|       - |  9210 | `					}` |
|       5 |  9211 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9212 | `					{` |
|       - |  9213 | `						SyBlob sResolved;` |
|       5 |  9214 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9215 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9216 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9217 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9218 | `						SyBlobRelease(&sResolved);` |
|       - |  9219 | `					}` |
|       5 |  9220 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9221 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9222 | `					}` |
|       5 |  9223 | `					if( pUsedTrait == 0 ){` |
|       4 |  9224 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9225 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9226 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9227 | `							return SXERR_ABORT;` |
|       - |  9228 | `						}` |
|       2 |  9229 | `					}else{` |
|       3 |  9230 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9231 | `					}` |
|       5 |  9232 | `					pGen->pIn++;` |
|       5 |  9233 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  9234 | `						break;` |
|       - |  9235 | `					}` |
|     ! 0 |  9236 | `					pGen->pIn++;` |
|     ! 0 |  9237 | `				}` |
|       5 |  9238 | `				continue;` |
|       - |  9239 | `			}` |
|      77 |  9240 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  9241 | `				iProtection = nKwrd;` |
|      73 |  9242 | `				pGen->pIn++;` |
|      68 |  9243 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  9244 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  9245 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9246 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9247 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9248 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9249 | `						return SXERR_ABORT;` |
|       - |  9250 | `					}` |
|     ! 0 |  9251 | `					goto done;` |
|       - |  9252 | `				}` |
|      73 |  9253 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  9254 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9255 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9256 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9257 | `							return SXERR_ABORT;` |
|       - |  9258 | `						}` |
|     ! 0 |  9259 | `						goto done;` |
|       - |  9260 | `					}` |
|      12 |  9261 | `					continue;` |
|       - |  9262 | `				}` |
|      63 |  9263 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  9264 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  9265 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9266 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9267 | `							return SXERR_ABORT;` |
|       - |  9268 | `						}` |
|     ! 0 |  9269 | `						goto done;` |
|       - |  9270 | `					}` |
|       5 |  9271 | `					continue;` |
|       - |  9272 | `				}` |
|      58 |  9273 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  9274 | `			}` |
|      62 |  9275 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  9276 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9277 | `					"Traits cannot have constants");` |
|     ! 0 |  9278 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9279 | `					return SXERR_ABORT;` |
|       - |  9280 | `				}` |
|     ! 0 |  9281 | `				goto done;` |
|     ! 0 |  9282 | `			}else{` |
|      62 |  9283 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9284 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9285 | `					pGen->pIn++;` |
|       5 |  9286 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9287 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9288 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9289 | `							iProtection = nKwrd;` |
|     ! 0 |  9290 | `							pGen->pIn++;` |
|     ! 0 |  9291 | `						}` |
|       1 |  9292 | `					}` |
|       4 |  9293 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9294 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  9295 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9296 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9297 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9298 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9299 | `							return SXERR_ABORT;` |
|       - |  9300 | `						}` |
|     ! 0 |  9301 | `						goto done;` |
|       - |  9302 | `					}` |
|       5 |  9303 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9304 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9305 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9306 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9307 | `								return SXERR_ABORT;` |
|       - |  9308 | `							}` |
|     ! 0 |  9309 | `							goto done;` |
|       - |  9310 | `						}` |
|       3 |  9311 | `						continue;` |
|       - |  9312 | `					}` |
|       3 |  9313 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9314 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9315 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9316 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9317 | `								return SXERR_ABORT;` |
|       - |  9318 | `							}` |
|     ! 0 |  9319 | `							goto done;` |
|       - |  9320 | `						}` |
|     ! 0 |  9321 | `						continue;` |
|       - |  9322 | `					}` |
|       3 |  9323 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      59 |  9324 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 |  9325 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 |  9326 | `					pGen->pIn++;` |
|       6 |  9327 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 |  9328 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  9329 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  9330 | `							iProtection = nKwrd;` |
|       6 |  9331 | `							pGen->pIn++;` |
|       2 |  9332 | `						}` |
|       2 |  9333 | `					}` |
|       6 |  9334 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9335 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9336 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9337 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9338 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9339 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9340 | `							return SXERR_ABORT;` |
|       - |  9341 | `						}` |
|     ! 0 |  9342 | `						goto done;` |
|       - |  9343 | `					}` |
|       6 |  9344 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9345 | `				}` |
|      60 |  9346 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9347 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9348 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9349 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9350 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9351 | `						return SXERR_ABORT;` |
|       - |  9352 | `					}` |
|     ! 0 |  9353 | `					goto done;` |
|       - |  9354 | `				}` |
|      60 |  9355 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9356 | `					pGen->pIn++;` |
|     ! 0 |  9357 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9358 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9359 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9360 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9361 | `							return SXERR_ABORT;` |
|       - |  9362 | `						}` |
|     ! 0 |  9363 | `						goto done;` |
|       - |  9364 | `					}` |
|     ! 0 |  9365 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9366 | `				}else{` |
|      60 |  9367 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9368 | `				}` |
|      60 |  9369 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9370 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9371 | `						return SXERR_ABORT;` |
|       - |  9372 | `					}` |
|     ! 0 |  9373 | `					goto done;` |
|       - |  9374 | `				}` |
|       - |  9375 | `			}` |
|      32 |  9376 | `		}else{` |
|     ! 0 |  9377 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9378 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9379 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9380 | `					return SXERR_ABORT;` |
|       - |  9381 | `				}` |
|     ! 0 |  9382 | `				goto done;` |
|       - |  9383 | `			}` |
|       - |  9384 | `		}` |
|       4 |  9385 | `	}` |
|       - |  9386 | `	/* Install the trait */` |
|      61 |  9387 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      61 |  9388 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9389 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9390 | `		return SXERR_ABORT;` |
|       - |  9391 | `	}` |
|      28 |  9392 | `done:` |
|       - |  9393 | `	/* Point beyond the trait body */` |
|      61 |  9394 | `	pGen->pIn = &pEnd[1];` |
|      61 |  9395 | `	pGen->pEnd = pTmp;` |
|      61 |  9396 | `	return PH7_OK;` |
|      33 |  9397 |  |
|       - |  9398 | `/*` |
|       - |  9399 | ` * Compile a user-defined class.` |
|       - |  9400 | ` *  According to the PHP language reference manual` |
|       - |  9401 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9402 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9403 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9404 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9405 | ` *   and functions (called "methods").` |
|       - |  9406 | ` */` |
|   90264 |  9407 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 |  9408 |  |
|       - |  9409 | `	sxi32 rc;` |
|   90269 |  9410 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   90269 |  9411 | `	return rc;` |
|       5 |  9412 |  |
|       - |  9413 | `/*` |
|       - |  9414 | ` * Exception handling.` |
|       - |  9415 | ` *  According to the PHP language reference manual` |
|       - |  9416 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9417 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9418 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9419 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9420 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9421 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9422 | ` *    (or re-thrown) within a catch block.` |
|       - |  9423 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9424 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9425 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9426 | ` *    been defined with set_exception_handler().` |
|       - |  9427 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9428 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9429 | ` */` |
|       - |  9430 | `/*` |
|       - |  9431 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9432 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9433 | ` * indicates failure.` |
|       - |  9434 | ` */` |
|   10130 |  9435 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  9436 |  |
|   10135 |  9437 | `	sxi32 rc = SXRET_OK;` |
|   10135 |  9438 | `	if( pRoot->pOp ){` |
|   10127 |  9439 | `		switch( pRoot->pOp->iOp ){` |
|    5061 |  9440 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9441 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9442 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9443 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9444 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9445 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   10127 |  9446 | `			break;` |
|     ! 0 |  9447 | `		default:` |
|       - |  9448 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9449 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9450 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9451 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9452 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9453 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9454 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9455 | `			}` |
|     ! 0 |  9456 | `			break;` |
|       - |  9457 | `		}` |
|    5074 |  9458 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9459 | `		/* Unexpected expression */` |
|     ! 0 |  9460 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9461 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9462 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9463 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9464 | `		}` |
|     ! 0 |  9465 | `	}` |
|   10135 |  9466 | `	return rc;` |
|       5 |  9467 |  |
|       - |  9468 | `/*` |
|       - |  9469 | ` * Compile a 'throw' statement.` |
|       - |  9470 | ` * throw: This is how you trigger an exception.` |
|       - |  9471 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9472 | ` */` |
|   10094 |  9473 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 |  9474 |  |
|   10099 |  9475 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9476 | `	GenBlock *pBlock;` |
|       - |  9477 | `	sxu32 nIdx;` |
|       - |  9478 | `	sxi32 rc;` |
|   10099 |  9479 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9480 | `	/* Compile the expression */` |
|   10099 |  9481 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   10099 |  9482 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9483 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9484 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9485 | `			return SXERR_ABORT;` |
|       - |  9486 | `		}` |
|     ! 0 |  9487 | `		return SXRET_OK;` |
|       - |  9488 | `	}` |
|   10099 |  9489 | `	pBlock = pGen->pCurrent;` |
|       - |  9490 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   46641 |  9491 | `	while(pBlock->pParent){` |
|   46637 |  9492 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   10095 |  9493 | `			break;` |
|       - |  9494 | `		}` |
|       - |  9495 | `		/* Point to the parent block */` |
|   36547 |  9496 | `		pBlock = pBlock->pParent;` |
|       5 |  9497 | `	}` |
|       - |  9498 | `	/* Emit the throw instruction */` |
|   10099 |  9499 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9500 | `	/* Emit the jump */` |
|   10099 |  9501 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   10099 |  9502 | `	return SXRET_OK;` |
|    5052 |  9503 |  |
|       - |  9504 | `/*` |
|       - |  9505 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9506 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9507 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9508 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9509 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9510 | ` */` |
|      36 |  9511 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9512 |  |
|      38 |  9513 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9514 | `	GenBlock *pBlock;` |
|       - |  9515 | `	sxu32 nIdx;` |
|       - |  9516 | `	sxi32 rc;` |
|      18 |  9517 | `	(void)iCompileFlag;` |
|      38 |  9518 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9519 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9520 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9521 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9522 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9523 | `			return SXERR_ABORT;` |
|       - |  9524 | `		}` |
|     ! 0 |  9525 | `		return SXRET_OK;` |
|       - |  9526 | `	}` |
|      38 |  9527 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9528 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9529 | `		return SXERR_ABORT;` |
|       - |  9530 | `	}` |
|      38 |  9531 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9532 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9533 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9534 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9535 | `			return SXERR_ABORT;` |
|       - |  9536 | `		}` |
|     ! 0 |  9537 | `		return SXRET_OK;` |
|       - |  9538 | `	}` |
|       - |  9539 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  9540 | `	pBlock = pGen->pCurrent;` |
|      60 |  9541 | `	while( pBlock->pParent ){` |
|      49 |  9542 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  9543 | `			break;` |
|       - |  9544 | `		}` |
|      23 |  9545 | `		pBlock = pBlock->pParent;` |
|       1 |  9546 | `	}` |
|      38 |  9547 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 |  9548 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 |  9549 | `	return SXRET_OK;` |
|      20 |  9550 |  |
|       - |  9551 | `/*` |
|       - |  9552 | ` * Compile a 'catch' block.` |
|       - |  9553 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  9554 | ` * an object containing the exception information.` |
|       - |  9555 | ` */` |
|     422 |  9556 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 |  9557 |  |
|     427 |  9558 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9559 | `	ph7_exception_block sCatch;` |
|       - |  9560 | `	SySet *pInstrContainer;` |
|       - |  9561 | `	SyString sClassName;` |
|       - |  9562 | `	GenBlock *pCatch;` |
|       - |  9563 | `	SyToken *pToken;` |
|       - |  9564 | `	SyString *pName;` |
|       - |  9565 | `	char *zDup;` |
|       - |  9566 | `	sxi32 rc;` |
|     427 |  9567 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  9568 | `	/* Zero the structure */` |
|     427 |  9569 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  9570 | `	/* Initialize fields */` |
|     427 |  9571 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     427 |  9572 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     427 |  9573 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  9574 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9575 | `			pToken = pGen->pIn;` |
|     ! 0 |  9576 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9577 | `				pToken--;` |
|     ! 0 |  9578 | `			}` |
|     ! 0 |  9579 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9580 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9581 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9582 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9583 | `				return SXERR_ABORT;` |
|       - |  9584 | `			}` |
|     ! 0 |  9585 | `			return SXERR_INVALID;` |
|       - |  9586 | `	}` |
|       - |  9587 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     427 |  9588 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     224 |  9589 | `	for(;;){` |
|       - |  9590 | `		SyBlob sResolved;` |
|     453 |  9591 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     453 |  9592 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 |  9593 | `			SyBlobRelease(&sResolved);` |
|       6 |  9594 | `			pToken = pGen->pIn;` |
|       6 |  9595 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9596 | `				pToken--;` |
|     ! 0 |  9597 | `			}` |
|       8 |  9598 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9599 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  9600 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 |  9601 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9602 | `				return SXERR_ABORT;` |
|       - |  9603 | `			}` |
|       6 |  9604 | `			return SXERR_INVALID;` |
|       - |  9605 | `		}` |
|       - |  9606 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - |  9607 | `		 * transient SyBlob allocation. */` |
|     671 |  9608 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     444 |  9609 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     449 |  9610 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     449 |  9611 | `		SyBlobRelease(&sResolved);` |
|     449 |  9612 | `		if( zDup == 0 ){` |
|     ! 0 |  9613 | `			goto Mem;` |
|       - |  9614 | `		}` |
|     449 |  9615 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     449 |  9616 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9617 | `			goto Mem;` |
|       - |  9618 | `		}` |
|       - |  9619 | `		/* Check for '\|' (multi-catch separator) */` |
|     457 |  9620 | `		if( pGen->pIn < pGen->pEnd &&` |
|     444 |  9621 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      31 |  9622 | `			pGen->pIn->sData.nByte == 1 &&` |
|      26 |  9623 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      28 |  9624 | `			pGen->pIn++; /* Consume the '\|' */` |
|      28 |  9625 | `			continue;` |
|       - |  9626 | `		}` |
|     423 |  9627 | `		break;` |
|     ! 0 |  9628 | `	}` |
|     627 |  9629 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     423 |  9630 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  9631 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9632 | `			pToken = pGen->pIn;` |
|     ! 0 |  9633 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9634 | `				pToken--;` |
|     ! 0 |  9635 | `			}` |
|     ! 0 |  9636 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9637 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9638 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9639 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9640 | `				return SXERR_ABORT;` |
|       - |  9641 | `			}` |
|     ! 0 |  9642 | `			return SXERR_INVALID;` |
|       - |  9643 | `	}` |
|     423 |  9644 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9645 | `	/* Duplicate instance name */` |
|     423 |  9646 | `	pName = &pGen->pIn->sData;` |
|     423 |  9647 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     423 |  9648 | `	if( zDup == 0 ){` |
|     ! 0 |  9649 | `		goto Mem;` |
|       - |  9650 | `	}` |
|     423 |  9651 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     423 |  9652 | `	pGen->pIn++;` |
|     423 |  9653 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9654 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9655 | `		pToken = pGen->pIn;` |
|     ! 0 |  9656 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9657 | `			pToken--;` |
|     ! 0 |  9658 | `		}` |
|     ! 0 |  9659 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9660 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9661 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9662 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9663 | `			return SXERR_ABORT;` |
|       - |  9664 | `		}` |
|     ! 0 |  9665 | `		return SXERR_INVALID;` |
|       - |  9666 | `	}` |
|       - |  9667 | `	/* Compile the block */` |
|     423 |  9668 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9669 | `	/* Create the catch block */` |
|     423 |  9670 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     423 |  9671 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9672 | `		return SXERR_ABORT;` |
|       - |  9673 | `	}` |
|       - |  9674 | `	/* Swap bytecode container */` |
|     423 |  9675 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     423 |  9676 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9677 | `	/* Compile the block */` |
|     423 |  9678 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9679 | `	/* Fix forward jumps now the destination is resolved  */` |
|     423 |  9680 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9681 | `	/* Emit the DONE instruction */` |
|     423 |  9682 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9683 | `	/* Leave the block */` |
|     423 |  9684 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9685 | `	/* Restore the default container */` |
|     423 |  9686 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9687 | `	/* Install the catch block */` |
|     423 |  9688 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     423 |  9689 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9690 | `		goto Mem;` |
|       - |  9691 | `	}` |
|     423 |  9692 | `	return SXRET_OK;` |
|     ! 0 |  9693 | `Mem:` |
|     ! 0 |  9694 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9695 | `	return SXERR_ABORT;` |
|     216 |  9696 |  |
|       - |  9697 | `/*` |
|       - |  9698 | ` * Compile a 'try' block.` |
|       - |  9699 | ` * A function using an exception should be in a "try" block.` |
|       - |  9700 | ` * If the exception does not trigger, the code will continue` |
|       - |  9701 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9702 | ` * is "thrown".` |
|       - |  9703 | ` */` |
|     436 |  9704 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 |  9705 |  |
|       - |  9706 | `	ph7_exception *pException;` |
|     441 |  9707 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9708 | `	GenBlock *pTry;` |
|       - |  9709 | `	sxu32 nJmpIdx;` |
|       - |  9710 | `	sxi32 rc;` |
|       - |  9711 | `	/* Create the exception container */` |
|     441 |  9712 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     441 |  9713 | `	if( pException == 0 ){` |
|     ! 0 |  9714 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9715 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9716 | `		return SXERR_ABORT;` |
|       - |  9717 | `	}` |
|       - |  9718 | `	/* Zero the structure */` |
|     441 |  9719 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9720 | `	/* Initialize fields */` |
|     441 |  9721 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     441 |  9722 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     441 |  9723 | `	pException->iHasFinally = 0;` |
|     441 |  9724 | `	pException->iFinallyDone = 0;` |
|     441 |  9725 | `	pException->pVm = pGen->pVm;` |
|       - |  9726 | `	/* Create the try block */` |
|     441 |  9727 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     441 |  9728 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9729 | `		return SXERR_ABORT;` |
|       - |  9730 | `	}` |
|       - |  9731 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     441 |  9732 | `	pTry->pUserData = pException;` |
|       - |  9733 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     441 |  9734 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9735 | `	/* Fix the jump later when the destination is resolved */` |
|     441 |  9736 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     441 |  9737 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9738 | `	/* Compile the block */` |
|     441 |  9739 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     441 |  9740 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9741 | `		return SXERR_ABORT;` |
|       - |  9742 | `	}` |
|       - |  9743 | `	/* Fix forward jumps now the destination is resolved */` |
|     441 |  9744 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9745 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     441 |  9746 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  9747 | `	/* Leave the block */` |
|     441 |  9748 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9749 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     441 |  9750 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     434 |  9751 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  9752 | `		/* Compile one or more catch blocks */` |
|     418 |  9753 | `		for(;;){` |
|     836 |  9754 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     658 |  9755 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     212 |  9756 | `					break;` |
|       - |  9757 | `			}` |
|     427 |  9758 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     427 |  9759 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9760 | `				return SXERR_ABORT;` |
|       - |  9761 | `			}` |
|       5 |  9762 | `		}` |
|     207 |  9763 | `	}` |
|       - |  9764 | `	/* Compile optional finally block */` |
|     441 |  9765 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     216 |  9766 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  9767 | `		SySet *pInstrContainer;` |
|       - |  9768 | `		GenBlock *pFinBlock;` |
|      53 |  9769 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  9770 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      53 |  9771 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      53 |  9772 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9773 | `			return SXERR_ABORT;` |
|       - |  9774 | `		}` |
|       - |  9775 | `		/* Swap bytecode container */` |
|      53 |  9776 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      53 |  9777 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  9778 | `		/* Compile the finally body */` |
|      53 |  9779 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      53 |  9780 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9781 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  9782 | `			return SXERR_ABORT;` |
|       - |  9783 | `		}` |
|       - |  9784 | `		/* Fix forward jumps now the destination is resolved */` |
|      53 |  9785 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9786 | `		/* Emit DONE to terminate the finally block */` |
|      53 |  9787 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9788 | `		/* Leave the block */` |
|      53 |  9789 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9790 | `		/* Restore the default container */` |
|      53 |  9791 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      53 |  9792 | `		pException->iHasFinally = 1;` |
|      24 |  9793 | `	}` |
|       - |  9794 | `	/* Must have at least one catch or finally */` |
|     441 |  9795 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       9 |  9796 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9797 | `			"Cannot use try without catch or finally");` |
|       9 |  9798 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9799 | `			return SXERR_ABORT;` |
|       - |  9800 | `		}` |
|       3 |  9801 | `	}` |
|     441 |  9802 | `	return SXRET_OK;` |
|     223 |  9803 |  |
|       - |  9804 | `/*` |
|       - |  9805 | ` * Compile a switch block.` |
|       - |  9806 | ` *  (See block-comment below for more information)` |
|       - |  9807 | ` */` |
|     112 |  9808 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 |  9809 |  |
|     117 |  9810 | `	sxi32 rc = SXRET_OK;` |
|     117 |  9811 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  9812 | `		/* Unexpected token */` |
|     ! 0 |  9813 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9814 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9815 | `			return SXERR_ABORT;` |
|       - |  9816 | `		}` |
|     ! 0 |  9817 | `		pGen->pIn++;` |
|     ! 0 |  9818 | `	}` |
|     117 |  9819 | `	pGen->pIn++;` |
|       - |  9820 | `	/* First instruction to execute in this block. */` |
|     117 |  9821 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  9822 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  9823 | `	 * or the '}' token */` |
|     206 |  9824 | `	for(;;){` |
|     417 |  9825 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9826 | `			/* No more input to process */` |
|     ! 0 |  9827 | `			break;` |
|       - |  9828 | `		}` |
|     417 |  9829 | `		rc = SXRET_OK;` |
|     417 |  9830 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 |  9831 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 |  9832 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  9833 | `					/* Unexpected token */` |
|     ! 0 |  9834 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9835 | `						&pGen->pIn->sData);` |
|     ! 0 |  9836 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9837 | `						return SXERR_ABORT;` |
|       - |  9838 | `					}` |
|       - |  9839 | `					/* FALL THROUGH */` |
|     ! 0 |  9840 | `				}` |
|      31 |  9841 | `				rc = SXERR_EOF;` |
|      31 |  9842 | `				break;` |
|       - |  9843 | `			}` |
|      32 |  9844 | `		}else{` |
|       - |  9845 | `			sxi32 nKwrd;` |
|       - |  9846 | `			/* Extract the keyword */` |
|     337 |  9847 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 |  9848 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 |  9849 | `				break;` |
|       - |  9850 | `			}` |
|     253 |  9851 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9852 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  9853 | `					/* Unexpected token */` |
|     ! 0 |  9854 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9855 | `						&pGen->pIn->sData);` |
|     ! 0 |  9856 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9857 | `						return SXERR_ABORT;` |
|       - |  9858 | `					}` |
|       - |  9859 | `					/* FALL THROUGH */` |
|     ! 0 |  9860 | `				}` |
|       - |  9861 | `				/* Block compiled */` |
|       3 |  9862 | `				break;` |
|       - |  9863 | `			}` |
|       - |  9864 | `		}` |
|       - |  9865 | `		/* Compile block */` |
|     305 |  9866 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 |  9867 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9868 | `			return SXERR_ABORT;` |
|       - |  9869 | `		}` |
|       5 |  9870 | `	}` |
|     117 |  9871 | `	return rc;` |
|      61 |  9872 |  |
|       - |  9873 | `/*` |
|       - |  9874 | ` * Compile a case eXpression.` |
|       - |  9875 | ` *  (See block-comment below for more information)` |
|       - |  9876 | ` */` |
|      92 |  9877 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 |  9878 |  |
|       - |  9879 | `	SySet *pInstrContainer;` |
|       - |  9880 | `	SyToken *pEnd,*pTmp;` |
|      97 |  9881 | `	sxi32 iNest = 0;` |
|       - |  9882 | `	sxi32 rc;` |
|       - |  9883 | `	/* Delimit the expression */` |
|      97 |  9884 | `	pEnd = pGen->pIn;` |
|     197 |  9885 | `	while( pEnd < pGen->pEnd ){` |
|     197 |  9886 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  9887 | `			/* Increment nesting level */` |
|       3 |  9888 | `			iNest++;` |
|     196 |  9889 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  9890 | `			/* Decrement nesting level */` |
|       3 |  9891 | `			iNest--;` |
|     194 |  9892 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 |  9893 | `			break;` |
|       - |  9894 | `		}` |
|     105 |  9895 | `		pEnd++;` |
|       5 |  9896 | `	}` |
|      97 |  9897 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  9898 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  9899 | `		if( rc == SXERR_ABORT ){` |
|       - |  9900 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9901 | `			return SXERR_ABORT;` |
|       - |  9902 | `		}` |
|     ! 0 |  9903 | `	}` |
|       - |  9904 | `	/* Swap token stream */` |
|      97 |  9905 | `	pTmp = pGen->pEnd;` |
|      97 |  9906 | `	pGen->pEnd = pEnd;` |
|      97 |  9907 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 |  9908 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 |  9909 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  9910 | `	/* Emit the done instruction */` |
|      97 |  9911 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 |  9912 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9913 | `	/* Update token stream */` |
|      97 |  9914 | `	pGen->pIn  = pEnd;` |
|      97 |  9915 | `	pGen->pEnd = pTmp;` |
|      97 |  9916 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9917 | `		return SXERR_ABORT;` |
|       - |  9918 | `	}` |
|      97 |  9919 | `	return SXRET_OK;` |
|      51 |  9920 |  |
|       - |  9921 | `/*` |
|       - |  9922 | ` * Compile the smart switch statement.` |
|       - |  9923 | ` * According to the PHP language reference manual` |
|       - |  9924 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  9925 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  9926 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  9927 | ` *  This is exactly what the switch statement is for.` |
|       - |  9928 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  9929 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  9930 | ` *  of the outer loop, use continue 2.` |
|       - |  9931 | ` *  Note that switch/case does loose comparision.` |
|       - |  9932 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  9933 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  9934 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  9935 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  9936 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  9937 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  9938 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  9939 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  9940 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  9941 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  9942 | ` *  list for the next case.` |
|       - |  9943 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  9944 | ` *  or floating-point numbers and strings.` |
|       - |  9945 | ` */` |
|      28 |  9946 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 |  9947 |  |
|       - |  9948 | `	GenBlock *pSwitchBlock;` |
|       - |  9949 | `	SyToken *pTmp,*pEnd;` |
|       - |  9950 | `	ph7_switch *pSwitch;` |
|       - |  9951 | `	sxu32 nToken;` |
|       - |  9952 | `	sxu32 nLine;` |
|       - |  9953 | `	sxi32 rc;` |
|      33 |  9954 | `	nLine = pGen->pIn->nLine;` |
|       - |  9955 | `	/* Jump the 'switch' keyword */` |
|      33 |  9956 | `	pGen->pIn++;` |
|      33 |  9957 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  9958 | `		/* Syntax error */` |
|     ! 0 |  9959 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  9960 | `		if( rc == SXERR_ABORT ){` |
|       - |  9961 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9962 | `			return SXERR_ABORT;` |
|       - |  9963 | `		}` |
|     ! 0 |  9964 | `		goto Synchronize;` |
|       - |  9965 | `	}` |
|       - |  9966 | `	/* Jump the left parenthesis '(' */` |
|      33 |  9967 | `	pGen->pIn++;` |
|      33 |  9968 | `	pEnd = 0; /* cc warning */` |
|       - |  9969 | `	/* Create the loop block */` |
|      47 |  9970 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  9971 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 |  9972 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9973 | `		return SXERR_ABORT;` |
|       - |  9974 | `	}` |
|       - |  9975 | `	/* Delimit the condition */` |
|      33 |  9976 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 |  9977 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  9978 | `		/* Empty expression */` |
|     ! 0 |  9979 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  9980 | `		if( rc == SXERR_ABORT ){` |
|       - |  9981 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9982 | `			return SXERR_ABORT;` |
|       - |  9983 | `		}` |
|     ! 0 |  9984 | `	}` |
|       - |  9985 | `	/* Swap token streams */` |
|      33 |  9986 | `	pTmp = pGen->pEnd;` |
|      33 |  9987 | `	pGen->pEnd = pEnd;` |
|       - |  9988 | `	/* Compile the expression */` |
|      33 |  9989 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 |  9990 | `	if( rc == SXERR_ABORT ){` |
|       - |  9991 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  9992 | `		return SXERR_ABORT;` |
|       - |  9993 | `	}` |
|       - |  9994 | `	/* Update token stream */` |
|      33 |  9995 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  9996 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9997 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9998 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9999 | `			return SXERR_ABORT;` |
|       - | 10000 | `		}` |
|     ! 0 | 10001 | `		pGen->pIn++;` |
|     ! 0 | 10002 | `	}` |
|      33 | 10003 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 10004 | `	pGen->pEnd = pTmp;` |
|      33 | 10005 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 10006 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 10007 | `			pTmp = pGen->pIn;` |
|     ! 0 | 10008 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 10009 | `				pTmp--;` |
|     ! 0 | 10010 | `			}` |
|       - | 10011 | `			/* Unexpected token */` |
|     ! 0 | 10012 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 10013 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10014 | `				return SXERR_ABORT;` |
|       - | 10015 | `			}` |
|     ! 0 | 10016 | `			goto Synchronize;` |
|       - | 10017 | `	}` |
|       - | 10018 | `	/* Set the delimiter token */` |
|      33 | 10019 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 10020 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 10021 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 10022 | `	}else{` |
|      31 | 10023 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 10024 | `	}` |
|      33 | 10025 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 10026 | `	/* Create the switch blocks container */` |
|      33 | 10027 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 10028 | `	if( pSwitch == 0 ){` |
|       - | 10029 | `		/* Abort compilation */` |
|     ! 0 | 10030 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10031 | `		return SXERR_ABORT;` |
|       - | 10032 | `	}` |
|       - | 10033 | `	/* Zero the structure */` |
|      33 | 10034 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 10035 | `	/* Initialize fields */` |
|      33 | 10036 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 10037 | `	/* Emit the switch instruction */` |
|      33 | 10038 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 10039 | `	/* Compile case blocks */` |
|     100 | 10040 | `	for(;;){` |
|       - | 10041 | `		sxu32 nKwrd;` |
|     119 | 10042 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10043 | `			/* No more input to process */` |
|     ! 0 | 10044 | `			break;` |
|       - | 10045 | `		}` |
|     119 | 10046 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 10047 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 10048 | `				/* Unexpected token */` |
|     ! 0 | 10049 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10050 | `					&pGen->pIn->sData);` |
|     ! 0 | 10051 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10052 | `					return SXERR_ABORT;` |
|       - | 10053 | `				}` |
|       - | 10054 | `				/* FALL THROUGH */` |
|     ! 0 | 10055 | `			}` |
|       - | 10056 | `			/* Block compiled */` |
|     ! 0 | 10057 | `			break;` |
|       - | 10058 | `		}` |
|       - | 10059 | `		/* Extract the keyword */` |
|     119 | 10060 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 10061 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10062 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 10063 | `				/* Unexpected token */` |
|     ! 0 | 10064 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10065 | `					&pGen->pIn->sData);` |
|     ! 0 | 10066 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10067 | `					return SXERR_ABORT;` |
|       - | 10068 | `				}` |
|       - | 10069 | `				/* FALL THROUGH */` |
|     ! 0 | 10070 | `			}` |
|       - | 10071 | `			/* Block compiled */` |
|       3 | 10072 | `			break;` |
|       - | 10073 | `		}` |
|     117 | 10074 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 10075 | `			/*` |
|       - | 10076 | `			 * Accroding to the PHP language reference manual` |
|       - | 10077 | `			 *  A special case is the default case. This case matches anything` |
|       - | 10078 | `			 *  that wasn't matched by the other cases.` |
|       - | 10079 | `			 */` |
|      25 | 10080 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 10081 | `				/* Default case already compiled */` |
|     ! 0 | 10082 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 10083 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10084 | `					return SXERR_ABORT;` |
|       - | 10085 | `				}` |
|     ! 0 | 10086 | `			}` |
|      25 | 10087 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 10088 | `			/* Compile the default block */` |
|      25 | 10089 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 10090 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10091 | `				return SXERR_ABORT;` |
|      25 | 10092 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 10093 | `				break;` |
|       1 | 10094 | `			}` |
|      98 | 10095 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 10096 | `			ph7_case_expr sCase;` |
|       - | 10097 | `			/* Standard case block */` |
|      97 | 10098 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 10099 | `			/* initialize the structure */` |
|      97 | 10100 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 10101 | `			/* Compile the case expression */` |
|      97 | 10102 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 10103 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10104 | `				return SXERR_ABORT;` |
|       - | 10105 | `			}` |
|       - | 10106 | `			/* Compile the case block */` |
|      97 | 10107 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 10108 | `			/* Insert in the switch container */` |
|      97 | 10109 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 10110 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10111 | `				return SXERR_ABORT;` |
|      97 | 10112 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 10113 | `				break;` |
|       - | 10114 | `			}` |
|      47 | 10115 | `		}else{` |
|       - | 10116 | `			/* Unexpected token */` |
|     ! 0 | 10117 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10118 | `				&pGen->pIn->sData);` |
|     ! 0 | 10119 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10120 | `				return SXERR_ABORT;` |
|       - | 10121 | `			}` |
|     ! 0 | 10122 | `			break;` |
|       - | 10123 | `		}` |
|       5 | 10124 | `	}` |
|       - | 10125 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 10126 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 10127 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10128 | `	/* Release the loop block */` |
|      33 | 10129 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 10130 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 10131 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 10132 | `		pGen->pIn++;` |
|      14 | 10133 | `	}` |
|       - | 10134 | `	/* Statement successfully compiled */` |
|      33 | 10135 | `	return SXRET_OK;` |
|     ! 0 | 10136 | `Synchronize:` |
|       - | 10137 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 10138 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 10139 | `		pGen->pIn++;` |
|     ! 0 | 10140 | `	}` |
|     ! 0 | 10141 | `	return SXRET_OK;` |
|      19 | 10142 |  |
|       - | 10143 | `/*` |
|       - | 10144 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 10145 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 10146 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 10147 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 10148 | ` */` |
|       - | 10149 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 10150 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 10151 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 10152 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 10153 |  |
|       - | 10154 | `/*` |
|       - | 10155 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 10156 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 10157 | ` * patched entries from the pending set.` |
|       - | 10158 | ` */` |
| 2416730 | 10159 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 10160 |  |
| 2416735 | 10161 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 10162 | `	sxu32 nTarget;` |
|       - | 10163 | `	sxu32 *aIdx;` |
|       - | 10164 | `	sxu32 i;` |
| 2416735 | 10165 | `	if( nCur <= nBaseline ){` |
| 2416645 | 10166 | `		return;` |
|       - | 10167 | `	}` |
|      93 | 10168 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      93 | 10169 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     191 | 10170 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     101 | 10171 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     101 | 10172 | `		if( pInstr ){` |
|     101 | 10173 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 | 10174 | `		}` |
|      52 | 10175 | `	}` |
|      93 | 10176 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1208370 | 10177 |  |
|       - | 10178 |  |
|       - | 10179 | `/*` |
|       - | 10180 | ` * By-reference out-parameters of builtin functions.` |
|       - | 10181 | ` *` |
|       - | 10182 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 10183 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 10184 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 10185 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 10186 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 10187 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 10188 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 10189 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 10190 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 10191 | ` * creates it" behaviour).` |
|       - | 10192 | ` *` |
|       - | 10193 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 10194 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 10195 | ` */` |
|  392694 | 10196 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 10197 |  |
|       - | 10198 | `	static const struct {` |
|       - | 10199 | `		const char *zName;` |
|       - | 10200 | `		sxu32 nByte;` |
|       - | 10201 | `		sxu32 mask;` |
|       - | 10202 | `	} aByRef[] = {` |
|       - | 10203 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10204 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10205 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10206 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10207 | `	};` |
|       - | 10208 | `	sxu32 i;` |
|  392699 | 10209 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1181 | 10210 | `		return 0;` |
|       - | 10211 | `	}` |
| 1957451 | 10212 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1565974 | 10213 | `		if( pName->nByte == aByRef[i].nByte` |
|  803658 | 10214 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      51 | 10215 | `			return aByRef[i].mask;` |
|       - | 10216 | `		}` |
|  782969 | 10217 | `	}` |
|  391477 | 10218 | `	return 0;` |
|  196352 | 10219 |  |
|       - | 10220 | `/*` |
|       - | 10221 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 10222 | ` *` |
|       - | 10223 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 10224 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 10225 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 10226 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 10227 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 10228 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 10229 | ` */` |
|  392694 | 10230 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 10231 |  |
|       - | 10232 | `	SyToken *p, *pEnd;` |
|  392699 | 10233 | `	pOut->zString = 0;` |
|  392699 | 10234 | `	pOut->nByte = 0;` |
|  392699 | 10235 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 10236 | `		return;` |
|       - | 10237 | `	}` |
|  392699 | 10238 | `	p = pLeft->pStart;` |
|  392699 | 10239 | `	pEnd = pLeft->pEnd;` |
|       - | 10240 | `	/* Optional single leading namespace separator (absolute path). */` |
|  392699 | 10241 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      26 | 10242 | `		p++;` |
|      11 | 10243 | `	}` |
|  392699 | 10244 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1155 | 10245 | `		return;` |
|       - | 10246 | `	}` |
|       - | 10247 | `	/* Must be a single component: nothing follows the name token. */` |
|  391549 | 10248 | `	if( p + 1 != pEnd ){` |
|      30 | 10249 | `		return;` |
|       - | 10250 | `	}` |
|  391523 | 10251 | `	*pOut = p->sData;` |
|  196352 | 10252 |  |
|       - | 10253 | `/*` |
|       - | 10254 | ` * Generate bytecode for a given expression tree.` |
|       - | 10255 | ` * If something goes wrong while generating bytecode` |
|       - | 10256 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 10257 | ` * this function takes care of generating the appropriate` |
|       - | 10258 | ` * error message.` |
|       - | 10259 | ` */` |
| 3256474 | 10260 | `static sxi32 GenStateEmitExprCode(` |
|       - | 10261 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10262 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 10263 | `	sxi32 iFlags /* Control flags */` |
|       - | 10264 | `	)` |
|       5 | 10265 |  |
|       - | 10266 | `	VmInstr *pInstr;` |
|       - | 10267 | `	sxu32 nJmpIdx;` |
| 3256479 | 10268 | `	sxi32 iP1 = 0;` |
| 3256479 | 10269 | `	sxu32 iP2 = 0;` |
| 3256479 | 10270 | `	void *p3  = 0;` |
|       - | 10271 | `	sxi32 iVmOp;` |
|       - | 10272 | `	sxi32 rc;` |
| 3256479 | 10273 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3256479 | 10274 | `	sxu32 nRhsNsBase = 0;` |
| 3256479 | 10275 | `	if( pNode->xCode ){` |
|       - | 10276 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 10277 | `		/* Compile node */` |
| 2017295 | 10278 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2017295 | 10279 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2017295 | 10280 | `		RE_SWAP_DELIMITER(pGen);` |
| 2017295 | 10281 | `		return rc;` |
|       - | 10282 | `	}` |
| 1239189 | 10283 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10284 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10285 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10286 | `		return SXERR_ABORT;` |
|       - | 10287 | `	}` |
| 1239189 | 10288 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1239189 | 10289 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      59 | 10290 | `		sxu32 nJmp = 0;` |
|       - | 10291 | `		sxu32 nNcNsBase;` |
|       - | 10292 | `		VmInstr *pInstrFix;` |
|       - | 10293 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10294 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10295 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10296 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10297 | `		 * stack slot carries a writable nIdx. */` |
|      59 | 10298 | `		if( pNode->pRight ){` |
|      59 | 10299 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10300 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      59 | 10301 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10302 | `				return rc;` |
|       - | 10303 | `			}` |
|      59 | 10304 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10305 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10306 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10307 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10308 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10309 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10310 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10311 | `			 * cascade for the actual write path stays correct. */` |
|      59 | 10312 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      59 | 10313 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      29 | 10314 | `				pInstrFix->iP2 = 3;` |
|      13 | 10315 | `			}` |
|      28 | 10316 | `		}` |
|       - | 10317 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      59 | 10318 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10319 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      59 | 10320 | `		if( pNode->pLeft ){` |
|      59 | 10321 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10322 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      59 | 10323 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10324 | `				return rc;` |
|       - | 10325 | `			}` |
|      59 | 10326 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      28 | 10327 | `		}` |
|       - | 10328 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      59 | 10329 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10330 | `		/* Patch the short-circuit jump to land after the store. */` |
|      59 | 10331 | `		if( nJmp > 0 ){` |
|      59 | 10332 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      59 | 10333 | `			if( pInstrFix ){` |
|      59 | 10334 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10335 | `			}` |
|      28 | 10336 | `		}` |
|      59 | 10337 | `		return SXRET_OK;` |
|       - | 10338 | `	}` |
| 1239133 | 10339 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10340 | `		sxu32 nJz,nJmp;` |
|       - | 10341 | `		sxu32 nTernaryNsBase;` |
|       - | 10342 | `		/* Ternary operator require special handling */` |
|       - | 10343 | `		/* Phase#1: Compile the condition */` |
|    2627 | 10344 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2627 | 10345 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2627 | 10346 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10347 | `			return rc;` |
|       - | 10348 | `		}` |
|       - | 10349 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10350 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10351 | `		 * condition expression, not leak past the ternary. */` |
|    2627 | 10352 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2627 | 10353 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2627 | 10354 | `		if( pNode->pLeft ){` |
|       - | 10355 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10356 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2559 | 10357 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10358 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2559 | 10359 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2559 | 10360 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2559 | 10361 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10362 | `				return rc;` |
|       - | 10363 | `			}` |
|    2559 | 10364 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1282 | 10365 | `		}else{` |
|       - | 10366 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10367 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10368 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10369 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10370 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10371 | `		}` |
|       - | 10372 | `		/* Phase#4: Emit the unconditional jump */` |
|    2627 | 10373 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10374 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2627 | 10375 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2627 | 10376 | `		if( pInstr ){` |
|    2627 | 10377 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1311 | 10378 | `		}` |
|    2627 | 10379 | `		if( !pNode->pLeft ){` |
|       - | 10380 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10381 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10382 | `		}` |
|       - | 10383 | `		/* Phase#6: Compile the 'else' expression */` |
|    2627 | 10384 | `		if( pNode->pRight ){` |
|    2627 | 10385 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2627 | 10386 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2627 | 10387 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10388 | `				return rc;` |
|       - | 10389 | `			}` |
|    2627 | 10390 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1311 | 10391 | `		}` |
|    2627 | 10392 | `		if( nJmp > 0 ){` |
|       - | 10393 | `			/* Phase#7: Fix the unconditional jump */` |
|    2627 | 10394 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2627 | 10395 | `			if( pInstr ){` |
|    2627 | 10396 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1311 | 10397 | `			}` |
|    1311 | 10398 | `		}` |
|       - | 10399 | `		/* All done */` |
|    2627 | 10400 | `		return SXRET_OK;` |
|       - | 10401 | `	}` |
| 1236511 | 10402 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10403 | `	/* Generate code for the left tree */` |
| 1236511 | 10404 | `	if( pNode->pLeft ){` |
| 1236473 | 10405 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1236473 | 10406 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10407 | `			ph7_expr_node **apNode;` |
|  392819 | 10408 | `			int hasSpread = 0;` |
|  392819 | 10409 | `			int hasNamed = 0;` |
|  392819 | 10410 | `			int bAnySpread = 0;` |
|  392819 | 10411 | `			sxu32 byRefMask = 0;` |
|       - | 10412 | `			sxi32 nArgs;` |
|       - | 10413 | `			sxi32 n;` |
|       - | 10414 | `			/* Recurse and generate bytecodes for function arguments */` |
|  392819 | 10415 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  392819 | 10416 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10417 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10418 | `			{` |
|  392819 | 10419 | `				int seenNamed = 0;` |
|  778015 | 10420 | `				for( n = 0; n < nArgs; ++n ){` |
|  385203 | 10421 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     188 | 10422 | `						seenNamed = 1;` |
|     188 | 10423 | `						hasNamed = 1;` |
|  385111 | 10424 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      23 | 10425 | `						bAnySpread = 1;` |
|  385009 | 10426 | `					}else if( seenNamed ){` |
|       3 | 10427 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10428 | `							"Cannot use positional argument after named argument");` |
|       3 | 10429 | `						return SXERR_SYNTAX;` |
|       - | 10430 | `					}` |
|  192603 | 10431 | `				}` |
|       - | 10432 | `			}` |
|       - | 10433 | `			/* Read-only load */` |
|  392817 | 10434 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10435 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10436 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10437 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10438 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  392817 | 10439 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  392817 | 10440 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  392812 | 10441 | `				if( pCallName->nByte == 5` |
|  215576 | 10442 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   20139 | 10443 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  382750 | 10444 | `				}else if( pCallName->nByte == 5` |
|  195442 | 10445 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      83 | 10446 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      39 | 10447 | `				}` |
|       - | 10448 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10449 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10450 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10451 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10452 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10453 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  392817 | 10454 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10455 | `					SyString sBuiltin;` |
|  392699 | 10456 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  392699 | 10457 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  196347 | 10458 | `				}` |
|  196406 | 10459 | `			}` |
|  778011 | 10460 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  385199 | 10461 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  385199 | 10462 | `				sxi32 iArgFlags = iFlags & ~EXPR_FLAG_LOAD_IDX_STORE;` |
|       - | 10463 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 10464 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate). */` |
|  385199 | 10465 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      31 | 10466 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      13 | 10467 | `				}` |
|  385199 | 10468 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  385199 | 10469 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10470 | `					return rc;` |
|       - | 10471 | `				}` |
|       - | 10472 | `				/* Each argument is an independent nullsafe scope. */` |
|  385199 | 10473 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  385199 | 10474 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10475 | `					/* Emit spread opcode to unpack this array argument */` |
|      23 | 10476 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      23 | 10477 | `					hasSpread = 1;` |
|      10 | 10478 | `				}` |
|  192602 | 10479 | `			}` |
|       - | 10480 | `			/* Total number of given arguments */` |
|  392817 | 10481 | `			iP1 = nArgs;` |
|  392817 | 10482 | `			iP2 = hasSpread;` |
|       - | 10483 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10484 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  392817 | 10485 | `			if( hasNamed ){` |
|     101 | 10486 | `				sxu32 nStrBytes = 0;` |
|       - | 10487 | `				char *zBuf;` |
|     297 | 10488 | `				for( n = 0; n < nArgs; ++n ){` |
|     199 | 10489 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10490 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      91 | 10491 | `					}` |
|     101 | 10492 | `				}` |
|       - | 10493 | `				{` |
|     101 | 10494 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     101 | 10495 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      98 | 10496 | `					&pGen->pVm->sAllocator, mapSize);` |
|     101 | 10497 | `				if( pMap ){` |
|     101 | 10498 | `					SyZero(pMap, mapSize);` |
|     101 | 10499 | `					pMap->bHasNamed = 1;` |
|     101 | 10500 | `					pMap->nTotal = (sxu32)nArgs;` |
|     101 | 10501 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     101 | 10502 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     297 | 10503 | `					for( n = 0; n < nArgs; ++n ){` |
|     199 | 10504 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10505 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     185 | 10506 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     185 | 10507 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     185 | 10508 | `							zBuf += nb;` |
|      91 | 10509 | `						}` |
|       - | 10510 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     101 | 10511 | `					}` |
|     101 | 10512 | `					p3 = (void *)pMap;` |
|      49 | 10513 | `				}` |
|       - | 10514 | `				}` |
|      49 | 10515 | `			}` |
|       - | 10516 | `			/* Remove stale flags now */` |
|  392817 | 10517 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  196406 | 10518 | `		}` |
| 1236471 | 10519 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1236471 | 10520 | `		if( rc != SXRET_OK ){` |
|      34 | 10521 | `			return rc;` |
|       - | 10522 | `		}` |
| 1236441 | 10523 | `		if( !bIsChainOp ){` |
|       - | 10524 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 10525 | `			 * target the end of that LHS chain, which is right here. */` |
|  577617 | 10526 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  288806 | 10527 | `		}` |
| 1236441 | 10528 | `		if( iVmOp == PH7_OP_CALL ){` |
|  392817 | 10529 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  392817 | 10530 | `			if( pInstr ){` |
|  392817 | 10531 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  391643 | 10532 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 10533 | `					sxu32 nQual;` |
|  391643 | 10534 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10535 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 10536 | `					 * so the later NEW handler (if any) can see it. */` |
|  391643 | 10537 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 10538 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 10539 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 10540 | `					 * imports — class imports must NOT affect function` |
|       - | 10541 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 10542 | `					 * before NEW; we store the original literal index in the` |
|       - | 10543 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 10544 | `					 * the unqualified name and re-qualify with class imports. */` |
|  391643 | 10545 | `					if( bAbsolute ){` |
|      26 | 10546 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      15 | 10547 | `					}else{` |
|  391621 | 10548 | `						int fromImport = 0;` |
|  391621 | 10549 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  391621 | 10550 | `						pInstr->iP2 = (sxi32)nQual;` |
|  391621 | 10551 | `						if( nQual != nOrig ){` |
|       - | 10552 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 10553 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 10554 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 10555 | `							if( !fromImport ){` |
|       - | 10556 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 10557 | `								if( p3 == 0 ){` |
|      67 | 10558 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 10559 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 10560 | `									if( pMap ){` |
|      67 | 10561 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 10562 | `										p3 = (void *)pMap;` |
|      31 | 10563 | `									}` |
|      31 | 10564 | `								}` |
|      67 | 10565 | `								if( p3 ){` |
|      67 | 10566 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 10567 | `								}` |
|      31 | 10568 | `							}` |
|      36 | 10569 | `						}` |
|       5 | 10570 | `					}` |
|  196998 | 10571 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 10572 | `					/* Method call,flag that */` |
|     899 | 10573 | `					pInstr->iP2 = 1;` |
|     447 | 10574 | `				}` |
|  196411 | 10575 | `			}` |
| 1040035 | 10576 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 10577 | `			ph7_expr_node **apNode;` |
|       - | 10578 | `			sxi32 n;` |
|   85123 | 10579 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 10580 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 10581 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       - | 10582 | `			/* Recurse and generate bytecodes for array index */` |
|   85123 | 10583 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  153617 | 10584 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   68499 | 10585 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   68499 | 10586 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   68499 | 10587 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10588 | `					return rc;` |
|       - | 10589 | `				}` |
|       - | 10590 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   68499 | 10591 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   34252 | 10592 | `			}` |
|   85123 | 10593 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   68499 | 10594 | `				iP1 = 1; /* Node have an index associated with it */` |
|   34247 | 10595 | `			}` |
|   85123 | 10596 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 10597 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     243 | 10598 | `				iP2 = 4;` |
|   85004 | 10599 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 10600 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 10601 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      54 | 10602 | `				iP2 = 5;` |
|   84860 | 10603 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 10604 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 10605 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 10606 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 10607 | `				iP2 = 6;` |
|   84823 | 10608 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 10609 | `				/* Create an empty entry when the desired index is not found */` |
|   33523 | 10610 | `				iP2 = 1;` |
|   16764 | 10611 | `			}` |
|  801070 | 10612 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 10613 | `			/* POP the left node */` |
|      32 | 10614 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 10615 | `		}` |
|  618218 | 10616 | `	}` |
| 1236479 | 10617 | `	rc = SXRET_OK;` |
| 1236479 | 10618 | `	nJmpIdx = 0;` |
|       - | 10619 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 10620 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 10621 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1236479 | 10622 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     325 | 10623 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     325 | 10624 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     325 | 10625 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     325 | 10626 | `			int isSpecial = 0;` |
|     325 | 10627 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     237 | 10628 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     237 | 10629 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     247 | 10630 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     215 | 10631 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     109 | 10632 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      93 | 10633 | `					isSpecial = 1;` |
|      44 | 10634 | `				}` |
|     138 | 10635 | `			}` |
|     369 | 10636 | `			pInstr->iP1 = 0;` |
|     369 | 10637 | `			if( !isSpecial ){` |
|     193 | 10638 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      94 | 10639 | `			}` |
|       - | 10640 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 10641 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     281 | 10642 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     193 | 10643 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     193 | 10644 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 10645 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      46 | 10646 | `					return SXRET_OK;` |
|       - | 10647 | `				}` |
|      73 | 10648 | `			}` |
|     117 | 10649 | `		}` |
|     193 | 10650 | `	}` |
|       - | 10651 | `	/* Generate code for the right tree */` |
| 1236401 | 10652 | `	if( pNode->pRight ){` |
|  682601 | 10653 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 10654 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   10383 | 10655 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  677412 | 10656 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 10657 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3489 | 10658 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  670481 | 10659 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 10660 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 10661 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 10662 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  668726 | 10663 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 10664 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 10665 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 10666 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 10667 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 10668 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 10669 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     101 | 10670 | `			sxu32 nNsJmp = 0;` |
|     101 | 10671 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     101 | 10672 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  668566 | 10673 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  277115 | 10674 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  138555 | 10675 | `		}` |
|  682601 | 10676 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  682601 | 10677 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  682601 | 10678 | `		if( !bIsChainOp ){` |
|       - | 10679 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 10680 | `			 * operator instruction is emitted. */` |
|  501749 | 10681 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  250872 | 10682 | `		}` |
|  682601 | 10683 | `		if( iVmOp == PH7_OP_STORE ){` |
|  273551 | 10684 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  273522 | 10685 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 10686 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 10687 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 10688 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 10689 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 10690 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 10691 | `				 */` |
|      56 | 10692 | `				iVmOp = 0;` |
|  273525 | 10693 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  273499 | 10694 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 10695 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   76437 | 10696 | `					iP2 = 1;` |
|   38221 | 10697 | `				}else{` |
|  197067 | 10698 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10699 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   33477 | 10700 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   33477 | 10701 | `						iP1 = pInstr->iP1;` |
|   16741 | 10702 | `					}else{` |
|  163595 | 10703 | `						p3 = pInstr->p3;` |
|       - | 10704 | `					}` |
|       - | 10705 | `					/* POP the last dynamic load instruction */` |
|  197067 | 10706 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 10707 | `				}` |
|  136752 | 10708 | `			}` |
|  545828 | 10709 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      52 | 10710 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      52 | 10711 | `			if( pInstr ){` |
|      52 | 10712 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10713 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 10714 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 10715 | `					 */` |
|      15 | 10716 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 10717 | `					iP1 = pInstr->iP1;` |
|      15 | 10718 | `					iP2 = pInstr->iP2;` |
|      15 | 10719 | `					p3  = pInstr->p3;` |
|       8 | 10720 | `				}else{` |
|      38 | 10721 | `					p3 = pInstr->p3;` |
|       - | 10722 | `				}` |
|      25 | 10723 | `			}` |
|      25 | 10724 | `		}` |
|  341298 | 10725 | `	}` |
| 1236401 | 10726 | `	if( iVmOp > 0 ){` |
| 1236195 | 10727 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   13617 | 10728 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 10729 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    9943 | 10730 | `				iP1 = 1;` |
|    4974 | 10731 | `			}` |
| 1229389 | 10732 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 10733 | `			/* Namespace-qualify the class name for NEW */ {` |
|   17725 | 10734 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   17725 | 10735 | `				VmInstr *pCallInstr = 0;` |
|   17725 | 10736 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   17669 | 10737 | `					pCallInstr = pPeek;` |
|   17669 | 10738 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    8832 | 10739 | `				}` |
|   17725 | 10740 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   17723 | 10741 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10742 | `					sxu32 nLitForClass;` |
|       - | 10743 | `					/* If the CALL handler already qualified the name using` |
|       - | 10744 | `					 * function imports, recover the original unqualified` |
|       - | 10745 | `					 * literal so we can re-qualify with class imports. */` |
|   17723 | 10746 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 10747 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 10748 | `					}else{` |
|   17691 | 10749 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 10750 | `					}` |
|   17723 | 10751 | `					pPeek->iP1 = 0;` |
|   17723 | 10752 | `					if( !bAbsolute ){` |
|   17705 | 10753 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    8855 | 10754 | `					}else{` |
|      22 | 10755 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 10756 | `					}` |
|    8859 | 10757 | `				}` |
|       - | 10758 | `			}` |
|   17725 | 10759 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   17725 | 10760 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 10761 | `				VmInstr *pPrev;` |
|   17669 | 10762 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   17669 | 10763 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 10764 | `					/* Pop the call instruction, preserve named-arg map */` |
|   17669 | 10765 | `					iP1 = pInstr->iP1;` |
|   17669 | 10766 | `					if( pInstr->p3 ){` |
|      43 | 10767 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 10768 | `					}` |
|   17669 | 10769 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    8832 | 10770 | `				}` |
|    8837 | 10771 | `			}` |
| 1213723 | 10772 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 10773 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 10774 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     161 | 10775 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     161 | 10776 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     161 | 10777 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     161 | 10778 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     161 | 10779 | `				int isSpecialIs = 0;` |
|     161 | 10780 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     157 | 10781 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     157 | 10782 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     157 | 10783 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     152 | 10784 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      77 | 10785 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 10786 | `						isSpecialIs = 1;` |
|       5 | 10787 | `					}` |
|      77 | 10788 | `				}` |
|     163 | 10789 | `				pInstr->iP1 = 0;` |
|     163 | 10790 | `				if( !isSpecialIs && !bAbsolute ){` |
|     141 | 10791 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      68 | 10792 | `				}` |
|      82 | 10793 | `			}` |
| 1204788 | 10794 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 10795 | `			/* Prevent constant expansion for member/property names.` |
|       - | 10796 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 10797 | `			 * should not trigger constant lookup. */` |
|  180857 | 10798 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  180857 | 10799 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  180815 | 10800 | `				pInstr->iP1 = 0;` |
|   90405 | 10801 | `			}` |
|  180857 | 10802 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 10803 | `				/* Static member access,remember that */` |
|     247 | 10804 | `				iP1 = 1;` |
|     247 | 10805 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     247 | 10806 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      38 | 10807 | `					p3 = pInstr->p3;` |
|      38 | 10808 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      17 | 10809 | `				}` |
|     121 | 10810 | `			}` |
|   90426 | 10811 | `		}` |
|       - | 10812 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 10813 | `		 * This is the primary emit path for user-visible calls. */` |
| 1236193 | 10814 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  410537 | 10815 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  205266 | 10816 | `		}` |
|       - | 10817 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1236193 | 10818 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  618094 | 10819 | `	}` |
| 1236399 | 10820 | `	if( nJmpIdx > 0 ){` |
|       - | 10821 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   13991 | 10822 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   13991 | 10823 | `		if( pInstr ){` |
|   13991 | 10824 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    6993 | 10825 | `		}` |
|    6993 | 10826 | `	}` |
| 1236399 | 10827 | `	return rc;` |
| 1628223 | 10828 |  |
|       - | 10829 | `/*` |
|       - | 10830 | ` * Compile a PHP expression.` |
|       - | 10831 | ` * According to the PHP language reference manual:` |
|       - | 10832 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 10833 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 10834 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 10835 | ` *  is "anything that has a value".` |
|       - | 10836 | ` * If something goes wrong while compiling the expression,this` |
|       - | 10837 | ` * function takes care of generating the appropriate error` |
|       - | 10838 | ` * message.` |
|       - | 10839 | ` */` |
|  875968 | 10840 | `static sxi32 PH7_CompileExpr(` |
|       - | 10841 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10842 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 10843 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 10844 | `	)` |
|       5 | 10845 |  |
|       - | 10846 | `	ph7_expr_node *pRoot;` |
|       - | 10847 | `	SySet sExprNode;` |
|       - | 10848 | `	SyToken *pEnd;` |
|       - | 10849 | `	sxi32 nExpr;` |
|       - | 10850 | `	sxi32 iNest;` |
|       - | 10851 | `	sxi32 rc;` |
|       - | 10852 | `	sxu32 nNullsafeBase;` |
|       - | 10853 | `	/* Initialize worker variables */` |
|  875973 | 10854 | `	nExpr = 0;` |
|  875973 | 10855 | `	pRoot = 0;` |
|       - | 10856 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 10857 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  875973 | 10858 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  875973 | 10859 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  875973 | 10860 | `	SySetAlloc(&sExprNode,0x10);` |
|  875973 | 10861 | `	rc = SXRET_OK;` |
|       - | 10862 | `	/* Delimit the expression */` |
|  875973 | 10863 | `	pEnd = pGen->pIn;` |
|  875973 | 10864 | `	iNest = 0;` |
| 5860787 | 10865 | `	while( pEnd < pGen->pEnd ){` |
| 5561953 | 10866 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10867 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     413 | 10868 | `			iNest++;` |
| 5561749 | 10869 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     421 | 10870 | `			iNest--;` |
| 5561337 | 10871 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  577439 | 10872 | `			if( iNest <= 0 ){` |
|  577139 | 10873 | `				break;` |
|       - | 10874 | `			}` |
|     150 | 10875 | `		}` |
| 4984819 | 10876 | `		pEnd++;` |
|       5 | 10877 | `	}` |
|  875973 | 10878 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   20339 | 10879 | `		SyToken *pEnd2 = pGen->pIn;` |
|   20339 | 10880 | `		iNest = 0;` |
|       - | 10881 | `		/* Stop at the first comma */` |
|   40949 | 10882 | `		while( pEnd2 < pEnd ){` |
|   20621 | 10883 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      61 | 10884 | `				iNest++;` |
|   20593 | 10885 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      61 | 10886 | `				iNest--;` |
|   20537 | 10887 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      53 | 10888 | `				if( iNest <= 0 ){` |
|       7 | 10889 | `					break;` |
|       - | 10890 | `				}` |
|      21 | 10891 | `			}` |
|   20615 | 10892 | `			pEnd2++;` |
|       5 | 10893 | `		}` |
|   20339 | 10894 | `		if( pEnd2 <pEnd ){` |
|       7 | 10895 | `			pEnd = pEnd2;` |
|       3 | 10896 | `		}` |
|   10167 | 10897 | `	}` |
|  875973 | 10898 | `	if( pEnd > pGen->pIn ){` |
|  875963 | 10899 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 10900 | `		/* Swap delimiter */` |
|  875963 | 10901 | `		pGen->pEnd = pEnd;` |
|       - | 10902 | `		/* Try to get an expression tree */` |
|  875963 | 10903 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  875963 | 10904 | `		if( rc == SXRET_OK && pRoot ){` |
|  875781 | 10905 | `			rc = SXRET_OK;` |
|  875781 | 10906 | `			if( xTreeValidator ){` |
|       - | 10907 | `				/* Call the upper layer validator callback */` |
|   24227 | 10908 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   12111 | 10909 | `			}` |
|  875781 | 10910 | `			if( rc != SXERR_ABORT ){` |
|       - | 10911 | `				/* Generate code for the given tree */` |
|  875781 | 10912 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 10913 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 10914 | `				 * expression so they short-circuit to its end. */` |
|  875781 | 10915 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  437888 | 10916 | `			}` |
|  875781 | 10917 | `			nExpr = 1;` |
|  437888 | 10918 | `		}` |
|       - | 10919 | `		/* Release the whole tree */` |
|  875963 | 10920 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 10921 | `		/* Synchronize token stream */` |
|  875963 | 10922 | `		pGen->pEnd = pTmp;` |
|  875963 | 10923 | `		pGen->pIn  = pEnd;` |
|  875963 | 10924 | `		if( rc == SXERR_ABORT ){` |
|      14 | 10925 | `			SySetRelease(&sExprNode);` |
|      14 | 10926 | `			return SXERR_ABORT;` |
|       - | 10927 | `		}` |
|  437974 | 10928 | `	}` |
|  875963 | 10929 | `	SySetRelease(&sExprNode);` |
|  875963 | 10930 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  437989 | 10931 |  |
|       - | 10932 | `/*` |
|       - | 10933 | ` * Return a pointer to the node construct handler associated` |
|       - | 10934 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 10935 | ` */` |
|  222802 | 10936 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 10937 |  |
|  222807 | 10938 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 10939 | `		/* Numeric literal: Either real or integer */` |
|  116965 | 10940 | `		return PH7_CompileNumLiteral;` |
|  105847 | 10941 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 10942 | `		/* Double quoted string */` |
|   21921 | 10943 | `		return PH7_CompileString;` |
|   83931 | 10944 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 10945 | `		/* Single quoted string */` |
|   83815 | 10946 | `		return PH7_CompileSimpleString;` |
|     121 | 10947 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 10948 | `		/* Heredoc */` |
|      68 | 10949 | `		return PH7_CompileHereDoc;` |
|      57 | 10950 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 10951 | `		/* Nowdoc */` |
|      50 | 10952 | `		return PH7_CompileNowDoc;` |
|       8 | 10953 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 10954 | `		/* Backtick quoted string */` |
|       6 | 10955 | `		return PH7_CompileBacktic;` |
|       - | 10956 | `	}` |
|       3 | 10957 | `	return 0;` |
|  111406 | 10958 |  |
|       - | 10959 | `/*` |
|       - | 10960 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 10961 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 10962 | ` * in write context" parse error.` |
|       - | 10963 | ` */` |
|    6778 | 10964 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 10965 |  |
|       - | 10966 | `	sxi32 rc;` |
|    6783 | 10967 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6781 | 10968 | `		return SXRET_OK;` |
|       - | 10969 | `	}` |
|       5 | 10970 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 10971 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 10972 | `		"Can't use nullsafe operator in write context");` |
|       3 | 10973 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3394 | 10974 |  |
|       - | 10975 | `/*` |
|       - | 10976 | ` * Compile an unset() statement.` |
|       - | 10977 | ` * unset($var, $arr[$key], ...);` |
|       - | 10978 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 10979 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 10980 | ` * parent array before extracting the element to unset.` |
|       - | 10981 | ` */` |
|    2912 | 10982 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 10983 |  |
|    2917 | 10984 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2917 | 10985 | `	sxu32 nIdx = 0;` |
|       - | 10986 | `	SyString sName;` |
|       - | 10987 | `	sxi32 rc;` |
|       - | 10988 | `	/* Jump the 'unset' keyword */` |
|    2917 | 10989 | `	pGen->pIn++;` |
|       - | 10990 | `	/* Save delimiter */` |
|    2917 | 10991 | `	pTmp = pGen->pEnd;` |
|       - | 10992 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2917 | 10993 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2917 | 10994 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 10995 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 10996 | `		SyToken *pClose;` |
|    2917 | 10997 | `		pGen->pIn++;   /* Skip '(' */` |
|    2917 | 10998 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2917 | 10999 | `		pEnd = pClose; /* Stop at ')' */` |
|    1456 | 11000 | `	}` |
|    2917 | 11001 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 11002 | `	/* Resolve the 'unset' builtin name once */` |
|    2917 | 11003 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     363 | 11004 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     363 | 11005 | `		if( pObj == 0 ){` |
|     ! 0 | 11006 | `			return SXERR_ABORT;` |
|       - | 11007 | `		}` |
|     363 | 11008 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     363 | 11009 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     179 | 11010 | `	}` |
|       - | 11011 | `	/* Compile each comma-separated argument */` |
|    9697 | 11012 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6785 | 11013 | `		if( pGen->pIn < pNext ){` |
|    6785 | 11014 | `			pGen->pEnd = pNext;` |
|    6785 | 11015 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 11016 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 11017 | `				GenStateUnsetValidator);` |
|    6785 | 11018 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11019 | `				return SXERR_ABORT;` |
|       - | 11020 | `			}` |
|    6785 | 11021 | `			if( rc != SXERR_EMPTY ){` |
|       - | 11022 | `				/* Emit call for this single argument */` |
|    6783 | 11023 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6783 | 11024 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6783 | 11025 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3389 | 11026 | `			}` |
|    3390 | 11027 | `		}` |
|       - | 11028 | `		/* Jump trailing commas */` |
|   10655 | 11029 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3875 | 11030 | `			pNext++;` |
|       5 | 11031 | `		}` |
|    6785 | 11032 | `		pGen->pIn = pNext;` |
|       5 | 11033 | `	}` |
|       - | 11034 | `	/* Skip past the closing ')' if present */` |
|    2917 | 11035 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2917 | 11036 | `		pGen->pIn++;` |
|    1456 | 11037 | `	}` |
|       - | 11038 | `	/* Restore token stream */` |
|    2917 | 11039 | `	pGen->pEnd = pTmp;` |
|    2917 | 11040 | `	return SXRET_OK;` |
|    1461 | 11041 |  |
|       - | 11042 | `/*` |
|       - | 11043 | ` * PHP Language construct table.` |
|       - | 11044 | ` */` |
|       - | 11045 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 11046 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 11047 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 11048 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 11049 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 11050 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 11051 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 11052 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 11053 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 11054 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 11055 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 11056 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 11057 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 11058 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 11059 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 11060 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 11061 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 11062 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 11063 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 11064 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 11065 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 11066 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 11067 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 11068 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 11069 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 11070 | `};` |
|       - | 11071 | `/*` |
|       - | 11072 | ` * Return a pointer to the statement handler routine associated` |
|       - | 11073 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 11074 | ` */` |
|  590560 | 11075 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 11076 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 11077 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 11078 | `	)` |
|       5 | 11079 |  |
|  590565 | 11080 | `	sxu32 n = 0;` |
| 3048286 | 11081 | `	for(;;){` |
| 6096577 | 11082 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  126929 | 11083 | `			break;` |
|       - | 11084 | `		}` |
| 5969653 | 11085 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  463641 | 11086 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 11087 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 11088 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 11089 | `					/* 'static' (class context),return null */` |
|     ! 0 | 11090 | `					return 0;` |
|       - | 11091 | `				}` |
|     ! 0 | 11092 | `			}` |
|  463636 | 11093 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       6 | 11094 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 11095 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 11096 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 11097 | `				return 0;` |
|       - | 11098 | `			}` |
|       - | 11099 | `			/* Return a pointer to the handler.` |
|       - | 11100 | `			*/` |
|  463641 | 11101 | `			return aLangConstruct[n].xConstruct;` |
|       - | 11102 | `		}` |
| 5506017 | 11103 | `		n++;` |
|       5 | 11104 | `	}` |
|  126929 | 11105 | `	if( pLookahed ){` |
|  126929 | 11106 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   36419 | 11107 | `			return PH7_CompileClassInterface;` |
|   90515 | 11108 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   90269 | 11109 | `			return PH7_CompileClass;` |
|     251 | 11110 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      61 | 11111 | `			return PH7_CompileTrait;` |
|       - | 11112 | `		}` |
|       - | 11113 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 11114 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 11115 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 11116 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|      95 | 11117 | `	}` |
|       - | 11118 | `	/* Not a language construct */` |
|     195 | 11119 | `	return 0;` |
|  295285 | 11120 |  |
|       - | 11121 | `/*` |
|       - | 11122 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 11123 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 11124 | ` */` |
|     190 | 11125 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 11126 |  |
|       - | 11127 | `	int rc;` |
|     195 | 11128 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     195 | 11129 | `	if( rc == FALSE ){` |
|      82 | 11130 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      81 | 11131 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 11132 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 11133 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 11134 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 11135 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 11136 | `			*/` |
|       - | 11137 | `			){` |
|      79 | 11138 | `				rc = TRUE;` |
|      37 | 11139 | `		}` |
|      41 | 11140 | `	}` |
|     195 | 11141 | `	return rc;` |
|       5 | 11142 |  |
|       - | 11143 | `/*` |
|       - | 11144 | ` * Compile a PHP chunk.` |
|       - | 11145 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11146 | ` * takes care of generating the appropriate error message.` |
|       - | 11147 | ` */` |
|  707392 | 11148 | `static sxi32 GenStateCompileChunk(` |
|       - | 11149 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11150 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 11151 | `	)` |
|       5 | 11152 |  |
|       - | 11153 | `	ProcLangConstruct xCons;` |
|       - | 11154 | `	sxi32 rc;` |
|  707397 | 11155 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  551164 | 11156 | `	for(;;){` |
|  904865 | 11157 | `		int bStmtIsDeclare = 0;` |
|  904865 | 11158 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11159 | `			/* No more input to process */` |
|   13699 | 11160 | `			break;` |
|       - | 11161 | `		}` |
|       - | 11162 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 11163 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  891171 | 11164 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  590591 | 11165 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  590591 | 11166 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 11167 | `				bStmtIsDeclare = 1;` |
|      20 | 11168 | `			}` |
|  295293 | 11169 | `		}` |
|  891171 | 11170 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 11171 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 11172 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  197443 | 11173 | `			pGen->bStrictTypesLocked = 1;` |
|   98719 | 11174 | `		}` |
|  891171 | 11175 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11176 | `			/* Compile block */` |
|      21 | 11177 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      21 | 11178 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11179 | `				break;` |
|       - | 11180 | `			}` |
|      13 | 11181 | `		}else{` |
|  891155 | 11182 | `			xCons = 0;` |
|  891155 | 11183 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 11184 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 11185 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 11186 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|      57 | 11187 | `				xCons = PH7_CompileClassModifiers;` |
|  891129 | 11188 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  590565 | 11189 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 11190 | `				/* Try to extract a language construct handler */` |
|  590565 | 11191 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  590565 | 11192 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 11193 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 11194 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 11195 | `						&pGen->pIn->sData);` |
|       9 | 11196 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11197 | `						break;` |
|       - | 11198 | `					}` |
|       - | 11199 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 11200 | `					 * this erroneous statement.` |
|       - | 11201 | `					 */` |
|       9 | 11202 | `					xCons = PH7_ErrorRecover;` |
|       4 | 11203 | `				}` |
|  595823 | 11204 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   49265 | 11205 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 11206 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 11207 | `				xCons = PH7_CompileLabel;` |
|      56 | 11208 | `			}` |
|  891155 | 11209 | `			if( xCons == 0 ){` |
|       - | 11210 | `				/* Assume an expression an try to compile it */` |
|  300613 | 11211 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  300613 | 11212 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 11213 | `					/* Pop l-value */` |
|  300463 | 11214 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  150229 | 11215 | `				}` |
|  150309 | 11216 | `			}else{` |
|       - | 11217 | `				/* Go compile the sucker */` |
|  590547 | 11218 | `				rc = xCons(&(*pGen));` |
|       - | 11219 | `			}` |
|  891155 | 11220 | `			if( rc == SXERR_ABORT ){` |
|       - | 11221 | `				/* Request to abort compilation */` |
|      14 | 11222 | `				break;` |
|       - | 11223 | `			}` |
|       - | 11224 | `		}` |
|       - | 11225 | `		/* Ignore trailing semi-colons ';' */` |
| 1441923 | 11226 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  550767 | 11227 | `			pGen->pIn++;` |
|       5 | 11228 | `		}` |
|  891161 | 11229 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 11230 | `			/* Compile a single statement and return */` |
|  693693 | 11231 | `			break;` |
|       - | 11232 | `		}` |
|       - | 11233 | `		/* LOOP ONE */` |
|       - | 11234 | `		/* LOOP TWO */` |
|       - | 11235 | `		/* LOOP THREE */` |
|       - | 11236 | `		/* LOOP FOUR */` |
|       5 | 11237 | `	}` |
|       - | 11238 | `	/* Return compilation status */` |
|  707397 | 11239 | `	return rc;` |
|       5 | 11240 |  |
|       - | 11241 | `/*` |
|       - | 11242 | ` * Compile a Raw PHP chunk.` |
|       - | 11243 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11244 | ` * takes care of generating the appropriate error message.` |
|       - | 11245 | ` */` |
|   13706 | 11246 | `static sxi32 PH7_CompilePHP(` |
|       - | 11247 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11248 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 11249 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 11250 | `	)` |
|       5 | 11251 |  |
|   13711 | 11252 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 11253 | `	sxi32 rc;` |
|       - | 11254 | `	/* Reset the token set */` |
|   13711 | 11255 | `	SySetReset(&(*pTokenSet));` |
|       - | 11256 | `	/* Mark as the default token set */` |
|   13711 | 11257 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 11258 | `	/* Advance the stream cursor */` |
|   13711 | 11259 | `	pGen->pRawIn++;` |
|       - | 11260 | `	/* Tokenize the PHP chunk first */` |
|   13711 | 11261 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 11262 | `	/* Point to the head and tail of the token stream. */` |
|   13711 | 11263 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   13711 | 11264 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   13711 | 11265 | `	if( is_expr ){` |
|     ! 0 | 11266 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 11267 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 11268 | `			/* A simple expression,compile it */` |
|     ! 0 | 11269 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 11270 | `		}` |
|       - | 11271 | `		/* Emit the DONE instruction */` |
|     ! 0 | 11272 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 11273 | `		return SXRET_OK;` |
|       - | 11274 | `	}` |
|   13711 | 11275 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 11276 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 11277 | `		/*` |
|       - | 11278 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 11279 | `		 * According to the PHP reference manual:` |
|       - | 11280 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 11281 | `		 *  immediately follow` |
|       - | 11282 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 11283 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 11284 | `		 * Symisc extension:` |
|       - | 11285 | `		 *   This short syntax works with all PHP opening` |
|       - | 11286 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 11287 | `		 *   only short tag.` |
|       - | 11288 | `		 */` |
|       - | 11289 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 11290 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 11291 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11292 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11293 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11294 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11295 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11296 | `		}` |
|       3 | 11297 | `		return SXRET_OK;` |
|       - | 11298 | `	}` |
|       - | 11299 | `	/* Compile the PHP chunk */` |
|   13709 | 11300 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11301 | `	/* Fix exceptions jumps */` |
|   13709 | 11302 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11303 | `	/* Fix gotos now, the jump destination is resolved */` |
|   13709 | 11304 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11305 | `		rc = SXERR_ABORT;` |
|       1 | 11306 | `	}` |
|       - | 11307 | `	/* Reset container */` |
|   13709 | 11308 | `	SySetReset(&pGen->aGoto);` |
|   13709 | 11309 | `	SySetReset(&pGen->aLabel);` |
|   13709 | 11310 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11311 | `	/* Compilation result */` |
|   13709 | 11312 | `	return rc;` |
|    6858 | 11313 |  |
|       - | 11314 | `/*` |
|       - | 11315 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11316 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11317 | ` * This is the only compile interface exported from this file.` |
|       - | 11318 | ` */` |
|   16498 | 11319 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11320 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11321 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11322 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11323 | `	)` |
|       5 | 11324 |  |
|       - | 11325 | `	SySet aPhpToken,aRawToken;` |
|       - | 11326 | `	ph7_gen_state *pCodeGen;` |
|       - | 11327 | `	ph7_value *pRawObj;` |
|       - | 11328 | `	sxu32 nObjIdx;` |
|       - | 11329 | `	sxi32 nRawObj;` |
|       - | 11330 | `	int is_expr;` |
|       - | 11331 | `	sxi8 bSavedStrict;` |
|       - | 11332 | `	sxi8 bSavedStrictLocked;` |
|       - | 11333 | `	sxi32 rc;` |
|   16503 | 11334 | `	if( pScript->nByte < 1 ){` |
|       - | 11335 | `		/* Nothing to compile */` |
|     ! 0 | 11336 | `		return PH7_OK;` |
|       - | 11337 | `	}` |
|       - | 11338 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11339 | `	 * file's flags so include/require restore them on return. */` |
|   16503 | 11340 | `	pCodeGen = &pVm->sCodeGen;` |
|   16503 | 11341 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   16503 | 11342 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   16503 | 11343 | `	pCodeGen->bStrictTypes = 0;` |
|   16503 | 11344 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11345 | `	/* Initialize the tokens containers */` |
|   16503 | 11346 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16503 | 11347 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16503 | 11348 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   16503 | 11349 | `	is_expr = 0;` |
|   16503 | 11350 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11351 | `		SyToken sTmp;` |
|       - | 11352 | `		/* PHP only: -*/` |
|    3377 | 11353 | `		sTmp.nLine = 1;` |
|    3377 | 11354 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3377 | 11355 | `		sTmp.pUserData = 0;` |
|    3377 | 11356 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3377 | 11357 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3377 | 11358 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11359 | `			/* A simple PHP expression */` |
|     ! 0 | 11360 | `			is_expr = 1;` |
|     ! 0 | 11361 | `		}` |
|    1691 | 11362 | `	}else{` |
|       - | 11363 | `		/* Tokenize raw text */` |
|   13131 | 11364 | `		SySetAlloc(&aRawToken,32);` |
|   13131 | 11365 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11366 | `	}` |
|       - | 11367 | `	/* Process high-level tokens */` |
|   16503 | 11368 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   16503 | 11369 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   16503 | 11370 | `	rc = PH7_OK;` |
|   16503 | 11371 | `	if( is_expr ){` |
|       - | 11372 | `		/* Compile the expression */` |
|     ! 0 | 11373 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 11374 | `		goto cleanup;` |
|       - | 11375 | `	}` |
|   16503 | 11376 | `	nObjIdx = 0;` |
|       - | 11377 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 11378 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 11379 | `	 * preventing namespace bleeding across include()d files. */` |
|   16503 | 11380 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 11381 | `	/* Start the compilation process */` |
|   14818 | 11382 | `	for(;;){` |
|   43335 | 11383 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   16491 | 11384 | `			break; /* No more tokens to process */` |
|       - | 11385 | `		}` |
|   26849 | 11386 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 11387 | `			/* Compile the PHP chunk */` |
|   13711 | 11388 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   13711 | 11389 | `			if( rc == SXERR_ABORT ){` |
|      16 | 11390 | `				break;` |
|       - | 11391 | `			}` |
|   13699 | 11392 | `			continue;` |
|       - | 11393 | `		}` |
|       - | 11394 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13143 | 11395 | `		nRawObj = 0;` |
|   26323 | 11396 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 11397 | `			/* Consume the raw chunk without any processing */` |
|   13185 | 11398 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13185 | 11399 | `			if( pRawObj == 0 ){` |
|     ! 0 | 11400 | `				rc = SXERR_MEM;` |
|     ! 0 | 11401 | `				break;` |
|       - | 11402 | `			}` |
|       - | 11403 | `			/* Mark as constant and emit the load constant instruction */` |
|   13185 | 11404 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13185 | 11405 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13185 | 11406 | `			++nRawObj;` |
|   13185 | 11407 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 11408 | `		}` |
|   13143 | 11409 | `		if( nRawObj > 0 ){` |
|       - | 11410 | `			/* Emit the consume instruction */` |
|   13143 | 11411 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6569 | 11412 | `		}` |
|    8254 | 11413 | `	}` |
|    8249 | 11414 | `cleanup:` |
|   16503 | 11415 | `	SySetRelease(&aRawToken);` |
|   16503 | 11416 | `	SySetRelease(&aPhpToken);` |
|       - | 11417 | `	/* Restore outer file's strict_types scope */` |
|   16503 | 11418 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   16503 | 11419 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   16503 | 11420 | `	return rc;` |
|    8254 | 11421 |  |
|       - | 11422 | `/*` |
|       - | 11423 | ` * Utility routines.Initialize the code generator.` |
|       - | 11424 | ` */` |
|    3306 | 11425 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 11426 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11427 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11428 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11429 | `	)` |
|       5 | 11430 |  |
|    3311 | 11431 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11432 | `	/* Zero the structure */` |
|    3311 | 11433 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 11434 | `	/* Initial state */` |
|    3311 | 11435 | `	pGen->pVm  = &(*pVm);` |
|    3311 | 11436 | `	pGen->xErr = xErr;` |
|    3311 | 11437 | `	pGen->pErrData = pErrData;` |
|    3311 | 11438 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3311 | 11439 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3311 | 11440 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3311 | 11441 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3311 | 11442 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 11443 | `	/* Error log buffer */` |
|    3311 | 11444 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 11445 | `	/* General purpose working buffer */` |
|    3311 | 11446 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 11447 | `	/* Namespace state */` |
|    3311 | 11448 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3311 | 11449 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3311 | 11450 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3311 | 11451 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11452 | `	/* Create the global scope */` |
|    3311 | 11453 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 11454 | `	/* Point to the global scope */` |
|    3311 | 11455 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3311 | 11456 | `	return SXRET_OK;` |
|       5 | 11457 |  |
|       - | 11458 | `/*` |
|       - | 11459 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 11460 | ` */` |
|   19464 | 11461 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 11462 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11463 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11464 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11465 | `	)` |
|       5 | 11466 |  |
|   19469 | 11467 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11468 | `	GenBlock *pBlock,*pParent;` |
|       - | 11469 | `	/* Reset state */` |
|   19469 | 11470 | `	SySetReset(&pGen->aLabel);` |
|   19469 | 11471 | `	SySetReset(&pGen->aGoto);` |
|   19469 | 11472 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   19469 | 11473 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   19469 | 11474 | `	SyBlobRelease(&pGen->sWorker);` |
|   19469 | 11475 | `	SyBlobRelease(&pGen->sNamespace);` |
|   19469 | 11476 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   19469 | 11477 | `	SyHashRelease(&pGen->hUseImports);` |
|   19469 | 11478 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   19469 | 11479 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   19469 | 11480 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   19469 | 11481 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   19469 | 11482 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11483 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 11484 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 11485 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 11486 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 11487 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 11488 | `	 * number of unique names, which is acceptable. */` |
|       - | 11489 | `	/* Point to the global scope */` |
|   19469 | 11490 | `	pBlock = pGen->pCurrent;` |
|   19469 | 11491 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 11492 | `		pParent = pBlock->pParent;` |
|     ! 0 | 11493 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 11494 | `		pBlock = pParent;` |
|     ! 0 | 11495 | `	}` |
|   19469 | 11496 | `	pGen->xErr = xErr;` |
|   19469 | 11497 | `	pGen->pErrData = pErrData;` |
|   19469 | 11498 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   19469 | 11499 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   19469 | 11500 | `	pGen->pIn = pGen->pEnd = 0;` |
|   19469 | 11501 | `	pGen->nErr = 0;` |
|   19469 | 11502 | `	return SXRET_OK;` |
|       5 | 11503 |  |
|       - | 11504 | `/*` |
|       - | 11505 | ` * Generate a compile-time error message.` |
|       - | 11506 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 11507 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 11508 | ` * abort compilation immediately.` |
|       - | 11509 | ` */` |
|     602 | 11510 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 11511 |  |
|     607 | 11512 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     607 | 11513 | `	const char *zErr = "Error";` |
|       - | 11514 | `	SyString *pFile;` |
|       - | 11515 | `	va_list ap;` |
|       - | 11516 | `	sxi32 rc;` |
|       - | 11517 | `	/* Reset the working buffer */` |
|     607 | 11518 | `	SyBlobReset(pWorker);` |
|       - | 11519 | `	/* Peek the processed file path if available */` |
|     607 | 11520 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     607 | 11521 | `	if( nErrType == E_ERROR ){` |
|       - | 11522 | `		/* Increment the error counter */` |
|     501 | 11523 | `		pGen->nErr++;` |
|     501 | 11524 | `		if( pGen->nErr > 15 ){` |
|       - | 11525 | `			/* Error count limit reached */` |
|       6 | 11526 | `			if( pGen->xErr ){` |
|       6 | 11527 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       6 | 11528 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       6 | 11529 | `				if( pFile ){` |
|       6 | 11530 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 11531 | `				}` |
|       6 | 11532 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       6 | 11533 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       6 | 11534 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 11535 | `				}` |
|       2 | 11536 | `			}` |
|       - | 11537 | `			/* Abort immediately */` |
|       6 | 11538 | `			return SXERR_ABORT;` |
|       - | 11539 | `		}` |
|     246 | 11540 | `	}` |
|     603 | 11541 | `	if( pGen->xErr == 0 ){` |
|       - | 11542 | `		/* No available error consumer,return immediately */` |
|       3 | 11543 | `		return SXRET_OK;` |
|       - | 11544 | `	}` |
|     600 | 11545 | `	switch(nErrType){` |
|     494 | 11546 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 11547 | `	case E_WARNING: zErr = "Warning";     break;` |
|      76 | 11548 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      12 | 11549 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 11550 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 11551 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 11552 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 11553 | `	default:` |
|     ! 0 | 11554 | `		break;` |
|       - | 11555 | `	}` |
|     600 | 11556 | `	rc = SXRET_OK;` |
|       - | 11557 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     600 | 11558 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     600 | 11559 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     600 | 11560 | `	va_start(ap,zFormat);` |
|     600 | 11561 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     600 | 11562 | `	va_end(ap);` |
|     600 | 11563 | `	if( pFile ){` |
|     600 | 11564 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     298 | 11565 | `	}` |
|       - | 11566 | `	/* Append a new line */` |
|     600 | 11567 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     600 | 11568 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 11569 | `		/* Consume the generated error message */` |
|     600 | 11570 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     298 | 11571 | `	}` |
|     600 | 11572 | `	return rc;` |
|     306 | 11573 |  |
|       - | 11574 |  |
