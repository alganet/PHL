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
|  743374 |   155 | `static void GenStateInitBlock(` |
|       - |   156 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   157 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   158 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   159 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   160 | `	void *pUserData      /* Upper layer private data */` |
|       - |   161 | `	)` |
|       5 |   162 |  |
|       - |   163 | `	/* Initialize block fields */` |
|  743379 |   164 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  743379 |   165 | `	pBlock->pUserData   = pUserData;` |
|  743379 |   166 | `	pBlock->pGen        = pGen;` |
|  743379 |   167 | `	pBlock->iFlags      = iType;` |
|  743379 |   168 | `	pBlock->pParent     = 0;` |
|  743379 |   169 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  743379 |   170 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  743379 |   171 |  |
|       - |   172 | `/*` |
|       - |   173 | ` * Allocate a new block instance.` |
|       - |   174 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   175 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   176 | ` * processing on failure.` |
|       - |   177 | ` */` |
|  740226 |   178 | `static sxi32 GenStateEnterBlock(` |
|       - |   179 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   180 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   181 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   182 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   183 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   184 | `	)` |
|       5 |   185 |  |
|       - |   186 | `	GenBlock *pBlock;` |
|       - |   187 | `	/* Allocate a new block instance */` |
|  740231 |   188 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  740231 |   189 | `	if( pBlock == 0 ){` |
|       - |   190 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   191 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   192 | `		 */` |
|     ! 0 |   193 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   194 | `		/* Abort processing immediately */` |
|     ! 0 |   195 | `		return SXERR_ABORT;` |
|       - |   196 | `	}` |
|       - |   197 | `	/* Zero the structure */` |
|  740231 |   198 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  740231 |   199 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   200 | `	/* Link to the parent block */` |
|  740231 |   201 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   202 | `	/* Mark as the current block */` |
|  740231 |   203 | `	pGen->pCurrent = pBlock;` |
|  740231 |   204 | `	if( ppBlock ){` |
|       - |   205 | `		/* Write a pointer to the new instance */` |
|  359547 |   206 | `		*ppBlock = pBlock;` |
|  179771 |   207 | `	}` |
|  740231 |   208 | `	return SXRET_OK;` |
|  370118 |   209 |  |
|       - |   210 | `/*` |
|       - |   211 | ` * Release block fields without freeing the whole instance.` |
|       - |   212 | ` */` |
|  740218 |   213 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   214 |  |
|  740223 |   215 | `	SySetRelease(&pBlock->aPostContFix);` |
|  740223 |   216 | `	SySetRelease(&pBlock->aJumpFix);` |
|  740223 |   217 |  |
|       - |   218 | `/*` |
|       - |   219 | ` * Release a block.` |
|       - |   220 | ` */` |
|  740218 |   221 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   222 |  |
|  740223 |   223 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  740223 |   224 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   225 | `	/* Free the instance */` |
|  740223 |   226 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  740223 |   227 |  |
|       - |   228 | `/*` |
|       - |   229 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   230 | ` */` |
|  740218 |   231 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   232 |  |
|  740223 |   233 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  740223 |   234 | `	if( pBlock == 0 ){` |
|       - |   235 | `		/* No more block to pop */` |
|     ! 0 |   236 | `		return SXERR_EMPTY;` |
|       - |   237 | `	}` |
|       - |   238 | `	/* Point to the upper block */` |
|  740223 |   239 | `	pGen->pCurrent = pBlock->pParent;` |
|  740223 |   240 | `	if( ppBlock ){` |
|       - |   241 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   242 | `		*ppBlock = pBlock;` |
|     ! 0 |   243 | `	}else{` |
|       - |   244 | `		/* Safely release the block */` |
|  740223 |   245 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   246 | `	}` |
|  740223 |   247 | `	return SXRET_OK;` |
|  370114 |   248 |  |
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
|  210194 |   259 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   260 |  |
|       - |   261 | `	JumpFixup sJumpFix;` |
|       - |   262 | `	sxi32 rc;` |
|       - |   263 | `	/* Init the JumpFixup structure */` |
|  210199 |   264 | `	sJumpFix.nJumpType = nJumpType;` |
|  210199 |   265 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   266 | `	/* Insert in the jump fixup table */` |
|  210199 |   267 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  210199 |   268 | `	return rc;` |
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
|  517770 |   281 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   282 |  |
|       - |   283 | `	JumpFixup *aFix;` |
|       - |   284 | `	VmInstr *pInstr;` |
|       - |   285 | `	sxu32 nFixed;` |
|       - |   286 | `	sxu32 n;` |
|       - |   287 | `	/* Point to the jump fixup table */` |
|  517775 |   288 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   289 | `	/* Fix the desired jumps */` |
|  931881 |   290 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  414111 |   291 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   292 | `			/* Already fixed */` |
|  165335 |   293 | `			continue;` |
|       - |   294 | `		}` |
|  248781 |   295 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   296 | `			/* Not of our interest */` |
|   38589 |   297 | `			continue;` |
|       - |   298 | `		}` |
|       - |   299 | `		/* Point to the instruction to fix */` |
|  210197 |   300 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  210197 |   301 | `		if( pInstr ){` |
|  210197 |   302 | `			pInstr->iP2 = nJumpDest;` |
|  210197 |   303 | `			nFixed++;` |
|       - |   304 | `			/* Mark as fixed */` |
|  210197 |   305 | `			aFix[n].nJumpType = -1;` |
|  105096 |   306 | `		}` |
|  105101 |   307 | `	}` |
|       - |   308 | `	/* Total number of fixed jumps */` |
|  517775 |   309 | `	return nFixed;` |
|       5 |   310 |  |
|       - |   311 | `/*` |
|       - |   312 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   313 | ` * The goto statement can be used to jump to another section` |
|       - |   314 | ` * in the program.` |
|       - |   315 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   316 | ` * statement for more information.` |
|       - |   317 | ` */` |
|  210418 |   318 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   319 |  |
|       - |   320 | `	JumpFixup *pJump,*aJumps;` |
|       - |   321 | `	Label *pLabel,*aLabel;` |
|       - |   322 | `	VmInstr *pInstr;` |
|       - |   323 | `	sxi32 rc;` |
|       - |   324 | `	sxu32 n;` |
|       - |   325 | `	/* Point to the goto table */` |
|  210423 |   326 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   327 | `	/* Fix */` |
|  210569 |   328 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  210421 |   353 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  210553 |   354 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   355 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   356 | `			/* Emit a warning */` |
|      40 |   357 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   358 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   359 | `		}` |
|      71 |   360 | `	}` |
|  210421 |   361 | `	return SXRET_OK;` |
|  105214 |   362 |  |
|       - |   363 | `/*` |
|       - |   364 | ` * Check if a given token value is installed in the literal table.` |
|       - |   365 | ` */` |
|  665052 |   366 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   367 |  |
|       - |   368 | `	SyHashEntry *pEntry;` |
|  665057 |   369 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  665057 |   370 | `	if( pEntry == 0 ){` |
|  288977 |   371 | `		return SXERR_NOTFOUND;` |
|       - |   372 | `	}` |
|  376085 |   373 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  376085 |   374 | `	return SXRET_OK;` |
|  332531 |   375 |  |
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
|  288972 |   386 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   387 |  |
|  288977 |   388 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  288977 |   389 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  144486 |   390 | `	}` |
|  288977 |   391 | `	return SXRET_OK;` |
|       5 |   392 |  |
|       - |   393 | `/*` |
|       - |   394 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   395 | ` * in the constant table.` |
|       - |   396 | ` */` |
|  110912 |   397 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   398 |  |
|       - |   399 | `	ph7_value *pObj;` |
|  110917 |   400 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   401 | `	/* Reserve a new constant */` |
|  110917 |   402 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  110917 |   403 | `	if( pObj == 0 ){` |
|     ! 0 |   404 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   405 | `		return 0;` |
|       - |   406 | `	}` |
|  110917 |   407 | `	*pIdx = nIdx;` |
|       - |   408 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   409 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   410 | `	 */` |
|  110917 |   411 | `	return pObj;` |
|   55461 |   412 |  |
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
|  397828 |   427 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   428 |  |
|       - |   429 | `	VmCallArgMap *pMap;` |
|  397833 |   430 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   431 | `	if( p3 == 0 ){` |
|      31 |   432 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   433 | `		if( pMap == 0 ) return 0;` |
|      31 |   434 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   435 | `		p3 = (void *)pMap;` |
|      14 |   436 | `	}` |
|      33 |   437 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   438 | `	return p3;` |
|  198919 |   439 |  |
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
|  111502 |   498 | `static int GenStateFindBadNumericSeparator(` |
|       - |   499 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   500 |  |
|  111507 |   501 | `	const char *z = pRaw->zString;` |
|  111507 |   502 | `	sxu32 n = pRaw->nByte;` |
|  111507 |   503 | `	int base = 10;` |
|       - |   504 | `	sxu32 i, start;` |
|  111507 |   505 | `	if( n < 2 ) return 0;` |
|    9389 |   506 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   507 | `		base = 16;` |
|    9354 |   508 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   509 | `		base = 2;` |
|     139 |   510 | `	}` |
|   34307 |   511 | `	for( i = 0; i < n; ++i ){` |
|   24937 |   512 | `		if( z[i] != '_' ) continue;` |
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
|    9375 |   529 | `	return 0;` |
|   55756 |   530 |  |
|       - |   531 | `/*` |
|       - |   532 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   533 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   534 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   535 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   536 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   537 | ` * so callers can bail from the current construct).` |
|       - |   538 | ` */` |
|  111502 |   539 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   540 |  |
|  111507 |   541 | `	const char *zBad = 0;` |
|  111507 |   542 | `	sxu32 nBad = 0;` |
|       - |   543 | `	SyString sBad;` |
|       - |   544 | `	sxi32 rc;` |
|  111507 |   545 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  111493 |   546 | `		return SXRET_OK;` |
|       - |   547 | `	}` |
|      18 |   548 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   549 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   550 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   551 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   552 | `		return SXERR_ABORT;` |
|       - |   553 | `	}` |
|      18 |   554 | `	return SXERR_SYNTAX;` |
|   55756 |   555 |  |
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
|  111488 |   572 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   573 | `	SyMemBackend *pAlloc,` |
|       - |   574 | `	const SyString *pToken,` |
|       - |   575 | `	char *zScratch, sxu32 nScratch,` |
|       - |   576 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   577 |  |
|       - |   578 | `	sxu32 i, j;` |
|  111493 |   579 | `	int hasUnderscore = 0;` |
|       - |   580 | `	char *zBuf;` |
|  111493 |   581 | `	*pzAlloc = 0;` |
|  236463 |   582 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  125227 |   583 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   62490 |   584 | `	}` |
|  111493 |   585 | `	if( !hasUnderscore ){` |
|  111241 |   586 | `		SyStringDupPtr(pOut, pToken);` |
|  111241 |   587 | `		return SXRET_OK;` |
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
|   55749 |   604 |  |
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
|  111474 |   621 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   622 |  |
|  111479 |   623 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  111479 |   624 | `	sxu32 nIdx = 0;` |
|       - |   625 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  111479 |   626 | `	char *zAlloc = 0;` |
|       - |   627 | `	SyString sNum;` |
|       - |   628 | `	sxi32 rc;` |
|   55737 |   629 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  111479 |   630 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  111479 |   631 | `	if( rc != SXRET_OK ){` |
|      14 |   632 | `		return rc;` |
|       - |   633 | `	}` |
|  167201 |   634 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   55732 |   635 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  111469 |   636 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   637 | `		return SXERR_ABORT;` |
|       - |   638 | `	}` |
|  111469 |   639 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   640 | `		ph7_value *pObj;` |
|       - |   641 | `		sxi64 iValue;` |
|  110917 |   642 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  110917 |   643 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  110917 |   644 | `		if( pObj == 0 ){` |
|     ! 0 |   645 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   646 | `			return SXERR_ABORT;` |
|       - |   647 | `		}` |
|  110917 |   648 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   55461 |   649 | `	}else{` |
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
|  111469 |   662 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   663 | `	/* Emit the load constant instruction */` |
|  111469 |   664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   665 | `	/* Node successfully compiled */` |
|  111469 |   666 | `	return SXRET_OK;` |
|   55742 |   667 |  |
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
|   22240 |   950 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   951 |  |
|       - |   952 | `	ph7_value *pConstObj;` |
|   22245 |   953 | `	sxu32 nIdx = 0;` |
|       - |   954 | `	/* Reserve a new constant */` |
|   22245 |   955 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   22245 |   956 | `	if( pConstObj == 0 ){` |
|     ! 0 |   957 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   958 | `		return 0;` |
|       - |   959 | `	}` |
|   22245 |   960 | `	(*pCount)++;` |
|   22245 |   961 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   962 | `	/* Emit the load constant instruction */` |
|   22245 |   963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   22245 |   964 | `	return pConstObj;` |
|   11125 |   965 |  |
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
|   20794 |  1004 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1005 |  |
|   20799 |  1006 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1007 | `	const char *zIn,*zCur,*zEnd;` |
|   20799 |  1008 | `	ph7_value *pObj = 0;` |
|       - |  1009 | `	sxi32 iCons;` |
|       - |  1010 | `	sxi32 rc;` |
|       - |  1011 | `	/* Delimit the string */` |
|   20799 |  1012 | `	zIn  = pStr->zString;` |
|   20799 |  1013 | `	zEnd = &zIn[pStr->nByte];` |
|   20799 |  1014 | `	if( zIn >= zEnd ){` |
|       - |  1015 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1016 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1017 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1018 | `		 */` |
|     277 |  1019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     277 |  1020 | `		return SXRET_OK;` |
|       - |  1021 | `	}` |
|   20527 |  1022 | `	zCur = 0;` |
|       - |  1023 | `	/* Compile the node */` |
|   20527 |  1024 | `	iCons = 0;` |
|   11290 |  1025 | `	for(;;){` |
|   33981 |  1026 | `		zCur = zIn;` |
|  165819 |  1027 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  133901 |  1028 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1029 | `				break;` |
|  133775 |  1030 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1934 |  1031 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     970 |  1032 | `					break;` |
|       - |  1033 | `			}` |
|  131843 |  1034 | `			zIn++;` |
|       5 |  1035 | `		}` |
|   33981 |  1036 | `		if( zIn > zCur ){` |
|   15705 |  1037 | `			if( pObj == 0 ){` |
|   15325 |  1038 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   15325 |  1039 | `				if( pObj == 0 ){` |
|     ! 0 |  1040 | `					return SXERR_ABORT;` |
|       - |  1041 | `				}` |
|    7660 |  1042 | `			}` |
|   15705 |  1043 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    7850 |  1044 | `		}` |
|   33981 |  1045 | `		if( zIn >= zEnd ){` |
|   20527 |  1046 | `			break;` |
|       - |  1047 | `		}` |
|   13459 |  1048 | `		if( zIn[0] == '\\' ){` |
|   11401 |  1049 | `			const char *zPtr = 0;` |
|       - |  1050 | `			sxu32 n;` |
|   11401 |  1051 | `			zIn++;` |
|   11401 |  1052 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1053 | `				break;` |
|       - |  1054 | `			}` |
|   11401 |  1055 | `			if( pObj == 0 ){` |
|    6925 |  1056 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    6925 |  1057 | `				if( pObj == 0 ){` |
|     ! 0 |  1058 | `					return SXERR_ABORT;` |
|       - |  1059 | `				}` |
|    3460 |  1060 | `			}` |
|   11401 |  1061 | `			n = sizeof(char); /* size of conversion */` |
|   11401 |  1062 | `			switch( zIn[0] ){` |
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
|    5272 |  1083 | `			case 'n':` |
|       - |  1084 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   10549 |  1085 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   10549 |  1086 | `				break;` |
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
|   11401 |  1154 | `			zIn += n;` |
|   11401 |  1155 | `			continue;` |
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
|   20527 |  1273 | `	if( iCons > 1 ){` |
|       - |  1274 | `		/* Concatenate all compiled constants */` |
|    1535 |  1275 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     765 |  1276 | `	}` |
|       - |  1277 | `	/* Node successfully compiled */` |
|   20527 |  1278 | `	return SXRET_OK;` |
|   10402 |  1279 |  |
|       - |  1280 | `/*` |
|       - |  1281 | ` * Compile a double quoted string.` |
|       - |  1282 | ` *  See the block-comment above for more information.` |
|       - |  1283 | ` */` |
|   20734 |  1284 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1285 |  |
|       - |  1286 | `	sxi32 rc;` |
|   20739 |  1287 | `	rc = GenStateCompileString(&(*pGen));` |
|   10367 |  1288 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1289 | `	/* Compilation result */` |
|   20739 |  1290 | `	return rc;` |
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
|   19094 |  1334 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   19099 |  1345 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1346 | `	/* Compile the expression*/` |
|   19099 |  1347 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1348 | `	/* Restore token stream */` |
|   19099 |  1349 | `	RE_SWAP_DELIMITER(pGen);` |
|   19099 |  1350 | `	return rc;` |
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
|   27726 |  1389 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1390 |  |
|       - |  1391 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1392 | `	SyToken *pKey,*pCur;` |
|   27731 |  1393 | `	sxi32 iEmitRef = 0;` |
|   27731 |  1394 | `	sxi32 iSpread = 0;` |
|   27731 |  1395 | `	sxi32 nPair = 0;` |
|       - |  1396 | `	sxi32 iNest;` |
|       - |  1397 | `	sxi32 rc;` |
|   27731 |  1398 | `	xValidator = 0;` |
|   22621 |  1399 | `	for(;;){` |
|       - |  1400 | `		/* Jump leading commas */` |
|   51199 |  1401 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5957 |  1402 | `			pGen->pIn++;` |
|       5 |  1403 | `		}` |
|   45247 |  1404 | `		pCur = pGen->pIn;` |
|   45247 |  1405 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1406 | `			/* No more entry to process */` |
|   27715 |  1407 | `			break;` |
|       - |  1408 | `		}` |
|   17537 |  1409 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1410 | `			continue;` |
|       - |  1411 | `		}` |
|       - |  1412 | `		/* Compile the key if available */` |
|   17537 |  1413 | `		pKey = pCur;` |
|   17537 |  1414 | `		iNest = 0;` |
|   49019 |  1415 | `		while( pCur < pGen->pIn ){` |
|   32951 |  1416 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1465 |  1417 | `				break;` |
|       - |  1418 | `			}` |
|       - |  1419 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1420 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1421 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1422 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1423 | `			 */` |
|   31491 |  1424 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   31485 |  1488 | `			if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     265 |  1489 | `				iNest++;` |
|   31354 |  1490 | `			}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1491 | `				/* Don't worry about mismatched brackets here,the expression` |
|       - |  1492 | `				 * parser will shortly detect any syntax error.` |
|       - |  1493 | `				 */` |
|     265 |  1494 | `				iNest--;` |
|     131 |  1495 | `			}` |
|   31485 |  1496 | `			pCur++;` |
|       5 |  1497 | `		}` |
|   17537 |  1498 | `		rc = SXERR_EMPTY;` |
|   17537 |  1499 | `		if( pCur < pGen->pIn ){` |
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
|   16802 |  1515 | `		}else if( pKey == pCur ){` |
|       - |  1516 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1517 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1518 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1519 | `		}else{` |
|       - |  1520 | `			/* Reset back the cursor and point to the entry value */` |
|   16077 |  1521 | `			pCur = pKey;` |
|       - |  1522 | `		}` |
|   17527 |  1523 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1524 | `			/* No available key,load NULL */` |
|   16079 |  1525 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    8037 |  1526 | `		}` |
|   17527 |  1527 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   17525 |  1546 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   17525 |  1547 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   17521 |  1560 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   17521 |  1561 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1562 | `			return SXERR_ABORT;` |
|       - |  1563 | `		}` |
|   17521 |  1564 | `		if( iSpread ){` |
|       - |  1565 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1566 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   17490 |  1567 | `		}else if( iEmitRef ){` |
|       - |  1568 | `			/* Emit the load reference instruction */` |
|      40 |  1569 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1570 | `		}` |
|   17521 |  1571 | `		xValidator = 0;` |
|   17521 |  1572 | `		iEmitRef = 0;` |
|   17521 |  1573 | `		iSpread = 0;` |
|   17521 |  1574 | `		nPair++;` |
|       5 |  1575 | `	}` |
|       - |  1576 | `	/* Emit the load map instruction */` |
|   27715 |  1577 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1578 | `	/* Node successfully compiled */` |
|   27715 |  1579 | `	return SXRET_OK;` |
|   13868 |  1580 |  |
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
|     738 |  1603 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1604 |  |
|       - |  1605 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     743 |  1606 | `	pGen->pIn++;` |
|     743 |  1607 | `	pGen->pEnd--;` |
|     369 |  1608 | `	SXUNUSED(iCompileFlag);` |
|     743 |  1609 | `	return GenStateCompileArrayBody(pGen);` |
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
|  987126 |  2737 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2738 |  |
|  987131 |  2739 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2740 | `	sxi32 iVv;` |
|       - |  2741 | `	sxi32 iP1;` |
|       - |  2742 | `	void *p3;` |
|       - |  2743 | `	sxi32 rc;` |
|  987131 |  2744 | `	iVv = -1; /* Variable variable counter */` |
| 1974269 |  2745 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  987143 |  2746 | `		pGen->pIn++;` |
|  987143 |  2747 | `		iVv++;` |
|       5 |  2748 | `	}` |
|  987131 |  2749 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2750 | `		/* Invalid variable name */` |
|     ! 0 |  2751 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2752 | `		if( rc == SXERR_ABORT ){` |
|       - |  2753 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2754 | `			return SXERR_ABORT;` |
|       - |  2755 | `		}` |
|     ! 0 |  2756 | `		return SXRET_OK;` |
|       - |  2757 | `	}` |
|  987131 |  2758 | `	p3  = 0;` |
|  987131 |  2759 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  987115 |  2779 | `		char *zName = 0;` |
|       - |  2780 | `		/* Extract variable name */` |
|  987115 |  2781 | `		pName = &pGen->pIn->sData;` |
|       - |  2782 | `		/* Advance the stream cursor */` |
|  987115 |  2783 | `		pGen->pIn++;` |
|  987115 |  2784 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  987115 |  2785 | `		if( pEntry == 0 ){` |
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
|  854703 |  2796 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2797 | `		}` |
|  987115 |  2798 | `		p3 = (void *)zName;` |
|       - |  2799 | `	}` |
|  987127 |  2800 | `	iP1 = 0;` |
|  987127 |  2801 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  359745 |  2802 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2803 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  359727 |  2804 | `			iP1 = 1;` |
|  179861 |  2805 | `		}` |
|  179870 |  2806 | `	}` |
|       - |  2807 | `	/* Emit the load instruction */` |
|  987127 |  2808 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  987139 |  2809 | `	while( iVv > 0 ){` |
|      13 |  2810 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2811 | `		iVv--;` |
|       1 |  2812 | `	}` |
|       - |  2813 | `	/* Node successfully compiled */` |
|  987127 |  2814 | `	return SXRET_OK;` |
|  493568 |  2815 |  |
|       - |  2816 | `/*` |
|       - |  2817 | ` * Load a literal.` |
|       - |  2818 | ` */` |
|  693982 |  2819 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2820 |  |
|  693987 |  2821 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2822 | `	ph7_value *pObj;` |
|       - |  2823 | `	SyString *pStr;` |
|       - |  2824 | `	sxu32 nIdx;` |
|       - |  2825 | `	/* Extract token value */` |
|  693987 |  2826 | `	pStr = &pToken->sData;` |
|       - |  2827 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  693987 |  2828 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  147089 |  2829 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2830 | `			/* NULL constant are always indexed at 0 */` |
|   54199 |  2831 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   54199 |  2832 | `			return SXRET_OK;` |
|   92895 |  2833 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2834 | `			/* TRUE constant are always indexed at 1 */` |
|     595 |  2835 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     595 |  2836 | `			return SXRET_OK;` |
|       5 |  2837 | `		}` |
|  648043 |  2838 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  109980 |  2839 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2840 | `			/* FALSE constant are always indexed at 2 */` |
|   41561 |  2841 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   41561 |  2842 | `			return SXRET_OK;` |
|  554658 |  2843 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   98622 |  2844 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
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
|  511829 |  2855 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   31864 |  2856 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  510963 |  2872 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   13347 |  2873 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  504285 |  2874 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   16812 |  2875 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  588181 |  2905 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2906 | `		ph7_value *pLitObj;` |
|       - |  2907 | `		/* Unknown literal,install it in the literal table */` |
|  244171 |  2908 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  244171 |  2909 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2910 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2911 | `			return SXERR_ABORT;` |
|       - |  2912 | `		}` |
|  244171 |  2913 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  244171 |  2914 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  122083 |  2915 | `	}` |
|       - |  2916 | `	/* Emit the load constant instruction */` |
|  588181 |  2917 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  588181 |  2918 | `	return SXRET_OK;` |
|  346996 |  2919 |  |
|       - |  2920 | `/*` |
|       - |  2921 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2922 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2923 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2924 | ` * Otherwise, load the simple literal directly.` |
|       - |  2925 | ` */` |
|  694018 |  2926 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  2927 |  |
|       - |  2928 | `	sxi32 rc;` |
|  694023 |  2929 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2930 | `		return SXRET_OK;` |
|       - |  2931 | `	}` |
|       - |  2932 | `	/* Check if this is a multi-token namespace path */` |
|  694023 |  2933 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
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
|  693987 |  2986 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  693987 |  2987 | `	return rc;` |
|  347014 |  2988 |  |
|       - |  2989 | `/*` |
|       - |  2990 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2991 | ` */` |
|  694018 |  2992 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2993 |  |
|       - |  2994 | `	sxi32 rc;` |
|  694023 |  2995 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  694023 |  2996 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2997 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2998 | `		return rc;` |
|       - |  2999 | `	}` |
|       - |  3000 | `	/* Node successfully compiled */` |
|  694023 |  3001 | `	return SXRET_OK;` |
|  347014 |  3002 |  |
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
|  382170 |  3534 | `static sxi32 PH7_CompileBlock(` |
|       - |  3535 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3536 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3537 | `	)` |
|       5 |  3538 |  |
|       - |  3539 | `	sxi32 rc;` |
|       - |  3540 | `	sxu32 nLine;` |
|  382175 |  3541 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  380689 |  3542 | `		nLine = pGen->pIn->nLine;` |
|  380689 |  3543 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  380689 |  3544 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3545 | `			return SXERR_ABORT;` |
|       - |  3546 | `		}` |
|  380689 |  3547 | `		pGen->pIn++;` |
|       - |  3548 | `		/* Compile until we hit the closing braces '}' */` |
|  519916 |  3549 | `		for(;;){` |
| 1039837 |  3550 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
| 1039817 |  3561 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3562 | `				/* Closing braces found,break immediately*/` |
|  380669 |  3563 | `				pGen->pIn++;` |
|  380669 |  3564 | `				break;` |
|       - |  3565 | `			}` |
|       - |  3566 | `			/* Compile a single statement */` |
|  659153 |  3567 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  659153 |  3568 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3569 | `				return SXERR_ABORT;` |
|       - |  3570 | `			}` |
|       5 |  3571 | `		}` |
|  380689 |  3572 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  191833 |  3573 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|  382175 |  3623 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3624 | `		pGen->pIn++;` |
|     ! 0 |  3625 | `	}` |
|  382175 |  3626 | `	return SXRET_OK;` |
|  191090 |  3627 |  |
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
|  208868 |  4610 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4611 |  |
|  208873 |  4612 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4613 | `	sxi32 rc;` |
|       - |  4614 | `	/* Jump the 'return' keyword */` |
|  208873 |  4615 | `	pGen->pIn++;` |
|  208873 |  4616 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4617 | `		/* Compile the expression */` |
|  208847 |  4618 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  208847 |  4619 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4620 | `			return SXERR_ABORT;` |
|  208847 |  4621 | `		}else if(rc != SXERR_EMPTY ){` |
|  208847 |  4622 | `			nRet = 1;` |
|  104421 |  4623 | `		}` |
|  104421 |  4624 | `	}` |
|       - |  4625 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4626 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4627 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4628 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4629 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  208873 |  4630 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  208873 |  4631 | `	return SXRET_OK;` |
|  104439 |  4632 |  |
|       - |  4633 | `/*` |
|       - |  4634 | ` * Compile a yield expression.` |
|       - |  4635 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4636 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4637 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4638 | ` */` |
|      72 |  4639 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4640 |  |
|       - |  4641 | `	SyToken *pTmp, *pSplit;` |
|      77 |  4642 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      77 |  4643 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4644 | `	sxi32 rc;` |
|      36 |  4645 | `	(void)iCompileFlag;` |
|       - |  4646 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      77 |  4647 | `	pGen->pIn++;` |
|       - |  4648 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4649 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      77 |  4650 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4651 | `		/* Bare yield — no value */` |
|     ! 0 |  4652 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4653 | `		return SXRET_OK;` |
|       - |  4654 | `	}` |
|       - |  4655 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      77 |  4656 | `	pSplit = 0;` |
|       - |  4657 | `	{` |
|      77 |  4658 | `		SyToken *pCur = pGen->pIn;` |
|      77 |  4659 | `		sxi32 nNest = 0;` |
|     163 |  4660 | `		while( pCur < pGen->pEnd ){` |
|     105 |  4661 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4662 | `				nNest++;` |
|     105 |  4663 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4664 | `				nNest--;` |
|     105 |  4665 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4666 | `				pSplit = pCur;` |
|      16 |  4667 | `				break;` |
|       - |  4668 | `			}` |
|      91 |  4669 | `			pCur++;` |
|       5 |  4670 | `		}` |
|       - |  4671 | `	}` |
|      77 |  4672 | `	pTmp = pGen->pEnd;` |
|      77 |  4673 | `	if( pSplit ){` |
|       - |  4674 | `		/* yield $key => $value */` |
|      16 |  4675 | `		pGen->pEnd = pSplit;` |
|      16 |  4676 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4677 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4678 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4679 | `		pGen->pEnd = pTmp;` |
|      16 |  4680 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4681 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4682 | `		iP1 = 1;` |
|      16 |  4683 | `		iP2 = 1;` |
|       9 |  4684 | `	}else{` |
|       - |  4685 | `		/* yield $value */` |
|      63 |  4686 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      63 |  4687 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      63 |  4688 | `		if( rc != SXERR_EMPTY ){` |
|      63 |  4689 | `			iP1 = 1;` |
|      29 |  4690 | `		}` |
|       - |  4691 | `	}` |
|      77 |  4692 | `	pGen->pEnd = pTmp;` |
|      77 |  4693 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      77 |  4694 | `	return SXRET_OK;` |
|      41 |  4695 |  |
|       - |  4696 | `/*` |
|       - |  4697 | ` * Compile the die/exit language construct.` |
|       - |  4698 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4699 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4700 | ` */` |
|     120 |  4701 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4702 |  |
|     125 |  4703 | `	sxi32 nExpr = 0;` |
|       - |  4704 | `	sxi32 rc;` |
|       - |  4705 | `	/* Jump the die/exit keyword */` |
|     125 |  4706 | `	pGen->pIn++;` |
|     125 |  4707 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4708 | `		/* Compile the expression */` |
|     125 |  4709 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4710 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4711 | `			return SXERR_ABORT;` |
|     125 |  4712 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4713 | `			nExpr = 1;` |
|      60 |  4714 | `		}` |
|      60 |  4715 | `	}` |
|       - |  4716 | `	/* Emit the HALT instruction */` |
|     125 |  4717 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4718 | `	return SXRET_OK;` |
|      65 |  4719 |  |
|       - |  4720 | `/*` |
|       - |  4721 | ` * Compile the 'echo' language construct.` |
|       - |  4722 | ` */` |
|   13398 |  4723 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4724 |  |
|   13403 |  4725 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4726 | `	sxi32 rc;` |
|       - |  4727 | `	/* Jump the 'echo' keyword */` |
|   13403 |  4728 | `	pGen->pIn++;` |
|       - |  4729 | `	/* Compile arguments one after one */` |
|   13403 |  4730 | `	pTmp = pGen->pEnd;` |
|   28969 |  4731 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   15571 |  4732 | `		if( pGen->pIn < pNext ){` |
|   15571 |  4733 | `			pGen->pEnd = pNext;` |
|   15571 |  4734 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   15571 |  4735 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4736 | `				return SXERR_ABORT;` |
|   15571 |  4737 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4738 | `				/* Emit the consume instruction */` |
|   15547 |  4739 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    7771 |  4740 | `			}` |
|    7783 |  4741 | `		}` |
|       - |  4742 | `		/* Jump trailing commas */` |
|   17739 |  4743 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    2173 |  4744 | `			pNext++;` |
|       5 |  4745 | `		}` |
|   15571 |  4746 | `		pGen->pIn = pNext;` |
|       5 |  4747 | `	}` |
|       - |  4748 | `	/* Restore token stream */` |
|   13403 |  4749 | `	pGen->pEnd = pTmp;` |
|   13403 |  4750 | `	return SXRET_OK;` |
|    6704 |  4751 |  |
|       - |  4752 | `/*` |
|       - |  4753 | ` * Compile the static statement.` |
|       - |  4754 | ` * According to the PHP language reference` |
|       - |  4755 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4756 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4757 | ` *  when program execution leaves this scope.` |
|       - |  4758 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4759 | ` * Symisc eXtension.` |
|       - |  4760 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4761 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4762 | ` *  Example` |
|       - |  4763 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4764 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4765 | ` */` |
|       6 |  4766 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       2 |  4767 |  |
|       - |  4768 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4769 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4770 | `	GenBlock *pBlock;` |
|       - |  4771 | `	SyString *pName;` |
|       - |  4772 | `	char *zDup;` |
|       - |  4773 | `	sxu32 nLine;` |
|       - |  4774 | `	sxi32 rc;` |
|       - |  4775 | `	/* Jump the static keyword */` |
|       8 |  4776 | `	nLine = pGen->pIn->nLine;` |
|       8 |  4777 | `	pGen->pIn++;` |
|       - |  4778 | `	/* Extract the enclosing function if any */` |
|       8 |  4779 | `	pBlock = pGen->pCurrent;` |
|      14 |  4780 | `	while( pBlock ){` |
|      14 |  4781 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       8 |  4782 | `			break;` |
|       - |  4783 | `		}` |
|       - |  4784 | `		/* Point to the upper block */` |
|       8 |  4785 | `		pBlock = pBlock->pParent;` |
|       2 |  4786 | `	}` |
|       8 |  4787 | `	if( pBlock == 0 ){` |
|       - |  4788 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4789 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4790 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4791 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4792 | `				return SXERR_ABORT;` |
|       - |  4793 | `			}` |
|     ! 0 |  4794 | `			goto Synchronize;` |
|       - |  4795 | `		}` |
|       - |  4796 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4797 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4798 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4799 | `			return SXERR_ABORT;` |
|     ! 0 |  4800 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4801 | `			/* Emit the POP instruction */` |
|     ! 0 |  4802 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4803 | `		}` |
|     ! 0 |  4804 | `		return SXRET_OK;` |
|       - |  4805 | `	}` |
|       8 |  4806 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4807 | `	/* Make sure we are dealing with a valid statement */` |
|       8 |  4808 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       4 |  4809 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4810 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4811 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4812 | `				return SXERR_ABORT;` |
|       - |  4813 | `			}` |
|       3 |  4814 | `			goto Synchronize;` |
|       - |  4815 | `	}` |
|       5 |  4816 | `	pGen->pIn++;` |
|       - |  4817 | `	/* Extract variable name */` |
|       5 |  4818 | `	pName = &pGen->pIn->sData;` |
|       5 |  4819 | `	pGen->pIn++; /* Jump the var name */` |
|       5 |  4820 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4821 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4822 | `		goto Synchronize;` |
|       - |  4823 | `	}` |
|       - |  4824 | `	/* Initialize the structure describing the static variable */` |
|       5 |  4825 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       5 |  4826 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4827 | `	/* Duplicate variable name */` |
|       5 |  4828 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       5 |  4829 | `	if( zDup == 0 ){` |
|     ! 0 |  4830 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4831 | `		return SXERR_ABORT;` |
|       - |  4832 | `	}` |
|       5 |  4833 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4834 | `	/* Check if we have an expression to compile */` |
|       5 |  4835 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4836 | `		SySet *pInstrContainer;` |
|       - |  4837 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4838 | `		 * Static variable can take any complex expression including function` |
|       - |  4839 | `		 * call as their initialization value.` |
|       - |  4840 | `		 * Example:` |
|       - |  4841 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4842 | `		 */` |
|       5 |  4843 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4844 | `		/* Swap bytecode container */` |
|       5 |  4845 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       5 |  4846 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4847 | `		/* Compile the expression */` |
|       5 |  4848 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4849 | `		/* Emit the done instruction */` |
|       5 |  4850 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4851 | `		/* Restore default bytecode container */` |
|       5 |  4852 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       2 |  4853 | `	}` |
|       - |  4854 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       5 |  4855 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       5 |  4856 | `	return SXRET_OK;` |
|       1 |  4857 | `Synchronize:` |
|       - |  4858 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  4859 | `	 * statement.` |
|       - |  4860 | `	 */` |
|       5 |  4861 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  4862 | `		pGen->pIn++;` |
|       1 |  4863 | `	}` |
|       3 |  4864 | `	return SXRET_OK;` |
|       5 |  4865 |  |
|       - |  4866 | `/*` |
|       - |  4867 | ` * Compile the var statement.` |
|       - |  4868 | ` * Symisc Extension:` |
|       - |  4869 | ` *      var statement can be used outside of a class definition.` |
|       - |  4870 | ` */` |
|       4 |  4871 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  4872 |  |
|       - |  4873 | `	sxu32 nLine;` |
|       - |  4874 | `	sxi32 rc;` |
|       5 |  4875 | `	nLine = pGen->pIn->nLine;` |
|       - |  4876 | `	/* Jump the 'var' keyword */` |
|       5 |  4877 | `	pGen->pIn++;` |
|       5 |  4878 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  4879 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  4880 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  4881 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  4882 | `			pGen->pIn++;` |
|     ! 0 |  4883 | `		}` |
|     ! 0 |  4884 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4885 | `			return SXERR_ABORT;` |
|       - |  4886 | `		}` |
|     ! 0 |  4887 | `	}else{` |
|       - |  4888 | `		/* Compile the expression */` |
|       5 |  4889 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  4890 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4891 | `			return SXERR_ABORT;` |
|       5 |  4892 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  4893 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  4894 | `		}` |
|       - |  4895 | `	}` |
|       5 |  4896 | `	return SXRET_OK;` |
|       3 |  4897 |  |
|       - |  4898 | `/*` |
|       - |  4899 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  4900 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  4901 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  4902 | ` */` |
|       - |  4903 | `/*` |
|       - |  4904 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  4905 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  4906 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  4907 | ` * qualified name and updates the instruction's operand index.` |
|       - |  4908 | ` *` |
|       - |  4909 | ` * Resolution order:` |
|       - |  4910 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  4911 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  4912 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  4913 | ` *` |
|       - |  4914 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  4915 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  4916 | ` * Returns the (possibly new) literal index.` |
|       - |  4917 | ` */` |
|  390092 |  4918 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  4919 |  |
|       - |  4920 | `	ph7_value *pLit;` |
|       - |  4921 | `	const char *zLit;` |
|       - |  4922 | `	SyString sQualified;` |
|       - |  4923 | `	sxu32 nLit;` |
|       - |  4924 | `	sxu32 k;` |
|       - |  4925 | `	sxu32 nNewIdx;` |
|       - |  4926 | `	int hasNsSep;` |
|       - |  4927 | `	SyHashEntry *pImport;` |
|       - |  4928 | `	ph7_value *pNew;` |
|  390097 |  4929 | `	if( pFromImport ){` |
|  372963 |  4930 | `		*pFromImport = 0;` |
|  186479 |  4931 | `	}` |
|  390097 |  4932 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  390097 |  4933 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4934 | `		return nOrigIdx;` |
|       - |  4935 | `	}` |
|  390097 |  4936 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  390097 |  4937 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4938 | `	/* Skip if already qualified (contains backslash) */` |
|  390097 |  4939 | `	hasNsSep = 0;` |
| 4220515 |  4940 | `	for( k = 0; k < nLit; k++ ){` |
| 3830431 |  4941 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1915214 |  4942 | `	}` |
|  390097 |  4943 | `	if( hasNsSep ){` |
|      11 |  4944 | `		return nOrigIdx;` |
|       - |  4945 | `	}` |
|       - |  4946 | `	/* Check use imports first (works even outside namespaces) */` |
|  390089 |  4947 | `	SyBlobReset(&pGen->sWorker);` |
|  390089 |  4948 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  390089 |  4949 | `	if( pImport ){` |
|      41 |  4950 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  4951 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  4952 | `		if( pFromImport ){` |
|      18 |  4953 | `			*pFromImport = 1;` |
|       8 |  4954 | `		}` |
|      23 |  4955 | `	}else{` |
|  390053 |  4956 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  389963 |  4957 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  4958 | `		}` |
|       - |  4959 | `		/* Prepend current namespace */` |
|      95 |  4960 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  4961 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  4962 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  4963 | `	}` |
|       - |  4964 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  4965 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  4966 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  4967 | `		return nNewIdx; /* Already interned */` |
|       - |  4968 | `	}` |
|      79 |  4969 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  4970 | `	if( pNew == 0 ){` |
|     ! 0 |  4971 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  4972 | `	}` |
|      79 |  4973 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  4974 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  4975 | `	return nNewIdx;` |
|  195051 |  4976 |  |
|       - |  4977 | `/*` |
|       - |  4978 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4979 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4980 | ` */` |
|   85722 |  4981 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  4982 |  |
|       - |  4983 | `	SyHashEntry *pImport;` |
|       - |  4984 | `	/* Check use imports first */` |
|   85727 |  4985 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   85727 |  4986 | `	if( pImport ){` |
|      15 |  4987 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 |  4988 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      15 |  4989 | `		return;` |
|       - |  4990 | `	}` |
|       - |  4991 | `	/* Prepend current namespace if active */` |
|   85715 |  4992 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4993 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4994 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4995 | `	}` |
|   85715 |  4996 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   42866 |  4997 |  |
|       - |  4998 | `/*` |
|       - |  4999 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5000 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5001 | ` * The caller must release pOut when done.` |
|       - |  5002 | ` */` |
|  120690 |  5003 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5004 |  |
|  120695 |  5005 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5006 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5007 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5008 | `	}` |
|  120695 |  5009 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  120695 |  5010 |  |
|       - |  5011 | `/*` |
|       - |  5012 | ` * Compile a namespace statement` |
|       - |  5013 | ` * According to the PHP language reference manual` |
|       - |  5014 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5015 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5016 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5017 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5018 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5019 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5020 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5021 | ` *  programming world.` |
|       - |  5022 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5023 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5024 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5025 | ` *  classes/functions/constants.` |
|       - |  5026 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5027 | ` *  readability of source code.` |
|       - |  5028 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5029 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5030 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5031 | ` *       class MyClass {}` |
|       - |  5032 | ` *       function myfunction() {}` |
|       - |  5033 | ` *       const MYCONST = 1;` |
|       - |  5034 | ` *       $a = new MyClass;` |
|       - |  5035 | ` *       $c = new \my\name\MyClass;` |
|       - |  5036 | ` *       $a = strlen('hi');` |
|       - |  5037 | ` *       $d = namespace\MYCONST;` |
|       - |  5038 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5039 | ` *       echo constant($d);` |
|       - |  5040 | ` * NOTE` |
|       - |  5041 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5042 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5043 | ` */` |
|       - |  5044 | `/*` |
|       - |  5045 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5046 | ` */` |
|      14 |  5047 | `static const char * TokenTypeName(sxu32 nType)` |
|       4 |  5048 |  |
|      18 |  5049 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 |  5050 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 |  5051 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 |  5052 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 |  5053 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 |  5054 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5055 | `	return "token";` |
|      11 |  5056 |  |
|     106 |  5057 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5058 |  |
|       - |  5059 | `	sxu32 nLine;` |
|       - |  5060 | `	sxi32 rc;` |
|     111 |  5061 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5062 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5063 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5064 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5065 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5066 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5067 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5068 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5069 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5070 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5071 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5072 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5073 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5074 | `		return SXRET_OK;` |
|       - |  5075 | `	}` |
|     111 |  5076 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5077 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5078 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5079 | `		return SXRET_OK;` |
|       - |  5080 | `	}` |
|     111 |  5081 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5082 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5083 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5084 | `		return SXRET_OK;` |
|       - |  5085 | `	}` |
|       - |  5086 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5087 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5088 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5089 | `			/* Append backslash separator */` |
|      27 |  5090 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5091 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5092 | `			}` |
|      16 |  5093 | `		}else{` |
|       - |  5094 | `			/* Append identifier */` |
|     131 |  5095 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5096 | `		}` |
|     153 |  5097 | `		pGen->pIn++;` |
|       5 |  5098 | `	}` |
|       - |  5099 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5100 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5101 | `	{` |
|     111 |  5102 | `		char *zNsDup = 0;` |
|     111 |  5103 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5104 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5105 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5106 | `		}` |
|     111 |  5107 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5108 | `	}` |
|     111 |  5109 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5110 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5111 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5112 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5113 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5114 | `			return SXERR_ABORT;` |
|       - |  5115 | `		}` |
|       2 |  5116 | `	}` |
|     111 |  5117 | `	return SXRET_OK;` |
|      58 |  5118 |  |
|       - |  5119 | `/*` |
|       - |  5120 | ` * Compile the 'use' statement` |
|       - |  5121 | ` * According to the PHP language reference manual` |
|       - |  5122 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5123 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5124 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5125 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5126 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5127 | ` *  a function or constant is not supported.` |
|       - |  5128 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5129 | ` * NOTE` |
|       - |  5130 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5131 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5132 | ` */` |
|      68 |  5133 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5134 |  |
|       - |  5135 | `	sxu32 nLine;` |
|       - |  5136 | `	sxi32 rc;` |
|       - |  5137 | `	SyBlob sPath;` |
|       - |  5138 | `	SyString sAlias;` |
|       - |  5139 | `	SyToken *pLast;` |
|       - |  5140 | `	char *zDup;` |
|       - |  5141 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5142 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5143 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5144 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5145 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5146 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5147 | `	iUseType = 0;` |
|      73 |  5148 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5149 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5150 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5151 | `			iUseType = 1;` |
|      16 |  5152 | `			pGen->pIn++;` |
|      23 |  5153 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5154 | `			iUseType = 2;` |
|      16 |  5155 | `			pGen->pIn++;` |
|       7 |  5156 | `		}` |
|      14 |  5157 | `	}` |
|       - |  5158 | `	/* Select target hash tables based on import type */` |
|      73 |  5159 | `	switch( iUseType ){` |
|       7 |  5160 | `		case 1:` |
|      16 |  5161 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5162 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5163 | `			break;` |
|       7 |  5164 | `		case 2:` |
|      16 |  5165 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5166 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5167 | `			break;` |
|      20 |  5168 | `		default:` |
|      45 |  5169 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5170 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5171 | `			break;` |
|       - |  5172 | `	}` |
|      73 |  5173 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5174 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5175 | `	for(;;){` |
|      75 |  5176 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5177 | `			break;` |
|       - |  5178 | `		}` |
|      75 |  5179 | `		SyBlobReset(&sPath);` |
|      75 |  5180 | `		pLast = 0;` |
|       - |  5181 | `		/* Collect the full namespace path */` |
|     261 |  5182 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5183 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5184 | `				pLast = pGen->pIn;` |
|     131 |  5185 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5186 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5187 | `				}` |
|     131 |  5188 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5189 | `			}` |
|     191 |  5190 | `			pGen->pIn++;` |
|       5 |  5191 | `		}` |
|      75 |  5192 | `		if( pLast == 0 ){` |
|       - |  5193 | `			/* Empty path */` |
|       6 |  5194 | `			break;` |
|       - |  5195 | `		}` |
|       - |  5196 | `		/* Default alias is the last component of the path */` |
|      71 |  5197 | `		sAlias = pLast->sData;` |
|       - |  5198 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5199 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5200 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      19 |  5201 | `			pGen->pIn++; /* Jump 'as' */` |
|      19 |  5202 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      19 |  5203 | `				sAlias = pGen->pIn->sData;` |
|      19 |  5204 | `				pGen->pIn++;` |
|       8 |  5205 | `			}` |
|       8 |  5206 | `		}` |
|       - |  5207 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5208 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5209 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5210 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5211 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5212 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5213 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5214 | `				return SXERR_ABORT;` |
|       - |  5215 | `			}` |
|       2 |  5216 | `		}` |
|       - |  5217 | `		/* Register the import: alias -> FQN.` |
|       - |  5218 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5219 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5220 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5221 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5222 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5223 | `		if( zDup ){` |
|      71 |  5224 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5225 | `			if( pVmHash ){` |
|       - |  5226 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5227 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5228 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5229 | `				if( zAliasDup ){` |
|      43 |  5230 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5231 | `				}` |
|      19 |  5232 | `			}` |
|      71 |  5233 | `			if( iUseType == 2 ){` |
|       - |  5234 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5235 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5236 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5237 | `				if( zAliasDup ){` |
|       - |  5238 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5239 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5240 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5241 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5242 | `					if( azPair ){` |
|      16 |  5243 | `						azPair[0] = zAliasDup;` |
|      16 |  5244 | `						azPair[1] = zDup;` |
|      16 |  5245 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5246 | `					}` |
|       7 |  5247 | `				}` |
|       7 |  5248 | `			}` |
|      33 |  5249 | `		}` |
|       - |  5250 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5251 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5252 | `			pGen->pIn++;` |
|       2 |  5253 | `		}else{` |
|      37 |  5254 | `			break;` |
|       - |  5255 | `		}` |
|       1 |  5256 | `	}` |
|      73 |  5257 | `	SyBlobRelease(&sPath);` |
|      73 |  5258 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5259 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5260 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5261 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5262 | `			return SXERR_ABORT;` |
|       - |  5263 | `		}` |
|       1 |  5264 | `	}` |
|      73 |  5265 | `	return SXRET_OK;` |
|      39 |  5266 |  |
|       - |  5267 | `/*` |
|       - |  5268 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5269 | ` *` |
|       - |  5270 | ` * According to the PHP language reference manual.` |
|       - |  5271 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5272 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5273 | ` *  declare (directive)` |
|       - |  5274 | ` *   statement` |
|       - |  5275 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5276 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5277 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5278 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5279 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5280 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5281 | ` * <?php` |
|       - |  5282 | ` * // these are the same:` |
|       - |  5283 | ` * // you can use this:` |
|       - |  5284 | ` * declare(ticks=1) {` |
|       - |  5285 | ` *   // entire script here` |
|       - |  5286 | ` * }` |
|       - |  5287 | ` * // or you can use this:` |
|       - |  5288 | ` * declare(ticks=1);` |
|       - |  5289 | ` * // entire script here` |
|       - |  5290 | ` * ?>` |
|       - |  5291 | ` *` |
|       - |  5292 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5293 | ` */` |
|       - |  5294 | `/*` |
|       - |  5295 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5296 | ` */` |
|      68 |  5297 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5298 |  |
|     103 |  5299 | `	return SyStringLength(pName) == nWant` |
|      68 |  5300 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5301 |  |
|       - |  5302 |  |
|      40 |  5303 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5304 |  |
|      45 |  5305 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5306 | `	SyToken *pBodyEnd = 0;` |
|       - |  5307 | `	SyToken *pBodyStart;` |
|       - |  5308 | `	SyToken *pCursor;` |
|       - |  5309 | `	int bHasStrictTypes;` |
|       - |  5310 | `	int bBlockForm;` |
|       - |  5311 | `	int bPlacementOk;` |
|       - |  5312 | `	sxi32 rc;` |
|      45 |  5313 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5314 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5315 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5316 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5317 | `			return SXERR_ABORT;` |
|       - |  5318 | `		}` |
|       6 |  5319 | `		goto Synchro;` |
|       - |  5320 | `	}` |
|      41 |  5321 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5322 | `	pBodyStart = pGen->pIn;` |
|       - |  5323 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5324 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5325 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5326 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5327 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5328 | `			return SXERR_ABORT;` |
|       - |  5329 | `		}` |
|     ! 0 |  5330 | `		return SXRET_OK;` |
|       - |  5331 | `	}` |
|       - |  5332 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5333 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5334 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5335 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5336 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5337 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5338 | `			return SXERR_ABORT;` |
|       - |  5339 | `		}` |
|     ! 0 |  5340 | `	}` |
|      41 |  5341 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5342 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5343 | `	bHasStrictTypes = 0;` |
|       - |  5344 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5345 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5346 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5347 | `	pCursor = pBodyStart;` |
|      53 |  5348 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5349 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5350 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5351 | `				bHasStrictTypes = 1;` |
|      37 |  5352 | `				break;` |
|       - |  5353 | `			}` |
|       2 |  5354 | `		}` |
|      14 |  5355 | `		pCursor++;` |
|       2 |  5356 | `	}` |
|      41 |  5357 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5358 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5359 | `			"strict_types declaration must not use block mode");` |
|       3 |  5360 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5361 | `		return SXRET_OK;` |
|       - |  5362 | `	}` |
|      39 |  5363 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5364 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5365 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5366 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5367 | `		return SXRET_OK;` |
|       - |  5368 | `	}` |
|       - |  5369 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5370 | `	pCursor = pBodyStart;` |
|      65 |  5371 | `	while( pCursor < pBodyEnd ){` |
|       - |  5372 | `		SyToken *pNameTok;` |
|       - |  5373 | `		SyToken *pEqTok;` |
|       - |  5374 | `		SyToken *pValTok;` |
|       - |  5375 | `		SyString *pDirName;` |
|       - |  5376 | `		int bIsStrict;` |
|       - |  5377 | `		int iStrictValue;` |
|      37 |  5378 | `		pNameTok = pCursor;` |
|      37 |  5379 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5380 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5381 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5382 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5383 | `			return SXRET_OK;` |
|       - |  5384 | `		}` |
|      37 |  5385 | `		pEqTok = pNameTok + 1;` |
|      37 |  5386 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5387 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5388 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5389 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5390 | `			return SXRET_OK;` |
|       - |  5391 | `		}` |
|      37 |  5392 | `		pValTok = pEqTok + 1;` |
|      37 |  5393 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5394 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5395 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5396 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5397 | `			return SXRET_OK;` |
|       - |  5398 | `		}` |
|      37 |  5399 | `		pDirName = &pNameTok->sData;` |
|      37 |  5400 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5401 | `		if( bIsStrict ){` |
|       - |  5402 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5403 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5404 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5405 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5406 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5407 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5408 | `				return SXRET_OK;` |
|       - |  5409 | `			}` |
|      33 |  5410 | `			iStrictValue = -1;` |
|      33 |  5411 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5412 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5413 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5414 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5415 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5416 | `			}` |
|      33 |  5417 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5418 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5419 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5420 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5421 | `				return SXRET_OK;` |
|       - |  5422 | `			}` |
|      30 |  5423 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5424 | `		}else{` |
|       - |  5425 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5426 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5427 | `			 * behavior don't regress. */` |
|       8 |  5428 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5429 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5430 | `				ph7_lib_version()` |
|       - |  5431 | `				);` |
|       - |  5432 | `		}` |
|      35 |  5433 | `		pCursor = pValTok + 1;` |
|       - |  5434 | `		/* Consume separating comma (or end). */` |
|      35 |  5435 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5436 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5437 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5438 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5439 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5440 | `				return SXRET_OK;` |
|       - |  5441 | `			}` |
|       3 |  5442 | `			pCursor++;` |
|       1 |  5443 | `		}` |
|       5 |  5444 | `	}` |
|       - |  5445 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5446 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5447 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5448 | `	return SXRET_OK;` |
|       2 |  5449 | `Synchro:` |
|       - |  5450 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5451 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5452 | `		pGen->pIn++;` |
|       2 |  5453 | `	}` |
|       6 |  5454 | `	return SXRET_OK;` |
|      25 |  5455 |  |
|       - |  5456 | `/*` |
|       - |  5457 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5458 | ` * as follows:` |
|       - |  5459 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5460 | ` * {` |
|       - |  5461 | ` *   return "Making a cup of $type.\n";` |
|       - |  5462 | ` * }` |
|       - |  5463 | ` * Symisc eXtension.` |
|       - |  5464 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5465 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5466 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5467 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5468 | ` *      {` |
|       - |  5469 | ` *       var_dump($a);` |
|       - |  5470 | ` *      }` |
|       - |  5471 | ` *     //call test without args` |
|       - |  5472 | ` *      test();` |
|       - |  5473 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5474 | ` *      Example:` |
|       - |  5475 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5476 | ` * 3 -) Function overloading!!` |
|       - |  5477 | ` *      Example:` |
|       - |  5478 | ` *      function foo($a) {` |
|       - |  5479 | ` *   	  return $a.PHP_EOL;` |
|       - |  5480 | ` *	    }` |
|       - |  5481 | ` *	    function foo($a, $b) {` |
|       - |  5482 | ` *   	  return $a + $b;` |
|       - |  5483 | ` *	    }` |
|       - |  5484 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5485 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5486 | ` *      // Same arg` |
|       - |  5487 | ` *	   function foo(string $a)` |
|       - |  5488 | ` *	   {` |
|       - |  5489 | ` *	     echo "a is a string\n";` |
|       - |  5490 | ` *	     var_dump($a);` |
|       - |  5491 | ` *	   }` |
|       - |  5492 | ` *	  function foo(int $a)` |
|       - |  5493 | ` *	  {` |
|       - |  5494 | ` *	    echo "a is integer\n";` |
|       - |  5495 | ` *	    var_dump($a);` |
|       - |  5496 | ` *	  }` |
|       - |  5497 | ` *	  function foo(array $a)` |
|       - |  5498 | ` *	  {` |
|       - |  5499 | ` * 	    echo "a is an array\n";` |
|       - |  5500 | ` * 	    var_dump($a);` |
|       - |  5501 | ` *	  }` |
|       - |  5502 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5503 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5504 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5505 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5506 | ` * introduced by the PH7 engine.` |
|       - |  5507 | ` */` |
|   59846 |  5508 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5509 |  |
|       - |  5510 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5511 | `	SySet *pInstrContainer;` |
|       - |  5512 | `	sxi32 rc;` |
|       - |  5513 | `	/* Swap token stream */` |
|   59851 |  5514 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   59851 |  5515 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   59851 |  5516 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5517 | `	/* Compile the expression holding the argument value */` |
|   59851 |  5518 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5519 | `	/* Emit the done instruction */` |
|   59851 |  5520 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   59851 |  5521 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   59851 |  5522 | `	RE_SWAP_DELIMITER(pGen);` |
|   59851 |  5523 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5524 | `		return SXERR_ABORT;` |
|       - |  5525 | `	}` |
|   59851 |  5526 | `	return SXRET_OK;` |
|   29928 |  5527 |  |
|       - |  5528 | `/*` |
|       - |  5529 | ` * Collect function arguments one after one.` |
|       - |  5530 | ` * According to the PHP language reference manual.` |
|       - |  5531 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5532 | ` * list of expressions.` |
|       - |  5533 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5534 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5535 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5536 | ` * for more information.` |
|       - |  5537 | ` * Example #1 Passing arrays to functions` |
|       - |  5538 | ` * <?php` |
|       - |  5539 | ` * function takes_array($input)` |
|       - |  5540 | ` * {` |
|       - |  5541 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5542 | ` * }` |
|       - |  5543 | ` * ?>` |
|       - |  5544 | ` * Making arguments be passed by reference` |
|       - |  5545 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5546 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5547 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5548 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5549 | ` * to the argument name in the function definition:` |
|       - |  5550 | ` * Example #2 Passing function parameters by reference` |
|       - |  5551 | ` * <?php` |
|       - |  5552 | ` * function add_some_extra(&$string)` |
|       - |  5553 | ` * {` |
|       - |  5554 | ` *   $string .= 'and something extra.';` |
|       - |  5555 | ` * }` |
|       - |  5556 | ` * $str = 'This is a string, ';` |
|       - |  5557 | ` * add_some_extra($str);` |
|       - |  5558 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5559 | ` * ?>` |
|       - |  5560 | ` *` |
|       - |  5561 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5562 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5563 | ` * on these extension.` |
|       - |  5564 | ` */` |
|   83002 |  5565 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5566 |  |
|       - |  5567 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5568 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5569 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5570 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5571 | `	sxi32 rc;` |
|       - |  5572 |  |
|   83007 |  5573 | `	pIn = pGen->pIn;` |
|   83007 |  5574 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5575 | `	/* Process arguments one after one */` |
|  103814 |  5576 | `	for(;;){` |
|  207633 |  5577 | `		if( pIn >= pEnd ){` |
|       - |  5578 | `			/* No more arguments to process */` |
|   82995 |  5579 | `			break;` |
|       - |  5580 | `		}` |
|  124643 |  5581 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  124643 |  5582 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  124643 |  5583 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  124643 |  5584 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5585 | `		/* Parse optional visibility modifier (constructor property promotion, PHP 8.0+) */` |
|  124643 |  5586 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   56827 |  5587 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   56827 |  5588 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      47 |  5589 | `				if( !bCtorCtx ){` |
|       6 |  5590 | `					if( bAbstractCtx ){` |
|       3 |  5591 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5592 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5593 | `					}else{` |
|       3 |  5594 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5595 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5596 | `					}` |
|       6 |  5597 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5598 | `						return SXERR_ABORT;` |
|       - |  5599 | `					}` |
|       6 |  5600 | `					return SXERR_SYNTAX;` |
|       - |  5601 | `				}` |
|      43 |  5602 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      43 |  5603 | `				if( nKw == PH7_TKWRD_PRIVATE ){` |
|       3 |  5604 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PRIVATE;` |
|      42 |  5605 | `				}else if( nKw == PH7_TKWRD_PROTECTED ){` |
|       3 |  5606 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PROTECTED;` |
|       2 |  5607 | `				}else{` |
|      39 |  5608 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PUBLIC;` |
|       - |  5609 | `				}` |
|      43 |  5610 | `				pIn++;` |
|      19 |  5611 | `			}` |
|   28409 |  5612 | `		}` |
|       - |  5613 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  157800 |  5614 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   97070 |  5615 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   67924 |  5616 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   66315 |  5617 | `			sxu32 nLineLocal = pIn->nLine;` |
|   66315 |  5618 | `			sxi32 iTFlags = 0;` |
|   66315 |  5619 | `			pGen->pIn = pIn;` |
|   66315 |  5620 | `			rc = GenStateParseUnionTypeDecl(` |
|   33155 |  5621 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   33155 |  5622 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5623 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5624 | `				/* bAllowVoid */ 0,` |
|   33155 |  5625 | `						nLineLocal);` |
|   66315 |  5626 | `			pIn = pGen->pIn;` |
|   66315 |  5627 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5628 | `				return SXERR_ABORT;` |
|   66315 |  5629 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5630 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5631 | `				return SXERR_SYNTAX;` |
|   66313 |  5632 | `			}else if( rc == SXERR_SYNTAX ){` |
|       6 |  5633 | `				if( pIn < pEnd ){` |
|       8 |  5634 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5635 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5636 | `						&pIn->sData);` |
|       4 |  5637 | `				}else{` |
|     ! 0 |  5638 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5639 | `						"syntax error, unexpected end of file");` |
|       - |  5640 | `				}` |
|       6 |  5641 | `				return SXERR_SYNTAX;` |
|       - |  5642 | `			}` |
|   66309 |  5643 | `			sArg.iFlags \|= iTFlags;` |
|   33152 |  5644 | `		}` |
|  124633 |  5645 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5646 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5647 | `			return rc;` |
|       - |  5648 | `		}` |
|  124633 |  5649 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5650 | `			/* Pass by reference,record that */` |
|    3181 |  5651 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3181 |  5652 | `			pIn++;` |
|    1588 |  5653 | `		}` |
|  124633 |  5654 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5655 | `			/* Variadic parameter: ...$args */` |
|      47 |  5656 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      47 |  5657 | `			pIn++;` |
|      21 |  5658 | `		}` |
|  124633 |  5659 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5660 | `			/* Invalid argument */` |
|     ! 0 |  5661 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5662 | `			return rc;` |
|       - |  5663 | `		}` |
|  124633 |  5664 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5665 | `		/* Copy argument name */` |
|  124633 |  5666 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  124633 |  5667 | `		if( zDup == 0 ){` |
|     ! 0 |  5668 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5669 | `			return SXERR_ABORT;` |
|       - |  5670 | `		}` |
|  124633 |  5671 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  124633 |  5672 | `		pIn++;` |
|  124633 |  5673 | `		if( pIn < pEnd ){` |
|   70005 |  5674 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5675 | `				SyToken *pDefend;` |
|   59853 |  5676 | `				sxi32 iNest = 0;` |
|   59853 |  5677 | `				pIn++; /* Jump the equal sign */` |
|   59853 |  5678 | `				pDefend = pIn;` |
|       - |  5679 | `				/* Process the default value associated with this argument */` |
|  125995 |  5680 | `				while( pDefend < pEnd ){` |
|   97635 |  5681 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   31493 |  5682 | `						break;` |
|       - |  5683 | `					}` |
|   66147 |  5684 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5685 | `						/* Increment nesting level */` |
|    3153 |  5686 | `						iNest++;` |
|   64573 |  5687 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5688 | `						/* Decrement nesting level */` |
|    3153 |  5689 | `						iNest--;` |
|    1574 |  5690 | `					}` |
|   66147 |  5691 | `					pDefend++;` |
|       5 |  5692 | `				}` |
|   59853 |  5693 | `				if( pIn >= pDefend ){` |
|       3 |  5694 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5695 | `					return rc;` |
|       - |  5696 | `				}` |
|       - |  5697 | `				/* Process default value */` |
|   59851 |  5698 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   59851 |  5699 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5700 | `					return rc;` |
|       - |  5701 | `				}` |
|       - |  5702 | `				/* Point beyond the default value */` |
|   59851 |  5703 | `				pIn = pDefend;` |
|   29923 |  5704 | `			}` |
|   70003 |  5705 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5706 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5707 | `				return rc;` |
|       - |  5708 | `			}` |
|   70003 |  5709 | `			pIn++; /* Jump the trailing comma */` |
|   34999 |  5710 | `		}` |
|       - |  5711 | `		/* Append argument signature */` |
|  124631 |  5712 | `		if( sArg.nType > 0 ){` |
|   66265 |  5713 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5714 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    9471 |  5715 | `				int marker = 'o';` |
|    9471 |  5716 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    9471 |  5717 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    4738 |  5718 | `			}else{` |
|       - |  5719 | `				int c;` |
|   56799 |  5720 | `				c = 'n'; /* cc warning */` |
|       - |  5721 | `				/* Type leading character */` |
|   56799 |  5722 | `				switch(sArg.nType){` |
|     ! 0 |  5723 | `				case MEMOBJ_HASHMAP:` |
|       - |  5724 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5725 | `					c = 'h';` |
|     ! 0 |  5726 | `					break;` |
|    7906 |  5727 | `				case MEMOBJ_INT:` |
|       - |  5728 | `					/* Integer */` |
|   15817 |  5729 | `					c = 'i';` |
|   15817 |  5730 | `					break;` |
|       1 |  5731 | `				case MEMOBJ_BOOL:` |
|       - |  5732 | `					/* Bool */` |
|       3 |  5733 | `					c = 'b';` |
|       3 |  5734 | `					break;` |
|       1 |  5735 | `				case MEMOBJ_REAL:` |
|       - |  5736 | `					/* Float */` |
|       3 |  5737 | `					c = 'f';` |
|       3 |  5738 | `					break;` |
|   20481 |  5739 | `				case MEMOBJ_STRING:` |
|       - |  5740 | `					/* String */` |
|   40967 |  5741 | `					c = 's';` |
|   40967 |  5742 | `					break;` |
|       7 |  5743 | `				case MEMOBJ_OBJ:` |
|       - |  5744 | `					/* Object */` |
|      16 |  5745 | `					c = 'o';` |
|      14 |  5746 | `					break;` |
|       1 |  5747 | `				default:` |
|       2 |  5748 | `					break;` |
|       - |  5749 | `				}` |
|   56799 |  5750 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5751 | `			}` |
|   33135 |  5752 | `		}else{` |
|       - |  5753 | `			/* No type is associated with this parameter which mean` |
|       - |  5754 | `			 * that this function is not condidate for overloading.` |
|       - |  5755 | `			 */` |
|   58371 |  5756 | `			SyBlobRelease(&sSig);` |
|       - |  5757 | `		}` |
|       - |  5758 | `		/* Save in the argument set */` |
|  124631 |  5759 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5760 | `	}` |
|   82995 |  5761 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5762 | `		/* Save function signature */` |
|   41059 |  5763 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   20527 |  5764 | `	}` |
|   82995 |  5765 | `	return SXRET_OK;` |
|   41506 |  5766 |  |
|       - |  5767 | `/*` |
|       - |  5768 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5769 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5770 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5771 | ` */` |
|  197052 |  5772 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5773 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5774 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5775 | `	)` |
|       5 |  5776 |  |
|       - |  5777 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5778 | `	GenBlock *pBlock;` |
|       - |  5779 | `	sxu32 nGotoOfft;` |
|       - |  5780 | `	sxi32 rc;` |
|       - |  5781 | `	/* Attach the new function */` |
|  197057 |  5782 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  197057 |  5783 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5784 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5785 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5786 | `		return SXERR_ABORT;` |
|       - |  5787 | `	}` |
|  197057 |  5788 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5789 | `	/* Swap bytecode containers */` |
|  197057 |  5790 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  197057 |  5791 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5792 | `	/* Emit constructor property promotion prologue:` |
|       - |  5793 | `	 *   $this->NAME = $NAME;` |
|       - |  5794 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5795 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5796 | `	{` |
|  197057 |  5797 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5798 | `		sxu32 i;` |
|  296377 |  5799 | `		for( i = 0; i < nArg; i++ ){` |
|   99325 |  5800 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5801 | `			char *zSrc;` |
|       - |  5802 | `			sxu32 nSrc,nName;` |
|       - |  5803 | `			SySet sToken;` |
|       - |  5804 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5805 | `			sxi32 rcPromote;` |
|   99325 |  5806 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   99295 |  5807 | `				continue;` |
|       - |  5808 | `			}` |
|       - |  5809 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5810 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5811 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5812 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5813 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      33 |  5814 | `			nName = SyStringLength(&pArg->sName);` |
|      33 |  5815 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      33 |  5816 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      33 |  5817 | `			if( zSrc == 0 ){` |
|     ! 0 |  5818 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5819 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5820 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5821 | `				return SXERR_ABORT;` |
|       - |  5822 | `			}` |
|       - |  5823 | `			{` |
|      33 |  5824 | `				char *z = zSrc;` |
|      33 |  5825 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      33 |  5826 | `				z += sizeof("$this->")-1;` |
|      33 |  5827 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      33 |  5828 | `				z += nName;` |
|      33 |  5829 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      33 |  5830 | `				z += sizeof(" = $")-1;` |
|      33 |  5831 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      33 |  5832 | `				z += nName;` |
|      33 |  5833 | `				*z = 0;` |
|       - |  5834 | `			}` |
|      33 |  5835 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      33 |  5836 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      33 |  5837 | `			pTmpIn = pGen->pIn;` |
|      33 |  5838 | `			pTmpEnd = pGen->pEnd;` |
|      33 |  5839 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      33 |  5840 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      33 |  5841 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 |  5842 | `			pGen->pIn = pTmpIn;` |
|      33 |  5843 | `			pGen->pEnd = pTmpEnd;` |
|      33 |  5844 | `			SySetRelease(&sToken);` |
|      33 |  5845 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  5846 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5847 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5848 | `				return SXERR_ABORT;` |
|       - |  5849 | `			}` |
|       - |  5850 | `			/* Discard the assignment result — this is a statement expression. */` |
|      33 |  5851 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      18 |  5852 | `		}` |
|       - |  5853 | `	}` |
|       - |  5854 | `	/* Compile the body */` |
|  197057 |  5855 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5856 | `	/* Fix exception jumps now the destination is resolved */` |
|  197057 |  5857 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5858 | `	/* Emit the final return if not yet done */` |
|  197057 |  5859 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5860 | `	/* Fix gotos jumps now the destination is resolved */` |
|  197057 |  5861 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5862 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5863 | `	}` |
|  197057 |  5864 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5865 | `	/* Restore the default container */` |
|  197057 |  5866 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5867 | `	/* Leave function block */` |
|  197057 |  5868 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  197057 |  5869 | `	if( rc == SXERR_ABORT ){` |
|       - |  5870 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5871 | `		return SXERR_ABORT;` |
|       - |  5872 | `	}` |
|       - |  5873 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5874 | `	{` |
|  197057 |  5875 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5876 | `		sxu32 i;` |
| 3848355 |  5877 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 3651339 |  5878 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      41 |  5879 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      41 |  5880 | `				break;` |
|       - |  5881 | `			}` |
| 1825654 |  5882 | `		}` |
|       - |  5883 | `	}` |
|       - |  5884 | `	/* All done, function body compiled */` |
|  197057 |  5885 | `	return SXRET_OK;` |
|   98531 |  5886 |  |
|       - |  5887 | `/*` |
|       - |  5888 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5889 | ` * According to the PHP language reference manual.` |
|       - |  5890 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5891 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5892 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5893 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5894 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5895 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5896 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5897 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5898 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5899 | ` *` |
|       - |  5900 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5901 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5902 | ` * on these extension.` |
|       - |  5903 | ` */` |
|       - |  5904 | `/*` |
|       - |  5905 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5906 | ` */` |
|     320 |  5907 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  5908 |  |
|       - |  5909 | `	sxu32 i;` |
|     893 |  5910 | `	for( i = 0; i < n; i++ ){` |
|     765 |  5911 | `		int a = zA[i], b = zB[i];` |
|     765 |  5912 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     765 |  5913 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     765 |  5914 | `		if( a != b ) return a - b;` |
|     289 |  5915 | `	}` |
|     133 |  5916 | `	return 0;` |
|     165 |  5917 |  |
|       - |  5918 | `/*` |
|       - |  5919 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5920 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5921 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5922 | ` */` |
|       - |  5923 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5924 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5925 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5926 |  |
|       - |  5927 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5928 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5929 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5930 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5931 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5932 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5933 |  |
|       - |  5934 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5935 | `struct PhlTypeAtom {` |
|       - |  5936 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5937 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5938 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5939 | `	sxu32 nCanon;` |
|       - |  5940 | `};` |
|       - |  5941 |  |
|       - |  5942 | `/*` |
|       - |  5943 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5944 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5945 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5946 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5947 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5948 | ` * already be consumed by the caller.` |
|       - |  5949 | ` */` |
|   66876 |  5950 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  5951 |  |
|   66881 |  5952 | `	SyToken *pIn = pGen->pIn;` |
|   66881 |  5953 | `	SyZero(pOut, sizeof(*pOut));` |
|   66881 |  5954 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   66881 |  5955 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5956 | `		return SXERR_SYNTAX;` |
|       - |  5957 | `	}` |
|       - |  5958 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   66881 |  5959 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5960 | `		pIn++;` |
|       8 |  5961 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5962 | `			return SXERR_SYNTAX;` |
|       - |  5963 | `		}` |
|       3 |  5964 | `	}` |
|   66881 |  5965 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5966 | `		return SXERR_SYNTAX;` |
|       - |  5967 | `	}` |
|   66881 |  5968 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   57197 |  5969 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   57197 |  5970 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      18 |  5971 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   57190 |  5972 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      59 |  5973 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   57156 |  5974 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   15989 |  5975 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   49137 |  5976 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   41091 |  5977 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   20602 |  5978 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      28 |  5979 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      46 |  5980 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  5981 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      21 |  5982 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       5 |  5983 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  5984 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  5985 | `			pOut->sClass = pIn->sData;` |
|       4 |  5986 | `		}else{` |
|       3 |  5987 | `			return SXERR_SYNTAX;` |
|       - |  5988 | `		}` |
|   57195 |  5989 | `		pIn++;` |
|   28600 |  5990 | `	}else{` |
|       - |  5991 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  5992 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    9689 |  5993 | `		SyString *pT = &pIn->sData;` |
|    9689 |  5994 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      18 |  5995 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      18 |  5996 | `			pIn++;` |
|    9681 |  5997 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     111 |  5998 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     111 |  5999 | `			pIn++;` |
|    9620 |  6000 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  6001 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  6002 | `			pIn++;` |
|       2 |  6003 | `		}else{` |
|       - |  6004 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    9565 |  6005 | `			SyToken *pFirst = pIn;` |
|    9565 |  6006 | `			SyToken *pLast = pIn;` |
|    9565 |  6007 | `			pOut->nType = SXU32_HIGH;` |
|    9565 |  6008 | `			pOut->sClass = pIn->sData;` |
|    9565 |  6009 | `			pIn++;` |
|   14343 |  6010 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    9568 |  6011 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6012 | `				pLast = &pIn[1];` |
|       3 |  6013 | `				pIn += 2;` |
|       1 |  6014 | `			}` |
|    9565 |  6015 | `			if( pLast != pFirst ){` |
|       3 |  6016 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6017 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6018 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6019 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6020 | `			}` |
|       - |  6021 | `		}` |
|       - |  6022 | `	}` |
|   66879 |  6023 | `	pGen->pIn = pIn;` |
|   66879 |  6024 | `	return SXRET_OK;` |
|   33443 |  6025 |  |
|       - |  6026 |  |
|       - |  6027 | `/*` |
|       - |  6028 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6029 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6030 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6031 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6032 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6033 | ` */` |
|   66776 |  6034 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6035 |  |
|       - |  6036 | `	int i;` |
|   66781 |  6037 | `	int nNonNull = 0;` |
|  133641 |  6038 | `	for( i = 0; i < nAtoms; i++ ){` |
|   66865 |  6039 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   66849 |  6040 | `			nNonNull++;` |
|   33422 |  6041 | `		}` |
|   33435 |  6042 | `	}` |
|   66781 |  6043 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6044 | `		/* Shorthand: ?T */` |
|      60 |  6045 | `		for( i = 0; i < nAtoms; i++ ){` |
|      60 |  6046 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      60 |  6047 | `			SyBlobAppend(pBlob, "?", 1);` |
|      60 |  6048 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      15 |  6049 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       9 |  6050 | `			}else{` |
|      47 |  6051 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6052 | `			}` |
|      60 |  6053 | `			return;` |
|     ! 0 |  6054 | `		}` |
|     ! 0 |  6055 | `	}` |
|       - |  6056 | `	{` |
|   66725 |  6057 | `		int bFirst = 1;` |
|       - |  6058 | `		/* 1) Classes in declaration order */` |
|  133523 |  6059 | `		for( i = 0; i < nAtoms; i++ ){` |
|   66803 |  6060 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    9557 |  6061 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    9557 |  6062 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    9557 |  6063 | `				bFirst = 0;` |
|    4776 |  6064 | `			}` |
|   33404 |  6065 | `		}` |
|       - |  6066 | `		/* 2) Built-ins in canonical order */` |
|       - |  6067 | `		{` |
|       - |  6068 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6069 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6070 | `			int k;` |
|  467045 |  6071 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  743889 |  6072 | `				for( i = 0; i < nAtoms; i++ ){` |
|  400701 |  6073 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   57137 |  6074 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   57137 |  6075 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   57137 |  6076 | `						bFirst = 0;` |
|   57137 |  6077 | `						break;` |
|       - |  6078 | `					}` |
|  171787 |  6079 | `				}` |
|  200165 |  6080 | `			}` |
|       - |  6081 | `		}` |
|       - |  6082 | `		/* 3) null suffix */` |
|   66725 |  6083 | `		if( bNullable ){` |
|      12 |  6084 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      12 |  6085 | `			SyBlobAppend(pBlob, "null", 4);` |
|       5 |  6086 | `		}` |
|       - |  6087 | `	}` |
|   33393 |  6088 |  |
|       - |  6089 |  |
|       - |  6090 | `/*` |
|       - |  6091 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6092 | ` *` |
|       - |  6093 | ` * Outputs:` |
|       - |  6094 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6095 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6096 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6097 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6098 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6099 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6100 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6101 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6102 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6103 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6104 | ` *` |
|       - |  6105 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6106 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6107 | ` */` |
|   66786 |  6108 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6109 | `	ph7_gen_state *pGen,` |
|       - |  6110 | `	sxu32 *pnType,` |
|       - |  6111 | `	SyString *pClass,` |
|       - |  6112 | `	SySet *pAlts,` |
|       - |  6113 | `	sxi32 *piTypeFlags,` |
|       - |  6114 | `	SyString *pTypeText,` |
|       - |  6115 | `	int iNullableFlag,` |
|       - |  6116 | `	int iUnionFlag,` |
|       - |  6117 | `	int bAllowVoid,` |
|       - |  6118 | `	sxu32 nLine` |
|       5 |  6119 | `){` |
|       - |  6120 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   66791 |  6121 | `	int nAtoms = 0;` |
|   66791 |  6122 | `	int bShortNullable = 0;` |
|   66791 |  6123 | `	int bExplicitNull = 0;` |
|       - |  6124 | `	sxi32 rc;` |
|   66791 |  6125 | `	*pnType = 0;` |
|   66791 |  6126 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   66791 |  6127 | `	*piTypeFlags = 0;` |
|   66791 |  6128 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6129 |  |
|   66791 |  6130 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6131 | `		return SXRET_OK;` |
|       - |  6132 | `	}` |
|       - |  6133 | ``	/* Optional `?` shorthand prefix */`` |
|   66786 |  6134 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      57 |  6135 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      56 |  6136 | `		bShortNullable = 1;` |
|      56 |  6137 | `		pGen->pIn++;` |
|      56 |  6138 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6139 | `			return SXERR_SYNTAX;` |
|       - |  6140 | `		}` |
|      26 |  6141 | `	}` |
|       - |  6142 | `	/* First atom is mandatory */` |
|   66791 |  6143 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   66791 |  6144 | `	if( rc != SXRET_OK ){` |
|       3 |  6145 | `		return rc;` |
|       - |  6146 | `	}` |
|   66789 |  6147 | `	nAtoms = 1;` |
|       - |  6148 | ``	/* Subsequent atoms separated by `\|` */`` |
|  100313 |  6149 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   66926 |  6150 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      97 |  6151 | `		if( bShortNullable ){` |
|       - |  6152 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6153 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6154 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  6155 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6156 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6157 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6158 | `		}` |
|      95 |  6159 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6160 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6161 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6162 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6163 | `		}` |
|      95 |  6164 | ``		pGen->pIn++; /* skip `\|` */`` |
|      95 |  6165 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      95 |  6166 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  6167 | `			return rc;` |
|       - |  6168 | `		}` |
|      95 |  6169 | `		nAtoms++;` |
|       5 |  6170 | `	}` |
|       - |  6171 | `	/* Validation pass.` |
|       - |  6172 | `	 *` |
|       - |  6173 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6174 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6175 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6176 | `	 */` |
|       - |  6177 | `	{` |
|       - |  6178 | `		int i, j;` |
|   66787 |  6179 | `		int bHasNonNull = 0;` |
|  133653 |  6180 | `		for( i = 0; i < nAtoms; i++ ){` |
|   66877 |  6181 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     111 |  6182 | `				if( nAtoms > 1 ){` |
|       3 |  6183 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6184 | `						"Void can only be used as a standalone type");` |
|       3 |  6185 | `					return SXERR_SYNTAX;` |
|       - |  6186 | `				}` |
|     109 |  6187 | `				if( !bAllowVoid ){` |
|     ! 0 |  6188 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6189 | `						"void cannot be used here");` |
|     ! 0 |  6190 | `					return SXERR_SYNTAX;` |
|       - |  6191 | `				}` |
|     109 |  6192 | `				if( bShortNullable ){` |
|     ! 0 |  6193 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6194 | `						"Void type cannot be nullable");` |
|     ! 0 |  6195 | `					return SXERR_SYNTAX;` |
|       - |  6196 | `				}` |
|      52 |  6197 | `			}` |
|   66875 |  6198 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6199 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6200 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6201 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6202 | `				 * does not return), and folding them would mislead any` |
|       - |  6203 | `				 * future return-enforcement work. */` |
|       3 |  6204 | `				if( nAtoms > 1 ){` |
|       3 |  6205 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6206 | `						"never can only be used as a standalone type");` |
|       3 |  6207 | `					return SXERR_SYNTAX;` |
|       - |  6208 | `				}` |
|     ! 0 |  6209 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6210 | `					"never type is not yet implemented");` |
|     ! 0 |  6211 | `				return SXERR_SYNTAX;` |
|       - |  6212 | `			}` |
|   66873 |  6213 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      18 |  6214 | `				bExplicitNull = 1;` |
|      10 |  6215 | `			}else{` |
|   66857 |  6216 | `				bHasNonNull = 1;` |
|       - |  6217 | `			}` |
|       - |  6218 | `			/* Duplicate detection */` |
|   66993 |  6219 | `			for( j = 0; j < i; j++ ){` |
|     127 |  6220 | `				int bDup = 0;` |
|     127 |  6221 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      17 |  6222 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6223 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      14 |  6224 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6225 | `								aAtoms[j].sClass.zString,` |
|      12 |  6226 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6227 | `							bDup = 1;` |
|     ! 0 |  6228 | `						}` |
|       8 |  6229 | `					}else{` |
|       3 |  6230 | `						bDup = 1;` |
|       - |  6231 | `					}` |
|       7 |  6232 | `				}` |
|     127 |  6233 | `				if( bDup ){` |
|       - |  6234 | `					const char *zName;` |
|       - |  6235 | `					sxu32 nName;` |
|       3 |  6236 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6237 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6238 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6239 | `					}else{` |
|       3 |  6240 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6241 | `						nName = aAtoms[i].nCanon;` |
|       - |  6242 | `					}` |
|       4 |  6243 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6244 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6245 | `					return SXERR_SYNTAX;` |
|       - |  6246 | `				}` |
|      65 |  6247 | `			}` |
|   33438 |  6248 | `		}` |
|   66781 |  6249 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6250 | `			if( bShortNullable ){` |
|       - |  6251 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6252 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6253 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6254 | `				return SXERR_SYNTAX;` |
|       - |  6255 | `			}` |
|       - |  6256 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6257 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6258 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6259 | `			 * atom, so set it here. */` |
|       7 |  6260 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6261 | `		}` |
|       - |  6262 | `	}` |
|       - |  6263 | `	/* Compute nullability flag */` |
|   66781 |  6264 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      70 |  6265 | `		*piTypeFlags \|= iNullableFlag;` |
|      33 |  6266 | `	}` |
|       - |  6267 | `	/* Build canonical type text */` |
|   66781 |  6268 | `	if( pTypeText ){` |
|       - |  6269 | `		SyBlob sBlob;` |
|   66781 |  6270 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  100144 |  6271 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   33388 |  6272 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   66781 |  6273 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  100013 |  6274 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   66672 |  6275 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   66677 |  6276 | `			if( zDup ){` |
|   66677 |  6277 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   33336 |  6278 | `			}` |
|   33336 |  6279 | `		}` |
|   66781 |  6280 | `		SyBlobRelease(&sBlob);` |
|   33388 |  6281 | `	}` |
|       - |  6282 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6283 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6284 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6285 | `	{` |
|   66781 |  6286 | `		int nNonNull = 0;` |
|   66781 |  6287 | `		int iNonNullIdx = -1;` |
|       - |  6288 | `		int i;` |
|  133641 |  6289 | `		for( i = 0; i < nAtoms; i++ ){` |
|   66865 |  6290 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   66849 |  6291 | `				nNonNull++;` |
|   66849 |  6292 | `				iNonNullIdx = i;` |
|   33422 |  6293 | `			}` |
|   33435 |  6294 | `		}` |
|   66781 |  6295 | `		if( nNonNull <= 1 ){` |
|       - |  6296 | `			/* Fast path: store as single type. */` |
|   66723 |  6297 | `			if( iNonNullIdx >= 0 ){` |
|   66717 |  6298 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   66717 |  6299 | `				if( pA->nType == SXU32_HIGH ){` |
|   14309 |  6300 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    4768 |  6301 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    9541 |  6302 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    9541 |  6303 | `					*pnType = SXU32_HIGH;` |
|    9541 |  6304 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   61949 |  6305 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     109 |  6306 | `					*pnType = MEMOBJ_VOID;` |
|      57 |  6307 | `				}else{` |
|       - |  6308 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6309 | `					 * pass above rejects it as not-yet-implemented. */` |
|   57077 |  6310 | `					*pnType = pA->nType;` |
|       - |  6311 | `				}` |
|   33356 |  6312 | `			}` |
|   33364 |  6313 | `		}else{` |
|       - |  6314 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      63 |  6315 | `			*piTypeFlags \|= iUnionFlag;` |
|     199 |  6316 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6317 | `				ph7_type_alt sAlt;` |
|     141 |  6318 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     137 |  6319 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     137 |  6320 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      44 |  6321 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      14 |  6322 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      30 |  6323 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      30 |  6324 | `					sAlt.nType = SXU32_HIGH;` |
|      30 |  6325 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      16 |  6326 | `				}else{` |
|     109 |  6327 | `					sAlt.nType = aAtoms[i].nType;` |
|     109 |  6328 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6329 | `				}` |
|     137 |  6330 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      71 |  6331 | `			}` |
|       - |  6332 | `		}` |
|       - |  6333 | `	}` |
|   66781 |  6334 | `	return SXRET_OK;` |
|   33398 |  6335 |  |
|       - |  6336 |  |
|       - |  6337 | `/*` |
|       - |  6338 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6339 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6340 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6341 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6342 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6343 | `` *          and union types `: T\|U`.`` |
|       - |  6344 | ` */` |
|  279078 |  6345 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6346 |  |
|  279083 |  6347 | `	sxi32 iFlags = 0;` |
|       - |  6348 | `	sxi32 rc;` |
|       - |  6349 | `	sxu32 nLine;` |
|  279083 |  6350 | `	pFunc->nReturnType = 0;` |
|  279083 |  6351 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  279083 |  6352 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  279083 |  6353 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  278743 |  6354 | `		return SXRET_OK;` |
|       - |  6355 | `	}` |
|     345 |  6356 | `	pGen->pIn++; /* Skip ':' */` |
|     345 |  6357 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6358 | `		return SXRET_OK;` |
|       - |  6359 | `	}` |
|     345 |  6360 | `	nLine = pGen->pIn->nLine;` |
|     345 |  6361 | `	rc = GenStateParseUnionTypeDecl(` |
|     170 |  6362 | `		pGen,` |
|     170 |  6363 | `		&pFunc->nReturnType,` |
|     170 |  6364 | `		&pFunc->sReturnClass,` |
|     170 |  6365 | `		&pFunc->aReturnUnion,` |
|       - |  6366 | `		&iFlags,` |
|     170 |  6367 | `		&pFunc->sReturnTypeName,` |
|       - |  6368 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6369 | `		/* iUnionFlag */ 0,` |
|       - |  6370 | `		/* bAllowVoid */ 1,` |
|     170 |  6371 | `		nLine);` |
|     170 |  6372 | `	(void)iFlags;` |
|     345 |  6373 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6374 | `		return SXERR_ABORT;` |
|       - |  6375 | `	}` |
|     345 |  6376 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6377 | `		/* Error already reported */` |
|     ! 0 |  6378 | `		return SXERR_SYNTAX;` |
|       - |  6379 | `	}` |
|     345 |  6380 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6381 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6382 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6383 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6384 | `				&pGen->pIn->sData);` |
|       3 |  6385 | `		}else{` |
|     ! 0 |  6386 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6387 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6388 | `		}` |
|       5 |  6389 | `		return SXERR_SYNTAX;` |
|       - |  6390 | `	}` |
|     341 |  6391 | `	return SXRET_OK;` |
|  139544 |  6392 |  |
|       - |  6393 |  |
|   42000 |  6394 | `static sxi32 GenStateCompileFunc(` |
|       - |  6395 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6396 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6397 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6398 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6399 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6400 | `	)` |
|       5 |  6401 |  |
|       - |  6402 | `	ph7_vm_func *pFunc;` |
|       - |  6403 | `	SyToken *pEnd;` |
|       - |  6404 | `	sxu32 nLine;` |
|       - |  6405 | `	char *zName;` |
|       - |  6406 | `	sxi32 rc;` |
|       - |  6407 | `	/* Extract line number */` |
|   42005 |  6408 | `	nLine = pGen->pIn->nLine;` |
|       - |  6409 | `	/* Jump the left parenthesis '(' */` |
|   42005 |  6410 | `	pGen->pIn++;` |
|       - |  6411 | `	/* Delimit the function signature */` |
|   42005 |  6412 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   42005 |  6413 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6414 | `		/* Syntax error */` |
|       9 |  6415 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       9 |  6416 | `		if( rc == SXERR_ABORT ){` |
|       - |  6417 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6418 | `			return SXERR_ABORT;` |
|       - |  6419 | `		}` |
|       9 |  6420 | `		pGen->pIn = pGen->pEnd;` |
|       9 |  6421 | `		return SXRET_OK;` |
|       - |  6422 | `	}` |
|       - |  6423 | `	/* Create the function state */` |
|   41999 |  6424 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   41999 |  6425 | `	if( pFunc == 0 ){` |
|     ! 0 |  6426 | `		goto OutOfMem;` |
|       - |  6427 | `	}` |
|       - |  6428 | `	/* Build the function name, prepending namespace if active */` |
|   42006 |  6429 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6430 | `		SyBlob sFQN;` |
|       - |  6431 | `		sxu32 nLen;` |
|      16 |  6432 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6433 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6434 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6435 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6436 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6437 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6438 | `		SyBlobRelease(&sFQN);` |
|      16 |  6439 | `		if( zName == 0 ){` |
|     ! 0 |  6440 | `			goto OutOfMem;` |
|       - |  6441 | `		}` |
|      16 |  6442 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6443 | `	}else{` |
|   41985 |  6444 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   41985 |  6445 | `		if( zName == 0 ){` |
|     ! 0 |  6446 | `			goto OutOfMem;` |
|       - |  6447 | `		}` |
|   41985 |  6448 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6449 | `	}` |
|   41999 |  6450 | `	if( pGen->pIn < pEnd ){` |
|       - |  6451 | `		/* Collect function arguments */` |
|   29071 |  6452 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   29071 |  6453 | `		if( rc == SXERR_ABORT ){` |
|       - |  6454 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6455 | `			return SXERR_ABORT;` |
|       - |  6456 | `		}` |
|   14533 |  6457 | `	}` |
|       - |  6458 | `	/* Point past ')' and parse optional return type ': type' */` |
|   41999 |  6459 | `	pGen->pIn = &pEnd[1];` |
|       - |  6460 | `	{` |
|   41999 |  6461 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   41999 |  6462 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6463 | `			return SXERR_ABORT;` |
|   41999 |  6464 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6465 | `			return SXERR_SYNTAX;` |
|       - |  6466 | `		}` |
|       - |  6467 | `	}` |
|   41995 |  6468 | `	if( bHandleClosure ){` |
|       - |  6469 | `		ph7_vm_func_closure_env sEnv;` |
|     251 |  6470 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     246 |  6471 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     136 |  6472 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      21 |  6473 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6474 | `				/* Closure,record environment variable */` |
|      21 |  6475 | `				pGen->pIn++;` |
|      21 |  6476 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6477 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6478 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6479 | `						return SXERR_ABORT;` |
|       - |  6480 | `					}` |
|     ! 0 |  6481 | `				}` |
|      21 |  6482 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6483 | `				/* Compile until we hit the first closing parenthesis */` |
|      41 |  6484 | `				while( pGen->pIn < pGen->pEnd ){` |
|      41 |  6485 | `					int iFlagsLocal = 0;` |
|      41 |  6486 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      21 |  6487 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      21 |  6488 | `						break;` |
|       - |  6489 | `					}` |
|      25 |  6490 | `					nLineLocal = pGen->pIn->nLine;` |
|      25 |  6491 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6492 | `						/* Pass by reference,record that */` |
|     ! 0 |  6493 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6494 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6495 | `							);` |
|     ! 0 |  6496 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6497 | `						pGen->pIn++;` |
|     ! 0 |  6498 | `					}` |
|      20 |  6499 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      25 |  6500 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6501 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6502 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6503 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6504 | `								return SXERR_ABORT;` |
|       - |  6505 | `							}` |
|       - |  6506 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6507 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6508 | `								pGen->pIn++;` |
|     ! 0 |  6509 | `							}` |
|     ! 0 |  6510 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6511 | `								pGen->pIn++;` |
|     ! 0 |  6512 | `							}` |
|     ! 0 |  6513 | `							break;` |
|       - |  6514 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6515 | `					}else{` |
|       - |  6516 | `						SyString *pNameLocal;` |
|       - |  6517 | `						char *zDup;` |
|       - |  6518 | `						/* Duplicate variable name */` |
|      25 |  6519 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      25 |  6520 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      25 |  6521 | `						if( zDup ){` |
|       - |  6522 | `							/* Zero the structure */` |
|      25 |  6523 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      25 |  6524 | `							sEnv.iFlags = iFlagsLocal;` |
|      25 |  6525 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      25 |  6526 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      25 |  6527 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6528 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6529 | `									got_this = 1;` |
|     ! 0 |  6530 | `							}` |
|       - |  6531 | `							/* Save imported variable */` |
|      25 |  6532 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      15 |  6533 | `						}else{` |
|     ! 0 |  6534 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6535 | `							 return SXERR_ABORT;` |
|       - |  6536 | `						}` |
|       - |  6537 | `					}` |
|      25 |  6538 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      31 |  6539 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6540 | `						/* Ignore trailing commas */` |
|       7 |  6541 | `						pGen->pIn++;` |
|       1 |  6542 | `					}` |
|       5 |  6543 | `				}` |
|      21 |  6544 | `				if( !got_this ){` |
|       - |  6545 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6546 | `					 * available to the closure environment.` |
|       - |  6547 | `					 */` |
|      21 |  6548 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      21 |  6549 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      21 |  6550 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      21 |  6551 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      21 |  6552 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       8 |  6553 | `				}` |
|      21 |  6554 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6555 | `					/* Mark as closure */` |
|      21 |  6556 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       8 |  6557 | `				}` |
|       8 |  6558 | `		}` |
|     123 |  6559 | `	}` |
|       - |  6560 | `	/* Compile the body */` |
|   41995 |  6561 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   41995 |  6562 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6563 | `		return SXERR_ABORT;` |
|       - |  6564 | `	}` |
|   41995 |  6565 | `	if( ppFunc ){` |
|     251 |  6566 | `		*ppFunc = pFunc;` |
|     123 |  6567 | `	}` |
|   41995 |  6568 | `	rc = SXRET_OK;` |
|   41995 |  6569 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6570 | `		/* Finally register the function */` |
|   41979 |  6571 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   20987 |  6572 | `	}` |
|   41995 |  6573 | `	if( rc == SXRET_OK ){` |
|   41995 |  6574 | `		return SXRET_OK;` |
|       - |  6575 | `	}` |
|       - |  6576 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6577 | `OutOfMem:` |
|       - |  6578 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6579 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6580 | `	 */` |
|     ! 0 |  6581 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6582 | `	return SXERR_ABORT;` |
|   21005 |  6583 |  |
|       - |  6584 | `/*` |
|       - |  6585 | ` * Compile a standard PHP function.` |
|       - |  6586 | ` *  Refer to the block-comment above for more information.` |
|       - |  6587 | ` */` |
|   41760 |  6588 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6589 |  |
|       - |  6590 | `	SyString *pName;` |
|       - |  6591 | `	sxi32 iFlags;` |
|       - |  6592 | `	sxu32 nLine;` |
|       - |  6593 | `	sxi32 rc;` |
|       - |  6594 |  |
|   41765 |  6595 | `	nLine = pGen->pIn->nLine;` |
|   41765 |  6596 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   41765 |  6597 | `	iFlags = 0;` |
|   41765 |  6598 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6599 | `		/* Return by reference,remember that */` |
|       7 |  6600 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6601 | `		/* Jump the '&' token */` |
|       7 |  6602 | `		pGen->pIn++;` |
|       3 |  6603 | `	}` |
|   41765 |  6604 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6605 | `		/* Invalid function name */` |
|       6 |  6606 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       6 |  6607 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6608 | `			return SXERR_ABORT;` |
|       - |  6609 | `		}` |
|       - |  6610 | `		/* Sychronize with the next semi-colon or braces*/` |
|      18 |  6611 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      14 |  6612 | `			pGen->pIn++;` |
|       2 |  6613 | `		}` |
|       6 |  6614 | `		return SXRET_OK;` |
|       - |  6615 | `	}` |
|   41761 |  6616 | `	pName = &pGen->pIn->sData;` |
|   41761 |  6617 | `	nLine = pGen->pIn->nLine;` |
|       - |  6618 | `	/* Jump the function name */` |
|   41761 |  6619 | `	pGen->pIn++;` |
|   41761 |  6620 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6621 | `		/* Syntax error */` |
|       3 |  6622 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6623 | `		if( rc == SXERR_ABORT ){` |
|       - |  6624 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6625 | `			return SXERR_ABORT;` |
|       - |  6626 | `		}` |
|       - |  6627 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6628 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6629 | `			pGen->pIn++;` |
|     ! 0 |  6630 | `		}` |
|       3 |  6631 | `		return SXRET_OK;` |
|       - |  6632 | `	}` |
|       - |  6633 | `	/* Compile function body */` |
|   41759 |  6634 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   41759 |  6635 | `	return rc;` |
|   20885 |  6636 |  |
|       - |  6637 | `/*` |
|       - |  6638 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6639 | ` * According to the PHP language reference manual` |
|       - |  6640 | ` *  Visibility:` |
|       - |  6641 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6642 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6643 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6644 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6645 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6646 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6647 | ` */` |
|  297328 |  6648 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  6649 |  |
|  297333 |  6650 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    9533 |  6651 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  287805 |  6652 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   40973 |  6653 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6654 | `	}` |
|       - |  6655 | `	/* Assume public by default */` |
|  246837 |  6656 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  148669 |  6657 |  |
|       - |  6658 | `/*` |
|       - |  6659 | ` * Compile a class constant.` |
|       - |  6660 | ` * According to the PHP language reference manual` |
|       - |  6661 | ` *  Class Constants` |
|       - |  6662 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6663 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6664 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6665 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6666 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6667 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6668 | ` * Symisc eXtension.` |
|       - |  6669 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6670 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6671 | ` *  Example:` |
|       - |  6672 | ` *   class Test{` |
|       - |  6673 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6674 | ` *   };` |
|       - |  6675 | ` *   var_dump(TEST::MyConst);` |
|       - |  6676 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6677 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6678 | ` */` |
|      32 |  6679 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  6680 |  |
|      37 |  6681 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6682 | `	SySet *pInstrContainer;` |
|       - |  6683 | `	ph7_class_attr *pCons;` |
|       - |  6684 | `	SyString *pName;` |
|       - |  6685 | `	sxi32 rc;` |
|       - |  6686 | `	/* Extract visibility level */` |
|      37 |  6687 | `	iProtection = GetProtectionLevel(iProtection);` |
|      37 |  6688 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      16 |  6689 | `loop:` |
|       - |  6690 | `	/* Mark as constant */` |
|      37 |  6691 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      37 |  6692 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6693 | `		/* Invalid constant name */` |
|     ! 0 |  6694 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6695 | `		if( rc == SXERR_ABORT ){` |
|       - |  6696 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6697 | `			return SXERR_ABORT;` |
|       - |  6698 | `		}` |
|     ! 0 |  6699 | `		goto Synchronize;` |
|       - |  6700 | `	}` |
|       - |  6701 | `	/* Peek constant name */` |
|      37 |  6702 | `	pName = &pGen->pIn->sData;` |
|       - |  6703 | `	/* Make sure the constant name isn't reserved */` |
|      37 |  6704 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6705 | `		/* Reserved constant name */` |
|     ! 0 |  6706 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6707 | `		if( rc == SXERR_ABORT ){` |
|       - |  6708 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6709 | `			return SXERR_ABORT;` |
|       - |  6710 | `		}` |
|     ! 0 |  6711 | `		goto Synchronize;` |
|       - |  6712 | `	}` |
|       - |  6713 | `	/* Advance the stream cursor */` |
|      37 |  6714 | `	pGen->pIn++;` |
|      37 |  6715 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6716 | `		/* Invalid declaration */` |
|     ! 0 |  6717 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6718 | `		if( rc == SXERR_ABORT ){` |
|       - |  6719 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6720 | `			return SXERR_ABORT;` |
|       - |  6721 | `		}` |
|     ! 0 |  6722 | `		goto Synchronize;` |
|       - |  6723 | `	}` |
|      37 |  6724 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6725 | `	/* Allocate a new class attribute */` |
|      37 |  6726 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      37 |  6727 | `	if( pCons == 0 ){` |
|     ! 0 |  6728 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6729 | `		return SXERR_ABORT;` |
|       - |  6730 | `	}` |
|       - |  6731 | `	/* Swap bytecode container */` |
|      37 |  6732 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      37 |  6733 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6734 | `	/* Compile constant value.` |
|       - |  6735 | `	 */` |
|      37 |  6736 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      37 |  6737 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6738 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6739 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6740 | `			return SXERR_ABORT;` |
|       - |  6741 | `		}` |
|       1 |  6742 | `	}` |
|       - |  6743 | `	/* Emit the done instruction */` |
|      37 |  6744 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      37 |  6745 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      37 |  6746 | `	if( rc == SXERR_ABORT ){` |
|       - |  6747 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6748 | `		return SXERR_ABORT;` |
|       - |  6749 | `	}` |
|       - |  6750 | `	/* All done,install the constant */` |
|      37 |  6751 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      37 |  6752 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6753 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6754 | `		return SXERR_ABORT;` |
|       - |  6755 | `	}` |
|      37 |  6756 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6757 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 |  6758 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 |  6759 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6760 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6761 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6762 | `				pTok--;` |
|     ! 0 |  6763 | `			}` |
|     ! 0 |  6764 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6765 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6766 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6767 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6768 | `				return SXERR_ABORT;` |
|       - |  6769 | `			}` |
|     ! 0 |  6770 | `		}else{` |
|     ! 0 |  6771 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 |  6772 | `				goto loop;` |
|       - |  6773 | `			}` |
|       - |  6774 | `		}` |
|     ! 0 |  6775 | `	}` |
|      37 |  6776 | `	return SXRET_OK;` |
|     ! 0 |  6777 | `Synchronize:` |
|       - |  6778 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  6779 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 |  6780 | `		pGen->pIn++;` |
|     ! 0 |  6781 | `	}` |
|     ! 0 |  6782 | `	return SXERR_CORRUPT;` |
|      21 |  6783 |  |
|       - |  6784 | `/*` |
|       - |  6785 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6786 | ` * According to the PHP language reference manual` |
|       - |  6787 | ` *  Properties` |
|       - |  6788 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6789 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6790 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6791 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6792 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6793 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6794 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6795 | ` * Symisc eXtension.` |
|       - |  6796 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6797 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6798 | ` *  Example:` |
|       - |  6799 | ` *   class Test{` |
|       - |  6800 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6801 | ` *   };` |
|       - |  6802 | ` *   var_dump(TEST::myVar);` |
|       - |  6803 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6804 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6805 | ` */` |
|       - |  6806 | `/*` |
|       - |  6807 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6808 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6809 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6810 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6811 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6812 | ` */` |
|  155174 |  6813 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  6814 |  |
|  155179 |  6815 | `	SyToken *p = pStart;` |
|  155179 |  6816 | `	if( p >= pEnd ) return 0;` |
|  155179 |  6817 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 |  6818 | `		p++;` |
|      16 |  6819 | `		if( p >= pEnd ) return 0;` |
|       7 |  6820 | `	}` |
|  155179 |  6821 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6822 | `		p++;` |
|       3 |  6823 | `		if( p >= pEnd ) return 0;` |
|       1 |  6824 | `	}` |
|  155179 |  6825 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6826 | `		return 0;` |
|       - |  6827 | `	}` |
|       - |  6828 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6829 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6830 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6831 | `	 * dispatch site. */` |
|  155179 |  6832 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  155157 |  6833 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  155209 |  6834 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3320 |  6835 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  155043 |  6836 | `			return 0;` |
|       - |  6837 | `		}` |
|      57 |  6838 | `	}` |
|     141 |  6839 | `	p++;` |
|       - |  6840 | `	/* Consume optional namespace path */` |
|     143 |  6841 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6842 | `		p += 2;` |
|       1 |  6843 | `	}` |
|       - |  6844 | ``	/* Consume any `\| Type` union alternatives */`` |
|     222 |  6845 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      91 |  6846 | `		&& p->sData.zString[0] == '\|' ){` |
|      16 |  6847 | `		p++;` |
|      16 |  6848 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      16 |  6849 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      16 |  6850 | `		p++;` |
|      16 |  6851 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6852 | `			p += 2;` |
|     ! 0 |  6853 | `		}` |
|       4 |  6854 | `	}` |
|     141 |  6855 | `	if( p >= pEnd ) return 0;` |
|     141 |  6856 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   77592 |  6857 |  |
|       - |  6858 |  |
|       - |  6859 | `/*` |
|       - |  6860 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6861 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6862 | ` * if not). Recognized forms:` |
|       - |  6863 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6864 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6865 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6866 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6867 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6868 | ` * on unrecoverable error.` |
|       - |  6869 | ` *` |
|       - |  6870 | ` * When a type is parsed:` |
|       - |  6871 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  6872 | ` *   *pClass is set to the class name (for class types)` |
|       - |  6873 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  6874 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  6875 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  6876 | ` */` |
|     136 |  6877 | `static sxi32 GenStateParsePropertyType(` |
|       - |  6878 | `	ph7_gen_state *pGen,` |
|       - |  6879 | `	sxu32 *pnType,` |
|       - |  6880 | `	SyString *pClass,` |
|       - |  6881 | `	sxi32 *piTypeFlags,` |
|       - |  6882 | `	SyString *pTypeText,` |
|       - |  6883 | `	SySet *pAlts` |
|       5 |  6884 | `){` |
|     141 |  6885 | `	sxi32 iFlags = 0;` |
|       - |  6886 | `	sxi32 rc;` |
|     141 |  6887 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6888 | `		return SXRET_OK;` |
|       - |  6889 | `	}` |
|       - |  6890 | `	/* If the first token is '$', there's no type */` |
|     141 |  6891 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  6892 | `		return SXRET_OK;` |
|       - |  6893 | `	}` |
|     141 |  6894 | `	rc = GenStateParseUnionTypeDecl(` |
|      68 |  6895 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  6896 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  6897 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  6898 | `		/* bAllowVoid */ 0,` |
|     136 |  6899 | `		pGen->pIn->nLine);` |
|     141 |  6900 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6901 | `		return rc;` |
|       - |  6902 | `	}` |
|       - |  6903 | `	/* Verify next token is '$' (start of property name) */` |
|     141 |  6904 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6905 | `		return SXERR_SYNTAX;` |
|       - |  6906 | `	}` |
|     141 |  6907 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     141 |  6908 | `	return SXRET_OK;` |
|      73 |  6909 |  |
|       - |  6910 |  |
|       - |  6911 | `/*` |
|       - |  6912 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  6913 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  6914 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  6915 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  6916 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  6917 | ` * by the type parser itself before reaching here.` |
|       - |  6918 | ` *` |
|       - |  6919 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  6920 | ` * use in the error message.` |
|       - |  6921 | ` */` |
|     202 |  6922 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  6923 | `	sxu32 nType,` |
|       - |  6924 | `	const SyString *pClass,` |
|       - |  6925 | `	const char **pzName,` |
|       - |  6926 | `	sxu32 *pnName)` |
|       5 |  6927 |  |
|       - |  6928 | `	const char *z;` |
|       - |  6929 | `	sxu32 n;` |
|     207 |  6930 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     173 |  6931 | `		return 0;` |
|       - |  6932 | `	}` |
|      38 |  6933 | `	z = pClass->zString;` |
|      38 |  6934 | `	n = pClass->nByte;` |
|      38 |  6935 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       6 |  6936 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  6937 | `	}` |
|       - |  6938 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  6939 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  6940 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      33 |  6941 | `	return 0;` |
|     106 |  6942 |  |
|       - |  6943 |  |
|       - |  6944 | `/*` |
|       - |  6945 | ` * Validate a parsed property type (main atom + any union alternatives)` |
|       - |  6946 | ` * against the disallowed-pseudo-types list. Emits a PHP-compatible` |
|       - |  6947 | ` * "Property C::$x cannot have type T" error on rejection, where T is` |
|       - |  6948 | ` * the full canonical type text (matching PHP's error wording for` |
|       - |  6949 | `` * unions like `callable\|int`).`` |
|       - |  6950 | ` *` |
|       - |  6951 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  6952 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  6953 | ` */` |
|     174 |  6954 | `static sxi32 GenStateValidatePropertyType(` |
|       - |  6955 | `	ph7_gen_state *pGen,` |
|       - |  6956 | `	ph7_class *pClass,` |
|       - |  6957 | `	const SyString *pPropName,` |
|       - |  6958 | `	sxu32 nType,` |
|       - |  6959 | `	const SyString *pTypeClass,` |
|       - |  6960 | `	const SyString *pTypeText,` |
|       - |  6961 | `	SySet *pUnionAlts,` |
|       - |  6962 | `	sxu32 nLine)` |
|       5 |  6963 |  |
|     179 |  6964 | `	const char *zBad = 0;` |
|     179 |  6965 | `	sxu32 nBad = 0;` |
|       - |  6966 | `	SyString sFallback;` |
|       - |  6967 | `	const SyString *pBad;` |
|       - |  6968 | `	sxi32 rc;` |
|     179 |  6969 | `	int bDisallowed = 0;` |
|     179 |  6970 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       3 |  6971 | `		bDisallowed = 1;` |
|     178 |  6972 | `	}else if( pUnionAlts ){` |
|       - |  6973 | `		sxu32 i;` |
|      44 |  6974 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      32 |  6975 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      32 |  6976 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  6977 | `				bDisallowed = 1;` |
|       3 |  6978 | `				break;` |
|       - |  6979 | `			}` |
|      17 |  6980 | `		}` |
|       7 |  6981 | `	}` |
|     179 |  6982 | `	if( !bDisallowed ){` |
|     175 |  6983 | `		return SXRET_OK;` |
|       - |  6984 | `	}` |
|       - |  6985 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  6986 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  6987 | `	 * canonical spelling if the type text is unavailable. */` |
|       6 |  6988 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       6 |  6989 | `		pBad = pTypeText;` |
|       4 |  6990 | `	}else{` |
|     ! 0 |  6991 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  6992 | `		pBad = &sFallback;` |
|       - |  6993 | `	}` |
|       8 |  6994 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6995 | `		"Property %z::$%z cannot have type %z",` |
|       2 |  6996 | `		&pClass->sName,pPropName,pBad);` |
|       6 |  6997 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6998 | `		return SXERR_ABORT;` |
|       - |  6999 | `	}` |
|       6 |  7000 | `	return SXERR_SYNTAX;` |
|      92 |  7001 |  |
|       - |  7002 |  |
|   60324 |  7003 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7004 |  |
|   60329 |  7005 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7006 | `	ph7_class_attr *pAttr;` |
|       - |  7007 | `	SyString *pName;` |
|       - |  7008 | `	sxi32 rc;` |
|   60329 |  7009 | `	sxu32 nType = 0;` |
|       - |  7010 | `	SyString sTypeClass;` |
|       - |  7011 | `	SyString sTypeText;` |
|       - |  7012 | `	SySet aUnionAlts;` |
|   60329 |  7013 | `	sxi32 iTypeFlags = 0;` |
|   60329 |  7014 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   60329 |  7015 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   60329 |  7016 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7017 | `	/* Extract visibility level */` |
|   60329 |  7018 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7019 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   60397 |  7020 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     141 |  7021 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     141 |  7022 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7023 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7024 | `			goto Synchronize;` |
|     141 |  7025 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7026 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7027 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7028 | `				&pGen->pIn->sData);` |
|     ! 0 |  7029 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7030 | `				return SXERR_ABORT;` |
|       - |  7031 | `			}` |
|     ! 0 |  7032 | `			goto Synchronize;` |
|     141 |  7033 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7034 | `			return SXERR_ABORT;` |
|       - |  7035 | `		}` |
|      68 |  7036 | `	}` |
|     ! 0 |  7037 | `loop:` |
|   60333 |  7038 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7039 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7040 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7041 | `			return SXERR_ABORT;` |
|       - |  7042 | `		}` |
|     ! 0 |  7043 | `		goto Synchronize;` |
|       - |  7044 | `	}` |
|   60333 |  7045 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   60333 |  7046 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7047 | `		/* Invalid attribute name */` |
|     ! 0 |  7048 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7049 | `		if( rc == SXERR_ABORT ){` |
|       - |  7050 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7051 | `			return SXERR_ABORT;` |
|       - |  7052 | `		}` |
|     ! 0 |  7053 | `		goto Synchronize;` |
|       - |  7054 | `	}` |
|       - |  7055 | `	/* Peek attribute name */` |
|   60333 |  7056 | `	pName = &pGen->pIn->sData;` |
|       - |  7057 | `	/* Advance the stream cursor */` |
|   60333 |  7058 | `	pGen->pIn++;` |
|   60333 |  7059 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7060 | `		/* Invalid declaration */` |
|       3 |  7061 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7062 | `		if( rc == SXERR_ABORT ){` |
|       - |  7063 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7064 | `			return SXERR_ABORT;` |
|       - |  7065 | `		}` |
|       3 |  7066 | `		goto Synchronize;` |
|       - |  7067 | `	}` |
|       - |  7068 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7069 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7070 | `	 * by the type parser. */` |
|   60331 |  7071 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     215 |  7072 | `		rc = GenStateValidatePropertyType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7073 | `			&sTypeText,` |
|     140 |  7074 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,nLine);` |
|     145 |  7075 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7076 | `			return SXERR_ABORT;` |
|     145 |  7077 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7078 | `			goto Synchronize;` |
|       - |  7079 | `		}` |
|      70 |  7080 | `	}` |
|       - |  7081 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   60331 |  7082 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7083 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7084 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7085 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7086 | `			return SXERR_ABORT;` |
|       - |  7087 | `		}` |
|       3 |  7088 | `		goto Synchronize;` |
|       - |  7089 | `	}` |
|       - |  7090 | `	/* Allocate a new class attribute */` |
|   60329 |  7091 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   60329 |  7092 | `	if( pAttr == 0 ){` |
|     ! 0 |  7093 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7094 | `		return SXERR_ABORT;` |
|       - |  7095 | `	}` |
|   60329 |  7096 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     143 |  7097 | `		pAttr->nType = nType;` |
|     143 |  7098 | `		pAttr->sClass = sTypeClass;` |
|     143 |  7099 | `		pAttr->sTypeName = sTypeText;` |
|     143 |  7100 | `		if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7101 | `			/* Copy the parsed alternatives into the attribute. The class-name` |
|       - |  7102 | `			 * SyStrings inside each ph7_type_alt point to memory owned by the` |
|       - |  7103 | `			 * VM allocator (SyMemBackendStrDup'd in GenStateParseUnionTypeDecl),` |
|       - |  7104 | `			 * so it's safe for multiple attrs in a multi-decl chain to share` |
|       - |  7105 | `			 * the same backing strings — they outlive the temporary set. */` |
|       - |  7106 | `			sxu32 i;` |
|      34 |  7107 | `			for( i = 0; i < SySetUsed(&aUnionAlts); i++ ){` |
|      24 |  7108 | `				ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&aUnionAlts, i);` |
|      24 |  7109 | `				SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      14 |  7110 | `			}` |
|       5 |  7111 | `		}` |
|      69 |  7112 | `	}` |
|   60329 |  7113 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7114 | `		SySet *pInstrContainer;` |
|   19287 |  7115 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7116 | `		/* Swap bytecode container */` |
|   19287 |  7117 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   19287 |  7118 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7119 | `		/* Compile attribute value.` |
|       - |  7120 | `		 */` |
|   19287 |  7121 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   19287 |  7122 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7123 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7124 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7125 | `				return SXERR_ABORT;` |
|       - |  7126 | `			}` |
|     ! 0 |  7127 | `		}` |
|       - |  7128 | `		/* Emit the done instruction */` |
|   19287 |  7129 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   19287 |  7130 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    9641 |  7131 | `	}` |
|       - |  7132 | `	/* All done,install the attribute */` |
|   60329 |  7133 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   60329 |  7134 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7135 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7136 | `		return SXERR_ABORT;` |
|       - |  7137 | `	}` |
|   60329 |  7138 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7139 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7140 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7141 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7142 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7143 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7144 | `				pTok--;` |
|     ! 0 |  7145 | `			}` |
|     ! 0 |  7146 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7147 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7148 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7149 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7150 | `				return SXERR_ABORT;` |
|       - |  7151 | `			}` |
|     ! 0 |  7152 | `		}else{` |
|       5 |  7153 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7154 | `				goto loop;` |
|       - |  7155 | `			}` |
|       - |  7156 | `		}` |
|     ! 0 |  7157 | `	}` |
|   60325 |  7158 | `	SySetRelease(&aUnionAlts);` |
|   60325 |  7159 | `	return SXRET_OK;` |
|       2 |  7160 | `Synchronize:` |
|       - |  7161 | `	/* Synchronize with the first semi-colon */` |
|      12 |  7162 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       8 |  7163 | `		pGen->pIn++;` |
|       2 |  7164 | `	}` |
|       6 |  7165 | `	SySetRelease(&aUnionAlts);` |
|       6 |  7166 | `	return SXERR_CORRUPT;` |
|   30167 |  7167 |  |
|       - |  7168 | `/*` |
|       - |  7169 | ` * Compile a class method.` |
|       - |  7170 | ` *` |
|       - |  7171 | ` * Refer to the official documentation for more information` |
|       - |  7172 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7173 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7174 | ` * overloading and many more.` |
|       - |  7175 | ` */` |
|  236972 |  7176 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7177 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7178 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7179 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7180 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7181 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7182 | `	)` |
|       5 |  7183 |  |
|  236977 |  7184 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7185 | `	ph7_class_method *pMeth;` |
|       - |  7186 | `	sxi32 iFuncFlags;` |
|       - |  7187 | `	SyString *pName;` |
|       - |  7188 | `	SyToken *pEnd;` |
|       - |  7189 | `	sxi32 rc;` |
|       - |  7190 | `	/* Extract visibility level */` |
|  236977 |  7191 | `	iProtection = GetProtectionLevel(iProtection);` |
|  236977 |  7192 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  236977 |  7193 | `	iFuncFlags = 0;` |
|  236977 |  7194 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7195 | `		/* Invalid method name */` |
|     ! 0 |  7196 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7197 | `		if( rc == SXERR_ABORT ){` |
|       - |  7198 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7199 | `			return SXERR_ABORT;` |
|       - |  7200 | `		}` |
|     ! 0 |  7201 | `		goto Synchronize;` |
|       - |  7202 | `	}` |
|  236977 |  7203 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7204 | `		/* Return by reference,remember that */` |
|     ! 0 |  7205 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7206 | `		/* Jump the '&' token */` |
|     ! 0 |  7207 | `		pGen->pIn++;` |
|     ! 0 |  7208 | `	}` |
|  236977 |  7209 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7210 | `		/* Invalid method name */` |
|     ! 0 |  7211 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7212 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7213 | `			return SXERR_ABORT;` |
|       - |  7214 | `		}` |
|     ! 0 |  7215 | `		goto Synchronize;` |
|       - |  7216 | `	}` |
|       - |  7217 | `	/* Peek method name */` |
|  236977 |  7218 | `	pName = &pGen->pIn->sData;` |
|  236977 |  7219 | `	nLine = pGen->pIn->nLine;` |
|       - |  7220 | `	/* Jump the method name */` |
|  236977 |  7221 | `	pGen->pIn++;` |
|  236977 |  7222 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7223 | `		/* Abstract method */` |
|   81905 |  7224 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7225 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7226 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7227 | `				&pClass->sName,pName);` |
|     ! 0 |  7228 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7229 | `				return SXERR_ABORT;` |
|       - |  7230 | `			}` |
|     ! 0 |  7231 | `		}` |
|       - |  7232 | `		/* Assemble method signature only */` |
|   81905 |  7233 | `		doBody = FALSE;` |
|   40950 |  7234 | `	}` |
|  236977 |  7235 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7236 | `		/* Syntax error */` |
|     ! 0 |  7237 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7238 | `		if( rc == SXERR_ABORT ){` |
|       - |  7239 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7240 | `			return SXERR_ABORT;` |
|       - |  7241 | `		}` |
|     ! 0 |  7242 | `		goto Synchronize;` |
|       - |  7243 | `	}` |
|       - |  7244 | `	/* Allocate a new class_method instance */` |
|  236977 |  7245 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  236977 |  7246 | `	if( pMeth == 0 ){` |
|     ! 0 |  7247 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7248 | `		return SXERR_ABORT;` |
|       - |  7249 | `	}` |
|       - |  7250 | `	/* Jump the left parenthesis '(' */` |
|  236977 |  7251 | `	pGen->pIn++;` |
|  236977 |  7252 | `	pEnd = 0; /* cc warning */` |
|       - |  7253 | `	/* Delimit the method signature */` |
|  236977 |  7254 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  236977 |  7255 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7256 | `		/* Syntax error */` |
|       3 |  7257 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7258 | `		if( rc == SXERR_ABORT ){` |
|       - |  7259 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7260 | `			return SXERR_ABORT;` |
|       - |  7261 | `		}` |
|       3 |  7262 | `		goto Synchronize;` |
|       - |  7263 | `	}` |
|       - |  7264 | `	{` |
|  236975 |  7265 | `		int bIsCtor = 0;` |
|  236975 |  7266 | `		int bAbstractCtor = 0;` |
|  345958 |  7267 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  140608 |  7268 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  227478 |  7269 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   18999 |  7270 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7271 | `				bAbstractCtor = 1;` |
|       2 |  7272 | `			}else{` |
|   18997 |  7273 | `				bIsCtor = 1;` |
|       - |  7274 | `			}` |
|    9497 |  7275 | `		}` |
|  236975 |  7276 | `		if( pGen->pIn < pEnd ){` |
|       - |  7277 | `			/* Collect method arguments */` |
|   53857 |  7278 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   53857 |  7279 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7280 | `				return SXERR_ABORT;` |
|       - |  7281 | `			}` |
|   26926 |  7282 | `		}` |
|       - |  7283 | `	}` |
|       - |  7284 | `	/* Point past ')' and parse optional return type ': type' */` |
|  236975 |  7285 | `	pGen->pIn = &pEnd[1];` |
|       - |  7286 | `	{` |
|  236975 |  7287 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  236975 |  7288 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7289 | `			return SXERR_ABORT;` |
|  236975 |  7290 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7291 | `			goto Synchronize;` |
|       - |  7292 | `		}` |
|       - |  7293 | `	}` |
|       - |  7294 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7295 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7296 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7297 | `	{` |
|  236975 |  7298 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7299 | `		sxu32 i;` |
|  322397 |  7300 | `		for( i = 0; i < nArg; i++ ){` |
|   85435 |  7301 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7302 | `			ph7_class_attr *pAttr;` |
|   85435 |  7303 | `			sxi32 iAttrFlags = 0;` |
|   85435 |  7304 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   85397 |  7305 | `				continue;` |
|       - |  7306 | `			}` |
|      43 |  7307 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7308 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7309 | `					"Cannot declare variadic promoted property");` |
|       3 |  7310 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7311 | `					return SXERR_ABORT;` |
|       - |  7312 | `				}` |
|       3 |  7313 | `				goto Synchronize;` |
|       - |  7314 | `			}` |
|       - |  7315 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7316 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7317 | `			 * appear as an alternative of a union type. */` |
|      36 |  7318 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      11 |  7319 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      56 |  7320 | `				rc = GenStateValidatePropertyType(pGen,pClass,&pArg->sName,` |
|      34 |  7321 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      34 |  7322 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      17 |  7323 | `					nLine);` |
|      39 |  7324 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7325 | `					return SXERR_ABORT;` |
|      39 |  7326 | `				}else if( rc != SXRET_OK ){` |
|       6 |  7327 | `					goto Synchronize;` |
|       - |  7328 | `				}` |
|      15 |  7329 | `			}` |
|       - |  7330 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      36 |  7331 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7332 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7333 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7334 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7335 | `					return SXERR_ABORT;` |
|       - |  7336 | `				}` |
|       3 |  7337 | `				goto Synchronize;` |
|       - |  7338 | `			}` |
|      33 |  7339 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      29 |  7340 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      13 |  7341 | `			}` |
|      33 |  7342 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7343 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7344 | `			}` |
|      33 |  7345 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7346 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7347 | `			}` |
|      33 |  7348 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      33 |  7349 | `			if( pAttr == 0 ){` |
|     ! 0 |  7350 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7351 | `				return SXERR_ABORT;` |
|       - |  7352 | `			}` |
|      33 |  7353 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      29 |  7354 | `				pAttr->nType = pArg->nType;` |
|      29 |  7355 | `				pAttr->sClass = pArg->sClass;` |
|      29 |  7356 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      29 |  7357 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7358 | `					sxu32 k;` |
|     ! 0 |  7359 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7360 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7361 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7362 | `					}` |
|     ! 0 |  7363 | `				}` |
|      13 |  7364 | `			}` |
|      33 |  7365 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      33 |  7366 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7367 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7368 | `				return SXERR_ABORT;` |
|       - |  7369 | `			}` |
|      18 |  7370 | `		}` |
|       - |  7371 | `	}` |
|  236967 |  7372 | `	if( doBody ){` |
|       - |  7373 | `		/* Compile method body */` |
|  155067 |  7374 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  155067 |  7375 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7376 | `			return SXERR_ABORT;` |
|       - |  7377 | `		}` |
|   77536 |  7378 | `	}else{` |
|       - |  7379 | `		/* Only method signature is allowed */` |
|   81905 |  7380 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7381 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7382 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7383 | `				if( rc == SXERR_ABORT ){` |
|       - |  7384 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7385 | `					return SXERR_ABORT;` |
|       - |  7386 | `				}` |
|     ! 0 |  7387 | `				return SXERR_CORRUPT;` |
|       - |  7388 | `			}` |
|       - |  7389 | `	}` |
|       - |  7390 | `	/* All done,install the method */` |
|  236967 |  7391 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  236967 |  7392 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7393 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7394 | `		return SXERR_ABORT;` |
|       - |  7395 | `	}` |
|  236967 |  7396 | `	return SXRET_OK;` |
|       5 |  7397 | `Synchronize:` |
|       - |  7398 | `	/* Synchronize with the first semi-colon */` |
|      34 |  7399 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      24 |  7400 | `		pGen->pIn++;` |
|       4 |  7401 | `	}` |
|      14 |  7402 | `	return SXERR_CORRUPT;` |
|  118491 |  7403 |  |
|       - |  7404 | `/*` |
|       - |  7405 | ` * Compile an object interface.` |
|       - |  7406 | ` *  According to the PHP language reference manual` |
|       - |  7407 | ` *   Object Interfaces:` |
|       - |  7408 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7409 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7410 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7411 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7412 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7413 | ` */` |
|   34672 |  7414 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  7415 |  |
|   34677 |  7416 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7417 | `	ph7_class *pClass,*pBase;` |
|       - |  7418 | `	SyToken *pEnd,*pTmp;` |
|       - |  7419 | `	SyString *pName;` |
|       - |  7420 | `	sxi32 nKwrd;` |
|       - |  7421 | `	sxi32 rc;` |
|       - |  7422 | `	/* Jump the 'interface' keyword */` |
|   34677 |  7423 | `	pGen->pIn++;` |
|       - |  7424 | `	/* Extract interface name */` |
|   34677 |  7425 | `	pName = &pGen->pIn->sData;` |
|       - |  7426 | `	/* Advance the stream cursor */` |
|   34677 |  7427 | `	pGen->pIn++;` |
|       - |  7428 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7429 | `		SyBlob sFQN;` |
|       - |  7430 | `		SyString sFQNStr;` |
|   34677 |  7431 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   34677 |  7432 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   34677 |  7433 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   34677 |  7434 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   34677 |  7435 | `		SyBlobRelease(&sFQN);` |
|       - |  7436 | `	}` |
|   34677 |  7437 | `	if( pClass == 0 ){` |
|     ! 0 |  7438 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7439 | `		return SXERR_ABORT;` |
|       - |  7440 | `	}` |
|       - |  7441 | `	/* Mark as an interface */` |
|   34677 |  7442 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7443 | `	/* Assume no base class is given */` |
|   34677 |  7444 | `	pBase = 0;` |
|   34677 |  7445 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    9457 |  7446 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    9457 |  7447 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7448 | `			SyBlob sResolved;` |
|       - |  7449 | `			SyString sBaseName;` |
|       - |  7450 | `			sxu32 nRefLine;` |
|       - |  7451 | `			/* Extract base interface */` |
|    9457 |  7452 | `			pGen->pIn++;` |
|    9457 |  7453 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    9457 |  7454 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    9457 |  7455 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7456 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7457 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7458 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7459 | `					pName);` |
|     ! 0 |  7460 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7461 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7462 | `					return SXERR_ABORT;` |
|       - |  7463 | `				}` |
|     ! 0 |  7464 | `				return SXRET_OK;` |
|       - |  7465 | `			}` |
|   14183 |  7466 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    9452 |  7467 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    9457 |  7468 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7469 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7470 | `			/* Only interfaces is allowed */` |
|    9457 |  7471 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7472 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7473 | `			}` |
|    9457 |  7474 | `			if( pBase == 0 ){` |
|     ! 0 |  7475 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7476 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  7477 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7478 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7479 | `					return SXERR_ABORT;` |
|       - |  7480 | `				}` |
|     ! 0 |  7481 | `			}` |
|    9457 |  7482 | `			SyBlobRelease(&sResolved);` |
|    4726 |  7483 | `		}` |
|    4726 |  7484 | `	}` |
|   34677 |  7485 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7486 | `		/* Syntax error */` |
|     ! 0 |  7487 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7488 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7489 | `		if( rc == SXERR_ABORT ){` |
|       - |  7490 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7491 | `			return SXERR_ABORT;` |
|       - |  7492 | `		}` |
|     ! 0 |  7493 | `		return SXRET_OK;` |
|       - |  7494 | `	}` |
|   34677 |  7495 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   34677 |  7496 | `	pEnd = 0; /* cc warning */` |
|       - |  7497 | `	/* Delimit the interface body */` |
|   34677 |  7498 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   34677 |  7499 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7500 | `		/* Syntax error */` |
|     ! 0 |  7501 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7502 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7503 | `		if( rc == SXERR_ABORT ){` |
|       - |  7504 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7505 | `			return SXERR_ABORT;` |
|       - |  7506 | `		}` |
|     ! 0 |  7507 | `		return SXRET_OK;` |
|       - |  7508 | `	}` |
|       - |  7509 | `	/* Swap token stream */` |
|   34677 |  7510 | `	pTmp = pGen->pEnd;` |
|   34677 |  7511 | `	pGen->pEnd = pEnd;` |
|       - |  7512 | `	/* Start the parse process` |
|       - |  7513 | `	 * Note (According to the PHP reference manual):` |
|       - |  7514 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7515 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7516 | `	 */` |
|   58280 |  7517 | `	for(;;){` |
|       - |  7518 | `		/* Jump leading/trailing semi-colons */` |
|  198453 |  7519 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   81893 |  7520 | `			pGen->pIn++;` |
|       5 |  7521 | `		}` |
|  116565 |  7522 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7523 | `			/* End of interface body */` |
|   34675 |  7524 | `			break;` |
|       - |  7525 | `		}` |
|   81895 |  7526 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7527 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7528 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7529 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7530 | `			if( rc == SXERR_ABORT ){` |
|       - |  7531 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7532 | `				return SXERR_ABORT;` |
|       - |  7533 | `			}` |
|     ! 0 |  7534 | `			goto done;` |
|       - |  7535 | `		}` |
|       - |  7536 | `		/* Extract the current keyword */` |
|   81895 |  7537 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   81895 |  7538 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7539 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7540 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7541 | `			const char *zKind = "member";` |
|       3 |  7542 | `			SyString *pMemberName = 0;` |
|       3 |  7543 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7544 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7545 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7546 | `					zKind = "constant";` |
|       3 |  7547 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7548 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7549 | `					}` |
|       1 |  7550 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7551 | `					zKind = "method";` |
|     ! 0 |  7552 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7553 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7554 | `					}` |
|     ! 0 |  7555 | `				}` |
|       1 |  7556 | `			}` |
|       3 |  7557 | `			if( pMemberName ){` |
|       4 |  7558 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7559 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7560 | `			}else{` |
|     ! 0 |  7561 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7562 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7563 | `			}` |
|       3 |  7564 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7565 | `				return SXERR_ABORT;` |
|       - |  7566 | `			}` |
|       3 |  7567 | `			goto done;` |
|       - |  7568 | `		}` |
|   81893 |  7569 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7570 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7571 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7572 | `			if( rc == SXERR_ABORT ){` |
|       - |  7573 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7574 | `				return SXERR_ABORT;` |
|       - |  7575 | `			}` |
|     ! 0 |  7576 | `			goto done;` |
|       - |  7577 | `		}` |
|   81893 |  7578 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7579 | `			/* Advance the stream cursor */` |
|   81889 |  7580 | `			pGen->pIn++;` |
|   81889 |  7581 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7582 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7583 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7584 | `				if( rc == SXERR_ABORT ){` |
|       - |  7585 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7586 | `					return SXERR_ABORT;` |
|       - |  7587 | `				}` |
|     ! 0 |  7588 | `				goto done;` |
|       - |  7589 | `			}` |
|   81889 |  7590 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   81889 |  7591 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7592 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7593 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7594 | `				if( rc == SXERR_ABORT ){` |
|       - |  7595 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7596 | `					return SXERR_ABORT;` |
|       - |  7597 | `				}` |
|     ! 0 |  7598 | `				goto done;` |
|       - |  7599 | `			}` |
|   40942 |  7600 | `		}` |
|   81893 |  7601 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7602 | `			/* Parse constant */` |
|       3 |  7603 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 |  7604 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7605 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7606 | `					return SXERR_ABORT;` |
|       - |  7607 | `				}` |
|     ! 0 |  7608 | `				goto done;` |
|       - |  7609 | `			}` |
|       2 |  7610 | `		}else{` |
|   81891 |  7611 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   81891 |  7612 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7613 | `				/* Static method,record that */` |
|    9449 |  7614 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7615 | `				/* Advance the stream cursor */` |
|    9449 |  7616 | `				pGen->pIn++;` |
|    9444 |  7617 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    9449 |  7618 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7619 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7620 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7621 | `						if( rc == SXERR_ABORT ){` |
|       - |  7622 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7623 | `							return SXERR_ABORT;` |
|       - |  7624 | `						}` |
|     ! 0 |  7625 | `						goto done;` |
|       - |  7626 | `				}` |
|    4722 |  7627 | `			}` |
|       - |  7628 | `			/* Process method signature (no body for interface methods) */` |
|   81891 |  7629 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   81891 |  7630 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7631 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7632 | `					return SXERR_ABORT;` |
|       - |  7633 | `				}` |
|     ! 0 |  7634 | `				goto done;` |
|       - |  7635 | `			}` |
|       - |  7636 | `		}` |
|       5 |  7637 | `	}` |
|       - |  7638 | `	/* Install the interface */` |
|   34675 |  7639 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   34675 |  7640 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7641 | `		/* Inherit from the base interface */` |
|    9457 |  7642 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    4726 |  7643 | `	}` |
|   34675 |  7644 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7645 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7646 | `		return SXERR_ABORT;` |
|       - |  7647 | `	}` |
|   17335 |  7648 | `done:` |
|       - |  7649 | `	/* Point beyond the interface body */` |
|   34677 |  7650 | `	pGen->pIn  = &pEnd[1];` |
|   34677 |  7651 | `	pGen->pEnd = pTmp;` |
|   34677 |  7652 | `	return PH7_OK;` |
|   17341 |  7653 |  |
|       - |  7654 | `/*` |
|       - |  7655 | ` * Compile a user-defined class.` |
|       - |  7656 | ` * According to the PHP language reference manual` |
|       - |  7657 | ` *  class` |
|       - |  7658 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7659 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7660 | ` *  of the properties and methods belonging to the class.` |
|       - |  7661 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7662 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7663 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7664 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7665 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7666 | ` *  (called "methods").` |
|       - |  7667 | ` */` |
|       - |  7668 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7669 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7670 | `struct TraitUseEntry {` |
|       - |  7671 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7672 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7673 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7674 | `};` |
|       - |  7675 | `/*` |
|       - |  7676 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7677 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7678 | ` */` |
|   85932 |  7679 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  7680 |  |
|       - |  7681 | `	ph7_class **apIface;` |
|       - |  7682 | `	sxu32 nIface,i;` |
|       - |  7683 | `	sxi32 rc;` |
|   85937 |  7684 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7685 | `		return SXRET_OK;` |
|       - |  7686 | `	}` |
|   85937 |  7687 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   85937 |  7688 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  171125 |  7689 | `	for(i = 0; i < nIface; i++){` |
|   85193 |  7690 | `		ph7_class *pIface = apIface[i];` |
|       - |  7691 | `		SyHashEntry *pEntry;` |
|   85193 |  7692 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  227255 |  7693 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  142067 |  7694 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7695 | `			ph7_class_method *pImplMeth;` |
|  142067 |  7696 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7697 | `			/* Find the implementing method in the class */` |
|  142067 |  7698 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  142067 |  7699 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  7700 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  7701 | `			}` |
|       - |  7702 | `			/* Check visibility: interface methods must be implemented as public */` |
|  142053 |  7703 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  7704 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7705 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  7706 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  7707 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7708 | `					return SXERR_ABORT;` |
|       - |  7709 | `				}` |
|       1 |  7710 | `			}` |
|       - |  7711 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7712 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7713 | `			 */` |
|       - |  7714 | `			{` |
|  142053 |  7715 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  142053 |  7716 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  142053 |  7717 | `				int sigError = 0;` |
|  142053 |  7718 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7719 | `					sigError = 1;` |
|  142052 |  7720 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7721 | `					/* Extra parameters must all have default values */` |
|       6 |  7722 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7723 | `					sxu32 k;` |
|       8 |  7724 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  7725 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7726 | `							sigError = 1;` |
|       3 |  7727 | `							break;` |
|       - |  7728 | `						}` |
|       2 |  7729 | `					}` |
|       2 |  7730 | `				}` |
|  142053 |  7731 | `				if( sigError ){` |
|       - |  7732 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7733 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7734 | `					sxu32 j;` |
|       6 |  7735 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  7736 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7737 | `					/* Build implementing method signature */` |
|       6 |  7738 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  7739 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  7740 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  7741 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  7742 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  7743 | `					}` |
|       - |  7744 | `					/* Build interface method signature */` |
|       6 |  7745 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  7746 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  7747 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  7748 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  7749 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  7750 | `					}` |
|       8 |  7751 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7752 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7753 | `						&pClass->sName,pMName,` |
|       4 |  7754 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7755 | `						&pIface->sName,pMName,` |
|       4 |  7756 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  7757 | `					SyBlobRelease(&sImplSig);` |
|       6 |  7758 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  7759 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7760 | `						return SXERR_ABORT;` |
|       - |  7761 | `					}` |
|       2 |  7762 | `				}` |
|       - |  7763 | `			}` |
|       5 |  7764 | `		}` |
|   42599 |  7765 | `	}` |
|   85937 |  7766 | `	return SXRET_OK;` |
|   42971 |  7767 |  |
|       - |  7768 | `/*` |
|       - |  7769 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7770 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7771 | ` */` |
|   85932 |  7772 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  7773 |  |
|       - |  7774 | `	ph7_class_method *pMeth;` |
|       - |  7775 | `	SyHashEntry *pEntry;` |
|       - |  7776 | `	sxu32 nAbstract;` |
|       - |  7777 | `	SyBlob sMsg;` |
|       - |  7778 | `	sxi32 rc;` |
|       - |  7779 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   85937 |  7780 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      27 |  7781 | `		return SXRET_OK;` |
|       - |  7782 | `	}` |
|       - |  7783 | `	/* Count abstract methods */` |
|   85915 |  7784 | `	nAbstract = 0;` |
|   85915 |  7785 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  833377 |  7786 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  747467 |  7787 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  747467 |  7788 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  7789 | `			nAbstract++;` |
|       8 |  7790 | `		}` |
|       5 |  7791 | `	}` |
|   85915 |  7792 | `	if( nAbstract == 0 ){` |
|   85901 |  7793 | `		return SXRET_OK;` |
|       - |  7794 | `	}` |
|       - |  7795 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  7796 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  7797 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7798 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7799 | `		&pClass->sName,nAbstract,` |
|       7 |  7800 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7801 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7802 | `	/* Second pass: list methods with origins */` |
|       - |  7803 | `	{` |
|      18 |  7804 | `		sxu32 nListed = 0;` |
|      18 |  7805 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  7806 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  7807 | `			ph7_class *pOrigin = 0;` |
|       - |  7808 | `			SyString *pMName;` |
|      22 |  7809 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  7810 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7811 | `				continue;` |
|       - |  7812 | `			}` |
|      20 |  7813 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  7814 | `			if( nListed > 0 ){` |
|       3 |  7815 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7816 | `			}` |
|       - |  7817 | `			/* Find the origin of this abstract method.` |
|       - |  7818 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7819 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7820 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7821 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7822 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7823 | `			 * class's namespace.` |
|       - |  7824 | `			 */` |
|       - |  7825 | `			{` |
|       - |  7826 | `				ph7_class **apIface;` |
|       - |  7827 | `				ph7_class **apTrait;` |
|       - |  7828 | `				ph7_class *pWalk;` |
|       - |  7829 | `				sxu32 i;` |
|       - |  7830 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7831 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  7832 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  7833 | `				 */` |
|      20 |  7834 | `				if( pClass->pBase ){` |
|      11 |  7835 | `					pWalk = pClass->pBase;` |
|      19 |  7836 | `					while( pWalk ){` |
|       - |  7837 | `						ph7_class_method *pParentMeth;` |
|      13 |  7838 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  7839 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  7840 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  7841 | `							 * in this class's ancestor chain.` |
|       - |  7842 | `							 */` |
|      13 |  7843 | `							int fromIface = 0;` |
|      13 |  7844 | `							ph7_class *pAnc = pWalk;` |
|      17 |  7845 | `							while( pAnc ){` |
|       - |  7846 | `								ph7_class **apPI;` |
|       - |  7847 | `								sxu32 j;` |
|      15 |  7848 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  7849 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  7850 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  7851 | `										fromIface = 1;` |
|      10 |  7852 | `										break;` |
|       - |  7853 | `									}` |
|     ! 0 |  7854 | `								}` |
|      15 |  7855 | `								if( fromIface ) break;` |
|       6 |  7856 | `								pAnc = pAnc->pBase;` |
|       2 |  7857 | `							}` |
|      13 |  7858 | `							if( !fromIface ){` |
|       3 |  7859 | `								pOrigin = pWalk;` |
|       3 |  7860 | `								break;` |
|       - |  7861 | `							}` |
|       4 |  7862 | `						}` |
|      10 |  7863 | `						pWalk = pWalk->pBase;` |
|       2 |  7864 | `					}` |
|       4 |  7865 | `				}` |
|       - |  7866 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  7867 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  7868 | `				 */` |
|      20 |  7869 | `				if( !pOrigin ){` |
|      18 |  7870 | `					pWalk = pClass;` |
|      40 |  7871 | `					while( pWalk && !pOrigin ){` |
|      26 |  7872 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  7873 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  7874 | `							ph7_class *pIface = apIface[i];` |
|      16 |  7875 | `							ph7_class *pDeepest = 0;` |
|      28 |  7876 | `							while( pIface ){` |
|      16 |  7877 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  7878 | `									pDeepest = pIface;` |
|       6 |  7879 | `								}` |
|      16 |  7880 | `								pIface = pIface->pBase;` |
|       4 |  7881 | `							}` |
|      16 |  7882 | `							if( pDeepest ){` |
|      16 |  7883 | `								pOrigin = pDeepest;` |
|      16 |  7884 | `								break;` |
|       - |  7885 | `							}` |
|     ! 0 |  7886 | `						}` |
|      26 |  7887 | `						pWalk = pWalk->pBase;` |
|       4 |  7888 | `					}` |
|       7 |  7889 | `				}` |
|       - |  7890 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  7891 | `				if( !pOrigin ){` |
|       3 |  7892 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  7893 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  7894 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  7895 | `							pOrigin = pClass;` |
|       3 |  7896 | `							break;` |
|       - |  7897 | `						}` |
|     ! 0 |  7898 | `					}` |
|       1 |  7899 | `				}` |
|       - |  7900 | `			}` |
|      20 |  7901 | `			if( pOrigin ){` |
|      20 |  7902 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  7903 | `			}else{` |
|       - |  7904 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  7905 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  7906 | `			}` |
|      20 |  7907 | `			nListed++;` |
|       4 |  7908 | `		}` |
|       - |  7909 | `	}` |
|      18 |  7910 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  7911 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  7912 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  7913 | `	SyBlobRelease(&sMsg);` |
|      18 |  7914 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7915 | `		return SXERR_ABORT;` |
|       - |  7916 | `	}` |
|      18 |  7917 | `	return SXRET_OK;` |
|   42971 |  7918 |  |
|       - |  7919 | `/*` |
|       - |  7920 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  7921 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  7922 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  7923 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  7924 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  7925 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  7926 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  7927 | ` */` |
|   85700 |  7928 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  7929 |  |
|   85705 |  7930 | `	int isAbsolute = 0;` |
|   85705 |  7931 | `	SyToken *pStart = pGen->pIn;` |
|       - |  7932 | `	SyBlob sName;` |
|   85705 |  7933 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      33 |  7934 | `		isAbsolute = 1;` |
|      33 |  7935 | `		pGen->pIn++;` |
|      15 |  7936 | `	}` |
|   85705 |  7937 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       9 |  7938 | `		pGen->pIn = pStart;` |
|       9 |  7939 | `		return SXERR_INVALID;` |
|       - |  7940 | `	}` |
|   85699 |  7941 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   85699 |  7942 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   85699 |  7943 | `	pGen->pIn++;` |
|  128559 |  7944 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   42870 |  7945 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  7946 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  7947 | `		pGen->pIn++;` |
|      13 |  7948 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  7949 | `		pGen->pIn++;` |
|       1 |  7950 | `	}` |
|   85699 |  7951 | `	if( isAbsolute ){` |
|      30 |  7952 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      16 |  7953 | `	}else{` |
|       - |  7954 | `		SyString sRaw;` |
|   85671 |  7955 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   85671 |  7956 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  7957 | `	}` |
|   85699 |  7958 | `	SyBlobRelease(&sName);` |
|   85699 |  7959 | `	return SXRET_OK;` |
|   42855 |  7960 |  |
|       - |  7961 | `/*` |
|       - |  7962 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  7963 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  7964 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  7965 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  7966 | ` * either direction cannot run unbounded.` |
|       - |  7967 | ` */` |
|       - |  7968 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    9562 |  7969 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  7970 |  |
|       - |  7971 | `	ph7_class **apParent;` |
|       - |  7972 | `	sxu32 n;` |
|   16003 |  7973 | `	while( pInterface ){` |
|   12747 |  7974 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  7975 | `			return FALSE;` |
|       - |  7976 | `		}` |
|   15907 |  7977 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    6320 |  7978 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    6311 |  7979 | `			return TRUE;` |
|       - |  7980 | `		}` |
|    6441 |  7981 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    6441 |  7982 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  7983 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  7984 | `				return TRUE;` |
|       - |  7985 | `			}` |
|     ! 0 |  7986 | `		}` |
|    6441 |  7987 | `		pInterface = pInterface->pBase;` |
|    6441 |  7988 | `		iDepth++;` |
|       5 |  7989 | `	}` |
|    3261 |  7990 | `	return FALSE;` |
|    4786 |  7991 |  |
|    9562 |  7992 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  7993 |  |
|    9567 |  7994 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  7995 |  |
|       - |  7996 | `/*` |
|       - |  7997 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  7998 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  7999 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8000 | ` */` |
|    6306 |  8001 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8002 |  |
|    6315 |  8003 | `	while( pBase ){` |
|      10 |  8004 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8005 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8006 | `			return TRUE;` |
|       - |  8007 | `		}` |
|      10 |  8008 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8009 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8010 | `			return TRUE;` |
|       - |  8011 | `		}` |
|       5 |  8012 | `		pBase = pBase->pBase;` |
|       1 |  8013 | `	}` |
|    6307 |  8014 | `	return FALSE;` |
|    3158 |  8015 |  |
|   85948 |  8016 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  8017 |  |
|   85953 |  8018 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8019 | `	ph7_class *pClass,*pBase;` |
|       - |  8020 | `	SyToken *pEnd,*pTmp;` |
|       - |  8021 | `	sxi32 iProtection;` |
|       - |  8022 | `	SySet aInterfaces;` |
|       - |  8023 | `	SySet aUseEntries;` |
|       - |  8024 | `	sxi32 iAttrflags;` |
|       - |  8025 | `	SyString *pName;` |
|       - |  8026 | `	sxi32 nKwrd;` |
|       - |  8027 | `	sxi32 rc;` |
|       - |  8028 | `	/* Jump the 'class' keyword */` |
|   85953 |  8029 | `	pGen->pIn++;` |
|   85953 |  8030 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8031 | `		/* Syntax error */` |
|     ! 0 |  8032 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8033 | `		if( rc == SXERR_ABORT ){` |
|       - |  8034 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8035 | `			return SXERR_ABORT;` |
|       - |  8036 | `		}` |
|       - |  8037 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8038 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8039 | `			pGen->pIn++;` |
|     ! 0 |  8040 | `		}` |
|     ! 0 |  8041 | `		return SXRET_OK;` |
|       - |  8042 | `	}` |
|       - |  8043 | `	/* Extract class name */` |
|   85953 |  8044 | `	pName = &pGen->pIn->sData;` |
|       - |  8045 | `	/* Advance the stream cursor */` |
|   85953 |  8046 | `	pGen->pIn++;` |
|       - |  8047 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8048 | `		SyBlob sFQN;` |
|       - |  8049 | `		SyString sFQNStr;` |
|   85953 |  8050 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   85953 |  8051 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   85953 |  8052 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   85953 |  8053 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   85953 |  8054 | `		SyBlobRelease(&sFQN);` |
|       - |  8055 | `	}` |
|   85953 |  8056 | `	if( pClass == 0 ){` |
|     ! 0 |  8057 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8058 | `		return SXERR_ABORT;` |
|       - |  8059 | `	}` |
|       - |  8060 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   85953 |  8061 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   85953 |  8062 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8063 | `	/* Assume a standalone class */` |
|   85953 |  8064 | `	pBase = 0;` |
|   85953 |  8065 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   75813 |  8066 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   75813 |  8067 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8068 | `			SyBlob sResolved;` |
|       - |  8069 | `			SyString sBaseName;` |
|       - |  8070 | `			sxu32 nRefLine;` |
|   66257 |  8071 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   66257 |  8072 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   66257 |  8073 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   66257 |  8074 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8075 | `				SyBlobRelease(&sResolved);` |
|       4 |  8076 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8077 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8078 | `					pName);` |
|       3 |  8079 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8080 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8081 | `					return SXERR_ABORT;` |
|       - |  8082 | `				}` |
|       3 |  8083 | `				return SXRET_OK;` |
|       - |  8084 | `			}` |
|   99380 |  8085 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   66250 |  8086 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   66255 |  8087 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8088 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8089 | `			/* Interfaces are not allowed */` |
|   66255 |  8090 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8091 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8092 | `			}` |
|   66255 |  8093 | `			if( pBase == 0 ){` |
|     ! 0 |  8094 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8095 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8096 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8097 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8098 | `					return SXERR_ABORT;` |
|       - |  8099 | `				}` |
|     ! 0 |  8100 | `			}else{` |
|   66255 |  8101 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8102 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8103 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8104 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8105 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8106 | `						return SXERR_ABORT;` |
|       - |  8107 | `					}` |
|     ! 0 |  8108 | `				}` |
|       - |  8109 | `			}` |
|   66255 |  8110 | `			SyBlobRelease(&sResolved);` |
|   33125 |  8111 | `		}` |
|   75811 |  8112 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8113 | `			ph7_class *pInterface;` |
|       - |  8114 | `			/* Interface implementation */` |
|    9567 |  8115 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    4781 |  8116 | `			for(;;){` |
|       - |  8117 | `				SyBlob sResolved;` |
|       - |  8118 | `				SyString sIntName;` |
|       - |  8119 | `				sxu32 nRefLine;` |
|    9567 |  8120 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    9567 |  8121 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    9567 |  8122 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8123 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8124 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8125 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8126 | `						pName);` |
|     ! 0 |  8127 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8128 | `						return SXERR_ABORT;` |
|       - |  8129 | `					}` |
|     ! 0 |  8130 | `					break;` |
|       - |  8131 | `				}` |
|   19129 |  8132 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    9562 |  8133 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    9567 |  8134 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8135 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8136 | `				/* Only interfaces are allowed */` |
|    9567 |  8137 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8138 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8139 | `				}` |
|    9567 |  8140 | `				if( pInterface == 0 ){` |
|     ! 0 |  8141 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8142 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8143 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8144 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8145 | `						return SXERR_ABORT;` |
|       - |  8146 | `					}` |
|     ! 0 |  8147 | `				}else{` |
|       - |  8148 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8149 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8150 | `					 * unless they already extend Exception or Error.` |
|       - |  8151 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8152 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8153 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    9567 |  8154 | `					SyString *pFqn = &pClass->sName;` |
|    9567 |  8155 | `					int bIsExceptionOrError =` |
|    7931 |  8156 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   15919 |  8157 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    7993 |  8158 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3158 |  8159 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   15866 |  8160 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    9462 |  8161 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3151 |  8162 | `						!bIsExceptionOrError ){` |
|      12 |  8163 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8164 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8165 | `							&pClass->sName);` |
|       9 |  8166 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8167 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8168 | `							return SXERR_ABORT;` |
|       - |  8169 | `						}` |
|       - |  8170 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8171 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8172 | `					}else{` |
|    9561 |  8173 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8174 | `					}` |
|       - |  8175 | `				}` |
|    9567 |  8176 | `				SyBlobRelease(&sResolved);` |
|    9567 |  8177 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    4786 |  8178 | `					break;` |
|       - |  8179 | `				}` |
|     ! 0 |  8180 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  8181 | `			}` |
|    4781 |  8182 | `		}` |
|   37903 |  8183 | `	}` |
|   85951 |  8184 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8185 | `		/* Syntax error */` |
|     ! 0 |  8186 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8187 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8188 | `		if( rc == SXERR_ABORT ){` |
|       - |  8189 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8190 | `			return SXERR_ABORT;` |
|       - |  8191 | `		}` |
|     ! 0 |  8192 | `		return SXRET_OK;` |
|       - |  8193 | `	}` |
|   85951 |  8194 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   85951 |  8195 | `	pEnd = 0; /* cc warning */` |
|       - |  8196 | `	/* Delimit the class body */` |
|   85951 |  8197 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   85951 |  8198 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8199 | `		/* Syntax error */` |
|     ! 0 |  8200 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8201 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8202 | `		if( rc == SXERR_ABORT ){` |
|       - |  8203 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8204 | `			return SXERR_ABORT;` |
|       - |  8205 | `		}` |
|     ! 0 |  8206 | `		return SXRET_OK;` |
|       - |  8207 | `	}` |
|       - |  8208 | `	/* Swap token stream */` |
|   85951 |  8209 | `	pTmp = pGen->pEnd;` |
|   85951 |  8210 | `	pGen->pEnd = pEnd;` |
|       - |  8211 | `	/* Set the inherited flags */` |
|   85951 |  8212 | `	pClass->iFlags = iFlags;` |
|       - |  8213 | `	/* Start the parse process */` |
|  120501 |  8214 | `	for(;;){` |
|       - |  8215 | `		/* Jump leading/trailing semi-colons */` |
|  361729 |  8216 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   60385 |  8217 | `			pGen->pIn++;` |
|       5 |  8218 | `		}` |
|  301349 |  8219 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8220 | `			/* End of class body */` |
|   85937 |  8221 | `			break;` |
|       - |  8222 | `		}` |
|  215417 |  8223 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8224 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8225 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8226 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8227 | `			if( rc == SXERR_ABORT ){` |
|       - |  8228 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8229 | `				return SXERR_ABORT;` |
|       - |  8230 | `			}` |
|     ! 0 |  8231 | `			goto done;` |
|       - |  8232 | `		}` |
|       - |  8233 | `		/* Assume public visibility */` |
|  215417 |  8234 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  215417 |  8235 | `		iAttrflags = 0;` |
|  215417 |  8236 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  8237 | `			/* Extract the current keyword */` |
|  215417 |  8238 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  215417 |  8239 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8240 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8241 | `				TraitUseEntry sUse;` |
|      49 |  8242 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      49 |  8243 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      49 |  8244 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      30 |  8245 | `				for(;;){` |
|       - |  8246 | `					ph7_class *pTrait;` |
|       - |  8247 | `					SyString *pTraitName;` |
|      57 |  8248 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8249 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8250 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8251 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8252 | `							return SXERR_ABORT;` |
|       - |  8253 | `						}` |
|     ! 0 |  8254 | `						break;` |
|       - |  8255 | `					}` |
|      57 |  8256 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8257 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8258 | `						SyBlob sResolved;` |
|      57 |  8259 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      57 |  8260 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     109 |  8261 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      52 |  8262 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      57 |  8263 | `						SyBlobRelease(&sResolved);` |
|       - |  8264 | `					}` |
|       - |  8265 | `					/* Only traits are allowed */` |
|      57 |  8266 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8267 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8268 | `					}` |
|      57 |  8269 | `					if( pTrait == 0 ){` |
|     ! 0 |  8270 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8271 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8272 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8273 | `							return SXERR_ABORT;` |
|       - |  8274 | `						}` |
|     ! 0 |  8275 | `					}else{` |
|      57 |  8276 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8277 | `					}` |
|      57 |  8278 | `					pGen->pIn++; /* Advance past trait name */` |
|      57 |  8279 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      27 |  8280 | `						break;` |
|       - |  8281 | `					}` |
|      10 |  8282 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  8283 | `				}` |
|       - |  8284 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      49 |  8285 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8286 | `					SyToken *pBlock;` |
|      10 |  8287 | `					pGen->pIn++; /* Jump '{' */` |
|      10 |  8288 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      10 |  8289 | `					sUse.pResolvStart = pGen->pIn;` |
|      10 |  8290 | `					sUse.pResolvEnd = pBlock;` |
|      10 |  8291 | `					if( pBlock < pGen->pEnd ){` |
|      10 |  8292 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       6 |  8293 | `					}else{` |
|     ! 0 |  8294 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8295 | `					}` |
|       4 |  8296 | `				}` |
|      49 |  8297 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8298 | `				/* The semicolon will be consumed by the outer loop */` |
|      49 |  8299 | `				continue;` |
|       - |  8300 | `			}` |
|  215373 |  8301 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  212101 |  8302 | `				iProtection = nKwrd;` |
|  212101 |  8303 | `				pGen->pIn++; /* Jump the visibility token */` |
|  212096 |  8304 | `				if( pGen->pIn >= pGen->pEnd` |
|  212101 |  8305 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8306 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8307 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8308 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8309 | `					if( rc == SXERR_ABORT ){` |
|       - |  8310 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8311 | `						return SXERR_ABORT;` |
|       - |  8312 | `					}` |
|     ! 0 |  8313 | `					goto done;` |
|       - |  8314 | `				}` |
|  212101 |  8315 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8316 | `					/* Attribute declaration (untyped) */` |
|   60171 |  8317 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   60171 |  8318 | `					if( rc != SXRET_OK ){` |
|       3 |  8319 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8320 | `							return SXERR_ABORT;` |
|       - |  8321 | `						}` |
|       3 |  8322 | `						goto done;` |
|       - |  8323 | `					}` |
|   60169 |  8324 | `					continue;` |
|       - |  8325 | `				}` |
|  151935 |  8326 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8327 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     127 |  8328 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     127 |  8329 | `					if( rc != SXRET_OK ){` |
|       3 |  8330 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8331 | `							return SXERR_ABORT;` |
|       - |  8332 | `						}` |
|       3 |  8333 | `						goto done;` |
|       - |  8334 | `					}` |
|     125 |  8335 | `					continue;` |
|       - |  8336 | `				}` |
|       - |  8337 | `				/* Extract the keyword */` |
|  151813 |  8338 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   75904 |  8339 | `			}` |
|  155085 |  8340 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8341 | `				/* Process constant declaration */` |
|      35 |  8342 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      35 |  8343 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8344 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8345 | `						return SXERR_ABORT;` |
|       - |  8346 | `					}` |
|     ! 0 |  8347 | `					goto done;` |
|       - |  8348 | `				}` |
|      20 |  8349 | `			}else{` |
|  155055 |  8350 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8351 | `					/* Static method or attribute,record that */` |
|    3193 |  8352 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    3193 |  8353 | `					pGen->pIn++; /* Jump the static keyword */` |
|    3193 |  8354 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8355 | `						/* Extract the keyword */` |
|    3187 |  8356 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    3187 |  8357 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8358 | `							iProtection = nKwrd;` |
|     ! 0 |  8359 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8360 | `						}` |
|    1591 |  8361 | `					}` |
|    3188 |  8362 | `					if( pGen->pIn >= pGen->pEnd` |
|    3193 |  8363 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8364 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8365 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8366 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8367 | `						if( rc == SXERR_ABORT ){` |
|       - |  8368 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8369 | `							return SXERR_ABORT;` |
|       - |  8370 | `						}` |
|     ! 0 |  8371 | `						goto done;` |
|       - |  8372 | `					}` |
|    3193 |  8373 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8374 | `						/* Attribute declaration */` |
|       5 |  8375 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8376 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8377 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8378 | `								return SXERR_ABORT;` |
|       - |  8379 | `							}` |
|     ! 0 |  8380 | `							goto done;` |
|       - |  8381 | `						}` |
|       5 |  8382 | `						continue;` |
|       - |  8383 | `					}` |
|    3189 |  8384 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8385 | `						/* Typed static attribute declaration */` |
|      13 |  8386 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      13 |  8387 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8388 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8389 | `								return SXERR_ABORT;` |
|       - |  8390 | `							}` |
|     ! 0 |  8391 | `							goto done;` |
|       - |  8392 | `						}` |
|      13 |  8393 | `						continue;` |
|       - |  8394 | `					}` |
|       - |  8395 | `					/* Extract the keyword */` |
|    3179 |  8396 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  153454 |  8397 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8398 | `					/* Abstract method,record that */` |
|      12 |  8399 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8400 | `					/* Mark the whole class as abstract */` |
|      12 |  8401 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8402 | `					/* Advance the stream cursor */` |
|      12 |  8403 | `					pGen->pIn++;` |
|      12 |  8404 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8405 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8406 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8407 | `							iProtection = nKwrd;` |
|      10 |  8408 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8409 | `						}` |
|       5 |  8410 | `					}` |
|      12 |  8411 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8412 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8413 | `							/* Static method */` |
|     ! 0 |  8414 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8415 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8416 | `					}` |
|      12 |  8417 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8418 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8419 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8420 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8421 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8422 | `							if( rc == SXERR_ABORT ){` |
|       - |  8423 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8424 | `								return SXERR_ABORT;` |
|       - |  8425 | `							}` |
|     ! 0 |  8426 | `							goto done;` |
|       - |  8427 | `					}` |
|      12 |  8428 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  151862 |  8429 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8430 | `					/* final method ,record that */` |
|       6 |  8431 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       6 |  8432 | `					pGen->pIn++; /* Jump the final keyword */` |
|       6 |  8433 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8434 | `						/* Extract the keyword */` |
|       6 |  8435 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  8436 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  8437 | `							iProtection = nKwrd;` |
|       6 |  8438 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 |  8439 | `						}` |
|       2 |  8440 | `					}` |
|       6 |  8441 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8442 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8443 | `							/* Static method */` |
|     ! 0 |  8444 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8445 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8446 | `					}` |
|       6 |  8447 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8448 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8449 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8450 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8451 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8452 | `							if( rc == SXERR_ABORT ){` |
|       - |  8453 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8454 | `								return SXERR_ABORT;` |
|       - |  8455 | `							}` |
|     ! 0 |  8456 | `							goto done;` |
|       - |  8457 | `					}` |
|       6 |  8458 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8459 | `				}` |
|  155041 |  8460 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8461 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8462 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8463 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8464 | `						if( rc == SXERR_ABORT ){` |
|       - |  8465 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8466 | `							return SXERR_ABORT;` |
|       - |  8467 | `						}` |
|     ! 0 |  8468 | `						goto done;` |
|       - |  8469 | `				}` |
|  155041 |  8470 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8471 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8472 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8473 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8474 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8475 | `						if( rc == SXERR_ABORT ){` |
|       - |  8476 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8477 | `							return SXERR_ABORT;` |
|       - |  8478 | `						}` |
|     ! 0 |  8479 | `						goto done;` |
|       - |  8480 | `					}` |
|       - |  8481 | `					/* Attribute declaration */` |
|       7 |  8482 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8483 | `				}else{` |
|       - |  8484 | `					/* Process method declaration */` |
|  155035 |  8485 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8486 | `				}` |
|  155041 |  8487 | `				if( rc != SXRET_OK ){` |
|      14 |  8488 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8489 | `						return SXERR_ABORT;` |
|       - |  8490 | `					}` |
|      14 |  8491 | `					goto done;` |
|       - |  8492 | `				}` |
|       - |  8493 | `			}` |
|   77533 |  8494 | `		}else{` |
|       - |  8495 | `			/* Attribute declaration */` |
|     ! 0 |  8496 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8497 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8498 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8499 | `					return SXERR_ABORT;` |
|       - |  8500 | `				}` |
|     ! 0 |  8501 | `				goto done;` |
|       - |  8502 | `			}` |
|       - |  8503 | `		}` |
|       5 |  8504 | `	}` |
|       - |  8505 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8506 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8507 | `	 */` |
|       - |  8508 | `	{` |
|       - |  8509 | `		TraitUseEntry *apUse;` |
|       - |  8510 | `		sxu32 nU;` |
|   85937 |  8511 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   85981 |  8512 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      49 |  8513 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      49 |  8514 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      49 |  8515 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      49 |  8516 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8517 | `			sxu32 nT;` |
|      49 |  8518 | `			if( !hasResolution ){` |
|       - |  8519 | `				/* No conflict resolution block: use standard trait application */` |
|      83 |  8520 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      47 |  8521 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      47 |  8522 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8523 | `						break;` |
|       - |  8524 | `					}` |
|      26 |  8525 | `				}` |
|      23 |  8526 | `			}else{` |
|       - |  8527 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8528 | `				 * then use the block to resolve method conflicts.` |
|       - |  8529 | `				 */` |
|       - |  8530 | `				SyToken *pR;` |
|      20 |  8531 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      12 |  8532 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8533 | `					ph7_class_attr *pAR;` |
|       - |  8534 | `					SyHashEntry *pER;` |
|       - |  8535 | `					SyString *pNR;` |
|      12 |  8536 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      17 |  8537 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8538 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8539 | `						pNR = &pAR->sName;` |
|     ! 0 |  8540 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8541 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8542 | `						}` |
|     ! 0 |  8543 | `					}` |
|      12 |  8544 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       7 |  8545 | `				}` |
|       - |  8546 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      10 |  8547 | `				pR = pUse->pResolvStart;` |
|      22 |  8548 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8549 | `					SyString sTrait,sMethod;` |
|       - |  8550 | `					ph7_class *pSrcTrait;` |
|       - |  8551 | `					ph7_class_method *pMeth;` |
|       - |  8552 | `					sxi32 nRKwrd;` |
|      34 |  8553 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      22 |  8554 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      14 |  8555 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      14 |  8556 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      14 |  8557 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      14 |  8558 | `					sMethod = pR->sData;` |
|      14 |  8559 | `					pR++;` |
|      14 |  8560 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8561 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8562 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8563 | `							sTrait = sMethod;` |
|       7 |  8564 | `							pR++;` |
|       7 |  8565 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8566 | `							sMethod = pR->sData;` |
|       7 |  8567 | `							pR++;` |
|       3 |  8568 | `						}` |
|       3 |  8569 | `					}` |
|      14 |  8570 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8571 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8572 | `						continue;` |
|       - |  8573 | `					}` |
|      14 |  8574 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      14 |  8575 | `					pR++;` |
|      14 |  8576 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8577 | `						pSrcTrait = 0;` |
|       7 |  8578 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8579 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8580 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8581 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8582 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8583 | `								break;` |
|       - |  8584 | `							}` |
|       2 |  8585 | `						}` |
|       5 |  8586 | `						if( pSrcTrait ){` |
|       5 |  8587 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8588 | `							if( pMeth ){` |
|       5 |  8589 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8590 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8591 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8592 | `								}` |
|       2 |  8593 | `							}` |
|       2 |  8594 | `						}` |
|       2 |  8595 | `					}` |
|      30 |  8596 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       2 |  8597 | `				}` |
|       - |  8598 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      20 |  8599 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8600 | `					ph7_class_method *pMR;` |
|       - |  8601 | `					SyHashEntry *pER;` |
|       - |  8602 | `					SyString *pNR;` |
|      12 |  8603 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      35 |  8604 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      20 |  8605 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      20 |  8606 | `						pNR = &pMR->sFunc.sName;` |
|      20 |  8607 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8608 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8609 | `						}` |
|       2 |  8610 | `					}` |
|       7 |  8611 | `				}` |
|       - |  8612 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      10 |  8613 | `				pR = pUse->pResolvStart;` |
|      22 |  8614 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8615 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8616 | `					ph7_class *pSrcTrait;` |
|       - |  8617 | `					ph7_class_method *pMeth;` |
|      22 |  8618 | `					int hasQual = 0;` |
|       - |  8619 | `					sxi32 nRKwrd;` |
|      34 |  8620 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      22 |  8621 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      14 |  8622 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      14 |  8623 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      14 |  8624 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      14 |  8625 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      14 |  8626 | `					sMethod = pR->sData;` |
|      14 |  8627 | `					pR++;` |
|      14 |  8628 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8629 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8630 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8631 | `							sTrait = sMethod;` |
|       7 |  8632 | `							hasQual = 1;` |
|       7 |  8633 | `							pR++;` |
|       7 |  8634 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8635 | `							sMethod = pR->sData;` |
|       7 |  8636 | `							pR++;` |
|       3 |  8637 | `						}` |
|       3 |  8638 | `					}` |
|      14 |  8639 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8640 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8641 | `						continue;` |
|       - |  8642 | `					}` |
|      14 |  8643 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      14 |  8644 | `					pR++;` |
|      14 |  8645 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      10 |  8646 | `						sxi32 iNewVis = -1;` |
|      10 |  8647 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  8648 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  8649 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  8650 | `								iNewVis = nAK;` |
|       7 |  8651 | `								pR++;` |
|       3 |  8652 | `							}` |
|       3 |  8653 | `						}` |
|      10 |  8654 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       8 |  8655 | `							sAlias = pR->sData;` |
|       8 |  8656 | `							pR++;` |
|       3 |  8657 | `						}` |
|      10 |  8658 | `						pMeth = 0;` |
|      10 |  8659 | `						if( hasQual ){` |
|       3 |  8660 | `							pSrcTrait = 0;` |
|       5 |  8661 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  8662 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  8663 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  8664 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  8665 | `									pSrcTrait = apTrait[nT];` |
|       3 |  8666 | `									break;` |
|       - |  8667 | `								}` |
|       2 |  8668 | `							}` |
|       3 |  8669 | `							if( pSrcTrait ){` |
|       3 |  8670 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  8671 | `							}` |
|       2 |  8672 | `						}else{` |
|       7 |  8673 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  8674 | `						}` |
|      10 |  8675 | `						if( pMeth ){` |
|      10 |  8676 | `							if( sAlias.nByte > 0 ){` |
|       - |  8677 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  8678 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  8679 | `								 */` |
|       - |  8680 | `								ph7_class_method *pAlias;` |
|       - |  8681 | `								char *zAliasDup;` |
|       8 |  8682 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       8 |  8683 | `								if( pAlias ){` |
|       8 |  8684 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       8 |  8685 | `									if( iNewVis >= 0 ){` |
|       5 |  8686 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8687 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8688 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  8689 | `									}` |
|       8 |  8690 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       8 |  8691 | `									if( zAliasDup ){` |
|       8 |  8692 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  8693 | `									}` |
|       5 |  8694 | `								}` |
|       6 |  8695 | `							}else if( iNewVis >= 0 ){` |
|       - |  8696 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  8697 | `								ph7_class_method *pCopy;` |
|       3 |  8698 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  8699 | `								if( pCopy ){` |
|       3 |  8700 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  8701 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  8702 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8703 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8704 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  8705 | `									/* Replace the method in the class hash */` |
|       3 |  8706 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  8707 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  8708 | `								}` |
|       1 |  8709 | `							}` |
|       4 |  8710 | `						}` |
|       4 |  8711 | `						SXUNUSED(hasQual);` |
|       4 |  8712 | `					}` |
|      18 |  8713 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       2 |  8714 | `				}` |
|       - |  8715 | `			}` |
|      49 |  8716 | `			SySetRelease(&pUse->aTraits);` |
|      27 |  8717 | `		}` |
|       - |  8718 | `	}` |
|       - |  8719 | `	/* Install the class */` |
|   85937 |  8720 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   85937 |  8721 | `	if( rc == SXRET_OK ){` |
|       - |  8722 | `		ph7_class **apInterface;` |
|       - |  8723 | `		sxu32 n;` |
|   85937 |  8724 | `		if( pBase ){` |
|       - |  8725 | `			/* Inherit from base class and mark as a subclass */` |
|   66255 |  8726 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   33125 |  8727 | `		}` |
|   85937 |  8728 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   95493 |  8729 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  8730 | `			/* Implements one or more interface */` |
|    9561 |  8731 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    9561 |  8732 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8733 | `				break;` |
|       - |  8734 | `			}` |
|    4783 |  8735 | `		}` |
|       - |  8736 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  8737 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  128898 |  8738 | `		if( rc == SXRET_OK` |
|   85932 |  8739 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   85937 |  8740 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   75639 |  8741 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  8742 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   75639 |  8743 | `			if( pStringable ){` |
|   75639 |  8744 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   75639 |  8745 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  8746 | `				sxu32 i;` |
|   75639 |  8747 | `				int bAlready = 0;` |
|   81939 |  8748 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    6307 |  8749 | `					if( apImpl[i] == pStringable ){` |
|       3 |  8750 | `						bAlready = 1;` |
|       3 |  8751 | `						break;` |
|       - |  8752 | `					}` |
|    3155 |  8753 | `				}` |
|   75639 |  8754 | `				if( !bAlready ){` |
|   75637 |  8755 | `					PH7_ClassImplement(pClass,pStringable);` |
|   37816 |  8756 | `				}` |
|   37817 |  8757 | `			}` |
|   37817 |  8758 | `		}` |
|       - |  8759 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   85937 |  8760 | `		if( rc == SXRET_OK ){` |
|   85937 |  8761 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   85937 |  8762 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8763 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8764 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8765 | `				return SXERR_ABORT;` |
|       - |  8766 | `			}` |
|   42966 |  8767 | `		}` |
|       - |  8768 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   85937 |  8769 | `		if( rc == SXRET_OK ){` |
|   85937 |  8770 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   85937 |  8771 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8772 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8773 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8774 | `				return SXERR_ABORT;` |
|       - |  8775 | `			}` |
|   42966 |  8776 | `		}` |
|   42966 |  8777 | `	}` |
|   85937 |  8778 | `	SySetRelease(&aUseEntries);` |
|   85937 |  8779 | `	SySetRelease(&aInterfaces);` |
|   85937 |  8780 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8781 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8782 | `		return SXERR_ABORT;` |
|       - |  8783 | `	}` |
|   42966 |  8784 | `done:` |
|       - |  8785 | `	/* Point beyond the class body */` |
|   85951 |  8786 | `	pGen->pIn = &pEnd[1];` |
|   85951 |  8787 | `	pGen->pEnd = pTmp;` |
|   85951 |  8788 | `	return PH7_OK;` |
|   42979 |  8789 |  |
|       - |  8790 | `/*` |
|       - |  8791 | ` * Compile a user-defined abstract class.` |
|       - |  8792 | ` *  According to the PHP language reference manual` |
|       - |  8793 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  8794 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  8795 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  8796 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  8797 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  8798 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  8799 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  8800 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  8801 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  8802 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  8803 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  8804 | ` *   could differ.` |
|       - |  8805 | ` */` |
|      20 |  8806 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       5 |  8807 |  |
|       - |  8808 | `	sxi32 rc;` |
|      25 |  8809 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      25 |  8810 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      25 |  8811 | `	return rc;` |
|       5 |  8812 |  |
|       - |  8813 | `/*` |
|       - |  8814 | ` * Compile a user-defined final class.` |
|       - |  8815 | ` *  According to the PHP language reference manual` |
|       - |  8816 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - |  8817 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - |  8818 | ` *    final then it cannot be extended.` |
|       - |  8819 | ` */` |
|       2 |  8820 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 |  8821 |  |
|       - |  8822 | `	sxi32 rc;` |
|       3 |  8823 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 |  8824 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 |  8825 | `	return rc;` |
|       1 |  8826 |  |
|       - |  8827 | `/*` |
|       - |  8828 | ` * Compile a user-defined trait.` |
|       - |  8829 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  8830 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  8831 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  8832 | ` */` |
|      56 |  8833 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  8834 |  |
|      61 |  8835 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8836 | `	ph7_class *pClass;` |
|       - |  8837 | `	SyToken *pEnd,*pTmp;` |
|       - |  8838 | `	sxi32 iProtection;` |
|       - |  8839 | `	sxi32 iAttrflags;` |
|       - |  8840 | `	SyString *pName;` |
|       - |  8841 | `	sxi32 nKwrd;` |
|       - |  8842 | `	sxi32 rc;` |
|       - |  8843 | `	/* Jump the 'trait' keyword */` |
|      61 |  8844 | `	pGen->pIn++;` |
|      61 |  8845 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8846 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  8847 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8848 | `			return SXERR_ABORT;` |
|       - |  8849 | `		}` |
|     ! 0 |  8850 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  8851 | `			pGen->pIn++;` |
|     ! 0 |  8852 | `		}` |
|     ! 0 |  8853 | `		return SXRET_OK;` |
|       - |  8854 | `	}` |
|       - |  8855 | `	/* Extract trait name */` |
|      61 |  8856 | `	pName = &pGen->pIn->sData;` |
|      61 |  8857 | `	pGen->pIn++;` |
|       - |  8858 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8859 | `		SyBlob sFQN;` |
|       - |  8860 | `		SyString sFQNStr;` |
|      61 |  8861 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      61 |  8862 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      61 |  8863 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      61 |  8864 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      61 |  8865 | `		SyBlobRelease(&sFQN);` |
|       - |  8866 | `	}` |
|      61 |  8867 | `	if( pClass == 0 ){` |
|     ! 0 |  8868 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8869 | `		return SXERR_ABORT;` |
|       - |  8870 | `	}` |
|       - |  8871 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      61 |  8872 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  8873 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  8874 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8875 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8876 | `			return SXERR_ABORT;` |
|       - |  8877 | `		}` |
|     ! 0 |  8878 | `		return SXRET_OK;` |
|       - |  8879 | `	}` |
|      61 |  8880 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      61 |  8881 | `	pEnd = 0;` |
|      61 |  8882 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      61 |  8883 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  8884 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  8885 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8886 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8887 | `			return SXERR_ABORT;` |
|       - |  8888 | `		}` |
|     ! 0 |  8889 | `		return SXRET_OK;` |
|       - |  8890 | `	}` |
|       - |  8891 | `	/* Swap token stream */` |
|      61 |  8892 | `	pTmp = pGen->pEnd;` |
|      61 |  8893 | `	pGen->pEnd = pEnd;` |
|       - |  8894 | `	/* Mark as trait */` |
|      61 |  8895 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  8896 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      56 |  8897 | `	for(;;){` |
|     161 |  8898 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  8899 | `			pGen->pIn++;` |
|       4 |  8900 | `		}` |
|     137 |  8901 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      61 |  8902 | `			break;` |
|       - |  8903 | `		}` |
|      81 |  8904 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8905 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8906 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8907 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8908 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8909 | `				return SXERR_ABORT;` |
|       - |  8910 | `			}` |
|     ! 0 |  8911 | `			goto done;` |
|       - |  8912 | `		}` |
|      81 |  8913 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      81 |  8914 | `		iAttrflags = 0;` |
|      81 |  8915 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      81 |  8916 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      81 |  8917 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8918 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  8919 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  8920 | `				for(;;){` |
|       - |  8921 | `					ph7_class *pUsedTrait;` |
|       - |  8922 | `					SyString *pUsedName;` |
|       5 |  8923 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8924 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8925 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  8926 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8927 | `							return SXERR_ABORT;` |
|       - |  8928 | `						}` |
|     ! 0 |  8929 | `						break;` |
|       - |  8930 | `					}` |
|       5 |  8931 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  8932 | `					{` |
|       - |  8933 | `						SyBlob sResolved;` |
|       5 |  8934 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  8935 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  8936 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  8937 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  8938 | `						SyBlobRelease(&sResolved);` |
|       - |  8939 | `					}` |
|       5 |  8940 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8941 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  8942 | `					}` |
|       5 |  8943 | `					if( pUsedTrait == 0 ){` |
|       4 |  8944 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  8945 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  8946 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8947 | `							return SXERR_ABORT;` |
|       - |  8948 | `						}` |
|       2 |  8949 | `					}else{` |
|       3 |  8950 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  8951 | `					}` |
|       5 |  8952 | `					pGen->pIn++;` |
|       5 |  8953 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  8954 | `						break;` |
|       - |  8955 | `					}` |
|     ! 0 |  8956 | `					pGen->pIn++;` |
|     ! 0 |  8957 | `				}` |
|       5 |  8958 | `				continue;` |
|       - |  8959 | `			}` |
|      77 |  8960 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  8961 | `				iProtection = nKwrd;` |
|      73 |  8962 | `				pGen->pIn++;` |
|      68 |  8963 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  8964 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8965 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8966 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8967 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8968 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8969 | `						return SXERR_ABORT;` |
|       - |  8970 | `					}` |
|     ! 0 |  8971 | `					goto done;` |
|       - |  8972 | `				}` |
|      73 |  8973 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  8974 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  8975 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8976 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8977 | `							return SXERR_ABORT;` |
|       - |  8978 | `						}` |
|     ! 0 |  8979 | `						goto done;` |
|       - |  8980 | `					}` |
|      12 |  8981 | `					continue;` |
|       - |  8982 | `				}` |
|      63 |  8983 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  8984 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8985 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8986 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8987 | `							return SXERR_ABORT;` |
|       - |  8988 | `						}` |
|     ! 0 |  8989 | `						goto done;` |
|       - |  8990 | `					}` |
|       5 |  8991 | `					continue;` |
|       - |  8992 | `				}` |
|      58 |  8993 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  8994 | `			}` |
|      62 |  8995 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  8996 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8997 | `					"Traits cannot have constants");` |
|     ! 0 |  8998 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8999 | `					return SXERR_ABORT;` |
|       - |  9000 | `				}` |
|     ! 0 |  9001 | `				goto done;` |
|     ! 0 |  9002 | `			}else{` |
|      62 |  9003 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9004 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9005 | `					pGen->pIn++;` |
|       5 |  9006 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9007 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9008 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9009 | `							iProtection = nKwrd;` |
|     ! 0 |  9010 | `							pGen->pIn++;` |
|     ! 0 |  9011 | `						}` |
|       1 |  9012 | `					}` |
|       4 |  9013 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9014 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  9015 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9016 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9017 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9018 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9019 | `							return SXERR_ABORT;` |
|       - |  9020 | `						}` |
|     ! 0 |  9021 | `						goto done;` |
|       - |  9022 | `					}` |
|       5 |  9023 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9024 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9025 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9026 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9027 | `								return SXERR_ABORT;` |
|       - |  9028 | `							}` |
|     ! 0 |  9029 | `							goto done;` |
|       - |  9030 | `						}` |
|       3 |  9031 | `						continue;` |
|       - |  9032 | `					}` |
|       3 |  9033 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9034 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9035 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9036 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9037 | `								return SXERR_ABORT;` |
|       - |  9038 | `							}` |
|     ! 0 |  9039 | `							goto done;` |
|       - |  9040 | `						}` |
|     ! 0 |  9041 | `						continue;` |
|       - |  9042 | `					}` |
|       3 |  9043 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      59 |  9044 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 |  9045 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 |  9046 | `					pGen->pIn++;` |
|       6 |  9047 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 |  9048 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  9049 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  9050 | `							iProtection = nKwrd;` |
|       6 |  9051 | `							pGen->pIn++;` |
|       2 |  9052 | `						}` |
|       2 |  9053 | `					}` |
|       6 |  9054 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9055 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9056 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9057 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9058 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9059 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9060 | `							return SXERR_ABORT;` |
|       - |  9061 | `						}` |
|     ! 0 |  9062 | `						goto done;` |
|       - |  9063 | `					}` |
|       6 |  9064 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9065 | `				}` |
|      60 |  9066 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9067 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9068 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9069 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9070 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9071 | `						return SXERR_ABORT;` |
|       - |  9072 | `					}` |
|     ! 0 |  9073 | `					goto done;` |
|       - |  9074 | `				}` |
|      60 |  9075 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9076 | `					pGen->pIn++;` |
|     ! 0 |  9077 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9078 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9079 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9080 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9081 | `							return SXERR_ABORT;` |
|       - |  9082 | `						}` |
|     ! 0 |  9083 | `						goto done;` |
|       - |  9084 | `					}` |
|     ! 0 |  9085 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9086 | `				}else{` |
|      60 |  9087 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9088 | `				}` |
|      60 |  9089 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9090 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9091 | `						return SXERR_ABORT;` |
|       - |  9092 | `					}` |
|     ! 0 |  9093 | `					goto done;` |
|       - |  9094 | `				}` |
|       - |  9095 | `			}` |
|      32 |  9096 | `		}else{` |
|     ! 0 |  9097 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9098 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9099 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9100 | `					return SXERR_ABORT;` |
|       - |  9101 | `				}` |
|     ! 0 |  9102 | `				goto done;` |
|       - |  9103 | `			}` |
|       - |  9104 | `		}` |
|       4 |  9105 | `	}` |
|       - |  9106 | `	/* Install the trait */` |
|      61 |  9107 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      61 |  9108 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9109 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9110 | `		return SXERR_ABORT;` |
|       - |  9111 | `	}` |
|      28 |  9112 | `done:` |
|       - |  9113 | `	/* Point beyond the trait body */` |
|      61 |  9114 | `	pGen->pIn = &pEnd[1];` |
|      61 |  9115 | `	pGen->pEnd = pTmp;` |
|      61 |  9116 | `	return PH7_OK;` |
|      33 |  9117 |  |
|       - |  9118 | `/*` |
|       - |  9119 | ` * Compile a user-defined class.` |
|       - |  9120 | ` *  According to the PHP language reference manual` |
|       - |  9121 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9122 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9123 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9124 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9125 | ` *   and functions (called "methods").` |
|       - |  9126 | ` */` |
|   85926 |  9127 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 |  9128 |  |
|       - |  9129 | `	sxi32 rc;` |
|   85931 |  9130 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   85931 |  9131 | `	return rc;` |
|       5 |  9132 |  |
|       - |  9133 | `/*` |
|       - |  9134 | ` * Exception handling.` |
|       - |  9135 | ` *  According to the PHP language reference manual` |
|       - |  9136 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9137 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9138 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9139 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9140 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9141 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9142 | ` *    (or re-thrown) within a catch block.` |
|       - |  9143 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9144 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9145 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9146 | ` *    been defined with set_exception_handler().` |
|       - |  9147 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9148 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9149 | ` */` |
|       - |  9150 | `/*` |
|       - |  9151 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9152 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9153 | ` * indicates failure.` |
|       - |  9154 | ` */` |
|    9654 |  9155 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  9156 |  |
|    9659 |  9157 | `	sxi32 rc = SXRET_OK;` |
|    9659 |  9158 | `	if( pRoot->pOp ){` |
|    9651 |  9159 | `		switch( pRoot->pOp->iOp ){` |
|    4823 |  9160 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9161 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9162 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9163 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9164 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9165 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    9651 |  9166 | `			break;` |
|     ! 0 |  9167 | `		default:` |
|       - |  9168 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9169 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9170 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9171 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9172 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9173 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9174 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9175 | `			}` |
|     ! 0 |  9176 | `			break;` |
|       - |  9177 | `		}` |
|    4836 |  9178 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9179 | `		/* Unexpected expression */` |
|     ! 0 |  9180 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9181 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9182 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9183 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9184 | `		}` |
|     ! 0 |  9185 | `	}` |
|    9659 |  9186 | `	return rc;` |
|       5 |  9187 |  |
|       - |  9188 | `/*` |
|       - |  9189 | ` * Compile a 'throw' statement.` |
|       - |  9190 | ` * throw: This is how you trigger an exception.` |
|       - |  9191 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9192 | ` */` |
|    9618 |  9193 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 |  9194 |  |
|    9623 |  9195 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9196 | `	GenBlock *pBlock;` |
|       - |  9197 | `	sxu32 nIdx;` |
|       - |  9198 | `	sxi32 rc;` |
|    9623 |  9199 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9200 | `	/* Compile the expression */` |
|    9623 |  9201 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    9623 |  9202 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9203 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9204 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9205 | `			return SXERR_ABORT;` |
|       - |  9206 | `		}` |
|     ! 0 |  9207 | `		return SXRET_OK;` |
|       - |  9208 | `	}` |
|    9623 |  9209 | `	pBlock = pGen->pCurrent;` |
|       - |  9210 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   44425 |  9211 | `	while(pBlock->pParent){` |
|   44421 |  9212 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    9619 |  9213 | `			break;` |
|       - |  9214 | `		}` |
|       - |  9215 | `		/* Point to the parent block */` |
|   34807 |  9216 | `		pBlock = pBlock->pParent;` |
|       5 |  9217 | `	}` |
|       - |  9218 | `	/* Emit the throw instruction */` |
|    9623 |  9219 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9220 | `	/* Emit the jump */` |
|    9623 |  9221 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    9623 |  9222 | `	return SXRET_OK;` |
|    4814 |  9223 |  |
|       - |  9224 | `/*` |
|       - |  9225 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9226 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9227 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9228 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9229 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9230 | ` */` |
|      36 |  9231 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9232 |  |
|      38 |  9233 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9234 | `	GenBlock *pBlock;` |
|       - |  9235 | `	sxu32 nIdx;` |
|       - |  9236 | `	sxi32 rc;` |
|      18 |  9237 | `	(void)iCompileFlag;` |
|      38 |  9238 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9239 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9240 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9241 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9242 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9243 | `			return SXERR_ABORT;` |
|       - |  9244 | `		}` |
|     ! 0 |  9245 | `		return SXRET_OK;` |
|       - |  9246 | `	}` |
|      38 |  9247 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9248 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9249 | `		return SXERR_ABORT;` |
|       - |  9250 | `	}` |
|      38 |  9251 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9252 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9253 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9254 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9255 | `			return SXERR_ABORT;` |
|       - |  9256 | `		}` |
|     ! 0 |  9257 | `		return SXRET_OK;` |
|       - |  9258 | `	}` |
|       - |  9259 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  9260 | `	pBlock = pGen->pCurrent;` |
|      60 |  9261 | `	while( pBlock->pParent ){` |
|      49 |  9262 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  9263 | `			break;` |
|       - |  9264 | `		}` |
|      23 |  9265 | `		pBlock = pBlock->pParent;` |
|       1 |  9266 | `	}` |
|      38 |  9267 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 |  9268 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 |  9269 | `	return SXRET_OK;` |
|      20 |  9270 |  |
|       - |  9271 | `/*` |
|       - |  9272 | ` * Compile a 'catch' block.` |
|       - |  9273 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  9274 | ` * an object containing the exception information.` |
|       - |  9275 | ` */` |
|     408 |  9276 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 |  9277 |  |
|     413 |  9278 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9279 | `	ph7_exception_block sCatch;` |
|       - |  9280 | `	SySet *pInstrContainer;` |
|       - |  9281 | `	SyString sClassName;` |
|       - |  9282 | `	GenBlock *pCatch;` |
|       - |  9283 | `	SyToken *pToken;` |
|       - |  9284 | `	SyString *pName;` |
|       - |  9285 | `	char *zDup;` |
|       - |  9286 | `	sxi32 rc;` |
|     413 |  9287 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  9288 | `	/* Zero the structure */` |
|     413 |  9289 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  9290 | `	/* Initialize fields */` |
|     413 |  9291 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     413 |  9292 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     413 |  9293 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  9294 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9295 | `			pToken = pGen->pIn;` |
|     ! 0 |  9296 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9297 | `				pToken--;` |
|     ! 0 |  9298 | `			}` |
|     ! 0 |  9299 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9300 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9301 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9302 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9303 | `				return SXERR_ABORT;` |
|       - |  9304 | `			}` |
|     ! 0 |  9305 | `			return SXERR_INVALID;` |
|       - |  9306 | `	}` |
|       - |  9307 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     413 |  9308 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     217 |  9309 | `	for(;;){` |
|       - |  9310 | `		SyBlob sResolved;` |
|     439 |  9311 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     439 |  9312 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 |  9313 | `			SyBlobRelease(&sResolved);` |
|       6 |  9314 | `			pToken = pGen->pIn;` |
|       6 |  9315 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9316 | `				pToken--;` |
|     ! 0 |  9317 | `			}` |
|       8 |  9318 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9319 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  9320 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 |  9321 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9322 | `				return SXERR_ABORT;` |
|       - |  9323 | `			}` |
|       6 |  9324 | `			return SXERR_INVALID;` |
|       - |  9325 | `		}` |
|       - |  9326 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - |  9327 | `		 * transient SyBlob allocation. */` |
|     650 |  9328 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     430 |  9329 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     435 |  9330 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     435 |  9331 | `		SyBlobRelease(&sResolved);` |
|     435 |  9332 | `		if( zDup == 0 ){` |
|     ! 0 |  9333 | `			goto Mem;` |
|       - |  9334 | `		}` |
|     435 |  9335 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     435 |  9336 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9337 | `			goto Mem;` |
|       - |  9338 | `		}` |
|       - |  9339 | `		/* Check for '\|' (multi-catch separator) */` |
|     443 |  9340 | `		if( pGen->pIn < pGen->pEnd &&` |
|     430 |  9341 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      31 |  9342 | `			pGen->pIn->sData.nByte == 1 &&` |
|      26 |  9343 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      28 |  9344 | `			pGen->pIn++; /* Consume the '\|' */` |
|      28 |  9345 | `			continue;` |
|       - |  9346 | `		}` |
|     409 |  9347 | `		break;` |
|     ! 0 |  9348 | `	}` |
|     606 |  9349 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     409 |  9350 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  9351 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9352 | `			pToken = pGen->pIn;` |
|     ! 0 |  9353 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9354 | `				pToken--;` |
|     ! 0 |  9355 | `			}` |
|     ! 0 |  9356 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9357 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9358 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9359 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9360 | `				return SXERR_ABORT;` |
|       - |  9361 | `			}` |
|     ! 0 |  9362 | `			return SXERR_INVALID;` |
|       - |  9363 | `	}` |
|     409 |  9364 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9365 | `	/* Duplicate instance name */` |
|     409 |  9366 | `	pName = &pGen->pIn->sData;` |
|     409 |  9367 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     409 |  9368 | `	if( zDup == 0 ){` |
|     ! 0 |  9369 | `		goto Mem;` |
|       - |  9370 | `	}` |
|     409 |  9371 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     409 |  9372 | `	pGen->pIn++;` |
|     409 |  9373 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9374 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9375 | `		pToken = pGen->pIn;` |
|     ! 0 |  9376 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9377 | `			pToken--;` |
|     ! 0 |  9378 | `		}` |
|     ! 0 |  9379 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9380 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9381 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9382 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9383 | `			return SXERR_ABORT;` |
|       - |  9384 | `		}` |
|     ! 0 |  9385 | `		return SXERR_INVALID;` |
|       - |  9386 | `	}` |
|       - |  9387 | `	/* Compile the block */` |
|     409 |  9388 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9389 | `	/* Create the catch block */` |
|     409 |  9390 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     409 |  9391 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9392 | `		return SXERR_ABORT;` |
|       - |  9393 | `	}` |
|       - |  9394 | `	/* Swap bytecode container */` |
|     409 |  9395 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     409 |  9396 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9397 | `	/* Compile the block */` |
|     409 |  9398 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9399 | `	/* Fix forward jumps now the destination is resolved  */` |
|     409 |  9400 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9401 | `	/* Emit the DONE instruction */` |
|     409 |  9402 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9403 | `	/* Leave the block */` |
|     409 |  9404 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9405 | `	/* Restore the default container */` |
|     409 |  9406 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9407 | `	/* Install the catch block */` |
|     409 |  9408 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     409 |  9409 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9410 | `		goto Mem;` |
|       - |  9411 | `	}` |
|     409 |  9412 | `	return SXRET_OK;` |
|     ! 0 |  9413 | `Mem:` |
|     ! 0 |  9414 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9415 | `	return SXERR_ABORT;` |
|     209 |  9416 |  |
|       - |  9417 | `/*` |
|       - |  9418 | ` * Compile a 'try' block.` |
|       - |  9419 | ` * A function using an exception should be in a "try" block.` |
|       - |  9420 | ` * If the exception does not trigger, the code will continue` |
|       - |  9421 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9422 | ` * is "thrown".` |
|       - |  9423 | ` */` |
|     422 |  9424 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 |  9425 |  |
|       - |  9426 | `	ph7_exception *pException;` |
|     427 |  9427 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9428 | `	GenBlock *pTry;` |
|       - |  9429 | `	sxu32 nJmpIdx;` |
|       - |  9430 | `	sxi32 rc;` |
|       - |  9431 | `	/* Create the exception container */` |
|     427 |  9432 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     427 |  9433 | `	if( pException == 0 ){` |
|     ! 0 |  9434 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9435 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9436 | `		return SXERR_ABORT;` |
|       - |  9437 | `	}` |
|       - |  9438 | `	/* Zero the structure */` |
|     427 |  9439 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9440 | `	/* Initialize fields */` |
|     427 |  9441 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     427 |  9442 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     427 |  9443 | `	pException->iHasFinally = 0;` |
|     427 |  9444 | `	pException->iFinallyDone = 0;` |
|     427 |  9445 | `	pException->pVm = pGen->pVm;` |
|       - |  9446 | `	/* Create the try block */` |
|     427 |  9447 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     427 |  9448 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9449 | `		return SXERR_ABORT;` |
|       - |  9450 | `	}` |
|       - |  9451 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     427 |  9452 | `	pTry->pUserData = pException;` |
|       - |  9453 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     427 |  9454 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9455 | `	/* Fix the jump later when the destination is resolved */` |
|     427 |  9456 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     427 |  9457 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9458 | `	/* Compile the block */` |
|     427 |  9459 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     427 |  9460 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9461 | `		return SXERR_ABORT;` |
|       - |  9462 | `	}` |
|       - |  9463 | `	/* Fix forward jumps now the destination is resolved */` |
|     427 |  9464 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9465 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     427 |  9466 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  9467 | `	/* Leave the block */` |
|     427 |  9468 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9469 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     427 |  9470 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     420 |  9471 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  9472 | `		/* Compile one or more catch blocks */` |
|     404 |  9473 | `		for(;;){` |
|     808 |  9474 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     635 |  9475 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     205 |  9476 | `					break;` |
|       - |  9477 | `			}` |
|     413 |  9478 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     413 |  9479 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9480 | `				return SXERR_ABORT;` |
|       - |  9481 | `			}` |
|       5 |  9482 | `		}` |
|     200 |  9483 | `	}` |
|       - |  9484 | `	/* Compile optional finally block */` |
|     427 |  9485 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     208 |  9486 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  9487 | `		SySet *pInstrContainer;` |
|       - |  9488 | `		GenBlock *pFinBlock;` |
|      52 |  9489 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  9490 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      52 |  9491 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      52 |  9492 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9493 | `			return SXERR_ABORT;` |
|       - |  9494 | `		}` |
|       - |  9495 | `		/* Swap bytecode container */` |
|      52 |  9496 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      52 |  9497 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  9498 | `		/* Compile the finally body */` |
|      52 |  9499 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      52 |  9500 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9501 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  9502 | `			return SXERR_ABORT;` |
|       - |  9503 | `		}` |
|       - |  9504 | `		/* Fix forward jumps now the destination is resolved */` |
|      52 |  9505 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9506 | `		/* Emit DONE to terminate the finally block */` |
|      52 |  9507 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9508 | `		/* Leave the block */` |
|      52 |  9509 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9510 | `		/* Restore the default container */` |
|      52 |  9511 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      52 |  9512 | `		pException->iHasFinally = 1;` |
|      24 |  9513 | `	}` |
|       - |  9514 | `	/* Must have at least one catch or finally */` |
|     427 |  9515 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 |  9516 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9517 | `			"Cannot use try without catch or finally");` |
|       8 |  9518 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9519 | `			return SXERR_ABORT;` |
|       - |  9520 | `		}` |
|       3 |  9521 | `	}` |
|     427 |  9522 | `	return SXRET_OK;` |
|     216 |  9523 |  |
|       - |  9524 | `/*` |
|       - |  9525 | ` * Compile a switch block.` |
|       - |  9526 | ` *  (See block-comment below for more information)` |
|       - |  9527 | ` */` |
|     112 |  9528 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 |  9529 |  |
|     117 |  9530 | `	sxi32 rc = SXRET_OK;` |
|     117 |  9531 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  9532 | `		/* Unexpected token */` |
|     ! 0 |  9533 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9534 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9535 | `			return SXERR_ABORT;` |
|       - |  9536 | `		}` |
|     ! 0 |  9537 | `		pGen->pIn++;` |
|     ! 0 |  9538 | `	}` |
|     117 |  9539 | `	pGen->pIn++;` |
|       - |  9540 | `	/* First instruction to execute in this block. */` |
|     117 |  9541 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  9542 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  9543 | `	 * or the '}' token */` |
|     206 |  9544 | `	for(;;){` |
|     417 |  9545 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9546 | `			/* No more input to process */` |
|     ! 0 |  9547 | `			break;` |
|       - |  9548 | `		}` |
|     417 |  9549 | `		rc = SXRET_OK;` |
|     417 |  9550 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 |  9551 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 |  9552 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  9553 | `					/* Unexpected token */` |
|     ! 0 |  9554 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9555 | `						&pGen->pIn->sData);` |
|     ! 0 |  9556 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9557 | `						return SXERR_ABORT;` |
|       - |  9558 | `					}` |
|       - |  9559 | `					/* FALL THROUGH */` |
|     ! 0 |  9560 | `				}` |
|      31 |  9561 | `				rc = SXERR_EOF;` |
|      31 |  9562 | `				break;` |
|       - |  9563 | `			}` |
|      32 |  9564 | `		}else{` |
|       - |  9565 | `			sxi32 nKwrd;` |
|       - |  9566 | `			/* Extract the keyword */` |
|     337 |  9567 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 |  9568 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 |  9569 | `				break;` |
|       - |  9570 | `			}` |
|     253 |  9571 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9572 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  9573 | `					/* Unexpected token */` |
|     ! 0 |  9574 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9575 | `						&pGen->pIn->sData);` |
|     ! 0 |  9576 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9577 | `						return SXERR_ABORT;` |
|       - |  9578 | `					}` |
|       - |  9579 | `					/* FALL THROUGH */` |
|     ! 0 |  9580 | `				}` |
|       - |  9581 | `				/* Block compiled */` |
|       3 |  9582 | `				break;` |
|       - |  9583 | `			}` |
|       - |  9584 | `		}` |
|       - |  9585 | `		/* Compile block */` |
|     305 |  9586 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 |  9587 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9588 | `			return SXERR_ABORT;` |
|       - |  9589 | `		}` |
|       5 |  9590 | `	}` |
|     117 |  9591 | `	return rc;` |
|      61 |  9592 |  |
|       - |  9593 | `/*` |
|       - |  9594 | ` * Compile a case eXpression.` |
|       - |  9595 | ` *  (See block-comment below for more information)` |
|       - |  9596 | ` */` |
|      92 |  9597 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 |  9598 |  |
|       - |  9599 | `	SySet *pInstrContainer;` |
|       - |  9600 | `	SyToken *pEnd,*pTmp;` |
|      97 |  9601 | `	sxi32 iNest = 0;` |
|       - |  9602 | `	sxi32 rc;` |
|       - |  9603 | `	/* Delimit the expression */` |
|      97 |  9604 | `	pEnd = pGen->pIn;` |
|     197 |  9605 | `	while( pEnd < pGen->pEnd ){` |
|     197 |  9606 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  9607 | `			/* Increment nesting level */` |
|       3 |  9608 | `			iNest++;` |
|     196 |  9609 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  9610 | `			/* Decrement nesting level */` |
|       3 |  9611 | `			iNest--;` |
|     194 |  9612 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 |  9613 | `			break;` |
|       - |  9614 | `		}` |
|     105 |  9615 | `		pEnd++;` |
|       5 |  9616 | `	}` |
|      97 |  9617 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  9618 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  9619 | `		if( rc == SXERR_ABORT ){` |
|       - |  9620 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9621 | `			return SXERR_ABORT;` |
|       - |  9622 | `		}` |
|     ! 0 |  9623 | `	}` |
|       - |  9624 | `	/* Swap token stream */` |
|      97 |  9625 | `	pTmp = pGen->pEnd;` |
|      97 |  9626 | `	pGen->pEnd = pEnd;` |
|      97 |  9627 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 |  9628 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 |  9629 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  9630 | `	/* Emit the done instruction */` |
|      97 |  9631 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 |  9632 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9633 | `	/* Update token stream */` |
|      97 |  9634 | `	pGen->pIn  = pEnd;` |
|      97 |  9635 | `	pGen->pEnd = pTmp;` |
|      97 |  9636 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9637 | `		return SXERR_ABORT;` |
|       - |  9638 | `	}` |
|      97 |  9639 | `	return SXRET_OK;` |
|      51 |  9640 |  |
|       - |  9641 | `/*` |
|       - |  9642 | ` * Compile the smart switch statement.` |
|       - |  9643 | ` * According to the PHP language reference manual` |
|       - |  9644 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  9645 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  9646 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  9647 | ` *  This is exactly what the switch statement is for.` |
|       - |  9648 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  9649 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  9650 | ` *  of the outer loop, use continue 2.` |
|       - |  9651 | ` *  Note that switch/case does loose comparision.` |
|       - |  9652 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  9653 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  9654 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  9655 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  9656 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  9657 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  9658 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  9659 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  9660 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  9661 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  9662 | ` *  list for the next case.` |
|       - |  9663 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  9664 | ` *  or floating-point numbers and strings.` |
|       - |  9665 | ` */` |
|      28 |  9666 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 |  9667 |  |
|       - |  9668 | `	GenBlock *pSwitchBlock;` |
|       - |  9669 | `	SyToken *pTmp,*pEnd;` |
|       - |  9670 | `	ph7_switch *pSwitch;` |
|       - |  9671 | `	sxu32 nToken;` |
|       - |  9672 | `	sxu32 nLine;` |
|       - |  9673 | `	sxi32 rc;` |
|      33 |  9674 | `	nLine = pGen->pIn->nLine;` |
|       - |  9675 | `	/* Jump the 'switch' keyword */` |
|      33 |  9676 | `	pGen->pIn++;` |
|      33 |  9677 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  9678 | `		/* Syntax error */` |
|     ! 0 |  9679 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  9680 | `		if( rc == SXERR_ABORT ){` |
|       - |  9681 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9682 | `			return SXERR_ABORT;` |
|       - |  9683 | `		}` |
|     ! 0 |  9684 | `		goto Synchronize;` |
|       - |  9685 | `	}` |
|       - |  9686 | `	/* Jump the left parenthesis '(' */` |
|      33 |  9687 | `	pGen->pIn++;` |
|      33 |  9688 | `	pEnd = 0; /* cc warning */` |
|       - |  9689 | `	/* Create the loop block */` |
|      47 |  9690 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  9691 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 |  9692 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9693 | `		return SXERR_ABORT;` |
|       - |  9694 | `	}` |
|       - |  9695 | `	/* Delimit the condition */` |
|      33 |  9696 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 |  9697 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  9698 | `		/* Empty expression */` |
|     ! 0 |  9699 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  9700 | `		if( rc == SXERR_ABORT ){` |
|       - |  9701 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9702 | `			return SXERR_ABORT;` |
|       - |  9703 | `		}` |
|     ! 0 |  9704 | `	}` |
|       - |  9705 | `	/* Swap token streams */` |
|      33 |  9706 | `	pTmp = pGen->pEnd;` |
|      33 |  9707 | `	pGen->pEnd = pEnd;` |
|       - |  9708 | `	/* Compile the expression */` |
|      33 |  9709 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 |  9710 | `	if( rc == SXERR_ABORT ){` |
|       - |  9711 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  9712 | `		return SXERR_ABORT;` |
|       - |  9713 | `	}` |
|       - |  9714 | `	/* Update token stream */` |
|      33 |  9715 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  9716 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9717 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9718 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9719 | `			return SXERR_ABORT;` |
|       - |  9720 | `		}` |
|     ! 0 |  9721 | `		pGen->pIn++;` |
|     ! 0 |  9722 | `	}` |
|      33 |  9723 | `	pGen->pIn  = &pEnd[1];` |
|      33 |  9724 | `	pGen->pEnd = pTmp;` |
|      33 |  9725 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 |  9726 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 |  9727 | `			pTmp = pGen->pIn;` |
|     ! 0 |  9728 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 |  9729 | `				pTmp--;` |
|     ! 0 |  9730 | `			}` |
|       - |  9731 | `			/* Unexpected token */` |
|     ! 0 |  9732 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 |  9733 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9734 | `				return SXERR_ABORT;` |
|       - |  9735 | `			}` |
|     ! 0 |  9736 | `			goto Synchronize;` |
|       - |  9737 | `	}` |
|       - |  9738 | `	/* Set the delimiter token */` |
|      33 |  9739 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 |  9740 | `		nToken = PH7_TK_KEYWORD;` |
|       - |  9741 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 |  9742 | `	}else{` |
|      31 |  9743 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - |  9744 | `	}` |
|      33 |  9745 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - |  9746 | `	/* Create the switch blocks container */` |
|      33 |  9747 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 |  9748 | `	if( pSwitch == 0 ){` |
|       - |  9749 | `		/* Abort compilation */` |
|     ! 0 |  9750 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9751 | `		return SXERR_ABORT;` |
|       - |  9752 | `	}` |
|       - |  9753 | `	/* Zero the structure */` |
|      33 |  9754 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - |  9755 | `	/* Initialize fields */` |
|      33 |  9756 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - |  9757 | `	/* Emit the switch instruction */` |
|      33 |  9758 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - |  9759 | `	/* Compile case blocks */` |
|     100 |  9760 | `	for(;;){` |
|       - |  9761 | `		sxu32 nKwrd;` |
|     119 |  9762 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9763 | `			/* No more input to process */` |
|     ! 0 |  9764 | `			break;` |
|       - |  9765 | `		}` |
|     119 |  9766 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9767 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - |  9768 | `				/* Unexpected token */` |
|     ! 0 |  9769 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9770 | `					&pGen->pIn->sData);` |
|     ! 0 |  9771 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9772 | `					return SXERR_ABORT;` |
|       - |  9773 | `				}` |
|       - |  9774 | `				/* FALL THROUGH */` |
|     ! 0 |  9775 | `			}` |
|       - |  9776 | `			/* Block compiled */` |
|     ! 0 |  9777 | `			break;` |
|       - |  9778 | `		}` |
|       - |  9779 | `		/* Extract the keyword */` |
|     119 |  9780 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 |  9781 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9782 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - |  9783 | `				/* Unexpected token */` |
|     ! 0 |  9784 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9785 | `					&pGen->pIn->sData);` |
|     ! 0 |  9786 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9787 | `					return SXERR_ABORT;` |
|       - |  9788 | `				}` |
|       - |  9789 | `				/* FALL THROUGH */` |
|     ! 0 |  9790 | `			}` |
|       - |  9791 | `			/* Block compiled */` |
|       3 |  9792 | `			break;` |
|       - |  9793 | `		}` |
|     117 |  9794 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - |  9795 | `			/*` |
|       - |  9796 | `			 * Accroding to the PHP language reference manual` |
|       - |  9797 | `			 *  A special case is the default case. This case matches anything` |
|       - |  9798 | `			 *  that wasn't matched by the other cases.` |
|       - |  9799 | `			 */` |
|      25 |  9800 | `			if( pSwitch->nDefault > 0 ){` |
|       - |  9801 | `				/* Default case already compiled */` |
|     ! 0 |  9802 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 |  9803 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9804 | `					return SXERR_ABORT;` |
|       - |  9805 | `				}` |
|     ! 0 |  9806 | `			}` |
|      25 |  9807 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - |  9808 | `			/* Compile the default block */` |
|      25 |  9809 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 |  9810 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9811 | `				return SXERR_ABORT;` |
|      25 |  9812 | `			}else if( rc == SXERR_EOF ){` |
|      23 |  9813 | `				break;` |
|       1 |  9814 | `			}` |
|      98 |  9815 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - |  9816 | `			ph7_case_expr sCase;` |
|       - |  9817 | `			/* Standard case block */` |
|      97 |  9818 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - |  9819 | `			/* initialize the structure */` |
|      97 |  9820 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  9821 | `			/* Compile the case expression */` |
|      97 |  9822 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 |  9823 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9824 | `				return SXERR_ABORT;` |
|       - |  9825 | `			}` |
|       - |  9826 | `			/* Compile the case block */` |
|      97 |  9827 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - |  9828 | `			/* Insert in the switch container */` |
|      97 |  9829 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 |  9830 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9831 | `				return SXERR_ABORT;` |
|      97 |  9832 | `			}else if( rc == SXERR_EOF ){` |
|       9 |  9833 | `				break;` |
|       - |  9834 | `			}` |
|      47 |  9835 | `		}else{` |
|       - |  9836 | `			/* Unexpected token */` |
|     ! 0 |  9837 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9838 | `				&pGen->pIn->sData);` |
|     ! 0 |  9839 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9840 | `				return SXERR_ABORT;` |
|       - |  9841 | `			}` |
|     ! 0 |  9842 | `			break;` |
|       - |  9843 | `		}` |
|       5 |  9844 | `	}` |
|       - |  9845 | `	/* Fix all jumps now the destination is resolved */` |
|      33 |  9846 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 |  9847 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9848 | `	/* Release the loop block */` |
|      33 |  9849 | `	GenStateLeaveBlock(pGen,0);` |
|      33 |  9850 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - |  9851 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 |  9852 | `		pGen->pIn++;` |
|      14 |  9853 | `	}` |
|       - |  9854 | `	/* Statement successfully compiled */` |
|      33 |  9855 | `	return SXRET_OK;` |
|     ! 0 |  9856 | `Synchronize:` |
|       - |  9857 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  9858 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 |  9859 | `		pGen->pIn++;` |
|     ! 0 |  9860 | `	}` |
|     ! 0 |  9861 | `	return SXRET_OK;` |
|      19 |  9862 |  |
|       - |  9863 | `/*` |
|       - |  9864 | ` * Chain operators participate in a postfix member-access chain.` |
|       - |  9865 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - |  9866 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - |  9867 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - |  9868 | ` */` |
|       - |  9869 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - |  9870 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - |  9871 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - |  9872 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - |  9873 |  |
|       - |  9874 | `/*` |
|       - |  9875 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - |  9876 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - |  9877 | ` * patched entries from the pending set.` |
|       - |  9878 | ` */` |
| 2303152 |  9879 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 |  9880 |  |
| 2303157 |  9881 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - |  9882 | `	sxu32 nTarget;` |
|       - |  9883 | `	sxu32 *aIdx;` |
|       - |  9884 | `	sxu32 i;` |
| 2303157 |  9885 | `	if( nCur <= nBaseline ){` |
| 2303067 |  9886 | `		return;` |
|       - |  9887 | `	}` |
|      93 |  9888 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      93 |  9889 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     191 |  9890 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     101 |  9891 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     101 |  9892 | `		if( pInstr ){` |
|     101 |  9893 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 |  9894 | `		}` |
|      52 |  9895 | `	}` |
|      93 |  9896 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1151581 |  9897 |  |
|       - |  9898 |  |
|       - |  9899 | `/*` |
|       - |  9900 | ` * By-reference out-parameters of builtin functions.` |
|       - |  9901 | ` *` |
|       - |  9902 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - |  9903 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - |  9904 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - |  9905 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - |  9906 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - |  9907 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - |  9908 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - |  9909 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - |  9910 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - |  9911 | ` * creates it" behaviour).` |
|       - |  9912 | ` *` |
|       - |  9913 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - |  9914 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - |  9915 | ` */` |
|  374008 |  9916 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 |  9917 |  |
|       - |  9918 | `	static const struct {` |
|       - |  9919 | `		const char *zName;` |
|       - |  9920 | `		sxu32 nByte;` |
|       - |  9921 | `		sxu32 mask;` |
|       - |  9922 | `	} aByRef[] = {` |
|       - |  9923 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - |  9924 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - |  9925 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - |  9926 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - |  9927 | `	};` |
|       - |  9928 | `	sxu32 i;` |
|  374013 |  9929 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1155 |  9930 | `		return 0;` |
|       - |  9931 | `	}` |
| 1864151 |  9932 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1491334 |  9933 | `		if( pName->nByte == aByRef[i].nByte` |
|  765334 |  9934 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      51 |  9935 | `			return aByRef[i].mask;` |
|       - |  9936 | `		}` |
|  745649 |  9937 | `	}` |
|  372817 |  9938 | `	return 0;` |
|  187009 |  9939 |  |
|       - |  9940 | `/*` |
|       - |  9941 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - |  9942 | ` *` |
|       - |  9943 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - |  9944 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - |  9945 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - |  9946 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - |  9947 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - |  9948 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - |  9949 | ` */` |
|  374008 |  9950 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 |  9951 |  |
|       - |  9952 | `	SyToken *p, *pEnd;` |
|  374013 |  9953 | `	pOut->zString = 0;` |
|  374013 |  9954 | `	pOut->nByte = 0;` |
|  374013 |  9955 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 |  9956 | `		return;` |
|       - |  9957 | `	}` |
|  374013 |  9958 | `	p = pLeft->pStart;` |
|  374013 |  9959 | `	pEnd = pLeft->pEnd;` |
|       - |  9960 | `	/* Optional single leading namespace separator (absolute path). */` |
|  374013 |  9961 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      24 |  9962 | `		p++;` |
|      10 |  9963 | `	}` |
|  374013 |  9964 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1129 |  9965 | `		return;` |
|       - |  9966 | `	}` |
|       - |  9967 | `	/* Must be a single component: nothing follows the name token. */` |
|  372889 |  9968 | `	if( p + 1 != pEnd ){` |
|      30 |  9969 | `		return;` |
|       - |  9970 | `	}` |
|  372863 |  9971 | `	*pOut = p->sData;` |
|  187009 |  9972 |  |
|       - |  9973 | `/*` |
|       - |  9974 | ` * Generate bytecode for a given expression tree.` |
|       - |  9975 | ` * If something goes wrong while generating bytecode` |
|       - |  9976 | ` * for the expression tree (A very unlikely scenario)` |
|       - |  9977 | ` * this function takes care of generating the appropriate` |
|       - |  9978 | ` * error message.` |
|       - |  9979 | ` */` |
| 3102756 |  9980 | `static sxi32 GenStateEmitExprCode(` |
|       - |  9981 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  9982 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - |  9983 | `	sxi32 iFlags /* Control flags */` |
|       - |  9984 | `	)` |
|       5 |  9985 |  |
|       - |  9986 | `	VmInstr *pInstr;` |
|       - |  9987 | `	sxu32 nJmpIdx;` |
| 3102761 |  9988 | `	sxi32 iP1 = 0;` |
| 3102761 |  9989 | `	sxu32 iP2 = 0;` |
| 3102761 |  9990 | `	void *p3  = 0;` |
|       - |  9991 | `	sxi32 iVmOp;` |
|       - |  9992 | `	sxi32 rc;` |
| 3102761 |  9993 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3102761 |  9994 | `	sxu32 nRhsNsBase = 0;` |
| 3102761 |  9995 | `	if( pNode->xCode ){` |
|       - |  9996 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - |  9997 | `		/* Compile node */` |
| 1922051 |  9998 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1922051 |  9999 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1922051 | 10000 | `		RE_SWAP_DELIMITER(pGen);` |
| 1922051 | 10001 | `		return rc;` |
|       - | 10002 | `	}` |
| 1180715 | 10003 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10004 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10005 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10006 | `		return SXERR_ABORT;` |
|       - | 10007 | `	}` |
| 1180715 | 10008 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1180715 | 10009 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      59 | 10010 | `		sxu32 nJmp = 0;` |
|       - | 10011 | `		sxu32 nNcNsBase;` |
|       - | 10012 | `		VmInstr *pInstrFix;` |
|       - | 10013 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10014 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10015 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10016 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10017 | `		 * stack slot carries a writable nIdx. */` |
|      59 | 10018 | `		if( pNode->pRight ){` |
|      59 | 10019 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10020 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      59 | 10021 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10022 | `				return rc;` |
|       - | 10023 | `			}` |
|      59 | 10024 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10025 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10026 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10027 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10028 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10029 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10030 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10031 | `			 * cascade for the actual write path stays correct. */` |
|      59 | 10032 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      59 | 10033 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      29 | 10034 | `				pInstrFix->iP2 = 3;` |
|      13 | 10035 | `			}` |
|      28 | 10036 | `		}` |
|       - | 10037 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      59 | 10038 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10039 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      59 | 10040 | `		if( pNode->pLeft ){` |
|      59 | 10041 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10042 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      59 | 10043 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10044 | `				return rc;` |
|       - | 10045 | `			}` |
|      59 | 10046 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      28 | 10047 | `		}` |
|       - | 10048 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      59 | 10049 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10050 | `		/* Patch the short-circuit jump to land after the store. */` |
|      59 | 10051 | `		if( nJmp > 0 ){` |
|      59 | 10052 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      59 | 10053 | `			if( pInstrFix ){` |
|      59 | 10054 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10055 | `			}` |
|      28 | 10056 | `		}` |
|      59 | 10057 | `		return SXRET_OK;` |
|       - | 10058 | `	}` |
| 1180659 | 10059 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10060 | `		sxu32 nJz,nJmp;` |
|       - | 10061 | `		sxu32 nTernaryNsBase;` |
|       - | 10062 | `		/* Ternary operator require special handling */` |
|       - | 10063 | `		/* Phase#1: Compile the condition */` |
|    2549 | 10064 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2549 | 10065 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2549 | 10066 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10067 | `			return rc;` |
|       - | 10068 | `		}` |
|       - | 10069 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10070 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10071 | `		 * condition expression, not leak past the ternary. */` |
|    2549 | 10072 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2549 | 10073 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2549 | 10074 | `		if( pNode->pLeft ){` |
|       - | 10075 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10076 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2481 | 10077 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10078 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2481 | 10079 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2481 | 10080 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2481 | 10081 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10082 | `				return rc;` |
|       - | 10083 | `			}` |
|    2481 | 10084 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1243 | 10085 | `		}else{` |
|       - | 10086 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10087 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10088 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10089 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10090 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10091 | `		}` |
|       - | 10092 | `		/* Phase#4: Emit the unconditional jump */` |
|    2549 | 10093 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10094 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2549 | 10095 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2549 | 10096 | `		if( pInstr ){` |
|    2549 | 10097 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1272 | 10098 | `		}` |
|    2549 | 10099 | `		if( !pNode->pLeft ){` |
|       - | 10100 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10101 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10102 | `		}` |
|       - | 10103 | `		/* Phase#6: Compile the 'else' expression */` |
|    2549 | 10104 | `		if( pNode->pRight ){` |
|    2549 | 10105 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2549 | 10106 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2549 | 10107 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10108 | `				return rc;` |
|       - | 10109 | `			}` |
|    2549 | 10110 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1272 | 10111 | `		}` |
|    2549 | 10112 | `		if( nJmp > 0 ){` |
|       - | 10113 | `			/* Phase#7: Fix the unconditional jump */` |
|    2549 | 10114 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2549 | 10115 | `			if( pInstr ){` |
|    2549 | 10116 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1272 | 10117 | `			}` |
|    1272 | 10118 | `		}` |
|       - | 10119 | `		/* All done */` |
|    2549 | 10120 | `		return SXRET_OK;` |
|       - | 10121 | `	}` |
| 1178115 | 10122 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10123 | `	/* Generate code for the left tree */` |
| 1178115 | 10124 | `	if( pNode->pLeft ){` |
| 1178077 | 10125 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1178077 | 10126 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10127 | `			ph7_expr_node **apNode;` |
|  374133 | 10128 | `			int hasSpread = 0;` |
|  374133 | 10129 | `			int hasNamed = 0;` |
|  374133 | 10130 | `			int bAnySpread = 0;` |
|  374133 | 10131 | `			sxu32 byRefMask = 0;` |
|       - | 10132 | `			sxi32 nArgs;` |
|       - | 10133 | `			sxi32 n;` |
|       - | 10134 | `			/* Recurse and generate bytecodes for function arguments */` |
|  374133 | 10135 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  374133 | 10136 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10137 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10138 | `			{` |
|  374133 | 10139 | `				int seenNamed = 0;` |
|  740869 | 10140 | `				for( n = 0; n < nArgs; ++n ){` |
|  366743 | 10141 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     188 | 10142 | `						seenNamed = 1;` |
|     188 | 10143 | `						hasNamed = 1;` |
|  366651 | 10144 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      23 | 10145 | `						bAnySpread = 1;` |
|  366549 | 10146 | `					}else if( seenNamed ){` |
|       3 | 10147 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10148 | `							"Cannot use positional argument after named argument");` |
|       3 | 10149 | `						return SXERR_SYNTAX;` |
|       - | 10150 | `					}` |
|  183373 | 10151 | `				}` |
|       - | 10152 | `			}` |
|       - | 10153 | `			/* Read-only load */` |
|  374131 | 10154 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10155 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10156 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10157 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10158 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  374131 | 10159 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  374131 | 10160 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  374126 | 10161 | `				if( pCallName->nByte == 5` |
|  205340 | 10162 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   19189 | 10163 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  364539 | 10164 | `				}else if( pCallName->nByte == 5` |
|  186156 | 10165 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      83 | 10166 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      39 | 10167 | `				}` |
|       - | 10168 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10169 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10170 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10171 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10172 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10173 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  374131 | 10174 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10175 | `					SyString sBuiltin;` |
|  374013 | 10176 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  374013 | 10177 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  187004 | 10178 | `				}` |
|  187063 | 10179 | `			}` |
|  740865 | 10180 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  366739 | 10181 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  366739 | 10182 | `				sxi32 iArgFlags = iFlags & ~EXPR_FLAG_LOAD_IDX_STORE;` |
|       - | 10183 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 10184 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate). */` |
|  366739 | 10185 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      31 | 10186 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      13 | 10187 | `				}` |
|  366739 | 10188 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  366739 | 10189 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10190 | `					return rc;` |
|       - | 10191 | `				}` |
|       - | 10192 | `				/* Each argument is an independent nullsafe scope. */` |
|  366739 | 10193 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  366739 | 10194 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10195 | `					/* Emit spread opcode to unpack this array argument */` |
|      23 | 10196 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      23 | 10197 | `					hasSpread = 1;` |
|      10 | 10198 | `				}` |
|  183372 | 10199 | `			}` |
|       - | 10200 | `			/* Total number of given arguments */` |
|  374131 | 10201 | `			iP1 = nArgs;` |
|  374131 | 10202 | `			iP2 = hasSpread;` |
|       - | 10203 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10204 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  374131 | 10205 | `			if( hasNamed ){` |
|     101 | 10206 | `				sxu32 nStrBytes = 0;` |
|       - | 10207 | `				char *zBuf;` |
|     297 | 10208 | `				for( n = 0; n < nArgs; ++n ){` |
|     199 | 10209 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10210 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      91 | 10211 | `					}` |
|     101 | 10212 | `				}` |
|       - | 10213 | `				{` |
|     101 | 10214 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     101 | 10215 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      98 | 10216 | `					&pGen->pVm->sAllocator, mapSize);` |
|     101 | 10217 | `				if( pMap ){` |
|     101 | 10218 | `					SyZero(pMap, mapSize);` |
|     101 | 10219 | `					pMap->bHasNamed = 1;` |
|     101 | 10220 | `					pMap->nTotal = (sxu32)nArgs;` |
|     101 | 10221 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     101 | 10222 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     297 | 10223 | `					for( n = 0; n < nArgs; ++n ){` |
|     199 | 10224 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10225 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     185 | 10226 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     185 | 10227 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     185 | 10228 | `							zBuf += nb;` |
|      91 | 10229 | `						}` |
|       - | 10230 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     101 | 10231 | `					}` |
|     101 | 10232 | `					p3 = (void *)pMap;` |
|      49 | 10233 | `				}` |
|       - | 10234 | `				}` |
|      49 | 10235 | `			}` |
|       - | 10236 | `			/* Remove stale flags now */` |
|  374131 | 10237 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  187063 | 10238 | `		}` |
| 1178075 | 10239 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1178075 | 10240 | `		if( rc != SXRET_OK ){` |
|      34 | 10241 | `			return rc;` |
|       - | 10242 | `		}` |
| 1178045 | 10243 | `		if( !bIsChainOp ){` |
|       - | 10244 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 10245 | `			 * target the end of that LHS chain, which is right here. */` |
|  550629 | 10246 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  275312 | 10247 | `		}` |
| 1178045 | 10248 | `		if( iVmOp == PH7_OP_CALL ){` |
|  374131 | 10249 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  374131 | 10250 | `			if( pInstr ){` |
|  374131 | 10251 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  372983 | 10252 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 10253 | `					sxu32 nQual;` |
|  372983 | 10254 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10255 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 10256 | `					 * so the later NEW handler (if any) can see it. */` |
|  372983 | 10257 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 10258 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 10259 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 10260 | `					 * imports — class imports must NOT affect function` |
|       - | 10261 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 10262 | `					 * before NEW; we store the original literal index in the` |
|       - | 10263 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 10264 | `					 * the unqualified name and re-qualify with class imports. */` |
|  372983 | 10265 | `					if( bAbsolute ){` |
|      24 | 10266 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      14 | 10267 | `					}else{` |
|  372963 | 10268 | `						int fromImport = 0;` |
|  372963 | 10269 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  372963 | 10270 | `						pInstr->iP2 = (sxi32)nQual;` |
|  372963 | 10271 | `						if( nQual != nOrig ){` |
|       - | 10272 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 10273 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 10274 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 10275 | `							if( !fromImport ){` |
|       - | 10276 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 10277 | `								if( p3 == 0 ){` |
|      67 | 10278 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 10279 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 10280 | `									if( pMap ){` |
|      67 | 10281 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 10282 | `										p3 = (void *)pMap;` |
|      31 | 10283 | `									}` |
|      31 | 10284 | `								}` |
|      67 | 10285 | `								if( p3 ){` |
|      67 | 10286 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 10287 | `								}` |
|      31 | 10288 | `							}` |
|      36 | 10289 | `						}` |
|       5 | 10290 | `					}` |
|  187642 | 10291 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 10292 | `					/* Method call,flag that */` |
|     873 | 10293 | `					pInstr->iP2 = 1;` |
|     434 | 10294 | `				}` |
|  187068 | 10295 | `			}` |
|  990982 | 10296 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 10297 | `			ph7_expr_node **apNode;` |
|       - | 10298 | `			sxi32 n;` |
|   81133 | 10299 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 10300 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 10301 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       - | 10302 | `			/* Recurse and generate bytecodes for array index */` |
|   81133 | 10303 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  146427 | 10304 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   65299 | 10305 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   65299 | 10306 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   65299 | 10307 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10308 | `					return rc;` |
|       - | 10309 | `				}` |
|       - | 10310 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   65299 | 10311 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   32652 | 10312 | `			}` |
|   81133 | 10313 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   65299 | 10314 | `				iP1 = 1; /* Node have an index associated with it */` |
|   32647 | 10315 | `			}` |
|   81133 | 10316 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 10317 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     241 | 10318 | `				iP2 = 4;` |
|   81015 | 10319 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 10320 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 10321 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      54 | 10322 | `				iP2 = 5;` |
|   80872 | 10323 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 10324 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 10325 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 10326 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 10327 | `				iP2 = 6;` |
|   80835 | 10328 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 10329 | `				/* Create an empty entry when the desired index is not found */` |
|   31943 | 10330 | `				iP2 = 1;` |
|   15974 | 10331 | `			}` |
|  763355 | 10332 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 10333 | `			/* POP the left node */` |
|      32 | 10334 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 10335 | `		}` |
|  589020 | 10336 | `	}` |
| 1178083 | 10337 | `	rc = SXRET_OK;` |
| 1178083 | 10338 | `	nJmpIdx = 0;` |
|       - | 10339 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 10340 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 10341 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1178083 | 10342 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     279 | 10343 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     279 | 10344 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     279 | 10345 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     279 | 10346 | `			int isSpecial = 0;` |
|     279 | 10347 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     191 | 10348 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     191 | 10349 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     201 | 10350 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     169 | 10351 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      86 | 10352 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      93 | 10353 | `					isSpecial = 1;` |
|      44 | 10354 | `				}` |
|     115 | 10355 | `			}` |
|     323 | 10356 | `			pInstr->iP1 = 0;` |
|     323 | 10357 | `			if( !isSpecial ){` |
|     147 | 10358 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      71 | 10359 | `			}` |
|       - | 10360 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 10361 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     235 | 10362 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     147 | 10363 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     147 | 10364 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 10365 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      46 | 10366 | `					return SXRET_OK;` |
|       - | 10367 | `				}` |
|      50 | 10368 | `			}` |
|      94 | 10369 | `		}` |
|     170 | 10370 | `	}` |
|       - | 10371 | `	/* Generate code for the right tree */` |
| 1178005 | 10372 | `	if( pNode->pRight ){` |
|  650487 | 10373 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 10374 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    9903 | 10375 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  645538 | 10376 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 10377 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3331 | 10378 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  638926 | 10379 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 10380 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 10381 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 10382 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  637250 | 10383 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 10384 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 10385 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 10386 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 10387 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 10388 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 10389 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     101 | 10390 | `			sxu32 nNsJmp = 0;` |
|     101 | 10391 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     101 | 10392 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  637090 | 10393 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  264123 | 10394 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  132059 | 10395 | `		}` |
|  650487 | 10396 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  650487 | 10397 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  650487 | 10398 | `		if( !bIsChainOp ){` |
|       - | 10399 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 10400 | `			 * operator instruction is emitted. */` |
|  478367 | 10401 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  239181 | 10402 | `		}` |
|  650487 | 10403 | `		if( iVmOp == PH7_OP_STORE ){` |
|  260729 | 10404 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  260700 | 10405 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 10406 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 10407 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 10408 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 10409 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 10410 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 10411 | `				 */` |
|      56 | 10412 | `				iVmOp = 0;` |
|  260703 | 10413 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  260677 | 10414 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 10415 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   72743 | 10416 | `					iP2 = 1;` |
|   36374 | 10417 | `				}else{` |
|  187939 | 10418 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10419 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   31897 | 10420 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   31897 | 10421 | `						iP1 = pInstr->iP1;` |
|   15951 | 10422 | `					}else{` |
|  156047 | 10423 | `						p3 = pInstr->p3;` |
|       - | 10424 | `					}` |
|       - | 10425 | `					/* POP the last dynamic load instruction */` |
|  187939 | 10426 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 10427 | `				}` |
|  130341 | 10428 | `			}` |
|  520125 | 10429 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      52 | 10430 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      52 | 10431 | `			if( pInstr ){` |
|      52 | 10432 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10433 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 10434 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 10435 | `					 */` |
|      15 | 10436 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 10437 | `					iP1 = pInstr->iP1;` |
|      15 | 10438 | `					iP2 = pInstr->iP2;` |
|      15 | 10439 | `					p3  = pInstr->p3;` |
|       8 | 10440 | `				}else{` |
|      38 | 10441 | `					p3 = pInstr->p3;` |
|       - | 10442 | `				}` |
|      25 | 10443 | `			}` |
|      25 | 10444 | `		}` |
|  325241 | 10445 | `	}` |
| 1178005 | 10446 | `	if( iVmOp > 0 ){` |
| 1177799 | 10447 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   12971 | 10448 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 10449 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    9465 | 10450 | `				iP1 = 1;` |
|    4735 | 10451 | `			}` |
| 1171316 | 10452 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 10453 | `			/* Namespace-qualify the class name for NEW */ {` |
|   16879 | 10454 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   16879 | 10455 | `				VmInstr *pCallInstr = 0;` |
|   16879 | 10456 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   16855 | 10457 | `					pCallInstr = pPeek;` |
|   16855 | 10458 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    8425 | 10459 | `				}` |
|   16879 | 10460 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   16877 | 10461 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10462 | `					sxu32 nLitForClass;` |
|       - | 10463 | `					/* If the CALL handler already qualified the name using` |
|       - | 10464 | `					 * function imports, recover the original unqualified` |
|       - | 10465 | `					 * literal so we can re-qualify with class imports. */` |
|   16877 | 10466 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 10467 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 10468 | `					}else{` |
|   16845 | 10469 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 10470 | `					}` |
|   16877 | 10471 | `					pPeek->iP1 = 0;` |
|   16877 | 10472 | `					if( !bAbsolute ){` |
|   16861 | 10473 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    8433 | 10474 | `					}else{` |
|      20 | 10475 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 10476 | `					}` |
|    8436 | 10477 | `				}` |
|       - | 10478 | `			}` |
|   16879 | 10479 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   16879 | 10480 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 10481 | `				VmInstr *pPrev;` |
|   16855 | 10482 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   16855 | 10483 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 10484 | `					/* Pop the call instruction, preserve named-arg map */` |
|   16855 | 10485 | `					iP1 = pInstr->iP1;` |
|   16855 | 10486 | `					if( pInstr->p3 ){` |
|      43 | 10487 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 10488 | `					}` |
|   16855 | 10489 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    8425 | 10490 | `				}` |
|    8430 | 10491 | `			}` |
| 1156396 | 10492 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 10493 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 10494 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     161 | 10495 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     161 | 10496 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     161 | 10497 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     161 | 10498 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     161 | 10499 | `				int isSpecialIs = 0;` |
|     161 | 10500 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     157 | 10501 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     157 | 10502 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     157 | 10503 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     152 | 10504 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      77 | 10505 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 10506 | `						isSpecialIs = 1;` |
|       5 | 10507 | `					}` |
|      77 | 10508 | `				}` |
|     163 | 10509 | `				pInstr->iP1 = 0;` |
|     163 | 10510 | `				if( !isSpecialIs && !bAbsolute ){` |
|     141 | 10511 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      68 | 10512 | `				}` |
|      82 | 10513 | `			}` |
| 1147884 | 10514 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 10515 | `			/* Prevent constant expansion for member/property names.` |
|       - | 10516 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 10517 | `			 * should not trigger constant lookup. */` |
|  172125 | 10518 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  172125 | 10519 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  172083 | 10520 | `				pInstr->iP1 = 0;` |
|   86039 | 10521 | `			}` |
|  172125 | 10522 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 10523 | `				/* Static member access,remember that */` |
|     201 | 10524 | `				iP1 = 1;` |
|     201 | 10525 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     201 | 10526 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      38 | 10527 | `					p3 = pInstr->p3;` |
|      38 | 10528 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      17 | 10529 | `				}` |
|      98 | 10530 | `			}` |
|   86060 | 10531 | `		}` |
|       - | 10532 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 10533 | `		 * This is the primary emit path for user-visible calls. */` |
| 1177797 | 10534 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  391005 | 10535 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  195500 | 10536 | `		}` |
|       - | 10537 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1177797 | 10538 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  588896 | 10539 | `	}` |
| 1178003 | 10540 | `	if( nJmpIdx > 0 ){` |
|       - | 10541 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   13353 | 10542 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   13353 | 10543 | `		if( pInstr ){` |
|   13353 | 10544 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    6674 | 10545 | `		}` |
|    6674 | 10546 | `	}` |
| 1178003 | 10547 | `	return rc;` |
| 1551364 | 10548 |  |
|       - | 10549 | `/*` |
|       - | 10550 | ` * Compile a PHP expression.` |
|       - | 10551 | ` * According to the PHP language reference manual:` |
|       - | 10552 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 10553 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 10554 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 10555 | ` *  is "anything that has a value".` |
|       - | 10556 | ` * If something goes wrong while compiling the expression,this` |
|       - | 10557 | ` * function takes care of generating the appropriate error` |
|       - | 10558 | ` * message.` |
|       - | 10559 | ` */` |
|  834654 | 10560 | `static sxi32 PH7_CompileExpr(` |
|       - | 10561 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10562 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 10563 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 10564 | `	)` |
|       5 | 10565 |  |
|       - | 10566 | `	ph7_expr_node *pRoot;` |
|       - | 10567 | `	SySet sExprNode;` |
|       - | 10568 | `	SyToken *pEnd;` |
|       - | 10569 | `	sxi32 nExpr;` |
|       - | 10570 | `	sxi32 iNest;` |
|       - | 10571 | `	sxi32 rc;` |
|       - | 10572 | `	sxu32 nNullsafeBase;` |
|       - | 10573 | `	/* Initialize worker variables */` |
|  834659 | 10574 | `	nExpr = 0;` |
|  834659 | 10575 | `	pRoot = 0;` |
|       - | 10576 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 10577 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  834659 | 10578 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  834659 | 10579 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  834659 | 10580 | `	SySetAlloc(&sExprNode,0x10);` |
|  834659 | 10581 | `	rc = SXRET_OK;` |
|       - | 10582 | `	/* Delimit the expression */` |
|  834659 | 10583 | `	pEnd = pGen->pIn;` |
|  834659 | 10584 | `	iNest = 0;` |
| 5584313 | 10585 | `	while( pEnd < pGen->pEnd ){` |
| 5299493 | 10586 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10587 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     411 | 10588 | `			iNest++;` |
| 5299290 | 10589 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     419 | 10590 | `			iNest--;` |
| 5298880 | 10591 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  550137 | 10592 | `			if( iNest <= 0 ){` |
|  549839 | 10593 | `				break;` |
|       - | 10594 | `			}` |
|     149 | 10595 | `		}` |
| 4749659 | 10596 | `		pEnd++;` |
|       5 | 10597 | `	}` |
|  834659 | 10598 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   19319 | 10599 | `		SyToken *pEnd2 = pGen->pIn;` |
|   19319 | 10600 | `		iNest = 0;` |
|       - | 10601 | `		/* Stop at the first comma */` |
|   38897 | 10602 | `		while( pEnd2 < pEnd ){` |
|   19587 | 10603 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      59 | 10604 | `				iNest++;` |
|   19560 | 10605 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      59 | 10606 | `				iNest--;` |
|   19506 | 10607 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      49 | 10608 | `				if( iNest <= 0 ){` |
|       5 | 10609 | `					break;` |
|       - | 10610 | `				}` |
|      20 | 10611 | `			}` |
|   19583 | 10612 | `			pEnd2++;` |
|       5 | 10613 | `		}` |
|   19319 | 10614 | `		if( pEnd2 <pEnd ){` |
|       5 | 10615 | `			pEnd = pEnd2;` |
|       2 | 10616 | `		}` |
|    9657 | 10617 | `	}` |
|  834659 | 10618 | `	if( pEnd > pGen->pIn ){` |
|  834649 | 10619 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 10620 | `		/* Swap delimiter */` |
|  834649 | 10621 | `		pGen->pEnd = pEnd;` |
|       - | 10622 | `		/* Try to get an expression tree */` |
|  834649 | 10623 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  834649 | 10624 | `		if( rc == SXRET_OK && pRoot ){` |
|  834467 | 10625 | `			rc = SXRET_OK;` |
|  834467 | 10626 | `			if( xTreeValidator ){` |
|       - | 10627 | `				/* Call the upper layer validator callback */` |
|   23377 | 10628 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   11686 | 10629 | `			}` |
|  834467 | 10630 | `			if( rc != SXERR_ABORT ){` |
|       - | 10631 | `				/* Generate code for the given tree */` |
|  834467 | 10632 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 10633 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 10634 | `				 * expression so they short-circuit to its end. */` |
|  834467 | 10635 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  417231 | 10636 | `			}` |
|  834467 | 10637 | `			nExpr = 1;` |
|  417231 | 10638 | `		}` |
|       - | 10639 | `		/* Release the whole tree */` |
|  834649 | 10640 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 10641 | `		/* Synchronize token stream */` |
|  834649 | 10642 | `		pGen->pEnd = pTmp;` |
|  834649 | 10643 | `		pGen->pIn  = pEnd;` |
|  834649 | 10644 | `		if( rc == SXERR_ABORT ){` |
|      14 | 10645 | `			SySetRelease(&sExprNode);` |
|      14 | 10646 | `			return SXERR_ABORT;` |
|       - | 10647 | `		}` |
|  417317 | 10648 | `	}` |
|  834649 | 10649 | `	SySetRelease(&sExprNode);` |
|  834649 | 10650 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  417332 | 10651 |  |
|       - | 10652 | `/*` |
|       - | 10653 | ` * Return a pointer to the node construct handler associated` |
|       - | 10654 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 10655 | ` */` |
|  212600 | 10656 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 10657 |  |
|  212605 | 10658 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 10659 | `		/* Numeric literal: Either real or integer */` |
|  111569 | 10660 | `		return PH7_CompileNumLiteral;` |
|  101041 | 10661 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 10662 | `		/* Double quoted string */` |
|   20745 | 10663 | `		return PH7_CompileString;` |
|   80301 | 10664 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 10665 | `		/* Single quoted string */` |
|   80187 | 10666 | `		return PH7_CompileSimpleString;` |
|     119 | 10667 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 10668 | `		/* Heredoc */` |
|      68 | 10669 | `		return PH7_CompileHereDoc;` |
|      55 | 10670 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 10671 | `		/* Nowdoc */` |
|      48 | 10672 | `		return PH7_CompileNowDoc;` |
|       8 | 10673 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 10674 | `		/* Backtick quoted string */` |
|       6 | 10675 | `		return PH7_CompileBacktic;` |
|       - | 10676 | `	}` |
|       3 | 10677 | `	return 0;` |
|  106305 | 10678 |  |
|       - | 10679 | `/*` |
|       - | 10680 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 10681 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 10682 | ` * in write context" parse error.` |
|       - | 10683 | ` */` |
|    6756 | 10684 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 10685 |  |
|       - | 10686 | `	sxi32 rc;` |
|    6761 | 10687 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6759 | 10688 | `		return SXRET_OK;` |
|       - | 10689 | `	}` |
|       5 | 10690 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 10691 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 10692 | `		"Can't use nullsafe operator in write context");` |
|       3 | 10693 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3383 | 10694 |  |
|       - | 10695 | `/*` |
|       - | 10696 | ` * Compile an unset() statement.` |
|       - | 10697 | ` * unset($var, $arr[$key], ...);` |
|       - | 10698 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 10699 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 10700 | ` * parent array before extracting the element to unset.` |
|       - | 10701 | ` */` |
|    2908 | 10702 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 10703 |  |
|    2913 | 10704 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2913 | 10705 | `	sxu32 nIdx = 0;` |
|       - | 10706 | `	SyString sName;` |
|       - | 10707 | `	sxi32 rc;` |
|       - | 10708 | `	/* Jump the 'unset' keyword */` |
|    2913 | 10709 | `	pGen->pIn++;` |
|       - | 10710 | `	/* Save delimiter */` |
|    2913 | 10711 | `	pTmp = pGen->pEnd;` |
|       - | 10712 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2913 | 10713 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2913 | 10714 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 10715 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 10716 | `		SyToken *pClose;` |
|    2913 | 10717 | `		pGen->pIn++;   /* Skip '(' */` |
|    2913 | 10718 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2913 | 10719 | `		pEnd = pClose; /* Stop at ')' */` |
|    1454 | 10720 | `	}` |
|    2913 | 10721 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 10722 | `	/* Resolve the 'unset' builtin name once */` |
|    2913 | 10723 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     359 | 10724 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     359 | 10725 | `		if( pObj == 0 ){` |
|     ! 0 | 10726 | `			return SXERR_ABORT;` |
|       - | 10727 | `		}` |
|     359 | 10728 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     359 | 10729 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     177 | 10730 | `	}` |
|       - | 10731 | `	/* Compile each comma-separated argument */` |
|    9671 | 10732 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6763 | 10733 | `		if( pGen->pIn < pNext ){` |
|    6763 | 10734 | `			pGen->pEnd = pNext;` |
|    6763 | 10735 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 10736 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 10737 | `				GenStateUnsetValidator);` |
|    6763 | 10738 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10739 | `				return SXERR_ABORT;` |
|       - | 10740 | `			}` |
|    6763 | 10741 | `			if( rc != SXERR_EMPTY ){` |
|       - | 10742 | `				/* Emit call for this single argument */` |
|    6761 | 10743 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6761 | 10744 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6761 | 10745 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3378 | 10746 | `			}` |
|    3379 | 10747 | `		}` |
|       - | 10748 | `		/* Jump trailing commas */` |
|   10615 | 10749 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3857 | 10750 | `			pNext++;` |
|       5 | 10751 | `		}` |
|    6763 | 10752 | `		pGen->pIn = pNext;` |
|       5 | 10753 | `	}` |
|       - | 10754 | `	/* Skip past the closing ')' if present */` |
|    2913 | 10755 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2913 | 10756 | `		pGen->pIn++;` |
|    1454 | 10757 | `	}` |
|       - | 10758 | `	/* Restore token stream */` |
|    2913 | 10759 | `	pGen->pEnd = pTmp;` |
|    2913 | 10760 | `	return SXRET_OK;` |
|    1459 | 10761 |  |
|       - | 10762 | `/*` |
|       - | 10763 | ` * PHP Language construct table.` |
|       - | 10764 | ` */` |
|       - | 10765 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 10766 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 10767 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 10768 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 10769 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 10770 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 10771 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 10772 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 10773 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 10774 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 10775 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 10776 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 10777 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 10778 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 10779 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 10780 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 10781 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 10782 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 10783 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 10784 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 10785 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 10786 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 10787 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 10788 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 10789 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 10790 | `};` |
|       - | 10791 | `/*` |
|       - | 10792 | ` * Return a pointer to the statement handler routine associated` |
|       - | 10793 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 10794 | ` */` |
|  562874 | 10795 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 10796 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 10797 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 10798 | `	)` |
|       5 | 10799 |  |
|  562879 | 10800 | `	sxu32 n = 0;` |
| 2905068 | 10801 | `	for(;;){` |
| 5810141 | 10802 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  120869 | 10803 | `			break;` |
|       - | 10804 | `		}` |
| 5689277 | 10805 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  442015 | 10806 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 10807 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 10808 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 10809 | `					/* 'static' (class context),return null */` |
|     ! 0 | 10810 | `					return 0;` |
|       - | 10811 | `				}` |
|     ! 0 | 10812 | `			}` |
|  442010 | 10813 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       6 | 10814 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 10815 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 10816 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 10817 | `				return 0;` |
|       - | 10818 | `			}` |
|       - | 10819 | `			/* Return a pointer to the handler.` |
|       - | 10820 | `			*/` |
|  442015 | 10821 | `			return aLangConstruct[n].xConstruct;` |
|       - | 10822 | `		}` |
| 5247267 | 10823 | `		n++;` |
|       5 | 10824 | `	}` |
|  120869 | 10825 | `	if( pLookahed ){` |
|  120869 | 10826 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   34677 | 10827 | `			return PH7_CompileClassInterface;` |
|   86197 | 10828 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   85931 | 10829 | `			return PH7_CompileClass;` |
|     271 | 10830 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      61 | 10831 | `			return PH7_CompileTrait;` |
|     210 | 10832 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      26 | 10833 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      25 | 10834 | `				return PH7_CompileAbstractClass;` |
|     190 | 10835 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 10836 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 10837 | `				return PH7_CompileFinalClass;` |
|       - | 10838 | `		}` |
|      94 | 10839 | `	}` |
|       - | 10840 | `	/* Not a language construct */` |
|     193 | 10841 | `	return 0;` |
|  281442 | 10842 |  |
|       - | 10843 | `/*` |
|       - | 10844 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 10845 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 10846 | ` */` |
|     188 | 10847 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 10848 |  |
|       - | 10849 | `	int rc;` |
|     193 | 10850 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     193 | 10851 | `	if( rc == FALSE ){` |
|      82 | 10852 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      81 | 10853 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 10854 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 10855 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 10856 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 10857 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 10858 | `			*/` |
|       - | 10859 | `			){` |
|      79 | 10860 | `				rc = TRUE;` |
|      37 | 10861 | `		}` |
|      41 | 10862 | `	}` |
|     193 | 10863 | `	return rc;` |
|       5 | 10864 |  |
|       - | 10865 | `/*` |
|       - | 10866 | ` * Compile a PHP chunk.` |
|       - | 10867 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10868 | ` * takes care of generating the appropriate error message.` |
|       - | 10869 | ` */` |
|  674000 | 10870 | `static sxi32 GenStateCompileChunk(` |
|       - | 10871 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10872 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 10873 | `	)` |
|       5 | 10874 |  |
|       - | 10875 | `	ProcLangConstruct xCons;` |
|       - | 10876 | `	sxi32 rc;` |
|  674005 | 10877 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  525688 | 10878 | `	for(;;){` |
|  862693 | 10879 | `		int bStmtIsDeclare = 0;` |
|  862693 | 10880 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10881 | `			/* No more input to process */` |
|   13361 | 10882 | `			break;` |
|       - | 10883 | `		}` |
|       - | 10884 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 10885 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  849337 | 10886 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  562879 | 10887 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  562879 | 10888 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 10889 | `				bStmtIsDeclare = 1;` |
|      20 | 10890 | `			}` |
|  281437 | 10891 | `		}` |
|  849337 | 10892 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 10893 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 10894 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  188663 | 10895 | `			pGen->bStrictTypesLocked = 1;` |
|   94329 | 10896 | `		}` |
|  849337 | 10897 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10898 | `			/* Compile block */` |
|      21 | 10899 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      21 | 10900 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10901 | `				break;` |
|       - | 10902 | `			}` |
|      13 | 10903 | `		}else{` |
|  849321 | 10904 | `			xCons = 0;` |
|  849321 | 10905 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  562879 | 10906 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 10907 | `				/* Try to extract a language construct handler */` |
|  562879 | 10908 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  562879 | 10909 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 10910 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10911 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 10912 | `						&pGen->pIn->sData);` |
|       9 | 10913 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10914 | `						break;` |
|       - | 10915 | `					}` |
|       - | 10916 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 10917 | `					 * this erroneous statement.` |
|       - | 10918 | `					 */` |
|       9 | 10919 | `					xCons = PH7_ErrorRecover;` |
|       4 | 10920 | `				}` |
|  567884 | 10921 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   46913 | 10922 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 10923 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 10924 | `				xCons = PH7_CompileLabel;` |
|      56 | 10925 | `			}` |
|  849321 | 10926 | `			if( xCons == 0 ){` |
|       - | 10927 | `				/* Assume an expression an try to compile it */` |
|  286515 | 10928 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  286515 | 10929 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 10930 | `					/* Pop l-value */` |
|  286365 | 10931 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  143180 | 10932 | `				}` |
|  143260 | 10933 | `			}else{` |
|       - | 10934 | `				/* Go compile the sucker */` |
|  562811 | 10935 | `				rc = xCons(&(*pGen));` |
|       - | 10936 | `			}` |
|  849321 | 10937 | `			if( rc == SXERR_ABORT ){` |
|       - | 10938 | `				/* Request to abort compilation */` |
|      14 | 10939 | `				break;` |
|       - | 10940 | `			}` |
|       - | 10941 | `		}` |
|       - | 10942 | `		/* Ignore trailing semi-colons ';' */` |
| 1374465 | 10943 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  525143 | 10944 | `			pGen->pIn++;` |
|       5 | 10945 | `		}` |
|  849327 | 10946 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 10947 | `			/* Compile a single statement and return */` |
|  660639 | 10948 | `			break;` |
|       - | 10949 | `		}` |
|       - | 10950 | `		/* LOOP ONE */` |
|       - | 10951 | `		/* LOOP TWO */` |
|       - | 10952 | `		/* LOOP THREE */` |
|       - | 10953 | `		/* LOOP FOUR */` |
|       5 | 10954 | `	}` |
|       - | 10955 | `	/* Return compilation status */` |
|  674005 | 10956 | `	return rc;` |
|       5 | 10957 |  |
|       - | 10958 | `/*` |
|       - | 10959 | ` * Compile a Raw PHP chunk.` |
|       - | 10960 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10961 | ` * takes care of generating the appropriate error message.` |
|       - | 10962 | ` */` |
|   13368 | 10963 | `static sxi32 PH7_CompilePHP(` |
|       - | 10964 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10965 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 10966 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 10967 | `	)` |
|       5 | 10968 |  |
|   13373 | 10969 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 10970 | `	sxi32 rc;` |
|       - | 10971 | `	/* Reset the token set */` |
|   13373 | 10972 | `	SySetReset(&(*pTokenSet));` |
|       - | 10973 | `	/* Mark as the default token set */` |
|   13373 | 10974 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 10975 | `	/* Advance the stream cursor */` |
|   13373 | 10976 | `	pGen->pRawIn++;` |
|       - | 10977 | `	/* Tokenize the PHP chunk first */` |
|   13373 | 10978 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 10979 | `	/* Point to the head and tail of the token stream. */` |
|   13373 | 10980 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   13373 | 10981 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   13373 | 10982 | `	if( is_expr ){` |
|     ! 0 | 10983 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 10984 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 10985 | `			/* A simple expression,compile it */` |
|     ! 0 | 10986 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 10987 | `		}` |
|       - | 10988 | `		/* Emit the DONE instruction */` |
|     ! 0 | 10989 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 10990 | `		return SXRET_OK;` |
|       - | 10991 | `	}` |
|   13373 | 10992 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 10993 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 10994 | `		/*` |
|       - | 10995 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 10996 | `		 * According to the PHP reference manual:` |
|       - | 10997 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 10998 | `		 *  immediately follow` |
|       - | 10999 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 11000 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 11001 | `		 * Symisc extension:` |
|       - | 11002 | `		 *   This short syntax works with all PHP opening` |
|       - | 11003 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 11004 | `		 *   only short tag.` |
|       - | 11005 | `		 */` |
|       - | 11006 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 11007 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 11008 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11009 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11010 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11011 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11012 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11013 | `		}` |
|       3 | 11014 | `		return SXRET_OK;` |
|       - | 11015 | `	}` |
|       - | 11016 | `	/* Compile the PHP chunk */` |
|   13371 | 11017 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11018 | `	/* Fix exceptions jumps */` |
|   13371 | 11019 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11020 | `	/* Fix gotos now, the jump destination is resolved */` |
|   13371 | 11021 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11022 | `		rc = SXERR_ABORT;` |
|       1 | 11023 | `	}` |
|       - | 11024 | `	/* Reset container */` |
|   13371 | 11025 | `	SySetReset(&pGen->aGoto);` |
|   13371 | 11026 | `	SySetReset(&pGen->aLabel);` |
|   13371 | 11027 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11028 | `	/* Compilation result */` |
|   13371 | 11029 | `	return rc;` |
|    6689 | 11030 |  |
|       - | 11031 | `/*` |
|       - | 11032 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11033 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11034 | ` * This is the only compile interface exported from this file.` |
|       - | 11035 | ` */` |
|   16062 | 11036 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11037 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11038 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11039 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11040 | `	)` |
|       5 | 11041 |  |
|       - | 11042 | `	SySet aPhpToken,aRawToken;` |
|       - | 11043 | `	ph7_gen_state *pCodeGen;` |
|       - | 11044 | `	ph7_value *pRawObj;` |
|       - | 11045 | `	sxu32 nObjIdx;` |
|       - | 11046 | `	sxi32 nRawObj;` |
|       - | 11047 | `	int is_expr;` |
|       - | 11048 | `	sxi8 bSavedStrict;` |
|       - | 11049 | `	sxi8 bSavedStrictLocked;` |
|       - | 11050 | `	sxi32 rc;` |
|   16067 | 11051 | `	if( pScript->nByte < 1 ){` |
|       - | 11052 | `		/* Nothing to compile */` |
|     ! 0 | 11053 | `		return PH7_OK;` |
|       - | 11054 | `	}` |
|       - | 11055 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11056 | `	 * file's flags so include/require restore them on return. */` |
|   16067 | 11057 | `	pCodeGen = &pVm->sCodeGen;` |
|   16067 | 11058 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   16067 | 11059 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   16067 | 11060 | `	pCodeGen->bStrictTypes = 0;` |
|   16067 | 11061 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11062 | `	/* Initialize the tokens containers */` |
|   16067 | 11063 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16067 | 11064 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16067 | 11065 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   16067 | 11066 | `	is_expr = 0;` |
|   16067 | 11067 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11068 | `		SyToken sTmp;` |
|       - | 11069 | `		/* PHP only: -*/` |
|    3217 | 11070 | `		sTmp.nLine = 1;` |
|    3217 | 11071 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3217 | 11072 | `		sTmp.pUserData = 0;` |
|    3217 | 11073 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3217 | 11074 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3217 | 11075 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11076 | `			/* A simple PHP expression */` |
|     ! 0 | 11077 | `			is_expr = 1;` |
|     ! 0 | 11078 | `		}` |
|    1611 | 11079 | `	}else{` |
|       - | 11080 | `		/* Tokenize raw text */` |
|   12855 | 11081 | `		SySetAlloc(&aRawToken,32);` |
|   12855 | 11082 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11083 | `	}` |
|       - | 11084 | `	/* Process high-level tokens */` |
|   16067 | 11085 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   16067 | 11086 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   16067 | 11087 | `	rc = PH7_OK;` |
|   16067 | 11088 | `	if( is_expr ){` |
|       - | 11089 | `		/* Compile the expression */` |
|     ! 0 | 11090 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 11091 | `		goto cleanup;` |
|       - | 11092 | `	}` |
|   16067 | 11093 | `	nObjIdx = 0;` |
|       - | 11094 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 11095 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 11096 | `	 * preventing namespace bleeding across include()d files. */` |
|   16067 | 11097 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 11098 | `	/* Start the compilation process */` |
|   14462 | 11099 | `	for(;;){` |
|   42285 | 11100 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   16055 | 11101 | `			break; /* No more tokens to process */` |
|       - | 11102 | `		}` |
|   26235 | 11103 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 11104 | `			/* Compile the PHP chunk */` |
|   13373 | 11105 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   13373 | 11106 | `			if( rc == SXERR_ABORT ){` |
|      16 | 11107 | `				break;` |
|       - | 11108 | `			}` |
|   13361 | 11109 | `			continue;` |
|       - | 11110 | `		}` |
|       - | 11111 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   12867 | 11112 | `		nRawObj = 0;` |
|   25771 | 11113 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 11114 | `			/* Consume the raw chunk without any processing */` |
|   12909 | 11115 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   12909 | 11116 | `			if( pRawObj == 0 ){` |
|     ! 0 | 11117 | `				rc = SXERR_MEM;` |
|     ! 0 | 11118 | `				break;` |
|       - | 11119 | `			}` |
|       - | 11120 | `			/* Mark as constant and emit the load constant instruction */` |
|   12909 | 11121 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   12909 | 11122 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   12909 | 11123 | `			++nRawObj;` |
|   12909 | 11124 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 11125 | `		}` |
|   12867 | 11126 | `		if( nRawObj > 0 ){` |
|       - | 11127 | `			/* Emit the consume instruction */` |
|   12867 | 11128 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6431 | 11129 | `		}` |
|    8036 | 11130 | `	}` |
|    8031 | 11131 | `cleanup:` |
|   16067 | 11132 | `	SySetRelease(&aRawToken);` |
|   16067 | 11133 | `	SySetRelease(&aPhpToken);` |
|       - | 11134 | `	/* Restore outer file's strict_types scope */` |
|   16067 | 11135 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   16067 | 11136 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   16067 | 11137 | `	return rc;` |
|    8036 | 11138 |  |
|       - | 11139 | `/*` |
|       - | 11140 | ` * Utility routines.Initialize the code generator.` |
|       - | 11141 | ` */` |
|    3148 | 11142 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 11143 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11144 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11145 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11146 | `	)` |
|       5 | 11147 |  |
|    3153 | 11148 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11149 | `	/* Zero the structure */` |
|    3153 | 11150 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 11151 | `	/* Initial state */` |
|    3153 | 11152 | `	pGen->pVm  = &(*pVm);` |
|    3153 | 11153 | `	pGen->xErr = xErr;` |
|    3153 | 11154 | `	pGen->pErrData = pErrData;` |
|    3153 | 11155 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3153 | 11156 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3153 | 11157 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3153 | 11158 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3153 | 11159 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 11160 | `	/* Error log buffer */` |
|    3153 | 11161 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 11162 | `	/* General purpose working buffer */` |
|    3153 | 11163 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 11164 | `	/* Namespace state */` |
|    3153 | 11165 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3153 | 11166 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3153 | 11167 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3153 | 11168 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11169 | `	/* Create the global scope */` |
|    3153 | 11170 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 11171 | `	/* Point to the global scope */` |
|    3153 | 11172 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3153 | 11173 | `	return SXRET_OK;` |
|       5 | 11174 |  |
|       - | 11175 | `/*` |
|       - | 11176 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 11177 | ` */` |
|   18896 | 11178 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 11179 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11180 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11181 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11182 | `	)` |
|       5 | 11183 |  |
|   18901 | 11184 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11185 | `	GenBlock *pBlock,*pParent;` |
|       - | 11186 | `	/* Reset state */` |
|   18901 | 11187 | `	SySetReset(&pGen->aLabel);` |
|   18901 | 11188 | `	SySetReset(&pGen->aGoto);` |
|   18901 | 11189 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   18901 | 11190 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   18901 | 11191 | `	SyBlobRelease(&pGen->sWorker);` |
|   18901 | 11192 | `	SyBlobRelease(&pGen->sNamespace);` |
|   18901 | 11193 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   18901 | 11194 | `	SyHashRelease(&pGen->hUseImports);` |
|   18901 | 11195 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   18901 | 11196 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   18901 | 11197 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   18901 | 11198 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   18901 | 11199 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11200 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 11201 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 11202 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 11203 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 11204 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 11205 | `	 * number of unique names, which is acceptable. */` |
|       - | 11206 | `	/* Point to the global scope */` |
|   18901 | 11207 | `	pBlock = pGen->pCurrent;` |
|   18901 | 11208 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 11209 | `		pParent = pBlock->pParent;` |
|     ! 0 | 11210 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 11211 | `		pBlock = pParent;` |
|     ! 0 | 11212 | `	}` |
|   18901 | 11213 | `	pGen->xErr = xErr;` |
|   18901 | 11214 | `	pGen->pErrData = pErrData;` |
|   18901 | 11215 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   18901 | 11216 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   18901 | 11217 | `	pGen->pIn = pGen->pEnd = 0;` |
|   18901 | 11218 | `	pGen->nErr = 0;` |
|   18901 | 11219 | `	return SXRET_OK;` |
|       5 | 11220 |  |
|       - | 11221 | `/*` |
|       - | 11222 | ` * Generate a compile-time error message.` |
|       - | 11223 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 11224 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 11225 | ` * abort compilation immediately.` |
|       - | 11226 | ` */` |
|     574 | 11227 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 11228 |  |
|     579 | 11229 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     579 | 11230 | `	const char *zErr = "Error";` |
|       - | 11231 | `	SyString *pFile;` |
|       - | 11232 | `	va_list ap;` |
|       - | 11233 | `	sxi32 rc;` |
|       - | 11234 | `	/* Reset the working buffer */` |
|     579 | 11235 | `	SyBlobReset(pWorker);` |
|       - | 11236 | `	/* Peek the processed file path if available */` |
|     579 | 11237 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     579 | 11238 | `	if( nErrType == E_ERROR ){` |
|       - | 11239 | `		/* Increment the error counter */` |
|     473 | 11240 | `		pGen->nErr++;` |
|     473 | 11241 | `		if( pGen->nErr > 15 ){` |
|       - | 11242 | `			/* Error count limit reached */` |
|       5 | 11243 | `			if( pGen->xErr ){` |
|       5 | 11244 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 11245 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 11246 | `				if( pFile ){` |
|       5 | 11247 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 11248 | `				}` |
|       5 | 11249 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 11250 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 11251 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 11252 | `				}` |
|       2 | 11253 | `			}` |
|       - | 11254 | `			/* Abort immediately */` |
|       5 | 11255 | `			return SXERR_ABORT;` |
|       - | 11256 | `		}` |
|     232 | 11257 | `	}` |
|     575 | 11258 | `	if( pGen->xErr == 0 ){` |
|       - | 11259 | `		/* No available error consumer,return immediately */` |
|       3 | 11260 | `		return SXRET_OK;` |
|       - | 11261 | `	}` |
|     572 | 11262 | `	switch(nErrType){` |
|     466 | 11263 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 11264 | `	case E_WARNING: zErr = "Warning";     break;` |
|      76 | 11265 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      12 | 11266 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 11267 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 11268 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 11269 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 11270 | `	default:` |
|     ! 0 | 11271 | `		break;` |
|       - | 11272 | `	}` |
|     572 | 11273 | `	rc = SXRET_OK;` |
|       - | 11274 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     572 | 11275 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     572 | 11276 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     572 | 11277 | `	va_start(ap,zFormat);` |
|     572 | 11278 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     572 | 11279 | `	va_end(ap);` |
|     572 | 11280 | `	if( pFile ){` |
|     572 | 11281 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     284 | 11282 | `	}` |
|       - | 11283 | `	/* Append a new line */` |
|     572 | 11284 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     572 | 11285 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 11286 | `		/* Consume the generated error message */` |
|     572 | 11287 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     284 | 11288 | `	}` |
|     572 | 11289 | `	return rc;` |
|     292 | 11290 |  |
|       - | 11291 |  |
