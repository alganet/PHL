# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5315/6636 lines (80.09%)

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
|    3442 |   131 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   132 |  |
|    3447 |   133 | `	GenBlock *pBlock = pCurrent;` |
|    9743 |   134 | `	for(;;){` |
|   19491 |   135 | `		if( pBlock->iFlags & iBlockType ){` |
|    3339 |   136 | `			iCount--; /* Decrement nesting level */` |
|    3339 |   137 | `			if( iCount < 1 ){` |
|       - |   138 | `				/* Block meet with the desired criteria */` |
|    3313 |   139 | `				return pBlock;` |
|       - |   140 | `			}` |
|      13 |   141 | `		}` |
|       - |   142 | `		/* Point to the upper block */` |
|   16183 |   143 | `		pBlock = pBlock->pParent;` |
|   16183 |   144 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   145 | `			/* Forbidden */` |
|      72 |   146 | `			break;` |
|       - |   147 | `		}` |
|       5 |   148 | `	}` |
|       - |   149 | `	/* No such block */` |
|     139 |   150 | `	return 0;` |
|    1726 |   151 |  |
|       - |   152 | `/*` |
|       - |   153 | ` * Initialize a freshly allocated block instance.` |
|       - |   154 | ` */` |
|  747086 |   155 | `static void GenStateInitBlock(` |
|       - |   156 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   157 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   158 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   159 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   160 | `	void *pUserData      /* Upper layer private data */` |
|       - |   161 | `	)` |
|       5 |   162 |  |
|       - |   163 | `	/* Initialize block fields */` |
|  747091 |   164 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  747091 |   165 | `	pBlock->pUserData   = pUserData;` |
|  747091 |   166 | `	pBlock->pGen        = pGen;` |
|  747091 |   167 | `	pBlock->iFlags      = iType;` |
|  747091 |   168 | `	pBlock->pParent     = 0;` |
|  747091 |   169 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  747091 |   170 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  747091 |   171 |  |
|       - |   172 | `/*` |
|       - |   173 | ` * Allocate a new block instance.` |
|       - |   174 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   175 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   176 | ` * processing on failure.` |
|       - |   177 | ` */` |
|  743922 |   178 | `static sxi32 GenStateEnterBlock(` |
|       - |   179 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   180 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   181 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   182 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   183 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   184 | `	)` |
|       5 |   185 |  |
|       - |   186 | `	GenBlock *pBlock;` |
|       - |   187 | `	/* Allocate a new block instance */` |
|  743927 |   188 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  743927 |   189 | `	if( pBlock == 0 ){` |
|       - |   190 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   191 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   192 | `		 */` |
|     ! 0 |   193 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   194 | `		/* Abort processing immediately */` |
|     ! 0 |   195 | `		return SXERR_ABORT;` |
|       - |   196 | `	}` |
|       - |   197 | `	/* Zero the structure */` |
|  743927 |   198 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  743927 |   199 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   200 | `	/* Link to the parent block */` |
|  743927 |   201 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   202 | `	/* Mark as the current block */` |
|  743927 |   203 | `	pGen->pCurrent = pBlock;` |
|  743927 |   204 | `	if( ppBlock ){` |
|       - |   205 | `		/* Write a pointer to the new instance */` |
|  361339 |   206 | `		*ppBlock = pBlock;` |
|  180667 |   207 | `	}` |
|  743927 |   208 | `	return SXRET_OK;` |
|  371966 |   209 |  |
|       - |   210 | `/*` |
|       - |   211 | ` * Release block fields without freeing the whole instance.` |
|       - |   212 | ` */` |
|  743914 |   213 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   214 |  |
|  743919 |   215 | `	SySetRelease(&pBlock->aPostContFix);` |
|  743919 |   216 | `	SySetRelease(&pBlock->aJumpFix);` |
|  743919 |   217 |  |
|       - |   218 | `/*` |
|       - |   219 | ` * Release a block.` |
|       - |   220 | ` */` |
|  743914 |   221 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   222 |  |
|  743919 |   223 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  743919 |   224 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   225 | `	/* Free the instance */` |
|  743919 |   226 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  743919 |   227 |  |
|       - |   228 | `/*` |
|       - |   229 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   230 | ` */` |
|  743914 |   231 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   232 |  |
|  743919 |   233 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  743919 |   234 | `	if( pBlock == 0 ){` |
|       - |   235 | `		/* No more block to pop */` |
|     ! 0 |   236 | `		return SXERR_EMPTY;` |
|       - |   237 | `	}` |
|       - |   238 | `	/* Point to the upper block */` |
|  743919 |   239 | `	pGen->pCurrent = pBlock->pParent;` |
|  743919 |   240 | `	if( ppBlock ){` |
|       - |   241 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   242 | `		*ppBlock = pBlock;` |
|     ! 0 |   243 | `	}else{` |
|       - |   244 | `		/* Safely release the block */` |
|  743919 |   245 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   246 | `	}` |
|  743919 |   247 | `	return SXRET_OK;` |
|  371962 |   248 |  |
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
|  211234 |   259 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   260 |  |
|       - |   261 | `	JumpFixup sJumpFix;` |
|       - |   262 | `	sxi32 rc;` |
|       - |   263 | `	/* Init the JumpFixup structure */` |
|  211239 |   264 | `	sJumpFix.nJumpType = nJumpType;` |
|  211239 |   265 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   266 | `	/* Insert in the jump fixup table */` |
|  211239 |   267 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  211239 |   268 | `	return rc;` |
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
|  520318 |   281 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   282 |  |
|       - |   283 | `	JumpFixup *aFix;` |
|       - |   284 | `	VmInstr *pInstr;` |
|       - |   285 | `	sxu32 nFixed;` |
|       - |   286 | `	sxu32 n;` |
|       - |   287 | `	/* Point to the jump fixup table */` |
|  520323 |   288 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   289 | `	/* Fix the desired jumps */` |
|  936501 |   290 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  416183 |   291 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   292 | `			/* Already fixed */` |
|  166175 |   293 | `			continue;` |
|       - |   294 | `		}` |
|  250013 |   295 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   296 | `			/* Not of our interest */` |
|   38781 |   297 | `			continue;` |
|       - |   298 | `		}` |
|       - |   299 | `		/* Point to the instruction to fix */` |
|  211237 |   300 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  211237 |   301 | `		if( pInstr ){` |
|  211237 |   302 | `			pInstr->iP2 = nJumpDest;` |
|  211237 |   303 | `			nFixed++;` |
|       - |   304 | `			/* Mark as fixed */` |
|  211237 |   305 | `			aFix[n].nJumpType = -1;` |
|  105616 |   306 | `		}` |
|  105621 |   307 | `	}` |
|       - |   308 | `	/* Total number of fixed jumps */` |
|  520323 |   309 | `	return nFixed;` |
|       5 |   310 |  |
|       - |   311 | `/*` |
|       - |   312 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   313 | ` * The goto statement can be used to jump to another section` |
|       - |   314 | ` * in the program.` |
|       - |   315 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   316 | ` * statement for more information.` |
|       - |   317 | ` */` |
|  211446 |   318 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   319 |  |
|       - |   320 | `	JumpFixup *pJump,*aJumps;` |
|       - |   321 | `	Label *pLabel,*aLabel;` |
|       - |   322 | `	VmInstr *pInstr;` |
|       - |   323 | `	sxi32 rc;` |
|       - |   324 | `	sxu32 n;` |
|       - |   325 | `	/* Point to the goto table */` |
|  211451 |   326 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   327 | `	/* Fix */` |
|  211597 |   328 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  211449 |   353 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  211581 |   354 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   355 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   356 | `			/* Emit a warning */` |
|      40 |   357 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   358 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   359 | `		}` |
|      71 |   360 | `	}` |
|  211449 |   361 | `	return SXRET_OK;` |
|  105728 |   362 |  |
|       - |   363 | `/*` |
|       - |   364 | ` * Check if a given token value is installed in the literal table.` |
|       - |   365 | ` */` |
|  668366 |   366 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   367 |  |
|       - |   368 | `	SyHashEntry *pEntry;` |
|  668371 |   369 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  668371 |   370 | `	if( pEntry == 0 ){` |
|  290439 |   371 | `		return SXERR_NOTFOUND;` |
|       - |   372 | `	}` |
|  377937 |   373 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  377937 |   374 | `	return SXRET_OK;` |
|  334188 |   375 |  |
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
|  290434 |   386 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   387 |  |
|  290439 |   388 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  290439 |   389 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  145217 |   390 | `	}` |
|  290439 |   391 | `	return SXRET_OK;` |
|       5 |   392 |  |
|       - |   393 | `/*` |
|       - |   394 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   395 | ` * in the constant table.` |
|       - |   396 | ` */` |
|  111464 |   397 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   398 |  |
|       - |   399 | `	ph7_value *pObj;` |
|  111469 |   400 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   401 | `	/* Reserve a new constant */` |
|  111469 |   402 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  111469 |   403 | `	if( pObj == 0 ){` |
|     ! 0 |   404 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   405 | `		return 0;` |
|       - |   406 | `	}` |
|  111469 |   407 | `	*pIdx = nIdx;` |
|       - |   408 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   409 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   410 | `	 */` |
|  111469 |   411 | `	return pObj;` |
|   55737 |   412 |  |
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
|  399736 |   427 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   428 |  |
|       - |   429 | `	VmCallArgMap *pMap;` |
|  399741 |   430 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   431 | `	if( p3 == 0 ){` |
|      31 |   432 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   433 | `		if( pMap == 0 ) return 0;` |
|      31 |   434 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   435 | `		p3 = (void *)pMap;` |
|      14 |   436 | `	}` |
|      33 |   437 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   438 | `	return p3;` |
|  199873 |   439 |  |
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
|  112054 |   498 | `static int GenStateFindBadNumericSeparator(` |
|       - |   499 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   500 |  |
|  112059 |   501 | `	const char *z = pRaw->zString;` |
|  112059 |   502 | `	sxu32 n = pRaw->nByte;` |
|  112059 |   503 | `	int base = 10;` |
|       - |   504 | `	sxu32 i, start;` |
|  112059 |   505 | `	if( n < 2 ) return 0;` |
|    9427 |   506 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   507 | `		base = 16;` |
|    9392 |   508 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   509 | `		base = 2;` |
|     139 |   510 | `	}` |
|   34421 |   511 | `	for( i = 0; i < n; ++i ){` |
|   25013 |   512 | `		if( z[i] != '_' ) continue;` |
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
|    9413 |   529 | `	return 0;` |
|   56032 |   530 |  |
|       - |   531 | `/*` |
|       - |   532 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   533 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   534 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   535 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   536 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   537 | ` * so callers can bail from the current construct).` |
|       - |   538 | ` */` |
|  112054 |   539 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   540 |  |
|  112059 |   541 | `	const char *zBad = 0;` |
|  112059 |   542 | `	sxu32 nBad = 0;` |
|       - |   543 | `	SyString sBad;` |
|       - |   544 | `	sxi32 rc;` |
|  112059 |   545 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  112045 |   546 | `		return SXRET_OK;` |
|       - |   547 | `	}` |
|      18 |   548 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   549 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   550 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   551 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   552 | `		return SXERR_ABORT;` |
|       - |   553 | `	}` |
|      18 |   554 | `	return SXERR_SYNTAX;` |
|   56032 |   555 |  |
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
|  112040 |   572 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   573 | `	SyMemBackend *pAlloc,` |
|       - |   574 | `	const SyString *pToken,` |
|       - |   575 | `	char *zScratch, sxu32 nScratch,` |
|       - |   576 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   577 |  |
|       - |   578 | `	sxu32 i, j;` |
|  112045 |   579 | `	int hasUnderscore = 0;` |
|       - |   580 | `	char *zBuf;` |
|  112045 |   581 | `	*pzAlloc = 0;` |
|  237605 |   582 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  125817 |   583 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   62785 |   584 | `	}` |
|  112045 |   585 | `	if( !hasUnderscore ){` |
|  111793 |   586 | `		SyStringDupPtr(pOut, pToken);` |
|  111793 |   587 | `		return SXRET_OK;` |
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
|   56025 |   604 |  |
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
|  112026 |   621 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   622 |  |
|  112031 |   623 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  112031 |   624 | `	sxu32 nIdx = 0;` |
|       - |   625 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  112031 |   626 | `	char *zAlloc = 0;` |
|       - |   627 | `	SyString sNum;` |
|       - |   628 | `	sxi32 rc;` |
|   56013 |   629 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  112031 |   630 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  112031 |   631 | `	if( rc != SXRET_OK ){` |
|      14 |   632 | `		return rc;` |
|       - |   633 | `	}` |
|  168029 |   634 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   56008 |   635 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  112021 |   636 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   637 | `		return SXERR_ABORT;` |
|       - |   638 | `	}` |
|  112021 |   639 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   640 | `		ph7_value *pObj;` |
|       - |   641 | `		sxi64 iValue;` |
|  111469 |   642 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  111469 |   643 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  111469 |   644 | `		if( pObj == 0 ){` |
|     ! 0 |   645 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   646 | `			return SXERR_ABORT;` |
|       - |   647 | `		}` |
|  111469 |   648 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   55737 |   649 | `	}else{` |
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
|  112021 |   662 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   663 | `	/* Emit the load constant instruction */` |
|  112021 |   664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   665 | `	/* Node successfully compiled */` |
|  112021 |   666 | `	return SXRET_OK;` |
|   56018 |   667 |  |
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
|   80536 |   679 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   680 |  |
|   80541 |   681 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   682 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   683 | `	ph7_value *pObj;` |
|       - |   684 | `	sxu32 nIdx;` |
|   80541 |   685 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   686 | `	/* Delimit the string */` |
|   80541 |   687 | `	zIn  = pStr->zString;` |
|   80541 |   688 | `	zEnd = &zIn[pStr->nByte];` |
|   80541 |   689 | `	if( zIn >= zEnd ){` |
|       - |   690 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   691 | `		 * rather than reserving a new object each time. */` |
|    6485 |   692 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    6485 |   693 | `		return SXRET_OK;` |
|       - |   694 | `	}` |
|   74061 |   695 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   696 | `		/* Already processed,emit the load constant instruction` |
|       - |   697 | `		 * and return.` |
|       - |   698 | `		 */` |
|   29515 |   699 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   29515 |   700 | `		return SXRET_OK;` |
|       - |   701 | `	}` |
|       - |   702 | `	/* Reserve a new constant */` |
|   44551 |   703 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   44551 |   704 | `	if( pObj == 0 ){` |
|     ! 0 |   705 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   706 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   707 | `		return SXERR_ABORT;` |
|       - |   708 | `	}` |
|   44551 |   709 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   710 | `	/* Compile the node */` |
|   44598 |   711 | `	for(;;){` |
|   89201 |   712 | `		if( zIn >= zEnd ){` |
|       - |   713 | `			/* End of input */` |
|   44551 |   714 | `			break;` |
|       - |   715 | `		}` |
|   44655 |   716 | `		zCur = zIn;` |
|  702835 |   717 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  658185 |   718 | `			zIn++;` |
|       5 |   719 | `		}` |
|   44655 |   720 | `		if( zIn > zCur ){` |
|       - |   721 | `			/* Append raw contents*/` |
|   44631 |   722 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   22313 |   723 | `		}` |
|   44655 |   724 | `		zIn++;` |
|   44655 |   725 | `		if( zIn < zEnd ){` |
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
|   44655 |   740 | `		zIn++;` |
|       5 |   741 | `	}` |
|       - |   742 | `	/* Emit the load constant instruction */` |
|   44551 |   743 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   44551 |   744 | `	if( pStr->nByte < 1024 ){` |
|       - |   745 | `		/* Install in the literal table */` |
|   44551 |   746 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   22273 |   747 | `	}` |
|       - |   748 | `	/* Node successfully compiled */` |
|   44551 |   749 | `	return SXRET_OK;` |
|   40273 |   750 |  |
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
|   22290 |   950 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   951 |  |
|       - |   952 | `	ph7_value *pConstObj;` |
|   22295 |   953 | `	sxu32 nIdx = 0;` |
|       - |   954 | `	/* Reserve a new constant */` |
|   22295 |   955 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   22295 |   956 | `	if( pConstObj == 0 ){` |
|     ! 0 |   957 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   958 | `		return 0;` |
|       - |   959 | `	}` |
|   22295 |   960 | `	(*pCount)++;` |
|   22295 |   961 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   962 | `	/* Emit the load constant instruction */` |
|   22295 |   963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   22295 |   964 | `	return pConstObj;` |
|   11150 |   965 |  |
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
|   20844 |  1004 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1005 |  |
|   20849 |  1006 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1007 | `	const char *zIn,*zCur,*zEnd;` |
|   20849 |  1008 | `	ph7_value *pObj = 0;` |
|       - |  1009 | `	sxi32 iCons;` |
|       - |  1010 | `	sxi32 rc;` |
|       - |  1011 | `	/* Delimit the string */` |
|   20849 |  1012 | `	zIn  = pStr->zString;` |
|   20849 |  1013 | `	zEnd = &zIn[pStr->nByte];` |
|   20849 |  1014 | `	if( zIn >= zEnd ){` |
|       - |  1015 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1016 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1017 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1018 | `		 */` |
|     277 |  1019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     277 |  1020 | `		return SXRET_OK;` |
|       - |  1021 | `	}` |
|   20577 |  1022 | `	zCur = 0;` |
|       - |  1023 | `	/* Compile the node */` |
|   20577 |  1024 | `	iCons = 0;` |
|   11315 |  1025 | `	for(;;){` |
|   34063 |  1026 | `		zCur = zIn;` |
|  166003 |  1027 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  134003 |  1028 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1029 | `				break;` |
|  133877 |  1030 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1934 |  1031 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     970 |  1032 | `					break;` |
|       - |  1033 | `			}` |
|  131945 |  1034 | `			zIn++;` |
|       5 |  1035 | `		}` |
|   34063 |  1036 | `		if( zIn > zCur ){` |
|   15725 |  1037 | `			if( pObj == 0 ){` |
|   15345 |  1038 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   15345 |  1039 | `				if( pObj == 0 ){` |
|     ! 0 |  1040 | `					return SXERR_ABORT;` |
|       - |  1041 | `				}` |
|    7670 |  1042 | `			}` |
|   15725 |  1043 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    7860 |  1044 | `		}` |
|   34063 |  1045 | `		if( zIn >= zEnd ){` |
|   20577 |  1046 | `			break;` |
|       - |  1047 | `		}` |
|   13491 |  1048 | `		if( zIn[0] == '\\' ){` |
|   11433 |  1049 | `			const char *zPtr = 0;` |
|       - |  1050 | `			sxu32 n;` |
|   11433 |  1051 | `			zIn++;` |
|   11433 |  1052 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1053 | `				break;` |
|       - |  1054 | `			}` |
|   11433 |  1055 | `			if( pObj == 0 ){` |
|    6955 |  1056 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    6955 |  1057 | `				if( pObj == 0 ){` |
|     ! 0 |  1058 | `					return SXERR_ABORT;` |
|       - |  1059 | `				}` |
|    3475 |  1060 | `			}` |
|   11433 |  1061 | `			n = sizeof(char); /* size of conversion */` |
|   11433 |  1062 | `			switch( zIn[0] ){` |
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
|    5288 |  1083 | `			case 'n':` |
|       - |  1084 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   10581 |  1085 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   10581 |  1086 | `				break;` |
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
|   11433 |  1154 | `			zIn += n;` |
|   11433 |  1155 | `			continue;` |
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
|   20577 |  1273 | `	if( iCons > 1 ){` |
|       - |  1274 | `		/* Concatenate all compiled constants */` |
|    1535 |  1275 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     765 |  1276 | `	}` |
|       - |  1277 | `	/* Node successfully compiled */` |
|   20577 |  1278 | `	return SXRET_OK;` |
|   10427 |  1279 |  |
|       - |  1280 | `/*` |
|       - |  1281 | ` * Compile a double quoted string.` |
|       - |  1282 | ` *  See the block-comment above for more information.` |
|       - |  1283 | ` */` |
|   20784 |  1284 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1285 |  |
|       - |  1286 | `	sxi32 rc;` |
|   20789 |  1287 | `	rc = GenStateCompileString(&(*pGen));` |
|   10392 |  1288 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1289 | `	/* Compilation result */` |
|   20789 |  1290 | `	return rc;` |
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
|   19162 |  1334 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   19167 |  1345 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1346 | `	/* Compile the expression*/` |
|   19167 |  1347 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1348 | `	/* Restore token stream */` |
|   19167 |  1349 | `	RE_SWAP_DELIMITER(pGen);` |
|   19167 |  1350 | `	return rc;` |
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
|   27856 |  1389 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1390 |  |
|       - |  1391 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1392 | `	SyToken *pKey,*pCur;` |
|   27861 |  1393 | `	sxi32 iEmitRef = 0;` |
|   27861 |  1394 | `	sxi32 iSpread = 0;` |
|   27861 |  1395 | `	sxi32 nPair = 0;` |
|       - |  1396 | `	sxi32 iNest;` |
|       - |  1397 | `	sxi32 rc;` |
|   27861 |  1398 | `	xValidator = 0;` |
|   22720 |  1399 | `	for(;;){` |
|       - |  1400 | `		/* Jump leading commas */` |
|   51415 |  1401 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5975 |  1402 | `			pGen->pIn++;` |
|       5 |  1403 | `		}` |
|   45445 |  1404 | `		pCur = pGen->pIn;` |
|   45445 |  1405 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1406 | `			/* No more entry to process */` |
|   27845 |  1407 | `			break;` |
|       - |  1408 | `		}` |
|   17605 |  1409 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1410 | `			continue;` |
|       - |  1411 | `		}` |
|       - |  1412 | `		/* Compile the key if available */` |
|   17605 |  1413 | `		pKey = pCur;` |
|   17605 |  1414 | `		iNest = 0;` |
|   49219 |  1415 | `		while( pCur < pGen->pIn ){` |
|   33083 |  1416 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1465 |  1417 | `				break;` |
|       - |  1418 | `			}` |
|       - |  1419 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1420 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1421 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1422 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1423 | `			 */` |
|   31623 |  1424 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   31617 |  1488 | `			if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     265 |  1489 | `				iNest++;` |
|   31486 |  1490 | `			}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1491 | `				/* Don't worry about mismatched brackets here,the expression` |
|       - |  1492 | `				 * parser will shortly detect any syntax error.` |
|       - |  1493 | `				 */` |
|     265 |  1494 | `				iNest--;` |
|     131 |  1495 | `			}` |
|   31617 |  1496 | `			pCur++;` |
|       5 |  1497 | `		}` |
|   17605 |  1498 | `		rc = SXERR_EMPTY;` |
|   17605 |  1499 | `		if( pCur < pGen->pIn ){` |
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
|   16870 |  1515 | `		}else if( pKey == pCur ){` |
|       - |  1516 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1517 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1518 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1519 | `		}else{` |
|       - |  1520 | `			/* Reset back the cursor and point to the entry value */` |
|   16145 |  1521 | `			pCur = pKey;` |
|       - |  1522 | `		}` |
|   17595 |  1523 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1524 | `			/* No available key,load NULL */` |
|   16147 |  1525 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    8071 |  1526 | `		}` |
|   17595 |  1527 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   17593 |  1546 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   17593 |  1547 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   17589 |  1560 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   17589 |  1561 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1562 | `			return SXERR_ABORT;` |
|       - |  1563 | `		}` |
|   17589 |  1564 | `		if( iSpread ){` |
|       - |  1565 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1566 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   17558 |  1567 | `		}else if( iEmitRef ){` |
|       - |  1568 | `			/* Emit the load reference instruction */` |
|      40 |  1569 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1570 | `		}` |
|   17589 |  1571 | `		xValidator = 0;` |
|   17589 |  1572 | `		iEmitRef = 0;` |
|   17589 |  1573 | `		iSpread = 0;` |
|   17589 |  1574 | `		nPair++;` |
|       5 |  1575 | `	}` |
|       - |  1576 | `	/* Emit the load map instruction */` |
|   27845 |  1577 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1578 | `	/* Node successfully compiled */` |
|   27845 |  1579 | `	return SXRET_OK;` |
|   13933 |  1580 |  |
|       - |  1581 | `/*` |
|       - |  1582 | ` * Compile the 'array' language construct.` |
|       - |  1583 | ` *	 According to the PHP language reference manual` |
|       - |  1584 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1585 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1586 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1587 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1588 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1589 | ` */` |
|   27116 |  1590 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1591 |  |
|       - |  1592 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   27121 |  1593 | `	pGen->pIn += 2;` |
|   27121 |  1594 | `	pGen->pEnd--;` |
|   13558 |  1595 | `	SXUNUSED(iCompileFlag);` |
|   27121 |  1596 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1597 |  |
|       - |  1598 | `/*` |
|       - |  1599 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1600 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1601 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1602 | ` */` |
|     740 |  1603 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1604 |  |
|       - |  1605 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     745 |  1606 | `	pGen->pIn++;` |
|     745 |  1607 | `	pGen->pEnd--;` |
|     370 |  1608 | `	SXUNUSED(iCompileFlag);` |
|     745 |  1609 | `	return GenStateCompileArrayBody(pGen);` |
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
|       - |  1797 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|       - |  1798 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|       - |  1799 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1800 | `/*` |
|       - |  1801 | ` * Compile an annoynmous function or a closure.` |
|       - |  1802 | ` * According to the PHP language reference` |
|       - |  1803 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1804 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1805 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1806 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1807 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1808 | ` *  Example Anonymous function variable assignment example` |
|       - |  1809 | ` * <?php` |
|       - |  1810 | ` * $greet = function($name)` |
|       - |  1811 | ` * {` |
|       - |  1812 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1813 | ` * };` |
|       - |  1814 | ` * $greet('World');` |
|       - |  1815 | ` * $greet('PHP');` |
|       - |  1816 | ` * ?>` |
|       - |  1817 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1818 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1819 | ` */` |
|     246 |  1820 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1821 |  |
|       - |  1822 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1823 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1824 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1825 | `							  * one thread is allowed to compile the script.` |
|       - |  1826 | `						      */` |
|       - |  1827 | `	ph7_value *pObj;` |
|       - |  1828 | `	SyString sName;` |
|       - |  1829 | `	sxu32 nIdx;` |
|       - |  1830 | `	sxu32 nLen;` |
|       - |  1831 | `	sxi32 rc;` |
|       - |  1832 |  |
|     251 |  1833 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     251 |  1834 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1835 | `		pGen->pIn++;` |
|     ! 0 |  1836 | `	}` |
|       - |  1837 | `	/* Reserve a constant for the lambda */` |
|     251 |  1838 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     251 |  1839 | `	if( pObj == 0 ){` |
|     ! 0 |  1840 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1841 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1842 | `		return SXERR_ABORT;` |
|       - |  1843 | `	}` |
|       - |  1844 | `	/* Generate a unique name */` |
|     251 |  1845 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1846 | `	/* Make sure the generated name is unique */` |
|     251 |  1847 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1848 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1849 | `	}` |
|     251 |  1850 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     251 |  1851 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - |  1852 | `	/* Compile the lambda body */` |
|     251 |  1853 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     251 |  1854 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1855 | `		return SXERR_ABORT;` |
|       - |  1856 | `	}` |
|     251 |  1857 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - |  1858 | `		/* Emit the load closure instruction */` |
|      21 |  1859 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|      13 |  1860 | `	}else{` |
|       - |  1861 | `		/* Emit the load constant instruction */` |
|     235 |  1862 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1863 | `	}` |
|       - |  1864 | `	/* Node successfully compiled */` |
|     251 |  1865 | `	return SXRET_OK;` |
|     128 |  1866 |  |
|       - |  1867 | `/*` |
|       - |  1868 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1869 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1870 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1871 | ` */` |
|     150 |  1872 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  1873 | `	ph7_gen_state *pGen,` |
|       - |  1874 | `	ph7_vm_func *pFunc,` |
|       - |  1875 | `	const char *zName,` |
|       - |  1876 | `	sxu32 nByte,` |
|       - |  1877 | `	SyString *aShadow,` |
|       - |  1878 | `	sxu32 nShadow)` |
|       2 |  1879 |  |
|       - |  1880 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  1881 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  1882 | `	sxu32 n, nEnv;` |
|       - |  1883 | `	char *zDup;` |
|     152 |  1884 | `	if( nByte == 0 ){` |
|     ! 0 |  1885 | `		return SXRET_OK;` |
|       - |  1886 | `	}` |
|     150 |  1887 | `	if( nByte == sizeof("this")-1` |
|      81 |  1888 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  1889 | `		return SXRET_OK;` |
|       - |  1890 | `	}` |
|     182 |  1891 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     128 |  1892 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     125 |  1893 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      98 |  1894 | `			return SXRET_OK;` |
|       - |  1895 | `		}` |
|      17 |  1896 | `	}` |
|      53 |  1897 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  1898 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  1899 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  1900 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  1901 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  1902 | `			return SXRET_OK;` |
|       - |  1903 | `		}` |
|      15 |  1904 | `	}` |
|      53 |  1905 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  1906 | `	if( zDup == 0 ){` |
|     ! 0 |  1907 | `		return SXERR_ABORT;` |
|       - |  1908 | `	}` |
|      53 |  1909 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  1910 | `	sEnv.iFlags = 0;` |
|      53 |  1911 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  1912 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  1913 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  1914 | `	return SXRET_OK;` |
|      77 |  1915 |  |
|       - |  1916 | `/*` |
|       - |  1917 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  1918 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  1919 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  1920 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  1921 | ` */` |
|      14 |  1922 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  1923 | `	ph7_gen_state *pGen,` |
|       - |  1924 | `	ph7_vm_func *pFunc,` |
|       - |  1925 | `	const char *zIn,` |
|       - |  1926 | `	const char *zEnd,` |
|       - |  1927 | `	SyString *aShadow,` |
|       - |  1928 | `	sxu32 nShadow)` |
|       1 |  1929 |  |
|       - |  1930 | `	sxi32 rc;` |
|     159 |  1931 | `	while( zIn < zEnd ){` |
|     145 |  1932 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  1933 | `			zIn++;` |
|     ! 0 |  1934 | `			if( zIn < zEnd ){` |
|     ! 0 |  1935 | `				zIn++;` |
|     ! 0 |  1936 | `			}` |
|     ! 0 |  1937 | `			continue;` |
|       - |  1938 | `		}` |
|     144 |  1939 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  1940 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  1941 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  1942 | `			const char *zName;` |
|      13 |  1943 | `			zIn++; /* skip '$' */` |
|      13 |  1944 | `			zName = zIn;` |
|      39 |  1945 | `			while( zIn < zEnd ){` |
|      35 |  1946 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  1947 | `				if( c >= 0xc0 ){` |
|     ! 0 |  1948 | `					zIn++;` |
|     ! 0 |  1949 | `					while( zIn < zEnd` |
|     ! 0 |  1950 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1951 | `						zIn++;` |
|     ! 0 |  1952 | `					}` |
|     ! 0 |  1953 | `					continue;` |
|       - |  1954 | `				}` |
|      35 |  1955 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  1956 | `					break;` |
|       - |  1957 | `				}` |
|      27 |  1958 | `				zIn++;` |
|       1 |  1959 | `			}` |
|      13 |  1960 | `			if( zIn > zName ){` |
|      19 |  1961 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  1962 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  1963 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1964 | `					return SXERR_ABORT;` |
|       - |  1965 | `				}` |
|       6 |  1966 | `			}` |
|      13 |  1967 | `			continue;` |
|       - |  1968 | `		}` |
|     133 |  1969 | `		zIn++;` |
|       1 |  1970 | `	}` |
|      15 |  1971 | `	return SXRET_OK;` |
|       8 |  1972 |  |
|       - |  1973 | `/*` |
|       - |  1974 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  1975 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  1976 | ` *   - plain $<id> pairs` |
|       - |  1977 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  1978 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  1979 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  1980 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  1981 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  1982 | ` *     are never mistakenly captured.` |
|       - |  1983 | ` */` |
|     136 |  1984 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  1985 | `	ph7_gen_state *pGen,` |
|       - |  1986 | `	ph7_vm_func *pFunc,` |
|       - |  1987 | `	SyToken *pStart,` |
|       - |  1988 | `	SyToken *pEnd,` |
|       - |  1989 | `	SyString *aShadow,` |
|       - |  1990 | `	sxu32 nShadow)` |
|       2 |  1991 |  |
|     138 |  1992 | `	SyToken *pScan = pStart;` |
|       - |  1993 | `	sxi32 rc;` |
|     512 |  1994 | `	while( pScan < pEnd ){` |
|     376 |  1995 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 |  1996 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 |  1997 | `				pScan->sData.zString,` |
|      14 |  1998 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 |  1999 | `				aShadow,nShadow);` |
|      15 |  2000 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2001 | `				return SXERR_ABORT;` |
|       - |  2002 | `			}` |
|      15 |  2003 | `			pScan++;` |
|      15 |  2004 | `			continue;` |
|       - |  2005 | `		}` |
|     362 |  2006 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  2007 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  2008 | `			SyToken *pFnKw = pScan;` |
|      20 |  2009 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2010 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  2011 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2012 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2013 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2014 | `			}` |
|      21 |  2015 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2016 | `				SyToken *pInnerSigStart;` |
|       - |  2017 | `				SyToken *pInnerSigEnd;` |
|       - |  2018 | `				SyToken *pInnerBodyEnd;` |
|       - |  2019 | `				SyString *aInnerShadow;` |
|       - |  2020 | `				sxu32 nInnerShadow;` |
|       - |  2021 | `				sxu32 nInnerParamMax;` |
|       - |  2022 | `				SyToken *p;` |
|       - |  2023 | `				int iNestInner;` |
|      19 |  2024 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2025 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2026 | `					pScan++;` |
|     ! 0 |  2027 | `				}` |
|      19 |  2028 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2029 | `					pScan++;` |
|     ! 0 |  2030 | `					continue;` |
|       - |  2031 | `				}` |
|      19 |  2032 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2033 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2034 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2035 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2036 | `					pScan = pEnd;` |
|     ! 0 |  2037 | `					continue;` |
|       - |  2038 | `				}` |
|       - |  2039 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2040 | `				nInnerParamMax = 0;` |
|      57 |  2041 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2042 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2043 | `						nInnerParamMax++;` |
|       6 |  2044 | `					}` |
|      20 |  2045 | `				}` |
|      19 |  2046 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2047 | `					&pGen->pVm->sAllocator,` |
|      18 |  2048 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2049 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2050 | `					return SXERR_ABORT;` |
|       - |  2051 | `				}` |
|      19 |  2052 | `				nInnerShadow = 0;` |
|      25 |  2053 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2054 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2055 | `				}` |
|      57 |  2056 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2057 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2058 | `						continue;` |
|       - |  2059 | `					}` |
|      13 |  2060 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2061 | `						break;` |
|       - |  2062 | `					}` |
|      13 |  2063 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2064 | `						continue;` |
|       - |  2065 | `					}` |
|      13 |  2066 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2067 | `				}` |
|      19 |  2068 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2069 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2070 | `					pScan++;` |
|     ! 0 |  2071 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2072 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2073 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2074 | `						pScan++;` |
|     ! 0 |  2075 | `					}` |
|     ! 0 |  2076 | `					if( pScan < pEnd` |
|     ! 0 |  2077 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2078 | `						pScan++;` |
|     ! 0 |  2079 | `					}` |
|     ! 0 |  2080 | `				}` |
|      19 |  2081 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2082 | `					pScan++; /* past '=>' */` |
|       9 |  2083 | `				}` |
|      19 |  2084 | `				pInnerBodyEnd = pScan;` |
|      19 |  2085 | `				iNestInner = 0;` |
|     131 |  2086 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2087 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2088 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2089 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2090 | `						break;` |
|       - |  2091 | `					}` |
|     113 |  2092 | `					if( pInnerBodyEnd->nType &` |
|       - |  2093 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2094 | `						iNestInner++;` |
|     112 |  2095 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2096 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2097 | `						iNestInner--;` |
|       1 |  2098 | `					}` |
|     113 |  2099 | `					pInnerBodyEnd++;` |
|       1 |  2100 | `				}` |
|       - |  2101 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2102 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2103 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2104 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2105 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2106 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2107 | `				 *` |
|       - |  2108 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2109 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2110 | `				 * range after the '=' sign. */` |
|       - |  2111 | `				{` |
|      19 |  2112 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2113 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2114 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2115 | `						SyToken *pEq = 0;` |
|      13 |  2116 | `						int iNestArg = 0;` |
|      49 |  2117 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2118 | `							if( iNestArg == 0` |
|      39 |  2119 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2120 | `								break;` |
|       - |  2121 | `							}` |
|      37 |  2122 | `							if( pArgEnd->nType &` |
|       - |  2123 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2124 | `								iNestArg++;` |
|      37 |  2125 | `							}else if( pArgEnd->nType &` |
|       - |  2126 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2127 | `								iNestArg--;` |
|     ! 0 |  2128 | `							}` |
|      36 |  2129 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2130 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2131 | `								pEq = pArgEnd;` |
|       3 |  2132 | `							}` |
|      37 |  2133 | `							pArgEnd++;` |
|       1 |  2134 | `						}` |
|      13 |  2135 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2136 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2137 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2138 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2139 | `								return SXERR_ABORT;` |
|       - |  2140 | `							}` |
|       3 |  2141 | `						}` |
|      13 |  2142 | `						pArgStart = pArgEnd;` |
|      12 |  2143 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2144 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2145 | `							pArgStart++;` |
|       1 |  2146 | `						}` |
|       1 |  2147 | `					}` |
|       - |  2148 | `				}` |
|      28 |  2149 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2150 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2151 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2152 | `					return SXERR_ABORT;` |
|       - |  2153 | `				}` |
|      19 |  2154 | `				pScan = pInnerBodyEnd;` |
|      19 |  2155 | `				continue;` |
|       - |  2156 | `			}` |
|       1 |  2157 | `		}` |
|     344 |  2158 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     206 |  2159 | `			pScan++;` |
|     206 |  2160 | `			continue;` |
|       - |  2161 | `		}` |
|       - |  2162 | `		{` |
|       - |  2163 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     140 |  2164 | `			SyToken *pDollar = pScan;` |
|     207 |  2165 | `			while( &pDollar[1] < pEnd` |
|     140 |  2166 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2167 | `				pDollar++;` |
|     ! 0 |  2168 | `			}` |
|     140 |  2169 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2170 | `				break;` |
|       - |  2171 | `			}` |
|     140 |  2172 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2173 | `				pScan = pDollar + 1;` |
|     ! 0 |  2174 | `				continue;` |
|       - |  2175 | `			}` |
|     209 |  2176 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     138 |  2177 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      69 |  2178 | `				aShadow,nShadow);` |
|     140 |  2179 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2180 | `				return SXERR_ABORT;` |
|       - |  2181 | `			}` |
|     140 |  2182 | `			pScan = pDollar + 2;` |
|       - |  2183 | `		}` |
|       2 |  2184 | `	}` |
|     138 |  2185 | `	return SXRET_OK;` |
|      70 |  2186 |  |
|       - |  2187 | `/*` |
|       - |  2188 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2189 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2190 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2191 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2192 | ` * $this is also made available.` |
|       - |  2193 | ` */` |
|     118 |  2194 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2195 |  |
|       - |  2196 | `	ph7_vm_func *pFunc;` |
|       - |  2197 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2198 | `	GenBlock *pBlock;` |
|       - |  2199 | `	SySet *pInstrContainer;` |
|       - |  2200 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2201 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2202 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2203 | `	SyToken *pSavedEnd;` |
|       - |  2204 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2205 | `	char zName[512];` |
|       - |  2206 | `	static int iCnt = 1;` |
|       - |  2207 | `	char *zDup;` |
|       - |  2208 | `	sxu32 nLen;` |
|       - |  2209 | `	sxu32 nLine;` |
|     122 |  2210 | `	sxi32 iFlags = 0;` |
|     122 |  2211 | `	int bStatic = 0;` |
|       - |  2212 | `	sxi32 rc;` |
|       - |  2213 | `	sxu32 n;` |
|      59 |  2214 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2215 |  |
|     122 |  2216 | `	nLine = pGen->pIn->nLine;` |
|       - |  2217 | `	/* Optional 'static' prefix */` |
|     118 |  2218 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     122 |  2219 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2220 | `		bStatic = 1;` |
|       3 |  2221 | `		pGen->pIn++;` |
|       1 |  2222 | `	}` |
|       - |  2223 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     118 |  2224 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     122 |  2225 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2226 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2227 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2228 | `		return SXERR_SYNTAX;` |
|       - |  2229 | `	}` |
|     122 |  2230 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2231 | `	/* Optional '&' — return by reference */` |
|     122 |  2232 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2233 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2234 | `		pGen->pIn++;` |
|     ! 0 |  2235 | `	}` |
|       - |  2236 | `	/* Expect '(' */` |
|     122 |  2237 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2238 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2239 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2240 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2241 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2242 | `		}else{` |
|     ! 0 |  2243 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2244 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2245 | `		}` |
|       3 |  2246 | `		return SXERR_SYNTAX;` |
|       - |  2247 | `	}` |
|     119 |  2248 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2249 | `	/* Delimit the parameter list */` |
|     119 |  2250 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     119 |  2251 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2252 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2253 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2254 | `		return SXERR_SYNTAX;` |
|       - |  2255 | `	}` |
|       - |  2256 | `	/* Allocate the function state */` |
|     117 |  2257 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     117 |  2258 | `	if( pFunc == 0 ){` |
|     ! 0 |  2259 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2260 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2261 | `		return SXERR_ABORT;` |
|       - |  2262 | `	}` |
|       - |  2263 | `	/* Generate a unique lambda name */` |
|     117 |  2264 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     215 |  2265 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     100 |  2266 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2267 | `	}` |
|     117 |  2268 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     117 |  2269 | `	if( zDup == 0 ){` |
|     ! 0 |  2270 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2271 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2272 | `		return SXERR_ABORT;` |
|       - |  2273 | `	}` |
|     117 |  2274 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2275 | `	/* Collect function arguments */` |
|     117 |  2276 | `	if( pGen->pIn < pSigEnd ){` |
|      87 |  2277 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      87 |  2278 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2279 | `			return SXERR_ABORT;` |
|       - |  2280 | `		}` |
|      42 |  2281 | `	}` |
|       - |  2282 | `	/* Point past ')' and parse optional return type */` |
|     117 |  2283 | `	pGen->pIn = &pSigEnd[1];` |
|     117 |  2284 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     117 |  2285 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2286 | `		return SXERR_ABORT;` |
|     117 |  2287 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2288 | `		return SXERR_SYNTAX;` |
|       - |  2289 | `	}` |
|       - |  2290 | `	/* Expect '=>' */` |
|     117 |  2291 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2292 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2293 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2294 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2295 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2296 | `		}else{` |
|     ! 0 |  2297 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2298 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2299 | `		}` |
|       3 |  2300 | `		return SXERR_SYNTAX;` |
|       - |  2301 | `	}` |
|     114 |  2302 | `	pGen->pIn++; /* Jump '=>' */` |
|     114 |  2303 | `	pBodyStart = pGen->pIn;` |
|     114 |  2304 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2305 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2306 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2307 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2308 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     114 |  2309 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2310 | `	{` |
|     114 |  2311 | `		SyString *aShadow = 0;` |
|     114 |  2312 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     114 |  2313 | `		if( nShadow > 0 ){` |
|      84 |  2314 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      82 |  2315 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      84 |  2316 | `			if( aShadow == 0 ){` |
|     ! 0 |  2317 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2318 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2319 | `				return SXERR_ABORT;` |
|       - |  2320 | `			}` |
|     184 |  2321 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     102 |  2322 | `				aShadow[n] = aArgs[n].sName;` |
|      52 |  2323 | `			}` |
|      41 |  2324 | `		}` |
|     170 |  2325 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      56 |  2326 | `			aShadow,nShadow);` |
|     114 |  2327 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2328 | `			return SXERR_ABORT;` |
|       - |  2329 | `		}` |
|       - |  2330 | `	}` |
|       - |  2331 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2332 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2333 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2334 | `	 * $this. */` |
|     114 |  2335 | `	if( !bStatic ){` |
|       - |  2336 | `		char *zThisDup;` |
|     112 |  2337 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     112 |  2338 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2339 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2340 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2341 | `			return SXERR_ABORT;` |
|       - |  2342 | `		}` |
|     112 |  2343 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     112 |  2344 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     112 |  2345 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     112 |  2346 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     112 |  2347 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      55 |  2348 | `	}` |
|       - |  2349 | `	/* Arrow functions are always closures */` |
|     114 |  2350 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2351 | `	/* Compile the body expression as an implicit return */` |
|     170 |  2352 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      56 |  2353 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     114 |  2354 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2355 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2356 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2357 | `		return SXERR_ABORT;` |
|       - |  2358 | `	}` |
|     114 |  2359 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     114 |  2360 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     114 |  2361 | `	pSavedEnd = pGen->pEnd;` |
|     114 |  2362 | `	pGen->pIn = pBodyStart;` |
|     114 |  2363 | `	pGen->pEnd = pBodyEnd;` |
|     114 |  2364 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     114 |  2365 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2366 | `		return SXERR_ABORT;` |
|       - |  2367 | `	}` |
|       - |  2368 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2369 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2370 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2371 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     114 |  2372 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     114 |  2373 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     114 |  2374 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     114 |  2375 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     114 |  2376 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2377 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     114 |  2378 | `	pGen->pIn = pBodyEnd;` |
|     114 |  2379 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2380 | `	/* Emit the load-closure instruction */` |
|     114 |  2381 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     114 |  2382 | `	return SXRET_OK;` |
|      63 |  2383 |  |
|       - |  2384 | `/*` |
|       - |  2385 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2386 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2387 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2388 | ` * expression's value.` |
|       - |  2389 | ` */` |
|     346 |  2390 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2391 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2392 |  |
|       - |  2393 | `	SySet *pInstrContainer;` |
|       - |  2394 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2395 | `	GenBlock *pArmBlock;` |
|       - |  2396 | `	sxi32 rc;` |
|     349 |  2397 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2398 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2399 | `	pGen->pIn  = pStart;` |
|     349 |  2400 | `	pGen->pEnd = pStop;` |
|     349 |  2401 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2402 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2403 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2404 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2405 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2406 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2407 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2408 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2409 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2410 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2411 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2412 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2413 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2414 | `		return SXERR_ABORT;` |
|       - |  2415 | `	}` |
|     349 |  2416 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2417 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2418 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2419 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2420 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2421 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2422 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2423 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2424 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2425 | `		return SXERR_ABORT;` |
|       - |  2426 | `	}` |
|     349 |  2427 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2428 | `		return SXERR_EMPTY;` |
|       - |  2429 | `	}` |
|     349 |  2430 | `	return SXRET_OK;` |
|     176 |  2431 |  |
|       - |  2432 | `/*` |
|       - |  2433 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2434 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2435 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2436 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2437 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2438 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2439 | ` */` |
|       - |  2440 | `/*` |
|       - |  2441 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2442 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2443 | ` * caller can bail out of the current expression.` |
|       - |  2444 | ` */` |
|       2 |  2445 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2446 |  |
|       - |  2447 | `	va_list ap;` |
|       - |  2448 | `	sxi32 rc;` |
|       - |  2449 | `	SyBlob sMsg;` |
|       3 |  2450 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2451 | `	va_start(ap,zFmt);` |
|       3 |  2452 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2453 | `	va_end(ap);` |
|       3 |  2454 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2455 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2456 | `	SyBlobRelease(&sMsg);` |
|       3 |  2457 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2458 | `		return SXERR_ABORT;` |
|       - |  2459 | `	}` |
|       3 |  2460 | `	return SXERR_SYNTAX;` |
|       2 |  2461 |  |
|       - |  2462 | `/*` |
|       - |  2463 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2464 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2465 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2466 | ` */` |
|     348 |  2467 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2468 |  |
|     352 |  2469 | `	SyToken *pCur = pStart;` |
|     352 |  2470 | `	int iNest = 0;` |
|     814 |  2471 | `	while( pCur < pEnd ){` |
|     780 |  2472 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2473 | `			iNest++;` |
|     774 |  2474 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2475 | `			iNest--;` |
|     762 |  2476 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2477 | `			return pCur;` |
|       - |  2478 | `		}` |
|     466 |  2479 | `		pCur++;` |
|       4 |  2480 | `	}` |
|      37 |  2481 | `	return pEnd;` |
|     178 |  2482 |  |
|      70 |  2483 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2484 |  |
|       - |  2485 | `	ph7_match *pMatch;` |
|       - |  2486 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2487 | `	int bHasDefault = 0;` |
|       - |  2488 | `	sxu32 nLine;` |
|       - |  2489 | `	sxi32 rc;` |
|      35 |  2490 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2491 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2492 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2493 | `	/* Expect '(' */` |
|      75 |  2494 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2495 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2496 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2497 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2498 | `	}` |
|      75 |  2499 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2500 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2501 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2502 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2503 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2504 | `	}` |
|      75 |  2505 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2506 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2507 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2508 | `	}` |
|       - |  2509 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2510 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2511 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2512 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2513 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2514 | `		return SXERR_ABORT;` |
|       - |  2515 | `	}` |
|      75 |  2516 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2517 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2518 | `	/* Expect '{' */` |
|      75 |  2519 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2520 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2521 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2522 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2523 | `	}` |
|      75 |  2524 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2525 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2526 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2527 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2528 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2529 | `	}` |
|       - |  2530 | `	/* Allocate ph7_match container */` |
|      75 |  2531 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2532 | `	if( pMatch == 0 ){` |
|     ! 0 |  2533 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2534 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2535 | `		return SXERR_ABORT;` |
|       - |  2536 | `	}` |
|      75 |  2537 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2538 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2539 | `	/* Iterate arms */` |
|     253 |  2540 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2541 | `		ph7_match_arm sArm;` |
|       - |  2542 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2543 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2544 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2545 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2546 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2547 | `		/* 'default' arm? */` |
|     182 |  2548 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2549 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2550 | `			if( bHasDefault ){` |
|       3 |  2551 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2552 | `					"Match expressions may only contain one default arm");` |
|       4 |  2553 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2554 | `			}` |
|      20 |  2555 | `			sArm.bDefault = 1;` |
|      20 |  2556 | `			bHasDefault = 1;` |
|      20 |  2557 | `			pGen->pIn++;` |
|      20 |  2558 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2559 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2560 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2561 | `			}` |
|      20 |  2562 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2563 | `		}else{` |
|       - |  2564 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2565 | `			pCondStart = pGen->pIn;` |
|     166 |  2566 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2567 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2568 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2569 | `				SySet sCondBc;` |
|       9 |  2570 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2571 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2572 | `						"syntax error, empty match condition expression");` |
|       - |  2573 | `				}` |
|       9 |  2574 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2575 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2576 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2577 | `					return SXERR_ABORT;` |
|       - |  2578 | `				}` |
|       9 |  2579 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2580 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2581 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2582 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2583 | `			}` |
|     166 |  2584 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2585 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2586 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2587 | `			}` |
|     163 |  2588 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2589 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2590 | `					"syntax error, empty match condition expression");` |
|       - |  2591 | `			}` |
|       - |  2592 | `			{` |
|       - |  2593 | `				SySet sCondBc;` |
|     163 |  2594 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2595 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2596 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2597 | `					return SXERR_ABORT;` |
|       - |  2598 | `				}` |
|     163 |  2599 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2600 | `			}` |
|     163 |  2601 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2602 | `		}` |
|       - |  2603 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2604 | `		pResStart = pGen->pIn;` |
|     181 |  2605 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2606 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2607 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2608 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2609 | `		}` |
|     181 |  2610 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2611 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2612 | `			return SXERR_ABORT;` |
|       - |  2613 | `		}` |
|     181 |  2614 | `		pGen->pIn = pResEnd;` |
|     181 |  2615 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2616 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2617 | `		}` |
|     181 |  2618 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2619 | `	}` |
|      69 |  2620 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2621 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2622 | `	return SXRET_OK;` |
|      40 |  2623 |  |
|       - |  2624 | `/*` |
|       - |  2625 | ` * Compile a backtick quoted string.` |
|       - |  2626 | ` */` |
|       4 |  2627 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2628 |  |
|       - |  2629 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2630 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2631 | `	 */` |
|       8 |  2632 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2633 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2634 | `		ph7_lib_version()` |
|       - |  2635 | `		);` |
|       - |  2636 | `	/* Load NULL */` |
|       6 |  2637 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2638 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2639 | `	/* Node successfully compiled */` |
|       6 |  2640 | `	return SXRET_OK;` |
|       2 |  2641 |  |
|       - |  2642 | `/*` |
|       - |  2643 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2644 | ` * construct.` |
|       - |  2645 | ` */` |
|      80 |  2646 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2647 |  |
|       - |  2648 | `	SyString *pName;` |
|       - |  2649 | `	sxu32 nKeyID;` |
|       - |  2650 | `	sxi32 rc;` |
|       - |  2651 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      85 |  2652 | `	pName = &pGen->pIn->sData;` |
|      85 |  2653 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  2654 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      85 |  2655 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2656 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2657 | `		/* Compile arguments one after one */` |
|       9 |  2658 | `		pTmp = pGen->pEnd;` |
|       - |  2659 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2660 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2661 | `		 *  mean that the following expression is valid:` |
|       - |  2662 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2663 | `		 */` |
|       9 |  2664 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2665 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2666 | `			if( pGen->pIn < pNext ){` |
|       9 |  2667 | `				pGen->pEnd = pNext;` |
|       9 |  2668 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2669 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2670 | `					return SXERR_ABORT;` |
|       - |  2671 | `				}` |
|       9 |  2672 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2673 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2674 | `					 * without the overhead of a function call.` |
|       - |  2675 | `					 * This is a very powerful optimization that improve` |
|       - |  2676 | `					 * performance greatly.` |
|       - |  2677 | `					 */` |
|       9 |  2678 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2679 | `				}` |
|       4 |  2680 | `			}` |
|       - |  2681 | `			/* Jump trailing commas */` |
|       9 |  2682 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2683 | `				pNext++;` |
|     ! 0 |  2684 | `			}` |
|       9 |  2685 | `			pGen->pIn = pNext;` |
|       1 |  2686 | `		}` |
|       - |  2687 | `		/* Restore token stream */` |
|       9 |  2688 | `		pGen->pEnd = pTmp;` |
|       5 |  2689 | `	}else{` |
|      77 |  2690 | `		sxi32 nArg = 0;` |
|      77 |  2691 | `		sxu32 nIdx = 0;` |
|      77 |  2692 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      77 |  2693 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2694 | `			return SXERR_ABORT;` |
|      77 |  2695 | `		}else if(rc != SXERR_EMPTY ){` |
|      77 |  2696 | `			nArg = 1;` |
|      36 |  2697 | `		}` |
|      77 |  2698 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2699 | `			ph7_value *pObj;` |
|       - |  2700 | `			/* Emit the call instruction */` |
|      29 |  2701 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  2702 | `			if( pObj == 0 ){` |
|     ! 0 |  2703 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2704 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2705 | `				return SXERR_ABORT;` |
|       - |  2706 | `			}` |
|      29 |  2707 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2708 | `			/* Install in the literal table */` |
|      29 |  2709 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      12 |  2710 | `		}` |
|       - |  2711 | `		/* Emit the call instruction */` |
|      77 |  2712 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      77 |  2713 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2714 | `	}` |
|       - |  2715 | `	/* Node successfully compiled */` |
|      85 |  2716 | `	return SXRET_OK;` |
|      45 |  2717 |  |
|       - |  2718 | `/*` |
|       - |  2719 | ` * Compile a node holding a variable declaration.` |
|       - |  2720 | ` * According to the PHP language reference` |
|       - |  2721 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2722 | ` *  The variable name is case-sensitive.` |
|       - |  2723 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2724 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2725 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2726 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2727 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2728 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2729 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2730 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2731 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2732 | ` *  the chapter on Expressions.` |
|       - |  2733 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2734 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2735 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2736 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2737 | ` *  is being assigned (the source variable).` |
|       - |  2738 | ` */` |
|  991974 |  2739 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2740 |  |
|  991979 |  2741 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2742 | `	sxi32 iVv;` |
|       - |  2743 | `	sxi32 iP1;` |
|       - |  2744 | `	void *p3;` |
|       - |  2745 | `	sxi32 rc;` |
|  991979 |  2746 | `	iVv = -1; /* Variable variable counter */` |
| 1983965 |  2747 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  991991 |  2748 | `		pGen->pIn++;` |
|  991991 |  2749 | `		iVv++;` |
|       5 |  2750 | `	}` |
|  991979 |  2751 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2752 | `		/* Invalid variable name */` |
|     ! 0 |  2753 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2754 | `		if( rc == SXERR_ABORT ){` |
|       - |  2755 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2756 | `			return SXERR_ABORT;` |
|       - |  2757 | `		}` |
|     ! 0 |  2758 | `		return SXRET_OK;` |
|       - |  2759 | `	}` |
|  991979 |  2760 | `	p3  = 0;` |
|  991979 |  2761 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2762 | `		/* Dynamic variable creation */` |
|      19 |  2763 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  2764 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  2765 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2766 | `			/* Empty expression */` |
|       3 |  2767 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2768 | `			return SXRET_OK;` |
|       - |  2769 | `		}` |
|       - |  2770 | `		/* Compile the expression holding the variable name */` |
|      16 |  2771 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2772 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2773 | `			return SXERR_ABORT;` |
|      16 |  2774 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2775 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2776 | `			return SXRET_OK;` |
|       - |  2777 | `		}` |
|       7 |  2778 | `	}else{` |
|       - |  2779 | `		SyHashEntry *pEntry;` |
|       - |  2780 | `		SyString *pName;` |
|  991963 |  2781 | `		char *zName = 0;` |
|       - |  2782 | `		/* Extract variable name */` |
|  991963 |  2783 | `		pName = &pGen->pIn->sData;` |
|       - |  2784 | `		/* Advance the stream cursor */` |
|  991963 |  2785 | `		pGen->pIn++;` |
|  991963 |  2786 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  991963 |  2787 | `		if( pEntry == 0 ){` |
|       - |  2788 | `			/* Duplicate name */` |
|  133073 |  2789 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  133073 |  2790 | `			if( zName == 0 ){` |
|     ! 0 |  2791 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2792 | `				return SXERR_ABORT;` |
|       - |  2793 | `			}` |
|       - |  2794 | `			/* Install in the hashtable */` |
|  133073 |  2795 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   66539 |  2796 | `		}else{` |
|       - |  2797 | `			/* Name already available */` |
|  858895 |  2798 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2799 | `		}` |
|  991963 |  2800 | `		p3 = (void *)zName;` |
|       - |  2801 | `	}` |
|  991975 |  2802 | `	iP1 = 0;` |
|  991975 |  2803 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  361489 |  2804 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2805 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  361471 |  2806 | `			iP1 = 1;` |
|  180733 |  2807 | `		}` |
|  180742 |  2808 | `	}` |
|       - |  2809 | `	/* Emit the load instruction */` |
|  991975 |  2810 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  991987 |  2811 | `	while( iVv > 0 ){` |
|      13 |  2812 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2813 | `		iVv--;` |
|       1 |  2814 | `	}` |
|       - |  2815 | `	/* Node successfully compiled */` |
|  991975 |  2816 | `	return SXRET_OK;` |
|  495992 |  2817 |  |
|       - |  2818 | `/*` |
|       - |  2819 | ` * Load a literal.` |
|       - |  2820 | ` */` |
|  697508 |  2821 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2822 |  |
|  697513 |  2823 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2824 | `	ph7_value *pObj;` |
|       - |  2825 | `	SyString *pStr;` |
|       - |  2826 | `	sxu32 nIdx;` |
|       - |  2827 | `	/* Extract token value */` |
|  697513 |  2828 | `	pStr = &pToken->sData;` |
|       - |  2829 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  697513 |  2830 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  147835 |  2831 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2832 | `			/* NULL constant are always indexed at 0 */` |
|   54475 |  2833 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   54475 |  2834 | `			return SXRET_OK;` |
|   93365 |  2835 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2836 | `			/* TRUE constant are always indexed at 1 */` |
|     597 |  2837 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     597 |  2838 | `			return SXRET_OK;` |
|       5 |  2839 | `		}` |
|  651329 |  2840 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  110524 |  2841 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2842 | `			/* FALSE constant are always indexed at 2 */` |
|   41769 |  2843 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   41769 |  2844 | `			return SXRET_OK;` |
|  557478 |  2845 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   99118 |  2846 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2847 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    9503 |  2848 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    9503 |  2849 | `			if( pObj == 0 ){` |
|     ! 0 |  2850 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2851 | `				return SXERR_ABORT;` |
|       - |  2852 | `			}` |
|    9503 |  2853 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2854 | `			/* Emit the load constant instruction */` |
|    9503 |  2855 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    9503 |  2856 | `			return SXRET_OK;` |
|  514433 |  2857 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   32024 |  2858 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2859 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       8 |  2860 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       8 |  2861 | `			if( pObj == 0 ){` |
|     ! 0 |  2862 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2863 | `				return SXERR_ABORT;` |
|       - |  2864 | `			}` |
|       8 |  2865 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2866 | `				SyString sNs;` |
|       8 |  2867 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  2868 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       5 |  2869 | `			}else{` |
|     ! 0 |  2870 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2871 | `			}` |
|       8 |  2872 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       8 |  2873 | `			return SXRET_OK;` |
|  513559 |  2874 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   13411 |  2875 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  506849 |  2876 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   16892 |  2877 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  2878 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  2879 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  2880 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  2881 | `				/* Point to the upper block */` |
|      11 |  2882 | `				pBlock = pBlock->pParent;` |
|       1 |  2883 | `			}` |
|      11 |  2884 | `			if( pBlock == 0 ){` |
|       - |  2885 | `				/* Called in the global scope,load NULL */` |
|       5 |  2886 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  2887 | `			}else{` |
|       - |  2888 | `				/* Extract the target function/method */` |
|       7 |  2889 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  2890 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  2891 | `					/* Not a class method,Load null */` |
|       3 |  2892 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2893 | `				}else{` |
|       5 |  2894 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  2895 | `					if( pObj == 0 ){` |
|     ! 0 |  2896 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2897 | `						return SXERR_ABORT;` |
|       - |  2898 | `					}` |
|       5 |  2899 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  2900 | `					/* Emit the load constant instruction */` |
|       5 |  2901 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  2902 | `				}` |
|       - |  2903 | `			}` |
|      11 |  2904 | `			return SXRET_OK;` |
|       - |  2905 | `	}` |
|       - |  2906 | `	/* Query literal table */` |
|  591173 |  2907 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2908 | `		ph7_value *pLitObj;` |
|       - |  2909 | `		/* Unknown literal,install it in the literal table */` |
|  245425 |  2910 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  245425 |  2911 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2912 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2913 | `			return SXERR_ABORT;` |
|       - |  2914 | `		}` |
|  245425 |  2915 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  245425 |  2916 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  122710 |  2917 | `	}` |
|       - |  2918 | `	/* Emit the load constant instruction */` |
|  591173 |  2919 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  591173 |  2920 | `	return SXRET_OK;` |
|  348759 |  2921 |  |
|       - |  2922 | `/*` |
|       - |  2923 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2924 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2925 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2926 | ` * Otherwise, load the simple literal directly.` |
|       - |  2927 | ` */` |
|  697544 |  2928 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  2929 |  |
|       - |  2930 | `	sxi32 rc;` |
|  697549 |  2931 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2932 | `		return SXRET_OK;` |
|       - |  2933 | `	}` |
|       - |  2934 | `	/* Check if this is a multi-token namespace path */` |
|  697549 |  2935 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  2936 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      40 |  2937 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      40 |  2938 | `		int isAbsolute = 0;` |
|      40 |  2939 | `		SyBlobReset(pWorker);` |
|       - |  2940 | `		/* Check for leading backslash (absolute path) */` |
|      40 |  2941 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      38 |  2942 | `			isAbsolute = 1;` |
|      38 |  2943 | `			pGen->pIn++; /* Skip leading backslash */` |
|      17 |  2944 | `		}` |
|       - |  2945 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      40 |  2946 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  2947 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  2948 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  2949 | `		}` |
|       - |  2950 | `		/* Collect all path components */` |
|     136 |  2951 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     136 |  2952 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      52 |  2953 | `				SyBlobAppend(pWorker,"\\",1);` |
|      28 |  2954 | `			}else{` |
|      88 |  2955 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  2956 | `			}` |
|     136 |  2957 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      40 |  2958 | `				pGen->pIn++;` |
|      40 |  2959 | `				break;` |
|       - |  2960 | `			}` |
|     100 |  2961 | `			pGen->pIn++;` |
|       4 |  2962 | `		}` |
|      40 |  2963 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  2964 | `			ph7_value *pObj;` |
|       - |  2965 | `			SyString sPath;` |
|       - |  2966 | `			sxu32 nIdx;` |
|      40 |  2967 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  2968 | `			/* Install in the literal table */` |
|      40 |  2969 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      20 |  2970 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 |  2971 | `				if( pObj == 0 ){` |
|     ! 0 |  2972 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2973 | `					return SXERR_ABORT;` |
|       - |  2974 | `				}` |
|      20 |  2975 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      20 |  2976 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 |  2977 | `			}` |
|       - |  2978 | `			/* Emit the load constant instruction.` |
|       - |  2979 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  2980 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      58 |  2981 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      18 |  2982 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      18 |  2983 | `				nIdx,0,0);` |
|      40 |  2984 | `			return SXRET_OK;` |
|       - |  2985 | `		}` |
|     ! 0 |  2986 | `	}` |
|       - |  2987 | `	/* Single-token literal: load directly */` |
|  697513 |  2988 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  697513 |  2989 | `	return rc;` |
|  348777 |  2990 |  |
|       - |  2991 | `/*` |
|       - |  2992 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2993 | ` */` |
|  697544 |  2994 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2995 |  |
|       - |  2996 | `	sxi32 rc;` |
|  697549 |  2997 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  697549 |  2998 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2999 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3000 | `		return rc;` |
|       - |  3001 | `	}` |
|       - |  3002 | `	/* Node successfully compiled */` |
|  697549 |  3003 | `	return SXRET_OK;` |
|  348777 |  3004 |  |
|       - |  3005 | `/*` |
|       - |  3006 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3007 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3008 | ` */` |
|       8 |  3009 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3010 |  |
|       - |  3011 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3012 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3013 | `		pGen->pIn++;` |
|       1 |  3014 | `	}` |
|       9 |  3015 | `	return SXRET_OK;` |
|       1 |  3016 |  |
|       - |  3017 | `/*` |
|       - |  3018 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3019 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3020 | ` */` |
|     104 |  3021 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3022 |  |
|     109 |  3023 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      29 |  3024 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3025 | `			return TRUE;` |
|      27 |  3026 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 |  3027 | `			return TRUE;` |
|       2 |  3028 | `		}` |
|      93 |  3029 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3030 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3031 | `			return TRUE;` |
|       - |  3032 | `		}` |
|     ! 0 |  3033 | `	}` |
|       - |  3034 | `	/* Not a reserved constant */` |
|     101 |  3035 | `	return FALSE;` |
|      57 |  3036 |  |
|       - |  3037 | `/*` |
|       - |  3038 | ` * Compile the 'const' statement.` |
|       - |  3039 | ` * According to the PHP language reference` |
|       - |  3040 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3041 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3042 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3043 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3044 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3045 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3046 | ` *  Syntax` |
|       - |  3047 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3048 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3049 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3050 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3051 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3052 | ` *  to get a list of all defined constants.` |
|       - |  3053 | ` *` |
|       - |  3054 | ` * Symisc eXtension.` |
|       - |  3055 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3056 | ` *  would allow only simple scalar value.` |
|       - |  3057 | ` *  Example` |
|       - |  3058 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3059 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3060 | ` */` |
|      32 |  3061 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3062 |  |
|       - |  3063 | `	SySet *pConsCode,*pInstrContainer;` |
|      37 |  3064 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3065 | `	SyString *pName;` |
|       - |  3066 | `	sxi32 rc;` |
|      37 |  3067 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      37 |  3068 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3069 | `		/* Invalid constant name */` |
|       7 |  3070 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 |  3071 | `		if( rc == SXERR_ABORT ){` |
|       - |  3072 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3073 | `			return SXERR_ABORT;` |
|       - |  3074 | `		}` |
|       7 |  3075 | `		goto Synchronize;` |
|       - |  3076 | `	}` |
|       - |  3077 | `	/* Peek constant name */` |
|      30 |  3078 | `	pName = &pGen->pIn->sData;` |
|       - |  3079 | `	/* Make sure the constant name isn't reserved */` |
|      30 |  3080 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3081 | `		/* Reserved constant */` |
|       9 |  3082 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 |  3083 | `		if( rc == SXERR_ABORT ){` |
|       - |  3084 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3085 | `			return SXERR_ABORT;` |
|       - |  3086 | `		}` |
|       9 |  3087 | `		goto Synchronize;` |
|       - |  3088 | `	}` |
|      21 |  3089 | `	pGen->pIn++;` |
|      21 |  3090 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3091 | `		/* Invalid statement*/` |
|       6 |  3092 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3093 | `		if( rc == SXERR_ABORT ){` |
|       - |  3094 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3095 | `			return SXERR_ABORT;` |
|       - |  3096 | `		}` |
|       6 |  3097 | `		goto Synchronize;` |
|       - |  3098 | `	}` |
|      15 |  3099 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3100 | `	/* Allocate a new constant value container */` |
|      15 |  3101 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3102 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3103 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3104 | `		return SXERR_ABORT;` |
|       - |  3105 | `	}` |
|      15 |  3106 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3107 | `	/* Swap bytecode container */` |
|      15 |  3108 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3109 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3110 | `	/* Compile constant value */` |
|      15 |  3111 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3112 | `	/* Emit the done instruction */` |
|      15 |  3113 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3114 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3115 | `	if( rc == SXERR_ABORT ){` |
|       - |  3116 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3117 | `		return SXERR_ABORT;` |
|       - |  3118 | `	}` |
|      15 |  3119 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3120 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3121 | `	{` |
|       - |  3122 | `		SyBlob sFQN;` |
|       - |  3123 | `		SyString sFQNStr;` |
|      15 |  3124 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3125 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3126 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3127 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3128 | `		SyBlobRelease(&sFQN);` |
|       - |  3129 | `	}` |
|      15 |  3130 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3131 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3132 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3133 | `	}` |
|      15 |  3134 | `	return SXRET_OK;` |
|       9 |  3135 | `Synchronize:` |
|       - |  3136 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3137 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      40 |  3138 | `		pGen->pIn++;` |
|       2 |  3139 | `	}` |
|      22 |  3140 | `	return SXRET_OK;` |
|      21 |  3141 |  |
|       - |  3142 | `/*` |
|       - |  3143 | ` * Compile the 'continue' statement.` |
|       - |  3144 | ` * According to the PHP language reference` |
|       - |  3145 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3146 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3147 | ` *  iteration.` |
|       - |  3148 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3149 | ` *  the purposes of continue.` |
|       - |  3150 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3151 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3152 | ` *  Note:` |
|       - |  3153 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3154 | ` */` |
|       - |  3155 | `/*` |
|       - |  3156 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3157 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3158 | ` * break/continue crosses a try boundary.` |
|       - |  3159 | ` *` |
|       - |  3160 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3161 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3162 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3163 | ` */` |
|    3304 |  3164 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3165 |  |
|    3309 |  3166 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   19337 |  3167 | `	while( pBlock && pBlock != pTarget ){` |
|   16033 |  3168 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3169 | `			if( pBlock->pUserData ){` |
|       - |  3170 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3171 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3172 | `			}else{` |
|       - |  3173 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3174 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3175 | `				 * exception context from a sub-execution.` |
|       - |  3176 | `				 */` |
|     ! 0 |  3177 | `				break;` |
|       - |  3178 | `			}` |
|       1 |  3179 | `		}` |
|   16033 |  3180 | `		pBlock = pBlock->pParent;` |
|       5 |  3181 | `	}` |
|    3309 |  3182 |  |
|    3208 |  3183 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3184 |  |
|       - |  3185 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3186 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3187 | `	sxu32 nLineLocal;` |
|       - |  3188 | `	sxi32 rc;` |
|    3213 |  3189 | `	nLineLocal = pGen->pIn->nLine;` |
|    3213 |  3190 | `	iLevel = 0;` |
|       - |  3191 | `	/* Jump the 'continue' keyword */` |
|    3213 |  3192 | `	pGen->pIn++;` |
|    3213 |  3193 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3194 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3195 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3196 | `		 */` |
|       - |  3197 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3198 | `		char *zAlloc = 0;` |
|       - |  3199 | `		SyString sNum;` |
|      17 |  3200 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3201 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3202 | `			return SXERR_ABORT;` |
|       - |  3203 | `		}` |
|      17 |  3204 | `		if( rc == SXRET_OK ){` |
|      20 |  3205 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3206 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3207 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3208 | `				return SXERR_ABORT;` |
|       - |  3209 | `			}` |
|      14 |  3210 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3211 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3212 | `		}` |
|      17 |  3213 | `		if( iLevel < 2 ){` |
|       3 |  3214 | `			iLevel = 0;` |
|       1 |  3215 | `		}` |
|      17 |  3216 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3217 | `	}` |
|       - |  3218 | `	/* Point to the target loop */` |
|    3213 |  3219 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3213 |  3220 | `	if( pLoop == 0 ){` |
|       - |  3221 | `		/* Illegal continue */` |
|      13 |  3222 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      13 |  3223 | `		if( rc == SXERR_ABORT ){` |
|       - |  3224 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3225 | `			return SXERR_ABORT;` |
|       - |  3226 | `		}` |
|       8 |  3227 | `	}else{` |
|    3203 |  3228 | `		sxu32 nInstrIdx = 0;` |
|       - |  3229 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3203 |  3230 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3203 |  3231 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3232 | `			/* According to the PHP language reference manual` |
|       - |  3233 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3234 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3235 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3236 | `			 */` |
|       5 |  3237 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3238 | `			if( rc == SXRET_OK ){` |
|       5 |  3239 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3240 | `			}` |
|       3 |  3241 | `		}else{` |
|       - |  3242 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3199 |  3243 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3199 |  3244 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3245 | `				JumpFixup sJumpFix;` |
|       - |  3246 | `				/* Post-continue */` |
|      14 |  3247 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3248 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3249 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3250 | `			}` |
|       - |  3251 | `		}` |
|       - |  3252 | `	}` |
|    3213 |  3253 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3254 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3255 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3256 | `	}` |
|       - |  3257 | `	/* Statement successfully compiled */` |
|    3213 |  3258 | `	return SXRET_OK;` |
|    1609 |  3259 |  |
|       - |  3260 | `/*` |
|       - |  3261 | ` * Compile the 'break' statement.` |
|       - |  3262 | ` * According to the PHP language reference` |
|       - |  3263 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3264 | ` *  structure.` |
|       - |  3265 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3266 | ` *  enclosing structures are to be broken out of.` |
|       - |  3267 | ` */` |
|     122 |  3268 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3269 |  |
|       - |  3270 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3271 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3272 | `	sxi32 rc;` |
|     127 |  3273 | `	iLevel = 0;` |
|       - |  3274 | `	/* Jump the 'break' keyword */` |
|     127 |  3275 | `	pGen->pIn++;` |
|     127 |  3276 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3277 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3278 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3279 | `		 */` |
|       - |  3280 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      18 |  3281 | `		char *zAlloc = 0;` |
|       - |  3282 | `		SyString sNum;` |
|      18 |  3283 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      18 |  3284 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3285 | `			return SXERR_ABORT;` |
|       - |  3286 | `		}` |
|      18 |  3287 | `		if( rc == SXRET_OK ){` |
|      21 |  3288 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3289 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3290 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3291 | `				return SXERR_ABORT;` |
|       - |  3292 | `			}` |
|      15 |  3293 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3294 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3295 | `		}` |
|      18 |  3296 | `		if( iLevel < 2 ){` |
|       3 |  3297 | `			iLevel = 0;` |
|       1 |  3298 | `		}` |
|      18 |  3299 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3300 | `	}` |
|       - |  3301 | `	/* Extract the target loop */` |
|     127 |  3302 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     127 |  3303 | `	if( pLoop == 0 ){` |
|       - |  3304 | `		/* Illegal break */` |
|      18 |  3305 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      18 |  3306 | `		if( rc == SXERR_ABORT ){` |
|       - |  3307 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3308 | `			return SXERR_ABORT;` |
|       - |  3309 | `		}` |
|      10 |  3310 | `	}else{` |
|       - |  3311 | `		sxu32 nInstrIdx;` |
|       - |  3312 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|     111 |  3313 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|     111 |  3314 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|     111 |  3315 | `		if( rc == SXRET_OK ){` |
|       - |  3316 | `			/* Fix the jump later when the jump destination is resolved */` |
|     111 |  3317 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      53 |  3318 | `		}` |
|       - |  3319 | `	}` |
|     127 |  3320 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3321 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3322 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3323 | `	}` |
|       - |  3324 | `	/* Statement successfully compiled */` |
|     127 |  3325 | `	return SXRET_OK;` |
|      66 |  3326 |  |
|       - |  3327 | `/*` |
|       - |  3328 | ` * Compile or record a label.` |
|       - |  3329 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3330 | ` * Example` |
|       - |  3331 | ` *  goto LABEL;` |
|       - |  3332 | ` *   echo 'Foo';` |
|       - |  3333 | ` *  LABEL:` |
|       - |  3334 | ` *   echo 'Bar';` |
|       - |  3335 | ` */` |
|     112 |  3336 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3337 |  |
|       - |  3338 | `	GenBlock *pBlock;` |
|       - |  3339 | `	Label sLabel;` |
|       - |  3340 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3341 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3342 | `	if( pBlock ){` |
|       - |  3343 | `		sxi32 rc;` |
|       8 |  3344 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3345 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3346 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3347 | `			return SXERR_ABORT;` |
|       - |  3348 | `		}` |
|       4 |  3349 | `	}else{` |
|     113 |  3350 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3351 | `		char *zDup;` |
|       - |  3352 | `		/* Initialize label fields */` |
|     113 |  3353 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3354 | `		/* Duplicate label name */` |
|     113 |  3355 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3356 | `		if( zDup == 0 ){` |
|     ! 0 |  3357 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3358 | `			return SXERR_ABORT;` |
|       - |  3359 | `		}` |
|     113 |  3360 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3361 | `		sLabel.bRef  = FALSE;` |
|     113 |  3362 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3363 | `		pBlock = pGen->pCurrent;` |
|     221 |  3364 | `		while( pBlock ){` |
|     133 |  3365 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      24 |  3366 | `				break;` |
|       - |  3367 | `			}` |
|       - |  3368 | `			/* Point to the upper block */` |
|     113 |  3369 | `			pBlock = pBlock->pParent;` |
|       5 |  3370 | `		}` |
|     113 |  3371 | `		if( pBlock ){` |
|      24 |  3372 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      14 |  3373 | `		}else{` |
|      93 |  3374 | `			sLabel.pFunc = 0;` |
|       - |  3375 | `		}` |
|       - |  3376 | `		/* Insert in label set */` |
|     113 |  3377 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3378 | `	}` |
|     117 |  3379 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3380 | `	return SXRET_OK;` |
|      61 |  3381 |  |
|       - |  3382 | `/*` |
|       - |  3383 | ` * Compile the so hated 'goto' statement.` |
|       - |  3384 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3385 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3386 | ` * a compiler it has to do this.` |
|       - |  3387 | ` * According to the PHP language reference manual` |
|       - |  3388 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3389 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3390 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3391 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3392 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3393 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3394 | ` *   of a multi-level break` |
|       - |  3395 | ` */` |
|     152 |  3396 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3397 |  |
|       - |  3398 | `	JumpFixup sJump;` |
|       - |  3399 | `	sxi32 rc;` |
|     157 |  3400 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3401 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3402 | `		/* Missing label */` |
|     ! 0 |  3403 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3404 | `		if( rc == SXERR_ABORT ){` |
|       - |  3405 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3406 | `			return SXERR_ABORT;` |
|       - |  3407 | `		}` |
|     ! 0 |  3408 | `		return SXRET_OK;` |
|       - |  3409 | `	}` |
|     157 |  3410 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  3411 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3412 | `		if( rc == SXERR_ABORT ){` |
|       - |  3413 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3414 | `			return SXERR_ABORT;` |
|       - |  3415 | `		}` |
|       3 |  3416 | `	}else{` |
|     153 |  3417 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3418 | `		GenBlock *pBlock;` |
|       - |  3419 | `		char *zDup;` |
|       - |  3420 | `		/* Prepare the jump destination */` |
|     153 |  3421 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3422 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3423 | `		/* Duplicate label name */` |
|     153 |  3424 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3425 | `		if( zDup == 0 ){` |
|     ! 0 |  3426 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3427 | `			return SXERR_ABORT;` |
|       - |  3428 | `		}` |
|     153 |  3429 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3430 | `		pBlock = pGen->pCurrent;` |
|     315 |  3431 | `		while( pBlock ){` |
|     199 |  3432 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      36 |  3433 | `				break;` |
|       - |  3434 | `			}` |
|       - |  3435 | `			/* Point to the upper block */` |
|     167 |  3436 | `			pBlock = pBlock->pParent;` |
|       5 |  3437 | `		}` |
|     153 |  3438 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       9 |  3439 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       9 |  3440 | `			if( rc == SXERR_ABORT ){` |
|       - |  3441 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3442 | `				return SXERR_ABORT;` |
|       - |  3443 | `			}` |
|       3 |  3444 | `		}` |
|     153 |  3445 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      29 |  3446 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      16 |  3447 | `		}else{` |
|     127 |  3448 | `			sJump.pFunc = 0;` |
|       - |  3449 | `		}` |
|       - |  3450 | `		/* Emit the unconditional jump */` |
|     153 |  3451 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3452 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3453 | `		}` |
|       - |  3454 | `	}` |
|     157 |  3455 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3456 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3457 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3458 | `	}` |
|       - |  3459 | `	/* Statement successfully compiled */` |
|     157 |  3460 | `	return SXRET_OK;` |
|      81 |  3461 |  |
|       - |  3462 | `/*` |
|       - |  3463 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3464 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3465 | ` * failure.` |
|       - |  3466 | ` */` |
|      20 |  3467 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  3468 |  |
|       - |  3469 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3470 | `	sxu32 nRawObj;` |
|      10 |  3471 | `	sxu32 nObjIdx;` |
|       - |  3472 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3473 | `	 * a PHP block.` |
|       - |  3474 | `	 */` |
|      10 |  3475 | `Consume:` |
|      22 |  3476 | `	nRawObj = nObjIdx = 0;` |
|      22 |  3477 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3478 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3479 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3480 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3481 | `			return SXERR_ABORT;` |
|       - |  3482 | `		}` |
|       - |  3483 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3484 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3485 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3486 | `		++nRawObj;` |
|     ! 0 |  3487 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3488 | `	}` |
|      22 |  3489 | `	if( nRawObj > 0 ){` |
|       - |  3490 | `		/* Emit the consume instruction */` |
|     ! 0 |  3491 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3492 | `	}` |
|      22 |  3493 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3494 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3495 | `		/* Reset the token set */` |
|     ! 0 |  3496 | `		SySetReset(pTokenSet);` |
|       - |  3497 | `		/* Tokenize input */` |
|     ! 0 |  3498 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3499 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3500 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3501 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3502 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3503 | `		/* Advance the stream cursor */` |
|     ! 0 |  3504 | `		pGen->pRawIn++;` |
|       - |  3505 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3506 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3507 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3508 | `			sxi32 rc;` |
|       - |  3509 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3510 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3511 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3512 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3513 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3514 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3515 | `				return SXERR_ABORT;` |
|     ! 0 |  3516 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3517 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3518 | `			}` |
|     ! 0 |  3519 | `			goto Consume;` |
|       - |  3520 | `		}` |
|     ! 0 |  3521 | `	}else{` |
|       - |  3522 | `		/* No more chunks to process */` |
|      22 |  3523 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  3524 | `		return SXERR_EOF;` |
|       - |  3525 | `	}` |
|     ! 0 |  3526 | `	return SXRET_OK;` |
|      12 |  3527 |  |
|       - |  3528 | `/*` |
|       - |  3529 | ` * Compile a PHP block.` |
|       - |  3530 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3531 | ` * optionally delimited by braces {}.` |
|       - |  3532 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3533 | ` * and this function takes care of generating the appropriate error` |
|       - |  3534 | ` * message.` |
|       - |  3535 | ` */` |
|  384074 |  3536 | `static sxi32 PH7_CompileBlock(` |
|       - |  3537 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3538 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3539 | `	)` |
|       5 |  3540 |  |
|       - |  3541 | `	sxi32 rc;` |
|       - |  3542 | `	sxu32 nLine;` |
|  384079 |  3543 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  382593 |  3544 | `		nLine = pGen->pIn->nLine;` |
|  382593 |  3545 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  382593 |  3546 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3547 | `			return SXERR_ABORT;` |
|       - |  3548 | `		}` |
|  382593 |  3549 | `		pGen->pIn++;` |
|       - |  3550 | `		/* Compile until we hit the closing braces '}' */` |
|  522524 |  3551 | `		for(;;){` |
| 1045053 |  3552 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  3553 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  3554 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3555 | `			 	   return SXERR_ABORT;` |
|       - |  3556 | `				}` |
|      22 |  3557 | `				if( rc == SXERR_EOF ){` |
|       - |  3558 | `					/* No more token to process. Missing closing braces */` |
|      22 |  3559 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      22 |  3560 | `					break;` |
|       - |  3561 | `				}` |
|     ! 0 |  3562 | `			}` |
| 1045033 |  3563 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3564 | `				/* Closing braces found,break immediately*/` |
|  382573 |  3565 | `				pGen->pIn++;` |
|  382573 |  3566 | `				break;` |
|       - |  3567 | `			}` |
|       - |  3568 | `			/* Compile a single statement */` |
|  662465 |  3569 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  662465 |  3570 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3571 | `				return SXERR_ABORT;` |
|       - |  3572 | `			}` |
|       5 |  3573 | `		}` |
|  382593 |  3574 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  192785 |  3575 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3576 | `		pGen->pIn++;` |
|     ! 0 |  3577 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3578 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3579 | `			return SXERR_ABORT;` |
|       - |  3580 | `		}` |
|       - |  3581 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3582 | `		for(;;){` |
|     ! 0 |  3583 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3584 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3585 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3586 | `			 	   return SXERR_ABORT;` |
|       - |  3587 | `				}` |
|     ! 0 |  3588 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3589 | `					/* No more token to process */` |
|     ! 0 |  3590 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3591 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3592 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3593 | `					}` |
|     ! 0 |  3594 | `					break;` |
|       - |  3595 | `				}` |
|     ! 0 |  3596 | `			}` |
|     ! 0 |  3597 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3598 | `				sxi32 nKwrd;` |
|       - |  3599 | `				/* Keyword found */` |
|     ! 0 |  3600 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3601 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3602 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3603 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3604 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3605 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3606 | `						}` |
|     ! 0 |  3607 | `						break;` |
|       - |  3608 | `				}` |
|     ! 0 |  3609 | `			}` |
|       - |  3610 | `			/* Compile a single statement */` |
|     ! 0 |  3611 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3612 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3613 | `				return SXERR_ABORT;` |
|       - |  3614 | `			}` |
|     ! 0 |  3615 | `		}` |
|     ! 0 |  3616 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3617 | `	}else{` |
|       - |  3618 | `		/* Compile a single statement */` |
|    1491 |  3619 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1491 |  3620 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3621 | `			return SXERR_ABORT;` |
|       - |  3622 | `		}` |
|       - |  3623 | `	}` |
|       - |  3624 | `	/* Jump trailing semi-colons ';' */` |
|  384079 |  3625 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3626 | `		pGen->pIn++;` |
|     ! 0 |  3627 | `	}` |
|  384079 |  3628 | `	return SXRET_OK;` |
|  192042 |  3629 |  |
|       - |  3630 | `/*` |
|       - |  3631 | ` * Compile the gentle 'while' statement.` |
|       - |  3632 | ` * According to the PHP language reference` |
|       - |  3633 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3634 | ` *  The basic form of a while statement is:` |
|       - |  3635 | ` *  while (expr)` |
|       - |  3636 | ` *   statement` |
|       - |  3637 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3638 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3639 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3640 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3641 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3642 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3643 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3644 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3645 | ` *  while (expr):` |
|       - |  3646 | ` *    statement` |
|       - |  3647 | ` *   endwhile;` |
|       - |  3648 | ` */` |
|   12764 |  3649 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3650 |  |
|   12769 |  3651 | `	GenBlock *pWhileBlock = 0;` |
|   12769 |  3652 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3653 | `	sxu32 nFalseJump;` |
|       - |  3654 | `	sxu32 nLine;` |
|       - |  3655 | `	sxi32 rc;` |
|   12769 |  3656 | `	nLine = pGen->pIn->nLine;` |
|       - |  3657 | `	/* Jump the 'while' keyword */` |
|   12769 |  3658 | `	pGen->pIn++;` |
|   12769 |  3659 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3660 | `		/* Syntax error */` |
|     ! 0 |  3661 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3662 | `		if( rc == SXERR_ABORT ){` |
|       - |  3663 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3664 | `			return SXERR_ABORT;` |
|       - |  3665 | `		}` |
|     ! 0 |  3666 | `		goto Synchronize;` |
|       - |  3667 | `	}` |
|       - |  3668 | `	/* Jump the left parenthesis '(' */` |
|   12769 |  3669 | `	pGen->pIn++;` |
|       - |  3670 | `	/* Create the loop block */` |
|   12769 |  3671 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   12769 |  3672 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3673 | `		return SXERR_ABORT;` |
|       - |  3674 | `	}` |
|       - |  3675 | `	/* Delimit the condition */` |
|   12769 |  3676 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   12769 |  3677 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3678 | `		/* Empty expression */` |
|       3 |  3679 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3680 | `		if( rc == SXERR_ABORT ){` |
|       - |  3681 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3682 | `			return SXERR_ABORT;` |
|       - |  3683 | `		}` |
|       1 |  3684 | `	}` |
|       - |  3685 | `	/* Swap token streams */` |
|   12769 |  3686 | `	pTmp = pGen->pEnd;` |
|   12769 |  3687 | `	pGen->pEnd = pEnd;` |
|       - |  3688 | `	/* Compile the expression */` |
|   12769 |  3689 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12769 |  3690 | `	if( rc == SXERR_ABORT ){` |
|       - |  3691 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3692 | `		return SXERR_ABORT;` |
|       - |  3693 | `	}` |
|       - |  3694 | `	/* Update token stream */` |
|   12769 |  3695 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3696 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3697 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3698 | `			return SXERR_ABORT;` |
|       - |  3699 | `		}` |
|     ! 0 |  3700 | `		pGen->pIn++;` |
|     ! 0 |  3701 | `	}` |
|       - |  3702 | `	/* Synchronize pointers */` |
|   12769 |  3703 | `	pGen->pIn  = &pEnd[1];` |
|   12769 |  3704 | `	pGen->pEnd = pTmp;` |
|       - |  3705 | `	/* Emit the false jump */` |
|   12769 |  3706 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3707 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   12769 |  3708 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3709 | `	/* Compile the loop body */` |
|   12769 |  3710 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   12769 |  3711 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3712 | `		return SXERR_ABORT;` |
|       - |  3713 | `	}` |
|       - |  3714 | `	/* Emit the unconditional jump to the start of the loop */` |
|   12769 |  3715 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3716 | `	/* Fix all jumps now the destination is resolved */` |
|   12769 |  3717 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3718 | `	/* Release the loop block */` |
|   12769 |  3719 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3720 | `	/* Statement successfully compiled */` |
|   12769 |  3721 | `	return SXRET_OK;` |
|     ! 0 |  3722 | `Synchronize:` |
|       - |  3723 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3724 | `	 * compiling this erroneous block.` |
|       - |  3725 | `	 */` |
|     ! 0 |  3726 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3727 | `		pGen->pIn++;` |
|     ! 0 |  3728 | `	}` |
|     ! 0 |  3729 | `	return SXRET_OK;` |
|    6387 |  3730 |  |
|       - |  3731 | `/*` |
|       - |  3732 | ` * Compile the ugly do..while() statement.` |
|       - |  3733 | ` * According to the PHP language reference` |
|       - |  3734 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3735 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3736 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3737 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3738 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3739 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3740 | ` *  would end immediately).` |
|       - |  3741 | ` *  There is just one syntax for do-while loops:` |
|       - |  3742 | ` *  <?php` |
|       - |  3743 | ` *  $i = 0;` |
|       - |  3744 | ` *  do {` |
|       - |  3745 | ` *   echo $i;` |
|       - |  3746 | ` *  } while ($i > 0);` |
|       - |  3747 | ` * ?>` |
|       - |  3748 | ` */` |
|       2 |  3749 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3750 |  |
|       3 |  3751 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3752 | `	GenBlock *pDoBlock = 0;` |
|       - |  3753 | `	sxu32 nLine;` |
|       - |  3754 | `	sxi32 rc;` |
|       3 |  3755 | `	nLine = pGen->pIn->nLine;` |
|       - |  3756 | `	/* Jump the 'do' keyword */` |
|       3 |  3757 | `	pGen->pIn++;` |
|       - |  3758 | `	/* Create the loop block */` |
|       3 |  3759 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3760 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3761 | `		return SXERR_ABORT;` |
|       - |  3762 | `	}` |
|       - |  3763 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3764 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3765 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3766 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3767 | `		return SXERR_ABORT;` |
|       - |  3768 | `	}` |
|       3 |  3769 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3770 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3771 | `	}` |
|       3 |  3772 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3773 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3774 | `			/* Missing 'while' statement */` |
|       3 |  3775 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3776 | `			if( rc == SXERR_ABORT ){` |
|       - |  3777 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3778 | `				return SXERR_ABORT;` |
|       - |  3779 | `			}` |
|       3 |  3780 | `			goto Synchronize;` |
|       - |  3781 | `	}` |
|       - |  3782 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3783 | `	pGen->pIn++;` |
|     ! 0 |  3784 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3785 | `		/* Syntax error */` |
|     ! 0 |  3786 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3787 | `		if( rc == SXERR_ABORT ){` |
|       - |  3788 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3789 | `			return SXERR_ABORT;` |
|       - |  3790 | `		}` |
|     ! 0 |  3791 | `		goto Synchronize;` |
|       - |  3792 | `	}` |
|       - |  3793 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3794 | `	pGen->pIn++;` |
|       - |  3795 | `	/* Delimit the condition */` |
|     ! 0 |  3796 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3797 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3798 | `		/* Empty expression */` |
|     ! 0 |  3799 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3800 | `		if( rc == SXERR_ABORT ){` |
|       - |  3801 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3802 | `			return SXERR_ABORT;` |
|       - |  3803 | `		}` |
|     ! 0 |  3804 | `		goto Synchronize;` |
|       - |  3805 | `	}` |
|       - |  3806 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3807 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3808 | `		JumpFixup *aPost;` |
|       - |  3809 | `		VmInstr *pInstr;` |
|       - |  3810 | `		sxu32 nJumpDest;` |
|       - |  3811 | `		sxu32 n;` |
|     ! 0 |  3812 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3813 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3814 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3815 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3816 | `			if( pInstr ){` |
|       - |  3817 | `				/* Fix */` |
|     ! 0 |  3818 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3819 | `			}` |
|     ! 0 |  3820 | `		}` |
|     ! 0 |  3821 | `	}` |
|       - |  3822 | `	/* Swap token streams */` |
|     ! 0 |  3823 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3824 | `	pGen->pEnd = pEnd;` |
|       - |  3825 | `	/* Compile the expression */` |
|     ! 0 |  3826 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3827 | `	if( rc == SXERR_ABORT ){` |
|       - |  3828 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3829 | `		return SXERR_ABORT;` |
|       - |  3830 | `	}` |
|       - |  3831 | `	/* Update token stream */` |
|     ! 0 |  3832 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3833 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3834 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3835 | `			return SXERR_ABORT;` |
|       - |  3836 | `		}` |
|     ! 0 |  3837 | `		pGen->pIn++;` |
|     ! 0 |  3838 | `	}` |
|     ! 0 |  3839 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3840 | `	pGen->pEnd = pTmp;` |
|       - |  3841 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3842 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3843 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3844 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3845 | `	/* Release the loop block */` |
|     ! 0 |  3846 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3847 | `	/* Statement successfully compiled */` |
|     ! 0 |  3848 | `	return SXRET_OK;` |
|       1 |  3849 | `Synchronize:` |
|       - |  3850 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3851 | `	 * compiling this erroneous block.` |
|       - |  3852 | `	 */` |
|       3 |  3853 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3854 | `		pGen->pIn++;` |
|     ! 0 |  3855 | `	}` |
|       3 |  3856 | `	return SXRET_OK;` |
|       2 |  3857 |  |
|       - |  3858 | `/*` |
|       - |  3859 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3860 | ` * According to the PHP language reference` |
|       - |  3861 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3862 | ` *  The syntax of a for loop is:` |
|       - |  3863 | ` *  for (expr1; expr2; expr3)` |
|       - |  3864 | ` *   statement` |
|       - |  3865 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3866 | ` *  the beginning of the loop.` |
|       - |  3867 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  3868 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  3869 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  3870 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  3871 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  3872 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  3873 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  3874 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  3875 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  3876 | ` *  of using the for truth expression.` |
|       - |  3877 | ` */` |
|   12776 |  3878 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  3879 |  |
|   12781 |  3880 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   12781 |  3881 | `	GenBlock *pForBlock = 0;` |
|       - |  3882 | `	sxu32 nFalseJump;` |
|       - |  3883 | `	sxu32 nLine;` |
|       - |  3884 | `	sxi32 rc;` |
|   12781 |  3885 | `	nLine = pGen->pIn->nLine;` |
|       - |  3886 | `	/* Jump the 'for' keyword */` |
|   12781 |  3887 | `	pGen->pIn++;` |
|   12781 |  3888 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3889 | `		/* Syntax error */` |
|     ! 0 |  3890 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  3891 | `		if( rc == SXERR_ABORT ){` |
|       - |  3892 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3893 | `			return SXERR_ABORT;` |
|       - |  3894 | `		}` |
|     ! 0 |  3895 | `		return SXRET_OK;` |
|       - |  3896 | `	}` |
|       - |  3897 | `	/* Jump the left parenthesis '(' */` |
|   12781 |  3898 | `	pGen->pIn++;` |
|       - |  3899 | `	/* Delimit the init-expr;condition;post-expr */` |
|   12781 |  3900 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   12781 |  3901 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3902 | `		/* Empty expression */` |
|     ! 0 |  3903 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  3904 | `		if( rc == SXERR_ABORT ){` |
|       - |  3905 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3906 | `			return SXERR_ABORT;` |
|       - |  3907 | `		}` |
|       - |  3908 | `		/* Synchronize */` |
|     ! 0 |  3909 | `		pGen->pIn = pEnd;` |
|     ! 0 |  3910 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3911 | `			pGen->pIn++;` |
|     ! 0 |  3912 | `		}` |
|     ! 0 |  3913 | `		return SXRET_OK;` |
|       - |  3914 | `	}` |
|       - |  3915 | `	/* Swap token streams */` |
|   12781 |  3916 | `	pTmp = pGen->pEnd;` |
|   12781 |  3917 | `	pGen->pEnd = pEnd;` |
|       - |  3918 | `	/* Compile initialization expressions if available */` |
|   12781 |  3919 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3920 | `	/* Pop operand lvalues */` |
|   12781 |  3921 | `	if( rc == SXERR_ABORT ){` |
|       - |  3922 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3923 | `		return SXERR_ABORT;` |
|   12781 |  3924 | `	}else if( rc != SXERR_EMPTY ){` |
|   12779 |  3925 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6387 |  3926 | `	}` |
|   12781 |  3927 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3928 | `		/* Syntax error */` |
|     ! 0 |  3929 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3930 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  3931 | `		if( rc == SXERR_ABORT ){` |
|       - |  3932 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3933 | `			return SXERR_ABORT;` |
|       - |  3934 | `		}` |
|     ! 0 |  3935 | `		return SXRET_OK;` |
|       - |  3936 | `	}` |
|       - |  3937 | `	/* Jump the trailing ';' */` |
|   12781 |  3938 | `	pGen->pIn++;` |
|       - |  3939 | `	/* Create the loop block */` |
|   12781 |  3940 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   12781 |  3941 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3942 | `		return SXERR_ABORT;` |
|       - |  3943 | `	}` |
|       - |  3944 | `	/* Deffer continue jumps */` |
|   12781 |  3945 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  3946 | `	/* Compile the condition */` |
|   12781 |  3947 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12781 |  3948 | `	if( rc == SXERR_ABORT ){` |
|       - |  3949 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3950 | `		return SXERR_ABORT;` |
|   12781 |  3951 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  3952 | `		/* Emit the false jump */` |
|   12779 |  3953 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3954 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   12779 |  3955 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    6387 |  3956 | `	}` |
|   12781 |  3957 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3958 | `		/* Syntax error */` |
|       6 |  3959 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3960 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  3961 | `		if( rc == SXERR_ABORT ){` |
|       - |  3962 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3963 | `			return SXERR_ABORT;` |
|       - |  3964 | `		}` |
|       6 |  3965 | `		return SXRET_OK;` |
|       - |  3966 | `	}` |
|       - |  3967 | `	/* Jump the trailing ';' */` |
|   12777 |  3968 | `	pGen->pIn++;` |
|       - |  3969 | `	/* Save the post condition stream */` |
|   12777 |  3970 | `	pPostStart = pGen->pIn;` |
|       - |  3971 | `	/* Compile the loop body */` |
|   12777 |  3972 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   12777 |  3973 | `	pGen->pEnd = pTmp;` |
|   12777 |  3974 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   12777 |  3975 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3976 | `		return SXERR_ABORT;` |
|       - |  3977 | `	}` |
|       - |  3978 | `	/* Fix post-continue jumps */` |
|   12777 |  3979 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  3980 | `		JumpFixup *aPost;` |
|       - |  3981 | `		VmInstr *pInstr;` |
|       - |  3982 | `		sxu32 nJumpDest;` |
|       - |  3983 | `		sxu32 n;` |
|      14 |  3984 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  3985 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  3986 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  3987 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  3988 | `			if( pInstr ){` |
|       - |  3989 | `				/* Fix jump */` |
|      14 |  3990 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  3991 | `			}` |
|       8 |  3992 | `		}` |
|       6 |  3993 | `	}` |
|       - |  3994 | `	/* compile the post-expressions if available */` |
|   12777 |  3995 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3996 | `		pPostStart++;` |
|     ! 0 |  3997 | `	}` |
|   12777 |  3998 | `	if( pPostStart < pEnd ){` |
|       - |  3999 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   12777 |  4000 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   12777 |  4001 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12777 |  4002 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4003 | `			/* Syntax error */` |
|     ! 0 |  4004 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4005 | `			if( rc == SXERR_ABORT ){` |
|       - |  4006 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4007 | `				return SXERR_ABORT;` |
|       - |  4008 | `			}` |
|     ! 0 |  4009 | `			return SXRET_OK;` |
|       - |  4010 | `		}` |
|   12777 |  4011 | `		RE_SWAP_DELIMITER(pGen);` |
|   12777 |  4012 | `		if( rc == SXERR_ABORT ){` |
|       - |  4013 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4014 | `			return SXERR_ABORT;` |
|   12777 |  4015 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4016 | `			/* Pop operand lvalue */` |
|   12777 |  4017 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6386 |  4018 | `		}` |
|    6386 |  4019 | `	}` |
|       - |  4020 | `	/* Emit the unconditional jump to the start of the loop */` |
|   12777 |  4021 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4022 | `	/* Fix all jumps now the destination is resolved */` |
|   12777 |  4023 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4024 | `	/* Release the loop block */` |
|   12777 |  4025 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4026 | `	/* Statement successfully compiled */` |
|   12777 |  4027 | `	return SXRET_OK;` |
|    6393 |  4028 |  |
|       - |  4029 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4030 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4031 | ` * are allowed.` |
|       - |  4032 | ` */` |
|    6830 |  4033 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4034 |  |
|    6835 |  4035 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    6835 |  4036 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4037 | `		/* Unexpected expression */` |
|     ! 0 |  4038 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4039 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4040 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4041 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4042 | `		}` |
|     ! 0 |  4043 | `	}` |
|    6835 |  4044 | `	return rc;` |
|       5 |  4045 |  |
|       - |  4046 | `/*` |
|       - |  4047 | ` * Compile the 'foreach' statement.` |
|       - |  4048 | ` * According to the PHP language reference` |
|       - |  4049 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4050 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4051 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4052 | ` *  is a minor but useful extension of the first:` |
|       - |  4053 | ` *  foreach (array_expression as $value)` |
|       - |  4054 | ` *    statement` |
|       - |  4055 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4056 | ` *   statement` |
|       - |  4057 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4058 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4059 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4060 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4061 | ` *  to the variable $key on each loop.` |
|       - |  4062 | ` *  Note:` |
|       - |  4063 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4064 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4065 | ` *  Note:` |
|       - |  4066 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4067 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4068 | ` *  or after the foreach without resetting it.` |
|       - |  4069 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4070 | ` *  of copying the value.` |
|       - |  4071 | ` */` |
|    3480 |  4072 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4073 |  |
|    3485 |  4074 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3485 |  4075 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3485 |  4076 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4077 | `	ph7_foreach_info *pInfo;` |
|       - |  4078 | `	sxu32 nFalseJump;` |
|       - |  4079 | `	VmInstr *pInstr;` |
|       - |  4080 | `	sxu32 nLine;` |
|       - |  4081 | `	sxi32 rc;` |
|    3485 |  4082 | `	nLine = pGen->pIn->nLine;` |
|       - |  4083 | `	/* Jump the 'foreach' keyword */` |
|    3485 |  4084 | `	pGen->pIn++;` |
|    3485 |  4085 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4086 | `		/* Syntax error */` |
|     ! 0 |  4087 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4088 | `		if( rc == SXERR_ABORT ){` |
|       - |  4089 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4090 | `			return SXERR_ABORT;` |
|       - |  4091 | `		}` |
|     ! 0 |  4092 | `		goto Synchronize;` |
|       - |  4093 | `	}` |
|       - |  4094 | `	/* Jump the left parenthesis '(' */` |
|    3485 |  4095 | `	pGen->pIn++;` |
|       - |  4096 | `	/* Create the loop block */` |
|    3485 |  4097 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3485 |  4098 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4099 | `		return SXERR_ABORT;` |
|       - |  4100 | `	}` |
|       - |  4101 | `	/* Delimit the expression */` |
|    3485 |  4102 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3485 |  4103 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4104 | `		/* Empty expression */` |
|     ! 0 |  4105 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4106 | `		if( rc == SXERR_ABORT ){` |
|       - |  4107 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4108 | `			return SXERR_ABORT;` |
|       - |  4109 | `		}` |
|       - |  4110 | `		/* Synchronize */` |
|     ! 0 |  4111 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4112 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4113 | `			pGen->pIn++;` |
|     ! 0 |  4114 | `		}` |
|     ! 0 |  4115 | `		return SXRET_OK;` |
|       - |  4116 | `	}` |
|       - |  4117 | `	/* Compile the array expression */` |
|    3485 |  4118 | `	pCur = pGen->pIn;` |
|   23331 |  4119 | `	while( pCur < pEnd ){` |
|   23331 |  4120 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3499 |  4121 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3499 |  4122 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4123 | `				/* Break with the first 'as' found */` |
|    3485 |  4124 | `				break;` |
|       - |  4125 | `			}` |
|       7 |  4126 | `		}` |
|       - |  4127 | `		/* Advance the stream cursor */` |
|   19851 |  4128 | `		pCur++;` |
|       5 |  4129 | `	}` |
|    3485 |  4130 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4131 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4132 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4133 | `		if( rc == SXERR_ABORT ){` |
|       - |  4134 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4135 | `			return SXERR_ABORT;` |
|       - |  4136 | `		}` |
|     ! 0 |  4137 | `		goto Synchronize;` |
|       - |  4138 | `	}` |
|       - |  4139 | `	/* Swap token streams */` |
|    3485 |  4140 | `	pTmp = pGen->pEnd;` |
|    3485 |  4141 | `	pGen->pEnd = pCur;` |
|    3485 |  4142 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3485 |  4143 | `	if( rc == SXERR_ABORT ){` |
|       - |  4144 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4145 | `		return SXERR_ABORT;` |
|       - |  4146 | `	}` |
|       - |  4147 | `	/* Update token stream */` |
|    3485 |  4148 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4149 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4150 | `		if( rc == SXERR_ABORT ){` |
|       - |  4151 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4152 | `			return SXERR_ABORT;` |
|       - |  4153 | `		}` |
|     ! 0 |  4154 | `		pGen->pIn++;` |
|     ! 0 |  4155 | `	}` |
|    3485 |  4156 | `	pCur++; /* Jump the 'as' keyword */` |
|    3485 |  4157 | `	pGen->pIn = pCur;` |
|    3485 |  4158 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4159 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4160 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4161 | `			return SXERR_ABORT;` |
|       - |  4162 | `		}` |
|     ! 0 |  4163 | `	}` |
|       - |  4164 | `	/* Create the foreach context */` |
|    3485 |  4165 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3485 |  4166 | `	if( pInfo == 0 ){` |
|     ! 0 |  4167 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4168 | `		return SXERR_ABORT;` |
|       - |  4169 | `	}` |
|       - |  4170 | `	/* Zero the structure */` |
|    3485 |  4171 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4172 | `	/* Initialize structure fields */` |
|    3485 |  4173 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4174 | `	/* Check if we have a key field */` |
|   10495 |  4175 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    7015 |  4176 | `		pCur++;` |
|       5 |  4177 | `	}` |
|    3485 |  4178 | `	if( pCur < pEnd ){` |
|       - |  4179 | `		/* Compile the expression holding the key name */` |
|    3365 |  4180 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4181 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4182 | `			if( rc == SXERR_ABORT ){` |
|       - |  4183 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4184 | `				return SXERR_ABORT;` |
|       - |  4185 | `			}` |
|     ! 0 |  4186 | `		}else{` |
|    3365 |  4187 | `			pGen->pEnd = pCur;` |
|    3365 |  4188 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3365 |  4189 | `			if( rc == SXERR_ABORT ){` |
|       - |  4190 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4191 | `				return SXERR_ABORT;` |
|       - |  4192 | `			}` |
|    3365 |  4193 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3365 |  4194 | `			if( pInstr->p3 ){` |
|       - |  4195 | `				/* Record key name */` |
|    3365 |  4196 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1680 |  4197 | `			}` |
|    3365 |  4198 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4199 | `		}` |
|    3365 |  4200 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1680 |  4201 | `	}` |
|    3485 |  4202 | `	pGen->pEnd = pEnd;` |
|    3485 |  4203 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4204 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4205 | `		if( rc == SXERR_ABORT ){` |
|       - |  4206 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4207 | `			return SXERR_ABORT;` |
|       - |  4208 | `		}` |
|     ! 0 |  4209 | `		goto Synchronize;` |
|       - |  4210 | `	}` |
|    3485 |  4211 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4212 | `		pGen->pIn++;` |
|       - |  4213 | `		/* Pass by reference  */` |
|      11 |  4214 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4215 | `	}` |
|       - |  4216 | `	/* Check if the value target is list() */` |
|    3485 |  4217 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4218 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4219 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4220 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4221 | `		 */` |
|       - |  4222 | `		static int iForeachListCnt = 0;` |
|       - |  4223 | `		char zTmp[128];` |
|       - |  4224 | `		sxu32 nLen;` |
|       - |  4225 | `		char *zDup;` |
|      10 |  4226 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4227 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4228 | `		if( zDup == 0 ){` |
|     ! 0 |  4229 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4230 | `			return SXERR_ABORT;` |
|       - |  4231 | `		}` |
|      10 |  4232 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4233 | `		/* Save list() token boundaries */` |
|      10 |  4234 | `		pListStart = pGen->pIn;` |
|       - |  4235 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4236 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4237 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4238 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4239 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4240 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4241 | `				return SXERR_ABORT;` |
|       - |  4242 | `			}` |
|       3 |  4243 | `			goto Synchronize;` |
|       - |  4244 | `		}` |
|       7 |  4245 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4246 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4247 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4248 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4249 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4250 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4251 | `				return SXERR_ABORT;` |
|       - |  4252 | `			}` |
|     ! 0 |  4253 | `			goto Synchronize;` |
|       - |  4254 | `		}` |
|       7 |  4255 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4256 | `		pListEnd = pGen->pIn;` |
|       7 |  4257 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3480 |  4258 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4259 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4260 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4261 | `		 */` |
|       - |  4262 | `		static int iForeachShortListCnt = 0;` |
|       - |  4263 | `		char zTmp[128];` |
|       - |  4264 | `		sxu32 nLen;` |
|       - |  4265 | `		char *zDup;` |
|       3 |  4266 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 |  4267 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 |  4268 | `		if( zDup == 0 ){` |
|     ! 0 |  4269 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4270 | `			return SXERR_ABORT;` |
|       - |  4271 | `		}` |
|       3 |  4272 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4273 | `		/* Save [...] token boundaries */` |
|       3 |  4274 | `		pListStart = pGen->pIn;` |
|       - |  4275 | `		/* Advance past [...] */` |
|       3 |  4276 | `		pGen->pIn++; /* Jump '[' */` |
|       3 |  4277 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 |  4278 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4279 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4280 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4281 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4282 | `				return SXERR_ABORT;` |
|       - |  4283 | `			}` |
|     ! 0 |  4284 | `			goto Synchronize;` |
|       - |  4285 | `		}` |
|       3 |  4286 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 |  4287 | `		pListEnd = pGen->pIn;` |
|       3 |  4288 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 |  4289 | `	}else{` |
|       - |  4290 | `		/* Compile the expression holding the value name */` |
|    3475 |  4291 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3475 |  4292 | `		if( rc == SXERR_ABORT ){` |
|       - |  4293 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4294 | `			return SXERR_ABORT;` |
|       - |  4295 | `		}` |
|    3475 |  4296 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3475 |  4297 | `		if( pInstr->p3 ){` |
|       - |  4298 | `			/* Record value name */` |
|    3475 |  4299 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1735 |  4300 | `		}` |
|       - |  4301 | `	}` |
|       - |  4302 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3483 |  4303 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4304 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3483 |  4305 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4306 | `	/* Record the first instruction to execute */` |
|    3483 |  4307 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4308 | `	/* Emit the FOREACH_STEP instruction */` |
|    3483 |  4309 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4310 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3483 |  4311 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4312 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3483 |  4313 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4314 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4315 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4316 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4317 | `		 */` |
|       9 |  4318 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4319 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4320 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4321 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4322 | `		 */` |
|       9 |  4323 | `		pSavedIn = pGen->pIn;` |
|       9 |  4324 | `		pSavedEnd = pGen->pEnd;` |
|       9 |  4325 | `		pGen->pIn = pListStart;` |
|       9 |  4326 | `		pGen->pEnd = pListEnd;` |
|       9 |  4327 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 |  4328 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 |  4329 | `		}else{` |
|       7 |  4330 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4331 | `		}` |
|       9 |  4332 | `		pGen->pIn = pSavedIn;` |
|       9 |  4333 | `		pGen->pEnd = pSavedEnd;` |
|       9 |  4334 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4335 | `			return SXERR_ABORT;` |
|       - |  4336 | `		}` |
|       - |  4337 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 |  4338 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 |  4339 | `	}` |
|       - |  4340 | `	/* Compile the loop body */` |
|    3483 |  4341 | `	pGen->pIn = &pEnd[1];` |
|    3483 |  4342 | `	pGen->pEnd = pTmp;` |
|    3483 |  4343 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3483 |  4344 | `	if( rc == SXERR_ABORT ){` |
|       - |  4345 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4346 | `		return SXERR_ABORT;` |
|       - |  4347 | `	}` |
|       - |  4348 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3483 |  4349 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4350 | `	/* Fix all jumps now the destination is resolved */` |
|    3483 |  4351 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4352 | `	/* Release the loop block */` |
|    3483 |  4353 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4354 | `	/* Statement successfully compiled */` |
|    3483 |  4355 | `	return SXRET_OK;` |
|       1 |  4356 | `Synchronize:` |
|       - |  4357 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4358 | `	 * compiling this erroneous block.` |
|       - |  4359 | `	 */` |
|       3 |  4360 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4361 | `		pGen->pIn++;` |
|     ! 0 |  4362 | `	}` |
|       3 |  4363 | `	return SXRET_OK;` |
|    1745 |  4364 |  |
|       - |  4365 | `/*` |
|       - |  4366 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4367 | ` * According to the PHP language reference` |
|       - |  4368 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4369 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4370 | ` *  that is similar to that of C:` |
|       - |  4371 | ` *  if (expr)` |
|       - |  4372 | ` *   statement` |
|       - |  4373 | ` *  else construct:` |
|       - |  4374 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4375 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4376 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4377 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4378 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4379 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4380 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4381 | ` *  elseif` |
|       - |  4382 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4383 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4384 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4385 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4386 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4387 | ` *   <?php` |
|       - |  4388 | ` *    if ($a > $b) {` |
|       - |  4389 | ` *     echo "a is bigger than b";` |
|       - |  4390 | ` *    } elseif ($a == $b) {` |
|       - |  4391 | ` *     echo "a is equal to b";` |
|       - |  4392 | ` *    } else {` |
|       - |  4393 | ` *     echo "a is smaller than b";` |
|       - |  4394 | ` *    }` |
|       - |  4395 | ` *    ?>` |
|       - |  4396 | ` */` |
|  132908 |  4397 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4398 |  |
|  132913 |  4399 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  132913 |  4400 | `	GenBlock *pCondBlock = 0;` |
|       - |  4401 | `	sxu32 nJumpIdx;` |
|       - |  4402 | `	sxu32 nKeyID;` |
|       - |  4403 | `	sxi32 rc;` |
|       - |  4404 | `	/* Jump the 'if' keyword */` |
|  132913 |  4405 | `	pGen->pIn++;` |
|  132913 |  4406 | `	pToken = pGen->pIn;` |
|       - |  4407 | `	/* Create the conditional block */` |
|  132913 |  4408 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  132913 |  4409 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4410 | `		return SXERR_ABORT;` |
|       - |  4411 | `	}` |
|       - |  4412 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   72795 |  4413 | `	for(;;){` |
|  145595 |  4414 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4415 | `			/* Syntax error */` |
|     ! 0 |  4416 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4417 | `				pToken--;` |
|     ! 0 |  4418 | `			}` |
|     ! 0 |  4419 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4420 | `			if( rc == SXERR_ABORT ){` |
|       - |  4421 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4422 | `				return SXERR_ABORT;` |
|       - |  4423 | `			}` |
|     ! 0 |  4424 | `			goto Synchronize;` |
|       - |  4425 | `		}` |
|       - |  4426 | `		/* Jump the left parenthesis '(' */` |
|  145595 |  4427 | `		pToken++;` |
|       - |  4428 | `		/* Delimit the condition */` |
|  145595 |  4429 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  145595 |  4430 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4431 | `			/* Syntax error */` |
|     ! 0 |  4432 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4433 | `				pToken--;` |
|     ! 0 |  4434 | `			}` |
|     ! 0 |  4435 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4436 | `			if( rc == SXERR_ABORT ){` |
|       - |  4437 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4438 | `				return SXERR_ABORT;` |
|       - |  4439 | `			}` |
|     ! 0 |  4440 | `			goto Synchronize;` |
|       - |  4441 | `		}` |
|       - |  4442 | `		/* Swap token streams */` |
|  145595 |  4443 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4444 | `		/* Compile the condition */` |
|  145595 |  4445 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4446 | `		/* Update token stream */` |
|  145595 |  4447 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4448 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4449 | `			pGen->pIn++;` |
|     ! 0 |  4450 | `		}` |
|  145595 |  4451 | `		pGen->pIn  = &pEnd[1];` |
|  145595 |  4452 | `		pGen->pEnd = pTmp;` |
|  145595 |  4453 | `		if( rc == SXERR_ABORT ){` |
|       - |  4454 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4455 | `			return SXERR_ABORT;` |
|       - |  4456 | `		}` |
|       - |  4457 | `		/* Emit the false jump */` |
|  145595 |  4458 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4459 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  145595 |  4460 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4461 | `		/* Compile the body */` |
|  145595 |  4462 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  145595 |  4463 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4464 | `			return SXERR_ABORT;` |
|       - |  4465 | `		}` |
|  145595 |  4466 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   40625 |  4467 | `			break;` |
|       - |  4468 | `		}` |
|       - |  4469 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   64355 |  4470 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   64355 |  4471 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   41439 |  4472 | `			break;` |
|       - |  4473 | `		}` |
|       - |  4474 | `		/* Emit the unconditional jump */` |
|   22921 |  4475 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4476 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   22921 |  4477 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   22921 |  4478 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   16567 |  4479 | `			pToken = &pGen->pIn[1];` |
|   16567 |  4480 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    6358 |  4481 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5122 |  4482 | `					break;` |
|       - |  4483 | `			}` |
|    6333 |  4484 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3164 |  4485 | `		}` |
|   12687 |  4486 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4487 | `		/* Synchronize cursors */` |
|   12687 |  4488 | `		pToken = pGen->pIn;` |
|       - |  4489 | `		/* Fix the false jump */` |
|   12687 |  4490 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4491 | `	} /* For(;;) */` |
|       - |  4492 | `	/* Fix the false jump */` |
|  132913 |  4493 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  132913 |  4494 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   51668 |  4495 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4496 | `			/* Compile the else block */` |
|   10239 |  4497 | `			pGen->pIn++;` |
|   10239 |  4498 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   10239 |  4499 | `			if( rc == SXERR_ABORT ){` |
|       - |  4500 |  |
|     ! 0 |  4501 | `				return SXERR_ABORT;` |
|       - |  4502 | `			}` |
|    5117 |  4503 | `	}` |
|  132913 |  4504 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4505 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  132913 |  4506 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4507 | `	/* Release the conditional block */` |
|  132913 |  4508 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4509 | `	/* Statement successfully compiled */` |
|  132913 |  4510 | `	return SXRET_OK;` |
|     ! 0 |  4511 | `Synchronize:` |
|       - |  4512 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4513 | `	 */` |
|     ! 0 |  4514 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4515 | `		pGen->pIn++;` |
|     ! 0 |  4516 | `	}` |
|     ! 0 |  4517 | `	return SXRET_OK;` |
|   66459 |  4518 |  |
|       - |  4519 | `/*` |
|       - |  4520 | ` * Compile the global construct.` |
|       - |  4521 | ` * According to the PHP language reference` |
|       - |  4522 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4523 | ` *  to be used in that function.` |
|       - |  4524 | ` *  Example #1 Using global` |
|       - |  4525 | ` *  <?php` |
|       - |  4526 | ` *   $a = 1;` |
|       - |  4527 | ` *   $b = 2;` |
|       - |  4528 | ` *   function Sum()` |
|       - |  4529 | ` *   {` |
|       - |  4530 | ` *    global $a, $b;` |
|       - |  4531 | ` *    $b = $a + $b;` |
|       - |  4532 | ` *   }` |
|       - |  4533 | ` *   Sum();` |
|       - |  4534 | ` *   echo $b;` |
|       - |  4535 | ` *  ?>` |
|       - |  4536 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4537 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4538 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4539 | ` */` |
|      36 |  4540 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4541 |  |
|      41 |  4542 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4543 | `	sxi32 nExpr;` |
|       - |  4544 | `	sxi32 rc;` |
|       - |  4545 | `	/* Jump the 'global' keyword */` |
|      41 |  4546 | `	pGen->pIn++;` |
|      41 |  4547 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4548 | `		/* Nothing to process */` |
|     ! 0 |  4549 | `		return SXRET_OK;` |
|       - |  4550 | `	}` |
|      41 |  4551 | `	pTmp = pGen->pEnd;` |
|      41 |  4552 | `	nExpr = 0;` |
|      87 |  4553 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4554 | `		if( pGen->pIn < pNext ){` |
|      51 |  4555 | `			pGen->pEnd = pNext;` |
|      51 |  4556 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4557 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4558 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4559 | `					return SXERR_ABORT;` |
|       - |  4560 | `				}` |
|     ! 0 |  4561 | `			}else{` |
|      51 |  4562 | `				pGen->pIn++;` |
|      51 |  4563 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4564 | `					/* Emit a warning */` |
|     ! 0 |  4565 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4566 | `				}else{` |
|      51 |  4567 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4568 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4569 | `						return SXERR_ABORT;` |
|      51 |  4570 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4571 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4572 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4573 | `							/* Variable name, not a constant */` |
|      51 |  4574 | `							pLast->iP1 = 0;` |
|      23 |  4575 | `						}` |
|      51 |  4576 | `						nExpr++;` |
|      23 |  4577 | `					}` |
|       - |  4578 | `				}` |
|       - |  4579 | `			}` |
|      23 |  4580 | `		}` |
|       - |  4581 | `		/* Next expression in the stream */` |
|      51 |  4582 | `		pGen->pIn = pNext;` |
|       - |  4583 | `		/* Jump trailing commas */` |
|      61 |  4584 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4585 | `			pGen->pIn++;` |
|       5 |  4586 | `		}` |
|       5 |  4587 | `	}` |
|       - |  4588 | `	/* Restore token stream */` |
|      41 |  4589 | `	pGen->pEnd = pTmp;` |
|      41 |  4590 | `	if( nExpr > 0 ){` |
|       - |  4591 | `		/* Emit the uplink instruction */` |
|      41 |  4592 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4593 | `	}` |
|      41 |  4594 | `	return SXRET_OK;` |
|      23 |  4595 |  |
|       - |  4596 | `/*` |
|       - |  4597 | ` * Compile the return statement.` |
|       - |  4598 | ` * According to the PHP language reference` |
|       - |  4599 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4600 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4601 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4602 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4603 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4604 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4605 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4606 | ` *  from within the main script file, then script execution end.` |
|       - |  4607 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4608 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4609 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4610 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4611 | ` */` |
|  209924 |  4612 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4613 |  |
|  209929 |  4614 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4615 | `	sxi32 rc;` |
|       - |  4616 | `	/* Jump the 'return' keyword */` |
|  209929 |  4617 | `	pGen->pIn++;` |
|  209929 |  4618 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4619 | `		/* Compile the expression */` |
|  209903 |  4620 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  209903 |  4621 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4622 | `			return SXERR_ABORT;` |
|  209903 |  4623 | `		}else if(rc != SXERR_EMPTY ){` |
|  209903 |  4624 | `			nRet = 1;` |
|  104949 |  4625 | `		}` |
|  104949 |  4626 | `	}` |
|       - |  4627 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4628 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4629 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4630 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4631 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  209929 |  4632 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  209929 |  4633 | `	return SXRET_OK;` |
|  104967 |  4634 |  |
|       - |  4635 | `/*` |
|       - |  4636 | ` * Compile a yield expression.` |
|       - |  4637 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4638 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4639 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4640 | ` */` |
|      72 |  4641 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4642 |  |
|       - |  4643 | `	SyToken *pTmp, *pSplit;` |
|      77 |  4644 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      77 |  4645 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4646 | `	sxi32 rc;` |
|      36 |  4647 | `	(void)iCompileFlag;` |
|       - |  4648 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      77 |  4649 | `	pGen->pIn++;` |
|       - |  4650 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4651 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      77 |  4652 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4653 | `		/* Bare yield — no value */` |
|     ! 0 |  4654 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4655 | `		return SXRET_OK;` |
|       - |  4656 | `	}` |
|       - |  4657 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      77 |  4658 | `	pSplit = 0;` |
|       - |  4659 | `	{` |
|      77 |  4660 | `		SyToken *pCur = pGen->pIn;` |
|      77 |  4661 | `		sxi32 nNest = 0;` |
|     163 |  4662 | `		while( pCur < pGen->pEnd ){` |
|     105 |  4663 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4664 | `				nNest++;` |
|     105 |  4665 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4666 | `				nNest--;` |
|     105 |  4667 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4668 | `				pSplit = pCur;` |
|      16 |  4669 | `				break;` |
|       - |  4670 | `			}` |
|      91 |  4671 | `			pCur++;` |
|       5 |  4672 | `		}` |
|       - |  4673 | `	}` |
|      77 |  4674 | `	pTmp = pGen->pEnd;` |
|      77 |  4675 | `	if( pSplit ){` |
|       - |  4676 | `		/* yield $key => $value */` |
|      16 |  4677 | `		pGen->pEnd = pSplit;` |
|      16 |  4678 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4679 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4680 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4681 | `		pGen->pEnd = pTmp;` |
|      16 |  4682 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4683 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4684 | `		iP1 = 1;` |
|      16 |  4685 | `		iP2 = 1;` |
|       9 |  4686 | `	}else{` |
|       - |  4687 | `		/* yield $value */` |
|      63 |  4688 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      63 |  4689 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      63 |  4690 | `		if( rc != SXERR_EMPTY ){` |
|      63 |  4691 | `			iP1 = 1;` |
|      29 |  4692 | `		}` |
|       - |  4693 | `	}` |
|      77 |  4694 | `	pGen->pEnd = pTmp;` |
|      77 |  4695 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      77 |  4696 | `	return SXRET_OK;` |
|      41 |  4697 |  |
|       - |  4698 | `/*` |
|       - |  4699 | ` * Compile the die/exit language construct.` |
|       - |  4700 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4701 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4702 | ` */` |
|     120 |  4703 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4704 |  |
|     125 |  4705 | `	sxi32 nExpr = 0;` |
|       - |  4706 | `	sxi32 rc;` |
|       - |  4707 | `	/* Jump the die/exit keyword */` |
|     125 |  4708 | `	pGen->pIn++;` |
|     125 |  4709 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4710 | `		/* Compile the expression */` |
|     125 |  4711 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4712 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4713 | `			return SXERR_ABORT;` |
|     125 |  4714 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4715 | `			nExpr = 1;` |
|      60 |  4716 | `		}` |
|      60 |  4717 | `	}` |
|       - |  4718 | `	/* Emit the HALT instruction */` |
|     125 |  4719 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4720 | `	return SXRET_OK;` |
|      65 |  4721 |  |
|       - |  4722 | `/*` |
|       - |  4723 | ` * Compile the 'echo' language construct.` |
|       - |  4724 | ` */` |
|   13436 |  4725 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4726 |  |
|   13441 |  4727 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4728 | `	sxi32 rc;` |
|       - |  4729 | `	/* Jump the 'echo' keyword */` |
|   13441 |  4730 | `	pGen->pIn++;` |
|       - |  4731 | `	/* Compile arguments one after one */` |
|   13441 |  4732 | `	pTmp = pGen->pEnd;` |
|   29085 |  4733 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   15649 |  4734 | `		if( pGen->pIn < pNext ){` |
|   15649 |  4735 | `			pGen->pEnd = pNext;` |
|   15649 |  4736 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   15649 |  4737 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4738 | `				return SXERR_ABORT;` |
|   15649 |  4739 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4740 | `				/* Emit the consume instruction */` |
|   15625 |  4741 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    7810 |  4742 | `			}` |
|    7822 |  4743 | `		}` |
|       - |  4744 | `		/* Jump trailing commas */` |
|   17857 |  4745 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    2213 |  4746 | `			pNext++;` |
|       5 |  4747 | `		}` |
|   15649 |  4748 | `		pGen->pIn = pNext;` |
|       5 |  4749 | `	}` |
|       - |  4750 | `	/* Restore token stream */` |
|   13441 |  4751 | `	pGen->pEnd = pTmp;` |
|   13441 |  4752 | `	return SXRET_OK;` |
|    6723 |  4753 |  |
|       - |  4754 | `/*` |
|       - |  4755 | ` * Compile the static statement.` |
|       - |  4756 | ` * According to the PHP language reference` |
|       - |  4757 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4758 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4759 | ` *  when program execution leaves this scope.` |
|       - |  4760 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4761 | ` * Symisc eXtension.` |
|       - |  4762 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4763 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4764 | ` *  Example` |
|       - |  4765 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4766 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4767 | ` */` |
|       6 |  4768 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       2 |  4769 |  |
|       - |  4770 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4771 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4772 | `	GenBlock *pBlock;` |
|       - |  4773 | `	SyString *pName;` |
|       - |  4774 | `	char *zDup;` |
|       - |  4775 | `	sxu32 nLine;` |
|       - |  4776 | `	sxi32 rc;` |
|       - |  4777 | `	/* Jump the static keyword */` |
|       8 |  4778 | `	nLine = pGen->pIn->nLine;` |
|       8 |  4779 | `	pGen->pIn++;` |
|       - |  4780 | `	/* Extract the enclosing function if any */` |
|       8 |  4781 | `	pBlock = pGen->pCurrent;` |
|      14 |  4782 | `	while( pBlock ){` |
|      14 |  4783 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       8 |  4784 | `			break;` |
|       - |  4785 | `		}` |
|       - |  4786 | `		/* Point to the upper block */` |
|       8 |  4787 | `		pBlock = pBlock->pParent;` |
|       2 |  4788 | `	}` |
|       8 |  4789 | `	if( pBlock == 0 ){` |
|       - |  4790 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4791 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4792 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4793 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4794 | `				return SXERR_ABORT;` |
|       - |  4795 | `			}` |
|     ! 0 |  4796 | `			goto Synchronize;` |
|       - |  4797 | `		}` |
|       - |  4798 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4799 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4800 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4801 | `			return SXERR_ABORT;` |
|     ! 0 |  4802 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4803 | `			/* Emit the POP instruction */` |
|     ! 0 |  4804 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4805 | `		}` |
|     ! 0 |  4806 | `		return SXRET_OK;` |
|       - |  4807 | `	}` |
|       8 |  4808 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4809 | `	/* Make sure we are dealing with a valid statement */` |
|       8 |  4810 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       4 |  4811 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4812 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4813 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4814 | `				return SXERR_ABORT;` |
|       - |  4815 | `			}` |
|       3 |  4816 | `			goto Synchronize;` |
|       - |  4817 | `	}` |
|       5 |  4818 | `	pGen->pIn++;` |
|       - |  4819 | `	/* Extract variable name */` |
|       5 |  4820 | `	pName = &pGen->pIn->sData;` |
|       5 |  4821 | `	pGen->pIn++; /* Jump the var name */` |
|       5 |  4822 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4823 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4824 | `		goto Synchronize;` |
|       - |  4825 | `	}` |
|       - |  4826 | `	/* Initialize the structure describing the static variable */` |
|       5 |  4827 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       5 |  4828 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4829 | `	/* Duplicate variable name */` |
|       5 |  4830 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       5 |  4831 | `	if( zDup == 0 ){` |
|     ! 0 |  4832 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4833 | `		return SXERR_ABORT;` |
|       - |  4834 | `	}` |
|       5 |  4835 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4836 | `	/* Check if we have an expression to compile */` |
|       5 |  4837 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4838 | `		SySet *pInstrContainer;` |
|       - |  4839 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4840 | `		 * Static variable can take any complex expression including function` |
|       - |  4841 | `		 * call as their initialization value.` |
|       - |  4842 | `		 * Example:` |
|       - |  4843 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4844 | `		 */` |
|       5 |  4845 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4846 | `		/* Swap bytecode container */` |
|       5 |  4847 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       5 |  4848 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4849 | `		/* Compile the expression */` |
|       5 |  4850 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4851 | `		/* Emit the done instruction */` |
|       5 |  4852 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4853 | `		/* Restore default bytecode container */` |
|       5 |  4854 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       2 |  4855 | `	}` |
|       - |  4856 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       5 |  4857 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       5 |  4858 | `	return SXRET_OK;` |
|       1 |  4859 | `Synchronize:` |
|       - |  4860 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  4861 | `	 * statement.` |
|       - |  4862 | `	 */` |
|       5 |  4863 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  4864 | `		pGen->pIn++;` |
|       1 |  4865 | `	}` |
|       3 |  4866 | `	return SXRET_OK;` |
|       5 |  4867 |  |
|       - |  4868 | `/*` |
|       - |  4869 | ` * Compile the var statement.` |
|       - |  4870 | ` * Symisc Extension:` |
|       - |  4871 | ` *      var statement can be used outside of a class definition.` |
|       - |  4872 | ` */` |
|       4 |  4873 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  4874 |  |
|       - |  4875 | `	sxu32 nLine;` |
|       - |  4876 | `	sxi32 rc;` |
|       5 |  4877 | `	nLine = pGen->pIn->nLine;` |
|       - |  4878 | `	/* Jump the 'var' keyword */` |
|       5 |  4879 | `	pGen->pIn++;` |
|       5 |  4880 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  4881 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  4882 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  4883 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  4884 | `			pGen->pIn++;` |
|     ! 0 |  4885 | `		}` |
|     ! 0 |  4886 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4887 | `			return SXERR_ABORT;` |
|       - |  4888 | `		}` |
|     ! 0 |  4889 | `	}else{` |
|       - |  4890 | `		/* Compile the expression */` |
|       5 |  4891 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  4892 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4893 | `			return SXERR_ABORT;` |
|       5 |  4894 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  4895 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  4896 | `		}` |
|       - |  4897 | `	}` |
|       5 |  4898 | `	return SXRET_OK;` |
|       3 |  4899 |  |
|       - |  4900 | `/*` |
|       - |  4901 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  4902 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  4903 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  4904 | ` */` |
|       - |  4905 | `/*` |
|       - |  4906 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  4907 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  4908 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  4909 | ` * qualified name and updates the instruction's operand index.` |
|       - |  4910 | ` *` |
|       - |  4911 | ` * Resolution order:` |
|       - |  4912 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  4913 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  4914 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  4915 | ` *` |
|       - |  4916 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  4917 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  4918 | ` * Returns the (possibly new) literal index.` |
|       - |  4919 | ` */` |
|  392046 |  4920 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  4921 |  |
|       - |  4922 | `	ph7_value *pLit;` |
|       - |  4923 | `	const char *zLit;` |
|       - |  4924 | `	SyString sQualified;` |
|       - |  4925 | `	sxu32 nLit;` |
|       - |  4926 | `	sxu32 k;` |
|       - |  4927 | `	sxu32 nNewIdx;` |
|       - |  4928 | `	int hasNsSep;` |
|       - |  4929 | `	SyHashEntry *pImport;` |
|       - |  4930 | `	ph7_value *pNew;` |
|  392051 |  4931 | `	if( pFromImport ){` |
|  374791 |  4932 | `		*pFromImport = 0;` |
|  187393 |  4933 | `	}` |
|  392051 |  4934 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  392051 |  4935 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4936 | `		return nOrigIdx;` |
|       - |  4937 | `	}` |
|  392051 |  4938 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  392051 |  4939 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4940 | `	/* Skip if already qualified (contains backslash) */` |
|  392051 |  4941 | `	hasNsSep = 0;` |
| 4241933 |  4942 | `	for( k = 0; k < nLit; k++ ){` |
| 3849895 |  4943 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1924946 |  4944 | `	}` |
|  392051 |  4945 | `	if( hasNsSep ){` |
|      11 |  4946 | `		return nOrigIdx;` |
|       - |  4947 | `	}` |
|       - |  4948 | `	/* Check use imports first (works even outside namespaces) */` |
|  392043 |  4949 | `	SyBlobReset(&pGen->sWorker);` |
|  392043 |  4950 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  392043 |  4951 | `	if( pImport ){` |
|      41 |  4952 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  4953 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  4954 | `		if( pFromImport ){` |
|      18 |  4955 | `			*pFromImport = 1;` |
|       8 |  4956 | `		}` |
|      23 |  4957 | `	}else{` |
|  392007 |  4958 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  391917 |  4959 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  4960 | `		}` |
|       - |  4961 | `		/* Prepend current namespace */` |
|      95 |  4962 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  4963 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  4964 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  4965 | `	}` |
|       - |  4966 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  4967 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  4968 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  4969 | `		return nNewIdx; /* Already interned */` |
|       - |  4970 | `	}` |
|      79 |  4971 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  4972 | `	if( pNew == 0 ){` |
|     ! 0 |  4973 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  4974 | `	}` |
|      79 |  4975 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  4976 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  4977 | `	return nNewIdx;` |
|  196028 |  4978 |  |
|       - |  4979 | `/*` |
|       - |  4980 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4981 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4982 | ` */` |
|   86160 |  4983 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  4984 |  |
|       - |  4985 | `	SyHashEntry *pImport;` |
|       - |  4986 | `	/* Check use imports first */` |
|   86165 |  4987 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   86165 |  4988 | `	if( pImport ){` |
|      15 |  4989 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 |  4990 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      15 |  4991 | `		return;` |
|       - |  4992 | `	}` |
|       - |  4993 | `	/* Prepend current namespace if active */` |
|   86153 |  4994 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4995 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4996 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4997 | `	}` |
|   86153 |  4998 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   43085 |  4999 |  |
|       - |  5000 | `/*` |
|       - |  5001 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5002 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5003 | ` * The caller must release pOut when done.` |
|       - |  5004 | ` */` |
|  121324 |  5005 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5006 |  |
|  121329 |  5007 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5008 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5009 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5010 | `	}` |
|  121329 |  5011 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  121329 |  5012 |  |
|       - |  5013 | `/*` |
|       - |  5014 | ` * Compile a namespace statement` |
|       - |  5015 | ` * According to the PHP language reference manual` |
|       - |  5016 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5017 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5018 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5019 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5020 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5021 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5022 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5023 | ` *  programming world.` |
|       - |  5024 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5025 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5026 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5027 | ` *  classes/functions/constants.` |
|       - |  5028 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5029 | ` *  readability of source code.` |
|       - |  5030 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5031 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5032 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5033 | ` *       class MyClass {}` |
|       - |  5034 | ` *       function myfunction() {}` |
|       - |  5035 | ` *       const MYCONST = 1;` |
|       - |  5036 | ` *       $a = new MyClass;` |
|       - |  5037 | ` *       $c = new \my\name\MyClass;` |
|       - |  5038 | ` *       $a = strlen('hi');` |
|       - |  5039 | ` *       $d = namespace\MYCONST;` |
|       - |  5040 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5041 | ` *       echo constant($d);` |
|       - |  5042 | ` * NOTE` |
|       - |  5043 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5044 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5045 | ` */` |
|       - |  5046 | `/*` |
|       - |  5047 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5048 | ` */` |
|      14 |  5049 | `static const char * TokenTypeName(sxu32 nType)` |
|       4 |  5050 |  |
|      18 |  5051 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 |  5052 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 |  5053 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 |  5054 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 |  5055 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 |  5056 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5057 | `	return "token";` |
|      11 |  5058 |  |
|     106 |  5059 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5060 |  |
|       - |  5061 | `	sxu32 nLine;` |
|       - |  5062 | `	sxi32 rc;` |
|     111 |  5063 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5064 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5065 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5066 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5067 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5068 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5069 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5070 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5071 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5072 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5073 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5074 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5075 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5076 | `		return SXRET_OK;` |
|       - |  5077 | `	}` |
|     111 |  5078 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5079 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5080 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5081 | `		return SXRET_OK;` |
|       - |  5082 | `	}` |
|     111 |  5083 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5084 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5085 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5086 | `		return SXRET_OK;` |
|       - |  5087 | `	}` |
|       - |  5088 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5089 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5090 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5091 | `			/* Append backslash separator */` |
|      27 |  5092 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5093 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5094 | `			}` |
|      16 |  5095 | `		}else{` |
|       - |  5096 | `			/* Append identifier */` |
|     131 |  5097 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5098 | `		}` |
|     153 |  5099 | `		pGen->pIn++;` |
|       5 |  5100 | `	}` |
|       - |  5101 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5102 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5103 | `	{` |
|     111 |  5104 | `		char *zNsDup = 0;` |
|     111 |  5105 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5106 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5107 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5108 | `		}` |
|     111 |  5109 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5110 | `	}` |
|     111 |  5111 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5112 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5113 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5114 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5115 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5116 | `			return SXERR_ABORT;` |
|       - |  5117 | `		}` |
|       2 |  5118 | `	}` |
|     111 |  5119 | `	return SXRET_OK;` |
|      58 |  5120 |  |
|       - |  5121 | `/*` |
|       - |  5122 | ` * Compile the 'use' statement` |
|       - |  5123 | ` * According to the PHP language reference manual` |
|       - |  5124 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5125 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5126 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5127 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5128 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5129 | ` *  a function or constant is not supported.` |
|       - |  5130 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5131 | ` * NOTE` |
|       - |  5132 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5133 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5134 | ` */` |
|      68 |  5135 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5136 |  |
|       - |  5137 | `	sxu32 nLine;` |
|       - |  5138 | `	sxi32 rc;` |
|       - |  5139 | `	SyBlob sPath;` |
|       - |  5140 | `	SyString sAlias;` |
|       - |  5141 | `	SyToken *pLast;` |
|       - |  5142 | `	char *zDup;` |
|       - |  5143 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5144 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5145 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5146 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5147 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5148 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5149 | `	iUseType = 0;` |
|      73 |  5150 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5151 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5152 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5153 | `			iUseType = 1;` |
|      16 |  5154 | `			pGen->pIn++;` |
|      23 |  5155 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5156 | `			iUseType = 2;` |
|      16 |  5157 | `			pGen->pIn++;` |
|       7 |  5158 | `		}` |
|      14 |  5159 | `	}` |
|       - |  5160 | `	/* Select target hash tables based on import type */` |
|      73 |  5161 | `	switch( iUseType ){` |
|       7 |  5162 | `		case 1:` |
|      16 |  5163 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5164 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5165 | `			break;` |
|       7 |  5166 | `		case 2:` |
|      16 |  5167 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5168 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5169 | `			break;` |
|      20 |  5170 | `		default:` |
|      45 |  5171 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5172 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5173 | `			break;` |
|       - |  5174 | `	}` |
|      73 |  5175 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5176 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5177 | `	for(;;){` |
|      75 |  5178 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5179 | `			break;` |
|       - |  5180 | `		}` |
|      75 |  5181 | `		SyBlobReset(&sPath);` |
|      75 |  5182 | `		pLast = 0;` |
|       - |  5183 | `		/* Collect the full namespace path */` |
|     261 |  5184 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5185 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5186 | `				pLast = pGen->pIn;` |
|     131 |  5187 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5188 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5189 | `				}` |
|     131 |  5190 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5191 | `			}` |
|     191 |  5192 | `			pGen->pIn++;` |
|       5 |  5193 | `		}` |
|      75 |  5194 | `		if( pLast == 0 ){` |
|       - |  5195 | `			/* Empty path */` |
|       6 |  5196 | `			break;` |
|       - |  5197 | `		}` |
|       - |  5198 | `		/* Default alias is the last component of the path */` |
|      71 |  5199 | `		sAlias = pLast->sData;` |
|       - |  5200 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5201 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5202 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      19 |  5203 | `			pGen->pIn++; /* Jump 'as' */` |
|      19 |  5204 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      19 |  5205 | `				sAlias = pGen->pIn->sData;` |
|      19 |  5206 | `				pGen->pIn++;` |
|       8 |  5207 | `			}` |
|       8 |  5208 | `		}` |
|       - |  5209 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5210 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5211 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5212 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5213 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5214 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5215 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5216 | `				return SXERR_ABORT;` |
|       - |  5217 | `			}` |
|       2 |  5218 | `		}` |
|       - |  5219 | `		/* Register the import: alias -> FQN.` |
|       - |  5220 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5221 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5222 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5223 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5224 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5225 | `		if( zDup ){` |
|      71 |  5226 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5227 | `			if( pVmHash ){` |
|       - |  5228 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5229 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5230 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5231 | `				if( zAliasDup ){` |
|      43 |  5232 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5233 | `				}` |
|      19 |  5234 | `			}` |
|      71 |  5235 | `			if( iUseType == 2 ){` |
|       - |  5236 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5237 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5238 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5239 | `				if( zAliasDup ){` |
|       - |  5240 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5241 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5242 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5243 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5244 | `					if( azPair ){` |
|      16 |  5245 | `						azPair[0] = zAliasDup;` |
|      16 |  5246 | `						azPair[1] = zDup;` |
|      16 |  5247 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5248 | `					}` |
|       7 |  5249 | `				}` |
|       7 |  5250 | `			}` |
|      33 |  5251 | `		}` |
|       - |  5252 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5253 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5254 | `			pGen->pIn++;` |
|       2 |  5255 | `		}else{` |
|      37 |  5256 | `			break;` |
|       - |  5257 | `		}` |
|       1 |  5258 | `	}` |
|      73 |  5259 | `	SyBlobRelease(&sPath);` |
|      73 |  5260 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5261 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5262 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5263 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5264 | `			return SXERR_ABORT;` |
|       - |  5265 | `		}` |
|       1 |  5266 | `	}` |
|      73 |  5267 | `	return SXRET_OK;` |
|      39 |  5268 |  |
|       - |  5269 | `/*` |
|       - |  5270 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5271 | ` *` |
|       - |  5272 | ` * According to the PHP language reference manual.` |
|       - |  5273 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5274 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5275 | ` *  declare (directive)` |
|       - |  5276 | ` *   statement` |
|       - |  5277 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5278 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5279 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5280 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5281 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5282 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5283 | ` * <?php` |
|       - |  5284 | ` * // these are the same:` |
|       - |  5285 | ` * // you can use this:` |
|       - |  5286 | ` * declare(ticks=1) {` |
|       - |  5287 | ` *   // entire script here` |
|       - |  5288 | ` * }` |
|       - |  5289 | ` * // or you can use this:` |
|       - |  5290 | ` * declare(ticks=1);` |
|       - |  5291 | ` * // entire script here` |
|       - |  5292 | ` * ?>` |
|       - |  5293 | ` *` |
|       - |  5294 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5295 | ` */` |
|       - |  5296 | `/*` |
|       - |  5297 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5298 | ` */` |
|      68 |  5299 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5300 |  |
|     103 |  5301 | `	return SyStringLength(pName) == nWant` |
|      68 |  5302 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5303 |  |
|       - |  5304 |  |
|      40 |  5305 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5306 |  |
|      45 |  5307 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5308 | `	SyToken *pBodyEnd = 0;` |
|       - |  5309 | `	SyToken *pBodyStart;` |
|       - |  5310 | `	SyToken *pCursor;` |
|       - |  5311 | `	int bHasStrictTypes;` |
|       - |  5312 | `	int bBlockForm;` |
|       - |  5313 | `	int bPlacementOk;` |
|       - |  5314 | `	sxi32 rc;` |
|      45 |  5315 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5316 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5317 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5318 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5319 | `			return SXERR_ABORT;` |
|       - |  5320 | `		}` |
|       6 |  5321 | `		goto Synchro;` |
|       - |  5322 | `	}` |
|      41 |  5323 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5324 | `	pBodyStart = pGen->pIn;` |
|       - |  5325 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5326 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5327 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5328 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5329 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5330 | `			return SXERR_ABORT;` |
|       - |  5331 | `		}` |
|     ! 0 |  5332 | `		return SXRET_OK;` |
|       - |  5333 | `	}` |
|       - |  5334 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5335 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5336 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5337 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5338 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5339 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5340 | `			return SXERR_ABORT;` |
|       - |  5341 | `		}` |
|     ! 0 |  5342 | `	}` |
|      41 |  5343 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5344 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5345 | `	bHasStrictTypes = 0;` |
|       - |  5346 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5347 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5348 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5349 | `	pCursor = pBodyStart;` |
|      53 |  5350 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5351 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5352 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5353 | `				bHasStrictTypes = 1;` |
|      37 |  5354 | `				break;` |
|       - |  5355 | `			}` |
|       2 |  5356 | `		}` |
|      14 |  5357 | `		pCursor++;` |
|       2 |  5358 | `	}` |
|      41 |  5359 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5360 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5361 | `			"strict_types declaration must not use block mode");` |
|       3 |  5362 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5363 | `		return SXRET_OK;` |
|       - |  5364 | `	}` |
|      39 |  5365 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5366 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5367 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5368 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5369 | `		return SXRET_OK;` |
|       - |  5370 | `	}` |
|       - |  5371 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5372 | `	pCursor = pBodyStart;` |
|      65 |  5373 | `	while( pCursor < pBodyEnd ){` |
|       - |  5374 | `		SyToken *pNameTok;` |
|       - |  5375 | `		SyToken *pEqTok;` |
|       - |  5376 | `		SyToken *pValTok;` |
|       - |  5377 | `		SyString *pDirName;` |
|       - |  5378 | `		int bIsStrict;` |
|       - |  5379 | `		int iStrictValue;` |
|      37 |  5380 | `		pNameTok = pCursor;` |
|      37 |  5381 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5382 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5383 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5384 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5385 | `			return SXRET_OK;` |
|       - |  5386 | `		}` |
|      37 |  5387 | `		pEqTok = pNameTok + 1;` |
|      37 |  5388 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5389 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5390 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5391 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5392 | `			return SXRET_OK;` |
|       - |  5393 | `		}` |
|      37 |  5394 | `		pValTok = pEqTok + 1;` |
|      37 |  5395 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5396 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5397 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5398 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5399 | `			return SXRET_OK;` |
|       - |  5400 | `		}` |
|      37 |  5401 | `		pDirName = &pNameTok->sData;` |
|      37 |  5402 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5403 | `		if( bIsStrict ){` |
|       - |  5404 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5405 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5406 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5407 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5408 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5409 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5410 | `				return SXRET_OK;` |
|       - |  5411 | `			}` |
|      33 |  5412 | `			iStrictValue = -1;` |
|      33 |  5413 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5414 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5415 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5416 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5417 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5418 | `			}` |
|      33 |  5419 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5420 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5421 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5422 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5423 | `				return SXRET_OK;` |
|       - |  5424 | `			}` |
|      30 |  5425 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5426 | `		}else{` |
|       - |  5427 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5428 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5429 | `			 * behavior don't regress. */` |
|       8 |  5430 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5431 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5432 | `				ph7_lib_version()` |
|       - |  5433 | `				);` |
|       - |  5434 | `		}` |
|      35 |  5435 | `		pCursor = pValTok + 1;` |
|       - |  5436 | `		/* Consume separating comma (or end). */` |
|      35 |  5437 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5438 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5439 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5440 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5441 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5442 | `				return SXRET_OK;` |
|       - |  5443 | `			}` |
|       3 |  5444 | `			pCursor++;` |
|       1 |  5445 | `		}` |
|       5 |  5446 | `	}` |
|       - |  5447 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5448 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5449 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5450 | `	return SXRET_OK;` |
|       2 |  5451 | `Synchro:` |
|       - |  5452 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5453 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5454 | `		pGen->pIn++;` |
|       2 |  5455 | `	}` |
|       6 |  5456 | `	return SXRET_OK;` |
|      25 |  5457 |  |
|       - |  5458 | `/*` |
|       - |  5459 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5460 | ` * as follows:` |
|       - |  5461 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5462 | ` * {` |
|       - |  5463 | ` *   return "Making a cup of $type.\n";` |
|       - |  5464 | ` * }` |
|       - |  5465 | ` * Symisc eXtension.` |
|       - |  5466 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5467 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5468 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5469 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5470 | ` *      {` |
|       - |  5471 | ` *       var_dump($a);` |
|       - |  5472 | ` *      }` |
|       - |  5473 | ` *     //call test without args` |
|       - |  5474 | ` *      test();` |
|       - |  5475 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5476 | ` *      Example:` |
|       - |  5477 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5478 | ` * 3 -) Function overloading!!` |
|       - |  5479 | ` *      Example:` |
|       - |  5480 | ` *      function foo($a) {` |
|       - |  5481 | ` *   	  return $a.PHP_EOL;` |
|       - |  5482 | ` *	    }` |
|       - |  5483 | ` *	    function foo($a, $b) {` |
|       - |  5484 | ` *   	  return $a + $b;` |
|       - |  5485 | ` *	    }` |
|       - |  5486 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5487 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5488 | ` *      // Same arg` |
|       - |  5489 | ` *	   function foo(string $a)` |
|       - |  5490 | ` *	   {` |
|       - |  5491 | ` *	     echo "a is a string\n";` |
|       - |  5492 | ` *	     var_dump($a);` |
|       - |  5493 | ` *	   }` |
|       - |  5494 | ` *	  function foo(int $a)` |
|       - |  5495 | ` *	  {` |
|       - |  5496 | ` *	    echo "a is integer\n";` |
|       - |  5497 | ` *	    var_dump($a);` |
|       - |  5498 | ` *	  }` |
|       - |  5499 | ` *	  function foo(array $a)` |
|       - |  5500 | ` *	  {` |
|       - |  5501 | ` * 	    echo "a is an array\n";` |
|       - |  5502 | ` * 	    var_dump($a);` |
|       - |  5503 | ` *	  }` |
|       - |  5504 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5505 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5506 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5507 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5508 | ` * introduced by the PH7 engine.` |
|       - |  5509 | ` */` |
|   60150 |  5510 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5511 |  |
|       - |  5512 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5513 | `	SySet *pInstrContainer;` |
|       - |  5514 | `	sxi32 rc;` |
|       - |  5515 | `	/* Swap token stream */` |
|   60155 |  5516 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   60155 |  5517 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   60155 |  5518 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5519 | `	/* Compile the expression holding the argument value */` |
|   60155 |  5520 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5521 | `	/* Emit the done instruction */` |
|   60155 |  5522 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   60155 |  5523 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   60155 |  5524 | `	RE_SWAP_DELIMITER(pGen);` |
|   60155 |  5525 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5526 | `		return SXERR_ABORT;` |
|       - |  5527 | `	}` |
|   60155 |  5528 | `	return SXRET_OK;` |
|   30080 |  5529 |  |
|       - |  5530 | `/*` |
|       - |  5531 | ` * Collect function arguments one after one.` |
|       - |  5532 | ` * According to the PHP language reference manual.` |
|       - |  5533 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5534 | ` * list of expressions.` |
|       - |  5535 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5536 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5537 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5538 | ` * for more information.` |
|       - |  5539 | ` * Example #1 Passing arrays to functions` |
|       - |  5540 | ` * <?php` |
|       - |  5541 | ` * function takes_array($input)` |
|       - |  5542 | ` * {` |
|       - |  5543 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5544 | ` * }` |
|       - |  5545 | ` * ?>` |
|       - |  5546 | ` * Making arguments be passed by reference` |
|       - |  5547 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5548 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5549 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5550 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5551 | ` * to the argument name in the function definition:` |
|       - |  5552 | ` * Example #2 Passing function parameters by reference` |
|       - |  5553 | ` * <?php` |
|       - |  5554 | ` * function add_some_extra(&$string)` |
|       - |  5555 | ` * {` |
|       - |  5556 | ` *   $string .= 'and something extra.';` |
|       - |  5557 | ` * }` |
|       - |  5558 | ` * $str = 'This is a string, ';` |
|       - |  5559 | ` * add_some_extra($str);` |
|       - |  5560 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5561 | ` * ?>` |
|       - |  5562 | ` *` |
|       - |  5563 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5564 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5565 | ` * on these extension.` |
|       - |  5566 | ` */` |
|   83418 |  5567 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5568 |  |
|       - |  5569 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5570 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5571 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5572 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5573 | `	sxi32 rc;` |
|       - |  5574 |  |
|   83423 |  5575 | `	pIn = pGen->pIn;` |
|   83423 |  5576 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5577 | `	/* Process arguments one after one */` |
|  104334 |  5578 | `	for(;;){` |
|  208673 |  5579 | `		if( pIn >= pEnd ){` |
|       - |  5580 | `			/* No more arguments to process */` |
|   83411 |  5581 | `			break;` |
|       - |  5582 | `		}` |
|  125267 |  5583 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  125267 |  5584 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  125267 |  5585 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  125267 |  5586 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5587 | `		/* Parse optional visibility modifier (constructor property promotion, PHP 8.0+) */` |
|  125267 |  5588 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   57115 |  5589 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   57115 |  5590 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      47 |  5591 | `				if( !bCtorCtx ){` |
|       6 |  5592 | `					if( bAbstractCtx ){` |
|       3 |  5593 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5594 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5595 | `					}else{` |
|       3 |  5596 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5597 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5598 | `					}` |
|       6 |  5599 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5600 | `						return SXERR_ABORT;` |
|       - |  5601 | `					}` |
|       6 |  5602 | `					return SXERR_SYNTAX;` |
|       - |  5603 | `				}` |
|      43 |  5604 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      43 |  5605 | `				if( nKw == PH7_TKWRD_PRIVATE ){` |
|       3 |  5606 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PRIVATE;` |
|      42 |  5607 | `				}else if( nKw == PH7_TKWRD_PROTECTED ){` |
|       3 |  5608 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PROTECTED;` |
|       2 |  5609 | `				}else{` |
|      39 |  5610 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PUBLIC;` |
|       - |  5611 | `				}` |
|      43 |  5612 | `				pIn++;` |
|      19 |  5613 | `			}` |
|   28553 |  5614 | `		}` |
|       - |  5615 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  158592 |  5616 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   97558 |  5617 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   68268 |  5618 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   66651 |  5619 | `			sxu32 nLineLocal = pIn->nLine;` |
|   66651 |  5620 | `			sxi32 iTFlags = 0;` |
|   66651 |  5621 | `			pGen->pIn = pIn;` |
|   66651 |  5622 | `			rc = GenStateParseUnionTypeDecl(` |
|   33323 |  5623 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   33323 |  5624 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5625 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5626 | `				/* bAllowVoid */ 0,` |
|   33323 |  5627 | `						nLineLocal);` |
|   66651 |  5628 | `			pIn = pGen->pIn;` |
|   66651 |  5629 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5630 | `				return SXERR_ABORT;` |
|   66651 |  5631 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5632 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5633 | `				return SXERR_SYNTAX;` |
|   66649 |  5634 | `			}else if( rc == SXERR_SYNTAX ){` |
|       6 |  5635 | `				if( pIn < pEnd ){` |
|       8 |  5636 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5637 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5638 | `						&pIn->sData);` |
|       4 |  5639 | `				}else{` |
|     ! 0 |  5640 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5641 | `						"syntax error, unexpected end of file");` |
|       - |  5642 | `				}` |
|       6 |  5643 | `				return SXERR_SYNTAX;` |
|       - |  5644 | `			}` |
|   66645 |  5645 | `			sArg.iFlags \|= iTFlags;` |
|   33320 |  5646 | `		}` |
|  125257 |  5647 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5648 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5649 | `			return rc;` |
|       - |  5650 | `		}` |
|  125257 |  5651 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5652 | `			/* Pass by reference,record that */` |
|    3197 |  5653 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3197 |  5654 | `			pIn++;` |
|    1596 |  5655 | `		}` |
|  125257 |  5656 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5657 | `			/* Variadic parameter: ...$args */` |
|      47 |  5658 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      47 |  5659 | `			pIn++;` |
|      21 |  5660 | `		}` |
|  125257 |  5661 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5662 | `			/* Invalid argument */` |
|     ! 0 |  5663 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5664 | `			return rc;` |
|       - |  5665 | `		}` |
|  125257 |  5666 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5667 | `		/* Copy argument name */` |
|  125257 |  5668 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  125257 |  5669 | `		if( zDup == 0 ){` |
|     ! 0 |  5670 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5671 | `			return SXERR_ABORT;` |
|       - |  5672 | `		}` |
|  125257 |  5673 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  125257 |  5674 | `		pIn++;` |
|  125257 |  5675 | `		if( pIn < pEnd ){` |
|   70357 |  5676 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5677 | `				SyToken *pDefend;` |
|   60157 |  5678 | `				sxi32 iNest = 0;` |
|   60157 |  5679 | `				pIn++; /* Jump the equal sign */` |
|   60157 |  5680 | `				pDefend = pIn;` |
|       - |  5681 | `				/* Process the default value associated with this argument */` |
|  126635 |  5682 | `				while( pDefend < pEnd ){` |
|   98131 |  5683 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   31653 |  5684 | `						break;` |
|       - |  5685 | `					}` |
|   66483 |  5686 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5687 | `						/* Increment nesting level */` |
|    3169 |  5688 | `						iNest++;` |
|   64901 |  5689 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5690 | `						/* Decrement nesting level */` |
|    3169 |  5691 | `						iNest--;` |
|    1582 |  5692 | `					}` |
|   66483 |  5693 | `					pDefend++;` |
|       5 |  5694 | `				}` |
|   60157 |  5695 | `				if( pIn >= pDefend ){` |
|       3 |  5696 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5697 | `					return rc;` |
|       - |  5698 | `				}` |
|       - |  5699 | `				/* Process default value */` |
|   60155 |  5700 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   60155 |  5701 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5702 | `					return rc;` |
|       - |  5703 | `				}` |
|       - |  5704 | `				/* Point beyond the default value */` |
|   60155 |  5705 | `				pIn = pDefend;` |
|   30075 |  5706 | `			}` |
|   70355 |  5707 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5708 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5709 | `				return rc;` |
|       - |  5710 | `			}` |
|   70355 |  5711 | `			pIn++; /* Jump the trailing comma */` |
|   35175 |  5712 | `		}` |
|       - |  5713 | `		/* Append argument signature */` |
|  125255 |  5714 | `		if( sArg.nType > 0 ){` |
|   66601 |  5715 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5716 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    9519 |  5717 | `				int marker = 'o';` |
|    9519 |  5718 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    9519 |  5719 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    4762 |  5720 | `			}else{` |
|       - |  5721 | `				int c;` |
|   57087 |  5722 | `				c = 'n'; /* cc warning */` |
|       - |  5723 | `				/* Type leading character */` |
|   57087 |  5724 | `				switch(sArg.nType){` |
|     ! 0 |  5725 | `				case MEMOBJ_HASHMAP:` |
|       - |  5726 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5727 | `					c = 'h';` |
|     ! 0 |  5728 | `					break;` |
|    7946 |  5729 | `				case MEMOBJ_INT:` |
|       - |  5730 | `					/* Integer */` |
|   15897 |  5731 | `					c = 'i';` |
|   15897 |  5732 | `					break;` |
|       1 |  5733 | `				case MEMOBJ_BOOL:` |
|       - |  5734 | `					/* Bool */` |
|       3 |  5735 | `					c = 'b';` |
|       3 |  5736 | `					break;` |
|       1 |  5737 | `				case MEMOBJ_REAL:` |
|       - |  5738 | `					/* Float */` |
|       3 |  5739 | `					c = 'f';` |
|       3 |  5740 | `					break;` |
|   20585 |  5741 | `				case MEMOBJ_STRING:` |
|       - |  5742 | `					/* String */` |
|   41175 |  5743 | `					c = 's';` |
|   41175 |  5744 | `					break;` |
|       7 |  5745 | `				case MEMOBJ_OBJ:` |
|       - |  5746 | `					/* Object */` |
|      16 |  5747 | `					c = 'o';` |
|      14 |  5748 | `					break;` |
|       1 |  5749 | `				default:` |
|       2 |  5750 | `					break;` |
|       - |  5751 | `				}` |
|   57087 |  5752 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5753 | `			}` |
|   33303 |  5754 | `		}else{` |
|       - |  5755 | `			/* No type is associated with this parameter which mean` |
|       - |  5756 | `			 * that this function is not condidate for overloading.` |
|       - |  5757 | `			 */` |
|   58659 |  5758 | `			SyBlobRelease(&sSig);` |
|       - |  5759 | `		}` |
|       - |  5760 | `		/* Save in the argument set */` |
|  125255 |  5761 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5762 | `	}` |
|   83411 |  5763 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5764 | `		/* Save function signature */` |
|   41267 |  5765 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   20631 |  5766 | `	}` |
|   83411 |  5767 | `	return SXRET_OK;` |
|   41714 |  5768 |  |
|       - |  5769 | `/*` |
|       - |  5770 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5771 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5772 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5773 | ` */` |
|  198044 |  5774 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5775 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5776 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5777 | `	)` |
|       5 |  5778 |  |
|       - |  5779 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5780 | `	GenBlock *pBlock;` |
|       - |  5781 | `	sxu32 nGotoOfft;` |
|       - |  5782 | `	sxi32 rc;` |
|       - |  5783 | `	/* Attach the new function */` |
|  198049 |  5784 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  198049 |  5785 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5786 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5787 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5788 | `		return SXERR_ABORT;` |
|       - |  5789 | `	}` |
|  198049 |  5790 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5791 | `	/* Swap bytecode containers */` |
|  198049 |  5792 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  198049 |  5793 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5794 | `	/* Emit constructor property promotion prologue:` |
|       - |  5795 | `	 *   $this->NAME = $NAME;` |
|       - |  5796 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5797 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5798 | `	{` |
|  198049 |  5799 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5800 | `		sxu32 i;` |
|  297865 |  5801 | `		for( i = 0; i < nArg; i++ ){` |
|   99821 |  5802 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5803 | `			char *zSrc;` |
|       - |  5804 | `			sxu32 nSrc,nName;` |
|       - |  5805 | `			SySet sToken;` |
|       - |  5806 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5807 | `			sxi32 rcPromote;` |
|   99821 |  5808 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   99791 |  5809 | `				continue;` |
|       - |  5810 | `			}` |
|       - |  5811 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5812 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5813 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5814 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5815 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      33 |  5816 | `			nName = SyStringLength(&pArg->sName);` |
|      33 |  5817 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      33 |  5818 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      33 |  5819 | `			if( zSrc == 0 ){` |
|     ! 0 |  5820 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5821 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5822 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5823 | `				return SXERR_ABORT;` |
|       - |  5824 | `			}` |
|       - |  5825 | `			{` |
|      33 |  5826 | `				char *z = zSrc;` |
|      33 |  5827 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      33 |  5828 | `				z += sizeof("$this->")-1;` |
|      33 |  5829 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      33 |  5830 | `				z += nName;` |
|      33 |  5831 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      33 |  5832 | `				z += sizeof(" = $")-1;` |
|      33 |  5833 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      33 |  5834 | `				z += nName;` |
|      33 |  5835 | `				*z = 0;` |
|       - |  5836 | `			}` |
|      33 |  5837 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      33 |  5838 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      33 |  5839 | `			pTmpIn = pGen->pIn;` |
|      33 |  5840 | `			pTmpEnd = pGen->pEnd;` |
|      33 |  5841 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      33 |  5842 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      33 |  5843 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 |  5844 | `			pGen->pIn = pTmpIn;` |
|      33 |  5845 | `			pGen->pEnd = pTmpEnd;` |
|      33 |  5846 | `			SySetRelease(&sToken);` |
|      33 |  5847 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  5848 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5849 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5850 | `				return SXERR_ABORT;` |
|       - |  5851 | `			}` |
|       - |  5852 | `			/* Discard the assignment result — this is a statement expression. */` |
|      33 |  5853 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      18 |  5854 | `		}` |
|       - |  5855 | `	}` |
|       - |  5856 | `	/* Compile the body */` |
|  198049 |  5857 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5858 | `	/* Fix exception jumps now the destination is resolved */` |
|  198049 |  5859 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5860 | `	/* Emit the final return if not yet done */` |
|  198049 |  5861 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5862 | `	/* Fix gotos jumps now the destination is resolved */` |
|  198049 |  5863 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5864 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5865 | `	}` |
|  198049 |  5866 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5867 | `	/* Restore the default container */` |
|  198049 |  5868 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5869 | `	/* Leave function block */` |
|  198049 |  5870 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  198049 |  5871 | `	if( rc == SXERR_ABORT ){` |
|       - |  5872 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5873 | `		return SXERR_ABORT;` |
|       - |  5874 | `	}` |
|       - |  5875 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5876 | `	{` |
|  198049 |  5877 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5878 | `		sxu32 i;` |
| 3867827 |  5879 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 3669819 |  5880 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      41 |  5881 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      41 |  5882 | `				break;` |
|       - |  5883 | `			}` |
| 1834894 |  5884 | `		}` |
|       - |  5885 | `	}` |
|       - |  5886 | `	/* All done, function body compiled */` |
|  198049 |  5887 | `	return SXRET_OK;` |
|   99027 |  5888 |  |
|       - |  5889 | `/*` |
|       - |  5890 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5891 | ` * According to the PHP language reference manual.` |
|       - |  5892 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5893 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5894 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5895 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5896 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5897 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5898 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5899 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5900 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5901 | ` *` |
|       - |  5902 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5903 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5904 | ` * on these extension.` |
|       - |  5905 | ` */` |
|       - |  5906 | `/*` |
|       - |  5907 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5908 | ` */` |
|     322 |  5909 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  5910 |  |
|       - |  5911 | `	sxu32 i;` |
|     911 |  5912 | `	for( i = 0; i < n; i++ ){` |
|     781 |  5913 | `		int a = zA[i], b = zB[i];` |
|     781 |  5914 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     781 |  5915 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     781 |  5916 | `		if( a != b ) return a - b;` |
|     297 |  5917 | `	}` |
|     135 |  5918 | `	return 0;` |
|     166 |  5919 |  |
|       - |  5920 | `/*` |
|       - |  5921 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5922 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5923 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5924 | ` */` |
|       - |  5925 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5926 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5927 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5928 |  |
|       - |  5929 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5930 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5931 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5932 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5933 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5934 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5935 |  |
|       - |  5936 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5937 | `struct PhlTypeAtom {` |
|       - |  5938 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5939 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5940 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5941 | `	sxu32 nCanon;` |
|       - |  5942 | `};` |
|       - |  5943 |  |
|       - |  5944 | `/*` |
|       - |  5945 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5946 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5947 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5948 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5949 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5950 | ` * already be consumed by the caller.` |
|       - |  5951 | ` */` |
|   67244 |  5952 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  5953 |  |
|   67249 |  5954 | `	SyToken *pIn = pGen->pIn;` |
|   67249 |  5955 | `	SyZero(pOut, sizeof(*pOut));` |
|   67249 |  5956 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   67249 |  5957 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5958 | `		return SXERR_SYNTAX;` |
|       - |  5959 | `	}` |
|       - |  5960 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   67249 |  5961 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5962 | `		pIn++;` |
|       8 |  5963 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5964 | `			return SXERR_SYNTAX;` |
|       - |  5965 | `		}` |
|       3 |  5966 | `	}` |
|   67249 |  5967 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5968 | `		return SXERR_SYNTAX;` |
|       - |  5969 | `	}` |
|   67249 |  5970 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   57515 |  5971 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   57515 |  5972 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      20 |  5973 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   57507 |  5974 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      61 |  5975 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   57471 |  5976 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   16087 |  5977 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   49402 |  5978 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   41305 |  5979 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   20711 |  5980 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      30 |  5981 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      47 |  5982 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  5983 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      21 |  5984 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       5 |  5985 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  5986 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  5987 | `			pOut->sClass = pIn->sData;` |
|       4 |  5988 | `		}else{` |
|       3 |  5989 | `			return SXERR_SYNTAX;` |
|       - |  5990 | `		}` |
|   57513 |  5991 | `		pIn++;` |
|   28759 |  5992 | `	}else{` |
|       - |  5993 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  5994 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    9739 |  5995 | `		SyString *pT = &pIn->sData;` |
|    9739 |  5996 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      18 |  5997 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      18 |  5998 | `			pIn++;` |
|    9731 |  5999 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     111 |  6000 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     111 |  6001 | `			pIn++;` |
|    9670 |  6002 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  6003 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  6004 | `			pIn++;` |
|       2 |  6005 | `		}else{` |
|       - |  6006 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    9615 |  6007 | `			SyToken *pFirst = pIn;` |
|    9615 |  6008 | `			SyToken *pLast = pIn;` |
|    9615 |  6009 | `			pOut->nType = SXU32_HIGH;` |
|    9615 |  6010 | `			pOut->sClass = pIn->sData;` |
|    9615 |  6011 | `			pIn++;` |
|   14418 |  6012 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    9618 |  6013 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6014 | `				pLast = &pIn[1];` |
|       3 |  6015 | `				pIn += 2;` |
|       1 |  6016 | `			}` |
|    9615 |  6017 | `			if( pLast != pFirst ){` |
|       3 |  6018 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6019 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6020 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6021 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6022 | `			}` |
|       - |  6023 | `		}` |
|       - |  6024 | `	}` |
|   67247 |  6025 | `	pGen->pIn = pIn;` |
|   67247 |  6026 | `	return SXRET_OK;` |
|   33627 |  6027 |  |
|       - |  6028 |  |
|       - |  6029 | `/*` |
|       - |  6030 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6031 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6032 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6033 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6034 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6035 | ` */` |
|   67140 |  6036 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6037 |  |
|       - |  6038 | `	int i;` |
|   67145 |  6039 | `	int nNonNull = 0;` |
|  134373 |  6040 | `	for( i = 0; i < nAtoms; i++ ){` |
|   67233 |  6041 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   67217 |  6042 | `			nNonNull++;` |
|   33606 |  6043 | `		}` |
|   33619 |  6044 | `	}` |
|   67145 |  6045 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6046 | `		/* Shorthand: ?T */` |
|      64 |  6047 | `		for( i = 0; i < nAtoms; i++ ){` |
|      64 |  6048 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      64 |  6049 | `			SyBlobAppend(pBlob, "?", 1);` |
|      64 |  6050 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      15 |  6051 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       9 |  6052 | `			}else{` |
|      51 |  6053 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6054 | `			}` |
|      64 |  6055 | `			return;` |
|     ! 0 |  6056 | `		}` |
|     ! 0 |  6057 | `	}` |
|       - |  6058 | `	{` |
|   67085 |  6059 | `		int bFirst = 1;` |
|       - |  6060 | `		/* 1) Classes in declaration order */` |
|  134247 |  6061 | `		for( i = 0; i < nAtoms; i++ ){` |
|   67167 |  6062 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    9607 |  6063 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    9607 |  6064 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    9607 |  6065 | `				bFirst = 0;` |
|    4801 |  6066 | `			}` |
|   33586 |  6067 | `		}` |
|       - |  6068 | `		/* 2) Built-ins in canonical order */` |
|       - |  6069 | `		{` |
|       - |  6070 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6071 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6072 | `			int k;` |
|  469565 |  6073 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  747915 |  6074 | `				for( i = 0; i < nAtoms; i++ ){` |
|  402881 |  6075 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   57451 |  6076 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   57451 |  6077 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   57451 |  6078 | `						bFirst = 0;` |
|   57451 |  6079 | `						break;` |
|       - |  6080 | `					}` |
|  172720 |  6081 | `				}` |
|  201245 |  6082 | `			}` |
|       - |  6083 | `		}` |
|       - |  6084 | `		/* 3) null suffix */` |
|   67085 |  6085 | `		if( bNullable ){` |
|      12 |  6086 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      12 |  6087 | `			SyBlobAppend(pBlob, "null", 4);` |
|       5 |  6088 | `		}` |
|       - |  6089 | `	}` |
|   33575 |  6090 |  |
|       - |  6091 |  |
|       - |  6092 | `/*` |
|       - |  6093 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6094 | ` *` |
|       - |  6095 | ` * Outputs:` |
|       - |  6096 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6097 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6098 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6099 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6100 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6101 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6102 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6103 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6104 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6105 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6106 | ` *` |
|       - |  6107 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6108 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6109 | ` */` |
|   67150 |  6110 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6111 | `	ph7_gen_state *pGen,` |
|       - |  6112 | `	sxu32 *pnType,` |
|       - |  6113 | `	SyString *pClass,` |
|       - |  6114 | `	SySet *pAlts,` |
|       - |  6115 | `	sxi32 *piTypeFlags,` |
|       - |  6116 | `	SyString *pTypeText,` |
|       - |  6117 | `	int iNullableFlag,` |
|       - |  6118 | `	int iUnionFlag,` |
|       - |  6119 | `	int bAllowVoid,` |
|       - |  6120 | `	sxu32 nLine` |
|       5 |  6121 | `){` |
|       - |  6122 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   67155 |  6123 | `	int nAtoms = 0;` |
|   67155 |  6124 | `	int bShortNullable = 0;` |
|   67155 |  6125 | `	int bExplicitNull = 0;` |
|       - |  6126 | `	sxi32 rc;` |
|   67155 |  6127 | `	*pnType = 0;` |
|   67155 |  6128 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   67155 |  6129 | `	*piTypeFlags = 0;` |
|   67155 |  6130 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6131 |  |
|   67155 |  6132 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6133 | `		return SXRET_OK;` |
|       - |  6134 | `	}` |
|       - |  6135 | ``	/* Optional `?` shorthand prefix */`` |
|   67150 |  6136 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      61 |  6137 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      60 |  6138 | `		bShortNullable = 1;` |
|      60 |  6139 | `		pGen->pIn++;` |
|      60 |  6140 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6141 | `			return SXERR_SYNTAX;` |
|       - |  6142 | `		}` |
|      28 |  6143 | `	}` |
|       - |  6144 | `	/* First atom is mandatory */` |
|   67155 |  6145 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   67155 |  6146 | `	if( rc != SXRET_OK ){` |
|       3 |  6147 | `		return rc;` |
|       - |  6148 | `	}` |
|   67153 |  6149 | `	nAtoms = 1;` |
|       - |  6150 | ``	/* Subsequent atoms separated by `\|` */`` |
|  100865 |  6151 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   67296 |  6152 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     101 |  6153 | `		if( bShortNullable ){` |
|       - |  6154 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6155 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6156 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  6157 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6158 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6159 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6160 | `		}` |
|      99 |  6161 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6162 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6163 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6164 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6165 | `		}` |
|      99 |  6166 | ``		pGen->pIn++; /* skip `\|` */`` |
|      99 |  6167 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      99 |  6168 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  6169 | `			return rc;` |
|       - |  6170 | `		}` |
|      99 |  6171 | `		nAtoms++;` |
|       5 |  6172 | `	}` |
|       - |  6173 | `	/* Validation pass.` |
|       - |  6174 | `	 *` |
|       - |  6175 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6176 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6177 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6178 | `	 */` |
|       - |  6179 | `	{` |
|       - |  6180 | `		int i, j;` |
|   67151 |  6181 | `		int bHasNonNull = 0;` |
|  134385 |  6182 | `		for( i = 0; i < nAtoms; i++ ){` |
|   67245 |  6183 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     111 |  6184 | `				if( nAtoms > 1 ){` |
|       3 |  6185 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6186 | `						"Void can only be used as a standalone type");` |
|       3 |  6187 | `					return SXERR_SYNTAX;` |
|       - |  6188 | `				}` |
|     109 |  6189 | `				if( !bAllowVoid ){` |
|     ! 0 |  6190 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6191 | `						"void cannot be used here");` |
|     ! 0 |  6192 | `					return SXERR_SYNTAX;` |
|       - |  6193 | `				}` |
|     109 |  6194 | `				if( bShortNullable ){` |
|     ! 0 |  6195 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6196 | `						"Void type cannot be nullable");` |
|     ! 0 |  6197 | `					return SXERR_SYNTAX;` |
|       - |  6198 | `				}` |
|      52 |  6199 | `			}` |
|   67243 |  6200 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6201 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6202 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6203 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6204 | `				 * does not return), and folding them would mislead any` |
|       - |  6205 | `				 * future return-enforcement work. */` |
|       3 |  6206 | `				if( nAtoms > 1 ){` |
|       3 |  6207 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6208 | `						"never can only be used as a standalone type");` |
|       3 |  6209 | `					return SXERR_SYNTAX;` |
|       - |  6210 | `				}` |
|     ! 0 |  6211 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6212 | `					"never type is not yet implemented");` |
|     ! 0 |  6213 | `				return SXERR_SYNTAX;` |
|       - |  6214 | `			}` |
|   67241 |  6215 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      18 |  6216 | `				bExplicitNull = 1;` |
|      10 |  6217 | `			}else{` |
|   67225 |  6218 | `				bHasNonNull = 1;` |
|       - |  6219 | `			}` |
|       - |  6220 | `			/* Duplicate detection */` |
|   67365 |  6221 | `			for( j = 0; j < i; j++ ){` |
|     131 |  6222 | `				int bDup = 0;` |
|     131 |  6223 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      17 |  6224 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6225 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      14 |  6226 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6227 | `								aAtoms[j].sClass.zString,` |
|      12 |  6228 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6229 | `							bDup = 1;` |
|     ! 0 |  6230 | `						}` |
|       8 |  6231 | `					}else{` |
|       3 |  6232 | `						bDup = 1;` |
|       - |  6233 | `					}` |
|       7 |  6234 | `				}` |
|     131 |  6235 | `				if( bDup ){` |
|       - |  6236 | `					const char *zName;` |
|       - |  6237 | `					sxu32 nName;` |
|       3 |  6238 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6239 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6240 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6241 | `					}else{` |
|       3 |  6242 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6243 | `						nName = aAtoms[i].nCanon;` |
|       - |  6244 | `					}` |
|       4 |  6245 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6246 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6247 | `					return SXERR_SYNTAX;` |
|       - |  6248 | `				}` |
|      67 |  6249 | `			}` |
|   33622 |  6250 | `		}` |
|   67145 |  6251 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6252 | `			if( bShortNullable ){` |
|       - |  6253 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6254 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6255 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6256 | `				return SXERR_SYNTAX;` |
|       - |  6257 | `			}` |
|       - |  6258 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6259 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6260 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6261 | `			 * atom, so set it here. */` |
|       7 |  6262 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6263 | `		}` |
|       - |  6264 | `	}` |
|       - |  6265 | `	/* Compute nullability flag */` |
|   67145 |  6266 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      74 |  6267 | `		*piTypeFlags \|= iNullableFlag;` |
|      35 |  6268 | `	}` |
|       - |  6269 | `	/* Build canonical type text */` |
|   67145 |  6270 | `	if( pTypeText ){` |
|       - |  6271 | `		SyBlob sBlob;` |
|   67145 |  6272 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  100688 |  6273 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   33570 |  6274 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   67145 |  6275 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  100559 |  6276 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   67036 |  6277 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   67041 |  6278 | `			if( zDup ){` |
|   67041 |  6279 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   33518 |  6280 | `			}` |
|   33518 |  6281 | `		}` |
|   67145 |  6282 | `		SyBlobRelease(&sBlob);` |
|   33570 |  6283 | `	}` |
|       - |  6284 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6285 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6286 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6287 | `	{` |
|   67145 |  6288 | `		int nNonNull = 0;` |
|   67145 |  6289 | `		int iNonNullIdx = -1;` |
|       - |  6290 | `		int i;` |
|  134373 |  6291 | `		for( i = 0; i < nAtoms; i++ ){` |
|   67233 |  6292 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   67217 |  6293 | `				nNonNull++;` |
|   67217 |  6294 | `				iNonNullIdx = i;` |
|   33606 |  6295 | `			}` |
|   33619 |  6296 | `		}` |
|   67145 |  6297 | `		if( nNonNull <= 1 ){` |
|       - |  6298 | `			/* Fast path: store as single type. */` |
|   67083 |  6299 | `			if( iNonNullIdx >= 0 ){` |
|   67077 |  6300 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   67077 |  6301 | `				if( pA->nType == SXU32_HIGH ){` |
|   14384 |  6302 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    4793 |  6303 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    9591 |  6304 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    9591 |  6305 | `					*pnType = SXU32_HIGH;` |
|    9591 |  6306 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   62284 |  6307 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     109 |  6308 | `					*pnType = MEMOBJ_VOID;` |
|      57 |  6309 | `				}else{` |
|       - |  6310 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6311 | `					 * pass above rejects it as not-yet-implemented. */` |
|   57387 |  6312 | `					*pnType = pA->nType;` |
|       - |  6313 | `				}` |
|   33536 |  6314 | `			}` |
|   33544 |  6315 | `		}else{` |
|       - |  6316 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      67 |  6317 | `			*piTypeFlags \|= iUnionFlag;` |
|     211 |  6318 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6319 | `				ph7_type_alt sAlt;` |
|     149 |  6320 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     145 |  6321 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     145 |  6322 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      44 |  6323 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      14 |  6324 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      30 |  6325 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      30 |  6326 | `					sAlt.nType = SXU32_HIGH;` |
|      30 |  6327 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      16 |  6328 | `				}else{` |
|     117 |  6329 | `					sAlt.nType = aAtoms[i].nType;` |
|     117 |  6330 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6331 | `				}` |
|     145 |  6332 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      75 |  6333 | `			}` |
|       - |  6334 | `		}` |
|       - |  6335 | `	}` |
|   67145 |  6336 | `	return SXRET_OK;` |
|   33580 |  6337 |  |
|       - |  6338 |  |
|       - |  6339 | `/*` |
|       - |  6340 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6341 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6342 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6343 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6344 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6345 | `` *          and union types `: T\|U`.`` |
|       - |  6346 | ` */` |
|  280486 |  6347 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6348 |  |
|  280491 |  6349 | `	sxi32 iFlags = 0;` |
|       - |  6350 | `	sxi32 rc;` |
|       - |  6351 | `	sxu32 nLine;` |
|  280491 |  6352 | `	pFunc->nReturnType = 0;` |
|  280491 |  6353 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  280491 |  6354 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  280491 |  6355 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  280151 |  6356 | `		return SXRET_OK;` |
|       - |  6357 | `	}` |
|     345 |  6358 | `	pGen->pIn++; /* Skip ':' */` |
|     345 |  6359 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6360 | `		return SXRET_OK;` |
|       - |  6361 | `	}` |
|     345 |  6362 | `	nLine = pGen->pIn->nLine;` |
|     345 |  6363 | `	rc = GenStateParseUnionTypeDecl(` |
|     170 |  6364 | `		pGen,` |
|     170 |  6365 | `		&pFunc->nReturnType,` |
|     170 |  6366 | `		&pFunc->sReturnClass,` |
|     170 |  6367 | `		&pFunc->aReturnUnion,` |
|       - |  6368 | `		&iFlags,` |
|     170 |  6369 | `		&pFunc->sReturnTypeName,` |
|       - |  6370 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6371 | `		/* iUnionFlag */ 0,` |
|       - |  6372 | `		/* bAllowVoid */ 1,` |
|     170 |  6373 | `		nLine);` |
|     170 |  6374 | `	(void)iFlags;` |
|     345 |  6375 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6376 | `		return SXERR_ABORT;` |
|       - |  6377 | `	}` |
|     345 |  6378 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6379 | `		/* Error already reported */` |
|     ! 0 |  6380 | `		return SXERR_SYNTAX;` |
|       - |  6381 | `	}` |
|     345 |  6382 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6383 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6384 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6385 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6386 | `				&pGen->pIn->sData);` |
|       3 |  6387 | `		}else{` |
|     ! 0 |  6388 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6389 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6390 | `		}` |
|       5 |  6391 | `		return SXERR_SYNTAX;` |
|       - |  6392 | `	}` |
|     341 |  6393 | `	return SXRET_OK;` |
|  140248 |  6394 |  |
|       - |  6395 |  |
|   42208 |  6396 | `static sxi32 GenStateCompileFunc(` |
|       - |  6397 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6398 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6399 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6400 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6401 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6402 | `	)` |
|       5 |  6403 |  |
|       - |  6404 | `	ph7_vm_func *pFunc;` |
|       - |  6405 | `	SyToken *pEnd;` |
|       - |  6406 | `	sxu32 nLine;` |
|       - |  6407 | `	char *zName;` |
|       - |  6408 | `	sxi32 rc;` |
|       - |  6409 | `	/* Extract line number */` |
|   42213 |  6410 | `	nLine = pGen->pIn->nLine;` |
|       - |  6411 | `	/* Jump the left parenthesis '(' */` |
|   42213 |  6412 | `	pGen->pIn++;` |
|       - |  6413 | `	/* Delimit the function signature */` |
|   42213 |  6414 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   42213 |  6415 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6416 | `		/* Syntax error */` |
|       9 |  6417 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       9 |  6418 | `		if( rc == SXERR_ABORT ){` |
|       - |  6419 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6420 | `			return SXERR_ABORT;` |
|       - |  6421 | `		}` |
|       9 |  6422 | `		pGen->pIn = pGen->pEnd;` |
|       9 |  6423 | `		return SXRET_OK;` |
|       - |  6424 | `	}` |
|       - |  6425 | `	/* Create the function state */` |
|   42207 |  6426 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   42207 |  6427 | `	if( pFunc == 0 ){` |
|     ! 0 |  6428 | `		goto OutOfMem;` |
|       - |  6429 | `	}` |
|       - |  6430 | `	/* Build the function name, prepending namespace if active */` |
|   42214 |  6431 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6432 | `		SyBlob sFQN;` |
|       - |  6433 | `		sxu32 nLen;` |
|      16 |  6434 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6435 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6436 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6437 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6438 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6439 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6440 | `		SyBlobRelease(&sFQN);` |
|      16 |  6441 | `		if( zName == 0 ){` |
|     ! 0 |  6442 | `			goto OutOfMem;` |
|       - |  6443 | `		}` |
|      16 |  6444 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6445 | `	}else{` |
|   42193 |  6446 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   42193 |  6447 | `		if( zName == 0 ){` |
|     ! 0 |  6448 | `			goto OutOfMem;` |
|       - |  6449 | `		}` |
|   42193 |  6450 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6451 | `	}` |
|   42207 |  6452 | `	if( pGen->pIn < pEnd ){` |
|       - |  6453 | `		/* Collect function arguments */` |
|   29215 |  6454 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   29215 |  6455 | `		if( rc == SXERR_ABORT ){` |
|       - |  6456 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6457 | `			return SXERR_ABORT;` |
|       - |  6458 | `		}` |
|   14605 |  6459 | `	}` |
|       - |  6460 | `	/* Point past ')' and parse optional return type ': type' */` |
|   42207 |  6461 | `	pGen->pIn = &pEnd[1];` |
|       - |  6462 | `	{` |
|   42207 |  6463 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   42207 |  6464 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6465 | `			return SXERR_ABORT;` |
|   42207 |  6466 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6467 | `			return SXERR_SYNTAX;` |
|       - |  6468 | `		}` |
|       - |  6469 | `	}` |
|   42203 |  6470 | `	if( bHandleClosure ){` |
|       - |  6471 | `		ph7_vm_func_closure_env sEnv;` |
|     251 |  6472 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     246 |  6473 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     136 |  6474 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      21 |  6475 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6476 | `				/* Closure,record environment variable */` |
|      21 |  6477 | `				pGen->pIn++;` |
|      21 |  6478 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6479 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6480 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6481 | `						return SXERR_ABORT;` |
|       - |  6482 | `					}` |
|     ! 0 |  6483 | `				}` |
|      21 |  6484 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6485 | `				/* Compile until we hit the first closing parenthesis */` |
|      41 |  6486 | `				while( pGen->pIn < pGen->pEnd ){` |
|      41 |  6487 | `					int iFlagsLocal = 0;` |
|      41 |  6488 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      21 |  6489 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      21 |  6490 | `						break;` |
|       - |  6491 | `					}` |
|      25 |  6492 | `					nLineLocal = pGen->pIn->nLine;` |
|      25 |  6493 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6494 | `						/* Pass by reference,record that */` |
|     ! 0 |  6495 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6496 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6497 | `							);` |
|     ! 0 |  6498 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6499 | `						pGen->pIn++;` |
|     ! 0 |  6500 | `					}` |
|      20 |  6501 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      25 |  6502 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6503 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6504 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6505 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6506 | `								return SXERR_ABORT;` |
|       - |  6507 | `							}` |
|       - |  6508 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6509 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6510 | `								pGen->pIn++;` |
|     ! 0 |  6511 | `							}` |
|     ! 0 |  6512 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6513 | `								pGen->pIn++;` |
|     ! 0 |  6514 | `							}` |
|     ! 0 |  6515 | `							break;` |
|       - |  6516 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6517 | `					}else{` |
|       - |  6518 | `						SyString *pNameLocal;` |
|       - |  6519 | `						char *zDup;` |
|       - |  6520 | `						/* Duplicate variable name */` |
|      25 |  6521 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      25 |  6522 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      25 |  6523 | `						if( zDup ){` |
|       - |  6524 | `							/* Zero the structure */` |
|      25 |  6525 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      25 |  6526 | `							sEnv.iFlags = iFlagsLocal;` |
|      25 |  6527 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      25 |  6528 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      25 |  6529 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6530 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6531 | `									got_this = 1;` |
|     ! 0 |  6532 | `							}` |
|       - |  6533 | `							/* Save imported variable */` |
|      25 |  6534 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      15 |  6535 | `						}else{` |
|     ! 0 |  6536 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6537 | `							 return SXERR_ABORT;` |
|       - |  6538 | `						}` |
|       - |  6539 | `					}` |
|      25 |  6540 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      31 |  6541 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6542 | `						/* Ignore trailing commas */` |
|       7 |  6543 | `						pGen->pIn++;` |
|       1 |  6544 | `					}` |
|       5 |  6545 | `				}` |
|      21 |  6546 | `				if( !got_this ){` |
|       - |  6547 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6548 | `					 * available to the closure environment.` |
|       - |  6549 | `					 */` |
|      21 |  6550 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      21 |  6551 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      21 |  6552 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      21 |  6553 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      21 |  6554 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       8 |  6555 | `				}` |
|      21 |  6556 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6557 | `					/* Mark as closure */` |
|      21 |  6558 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       8 |  6559 | `				}` |
|       8 |  6560 | `		}` |
|     123 |  6561 | `	}` |
|       - |  6562 | `	/* Compile the body */` |
|   42203 |  6563 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   42203 |  6564 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6565 | `		return SXERR_ABORT;` |
|       - |  6566 | `	}` |
|   42203 |  6567 | `	if( ppFunc ){` |
|     251 |  6568 | `		*ppFunc = pFunc;` |
|     123 |  6569 | `	}` |
|   42203 |  6570 | `	rc = SXRET_OK;` |
|   42203 |  6571 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6572 | `		/* Finally register the function */` |
|   42187 |  6573 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   21091 |  6574 | `	}` |
|   42203 |  6575 | `	if( rc == SXRET_OK ){` |
|   42203 |  6576 | `		return SXRET_OK;` |
|       - |  6577 | `	}` |
|       - |  6578 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6579 | `OutOfMem:` |
|       - |  6580 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6581 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6582 | `	 */` |
|     ! 0 |  6583 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6584 | `	return SXERR_ABORT;` |
|   21109 |  6585 |  |
|       - |  6586 | `/*` |
|       - |  6587 | ` * Compile a standard PHP function.` |
|       - |  6588 | ` *  Refer to the block-comment above for more information.` |
|       - |  6589 | ` */` |
|   41968 |  6590 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6591 |  |
|       - |  6592 | `	SyString *pName;` |
|       - |  6593 | `	sxi32 iFlags;` |
|       - |  6594 | `	sxu32 nLine;` |
|       - |  6595 | `	sxi32 rc;` |
|       - |  6596 |  |
|   41973 |  6597 | `	nLine = pGen->pIn->nLine;` |
|   41973 |  6598 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   41973 |  6599 | `	iFlags = 0;` |
|   41973 |  6600 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6601 | `		/* Return by reference,remember that */` |
|       7 |  6602 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6603 | `		/* Jump the '&' token */` |
|       7 |  6604 | `		pGen->pIn++;` |
|       3 |  6605 | `	}` |
|   41973 |  6606 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6607 | `		/* Invalid function name */` |
|       6 |  6608 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       6 |  6609 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6610 | `			return SXERR_ABORT;` |
|       - |  6611 | `		}` |
|       - |  6612 | `		/* Sychronize with the next semi-colon or braces*/` |
|      18 |  6613 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      14 |  6614 | `			pGen->pIn++;` |
|       2 |  6615 | `		}` |
|       6 |  6616 | `		return SXRET_OK;` |
|       - |  6617 | `	}` |
|   41969 |  6618 | `	pName = &pGen->pIn->sData;` |
|   41969 |  6619 | `	nLine = pGen->pIn->nLine;` |
|       - |  6620 | `	/* Jump the function name */` |
|   41969 |  6621 | `	pGen->pIn++;` |
|   41969 |  6622 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6623 | `		/* Syntax error */` |
|       3 |  6624 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6625 | `		if( rc == SXERR_ABORT ){` |
|       - |  6626 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6627 | `			return SXERR_ABORT;` |
|       - |  6628 | `		}` |
|       - |  6629 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6630 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6631 | `			pGen->pIn++;` |
|     ! 0 |  6632 | `		}` |
|       3 |  6633 | `		return SXRET_OK;` |
|       - |  6634 | `	}` |
|       - |  6635 | `	/* Compile function body */` |
|   41967 |  6636 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   41967 |  6637 | `	return rc;` |
|   20989 |  6638 |  |
|       - |  6639 | `/*` |
|       - |  6640 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6641 | ` * According to the PHP language reference manual` |
|       - |  6642 | ` *  Visibility:` |
|       - |  6643 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6644 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6645 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6646 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6647 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6648 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6649 | ` */` |
|  298876 |  6650 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  6651 |  |
|  298881 |  6652 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    9581 |  6653 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  289305 |  6654 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   41181 |  6655 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6656 | `	}` |
|       - |  6657 | `	/* Assume public by default */` |
|  248129 |  6658 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  149443 |  6659 |  |
|       - |  6660 | `/*` |
|       - |  6661 | ` * Compile a class constant.` |
|       - |  6662 | ` * According to the PHP language reference manual` |
|       - |  6663 | ` *  Class Constants` |
|       - |  6664 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6665 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6666 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6667 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6668 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6669 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6670 | ` * Symisc eXtension.` |
|       - |  6671 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6672 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6673 | ` *  Example:` |
|       - |  6674 | ` *   class Test{` |
|       - |  6675 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6676 | ` *   };` |
|       - |  6677 | ` *   var_dump(TEST::MyConst);` |
|       - |  6678 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6679 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6680 | ` */` |
|       - |  6681 | `/*` |
|       - |  6682 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  6683 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  6684 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  6685 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  6686 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  6687 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  6688 | ` */` |
|      76 |  6689 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  6690 |  |
|       - |  6691 | `	SyToken *p0, *p1;` |
|      81 |  6692 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6693 | `		return 0;` |
|       - |  6694 | `	}` |
|      81 |  6695 | `	p0 = pGen->pIn;` |
|       - |  6696 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      81 |  6697 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  6698 | `		return 1;` |
|       - |  6699 | `	}` |
|      81 |  6700 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  6701 | `		return 1;` |
|       - |  6702 | `	}` |
|       - |  6703 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  6704 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  6705 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      77 |  6706 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      77 |  6707 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      77 |  6708 | `		if( p1 ){` |
|      77 |  6709 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      24 |  6710 | `				return 1;` |
|       - |  6711 | `			}` |
|      57 |  6712 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  6713 | `				return 1;` |
|       - |  6714 | `			}` |
|      24 |  6715 | `		}` |
|      24 |  6716 | `	}` |
|      53 |  6717 | `	return 0;` |
|      43 |  6718 |  |
|       - |  6719 | `/*` |
|       - |  6720 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  6721 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  6722 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  6723 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  6724 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  6725 | ` * share the same backing.` |
|       - |  6726 | ` */` |
|     164 |  6727 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  6728 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  6729 |  |
|     169 |  6730 | `	pAttr->nType = nType;` |
|     169 |  6731 | `	pAttr->sClass = *pClass;` |
|     169 |  6732 | `	pAttr->sTypeName = *pTypeName;` |
|     169 |  6733 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  6734 | `		sxu32 i;` |
|      46 |  6735 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      32 |  6736 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      32 |  6737 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      18 |  6738 | `		}` |
|       7 |  6739 | `	}` |
|     169 |  6740 |  |
|      76 |  6741 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  6742 |  |
|      81 |  6743 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6744 | `	SySet *pInstrContainer;` |
|       - |  6745 | `	ph7_class_attr *pCons;` |
|       - |  6746 | `	SyString *pName;` |
|       - |  6747 | `	sxi32 rc;` |
|      81 |  6748 | `	sxu32 nType = 0;` |
|       - |  6749 | `	SyString sTypeClass;` |
|       - |  6750 | `	SyString sTypeText;` |
|       - |  6751 | `	SySet aUnionAlts;` |
|      81 |  6752 | `	sxi32 iTypeFlags = 0;` |
|      81 |  6753 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      81 |  6754 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      81 |  6755 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  6756 | `	/* Extract visibility level */` |
|      81 |  6757 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  6758 | `	/* Mark as constant */` |
|      81 |  6759 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      81 |  6760 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  6761 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  6762 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|      95 |  6763 | `	if( GenStateClassConstHasType(pGen) ){` |
|      46 |  6764 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      28 |  6765 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  6766 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  6767 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  6768 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  6769 | `		 * and success paths release. */` |
|      32 |  6770 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  6771 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  6772 | `			goto Synchronize;` |
|      32 |  6773 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  6774 | `			return SXERR_ABORT;` |
|      32 |  6775 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  6776 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  6777 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  6778 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6779 | `				return SXERR_ABORT;` |
|       - |  6780 | `			}` |
|     ! 0 |  6781 | `			goto Synchronize;` |
|       - |  6782 | `		}` |
|      32 |  6783 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      14 |  6784 | `	}` |
|      38 |  6785 | `loop:` |
|      83 |  6786 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6787 | `		/* Invalid constant name */` |
|     ! 0 |  6788 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6789 | `		if( rc == SXERR_ABORT ){` |
|       - |  6790 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6791 | `			return SXERR_ABORT;` |
|       - |  6792 | `		}` |
|     ! 0 |  6793 | `		goto Synchronize;` |
|       - |  6794 | `	}` |
|       - |  6795 | `	/* Peek constant name */` |
|      83 |  6796 | `	pName = &pGen->pIn->sData;` |
|       - |  6797 | `	/* Make sure the constant name isn't reserved */` |
|      83 |  6798 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6799 | `		/* Reserved constant name */` |
|     ! 0 |  6800 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6801 | `		if( rc == SXERR_ABORT ){` |
|       - |  6802 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6803 | `			return SXERR_ABORT;` |
|       - |  6804 | `		}` |
|     ! 0 |  6805 | `		goto Synchronize;` |
|       - |  6806 | `	}` |
|       - |  6807 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      83 |  6808 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      46 |  6809 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      28 |  6810 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      14 |  6811 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      32 |  6812 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6813 | `			return SXERR_ABORT;` |
|      32 |  6814 | `		}else if( rc != SXRET_OK ){` |
|       3 |  6815 | `			goto Synchronize;` |
|       - |  6816 | `		}` |
|      13 |  6817 | `	}` |
|       - |  6818 | `	/* Advance the stream cursor */` |
|      81 |  6819 | `	pGen->pIn++;` |
|      81 |  6820 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6821 | `		/* Invalid declaration */` |
|     ! 0 |  6822 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6823 | `		if( rc == SXERR_ABORT ){` |
|       - |  6824 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6825 | `			return SXERR_ABORT;` |
|       - |  6826 | `		}` |
|     ! 0 |  6827 | `		goto Synchronize;` |
|       - |  6828 | `	}` |
|      81 |  6829 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6830 | `	/* Allocate a new class attribute */` |
|      81 |  6831 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      81 |  6832 | `	if( pCons == 0 ){` |
|     ! 0 |  6833 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6834 | `		return SXERR_ABORT;` |
|       - |  6835 | `	}` |
|      81 |  6836 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      29 |  6837 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      13 |  6838 | `	}` |
|       - |  6839 | `	/* Swap bytecode container */` |
|      81 |  6840 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      81 |  6841 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6842 | `	/* Compile constant value.` |
|       - |  6843 | `	 */` |
|      81 |  6844 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      81 |  6845 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6846 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6847 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6848 | `			return SXERR_ABORT;` |
|       - |  6849 | `		}` |
|       1 |  6850 | `	}` |
|       - |  6851 | `	/* Emit the done instruction */` |
|      81 |  6852 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      81 |  6853 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      81 |  6854 | `	if( rc == SXERR_ABORT ){` |
|       - |  6855 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6856 | `		return SXERR_ABORT;` |
|       - |  6857 | `	}` |
|       - |  6858 | `	/* All done,install the constant */` |
|      81 |  6859 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      81 |  6860 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6861 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6862 | `		return SXERR_ABORT;` |
|       - |  6863 | `	}` |
|      81 |  6864 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6865 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  6866 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  6867 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6868 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6869 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6870 | `				pTok--;` |
|     ! 0 |  6871 | `			}` |
|     ! 0 |  6872 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6873 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6874 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6875 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6876 | `				return SXERR_ABORT;` |
|       - |  6877 | `			}` |
|     ! 0 |  6878 | `		}else{` |
|       3 |  6879 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  6880 | `				goto loop;` |
|       - |  6881 | `			}` |
|       - |  6882 | `		}` |
|     ! 0 |  6883 | `	}` |
|      79 |  6884 | `	SySetRelease(&aUnionAlts);` |
|      79 |  6885 | `	return SXRET_OK;` |
|       1 |  6886 | `Synchronize:` |
|       3 |  6887 | `	SySetRelease(&aUnionAlts);` |
|       - |  6888 | `	/* Synchronize with the first semi-colon */` |
|       9 |  6889 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  6890 | `		pGen->pIn++;` |
|       1 |  6891 | `	}` |
|       3 |  6892 | `	return SXERR_CORRUPT;` |
|      43 |  6893 |  |
|       - |  6894 | `/*` |
|       - |  6895 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6896 | ` * According to the PHP language reference manual` |
|       - |  6897 | ` *  Properties` |
|       - |  6898 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6899 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6900 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6901 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6902 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6903 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6904 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6905 | ` * Symisc eXtension.` |
|       - |  6906 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6907 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6908 | ` *  Example:` |
|       - |  6909 | ` *   class Test{` |
|       - |  6910 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6911 | ` *   };` |
|       - |  6912 | ` *   var_dump(TEST::myVar);` |
|       - |  6913 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6914 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6915 | ` */` |
|       - |  6916 | `/*` |
|       - |  6917 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6918 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6919 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6920 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6921 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6922 | ` */` |
|  155960 |  6923 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  6924 |  |
|  155965 |  6925 | `	SyToken *p = pStart;` |
|  155965 |  6926 | `	if( p >= pEnd ) return 0;` |
|  155965 |  6927 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 |  6928 | `		p++;` |
|      16 |  6929 | `		if( p >= pEnd ) return 0;` |
|       7 |  6930 | `	}` |
|  155965 |  6931 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6932 | `		p++;` |
|       3 |  6933 | `		if( p >= pEnd ) return 0;` |
|       1 |  6934 | `	}` |
|  155965 |  6935 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6936 | `		return 0;` |
|       - |  6937 | `	}` |
|       - |  6938 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6939 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6940 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6941 | `	 * dispatch site. */` |
|  155965 |  6942 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  155943 |  6943 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  155995 |  6944 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3338 |  6945 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  155829 |  6946 | `			return 0;` |
|       - |  6947 | `		}` |
|      57 |  6948 | `	}` |
|     141 |  6949 | `	p++;` |
|       - |  6950 | `	/* Consume optional namespace path */` |
|     143 |  6951 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6952 | `		p += 2;` |
|       1 |  6953 | `	}` |
|       - |  6954 | ``	/* Consume any `\| Type` union alternatives */`` |
|     222 |  6955 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      91 |  6956 | `		&& p->sData.zString[0] == '\|' ){` |
|      16 |  6957 | `		p++;` |
|      16 |  6958 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      16 |  6959 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      16 |  6960 | `		p++;` |
|      16 |  6961 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6962 | `			p += 2;` |
|     ! 0 |  6963 | `		}` |
|       4 |  6964 | `	}` |
|     141 |  6965 | `	if( p >= pEnd ) return 0;` |
|     141 |  6966 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   77985 |  6967 |  |
|       - |  6968 |  |
|       - |  6969 | `/*` |
|       - |  6970 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6971 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6972 | ` * if not). Recognized forms:` |
|       - |  6973 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6974 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6975 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6976 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6977 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6978 | ` * on unrecoverable error.` |
|       - |  6979 | ` *` |
|       - |  6980 | ` * When a type is parsed:` |
|       - |  6981 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  6982 | ` *   *pClass is set to the class name (for class types)` |
|       - |  6983 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  6984 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  6985 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  6986 | ` */` |
|     136 |  6987 | `static sxi32 GenStateParsePropertyType(` |
|       - |  6988 | `	ph7_gen_state *pGen,` |
|       - |  6989 | `	sxu32 *pnType,` |
|       - |  6990 | `	SyString *pClass,` |
|       - |  6991 | `	sxi32 *piTypeFlags,` |
|       - |  6992 | `	SyString *pTypeText,` |
|       - |  6993 | `	SySet *pAlts` |
|       5 |  6994 | `){` |
|     141 |  6995 | `	sxi32 iFlags = 0;` |
|       - |  6996 | `	sxi32 rc;` |
|     141 |  6997 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6998 | `		return SXRET_OK;` |
|       - |  6999 | `	}` |
|       - |  7000 | `	/* If the first token is '$', there's no type */` |
|     141 |  7001 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7002 | `		return SXRET_OK;` |
|       - |  7003 | `	}` |
|     141 |  7004 | `	rc = GenStateParseUnionTypeDecl(` |
|      68 |  7005 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7006 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7007 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7008 | `		/* bAllowVoid */ 0,` |
|     136 |  7009 | `		pGen->pIn->nLine);` |
|     141 |  7010 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7011 | `		return rc;` |
|       - |  7012 | `	}` |
|       - |  7013 | `	/* Verify next token is '$' (start of property name) */` |
|     141 |  7014 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7015 | `		return SXERR_SYNTAX;` |
|       - |  7016 | `	}` |
|     141 |  7017 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     141 |  7018 | `	return SXRET_OK;` |
|      73 |  7019 |  |
|       - |  7020 |  |
|       - |  7021 | `/*` |
|       - |  7022 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7023 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7024 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7025 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7026 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7027 | ` * by the type parser itself before reaching here.` |
|       - |  7028 | ` *` |
|       - |  7029 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7030 | ` * use in the error message.` |
|       - |  7031 | ` */` |
|     238 |  7032 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7033 | `	sxu32 nType,` |
|       - |  7034 | `	const SyString *pClass,` |
|       - |  7035 | `	const char **pzName,` |
|       - |  7036 | `	sxu32 *pnName)` |
|       5 |  7037 |  |
|       - |  7038 | `	const char *z;` |
|       - |  7039 | `	sxu32 n;` |
|     243 |  7040 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     207 |  7041 | `		return 0;` |
|       - |  7042 | `	}` |
|      40 |  7043 | `	z = pClass->zString;` |
|      40 |  7044 | `	n = pClass->nByte;` |
|      40 |  7045 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7046 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7047 | `	}` |
|       - |  7048 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7049 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7050 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      33 |  7051 | `	return 0;` |
|     124 |  7052 |  |
|       - |  7053 |  |
|       - |  7054 | `/*` |
|       - |  7055 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7056 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7057 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7058 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7059 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7060 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7061 | ` *` |
|       - |  7062 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7063 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7064 | ` */` |
|     202 |  7065 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7066 | `	ph7_gen_state *pGen,` |
|       - |  7067 | `	ph7_class *pClass,` |
|       - |  7068 | `	const SyString *pMemberName,` |
|       - |  7069 | `	sxu32 nType,` |
|       - |  7070 | `	const SyString *pTypeClass,` |
|       - |  7071 | `	const SyString *pTypeText,` |
|       - |  7072 | `	SySet *pUnionAlts,` |
|       - |  7073 | `	const char *zErrFmt,` |
|       - |  7074 | `	sxu32 nLine)` |
|       5 |  7075 |  |
|     207 |  7076 | `	const char *zBad = 0;` |
|     207 |  7077 | `	sxu32 nBad = 0;` |
|       - |  7078 | `	SyString sFallback;` |
|       - |  7079 | `	const SyString *pBad;` |
|       - |  7080 | `	sxi32 rc;` |
|     207 |  7081 | `	int bDisallowed = 0;` |
|     207 |  7082 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7083 | `		bDisallowed = 1;` |
|     205 |  7084 | `	}else if( pUnionAlts ){` |
|       - |  7085 | `		sxu32 i;` |
|      56 |  7086 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      40 |  7087 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      40 |  7088 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7089 | `				bDisallowed = 1;` |
|       3 |  7090 | `				break;` |
|       - |  7091 | `			}` |
|      21 |  7092 | `		}` |
|       9 |  7093 | `	}` |
|     207 |  7094 | `	if( !bDisallowed ){` |
|     201 |  7095 | `		return SXRET_OK;` |
|       - |  7096 | `	}` |
|       - |  7097 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7098 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7099 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7100 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7101 | `		pBad = pTypeText;` |
|       5 |  7102 | `	}else{` |
|     ! 0 |  7103 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7104 | `		pBad = &sFallback;` |
|       - |  7105 | `	}` |
|      11 |  7106 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7107 | `		zErrFmt,` |
|       3 |  7108 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7109 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7110 | `		return SXERR_ABORT;` |
|       - |  7111 | `	}` |
|       8 |  7112 | `	return SXERR_SYNTAX;` |
|     106 |  7113 |  |
|       - |  7114 |  |
|   60628 |  7115 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7116 |  |
|   60633 |  7117 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7118 | `	ph7_class_attr *pAttr;` |
|       - |  7119 | `	SyString *pName;` |
|       - |  7120 | `	sxi32 rc;` |
|   60633 |  7121 | `	sxu32 nType = 0;` |
|       - |  7122 | `	SyString sTypeClass;` |
|       - |  7123 | `	SyString sTypeText;` |
|       - |  7124 | `	SySet aUnionAlts;` |
|   60633 |  7125 | `	sxi32 iTypeFlags = 0;` |
|   60633 |  7126 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   60633 |  7127 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   60633 |  7128 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7129 | `	/* Extract visibility level */` |
|   60633 |  7130 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7131 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   60701 |  7132 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     141 |  7133 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     141 |  7134 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7135 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7136 | `			goto Synchronize;` |
|     141 |  7137 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7138 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7139 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7140 | `				&pGen->pIn->sData);` |
|     ! 0 |  7141 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7142 | `				return SXERR_ABORT;` |
|       - |  7143 | `			}` |
|     ! 0 |  7144 | `			goto Synchronize;` |
|     141 |  7145 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7146 | `			return SXERR_ABORT;` |
|       - |  7147 | `		}` |
|      68 |  7148 | `	}` |
|     ! 0 |  7149 | `loop:` |
|   60637 |  7150 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7151 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7152 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7153 | `			return SXERR_ABORT;` |
|       - |  7154 | `		}` |
|     ! 0 |  7155 | `		goto Synchronize;` |
|       - |  7156 | `	}` |
|   60637 |  7157 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   60637 |  7158 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7159 | `		/* Invalid attribute name */` |
|     ! 0 |  7160 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7161 | `		if( rc == SXERR_ABORT ){` |
|       - |  7162 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7163 | `			return SXERR_ABORT;` |
|       - |  7164 | `		}` |
|     ! 0 |  7165 | `		goto Synchronize;` |
|       - |  7166 | `	}` |
|       - |  7167 | `	/* Peek attribute name */` |
|   60637 |  7168 | `	pName = &pGen->pIn->sData;` |
|       - |  7169 | `	/* Advance the stream cursor */` |
|   60637 |  7170 | `	pGen->pIn++;` |
|   60637 |  7171 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7172 | `		/* Invalid declaration */` |
|       3 |  7173 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7174 | `		if( rc == SXERR_ABORT ){` |
|       - |  7175 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7176 | `			return SXERR_ABORT;` |
|       - |  7177 | `		}` |
|       3 |  7178 | `		goto Synchronize;` |
|       - |  7179 | `	}` |
|       - |  7180 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7181 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7182 | `	 * by the type parser. */` |
|   60635 |  7183 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     215 |  7184 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7185 | `			&sTypeText,` |
|     140 |  7186 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      70 |  7187 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     145 |  7188 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7189 | `			return SXERR_ABORT;` |
|     145 |  7190 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7191 | `			goto Synchronize;` |
|       - |  7192 | `		}` |
|      70 |  7193 | `	}` |
|       - |  7194 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   60635 |  7195 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7196 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7197 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7198 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7199 | `			return SXERR_ABORT;` |
|       - |  7200 | `		}` |
|       3 |  7201 | `		goto Synchronize;` |
|       - |  7202 | `	}` |
|       - |  7203 | `	/* Allocate a new class attribute */` |
|   60633 |  7204 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   60633 |  7205 | `	if( pAttr == 0 ){` |
|     ! 0 |  7206 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7207 | `		return SXERR_ABORT;` |
|       - |  7208 | `	}` |
|   60633 |  7209 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     143 |  7210 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      69 |  7211 | `	}` |
|   60633 |  7212 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7213 | `		SySet *pInstrContainer;` |
|   19383 |  7214 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7215 | `		/* Swap bytecode container */` |
|   19383 |  7216 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   19383 |  7217 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7218 | `		/* Compile attribute value.` |
|       - |  7219 | `		 */` |
|   19383 |  7220 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   19383 |  7221 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7222 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7223 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7224 | `				return SXERR_ABORT;` |
|       - |  7225 | `			}` |
|     ! 0 |  7226 | `		}` |
|       - |  7227 | `		/* Emit the done instruction */` |
|   19383 |  7228 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   19383 |  7229 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    9689 |  7230 | `	}` |
|       - |  7231 | `	/* All done,install the attribute */` |
|   60633 |  7232 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   60633 |  7233 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7234 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7235 | `		return SXERR_ABORT;` |
|       - |  7236 | `	}` |
|   60633 |  7237 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7238 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7239 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7240 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7241 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7242 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7243 | `				pTok--;` |
|     ! 0 |  7244 | `			}` |
|     ! 0 |  7245 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7246 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7247 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7248 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7249 | `				return SXERR_ABORT;` |
|       - |  7250 | `			}` |
|     ! 0 |  7251 | `		}else{` |
|       5 |  7252 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7253 | `				goto loop;` |
|       - |  7254 | `			}` |
|       - |  7255 | `		}` |
|     ! 0 |  7256 | `	}` |
|   60629 |  7257 | `	SySetRelease(&aUnionAlts);` |
|   60629 |  7258 | `	return SXRET_OK;` |
|       2 |  7259 | `Synchronize:` |
|       - |  7260 | `	/* Synchronize with the first semi-colon */` |
|      12 |  7261 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       8 |  7262 | `		pGen->pIn++;` |
|       2 |  7263 | `	}` |
|       6 |  7264 | `	SySetRelease(&aUnionAlts);` |
|       6 |  7265 | `	return SXERR_CORRUPT;` |
|   30319 |  7266 |  |
|       - |  7267 | `/*` |
|       - |  7268 | ` * Compile a class method.` |
|       - |  7269 | ` *` |
|       - |  7270 | ` * Refer to the official documentation for more information` |
|       - |  7271 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7272 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7273 | ` * overloading and many more.` |
|       - |  7274 | ` */` |
|  238172 |  7275 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7276 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7277 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7278 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7279 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7280 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7281 | `	)` |
|       5 |  7282 |  |
|  238177 |  7283 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7284 | `	ph7_class_method *pMeth;` |
|       - |  7285 | `	sxi32 iFuncFlags;` |
|       - |  7286 | `	SyString *pName;` |
|       - |  7287 | `	SyToken *pEnd;` |
|       - |  7288 | `	sxi32 rc;` |
|       - |  7289 | `	/* Extract visibility level */` |
|  238177 |  7290 | `	iProtection = GetProtectionLevel(iProtection);` |
|  238177 |  7291 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  238177 |  7292 | `	iFuncFlags = 0;` |
|  238177 |  7293 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7294 | `		/* Invalid method name */` |
|     ! 0 |  7295 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7296 | `		if( rc == SXERR_ABORT ){` |
|       - |  7297 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7298 | `			return SXERR_ABORT;` |
|       - |  7299 | `		}` |
|     ! 0 |  7300 | `		goto Synchronize;` |
|       - |  7301 | `	}` |
|  238177 |  7302 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7303 | `		/* Return by reference,remember that */` |
|     ! 0 |  7304 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7305 | `		/* Jump the '&' token */` |
|     ! 0 |  7306 | `		pGen->pIn++;` |
|     ! 0 |  7307 | `	}` |
|  238177 |  7308 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7309 | `		/* Invalid method name */` |
|     ! 0 |  7310 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7311 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7312 | `			return SXERR_ABORT;` |
|       - |  7313 | `		}` |
|     ! 0 |  7314 | `		goto Synchronize;` |
|       - |  7315 | `	}` |
|       - |  7316 | `	/* Peek method name */` |
|  238177 |  7317 | `	pName = &pGen->pIn->sData;` |
|  238177 |  7318 | `	nLine = pGen->pIn->nLine;` |
|       - |  7319 | `	/* Jump the method name */` |
|  238177 |  7320 | `	pGen->pIn++;` |
|  238177 |  7321 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7322 | `		/* Abstract method */` |
|   82321 |  7323 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7324 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7325 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7326 | `				&pClass->sName,pName);` |
|     ! 0 |  7327 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7328 | `				return SXERR_ABORT;` |
|       - |  7329 | `			}` |
|     ! 0 |  7330 | `		}` |
|       - |  7331 | `		/* Assemble method signature only */` |
|   82321 |  7332 | `		doBody = FALSE;` |
|   41158 |  7333 | `	}` |
|  238177 |  7334 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7335 | `		/* Syntax error */` |
|     ! 0 |  7336 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7337 | `		if( rc == SXERR_ABORT ){` |
|       - |  7338 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7339 | `			return SXERR_ABORT;` |
|       - |  7340 | `		}` |
|     ! 0 |  7341 | `		goto Synchronize;` |
|       - |  7342 | `	}` |
|       - |  7343 | `	/* Allocate a new class_method instance */` |
|  238177 |  7344 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  238177 |  7345 | `	if( pMeth == 0 ){` |
|     ! 0 |  7346 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7347 | `		return SXERR_ABORT;` |
|       - |  7348 | `	}` |
|       - |  7349 | `	/* Jump the left parenthesis '(' */` |
|  238177 |  7350 | `	pGen->pIn++;` |
|  238177 |  7351 | `	pEnd = 0; /* cc warning */` |
|       - |  7352 | `	/* Delimit the method signature */` |
|  238177 |  7353 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  238177 |  7354 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7355 | `		/* Syntax error */` |
|       3 |  7356 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7357 | `		if( rc == SXERR_ABORT ){` |
|       - |  7358 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7359 | `			return SXERR_ABORT;` |
|       - |  7360 | `		}` |
|       3 |  7361 | `		goto Synchronize;` |
|       - |  7362 | `	}` |
|       - |  7363 | `	{` |
|  238175 |  7364 | `		int bIsCtor = 0;` |
|  238175 |  7365 | `		int bAbstractCtor = 0;` |
|  347710 |  7366 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  141320 |  7367 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  228630 |  7368 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   19095 |  7369 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7370 | `				bAbstractCtor = 1;` |
|       2 |  7371 | `			}else{` |
|   19093 |  7372 | `				bIsCtor = 1;` |
|       - |  7373 | `			}` |
|    9545 |  7374 | `		}` |
|  238175 |  7375 | `		if( pGen->pIn < pEnd ){` |
|       - |  7376 | `			/* Collect method arguments */` |
|   54129 |  7377 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   54129 |  7378 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7379 | `				return SXERR_ABORT;` |
|       - |  7380 | `			}` |
|   27062 |  7381 | `		}` |
|       - |  7382 | `	}` |
|       - |  7383 | `	/* Point past ')' and parse optional return type ': type' */` |
|  238175 |  7384 | `	pGen->pIn = &pEnd[1];` |
|       - |  7385 | `	{` |
|  238175 |  7386 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  238175 |  7387 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7388 | `			return SXERR_ABORT;` |
|  238175 |  7389 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7390 | `			goto Synchronize;` |
|       - |  7391 | `		}` |
|       - |  7392 | `	}` |
|       - |  7393 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7394 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7395 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7396 | `	{` |
|  238175 |  7397 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7398 | `		sxu32 i;` |
|  324029 |  7399 | `		for( i = 0; i < nArg; i++ ){` |
|   85867 |  7400 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7401 | `			ph7_class_attr *pAttr;` |
|   85867 |  7402 | `			sxi32 iAttrFlags = 0;` |
|   85867 |  7403 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   85829 |  7404 | `				continue;` |
|       - |  7405 | `			}` |
|      43 |  7406 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7407 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7408 | `					"Cannot declare variadic promoted property");` |
|       3 |  7409 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7410 | `					return SXERR_ABORT;` |
|       - |  7411 | `				}` |
|       3 |  7412 | `				goto Synchronize;` |
|       - |  7413 | `			}` |
|       - |  7414 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7415 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7416 | `			 * appear as an alternative of a union type. */` |
|      36 |  7417 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      11 |  7418 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      56 |  7419 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      34 |  7420 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      34 |  7421 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      17 |  7422 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      39 |  7423 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7424 | `					return SXERR_ABORT;` |
|      39 |  7425 | `				}else if( rc != SXRET_OK ){` |
|       6 |  7426 | `					goto Synchronize;` |
|       - |  7427 | `				}` |
|      15 |  7428 | `			}` |
|       - |  7429 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      36 |  7430 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7431 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7432 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7433 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7434 | `					return SXERR_ABORT;` |
|       - |  7435 | `				}` |
|       3 |  7436 | `				goto Synchronize;` |
|       - |  7437 | `			}` |
|      33 |  7438 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      29 |  7439 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      13 |  7440 | `			}` |
|      33 |  7441 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7442 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7443 | `			}` |
|      33 |  7444 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7445 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7446 | `			}` |
|      33 |  7447 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      33 |  7448 | `			if( pAttr == 0 ){` |
|     ! 0 |  7449 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7450 | `				return SXERR_ABORT;` |
|       - |  7451 | `			}` |
|      33 |  7452 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      29 |  7453 | `				pAttr->nType = pArg->nType;` |
|      29 |  7454 | `				pAttr->sClass = pArg->sClass;` |
|      29 |  7455 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      29 |  7456 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7457 | `					sxu32 k;` |
|     ! 0 |  7458 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7459 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7460 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7461 | `					}` |
|     ! 0 |  7462 | `				}` |
|      13 |  7463 | `			}` |
|      33 |  7464 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      33 |  7465 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7466 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7467 | `				return SXERR_ABORT;` |
|       - |  7468 | `			}` |
|      18 |  7469 | `		}` |
|       - |  7470 | `	}` |
|  238167 |  7471 | `	if( doBody ){` |
|       - |  7472 | `		/* Compile method body */` |
|  155851 |  7473 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  155851 |  7474 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7475 | `			return SXERR_ABORT;` |
|       - |  7476 | `		}` |
|   77928 |  7477 | `	}else{` |
|       - |  7478 | `		/* Only method signature is allowed */` |
|   82321 |  7479 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7480 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7481 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7482 | `				if( rc == SXERR_ABORT ){` |
|       - |  7483 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7484 | `					return SXERR_ABORT;` |
|       - |  7485 | `				}` |
|     ! 0 |  7486 | `				return SXERR_CORRUPT;` |
|       - |  7487 | `			}` |
|       - |  7488 | `	}` |
|       - |  7489 | `	/* All done,install the method */` |
|  238167 |  7490 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  238167 |  7491 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7492 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7493 | `		return SXERR_ABORT;` |
|       - |  7494 | `	}` |
|  238167 |  7495 | `	return SXRET_OK;` |
|       5 |  7496 | `Synchronize:` |
|       - |  7497 | `	/* Synchronize with the first semi-colon */` |
|      34 |  7498 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      24 |  7499 | `		pGen->pIn++;` |
|       4 |  7500 | `	}` |
|      14 |  7501 | `	return SXERR_CORRUPT;` |
|  119091 |  7502 |  |
|       - |  7503 | `/*` |
|       - |  7504 | ` * Compile an object interface.` |
|       - |  7505 | ` *  According to the PHP language reference manual` |
|       - |  7506 | ` *   Object Interfaces:` |
|       - |  7507 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7508 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7509 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7510 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7511 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7512 | ` */` |
|   34852 |  7513 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  7514 |  |
|   34857 |  7515 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7516 | `	ph7_class *pClass,*pBase;` |
|       - |  7517 | `	SyToken *pEnd,*pTmp;` |
|       - |  7518 | `	SyString *pName;` |
|       - |  7519 | `	sxi32 nKwrd;` |
|       - |  7520 | `	sxi32 rc;` |
|       - |  7521 | `	/* Jump the 'interface' keyword */` |
|   34857 |  7522 | `	pGen->pIn++;` |
|       - |  7523 | `	/* Extract interface name */` |
|   34857 |  7524 | `	pName = &pGen->pIn->sData;` |
|       - |  7525 | `	/* Advance the stream cursor */` |
|   34857 |  7526 | `	pGen->pIn++;` |
|       - |  7527 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7528 | `		SyBlob sFQN;` |
|       - |  7529 | `		SyString sFQNStr;` |
|   34857 |  7530 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   34857 |  7531 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   34857 |  7532 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   34857 |  7533 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   34857 |  7534 | `		SyBlobRelease(&sFQN);` |
|       - |  7535 | `	}` |
|   34857 |  7536 | `	if( pClass == 0 ){` |
|     ! 0 |  7537 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7538 | `		return SXERR_ABORT;` |
|       - |  7539 | `	}` |
|       - |  7540 | `	/* Mark as an interface */` |
|   34857 |  7541 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7542 | `	/* Assume no base class is given */` |
|   34857 |  7543 | `	pBase = 0;` |
|   34857 |  7544 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    9505 |  7545 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    9505 |  7546 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7547 | `			SyBlob sResolved;` |
|       - |  7548 | `			SyString sBaseName;` |
|       - |  7549 | `			sxu32 nRefLine;` |
|       - |  7550 | `			/* Extract base interface */` |
|    9505 |  7551 | `			pGen->pIn++;` |
|    9505 |  7552 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    9505 |  7553 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    9505 |  7554 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7555 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7556 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7557 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7558 | `					pName);` |
|     ! 0 |  7559 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7560 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7561 | `					return SXERR_ABORT;` |
|       - |  7562 | `				}` |
|     ! 0 |  7563 | `				return SXRET_OK;` |
|       - |  7564 | `			}` |
|   14255 |  7565 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    9500 |  7566 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    9505 |  7567 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7568 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7569 | `			/* Only interfaces is allowed */` |
|    9505 |  7570 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7571 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7572 | `			}` |
|    9505 |  7573 | `			if( pBase == 0 ){` |
|     ! 0 |  7574 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7575 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  7576 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7577 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7578 | `					return SXERR_ABORT;` |
|       - |  7579 | `				}` |
|     ! 0 |  7580 | `			}` |
|    9505 |  7581 | `			SyBlobRelease(&sResolved);` |
|    4750 |  7582 | `		}` |
|    4750 |  7583 | `	}` |
|   34857 |  7584 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7585 | `		/* Syntax error */` |
|     ! 0 |  7586 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7587 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7588 | `		if( rc == SXERR_ABORT ){` |
|       - |  7589 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7590 | `			return SXERR_ABORT;` |
|       - |  7591 | `		}` |
|     ! 0 |  7592 | `		return SXRET_OK;` |
|       - |  7593 | `	}` |
|   34857 |  7594 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   34857 |  7595 | `	pEnd = 0; /* cc warning */` |
|       - |  7596 | `	/* Delimit the interface body */` |
|   34857 |  7597 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   34857 |  7598 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7599 | `		/* Syntax error */` |
|     ! 0 |  7600 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7601 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7602 | `		if( rc == SXERR_ABORT ){` |
|       - |  7603 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7604 | `			return SXERR_ABORT;` |
|       - |  7605 | `		}` |
|     ! 0 |  7606 | `		return SXRET_OK;` |
|       - |  7607 | `	}` |
|       - |  7608 | `	/* Swap token stream */` |
|   34857 |  7609 | `	pTmp = pGen->pEnd;` |
|   34857 |  7610 | `	pGen->pEnd = pEnd;` |
|       - |  7611 | `	/* Start the parse process` |
|       - |  7612 | `	 * Note (According to the PHP reference manual):` |
|       - |  7613 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7614 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7615 | `	 */` |
|   58580 |  7616 | `	for(;;){` |
|       - |  7617 | `		/* Jump leading/trailing semi-colons */` |
|  199473 |  7618 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   82313 |  7619 | `			pGen->pIn++;` |
|       5 |  7620 | `		}` |
|  117165 |  7621 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7622 | `			/* End of interface body */` |
|   34855 |  7623 | `			break;` |
|       - |  7624 | `		}` |
|   82315 |  7625 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7626 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7627 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7628 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7629 | `			if( rc == SXERR_ABORT ){` |
|       - |  7630 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7631 | `				return SXERR_ABORT;` |
|       - |  7632 | `			}` |
|     ! 0 |  7633 | `			goto done;` |
|       - |  7634 | `		}` |
|       - |  7635 | `		/* Extract the current keyword */` |
|   82315 |  7636 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   82315 |  7637 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7638 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7639 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7640 | `			const char *zKind = "member";` |
|       3 |  7641 | `			SyString *pMemberName = 0;` |
|       3 |  7642 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7643 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7644 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7645 | `					zKind = "constant";` |
|       3 |  7646 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7647 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7648 | `					}` |
|       1 |  7649 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7650 | `					zKind = "method";` |
|     ! 0 |  7651 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7652 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7653 | `					}` |
|     ! 0 |  7654 | `				}` |
|       1 |  7655 | `			}` |
|       3 |  7656 | `			if( pMemberName ){` |
|       4 |  7657 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7658 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7659 | `			}else{` |
|     ! 0 |  7660 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7661 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7662 | `			}` |
|       3 |  7663 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7664 | `				return SXERR_ABORT;` |
|       - |  7665 | `			}` |
|       3 |  7666 | `			goto done;` |
|       - |  7667 | `		}` |
|   82313 |  7668 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7669 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7670 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7671 | `			if( rc == SXERR_ABORT ){` |
|       - |  7672 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7673 | `				return SXERR_ABORT;` |
|       - |  7674 | `			}` |
|     ! 0 |  7675 | `			goto done;` |
|       - |  7676 | `		}` |
|   82313 |  7677 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7678 | `			/* Advance the stream cursor */` |
|   82305 |  7679 | `			pGen->pIn++;` |
|   82305 |  7680 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7681 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7682 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7683 | `				if( rc == SXERR_ABORT ){` |
|       - |  7684 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7685 | `					return SXERR_ABORT;` |
|       - |  7686 | `				}` |
|     ! 0 |  7687 | `				goto done;` |
|       - |  7688 | `			}` |
|   82305 |  7689 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   82305 |  7690 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7691 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7692 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7693 | `				if( rc == SXERR_ABORT ){` |
|       - |  7694 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7695 | `					return SXERR_ABORT;` |
|       - |  7696 | `				}` |
|     ! 0 |  7697 | `				goto done;` |
|       - |  7698 | `			}` |
|   41150 |  7699 | `		}` |
|   82313 |  7700 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7701 | `			/* Parse constant */` |
|       7 |  7702 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       7 |  7703 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7704 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7705 | `					return SXERR_ABORT;` |
|       - |  7706 | `				}` |
|     ! 0 |  7707 | `				goto done;` |
|       - |  7708 | `			}` |
|       4 |  7709 | `		}else{` |
|   82307 |  7710 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   82307 |  7711 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7712 | `				/* Static method,record that */` |
|    9497 |  7713 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7714 | `				/* Advance the stream cursor */` |
|    9497 |  7715 | `				pGen->pIn++;` |
|    9492 |  7716 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    9497 |  7717 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7718 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7719 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7720 | `						if( rc == SXERR_ABORT ){` |
|       - |  7721 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7722 | `							return SXERR_ABORT;` |
|       - |  7723 | `						}` |
|     ! 0 |  7724 | `						goto done;` |
|       - |  7725 | `				}` |
|    4746 |  7726 | `			}` |
|       - |  7727 | `			/* Process method signature (no body for interface methods) */` |
|   82307 |  7728 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   82307 |  7729 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7730 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7731 | `					return SXERR_ABORT;` |
|       - |  7732 | `				}` |
|     ! 0 |  7733 | `				goto done;` |
|       - |  7734 | `			}` |
|       - |  7735 | `		}` |
|       5 |  7736 | `	}` |
|       - |  7737 | `	/* Install the interface */` |
|   34855 |  7738 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   34855 |  7739 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7740 | `		/* Inherit from the base interface */` |
|    9505 |  7741 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    4750 |  7742 | `	}` |
|   34855 |  7743 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7744 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7745 | `		return SXERR_ABORT;` |
|       - |  7746 | `	}` |
|   17425 |  7747 | `done:` |
|       - |  7748 | `	/* Point beyond the interface body */` |
|   34857 |  7749 | `	pGen->pIn  = &pEnd[1];` |
|   34857 |  7750 | `	pGen->pEnd = pTmp;` |
|   34857 |  7751 | `	return PH7_OK;` |
|   17431 |  7752 |  |
|       - |  7753 | `/*` |
|       - |  7754 | ` * Compile a user-defined class.` |
|       - |  7755 | ` * According to the PHP language reference manual` |
|       - |  7756 | ` *  class` |
|       - |  7757 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7758 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7759 | ` *  of the properties and methods belonging to the class.` |
|       - |  7760 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7761 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7762 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7763 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7764 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7765 | ` *  (called "methods").` |
|       - |  7766 | ` */` |
|       - |  7767 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7768 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7769 | `struct TraitUseEntry {` |
|       - |  7770 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7771 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7772 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7773 | `};` |
|       - |  7774 | `/*` |
|       - |  7775 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7776 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7777 | ` */` |
|   86384 |  7778 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  7779 |  |
|       - |  7780 | `	ph7_class **apIface;` |
|       - |  7781 | `	sxu32 nIface,i;` |
|       - |  7782 | `	sxi32 rc;` |
|   86389 |  7783 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7784 | `		return SXRET_OK;` |
|       - |  7785 | `	}` |
|   86389 |  7786 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   86389 |  7787 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  172013 |  7788 | `	for(i = 0; i < nIface; i++){` |
|   85629 |  7789 | `		ph7_class *pIface = apIface[i];` |
|       - |  7790 | `		SyHashEntry *pEntry;` |
|   85629 |  7791 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  228411 |  7792 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  142787 |  7793 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7794 | `			ph7_class_method *pImplMeth;` |
|  142787 |  7795 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7796 | `			/* Find the implementing method in the class */` |
|  142787 |  7797 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  142787 |  7798 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  7799 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  7800 | `			}` |
|       - |  7801 | `			/* Check visibility: interface methods must be implemented as public */` |
|  142773 |  7802 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  7803 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7804 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  7805 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  7806 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7807 | `					return SXERR_ABORT;` |
|       - |  7808 | `				}` |
|       1 |  7809 | `			}` |
|       - |  7810 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7811 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7812 | `			 */` |
|       - |  7813 | `			{` |
|  142773 |  7814 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  142773 |  7815 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  142773 |  7816 | `				int sigError = 0;` |
|  142773 |  7817 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7818 | `					sigError = 1;` |
|  142772 |  7819 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7820 | `					/* Extra parameters must all have default values */` |
|       6 |  7821 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7822 | `					sxu32 k;` |
|       8 |  7823 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  7824 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7825 | `							sigError = 1;` |
|       3 |  7826 | `							break;` |
|       - |  7827 | `						}` |
|       2 |  7828 | `					}` |
|       2 |  7829 | `				}` |
|  142773 |  7830 | `				if( sigError ){` |
|       - |  7831 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7832 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7833 | `					sxu32 j;` |
|       6 |  7834 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  7835 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7836 | `					/* Build implementing method signature */` |
|       6 |  7837 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  7838 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  7839 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  7840 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  7841 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  7842 | `					}` |
|       - |  7843 | `					/* Build interface method signature */` |
|       6 |  7844 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  7845 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  7846 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  7847 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  7848 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  7849 | `					}` |
|       8 |  7850 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7851 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7852 | `						&pClass->sName,pMName,` |
|       4 |  7853 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7854 | `						&pIface->sName,pMName,` |
|       4 |  7855 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  7856 | `					SyBlobRelease(&sImplSig);` |
|       6 |  7857 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  7858 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7859 | `						return SXERR_ABORT;` |
|       - |  7860 | `					}` |
|       2 |  7861 | `				}` |
|       - |  7862 | `			}` |
|       5 |  7863 | `		}` |
|   42817 |  7864 | `	}` |
|   86389 |  7865 | `	return SXRET_OK;` |
|   43197 |  7866 |  |
|       - |  7867 | `/*` |
|       - |  7868 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7869 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7870 | ` */` |
|   86384 |  7871 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  7872 |  |
|       - |  7873 | `	ph7_class_method *pMeth;` |
|       - |  7874 | `	SyHashEntry *pEntry;` |
|       - |  7875 | `	sxu32 nAbstract;` |
|       - |  7876 | `	SyBlob sMsg;` |
|       - |  7877 | `	sxi32 rc;` |
|       - |  7878 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   86389 |  7879 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      27 |  7880 | `		return SXRET_OK;` |
|       - |  7881 | `	}` |
|       - |  7882 | `	/* Count abstract methods */` |
|   86367 |  7883 | `	nAbstract = 0;` |
|   86367 |  7884 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  837621 |  7885 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  751259 |  7886 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  751259 |  7887 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  7888 | `			nAbstract++;` |
|       8 |  7889 | `		}` |
|       5 |  7890 | `	}` |
|   86367 |  7891 | `	if( nAbstract == 0 ){` |
|   86353 |  7892 | `		return SXRET_OK;` |
|       - |  7893 | `	}` |
|       - |  7894 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  7895 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  7896 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7897 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7898 | `		&pClass->sName,nAbstract,` |
|       7 |  7899 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7900 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7901 | `	/* Second pass: list methods with origins */` |
|       - |  7902 | `	{` |
|      18 |  7903 | `		sxu32 nListed = 0;` |
|      18 |  7904 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  7905 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  7906 | `			ph7_class *pOrigin = 0;` |
|       - |  7907 | `			SyString *pMName;` |
|      22 |  7908 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  7909 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7910 | `				continue;` |
|       - |  7911 | `			}` |
|      20 |  7912 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  7913 | `			if( nListed > 0 ){` |
|       3 |  7914 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7915 | `			}` |
|       - |  7916 | `			/* Find the origin of this abstract method.` |
|       - |  7917 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7918 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7919 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7920 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7921 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7922 | `			 * class's namespace.` |
|       - |  7923 | `			 */` |
|       - |  7924 | `			{` |
|       - |  7925 | `				ph7_class **apIface;` |
|       - |  7926 | `				ph7_class **apTrait;` |
|       - |  7927 | `				ph7_class *pWalk;` |
|       - |  7928 | `				sxu32 i;` |
|       - |  7929 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7930 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  7931 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  7932 | `				 */` |
|      20 |  7933 | `				if( pClass->pBase ){` |
|      11 |  7934 | `					pWalk = pClass->pBase;` |
|      19 |  7935 | `					while( pWalk ){` |
|       - |  7936 | `						ph7_class_method *pParentMeth;` |
|      13 |  7937 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  7938 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  7939 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  7940 | `							 * in this class's ancestor chain.` |
|       - |  7941 | `							 */` |
|      13 |  7942 | `							int fromIface = 0;` |
|      13 |  7943 | `							ph7_class *pAnc = pWalk;` |
|      17 |  7944 | `							while( pAnc ){` |
|       - |  7945 | `								ph7_class **apPI;` |
|       - |  7946 | `								sxu32 j;` |
|      15 |  7947 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  7948 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  7949 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  7950 | `										fromIface = 1;` |
|      10 |  7951 | `										break;` |
|       - |  7952 | `									}` |
|     ! 0 |  7953 | `								}` |
|      15 |  7954 | `								if( fromIface ) break;` |
|       6 |  7955 | `								pAnc = pAnc->pBase;` |
|       2 |  7956 | `							}` |
|      13 |  7957 | `							if( !fromIface ){` |
|       3 |  7958 | `								pOrigin = pWalk;` |
|       3 |  7959 | `								break;` |
|       - |  7960 | `							}` |
|       4 |  7961 | `						}` |
|      10 |  7962 | `						pWalk = pWalk->pBase;` |
|       2 |  7963 | `					}` |
|       4 |  7964 | `				}` |
|       - |  7965 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  7966 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  7967 | `				 */` |
|      20 |  7968 | `				if( !pOrigin ){` |
|      18 |  7969 | `					pWalk = pClass;` |
|      40 |  7970 | `					while( pWalk && !pOrigin ){` |
|      26 |  7971 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  7972 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  7973 | `							ph7_class *pIface = apIface[i];` |
|      16 |  7974 | `							ph7_class *pDeepest = 0;` |
|      28 |  7975 | `							while( pIface ){` |
|      16 |  7976 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  7977 | `									pDeepest = pIface;` |
|       6 |  7978 | `								}` |
|      16 |  7979 | `								pIface = pIface->pBase;` |
|       4 |  7980 | `							}` |
|      16 |  7981 | `							if( pDeepest ){` |
|      16 |  7982 | `								pOrigin = pDeepest;` |
|      16 |  7983 | `								break;` |
|       - |  7984 | `							}` |
|     ! 0 |  7985 | `						}` |
|      26 |  7986 | `						pWalk = pWalk->pBase;` |
|       4 |  7987 | `					}` |
|       7 |  7988 | `				}` |
|       - |  7989 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  7990 | `				if( !pOrigin ){` |
|       3 |  7991 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  7992 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  7993 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  7994 | `							pOrigin = pClass;` |
|       3 |  7995 | `							break;` |
|       - |  7996 | `						}` |
|     ! 0 |  7997 | `					}` |
|       1 |  7998 | `				}` |
|       - |  7999 | `			}` |
|      20 |  8000 | `			if( pOrigin ){` |
|      20 |  8001 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8002 | `			}else{` |
|       - |  8003 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8004 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8005 | `			}` |
|      20 |  8006 | `			nListed++;` |
|       4 |  8007 | `		}` |
|       - |  8008 | `	}` |
|      18 |  8009 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8010 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8011 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8012 | `	SyBlobRelease(&sMsg);` |
|      18 |  8013 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8014 | `		return SXERR_ABORT;` |
|       - |  8015 | `	}` |
|      18 |  8016 | `	return SXRET_OK;` |
|   43197 |  8017 |  |
|       - |  8018 | `/*` |
|       - |  8019 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8020 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8021 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8022 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8023 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8024 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8025 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8026 | ` */` |
|   86138 |  8027 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8028 |  |
|   86143 |  8029 | `	int isAbsolute = 0;` |
|   86143 |  8030 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8031 | `	SyBlob sName;` |
|   86143 |  8032 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      33 |  8033 | `		isAbsolute = 1;` |
|      33 |  8034 | `		pGen->pIn++;` |
|      15 |  8035 | `	}` |
|   86143 |  8036 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       9 |  8037 | `		pGen->pIn = pStart;` |
|       9 |  8038 | `		return SXERR_INVALID;` |
|       - |  8039 | `	}` |
|   86137 |  8040 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   86137 |  8041 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   86137 |  8042 | `	pGen->pIn++;` |
|  129216 |  8043 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   43089 |  8044 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8045 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8046 | `		pGen->pIn++;` |
|      13 |  8047 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8048 | `		pGen->pIn++;` |
|       1 |  8049 | `	}` |
|   86137 |  8050 | `	if( isAbsolute ){` |
|      30 |  8051 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      16 |  8052 | `	}else{` |
|       - |  8053 | `		SyString sRaw;` |
|   86109 |  8054 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   86109 |  8055 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8056 | `	}` |
|   86137 |  8057 | `	SyBlobRelease(&sName);` |
|   86137 |  8058 | `	return SXRET_OK;` |
|   43074 |  8059 |  |
|       - |  8060 | `/*` |
|       - |  8061 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8062 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8063 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8064 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8065 | ` * either direction cannot run unbounded.` |
|       - |  8066 | ` */` |
|       - |  8067 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    9614 |  8068 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8069 |  |
|       - |  8070 | `	ph7_class **apParent;` |
|       - |  8071 | `	sxu32 n;` |
|   16091 |  8072 | `	while( pInterface ){` |
|   12815 |  8073 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8074 | `			return FALSE;` |
|       - |  8075 | `		}` |
|   15991 |  8076 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    6352 |  8077 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    6343 |  8078 | `			return TRUE;` |
|       - |  8079 | `		}` |
|    6477 |  8080 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    6477 |  8081 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8082 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8083 | `				return TRUE;` |
|       - |  8084 | `			}` |
|     ! 0 |  8085 | `		}` |
|    6477 |  8086 | `		pInterface = pInterface->pBase;` |
|    6477 |  8087 | `		iDepth++;` |
|       5 |  8088 | `	}` |
|    3281 |  8089 | `	return FALSE;` |
|    4812 |  8090 |  |
|    9614 |  8091 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8092 |  |
|    9619 |  8093 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8094 |  |
|       - |  8095 | `/*` |
|       - |  8096 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8097 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8098 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8099 | ` */` |
|    6338 |  8100 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8101 |  |
|    6347 |  8102 | `	while( pBase ){` |
|      10 |  8103 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8104 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8105 | `			return TRUE;` |
|       - |  8106 | `		}` |
|      10 |  8107 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8108 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8109 | `			return TRUE;` |
|       - |  8110 | `		}` |
|       5 |  8111 | `		pBase = pBase->pBase;` |
|       1 |  8112 | `	}` |
|    6339 |  8113 | `	return FALSE;` |
|    3174 |  8114 |  |
|   86402 |  8115 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  8116 |  |
|   86407 |  8117 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8118 | `	ph7_class *pClass,*pBase;` |
|       - |  8119 | `	SyToken *pEnd,*pTmp;` |
|       - |  8120 | `	sxi32 iProtection;` |
|       - |  8121 | `	SySet aInterfaces;` |
|       - |  8122 | `	SySet aUseEntries;` |
|       - |  8123 | `	sxi32 iAttrflags;` |
|       - |  8124 | `	SyString *pName;` |
|       - |  8125 | `	sxi32 nKwrd;` |
|       - |  8126 | `	sxi32 rc;` |
|       - |  8127 | `	/* Jump the 'class' keyword */` |
|   86407 |  8128 | `	pGen->pIn++;` |
|   86407 |  8129 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8130 | `		/* Syntax error */` |
|     ! 0 |  8131 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8132 | `		if( rc == SXERR_ABORT ){` |
|       - |  8133 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8134 | `			return SXERR_ABORT;` |
|       - |  8135 | `		}` |
|       - |  8136 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8137 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8138 | `			pGen->pIn++;` |
|     ! 0 |  8139 | `		}` |
|     ! 0 |  8140 | `		return SXRET_OK;` |
|       - |  8141 | `	}` |
|       - |  8142 | `	/* Extract class name */` |
|   86407 |  8143 | `	pName = &pGen->pIn->sData;` |
|       - |  8144 | `	/* Advance the stream cursor */` |
|   86407 |  8145 | `	pGen->pIn++;` |
|       - |  8146 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8147 | `		SyBlob sFQN;` |
|       - |  8148 | `		SyString sFQNStr;` |
|   86407 |  8149 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   86407 |  8150 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   86407 |  8151 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   86407 |  8152 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   86407 |  8153 | `		SyBlobRelease(&sFQN);` |
|       - |  8154 | `	}` |
|   86407 |  8155 | `	if( pClass == 0 ){` |
|     ! 0 |  8156 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8157 | `		return SXERR_ABORT;` |
|       - |  8158 | `	}` |
|       - |  8159 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   86407 |  8160 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   86407 |  8161 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8162 | `	/* Assume a standalone class */` |
|   86407 |  8163 | `	pBase = 0;` |
|   86407 |  8164 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   76203 |  8165 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   76203 |  8166 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8167 | `			SyBlob sResolved;` |
|       - |  8168 | `			SyString sBaseName;` |
|       - |  8169 | `			sxu32 nRefLine;` |
|   66595 |  8170 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   66595 |  8171 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   66595 |  8172 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   66595 |  8173 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8174 | `				SyBlobRelease(&sResolved);` |
|       4 |  8175 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8176 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8177 | `					pName);` |
|       3 |  8178 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8179 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8180 | `					return SXERR_ABORT;` |
|       - |  8181 | `				}` |
|       3 |  8182 | `				return SXRET_OK;` |
|       - |  8183 | `			}` |
|   99887 |  8184 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   66588 |  8185 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   66593 |  8186 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8187 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8188 | `			/* Interfaces are not allowed */` |
|   66593 |  8189 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8190 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8191 | `			}` |
|   66593 |  8192 | `			if( pBase == 0 ){` |
|     ! 0 |  8193 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8194 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8195 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8196 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8197 | `					return SXERR_ABORT;` |
|       - |  8198 | `				}` |
|     ! 0 |  8199 | `			}else{` |
|   66593 |  8200 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8201 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8202 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8203 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8204 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8205 | `						return SXERR_ABORT;` |
|       - |  8206 | `					}` |
|     ! 0 |  8207 | `				}` |
|       - |  8208 | `			}` |
|   66593 |  8209 | `			SyBlobRelease(&sResolved);` |
|   33294 |  8210 | `		}` |
|   76201 |  8211 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8212 | `			ph7_class *pInterface;` |
|       - |  8213 | `			/* Interface implementation */` |
|    9619 |  8214 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    4807 |  8215 | `			for(;;){` |
|       - |  8216 | `				SyBlob sResolved;` |
|       - |  8217 | `				SyString sIntName;` |
|       - |  8218 | `				sxu32 nRefLine;` |
|    9619 |  8219 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    9619 |  8220 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    9619 |  8221 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8222 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8223 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8224 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8225 | `						pName);` |
|     ! 0 |  8226 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8227 | `						return SXERR_ABORT;` |
|       - |  8228 | `					}` |
|     ! 0 |  8229 | `					break;` |
|       - |  8230 | `				}` |
|   19233 |  8231 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    9614 |  8232 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    9619 |  8233 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8234 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8235 | `				/* Only interfaces are allowed */` |
|    9619 |  8236 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8237 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8238 | `				}` |
|    9619 |  8239 | `				if( pInterface == 0 ){` |
|     ! 0 |  8240 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8241 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8242 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8243 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8244 | `						return SXERR_ABORT;` |
|       - |  8245 | `					}` |
|     ! 0 |  8246 | `				}else{` |
|       - |  8247 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8248 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8249 | `					 * unless they already extend Exception or Error.` |
|       - |  8250 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8251 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8252 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    9619 |  8253 | `					SyString *pFqn = &pClass->sName;` |
|    9619 |  8254 | `					int bIsExceptionOrError =` |
|    7973 |  8255 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   16005 |  8256 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    8037 |  8257 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3174 |  8258 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   15950 |  8259 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    9510 |  8260 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3167 |  8261 | `						!bIsExceptionOrError ){` |
|      12 |  8262 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8263 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8264 | `							&pClass->sName);` |
|       9 |  8265 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8266 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8267 | `							return SXERR_ABORT;` |
|       - |  8268 | `						}` |
|       - |  8269 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8270 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8271 | `					}else{` |
|    9613 |  8272 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8273 | `					}` |
|       - |  8274 | `				}` |
|    9619 |  8275 | `				SyBlobRelease(&sResolved);` |
|    9619 |  8276 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    4812 |  8277 | `					break;` |
|       - |  8278 | `				}` |
|     ! 0 |  8279 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  8280 | `			}` |
|    4807 |  8281 | `		}` |
|   38098 |  8282 | `	}` |
|   86405 |  8283 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8284 | `		/* Syntax error */` |
|     ! 0 |  8285 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8286 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8287 | `		if( rc == SXERR_ABORT ){` |
|       - |  8288 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8289 | `			return SXERR_ABORT;` |
|       - |  8290 | `		}` |
|     ! 0 |  8291 | `		return SXRET_OK;` |
|       - |  8292 | `	}` |
|   86405 |  8293 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   86405 |  8294 | `	pEnd = 0; /* cc warning */` |
|       - |  8295 | `	/* Delimit the class body */` |
|   86405 |  8296 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   86405 |  8297 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8298 | `		/* Syntax error */` |
|     ! 0 |  8299 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8300 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8301 | `		if( rc == SXERR_ABORT ){` |
|       - |  8302 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8303 | `			return SXERR_ABORT;` |
|       - |  8304 | `		}` |
|     ! 0 |  8305 | `		return SXRET_OK;` |
|       - |  8306 | `	}` |
|       - |  8307 | `	/* Swap token stream */` |
|   86405 |  8308 | `	pTmp = pGen->pEnd;` |
|   86405 |  8309 | `	pGen->pEnd = pEnd;` |
|       - |  8310 | `	/* Set the inherited flags */` |
|   86405 |  8311 | `	pClass->iFlags = iFlags;` |
|       - |  8312 | `	/* Start the parse process */` |
|  121134 |  8313 | `	for(;;){` |
|       - |  8314 | `		/* Jump leading/trailing semi-colons */` |
|  363651 |  8315 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   60727 |  8316 | `			pGen->pIn++;` |
|       5 |  8317 | `		}` |
|  302929 |  8318 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8319 | `			/* End of class body */` |
|   86389 |  8320 | `			break;` |
|       - |  8321 | `		}` |
|  216545 |  8322 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8323 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8324 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8325 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8326 | `			if( rc == SXERR_ABORT ){` |
|       - |  8327 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8328 | `				return SXERR_ABORT;` |
|       - |  8329 | `			}` |
|     ! 0 |  8330 | `			goto done;` |
|       - |  8331 | `		}` |
|       - |  8332 | `		/* Assume public visibility */` |
|  216545 |  8333 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  216545 |  8334 | `		iAttrflags = 0;` |
|  216545 |  8335 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  8336 | `			/* Extract the current keyword */` |
|  216545 |  8337 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  216545 |  8338 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8339 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8340 | `				TraitUseEntry sUse;` |
|      49 |  8341 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      49 |  8342 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      49 |  8343 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      30 |  8344 | `				for(;;){` |
|       - |  8345 | `					ph7_class *pTrait;` |
|       - |  8346 | `					SyString *pTraitName;` |
|      57 |  8347 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8348 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8349 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8350 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8351 | `							return SXERR_ABORT;` |
|       - |  8352 | `						}` |
|     ! 0 |  8353 | `						break;` |
|       - |  8354 | `					}` |
|      57 |  8355 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8356 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8357 | `						SyBlob sResolved;` |
|      57 |  8358 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      57 |  8359 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     109 |  8360 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      52 |  8361 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      57 |  8362 | `						SyBlobRelease(&sResolved);` |
|       - |  8363 | `					}` |
|       - |  8364 | `					/* Only traits are allowed */` |
|      57 |  8365 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8366 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8367 | `					}` |
|      57 |  8368 | `					if( pTrait == 0 ){` |
|     ! 0 |  8369 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8370 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8371 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8372 | `							return SXERR_ABORT;` |
|       - |  8373 | `						}` |
|     ! 0 |  8374 | `					}else{` |
|      57 |  8375 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8376 | `					}` |
|      57 |  8377 | `					pGen->pIn++; /* Advance past trait name */` |
|      57 |  8378 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      27 |  8379 | `						break;` |
|       - |  8380 | `					}` |
|      10 |  8381 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  8382 | `				}` |
|       - |  8383 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      49 |  8384 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8385 | `					SyToken *pBlock;` |
|      10 |  8386 | `					pGen->pIn++; /* Jump '{' */` |
|      10 |  8387 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      10 |  8388 | `					sUse.pResolvStart = pGen->pIn;` |
|      10 |  8389 | `					sUse.pResolvEnd = pBlock;` |
|      10 |  8390 | `					if( pBlock < pGen->pEnd ){` |
|      10 |  8391 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       6 |  8392 | `					}else{` |
|     ! 0 |  8393 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8394 | `					}` |
|       4 |  8395 | `				}` |
|      49 |  8396 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8397 | `				/* The semicolon will be consumed by the outer loop */` |
|      49 |  8398 | `				continue;` |
|       - |  8399 | `			}` |
|  216501 |  8400 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  213175 |  8401 | `				iProtection = nKwrd;` |
|  213175 |  8402 | `				pGen->pIn++; /* Jump the visibility token */` |
|  213170 |  8403 | `				if( pGen->pIn >= pGen->pEnd` |
|  213175 |  8404 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8405 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8406 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8407 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8408 | `					if( rc == SXERR_ABORT ){` |
|       - |  8409 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8410 | `						return SXERR_ABORT;` |
|       - |  8411 | `					}` |
|     ! 0 |  8412 | `					goto done;` |
|       - |  8413 | `				}` |
|  213175 |  8414 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8415 | `					/* Attribute declaration (untyped) */` |
|   60475 |  8416 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   60475 |  8417 | `					if( rc != SXRET_OK ){` |
|       3 |  8418 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8419 | `							return SXERR_ABORT;` |
|       - |  8420 | `						}` |
|       3 |  8421 | `						goto done;` |
|       - |  8422 | `					}` |
|   60473 |  8423 | `					continue;` |
|       - |  8424 | `				}` |
|  152705 |  8425 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8426 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     127 |  8427 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     127 |  8428 | `					if( rc != SXRET_OK ){` |
|       3 |  8429 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8430 | `							return SXERR_ABORT;` |
|       - |  8431 | `						}` |
|       3 |  8432 | `						goto done;` |
|       - |  8433 | `					}` |
|     125 |  8434 | `					continue;` |
|       - |  8435 | `				}` |
|       - |  8436 | `				/* Extract the keyword */` |
|  152583 |  8437 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   76289 |  8438 | `			}` |
|  155909 |  8439 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8440 | `				/* Process constant declaration */` |
|      65 |  8441 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      65 |  8442 | `				if( rc != SXRET_OK ){` |
|       3 |  8443 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8444 | `						return SXERR_ABORT;` |
|       - |  8445 | `					}` |
|       3 |  8446 | `					goto done;` |
|       - |  8447 | `				}` |
|      34 |  8448 | `			}else{` |
|  155849 |  8449 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8450 | `					/* Static method or attribute,record that */` |
|    3209 |  8451 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    3209 |  8452 | `					pGen->pIn++; /* Jump the static keyword */` |
|    3209 |  8453 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8454 | `						/* Extract the keyword */` |
|    3203 |  8455 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    3203 |  8456 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8457 | `							iProtection = nKwrd;` |
|     ! 0 |  8458 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8459 | `						}` |
|    1599 |  8460 | `					}` |
|    3204 |  8461 | `					if( pGen->pIn >= pGen->pEnd` |
|    3209 |  8462 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8463 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8464 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8465 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8466 | `						if( rc == SXERR_ABORT ){` |
|       - |  8467 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8468 | `							return SXERR_ABORT;` |
|       - |  8469 | `						}` |
|     ! 0 |  8470 | `						goto done;` |
|       - |  8471 | `					}` |
|    3209 |  8472 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8473 | `						/* Attribute declaration */` |
|       5 |  8474 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8475 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8476 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8477 | `								return SXERR_ABORT;` |
|       - |  8478 | `							}` |
|     ! 0 |  8479 | `							goto done;` |
|       - |  8480 | `						}` |
|       5 |  8481 | `						continue;` |
|       - |  8482 | `					}` |
|    3205 |  8483 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8484 | `						/* Typed static attribute declaration */` |
|      13 |  8485 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      13 |  8486 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8487 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8488 | `								return SXERR_ABORT;` |
|       - |  8489 | `							}` |
|     ! 0 |  8490 | `							goto done;` |
|       - |  8491 | `						}` |
|      13 |  8492 | `						continue;` |
|       - |  8493 | `					}` |
|       - |  8494 | `					/* Extract the keyword */` |
|    3195 |  8495 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  154240 |  8496 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8497 | `					/* Abstract method,record that */` |
|      12 |  8498 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8499 | `					/* Mark the whole class as abstract */` |
|      12 |  8500 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8501 | `					/* Advance the stream cursor */` |
|      12 |  8502 | `					pGen->pIn++;` |
|      12 |  8503 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8504 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8505 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8506 | `							iProtection = nKwrd;` |
|      10 |  8507 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8508 | `						}` |
|       5 |  8509 | `					}` |
|      12 |  8510 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8511 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8512 | `							/* Static method */` |
|     ! 0 |  8513 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8514 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8515 | `					}` |
|      12 |  8516 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8517 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8518 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8519 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8520 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8521 | `							if( rc == SXERR_ABORT ){` |
|       - |  8522 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8523 | `								return SXERR_ABORT;` |
|       - |  8524 | `							}` |
|     ! 0 |  8525 | `							goto done;` |
|       - |  8526 | `					}` |
|      12 |  8527 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  152640 |  8528 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8529 | `					/* final method ,record that */` |
|      18 |  8530 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      18 |  8531 | `					pGen->pIn++; /* Jump the final keyword */` |
|      18 |  8532 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8533 | `						/* Extract the keyword */` |
|      18 |  8534 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      18 |  8535 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  8536 | `							iProtection = nKwrd;` |
|       9 |  8537 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  8538 | `						}` |
|       7 |  8539 | `					}` |
|      18 |  8540 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  8541 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  8542 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  8543 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  8544 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  8545 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  8546 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  8547 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  8548 | `									return SXERR_ABORT;` |
|       - |  8549 | `								}` |
|     ! 0 |  8550 | `								goto done;` |
|       - |  8551 | `							}` |
|      12 |  8552 | `							continue;` |
|       - |  8553 | `					}` |
|       6 |  8554 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8555 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8556 | `							/* Static method */` |
|     ! 0 |  8557 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8558 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8559 | `					}` |
|       6 |  8560 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8561 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8562 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8563 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8564 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8565 | `							if( rc == SXERR_ABORT ){` |
|       - |  8566 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8567 | `								return SXERR_ABORT;` |
|       - |  8568 | `							}` |
|     ! 0 |  8569 | `							goto done;` |
|       - |  8570 | `					}` |
|       6 |  8571 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8572 | `				}` |
|  155825 |  8573 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8574 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8575 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8576 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8577 | `						if( rc == SXERR_ABORT ){` |
|       - |  8578 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8579 | `							return SXERR_ABORT;` |
|       - |  8580 | `						}` |
|     ! 0 |  8581 | `						goto done;` |
|       - |  8582 | `				}` |
|  155825 |  8583 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8584 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8585 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8586 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8587 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8588 | `						if( rc == SXERR_ABORT ){` |
|       - |  8589 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8590 | `							return SXERR_ABORT;` |
|       - |  8591 | `						}` |
|     ! 0 |  8592 | `						goto done;` |
|       - |  8593 | `					}` |
|       - |  8594 | `					/* Attribute declaration */` |
|       7 |  8595 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8596 | `				}else{` |
|       - |  8597 | `					/* Process method declaration */` |
|  155819 |  8598 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8599 | `				}` |
|  155825 |  8600 | `				if( rc != SXRET_OK ){` |
|      14 |  8601 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8602 | `						return SXERR_ABORT;` |
|       - |  8603 | `					}` |
|      14 |  8604 | `					goto done;` |
|       - |  8605 | `				}` |
|       - |  8606 | `			}` |
|   77939 |  8607 | `		}else{` |
|       - |  8608 | `			/* Attribute declaration */` |
|     ! 0 |  8609 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8610 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8611 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8612 | `					return SXERR_ABORT;` |
|       - |  8613 | `				}` |
|     ! 0 |  8614 | `				goto done;` |
|       - |  8615 | `			}` |
|       - |  8616 | `		}` |
|       5 |  8617 | `	}` |
|       - |  8618 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8619 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8620 | `	 */` |
|       - |  8621 | `	{` |
|       - |  8622 | `		TraitUseEntry *apUse;` |
|       - |  8623 | `		sxu32 nU;` |
|   86389 |  8624 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   86433 |  8625 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      49 |  8626 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      49 |  8627 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      49 |  8628 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      49 |  8629 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8630 | `			sxu32 nT;` |
|      49 |  8631 | `			if( !hasResolution ){` |
|       - |  8632 | `				/* No conflict resolution block: use standard trait application */` |
|      83 |  8633 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      47 |  8634 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      47 |  8635 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8636 | `						break;` |
|       - |  8637 | `					}` |
|      26 |  8638 | `				}` |
|      23 |  8639 | `			}else{` |
|       - |  8640 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8641 | `				 * then use the block to resolve method conflicts.` |
|       - |  8642 | `				 */` |
|       - |  8643 | `				SyToken *pR;` |
|      20 |  8644 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      12 |  8645 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8646 | `					ph7_class_attr *pAR;` |
|       - |  8647 | `					SyHashEntry *pER;` |
|       - |  8648 | `					SyString *pNR;` |
|      12 |  8649 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      17 |  8650 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8651 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8652 | `						pNR = &pAR->sName;` |
|     ! 0 |  8653 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8654 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8655 | `						}` |
|     ! 0 |  8656 | `					}` |
|      12 |  8657 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       7 |  8658 | `				}` |
|       - |  8659 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      10 |  8660 | `				pR = pUse->pResolvStart;` |
|      22 |  8661 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8662 | `					SyString sTrait,sMethod;` |
|       - |  8663 | `					ph7_class *pSrcTrait;` |
|       - |  8664 | `					ph7_class_method *pMeth;` |
|       - |  8665 | `					sxi32 nRKwrd;` |
|      34 |  8666 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      22 |  8667 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      14 |  8668 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      14 |  8669 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      14 |  8670 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      14 |  8671 | `					sMethod = pR->sData;` |
|      14 |  8672 | `					pR++;` |
|      14 |  8673 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8674 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8675 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8676 | `							sTrait = sMethod;` |
|       7 |  8677 | `							pR++;` |
|       7 |  8678 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8679 | `							sMethod = pR->sData;` |
|       7 |  8680 | `							pR++;` |
|       3 |  8681 | `						}` |
|       3 |  8682 | `					}` |
|      14 |  8683 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8684 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8685 | `						continue;` |
|       - |  8686 | `					}` |
|      14 |  8687 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      14 |  8688 | `					pR++;` |
|      14 |  8689 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8690 | `						pSrcTrait = 0;` |
|       7 |  8691 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8692 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8693 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8694 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8695 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8696 | `								break;` |
|       - |  8697 | `							}` |
|       2 |  8698 | `						}` |
|       5 |  8699 | `						if( pSrcTrait ){` |
|       5 |  8700 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8701 | `							if( pMeth ){` |
|       5 |  8702 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8703 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8704 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8705 | `								}` |
|       2 |  8706 | `							}` |
|       2 |  8707 | `						}` |
|       2 |  8708 | `					}` |
|      30 |  8709 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       2 |  8710 | `				}` |
|       - |  8711 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      20 |  8712 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8713 | `					ph7_class_method *pMR;` |
|       - |  8714 | `					SyHashEntry *pER;` |
|       - |  8715 | `					SyString *pNR;` |
|      12 |  8716 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      35 |  8717 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      20 |  8718 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      20 |  8719 | `						pNR = &pMR->sFunc.sName;` |
|      20 |  8720 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8721 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8722 | `						}` |
|       2 |  8723 | `					}` |
|       7 |  8724 | `				}` |
|       - |  8725 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      10 |  8726 | `				pR = pUse->pResolvStart;` |
|      22 |  8727 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8728 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8729 | `					ph7_class *pSrcTrait;` |
|       - |  8730 | `					ph7_class_method *pMeth;` |
|      22 |  8731 | `					int hasQual = 0;` |
|       - |  8732 | `					sxi32 nRKwrd;` |
|      34 |  8733 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      22 |  8734 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      14 |  8735 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      14 |  8736 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      14 |  8737 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      14 |  8738 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      14 |  8739 | `					sMethod = pR->sData;` |
|      14 |  8740 | `					pR++;` |
|      14 |  8741 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8742 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8743 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8744 | `							sTrait = sMethod;` |
|       7 |  8745 | `							hasQual = 1;` |
|       7 |  8746 | `							pR++;` |
|       7 |  8747 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8748 | `							sMethod = pR->sData;` |
|       7 |  8749 | `							pR++;` |
|       3 |  8750 | `						}` |
|       3 |  8751 | `					}` |
|      14 |  8752 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8753 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8754 | `						continue;` |
|       - |  8755 | `					}` |
|      14 |  8756 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      14 |  8757 | `					pR++;` |
|      14 |  8758 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      10 |  8759 | `						sxi32 iNewVis = -1;` |
|      10 |  8760 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  8761 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  8762 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  8763 | `								iNewVis = nAK;` |
|       7 |  8764 | `								pR++;` |
|       3 |  8765 | `							}` |
|       3 |  8766 | `						}` |
|      10 |  8767 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       8 |  8768 | `							sAlias = pR->sData;` |
|       8 |  8769 | `							pR++;` |
|       3 |  8770 | `						}` |
|      10 |  8771 | `						pMeth = 0;` |
|      10 |  8772 | `						if( hasQual ){` |
|       3 |  8773 | `							pSrcTrait = 0;` |
|       5 |  8774 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  8775 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  8776 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  8777 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  8778 | `									pSrcTrait = apTrait[nT];` |
|       3 |  8779 | `									break;` |
|       - |  8780 | `								}` |
|       2 |  8781 | `							}` |
|       3 |  8782 | `							if( pSrcTrait ){` |
|       3 |  8783 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  8784 | `							}` |
|       2 |  8785 | `						}else{` |
|       7 |  8786 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  8787 | `						}` |
|      10 |  8788 | `						if( pMeth ){` |
|      10 |  8789 | `							if( sAlias.nByte > 0 ){` |
|       - |  8790 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  8791 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  8792 | `								 */` |
|       - |  8793 | `								ph7_class_method *pAlias;` |
|       - |  8794 | `								char *zAliasDup;` |
|       8 |  8795 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       8 |  8796 | `								if( pAlias ){` |
|       8 |  8797 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       8 |  8798 | `									if( iNewVis >= 0 ){` |
|       5 |  8799 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8800 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8801 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  8802 | `									}` |
|       8 |  8803 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       8 |  8804 | `									if( zAliasDup ){` |
|       8 |  8805 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  8806 | `									}` |
|       5 |  8807 | `								}` |
|       6 |  8808 | `							}else if( iNewVis >= 0 ){` |
|       - |  8809 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  8810 | `								ph7_class_method *pCopy;` |
|       3 |  8811 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  8812 | `								if( pCopy ){` |
|       3 |  8813 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  8814 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  8815 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8816 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8817 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  8818 | `									/* Replace the method in the class hash */` |
|       3 |  8819 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  8820 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  8821 | `								}` |
|       1 |  8822 | `							}` |
|       4 |  8823 | `						}` |
|       4 |  8824 | `						SXUNUSED(hasQual);` |
|       4 |  8825 | `					}` |
|      18 |  8826 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       2 |  8827 | `				}` |
|       - |  8828 | `			}` |
|      49 |  8829 | `			SySetRelease(&pUse->aTraits);` |
|      27 |  8830 | `		}` |
|       - |  8831 | `	}` |
|       - |  8832 | `	/* Install the class */` |
|   86389 |  8833 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   86389 |  8834 | `	if( rc == SXRET_OK ){` |
|       - |  8835 | `		ph7_class **apInterface;` |
|       - |  8836 | `		sxu32 n;` |
|   86389 |  8837 | `		if( pBase ){` |
|       - |  8838 | `			/* Inherit from base class and mark as a subclass */` |
|   66593 |  8839 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   33294 |  8840 | `		}` |
|   86389 |  8841 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   95997 |  8842 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  8843 | `			/* Implements one or more interface */` |
|    9613 |  8844 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    9613 |  8845 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8846 | `				break;` |
|       - |  8847 | `			}` |
|    4809 |  8848 | `		}` |
|       - |  8849 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  8850 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  129576 |  8851 | `		if( rc == SXRET_OK` |
|   86384 |  8852 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   86389 |  8853 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   76023 |  8854 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  8855 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   76023 |  8856 | `			if( pStringable ){` |
|   76023 |  8857 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   76023 |  8858 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  8859 | `				sxu32 i;` |
|   76023 |  8860 | `				int bAlready = 0;` |
|   82355 |  8861 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    6339 |  8862 | `					if( apImpl[i] == pStringable ){` |
|       3 |  8863 | `						bAlready = 1;` |
|       3 |  8864 | `						break;` |
|       - |  8865 | `					}` |
|    3171 |  8866 | `				}` |
|   76023 |  8867 | `				if( !bAlready ){` |
|   76021 |  8868 | `					PH7_ClassImplement(pClass,pStringable);` |
|   38008 |  8869 | `				}` |
|   38009 |  8870 | `			}` |
|   38009 |  8871 | `		}` |
|       - |  8872 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   86389 |  8873 | `		if( rc == SXRET_OK ){` |
|   86389 |  8874 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   86389 |  8875 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8876 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8877 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8878 | `				return SXERR_ABORT;` |
|       - |  8879 | `			}` |
|   43192 |  8880 | `		}` |
|       - |  8881 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   86389 |  8882 | `		if( rc == SXRET_OK ){` |
|   86389 |  8883 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   86389 |  8884 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8885 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8886 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8887 | `				return SXERR_ABORT;` |
|       - |  8888 | `			}` |
|   43192 |  8889 | `		}` |
|   43192 |  8890 | `	}` |
|   86389 |  8891 | `	SySetRelease(&aUseEntries);` |
|   86389 |  8892 | `	SySetRelease(&aInterfaces);` |
|   86389 |  8893 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8894 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8895 | `		return SXERR_ABORT;` |
|       - |  8896 | `	}` |
|   43192 |  8897 | `done:` |
|       - |  8898 | `	/* Point beyond the class body */` |
|   86405 |  8899 | `	pGen->pIn = &pEnd[1];` |
|   86405 |  8900 | `	pGen->pEnd = pTmp;` |
|   86405 |  8901 | `	return PH7_OK;` |
|   43206 |  8902 |  |
|       - |  8903 | `/*` |
|       - |  8904 | ` * Compile a user-defined abstract class.` |
|       - |  8905 | ` *  According to the PHP language reference manual` |
|       - |  8906 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  8907 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  8908 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  8909 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  8910 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  8911 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  8912 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  8913 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  8914 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  8915 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  8916 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  8917 | ` *   could differ.` |
|       - |  8918 | ` */` |
|      20 |  8919 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       5 |  8920 |  |
|       - |  8921 | `	sxi32 rc;` |
|      25 |  8922 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      25 |  8923 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      25 |  8924 | `	return rc;` |
|       5 |  8925 |  |
|       - |  8926 | `/*` |
|       - |  8927 | ` * Compile a user-defined final class.` |
|       - |  8928 | ` *  According to the PHP language reference manual` |
|       - |  8929 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - |  8930 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - |  8931 | ` *    final then it cannot be extended.` |
|       - |  8932 | ` */` |
|       2 |  8933 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 |  8934 |  |
|       - |  8935 | `	sxi32 rc;` |
|       3 |  8936 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 |  8937 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 |  8938 | `	return rc;` |
|       1 |  8939 |  |
|       - |  8940 | `/*` |
|       - |  8941 | ` * Compile a user-defined trait.` |
|       - |  8942 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  8943 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  8944 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  8945 | ` */` |
|      56 |  8946 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  8947 |  |
|      61 |  8948 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8949 | `	ph7_class *pClass;` |
|       - |  8950 | `	SyToken *pEnd,*pTmp;` |
|       - |  8951 | `	sxi32 iProtection;` |
|       - |  8952 | `	sxi32 iAttrflags;` |
|       - |  8953 | `	SyString *pName;` |
|       - |  8954 | `	sxi32 nKwrd;` |
|       - |  8955 | `	sxi32 rc;` |
|       - |  8956 | `	/* Jump the 'trait' keyword */` |
|      61 |  8957 | `	pGen->pIn++;` |
|      61 |  8958 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8959 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  8960 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8961 | `			return SXERR_ABORT;` |
|       - |  8962 | `		}` |
|     ! 0 |  8963 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  8964 | `			pGen->pIn++;` |
|     ! 0 |  8965 | `		}` |
|     ! 0 |  8966 | `		return SXRET_OK;` |
|       - |  8967 | `	}` |
|       - |  8968 | `	/* Extract trait name */` |
|      61 |  8969 | `	pName = &pGen->pIn->sData;` |
|      61 |  8970 | `	pGen->pIn++;` |
|       - |  8971 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8972 | `		SyBlob sFQN;` |
|       - |  8973 | `		SyString sFQNStr;` |
|      61 |  8974 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      61 |  8975 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      61 |  8976 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      61 |  8977 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      61 |  8978 | `		SyBlobRelease(&sFQN);` |
|       - |  8979 | `	}` |
|      61 |  8980 | `	if( pClass == 0 ){` |
|     ! 0 |  8981 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8982 | `		return SXERR_ABORT;` |
|       - |  8983 | `	}` |
|       - |  8984 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      61 |  8985 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  8986 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  8987 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8988 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8989 | `			return SXERR_ABORT;` |
|       - |  8990 | `		}` |
|     ! 0 |  8991 | `		return SXRET_OK;` |
|       - |  8992 | `	}` |
|      61 |  8993 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      61 |  8994 | `	pEnd = 0;` |
|      61 |  8995 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      61 |  8996 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  8997 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  8998 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8999 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9000 | `			return SXERR_ABORT;` |
|       - |  9001 | `		}` |
|     ! 0 |  9002 | `		return SXRET_OK;` |
|       - |  9003 | `	}` |
|       - |  9004 | `	/* Swap token stream */` |
|      61 |  9005 | `	pTmp = pGen->pEnd;` |
|      61 |  9006 | `	pGen->pEnd = pEnd;` |
|       - |  9007 | `	/* Mark as trait */` |
|      61 |  9008 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9009 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      56 |  9010 | `	for(;;){` |
|     161 |  9011 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9012 | `			pGen->pIn++;` |
|       4 |  9013 | `		}` |
|     137 |  9014 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      61 |  9015 | `			break;` |
|       - |  9016 | `		}` |
|      81 |  9017 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9018 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9019 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9020 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9021 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9022 | `				return SXERR_ABORT;` |
|       - |  9023 | `			}` |
|     ! 0 |  9024 | `			goto done;` |
|       - |  9025 | `		}` |
|      81 |  9026 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      81 |  9027 | `		iAttrflags = 0;` |
|      81 |  9028 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      81 |  9029 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      81 |  9030 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9031 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9032 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9033 | `				for(;;){` |
|       - |  9034 | `					ph7_class *pUsedTrait;` |
|       - |  9035 | `					SyString *pUsedName;` |
|       5 |  9036 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9037 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9038 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9039 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9040 | `							return SXERR_ABORT;` |
|       - |  9041 | `						}` |
|     ! 0 |  9042 | `						break;` |
|       - |  9043 | `					}` |
|       5 |  9044 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9045 | `					{` |
|       - |  9046 | `						SyBlob sResolved;` |
|       5 |  9047 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9048 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9049 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9050 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9051 | `						SyBlobRelease(&sResolved);` |
|       - |  9052 | `					}` |
|       5 |  9053 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9054 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9055 | `					}` |
|       5 |  9056 | `					if( pUsedTrait == 0 ){` |
|       4 |  9057 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9058 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9059 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9060 | `							return SXERR_ABORT;` |
|       - |  9061 | `						}` |
|       2 |  9062 | `					}else{` |
|       3 |  9063 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9064 | `					}` |
|       5 |  9065 | `					pGen->pIn++;` |
|       5 |  9066 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  9067 | `						break;` |
|       - |  9068 | `					}` |
|     ! 0 |  9069 | `					pGen->pIn++;` |
|     ! 0 |  9070 | `				}` |
|       5 |  9071 | `				continue;` |
|       - |  9072 | `			}` |
|      77 |  9073 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  9074 | `				iProtection = nKwrd;` |
|      73 |  9075 | `				pGen->pIn++;` |
|      68 |  9076 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  9077 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  9078 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9079 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9080 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9081 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9082 | `						return SXERR_ABORT;` |
|       - |  9083 | `					}` |
|     ! 0 |  9084 | `					goto done;` |
|       - |  9085 | `				}` |
|      73 |  9086 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  9087 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9088 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9089 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9090 | `							return SXERR_ABORT;` |
|       - |  9091 | `						}` |
|     ! 0 |  9092 | `						goto done;` |
|       - |  9093 | `					}` |
|      12 |  9094 | `					continue;` |
|       - |  9095 | `				}` |
|      63 |  9096 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  9097 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  9098 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9099 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9100 | `							return SXERR_ABORT;` |
|       - |  9101 | `						}` |
|     ! 0 |  9102 | `						goto done;` |
|       - |  9103 | `					}` |
|       5 |  9104 | `					continue;` |
|       - |  9105 | `				}` |
|      58 |  9106 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  9107 | `			}` |
|      62 |  9108 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  9109 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9110 | `					"Traits cannot have constants");` |
|     ! 0 |  9111 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9112 | `					return SXERR_ABORT;` |
|       - |  9113 | `				}` |
|     ! 0 |  9114 | `				goto done;` |
|     ! 0 |  9115 | `			}else{` |
|      62 |  9116 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9117 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9118 | `					pGen->pIn++;` |
|       5 |  9119 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9120 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9121 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9122 | `							iProtection = nKwrd;` |
|     ! 0 |  9123 | `							pGen->pIn++;` |
|     ! 0 |  9124 | `						}` |
|       1 |  9125 | `					}` |
|       4 |  9126 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9127 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  9128 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9129 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9130 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9131 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9132 | `							return SXERR_ABORT;` |
|       - |  9133 | `						}` |
|     ! 0 |  9134 | `						goto done;` |
|       - |  9135 | `					}` |
|       5 |  9136 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9137 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9138 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9139 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9140 | `								return SXERR_ABORT;` |
|       - |  9141 | `							}` |
|     ! 0 |  9142 | `							goto done;` |
|       - |  9143 | `						}` |
|       3 |  9144 | `						continue;` |
|       - |  9145 | `					}` |
|       3 |  9146 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9147 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9148 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9149 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9150 | `								return SXERR_ABORT;` |
|       - |  9151 | `							}` |
|     ! 0 |  9152 | `							goto done;` |
|       - |  9153 | `						}` |
|     ! 0 |  9154 | `						continue;` |
|       - |  9155 | `					}` |
|       3 |  9156 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      59 |  9157 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 |  9158 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 |  9159 | `					pGen->pIn++;` |
|       6 |  9160 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 |  9161 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  9162 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  9163 | `							iProtection = nKwrd;` |
|       6 |  9164 | `							pGen->pIn++;` |
|       2 |  9165 | `						}` |
|       2 |  9166 | `					}` |
|       6 |  9167 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9168 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9169 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9170 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9171 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9172 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9173 | `							return SXERR_ABORT;` |
|       - |  9174 | `						}` |
|     ! 0 |  9175 | `						goto done;` |
|       - |  9176 | `					}` |
|       6 |  9177 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9178 | `				}` |
|      60 |  9179 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9180 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9181 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9182 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9183 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9184 | `						return SXERR_ABORT;` |
|       - |  9185 | `					}` |
|     ! 0 |  9186 | `					goto done;` |
|       - |  9187 | `				}` |
|      60 |  9188 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9189 | `					pGen->pIn++;` |
|     ! 0 |  9190 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9191 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9192 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9193 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9194 | `							return SXERR_ABORT;` |
|       - |  9195 | `						}` |
|     ! 0 |  9196 | `						goto done;` |
|       - |  9197 | `					}` |
|     ! 0 |  9198 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9199 | `				}else{` |
|      60 |  9200 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9201 | `				}` |
|      60 |  9202 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9203 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9204 | `						return SXERR_ABORT;` |
|       - |  9205 | `					}` |
|     ! 0 |  9206 | `					goto done;` |
|       - |  9207 | `				}` |
|       - |  9208 | `			}` |
|      32 |  9209 | `		}else{` |
|     ! 0 |  9210 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9211 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9212 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9213 | `					return SXERR_ABORT;` |
|       - |  9214 | `				}` |
|     ! 0 |  9215 | `				goto done;` |
|       - |  9216 | `			}` |
|       - |  9217 | `		}` |
|       4 |  9218 | `	}` |
|       - |  9219 | `	/* Install the trait */` |
|      61 |  9220 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      61 |  9221 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9222 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9223 | `		return SXERR_ABORT;` |
|       - |  9224 | `	}` |
|      28 |  9225 | `done:` |
|       - |  9226 | `	/* Point beyond the trait body */` |
|      61 |  9227 | `	pGen->pIn = &pEnd[1];` |
|      61 |  9228 | `	pGen->pEnd = pTmp;` |
|      61 |  9229 | `	return PH7_OK;` |
|      33 |  9230 |  |
|       - |  9231 | `/*` |
|       - |  9232 | ` * Compile a user-defined class.` |
|       - |  9233 | ` *  According to the PHP language reference manual` |
|       - |  9234 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9235 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9236 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9237 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9238 | ` *   and functions (called "methods").` |
|       - |  9239 | ` */` |
|   86380 |  9240 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 |  9241 |  |
|       - |  9242 | `	sxi32 rc;` |
|   86385 |  9243 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   86385 |  9244 | `	return rc;` |
|       5 |  9245 |  |
|       - |  9246 | `/*` |
|       - |  9247 | ` * Exception handling.` |
|       - |  9248 | ` *  According to the PHP language reference manual` |
|       - |  9249 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9250 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9251 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9252 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9253 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9254 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9255 | ` *    (or re-thrown) within a catch block.` |
|       - |  9256 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9257 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9258 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9259 | ` *    been defined with set_exception_handler().` |
|       - |  9260 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9261 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9262 | ` */` |
|       - |  9263 | `/*` |
|       - |  9264 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9265 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9266 | ` * indicates failure.` |
|       - |  9267 | ` */` |
|    9702 |  9268 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  9269 |  |
|    9707 |  9270 | `	sxi32 rc = SXRET_OK;` |
|    9707 |  9271 | `	if( pRoot->pOp ){` |
|    9699 |  9272 | `		switch( pRoot->pOp->iOp ){` |
|    4847 |  9273 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9274 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9275 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9276 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9277 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9278 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    9699 |  9279 | `			break;` |
|     ! 0 |  9280 | `		default:` |
|       - |  9281 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9282 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9283 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9284 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9285 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9286 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9287 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9288 | `			}` |
|     ! 0 |  9289 | `			break;` |
|       - |  9290 | `		}` |
|    4860 |  9291 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9292 | `		/* Unexpected expression */` |
|     ! 0 |  9293 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9294 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9295 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9296 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9297 | `		}` |
|     ! 0 |  9298 | `	}` |
|    9707 |  9299 | `	return rc;` |
|       5 |  9300 |  |
|       - |  9301 | `/*` |
|       - |  9302 | ` * Compile a 'throw' statement.` |
|       - |  9303 | ` * throw: This is how you trigger an exception.` |
|       - |  9304 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9305 | ` */` |
|    9666 |  9306 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 |  9307 |  |
|    9671 |  9308 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9309 | `	GenBlock *pBlock;` |
|       - |  9310 | `	sxu32 nIdx;` |
|       - |  9311 | `	sxi32 rc;` |
|    9671 |  9312 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9313 | `	/* Compile the expression */` |
|    9671 |  9314 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    9671 |  9315 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9316 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9317 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9318 | `			return SXERR_ABORT;` |
|       - |  9319 | `		}` |
|     ! 0 |  9320 | `		return SXRET_OK;` |
|       - |  9321 | `	}` |
|    9671 |  9322 | `	pBlock = pGen->pCurrent;` |
|       - |  9323 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   44649 |  9324 | `	while(pBlock->pParent){` |
|   44645 |  9325 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    9667 |  9326 | `			break;` |
|       - |  9327 | `		}` |
|       - |  9328 | `		/* Point to the parent block */` |
|   34983 |  9329 | `		pBlock = pBlock->pParent;` |
|       5 |  9330 | `	}` |
|       - |  9331 | `	/* Emit the throw instruction */` |
|    9671 |  9332 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9333 | `	/* Emit the jump */` |
|    9671 |  9334 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    9671 |  9335 | `	return SXRET_OK;` |
|    4838 |  9336 |  |
|       - |  9337 | `/*` |
|       - |  9338 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9339 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9340 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9341 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9342 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9343 | ` */` |
|      36 |  9344 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9345 |  |
|      38 |  9346 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9347 | `	GenBlock *pBlock;` |
|       - |  9348 | `	sxu32 nIdx;` |
|       - |  9349 | `	sxi32 rc;` |
|      18 |  9350 | `	(void)iCompileFlag;` |
|      38 |  9351 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9352 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9353 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9354 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9355 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9356 | `			return SXERR_ABORT;` |
|       - |  9357 | `		}` |
|     ! 0 |  9358 | `		return SXRET_OK;` |
|       - |  9359 | `	}` |
|      38 |  9360 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9361 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9362 | `		return SXERR_ABORT;` |
|       - |  9363 | `	}` |
|      38 |  9364 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9365 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9366 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9367 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9368 | `			return SXERR_ABORT;` |
|       - |  9369 | `		}` |
|     ! 0 |  9370 | `		return SXRET_OK;` |
|       - |  9371 | `	}` |
|       - |  9372 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  9373 | `	pBlock = pGen->pCurrent;` |
|      60 |  9374 | `	while( pBlock->pParent ){` |
|      49 |  9375 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  9376 | `			break;` |
|       - |  9377 | `		}` |
|      23 |  9378 | `		pBlock = pBlock->pParent;` |
|       1 |  9379 | `	}` |
|      38 |  9380 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 |  9381 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 |  9382 | `	return SXRET_OK;` |
|      20 |  9383 |  |
|       - |  9384 | `/*` |
|       - |  9385 | ` * Compile a 'catch' block.` |
|       - |  9386 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  9387 | ` * an object containing the exception information.` |
|       - |  9388 | ` */` |
|     408 |  9389 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 |  9390 |  |
|     413 |  9391 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9392 | `	ph7_exception_block sCatch;` |
|       - |  9393 | `	SySet *pInstrContainer;` |
|       - |  9394 | `	SyString sClassName;` |
|       - |  9395 | `	GenBlock *pCatch;` |
|       - |  9396 | `	SyToken *pToken;` |
|       - |  9397 | `	SyString *pName;` |
|       - |  9398 | `	char *zDup;` |
|       - |  9399 | `	sxi32 rc;` |
|     413 |  9400 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  9401 | `	/* Zero the structure */` |
|     413 |  9402 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  9403 | `	/* Initialize fields */` |
|     413 |  9404 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     413 |  9405 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     413 |  9406 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  9407 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9408 | `			pToken = pGen->pIn;` |
|     ! 0 |  9409 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9410 | `				pToken--;` |
|     ! 0 |  9411 | `			}` |
|     ! 0 |  9412 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9413 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9414 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9415 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9416 | `				return SXERR_ABORT;` |
|       - |  9417 | `			}` |
|     ! 0 |  9418 | `			return SXERR_INVALID;` |
|       - |  9419 | `	}` |
|       - |  9420 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     413 |  9421 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     217 |  9422 | `	for(;;){` |
|       - |  9423 | `		SyBlob sResolved;` |
|     439 |  9424 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     439 |  9425 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 |  9426 | `			SyBlobRelease(&sResolved);` |
|       6 |  9427 | `			pToken = pGen->pIn;` |
|       6 |  9428 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9429 | `				pToken--;` |
|     ! 0 |  9430 | `			}` |
|       8 |  9431 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9432 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  9433 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 |  9434 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9435 | `				return SXERR_ABORT;` |
|       - |  9436 | `			}` |
|       6 |  9437 | `			return SXERR_INVALID;` |
|       - |  9438 | `		}` |
|       - |  9439 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - |  9440 | `		 * transient SyBlob allocation. */` |
|     650 |  9441 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     430 |  9442 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     435 |  9443 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     435 |  9444 | `		SyBlobRelease(&sResolved);` |
|     435 |  9445 | `		if( zDup == 0 ){` |
|     ! 0 |  9446 | `			goto Mem;` |
|       - |  9447 | `		}` |
|     435 |  9448 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     435 |  9449 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9450 | `			goto Mem;` |
|       - |  9451 | `		}` |
|       - |  9452 | `		/* Check for '\|' (multi-catch separator) */` |
|     443 |  9453 | `		if( pGen->pIn < pGen->pEnd &&` |
|     430 |  9454 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      31 |  9455 | `			pGen->pIn->sData.nByte == 1 &&` |
|      26 |  9456 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      28 |  9457 | `			pGen->pIn++; /* Consume the '\|' */` |
|      28 |  9458 | `			continue;` |
|       - |  9459 | `		}` |
|     409 |  9460 | `		break;` |
|     ! 0 |  9461 | `	}` |
|     606 |  9462 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     409 |  9463 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  9464 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9465 | `			pToken = pGen->pIn;` |
|     ! 0 |  9466 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9467 | `				pToken--;` |
|     ! 0 |  9468 | `			}` |
|     ! 0 |  9469 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9470 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9471 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9472 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9473 | `				return SXERR_ABORT;` |
|       - |  9474 | `			}` |
|     ! 0 |  9475 | `			return SXERR_INVALID;` |
|       - |  9476 | `	}` |
|     409 |  9477 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9478 | `	/* Duplicate instance name */` |
|     409 |  9479 | `	pName = &pGen->pIn->sData;` |
|     409 |  9480 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     409 |  9481 | `	if( zDup == 0 ){` |
|     ! 0 |  9482 | `		goto Mem;` |
|       - |  9483 | `	}` |
|     409 |  9484 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     409 |  9485 | `	pGen->pIn++;` |
|     409 |  9486 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9487 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9488 | `		pToken = pGen->pIn;` |
|     ! 0 |  9489 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9490 | `			pToken--;` |
|     ! 0 |  9491 | `		}` |
|     ! 0 |  9492 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9493 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9494 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9495 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9496 | `			return SXERR_ABORT;` |
|       - |  9497 | `		}` |
|     ! 0 |  9498 | `		return SXERR_INVALID;` |
|       - |  9499 | `	}` |
|       - |  9500 | `	/* Compile the block */` |
|     409 |  9501 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9502 | `	/* Create the catch block */` |
|     409 |  9503 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     409 |  9504 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9505 | `		return SXERR_ABORT;` |
|       - |  9506 | `	}` |
|       - |  9507 | `	/* Swap bytecode container */` |
|     409 |  9508 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     409 |  9509 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9510 | `	/* Compile the block */` |
|     409 |  9511 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9512 | `	/* Fix forward jumps now the destination is resolved  */` |
|     409 |  9513 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9514 | `	/* Emit the DONE instruction */` |
|     409 |  9515 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9516 | `	/* Leave the block */` |
|     409 |  9517 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9518 | `	/* Restore the default container */` |
|     409 |  9519 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9520 | `	/* Install the catch block */` |
|     409 |  9521 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     409 |  9522 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9523 | `		goto Mem;` |
|       - |  9524 | `	}` |
|     409 |  9525 | `	return SXRET_OK;` |
|     ! 0 |  9526 | `Mem:` |
|     ! 0 |  9527 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9528 | `	return SXERR_ABORT;` |
|     209 |  9529 |  |
|       - |  9530 | `/*` |
|       - |  9531 | ` * Compile a 'try' block.` |
|       - |  9532 | ` * A function using an exception should be in a "try" block.` |
|       - |  9533 | ` * If the exception does not trigger, the code will continue` |
|       - |  9534 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9535 | ` * is "thrown".` |
|       - |  9536 | ` */` |
|     422 |  9537 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 |  9538 |  |
|       - |  9539 | `	ph7_exception *pException;` |
|     427 |  9540 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9541 | `	GenBlock *pTry;` |
|       - |  9542 | `	sxu32 nJmpIdx;` |
|       - |  9543 | `	sxi32 rc;` |
|       - |  9544 | `	/* Create the exception container */` |
|     427 |  9545 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     427 |  9546 | `	if( pException == 0 ){` |
|     ! 0 |  9547 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9548 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9549 | `		return SXERR_ABORT;` |
|       - |  9550 | `	}` |
|       - |  9551 | `	/* Zero the structure */` |
|     427 |  9552 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9553 | `	/* Initialize fields */` |
|     427 |  9554 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     427 |  9555 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     427 |  9556 | `	pException->iHasFinally = 0;` |
|     427 |  9557 | `	pException->iFinallyDone = 0;` |
|     427 |  9558 | `	pException->pVm = pGen->pVm;` |
|       - |  9559 | `	/* Create the try block */` |
|     427 |  9560 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     427 |  9561 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9562 | `		return SXERR_ABORT;` |
|       - |  9563 | `	}` |
|       - |  9564 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     427 |  9565 | `	pTry->pUserData = pException;` |
|       - |  9566 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     427 |  9567 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9568 | `	/* Fix the jump later when the destination is resolved */` |
|     427 |  9569 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     427 |  9570 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9571 | `	/* Compile the block */` |
|     427 |  9572 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     427 |  9573 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9574 | `		return SXERR_ABORT;` |
|       - |  9575 | `	}` |
|       - |  9576 | `	/* Fix forward jumps now the destination is resolved */` |
|     427 |  9577 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9578 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     427 |  9579 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  9580 | `	/* Leave the block */` |
|     427 |  9581 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9582 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     427 |  9583 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     420 |  9584 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  9585 | `		/* Compile one or more catch blocks */` |
|     404 |  9586 | `		for(;;){` |
|     808 |  9587 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     635 |  9588 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     205 |  9589 | `					break;` |
|       - |  9590 | `			}` |
|     413 |  9591 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     413 |  9592 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9593 | `				return SXERR_ABORT;` |
|       - |  9594 | `			}` |
|       5 |  9595 | `		}` |
|     200 |  9596 | `	}` |
|       - |  9597 | `	/* Compile optional finally block */` |
|     427 |  9598 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     208 |  9599 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  9600 | `		SySet *pInstrContainer;` |
|       - |  9601 | `		GenBlock *pFinBlock;` |
|      52 |  9602 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  9603 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      52 |  9604 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      52 |  9605 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9606 | `			return SXERR_ABORT;` |
|       - |  9607 | `		}` |
|       - |  9608 | `		/* Swap bytecode container */` |
|      52 |  9609 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      52 |  9610 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  9611 | `		/* Compile the finally body */` |
|      52 |  9612 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      52 |  9613 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9614 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  9615 | `			return SXERR_ABORT;` |
|       - |  9616 | `		}` |
|       - |  9617 | `		/* Fix forward jumps now the destination is resolved */` |
|      52 |  9618 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9619 | `		/* Emit DONE to terminate the finally block */` |
|      52 |  9620 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9621 | `		/* Leave the block */` |
|      52 |  9622 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9623 | `		/* Restore the default container */` |
|      52 |  9624 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      52 |  9625 | `		pException->iHasFinally = 1;` |
|      24 |  9626 | `	}` |
|       - |  9627 | `	/* Must have at least one catch or finally */` |
|     427 |  9628 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 |  9629 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9630 | `			"Cannot use try without catch or finally");` |
|       8 |  9631 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9632 | `			return SXERR_ABORT;` |
|       - |  9633 | `		}` |
|       3 |  9634 | `	}` |
|     427 |  9635 | `	return SXRET_OK;` |
|     216 |  9636 |  |
|       - |  9637 | `/*` |
|       - |  9638 | ` * Compile a switch block.` |
|       - |  9639 | ` *  (See block-comment below for more information)` |
|       - |  9640 | ` */` |
|     112 |  9641 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 |  9642 |  |
|     117 |  9643 | `	sxi32 rc = SXRET_OK;` |
|     117 |  9644 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  9645 | `		/* Unexpected token */` |
|     ! 0 |  9646 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9647 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9648 | `			return SXERR_ABORT;` |
|       - |  9649 | `		}` |
|     ! 0 |  9650 | `		pGen->pIn++;` |
|     ! 0 |  9651 | `	}` |
|     117 |  9652 | `	pGen->pIn++;` |
|       - |  9653 | `	/* First instruction to execute in this block. */` |
|     117 |  9654 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  9655 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  9656 | `	 * or the '}' token */` |
|     206 |  9657 | `	for(;;){` |
|     417 |  9658 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9659 | `			/* No more input to process */` |
|     ! 0 |  9660 | `			break;` |
|       - |  9661 | `		}` |
|     417 |  9662 | `		rc = SXRET_OK;` |
|     417 |  9663 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 |  9664 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 |  9665 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  9666 | `					/* Unexpected token */` |
|     ! 0 |  9667 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9668 | `						&pGen->pIn->sData);` |
|     ! 0 |  9669 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9670 | `						return SXERR_ABORT;` |
|       - |  9671 | `					}` |
|       - |  9672 | `					/* FALL THROUGH */` |
|     ! 0 |  9673 | `				}` |
|      31 |  9674 | `				rc = SXERR_EOF;` |
|      31 |  9675 | `				break;` |
|       - |  9676 | `			}` |
|      32 |  9677 | `		}else{` |
|       - |  9678 | `			sxi32 nKwrd;` |
|       - |  9679 | `			/* Extract the keyword */` |
|     337 |  9680 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 |  9681 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 |  9682 | `				break;` |
|       - |  9683 | `			}` |
|     253 |  9684 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9685 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  9686 | `					/* Unexpected token */` |
|     ! 0 |  9687 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9688 | `						&pGen->pIn->sData);` |
|     ! 0 |  9689 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9690 | `						return SXERR_ABORT;` |
|       - |  9691 | `					}` |
|       - |  9692 | `					/* FALL THROUGH */` |
|     ! 0 |  9693 | `				}` |
|       - |  9694 | `				/* Block compiled */` |
|       3 |  9695 | `				break;` |
|       - |  9696 | `			}` |
|       - |  9697 | `		}` |
|       - |  9698 | `		/* Compile block */` |
|     305 |  9699 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 |  9700 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9701 | `			return SXERR_ABORT;` |
|       - |  9702 | `		}` |
|       5 |  9703 | `	}` |
|     117 |  9704 | `	return rc;` |
|      61 |  9705 |  |
|       - |  9706 | `/*` |
|       - |  9707 | ` * Compile a case eXpression.` |
|       - |  9708 | ` *  (See block-comment below for more information)` |
|       - |  9709 | ` */` |
|      92 |  9710 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 |  9711 |  |
|       - |  9712 | `	SySet *pInstrContainer;` |
|       - |  9713 | `	SyToken *pEnd,*pTmp;` |
|      97 |  9714 | `	sxi32 iNest = 0;` |
|       - |  9715 | `	sxi32 rc;` |
|       - |  9716 | `	/* Delimit the expression */` |
|      97 |  9717 | `	pEnd = pGen->pIn;` |
|     197 |  9718 | `	while( pEnd < pGen->pEnd ){` |
|     197 |  9719 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  9720 | `			/* Increment nesting level */` |
|       3 |  9721 | `			iNest++;` |
|     196 |  9722 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  9723 | `			/* Decrement nesting level */` |
|       3 |  9724 | `			iNest--;` |
|     194 |  9725 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 |  9726 | `			break;` |
|       - |  9727 | `		}` |
|     105 |  9728 | `		pEnd++;` |
|       5 |  9729 | `	}` |
|      97 |  9730 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  9731 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  9732 | `		if( rc == SXERR_ABORT ){` |
|       - |  9733 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9734 | `			return SXERR_ABORT;` |
|       - |  9735 | `		}` |
|     ! 0 |  9736 | `	}` |
|       - |  9737 | `	/* Swap token stream */` |
|      97 |  9738 | `	pTmp = pGen->pEnd;` |
|      97 |  9739 | `	pGen->pEnd = pEnd;` |
|      97 |  9740 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 |  9741 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 |  9742 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  9743 | `	/* Emit the done instruction */` |
|      97 |  9744 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 |  9745 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9746 | `	/* Update token stream */` |
|      97 |  9747 | `	pGen->pIn  = pEnd;` |
|      97 |  9748 | `	pGen->pEnd = pTmp;` |
|      97 |  9749 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9750 | `		return SXERR_ABORT;` |
|       - |  9751 | `	}` |
|      97 |  9752 | `	return SXRET_OK;` |
|      51 |  9753 |  |
|       - |  9754 | `/*` |
|       - |  9755 | ` * Compile the smart switch statement.` |
|       - |  9756 | ` * According to the PHP language reference manual` |
|       - |  9757 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  9758 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  9759 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  9760 | ` *  This is exactly what the switch statement is for.` |
|       - |  9761 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  9762 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  9763 | ` *  of the outer loop, use continue 2.` |
|       - |  9764 | ` *  Note that switch/case does loose comparision.` |
|       - |  9765 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  9766 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  9767 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  9768 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  9769 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  9770 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  9771 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  9772 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  9773 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  9774 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  9775 | ` *  list for the next case.` |
|       - |  9776 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  9777 | ` *  or floating-point numbers and strings.` |
|       - |  9778 | ` */` |
|      28 |  9779 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 |  9780 |  |
|       - |  9781 | `	GenBlock *pSwitchBlock;` |
|       - |  9782 | `	SyToken *pTmp,*pEnd;` |
|       - |  9783 | `	ph7_switch *pSwitch;` |
|       - |  9784 | `	sxu32 nToken;` |
|       - |  9785 | `	sxu32 nLine;` |
|       - |  9786 | `	sxi32 rc;` |
|      33 |  9787 | `	nLine = pGen->pIn->nLine;` |
|       - |  9788 | `	/* Jump the 'switch' keyword */` |
|      33 |  9789 | `	pGen->pIn++;` |
|      33 |  9790 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  9791 | `		/* Syntax error */` |
|     ! 0 |  9792 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  9793 | `		if( rc == SXERR_ABORT ){` |
|       - |  9794 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9795 | `			return SXERR_ABORT;` |
|       - |  9796 | `		}` |
|     ! 0 |  9797 | `		goto Synchronize;` |
|       - |  9798 | `	}` |
|       - |  9799 | `	/* Jump the left parenthesis '(' */` |
|      33 |  9800 | `	pGen->pIn++;` |
|      33 |  9801 | `	pEnd = 0; /* cc warning */` |
|       - |  9802 | `	/* Create the loop block */` |
|      47 |  9803 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  9804 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 |  9805 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9806 | `		return SXERR_ABORT;` |
|       - |  9807 | `	}` |
|       - |  9808 | `	/* Delimit the condition */` |
|      33 |  9809 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 |  9810 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  9811 | `		/* Empty expression */` |
|     ! 0 |  9812 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  9813 | `		if( rc == SXERR_ABORT ){` |
|       - |  9814 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9815 | `			return SXERR_ABORT;` |
|       - |  9816 | `		}` |
|     ! 0 |  9817 | `	}` |
|       - |  9818 | `	/* Swap token streams */` |
|      33 |  9819 | `	pTmp = pGen->pEnd;` |
|      33 |  9820 | `	pGen->pEnd = pEnd;` |
|       - |  9821 | `	/* Compile the expression */` |
|      33 |  9822 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 |  9823 | `	if( rc == SXERR_ABORT ){` |
|       - |  9824 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  9825 | `		return SXERR_ABORT;` |
|       - |  9826 | `	}` |
|       - |  9827 | `	/* Update token stream */` |
|      33 |  9828 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  9829 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9830 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9831 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9832 | `			return SXERR_ABORT;` |
|       - |  9833 | `		}` |
|     ! 0 |  9834 | `		pGen->pIn++;` |
|     ! 0 |  9835 | `	}` |
|      33 |  9836 | `	pGen->pIn  = &pEnd[1];` |
|      33 |  9837 | `	pGen->pEnd = pTmp;` |
|      33 |  9838 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 |  9839 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 |  9840 | `			pTmp = pGen->pIn;` |
|     ! 0 |  9841 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 |  9842 | `				pTmp--;` |
|     ! 0 |  9843 | `			}` |
|       - |  9844 | `			/* Unexpected token */` |
|     ! 0 |  9845 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 |  9846 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9847 | `				return SXERR_ABORT;` |
|       - |  9848 | `			}` |
|     ! 0 |  9849 | `			goto Synchronize;` |
|       - |  9850 | `	}` |
|       - |  9851 | `	/* Set the delimiter token */` |
|      33 |  9852 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 |  9853 | `		nToken = PH7_TK_KEYWORD;` |
|       - |  9854 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 |  9855 | `	}else{` |
|      31 |  9856 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - |  9857 | `	}` |
|      33 |  9858 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - |  9859 | `	/* Create the switch blocks container */` |
|      33 |  9860 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 |  9861 | `	if( pSwitch == 0 ){` |
|       - |  9862 | `		/* Abort compilation */` |
|     ! 0 |  9863 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9864 | `		return SXERR_ABORT;` |
|       - |  9865 | `	}` |
|       - |  9866 | `	/* Zero the structure */` |
|      33 |  9867 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - |  9868 | `	/* Initialize fields */` |
|      33 |  9869 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - |  9870 | `	/* Emit the switch instruction */` |
|      33 |  9871 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - |  9872 | `	/* Compile case blocks */` |
|     100 |  9873 | `	for(;;){` |
|       - |  9874 | `		sxu32 nKwrd;` |
|     119 |  9875 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9876 | `			/* No more input to process */` |
|     ! 0 |  9877 | `			break;` |
|       - |  9878 | `		}` |
|     119 |  9879 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9880 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - |  9881 | `				/* Unexpected token */` |
|     ! 0 |  9882 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9883 | `					&pGen->pIn->sData);` |
|     ! 0 |  9884 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9885 | `					return SXERR_ABORT;` |
|       - |  9886 | `				}` |
|       - |  9887 | `				/* FALL THROUGH */` |
|     ! 0 |  9888 | `			}` |
|       - |  9889 | `			/* Block compiled */` |
|     ! 0 |  9890 | `			break;` |
|       - |  9891 | `		}` |
|       - |  9892 | `		/* Extract the keyword */` |
|     119 |  9893 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 |  9894 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9895 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - |  9896 | `				/* Unexpected token */` |
|     ! 0 |  9897 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9898 | `					&pGen->pIn->sData);` |
|     ! 0 |  9899 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9900 | `					return SXERR_ABORT;` |
|       - |  9901 | `				}` |
|       - |  9902 | `				/* FALL THROUGH */` |
|     ! 0 |  9903 | `			}` |
|       - |  9904 | `			/* Block compiled */` |
|       3 |  9905 | `			break;` |
|       - |  9906 | `		}` |
|     117 |  9907 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - |  9908 | `			/*` |
|       - |  9909 | `			 * Accroding to the PHP language reference manual` |
|       - |  9910 | `			 *  A special case is the default case. This case matches anything` |
|       - |  9911 | `			 *  that wasn't matched by the other cases.` |
|       - |  9912 | `			 */` |
|      25 |  9913 | `			if( pSwitch->nDefault > 0 ){` |
|       - |  9914 | `				/* Default case already compiled */` |
|     ! 0 |  9915 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 |  9916 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9917 | `					return SXERR_ABORT;` |
|       - |  9918 | `				}` |
|     ! 0 |  9919 | `			}` |
|      25 |  9920 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - |  9921 | `			/* Compile the default block */` |
|      25 |  9922 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 |  9923 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9924 | `				return SXERR_ABORT;` |
|      25 |  9925 | `			}else if( rc == SXERR_EOF ){` |
|      23 |  9926 | `				break;` |
|       1 |  9927 | `			}` |
|      98 |  9928 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - |  9929 | `			ph7_case_expr sCase;` |
|       - |  9930 | `			/* Standard case block */` |
|      97 |  9931 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - |  9932 | `			/* initialize the structure */` |
|      97 |  9933 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  9934 | `			/* Compile the case expression */` |
|      97 |  9935 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 |  9936 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9937 | `				return SXERR_ABORT;` |
|       - |  9938 | `			}` |
|       - |  9939 | `			/* Compile the case block */` |
|      97 |  9940 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - |  9941 | `			/* Insert in the switch container */` |
|      97 |  9942 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 |  9943 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9944 | `				return SXERR_ABORT;` |
|      97 |  9945 | `			}else if( rc == SXERR_EOF ){` |
|       9 |  9946 | `				break;` |
|       - |  9947 | `			}` |
|      47 |  9948 | `		}else{` |
|       - |  9949 | `			/* Unexpected token */` |
|     ! 0 |  9950 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9951 | `				&pGen->pIn->sData);` |
|     ! 0 |  9952 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9953 | `				return SXERR_ABORT;` |
|       - |  9954 | `			}` |
|     ! 0 |  9955 | `			break;` |
|       - |  9956 | `		}` |
|       5 |  9957 | `	}` |
|       - |  9958 | `	/* Fix all jumps now the destination is resolved */` |
|      33 |  9959 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 |  9960 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9961 | `	/* Release the loop block */` |
|      33 |  9962 | `	GenStateLeaveBlock(pGen,0);` |
|      33 |  9963 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - |  9964 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 |  9965 | `		pGen->pIn++;` |
|      14 |  9966 | `	}` |
|       - |  9967 | `	/* Statement successfully compiled */` |
|      33 |  9968 | `	return SXRET_OK;` |
|     ! 0 |  9969 | `Synchronize:` |
|       - |  9970 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  9971 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 |  9972 | `		pGen->pIn++;` |
|     ! 0 |  9973 | `	}` |
|     ! 0 |  9974 | `	return SXRET_OK;` |
|      19 |  9975 |  |
|       - |  9976 | `/*` |
|       - |  9977 | ` * Chain operators participate in a postfix member-access chain.` |
|       - |  9978 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - |  9979 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - |  9980 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - |  9981 | ` */` |
|       - |  9982 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - |  9983 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - |  9984 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - |  9985 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - |  9986 |  |
|       - |  9987 | `/*` |
|       - |  9988 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - |  9989 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - |  9990 | ` * patched entries from the pending set.` |
|       - |  9991 | ` */` |
| 2314384 |  9992 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 |  9993 |  |
| 2314389 |  9994 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - |  9995 | `	sxu32 nTarget;` |
|       - |  9996 | `	sxu32 *aIdx;` |
|       - |  9997 | `	sxu32 i;` |
| 2314389 |  9998 | `	if( nCur <= nBaseline ){` |
| 2314299 |  9999 | `		return;` |
|       - | 10000 | `	}` |
|      93 | 10001 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      93 | 10002 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     191 | 10003 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     101 | 10004 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     101 | 10005 | `		if( pInstr ){` |
|     101 | 10006 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 | 10007 | `		}` |
|      52 | 10008 | `	}` |
|      93 | 10009 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1157197 | 10010 |  |
|       - | 10011 |  |
|       - | 10012 | `/*` |
|       - | 10013 | ` * By-reference out-parameters of builtin functions.` |
|       - | 10014 | ` *` |
|       - | 10015 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 10016 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 10017 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 10018 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 10019 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 10020 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 10021 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 10022 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 10023 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 10024 | ` * creates it" behaviour).` |
|       - | 10025 | ` *` |
|       - | 10026 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 10027 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 10028 | ` */` |
|  375836 | 10029 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 10030 |  |
|       - | 10031 | `	static const struct {` |
|       - | 10032 | `		const char *zName;` |
|       - | 10033 | `		sxu32 nByte;` |
|       - | 10034 | `		sxu32 mask;` |
|       - | 10035 | `	} aByRef[] = {` |
|       - | 10036 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10037 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10038 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10039 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10040 | `	};` |
|       - | 10041 | `	sxu32 i;` |
|  375841 | 10042 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1155 | 10043 | `		return 0;` |
|       - | 10044 | `	}` |
| 1873291 | 10045 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1498646 | 10046 | `		if( pName->nByte == aByRef[i].nByte` |
|  769086 | 10047 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      51 | 10048 | `			return aByRef[i].mask;` |
|       - | 10049 | `		}` |
|  749305 | 10050 | `	}` |
|  374645 | 10051 | `	return 0;` |
|  187923 | 10052 |  |
|       - | 10053 | `/*` |
|       - | 10054 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 10055 | ` *` |
|       - | 10056 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 10057 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 10058 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 10059 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 10060 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 10061 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 10062 | ` */` |
|  375836 | 10063 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 10064 |  |
|       - | 10065 | `	SyToken *p, *pEnd;` |
|  375841 | 10066 | `	pOut->zString = 0;` |
|  375841 | 10067 | `	pOut->nByte = 0;` |
|  375841 | 10068 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 10069 | `		return;` |
|       - | 10070 | `	}` |
|  375841 | 10071 | `	p = pLeft->pStart;` |
|  375841 | 10072 | `	pEnd = pLeft->pEnd;` |
|       - | 10073 | `	/* Optional single leading namespace separator (absolute path). */` |
|  375841 | 10074 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      24 | 10075 | `		p++;` |
|      10 | 10076 | `	}` |
|  375841 | 10077 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1129 | 10078 | `		return;` |
|       - | 10079 | `	}` |
|       - | 10080 | `	/* Must be a single component: nothing follows the name token. */` |
|  374717 | 10081 | `	if( p + 1 != pEnd ){` |
|      30 | 10082 | `		return;` |
|       - | 10083 | `	}` |
|  374691 | 10084 | `	*pOut = p->sData;` |
|  187923 | 10085 |  |
|       - | 10086 | `/*` |
|       - | 10087 | ` * Generate bytecode for a given expression tree.` |
|       - | 10088 | ` * If something goes wrong while generating bytecode` |
|       - | 10089 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 10090 | ` * this function takes care of generating the appropriate` |
|       - | 10091 | ` * error message.` |
|       - | 10092 | ` */` |
| 3118040 | 10093 | `static sxi32 GenStateEmitExprCode(` |
|       - | 10094 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10095 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 10096 | `	sxi32 iFlags /* Control flags */` |
|       - | 10097 | `	)` |
|       5 | 10098 |  |
|       - | 10099 | `	VmInstr *pInstr;` |
|       - | 10100 | `	sxu32 nJmpIdx;` |
| 3118045 | 10101 | `	sxi32 iP1 = 0;` |
| 3118045 | 10102 | `	sxu32 iP2 = 0;` |
| 3118045 | 10103 | `	void *p3  = 0;` |
|       - | 10104 | `	sxi32 iVmOp;` |
|       - | 10105 | `	sxi32 rc;` |
| 3118045 | 10106 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3118045 | 10107 | `	sxu32 nRhsNsBase = 0;` |
| 3118045 | 10108 | `	if( pNode->xCode ){` |
|       - | 10109 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 10110 | `		/* Compile node */` |
| 1931511 | 10111 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1931511 | 10112 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1931511 | 10113 | `		RE_SWAP_DELIMITER(pGen);` |
| 1931511 | 10114 | `		return rc;` |
|       - | 10115 | `	}` |
| 1186539 | 10116 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10117 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10118 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10119 | `		return SXERR_ABORT;` |
|       - | 10120 | `	}` |
| 1186539 | 10121 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1186539 | 10122 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      59 | 10123 | `		sxu32 nJmp = 0;` |
|       - | 10124 | `		sxu32 nNcNsBase;` |
|       - | 10125 | `		VmInstr *pInstrFix;` |
|       - | 10126 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10127 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10128 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10129 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10130 | `		 * stack slot carries a writable nIdx. */` |
|      59 | 10131 | `		if( pNode->pRight ){` |
|      59 | 10132 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10133 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      59 | 10134 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10135 | `				return rc;` |
|       - | 10136 | `			}` |
|      59 | 10137 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10138 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10139 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10140 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10141 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10142 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10143 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10144 | `			 * cascade for the actual write path stays correct. */` |
|      59 | 10145 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      59 | 10146 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      29 | 10147 | `				pInstrFix->iP2 = 3;` |
|      13 | 10148 | `			}` |
|      28 | 10149 | `		}` |
|       - | 10150 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      59 | 10151 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10152 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      59 | 10153 | `		if( pNode->pLeft ){` |
|      59 | 10154 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10155 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      59 | 10156 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10157 | `				return rc;` |
|       - | 10158 | `			}` |
|      59 | 10159 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      28 | 10160 | `		}` |
|       - | 10161 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      59 | 10162 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10163 | `		/* Patch the short-circuit jump to land after the store. */` |
|      59 | 10164 | `		if( nJmp > 0 ){` |
|      59 | 10165 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      59 | 10166 | `			if( pInstrFix ){` |
|      59 | 10167 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10168 | `			}` |
|      28 | 10169 | `		}` |
|      59 | 10170 | `		return SXRET_OK;` |
|       - | 10171 | `	}` |
| 1186483 | 10172 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10173 | `		sxu32 nJz,nJmp;` |
|       - | 10174 | `		sxu32 nTernaryNsBase;` |
|       - | 10175 | `		/* Ternary operator require special handling */` |
|       - | 10176 | `		/* Phase#1: Compile the condition */` |
|    2553 | 10177 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2553 | 10178 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2553 | 10179 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10180 | `			return rc;` |
|       - | 10181 | `		}` |
|       - | 10182 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10183 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10184 | `		 * condition expression, not leak past the ternary. */` |
|    2553 | 10185 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2553 | 10186 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2553 | 10187 | `		if( pNode->pLeft ){` |
|       - | 10188 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10189 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2485 | 10190 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10191 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2485 | 10192 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2485 | 10193 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2485 | 10194 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10195 | `				return rc;` |
|       - | 10196 | `			}` |
|    2485 | 10197 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1245 | 10198 | `		}else{` |
|       - | 10199 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10200 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10201 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10202 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10203 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10204 | `		}` |
|       - | 10205 | `		/* Phase#4: Emit the unconditional jump */` |
|    2553 | 10206 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10207 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2553 | 10208 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2553 | 10209 | `		if( pInstr ){` |
|    2553 | 10210 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1274 | 10211 | `		}` |
|    2553 | 10212 | `		if( !pNode->pLeft ){` |
|       - | 10213 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10214 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10215 | `		}` |
|       - | 10216 | `		/* Phase#6: Compile the 'else' expression */` |
|    2553 | 10217 | `		if( pNode->pRight ){` |
|    2553 | 10218 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2553 | 10219 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2553 | 10220 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10221 | `				return rc;` |
|       - | 10222 | `			}` |
|    2553 | 10223 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1274 | 10224 | `		}` |
|    2553 | 10225 | `		if( nJmp > 0 ){` |
|       - | 10226 | `			/* Phase#7: Fix the unconditional jump */` |
|    2553 | 10227 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2553 | 10228 | `			if( pInstr ){` |
|    2553 | 10229 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1274 | 10230 | `			}` |
|    1274 | 10231 | `		}` |
|       - | 10232 | `		/* All done */` |
|    2553 | 10233 | `		return SXRET_OK;` |
|       - | 10234 | `	}` |
| 1183935 | 10235 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10236 | `	/* Generate code for the left tree */` |
| 1183935 | 10237 | `	if( pNode->pLeft ){` |
| 1183897 | 10238 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1183897 | 10239 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10240 | `			ph7_expr_node **apNode;` |
|  375961 | 10241 | `			int hasSpread = 0;` |
|  375961 | 10242 | `			int hasNamed = 0;` |
|  375961 | 10243 | `			int bAnySpread = 0;` |
|  375961 | 10244 | `			sxu32 byRefMask = 0;` |
|       - | 10245 | `			sxi32 nArgs;` |
|       - | 10246 | `			sxi32 n;` |
|       - | 10247 | `			/* Recurse and generate bytecodes for function arguments */` |
|  375961 | 10248 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  375961 | 10249 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10250 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10251 | `			{` |
|  375961 | 10252 | `				int seenNamed = 0;` |
|  744477 | 10253 | `				for( n = 0; n < nArgs; ++n ){` |
|  368523 | 10254 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     188 | 10255 | `						seenNamed = 1;` |
|     188 | 10256 | `						hasNamed = 1;` |
|  368431 | 10257 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      23 | 10258 | `						bAnySpread = 1;` |
|  368329 | 10259 | `					}else if( seenNamed ){` |
|       3 | 10260 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10261 | `							"Cannot use positional argument after named argument");` |
|       3 | 10262 | `						return SXERR_SYNTAX;` |
|       - | 10263 | `					}` |
|  184263 | 10264 | `				}` |
|       - | 10265 | `			}` |
|       - | 10266 | `			/* Read-only load */` |
|  375959 | 10267 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10268 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10269 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10270 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10271 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  375959 | 10272 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  375959 | 10273 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  375954 | 10274 | `				if( pCallName->nByte == 5` |
|  206342 | 10275 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   19285 | 10276 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  366319 | 10277 | `				}else if( pCallName->nByte == 5` |
|  187062 | 10278 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      83 | 10279 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      39 | 10280 | `				}` |
|       - | 10281 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10282 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10283 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10284 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10285 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10286 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  375959 | 10287 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10288 | `					SyString sBuiltin;` |
|  375841 | 10289 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  375841 | 10290 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  187918 | 10291 | `				}` |
|  187977 | 10292 | `			}` |
|  744473 | 10293 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  368519 | 10294 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  368519 | 10295 | `				sxi32 iArgFlags = iFlags & ~EXPR_FLAG_LOAD_IDX_STORE;` |
|       - | 10296 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 10297 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate). */` |
|  368519 | 10298 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      31 | 10299 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      13 | 10300 | `				}` |
|  368519 | 10301 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  368519 | 10302 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10303 | `					return rc;` |
|       - | 10304 | `				}` |
|       - | 10305 | `				/* Each argument is an independent nullsafe scope. */` |
|  368519 | 10306 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  368519 | 10307 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10308 | `					/* Emit spread opcode to unpack this array argument */` |
|      23 | 10309 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      23 | 10310 | `					hasSpread = 1;` |
|      10 | 10311 | `				}` |
|  184262 | 10312 | `			}` |
|       - | 10313 | `			/* Total number of given arguments */` |
|  375959 | 10314 | `			iP1 = nArgs;` |
|  375959 | 10315 | `			iP2 = hasSpread;` |
|       - | 10316 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10317 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  375959 | 10318 | `			if( hasNamed ){` |
|     101 | 10319 | `				sxu32 nStrBytes = 0;` |
|       - | 10320 | `				char *zBuf;` |
|     297 | 10321 | `				for( n = 0; n < nArgs; ++n ){` |
|     199 | 10322 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10323 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      91 | 10324 | `					}` |
|     101 | 10325 | `				}` |
|       - | 10326 | `				{` |
|     101 | 10327 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     101 | 10328 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      98 | 10329 | `					&pGen->pVm->sAllocator, mapSize);` |
|     101 | 10330 | `				if( pMap ){` |
|     101 | 10331 | `					SyZero(pMap, mapSize);` |
|     101 | 10332 | `					pMap->bHasNamed = 1;` |
|     101 | 10333 | `					pMap->nTotal = (sxu32)nArgs;` |
|     101 | 10334 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     101 | 10335 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     297 | 10336 | `					for( n = 0; n < nArgs; ++n ){` |
|     199 | 10337 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10338 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     185 | 10339 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     185 | 10340 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     185 | 10341 | `							zBuf += nb;` |
|      91 | 10342 | `						}` |
|       - | 10343 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     101 | 10344 | `					}` |
|     101 | 10345 | `					p3 = (void *)pMap;` |
|      49 | 10346 | `				}` |
|       - | 10347 | `				}` |
|      49 | 10348 | `			}` |
|       - | 10349 | `			/* Remove stale flags now */` |
|  375959 | 10350 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  187977 | 10351 | `		}` |
| 1183895 | 10352 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1183895 | 10353 | `		if( rc != SXRET_OK ){` |
|      34 | 10354 | `			return rc;` |
|       - | 10355 | `		}` |
| 1183865 | 10356 | `		if( !bIsChainOp ){` |
|       - | 10357 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 10358 | `			 * target the end of that LHS chain, which is right here. */` |
|  553307 | 10359 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  276651 | 10360 | `		}` |
| 1183865 | 10361 | `		if( iVmOp == PH7_OP_CALL ){` |
|  375959 | 10362 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  375959 | 10363 | `			if( pInstr ){` |
|  375959 | 10364 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  374811 | 10365 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 10366 | `					sxu32 nQual;` |
|  374811 | 10367 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10368 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 10369 | `					 * so the later NEW handler (if any) can see it. */` |
|  374811 | 10370 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 10371 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 10372 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 10373 | `					 * imports — class imports must NOT affect function` |
|       - | 10374 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 10375 | `					 * before NEW; we store the original literal index in the` |
|       - | 10376 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 10377 | `					 * the unqualified name and re-qualify with class imports. */` |
|  374811 | 10378 | `					if( bAbsolute ){` |
|      24 | 10379 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      14 | 10380 | `					}else{` |
|  374791 | 10381 | `						int fromImport = 0;` |
|  374791 | 10382 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  374791 | 10383 | `						pInstr->iP2 = (sxi32)nQual;` |
|  374791 | 10384 | `						if( nQual != nOrig ){` |
|       - | 10385 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 10386 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 10387 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 10388 | `							if( !fromImport ){` |
|       - | 10389 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 10390 | `								if( p3 == 0 ){` |
|      67 | 10391 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 10392 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 10393 | `									if( pMap ){` |
|      67 | 10394 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 10395 | `										p3 = (void *)pMap;` |
|      31 | 10396 | `									}` |
|      31 | 10397 | `								}` |
|      67 | 10398 | `								if( p3 ){` |
|      67 | 10399 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 10400 | `								}` |
|      31 | 10401 | `							}` |
|      36 | 10402 | `						}` |
|       5 | 10403 | `					}` |
|  188556 | 10404 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 10405 | `					/* Method call,flag that */` |
|     873 | 10406 | `					pInstr->iP2 = 1;` |
|     434 | 10407 | `				}` |
|  187982 | 10408 | `			}` |
|  995888 | 10409 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 10410 | `			ph7_expr_node **apNode;` |
|       - | 10411 | `			sxi32 n;` |
|   81537 | 10412 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 10413 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 10414 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       - | 10415 | `			/* Recurse and generate bytecodes for array index */` |
|   81537 | 10416 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  147155 | 10417 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   65623 | 10418 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   65623 | 10419 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   65623 | 10420 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10421 | `					return rc;` |
|       - | 10422 | `				}` |
|       - | 10423 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   65623 | 10424 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   32814 | 10425 | `			}` |
|   81537 | 10426 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   65623 | 10427 | `				iP1 = 1; /* Node have an index associated with it */` |
|   32809 | 10428 | `			}` |
|   81537 | 10429 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 10430 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     241 | 10431 | `				iP2 = 4;` |
|   81419 | 10432 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 10433 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 10434 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      54 | 10435 | `				iP2 = 5;` |
|   81276 | 10436 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 10437 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 10438 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 10439 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 10440 | `				iP2 = 6;` |
|   81239 | 10441 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 10442 | `				/* Create an empty entry when the desired index is not found */` |
|   32103 | 10443 | `				iP2 = 1;` |
|   16054 | 10444 | `			}` |
|  767145 | 10445 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 10446 | `			/* POP the left node */` |
|      32 | 10447 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 10448 | `		}` |
|  591930 | 10449 | `	}` |
| 1183903 | 10450 | `	rc = SXRET_OK;` |
| 1183903 | 10451 | `	nJmpIdx = 0;` |
|       - | 10452 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 10453 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 10454 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1183903 | 10455 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     325 | 10456 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     325 | 10457 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     325 | 10458 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     325 | 10459 | `			int isSpecial = 0;` |
|     325 | 10460 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     237 | 10461 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     237 | 10462 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     247 | 10463 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     215 | 10464 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     109 | 10465 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      93 | 10466 | `					isSpecial = 1;` |
|      44 | 10467 | `				}` |
|     138 | 10468 | `			}` |
|     369 | 10469 | `			pInstr->iP1 = 0;` |
|     369 | 10470 | `			if( !isSpecial ){` |
|     193 | 10471 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      94 | 10472 | `			}` |
|       - | 10473 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 10474 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     281 | 10475 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     193 | 10476 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     193 | 10477 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 10478 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      46 | 10479 | `					return SXRET_OK;` |
|       - | 10480 | `				}` |
|      73 | 10481 | `			}` |
|     117 | 10482 | `		}` |
|     193 | 10483 | `	}` |
|       - | 10484 | `	/* Generate code for the right tree */` |
| 1183825 | 10485 | `	if( pNode->pRight ){` |
|  653723 | 10486 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 10487 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    9951 | 10488 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  648750 | 10489 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 10490 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3347 | 10491 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  642106 | 10492 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 10493 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 10494 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 10495 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  640422 | 10496 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 10497 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 10498 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 10499 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 10500 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 10501 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 10502 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     101 | 10503 | `			sxu32 nNsJmp = 0;` |
|     101 | 10504 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     101 | 10505 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  640262 | 10506 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  265419 | 10507 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  132707 | 10508 | `		}` |
|  653723 | 10509 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  653723 | 10510 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  653723 | 10511 | `		if( !bIsChainOp ){` |
|       - | 10512 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 10513 | `			 * operator instruction is emitted. */` |
|  480693 | 10514 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  240344 | 10515 | `		}` |
|  653723 | 10516 | `		if( iVmOp == PH7_OP_STORE ){` |
|  262009 | 10517 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  261980 | 10518 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 10519 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 10520 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 10521 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 10522 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 10523 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 10524 | `				 */` |
|      56 | 10525 | `				iVmOp = 0;` |
|  261983 | 10526 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  261957 | 10527 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 10528 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   73111 | 10529 | `					iP2 = 1;` |
|   36558 | 10530 | `				}else{` |
|  188851 | 10531 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10532 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   32057 | 10533 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   32057 | 10534 | `						iP1 = pInstr->iP1;` |
|   16031 | 10535 | `					}else{` |
|  156799 | 10536 | `						p3 = pInstr->p3;` |
|       - | 10537 | `					}` |
|       - | 10538 | `					/* POP the last dynamic load instruction */` |
|  188851 | 10539 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 10540 | `				}` |
|  130981 | 10541 | `			}` |
|  522721 | 10542 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      52 | 10543 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      52 | 10544 | `			if( pInstr ){` |
|      52 | 10545 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10546 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 10547 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 10548 | `					 */` |
|      15 | 10549 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 10550 | `					iP1 = pInstr->iP1;` |
|      15 | 10551 | `					iP2 = pInstr->iP2;` |
|      15 | 10552 | `					p3  = pInstr->p3;` |
|       8 | 10553 | `				}else{` |
|      38 | 10554 | `					p3 = pInstr->p3;` |
|       - | 10555 | `				}` |
|      25 | 10556 | `			}` |
|      25 | 10557 | `		}` |
|  326859 | 10558 | `	}` |
| 1183825 | 10559 | `	if( iVmOp > 0 ){` |
| 1183619 | 10560 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   13035 | 10561 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 10562 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    9513 | 10563 | `				iP1 = 1;` |
|    4759 | 10564 | `			}` |
| 1177104 | 10565 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 10566 | `			/* Namespace-qualify the class name for NEW */ {` |
|   16959 | 10567 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   16959 | 10568 | `				VmInstr *pCallInstr = 0;` |
|   16959 | 10569 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   16935 | 10570 | `					pCallInstr = pPeek;` |
|   16935 | 10571 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    8465 | 10572 | `				}` |
|   16959 | 10573 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   16957 | 10574 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10575 | `					sxu32 nLitForClass;` |
|       - | 10576 | `					/* If the CALL handler already qualified the name using` |
|       - | 10577 | `					 * function imports, recover the original unqualified` |
|       - | 10578 | `					 * literal so we can re-qualify with class imports. */` |
|   16957 | 10579 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 10580 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 10581 | `					}else{` |
|   16925 | 10582 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 10583 | `					}` |
|   16957 | 10584 | `					pPeek->iP1 = 0;` |
|   16957 | 10585 | `					if( !bAbsolute ){` |
|   16941 | 10586 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    8473 | 10587 | `					}else{` |
|      20 | 10588 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 10589 | `					}` |
|    8476 | 10590 | `				}` |
|       - | 10591 | `			}` |
|   16959 | 10592 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   16959 | 10593 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 10594 | `				VmInstr *pPrev;` |
|   16935 | 10595 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   16935 | 10596 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 10597 | `					/* Pop the call instruction, preserve named-arg map */` |
|   16935 | 10598 | `					iP1 = pInstr->iP1;` |
|   16935 | 10599 | `					if( pInstr->p3 ){` |
|      43 | 10600 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 10601 | `					}` |
|   16935 | 10602 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    8465 | 10603 | `				}` |
|    8470 | 10604 | `			}` |
| 1162112 | 10605 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 10606 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 10607 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     161 | 10608 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     161 | 10609 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     161 | 10610 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     161 | 10611 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     161 | 10612 | `				int isSpecialIs = 0;` |
|     161 | 10613 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     157 | 10614 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     157 | 10615 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     157 | 10616 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     152 | 10617 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      77 | 10618 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 10619 | `						isSpecialIs = 1;` |
|       5 | 10620 | `					}` |
|      77 | 10621 | `				}` |
|     163 | 10622 | `				pInstr->iP1 = 0;` |
|     163 | 10623 | `				if( !isSpecialIs && !bAbsolute ){` |
|     141 | 10624 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      68 | 10625 | `				}` |
|      82 | 10626 | `			}` |
| 1153560 | 10627 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 10628 | `			/* Prevent constant expansion for member/property names.` |
|       - | 10629 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 10630 | `			 * should not trigger constant lookup. */` |
|  173035 | 10631 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  173035 | 10632 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  172993 | 10633 | `				pInstr->iP1 = 0;` |
|   86494 | 10634 | `			}` |
|  173035 | 10635 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 10636 | `				/* Static member access,remember that */` |
|     247 | 10637 | `				iP1 = 1;` |
|     247 | 10638 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     247 | 10639 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      38 | 10640 | `					p3 = pInstr->p3;` |
|      38 | 10641 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      17 | 10642 | `				}` |
|     121 | 10643 | `			}` |
|   86515 | 10644 | `		}` |
|       - | 10645 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 10646 | `		 * This is the primary emit path for user-visible calls. */` |
| 1183617 | 10647 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  392913 | 10648 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  196454 | 10649 | `		}` |
|       - | 10650 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1183617 | 10651 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  591806 | 10652 | `	}` |
| 1183823 | 10653 | `	if( nJmpIdx > 0 ){` |
|       - | 10654 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   13417 | 10655 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   13417 | 10656 | `		if( pInstr ){` |
|   13417 | 10657 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    6706 | 10658 | `		}` |
|    6706 | 10659 | `	}` |
| 1183823 | 10660 | `	return rc;` |
| 1559006 | 10661 |  |
|       - | 10662 | `/*` |
|       - | 10663 | ` * Compile a PHP expression.` |
|       - | 10664 | ` * According to the PHP language reference manual:` |
|       - | 10665 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 10666 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 10667 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 10668 | ` *  is "anything that has a value".` |
|       - | 10669 | ` * If something goes wrong while compiling the expression,this` |
|       - | 10670 | ` * function takes care of generating the appropriate error` |
|       - | 10671 | ` * message.` |
|       - | 10672 | ` */` |
|  838766 | 10673 | `static sxi32 PH7_CompileExpr(` |
|       - | 10674 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10675 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 10676 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 10677 | `	)` |
|       5 | 10678 |  |
|       - | 10679 | `	ph7_expr_node *pRoot;` |
|       - | 10680 | `	SySet sExprNode;` |
|       - | 10681 | `	SyToken *pEnd;` |
|       - | 10682 | `	sxi32 nExpr;` |
|       - | 10683 | `	sxi32 iNest;` |
|       - | 10684 | `	sxi32 rc;` |
|       - | 10685 | `	sxu32 nNullsafeBase;` |
|       - | 10686 | `	/* Initialize worker variables */` |
|  838771 | 10687 | `	nExpr = 0;` |
|  838771 | 10688 | `	pRoot = 0;` |
|       - | 10689 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 10690 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  838771 | 10691 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  838771 | 10692 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  838771 | 10693 | `	SySetAlloc(&sExprNode,0x10);` |
|  838771 | 10694 | `	rc = SXRET_OK;` |
|       - | 10695 | `	/* Delimit the expression */` |
|  838771 | 10696 | `	pEnd = pGen->pIn;` |
|  838771 | 10697 | `	iNest = 0;` |
| 5611661 | 10698 | `	while( pEnd < pGen->pEnd ){` |
| 5325495 | 10699 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10700 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     411 | 10701 | `			iNest++;` |
| 5325292 | 10702 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     419 | 10703 | `			iNest--;` |
| 5324882 | 10704 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  552903 | 10705 | `			if( iNest <= 0 ){` |
|  552605 | 10706 | `				break;` |
|       - | 10707 | `			}` |
|     149 | 10708 | `		}` |
| 4772895 | 10709 | `		pEnd++;` |
|       5 | 10710 | `	}` |
|  838771 | 10711 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   19459 | 10712 | `		SyToken *pEnd2 = pGen->pIn;` |
|   19459 | 10713 | `		iNest = 0;` |
|       - | 10714 | `		/* Stop at the first comma */` |
|   39189 | 10715 | `		while( pEnd2 < pEnd ){` |
|   19741 | 10716 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      61 | 10717 | `				iNest++;` |
|   19713 | 10718 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      61 | 10719 | `				iNest--;` |
|   19657 | 10720 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      53 | 10721 | `				if( iNest <= 0 ){` |
|       7 | 10722 | `					break;` |
|       - | 10723 | `				}` |
|      21 | 10724 | `			}` |
|   19735 | 10725 | `			pEnd2++;` |
|       5 | 10726 | `		}` |
|   19459 | 10727 | `		if( pEnd2 <pEnd ){` |
|       7 | 10728 | `			pEnd = pEnd2;` |
|       3 | 10729 | `		}` |
|    9727 | 10730 | `	}` |
|  838771 | 10731 | `	if( pEnd > pGen->pIn ){` |
|  838761 | 10732 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 10733 | `		/* Swap delimiter */` |
|  838761 | 10734 | `		pGen->pEnd = pEnd;` |
|       - | 10735 | `		/* Try to get an expression tree */` |
|  838761 | 10736 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  838761 | 10737 | `		if( rc == SXRET_OK && pRoot ){` |
|  838579 | 10738 | `			rc = SXRET_OK;` |
|  838579 | 10739 | `			if( xTreeValidator ){` |
|       - | 10740 | `				/* Call the upper layer validator callback */` |
|   23457 | 10741 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   11726 | 10742 | `			}` |
|  838579 | 10743 | `			if( rc != SXERR_ABORT ){` |
|       - | 10744 | `				/* Generate code for the given tree */` |
|  838579 | 10745 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 10746 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 10747 | `				 * expression so they short-circuit to its end. */` |
|  838579 | 10748 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  419287 | 10749 | `			}` |
|  838579 | 10750 | `			nExpr = 1;` |
|  419287 | 10751 | `		}` |
|       - | 10752 | `		/* Release the whole tree */` |
|  838761 | 10753 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 10754 | `		/* Synchronize token stream */` |
|  838761 | 10755 | `		pGen->pEnd = pTmp;` |
|  838761 | 10756 | `		pGen->pIn  = pEnd;` |
|  838761 | 10757 | `		if( rc == SXERR_ABORT ){` |
|      14 | 10758 | `			SySetRelease(&sExprNode);` |
|      14 | 10759 | `			return SXERR_ABORT;` |
|       - | 10760 | `		}` |
|  419373 | 10761 | `	}` |
|  838761 | 10762 | `	SySetRelease(&sExprNode);` |
|  838761 | 10763 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  419388 | 10764 |  |
|       - | 10765 | `/*` |
|       - | 10766 | ` * Return a pointer to the node construct handler associated` |
|       - | 10767 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 10768 | ` */` |
|  213556 | 10769 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 10770 |  |
|  213561 | 10771 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 10772 | `		/* Numeric literal: Either real or integer */` |
|  112121 | 10773 | `		return PH7_CompileNumLiteral;` |
|  101445 | 10774 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 10775 | `		/* Double quoted string */` |
|   20795 | 10776 | `		return PH7_CompileString;` |
|   80655 | 10777 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 10778 | `		/* Single quoted string */` |
|   80541 | 10779 | `		return PH7_CompileSimpleString;` |
|     119 | 10780 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 10781 | `		/* Heredoc */` |
|      68 | 10782 | `		return PH7_CompileHereDoc;` |
|      55 | 10783 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 10784 | `		/* Nowdoc */` |
|      48 | 10785 | `		return PH7_CompileNowDoc;` |
|       8 | 10786 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 10787 | `		/* Backtick quoted string */` |
|       6 | 10788 | `		return PH7_CompileBacktic;` |
|       - | 10789 | `	}` |
|       3 | 10790 | `	return 0;` |
|  106783 | 10791 |  |
|       - | 10792 | `/*` |
|       - | 10793 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 10794 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 10795 | ` * in write context" parse error.` |
|       - | 10796 | ` */` |
|    6756 | 10797 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 10798 |  |
|       - | 10799 | `	sxi32 rc;` |
|    6761 | 10800 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6759 | 10801 | `		return SXRET_OK;` |
|       - | 10802 | `	}` |
|       5 | 10803 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 10804 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 10805 | `		"Can't use nullsafe operator in write context");` |
|       3 | 10806 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3383 | 10807 |  |
|       - | 10808 | `/*` |
|       - | 10809 | ` * Compile an unset() statement.` |
|       - | 10810 | ` * unset($var, $arr[$key], ...);` |
|       - | 10811 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 10812 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 10813 | ` * parent array before extracting the element to unset.` |
|       - | 10814 | ` */` |
|    2908 | 10815 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 10816 |  |
|    2913 | 10817 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2913 | 10818 | `	sxu32 nIdx = 0;` |
|       - | 10819 | `	SyString sName;` |
|       - | 10820 | `	sxi32 rc;` |
|       - | 10821 | `	/* Jump the 'unset' keyword */` |
|    2913 | 10822 | `	pGen->pIn++;` |
|       - | 10823 | `	/* Save delimiter */` |
|    2913 | 10824 | `	pTmp = pGen->pEnd;` |
|       - | 10825 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2913 | 10826 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2913 | 10827 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 10828 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 10829 | `		SyToken *pClose;` |
|    2913 | 10830 | `		pGen->pIn++;   /* Skip '(' */` |
|    2913 | 10831 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2913 | 10832 | `		pEnd = pClose; /* Stop at ')' */` |
|    1454 | 10833 | `	}` |
|    2913 | 10834 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 10835 | `	/* Resolve the 'unset' builtin name once */` |
|    2913 | 10836 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     359 | 10837 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     359 | 10838 | `		if( pObj == 0 ){` |
|     ! 0 | 10839 | `			return SXERR_ABORT;` |
|       - | 10840 | `		}` |
|     359 | 10841 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     359 | 10842 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     177 | 10843 | `	}` |
|       - | 10844 | `	/* Compile each comma-separated argument */` |
|    9671 | 10845 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6763 | 10846 | `		if( pGen->pIn < pNext ){` |
|    6763 | 10847 | `			pGen->pEnd = pNext;` |
|    6763 | 10848 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 10849 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 10850 | `				GenStateUnsetValidator);` |
|    6763 | 10851 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10852 | `				return SXERR_ABORT;` |
|       - | 10853 | `			}` |
|    6763 | 10854 | `			if( rc != SXERR_EMPTY ){` |
|       - | 10855 | `				/* Emit call for this single argument */` |
|    6761 | 10856 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6761 | 10857 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6761 | 10858 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3378 | 10859 | `			}` |
|    3379 | 10860 | `		}` |
|       - | 10861 | `		/* Jump trailing commas */` |
|   10615 | 10862 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3857 | 10863 | `			pNext++;` |
|       5 | 10864 | `		}` |
|    6763 | 10865 | `		pGen->pIn = pNext;` |
|       5 | 10866 | `	}` |
|       - | 10867 | `	/* Skip past the closing ')' if present */` |
|    2913 | 10868 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2913 | 10869 | `		pGen->pIn++;` |
|    1454 | 10870 | `	}` |
|       - | 10871 | `	/* Restore token stream */` |
|    2913 | 10872 | `	pGen->pEnd = pTmp;` |
|    2913 | 10873 | `	return SXRET_OK;` |
|    1459 | 10874 |  |
|       - | 10875 | `/*` |
|       - | 10876 | ` * PHP Language construct table.` |
|       - | 10877 | ` */` |
|       - | 10878 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 10879 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 10880 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 10881 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 10882 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 10883 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 10884 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 10885 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 10886 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 10887 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 10888 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 10889 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 10890 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 10891 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 10892 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 10893 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 10894 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 10895 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 10896 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 10897 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 10898 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 10899 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 10900 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 10901 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 10902 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 10903 | `};` |
|       - | 10904 | `/*` |
|       - | 10905 | ` * Return a pointer to the statement handler routine associated` |
|       - | 10906 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 10907 | ` */` |
|  565676 | 10908 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 10909 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 10910 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 10911 | `	)` |
|       5 | 10912 |  |
|  565681 | 10913 | `	sxu32 n = 0;` |
| 2919797 | 10914 | `	for(;;){` |
| 5839599 | 10915 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  121505 | 10916 | `			break;` |
|       - | 10917 | `		}` |
| 5718099 | 10918 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  444181 | 10919 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 10920 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 10921 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 10922 | `					/* 'static' (class context),return null */` |
|     ! 0 | 10923 | `					return 0;` |
|       - | 10924 | `				}` |
|     ! 0 | 10925 | `			}` |
|  444176 | 10926 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       6 | 10927 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 10928 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 10929 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 10930 | `				return 0;` |
|       - | 10931 | `			}` |
|       - | 10932 | `			/* Return a pointer to the handler.` |
|       - | 10933 | `			*/` |
|  444181 | 10934 | `			return aLangConstruct[n].xConstruct;` |
|       - | 10935 | `		}` |
| 5273923 | 10936 | `		n++;` |
|       5 | 10937 | `	}` |
|  121505 | 10938 | `	if( pLookahed ){` |
|  121505 | 10939 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   34857 | 10940 | `			return PH7_CompileClassInterface;` |
|   86653 | 10941 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   86385 | 10942 | `			return PH7_CompileClass;` |
|     273 | 10943 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      61 | 10944 | `			return PH7_CompileTrait;` |
|     212 | 10945 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      26 | 10946 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      25 | 10947 | `				return PH7_CompileAbstractClass;` |
|     192 | 10948 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 10949 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 10950 | `				return PH7_CompileFinalClass;` |
|       - | 10951 | `		}` |
|      95 | 10952 | `	}` |
|       - | 10953 | `	/* Not a language construct */` |
|     195 | 10954 | `	return 0;` |
|  282843 | 10955 |  |
|       - | 10956 | `/*` |
|       - | 10957 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 10958 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 10959 | ` */` |
|     190 | 10960 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 10961 |  |
|       - | 10962 | `	int rc;` |
|     195 | 10963 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     195 | 10964 | `	if( rc == FALSE ){` |
|      82 | 10965 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      81 | 10966 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 10967 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 10968 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 10969 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 10970 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 10971 | `			*/` |
|       - | 10972 | `			){` |
|      79 | 10973 | `				rc = TRUE;` |
|      37 | 10974 | `		}` |
|      41 | 10975 | `	}` |
|     195 | 10976 | `	return rc;` |
|       5 | 10977 |  |
|       - | 10978 | `/*` |
|       - | 10979 | ` * Compile a PHP chunk.` |
|       - | 10980 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10981 | ` * takes care of generating the appropriate error message.` |
|       - | 10982 | ` */` |
|  677348 | 10983 | `static sxi32 GenStateCompileChunk(` |
|       - | 10984 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10985 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 10986 | `	)` |
|       5 | 10987 |  |
|       - | 10988 | `	ProcLangConstruct xCons;` |
|       - | 10989 | `	sxi32 rc;` |
|  677353 | 10990 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  528244 | 10991 | `	for(;;){` |
|  866923 | 10992 | `		int bStmtIsDeclare = 0;` |
|  866923 | 10993 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10994 | `			/* No more input to process */` |
|   13397 | 10995 | `			break;` |
|       - | 10996 | `		}` |
|       - | 10997 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 10998 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  853531 | 10999 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  565681 | 11000 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  565681 | 11001 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 11002 | `				bStmtIsDeclare = 1;` |
|      20 | 11003 | `			}` |
|  282838 | 11004 | `		}` |
|  853531 | 11005 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 11006 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 11007 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  189545 | 11008 | `			pGen->bStrictTypesLocked = 1;` |
|   94770 | 11009 | `		}` |
|  853531 | 11010 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11011 | `			/* Compile block */` |
|      21 | 11012 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      21 | 11013 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11014 | `				break;` |
|       - | 11015 | `			}` |
|      13 | 11016 | `		}else{` |
|  853515 | 11017 | `			xCons = 0;` |
|  853515 | 11018 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  565681 | 11019 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 11020 | `				/* Try to extract a language construct handler */` |
|  565681 | 11021 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  565681 | 11022 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 11023 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 11024 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 11025 | `						&pGen->pIn->sData);` |
|       9 | 11026 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11027 | `						break;` |
|       - | 11028 | `					}` |
|       - | 11029 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 11030 | `					 * this erroneous statement.` |
|       - | 11031 | `					 */` |
|       9 | 11032 | `					xCons = PH7_ErrorRecover;` |
|       4 | 11033 | `				}` |
|  570677 | 11034 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   47137 | 11035 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 11036 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 11037 | `				xCons = PH7_CompileLabel;` |
|      56 | 11038 | `			}` |
|  853515 | 11039 | `			if( xCons == 0 ){` |
|       - | 11040 | `				/* Assume an expression an try to compile it */` |
|  287909 | 11041 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  287909 | 11042 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 11043 | `					/* Pop l-value */` |
|  287759 | 11044 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  143877 | 11045 | `				}` |
|  143957 | 11046 | `			}else{` |
|       - | 11047 | `				/* Go compile the sucker */` |
|  565611 | 11048 | `				rc = xCons(&(*pGen));` |
|       - | 11049 | `			}` |
|  853515 | 11050 | `			if( rc == SXERR_ABORT ){` |
|       - | 11051 | `				/* Request to abort compilation */` |
|      14 | 11052 | `				break;` |
|       - | 11053 | `			}` |
|       - | 11054 | `		}` |
|       - | 11055 | `		/* Ignore trailing semi-colons ';' */` |
| 1381211 | 11056 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  527695 | 11057 | `			pGen->pIn++;` |
|       5 | 11058 | `		}` |
|  853521 | 11059 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 11060 | `			/* Compile a single statement and return */` |
|  663951 | 11061 | `			break;` |
|       - | 11062 | `		}` |
|       - | 11063 | `		/* LOOP ONE */` |
|       - | 11064 | `		/* LOOP TWO */` |
|       - | 11065 | `		/* LOOP THREE */` |
|       - | 11066 | `		/* LOOP FOUR */` |
|       5 | 11067 | `	}` |
|       - | 11068 | `	/* Return compilation status */` |
|  677353 | 11069 | `	return rc;` |
|       5 | 11070 |  |
|       - | 11071 | `/*` |
|       - | 11072 | ` * Compile a Raw PHP chunk.` |
|       - | 11073 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11074 | ` * takes care of generating the appropriate error message.` |
|       - | 11075 | ` */` |
|   13404 | 11076 | `static sxi32 PH7_CompilePHP(` |
|       - | 11077 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11078 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 11079 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 11080 | `	)` |
|       5 | 11081 |  |
|   13409 | 11082 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 11083 | `	sxi32 rc;` |
|       - | 11084 | `	/* Reset the token set */` |
|   13409 | 11085 | `	SySetReset(&(*pTokenSet));` |
|       - | 11086 | `	/* Mark as the default token set */` |
|   13409 | 11087 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 11088 | `	/* Advance the stream cursor */` |
|   13409 | 11089 | `	pGen->pRawIn++;` |
|       - | 11090 | `	/* Tokenize the PHP chunk first */` |
|   13409 | 11091 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 11092 | `	/* Point to the head and tail of the token stream. */` |
|   13409 | 11093 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   13409 | 11094 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   13409 | 11095 | `	if( is_expr ){` |
|     ! 0 | 11096 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 11097 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 11098 | `			/* A simple expression,compile it */` |
|     ! 0 | 11099 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 11100 | `		}` |
|       - | 11101 | `		/* Emit the DONE instruction */` |
|     ! 0 | 11102 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 11103 | `		return SXRET_OK;` |
|       - | 11104 | `	}` |
|   13409 | 11105 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 11106 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 11107 | `		/*` |
|       - | 11108 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 11109 | `		 * According to the PHP reference manual:` |
|       - | 11110 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 11111 | `		 *  immediately follow` |
|       - | 11112 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 11113 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 11114 | `		 * Symisc extension:` |
|       - | 11115 | `		 *   This short syntax works with all PHP opening` |
|       - | 11116 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 11117 | `		 *   only short tag.` |
|       - | 11118 | `		 */` |
|       - | 11119 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 11120 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 11121 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11122 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11123 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11124 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11125 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11126 | `		}` |
|       3 | 11127 | `		return SXRET_OK;` |
|       - | 11128 | `	}` |
|       - | 11129 | `	/* Compile the PHP chunk */` |
|   13407 | 11130 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11131 | `	/* Fix exceptions jumps */` |
|   13407 | 11132 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11133 | `	/* Fix gotos now, the jump destination is resolved */` |
|   13407 | 11134 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11135 | `		rc = SXERR_ABORT;` |
|       1 | 11136 | `	}` |
|       - | 11137 | `	/* Reset container */` |
|   13407 | 11138 | `	SySetReset(&pGen->aGoto);` |
|   13407 | 11139 | `	SySetReset(&pGen->aLabel);` |
|   13407 | 11140 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11141 | `	/* Compilation result */` |
|   13407 | 11142 | `	return rc;` |
|    6707 | 11143 |  |
|       - | 11144 | `/*` |
|       - | 11145 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11146 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11147 | ` * This is the only compile interface exported from this file.` |
|       - | 11148 | ` */` |
|   16116 | 11149 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11150 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11151 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11152 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11153 | `	)` |
|       5 | 11154 |  |
|       - | 11155 | `	SySet aPhpToken,aRawToken;` |
|       - | 11156 | `	ph7_gen_state *pCodeGen;` |
|       - | 11157 | `	ph7_value *pRawObj;` |
|       - | 11158 | `	sxu32 nObjIdx;` |
|       - | 11159 | `	sxi32 nRawObj;` |
|       - | 11160 | `	int is_expr;` |
|       - | 11161 | `	sxi8 bSavedStrict;` |
|       - | 11162 | `	sxi8 bSavedStrictLocked;` |
|       - | 11163 | `	sxi32 rc;` |
|   16121 | 11164 | `	if( pScript->nByte < 1 ){` |
|       - | 11165 | `		/* Nothing to compile */` |
|     ! 0 | 11166 | `		return PH7_OK;` |
|       - | 11167 | `	}` |
|       - | 11168 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11169 | `	 * file's flags so include/require restore them on return. */` |
|   16121 | 11170 | `	pCodeGen = &pVm->sCodeGen;` |
|   16121 | 11171 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   16121 | 11172 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   16121 | 11173 | `	pCodeGen->bStrictTypes = 0;` |
|   16121 | 11174 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11175 | `	/* Initialize the tokens containers */` |
|   16121 | 11176 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16121 | 11177 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16121 | 11178 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   16121 | 11179 | `	is_expr = 0;` |
|   16121 | 11180 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11181 | `		SyToken sTmp;` |
|       - | 11182 | `		/* PHP only: -*/` |
|    3235 | 11183 | `		sTmp.nLine = 1;` |
|    3235 | 11184 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3235 | 11185 | `		sTmp.pUserData = 0;` |
|    3235 | 11186 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3235 | 11187 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3235 | 11188 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11189 | `			/* A simple PHP expression */` |
|     ! 0 | 11190 | `			is_expr = 1;` |
|     ! 0 | 11191 | `		}` |
|    1620 | 11192 | `	}else{` |
|       - | 11193 | `		/* Tokenize raw text */` |
|   12891 | 11194 | `		SySetAlloc(&aRawToken,32);` |
|   12891 | 11195 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11196 | `	}` |
|       - | 11197 | `	/* Process high-level tokens */` |
|   16121 | 11198 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   16121 | 11199 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   16121 | 11200 | `	rc = PH7_OK;` |
|   16121 | 11201 | `	if( is_expr ){` |
|       - | 11202 | `		/* Compile the expression */` |
|     ! 0 | 11203 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 11204 | `		goto cleanup;` |
|       - | 11205 | `	}` |
|   16121 | 11206 | `	nObjIdx = 0;` |
|       - | 11207 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 11208 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 11209 | `	 * preventing namespace bleeding across include()d files. */` |
|   16121 | 11210 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 11211 | `	/* Start the compilation process */` |
|   14507 | 11212 | `	for(;;){` |
|   42411 | 11213 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   16109 | 11214 | `			break; /* No more tokens to process */` |
|       - | 11215 | `		}` |
|   26307 | 11216 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 11217 | `			/* Compile the PHP chunk */` |
|   13409 | 11218 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   13409 | 11219 | `			if( rc == SXERR_ABORT ){` |
|      16 | 11220 | `				break;` |
|       - | 11221 | `			}` |
|   13397 | 11222 | `			continue;` |
|       - | 11223 | `		}` |
|       - | 11224 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   12903 | 11225 | `		nRawObj = 0;` |
|   25843 | 11226 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 11227 | `			/* Consume the raw chunk without any processing */` |
|   12945 | 11228 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   12945 | 11229 | `			if( pRawObj == 0 ){` |
|     ! 0 | 11230 | `				rc = SXERR_MEM;` |
|     ! 0 | 11231 | `				break;` |
|       - | 11232 | `			}` |
|       - | 11233 | `			/* Mark as constant and emit the load constant instruction */` |
|   12945 | 11234 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   12945 | 11235 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   12945 | 11236 | `			++nRawObj;` |
|   12945 | 11237 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 11238 | `		}` |
|   12903 | 11239 | `		if( nRawObj > 0 ){` |
|       - | 11240 | `			/* Emit the consume instruction */` |
|   12903 | 11241 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6449 | 11242 | `		}` |
|    8063 | 11243 | `	}` |
|    8058 | 11244 | `cleanup:` |
|   16121 | 11245 | `	SySetRelease(&aRawToken);` |
|   16121 | 11246 | `	SySetRelease(&aPhpToken);` |
|       - | 11247 | `	/* Restore outer file's strict_types scope */` |
|   16121 | 11248 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   16121 | 11249 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   16121 | 11250 | `	return rc;` |
|    8063 | 11251 |  |
|       - | 11252 | `/*` |
|       - | 11253 | ` * Utility routines.Initialize the code generator.` |
|       - | 11254 | ` */` |
|    3164 | 11255 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 11256 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11257 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11258 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11259 | `	)` |
|       5 | 11260 |  |
|    3169 | 11261 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11262 | `	/* Zero the structure */` |
|    3169 | 11263 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 11264 | `	/* Initial state */` |
|    3169 | 11265 | `	pGen->pVm  = &(*pVm);` |
|    3169 | 11266 | `	pGen->xErr = xErr;` |
|    3169 | 11267 | `	pGen->pErrData = pErrData;` |
|    3169 | 11268 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3169 | 11269 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3169 | 11270 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3169 | 11271 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3169 | 11272 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 11273 | `	/* Error log buffer */` |
|    3169 | 11274 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 11275 | `	/* General purpose working buffer */` |
|    3169 | 11276 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 11277 | `	/* Namespace state */` |
|    3169 | 11278 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3169 | 11279 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3169 | 11280 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3169 | 11281 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11282 | `	/* Create the global scope */` |
|    3169 | 11283 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 11284 | `	/* Point to the global scope */` |
|    3169 | 11285 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3169 | 11286 | `	return SXRET_OK;` |
|       5 | 11287 |  |
|       - | 11288 | `/*` |
|       - | 11289 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 11290 | ` */` |
|   18962 | 11291 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 11292 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11293 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11294 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11295 | `	)` |
|       5 | 11296 |  |
|   18967 | 11297 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11298 | `	GenBlock *pBlock,*pParent;` |
|       - | 11299 | `	/* Reset state */` |
|   18967 | 11300 | `	SySetReset(&pGen->aLabel);` |
|   18967 | 11301 | `	SySetReset(&pGen->aGoto);` |
|   18967 | 11302 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   18967 | 11303 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   18967 | 11304 | `	SyBlobRelease(&pGen->sWorker);` |
|   18967 | 11305 | `	SyBlobRelease(&pGen->sNamespace);` |
|   18967 | 11306 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   18967 | 11307 | `	SyHashRelease(&pGen->hUseImports);` |
|   18967 | 11308 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   18967 | 11309 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   18967 | 11310 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   18967 | 11311 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   18967 | 11312 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11313 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 11314 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 11315 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 11316 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 11317 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 11318 | `	 * number of unique names, which is acceptable. */` |
|       - | 11319 | `	/* Point to the global scope */` |
|   18967 | 11320 | `	pBlock = pGen->pCurrent;` |
|   18967 | 11321 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 11322 | `		pParent = pBlock->pParent;` |
|     ! 0 | 11323 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 11324 | `		pBlock = pParent;` |
|     ! 0 | 11325 | `	}` |
|   18967 | 11326 | `	pGen->xErr = xErr;` |
|   18967 | 11327 | `	pGen->pErrData = pErrData;` |
|   18967 | 11328 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   18967 | 11329 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   18967 | 11330 | `	pGen->pIn = pGen->pEnd = 0;` |
|   18967 | 11331 | `	pGen->nErr = 0;` |
|   18967 | 11332 | `	return SXRET_OK;` |
|       5 | 11333 |  |
|       - | 11334 | `/*` |
|       - | 11335 | ` * Generate a compile-time error message.` |
|       - | 11336 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 11337 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 11338 | ` * abort compilation immediately.` |
|       - | 11339 | ` */` |
|     580 | 11340 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 11341 |  |
|     585 | 11342 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     585 | 11343 | `	const char *zErr = "Error";` |
|       - | 11344 | `	SyString *pFile;` |
|       - | 11345 | `	va_list ap;` |
|       - | 11346 | `	sxi32 rc;` |
|       - | 11347 | `	/* Reset the working buffer */` |
|     585 | 11348 | `	SyBlobReset(pWorker);` |
|       - | 11349 | `	/* Peek the processed file path if available */` |
|     585 | 11350 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     585 | 11351 | `	if( nErrType == E_ERROR ){` |
|       - | 11352 | `		/* Increment the error counter */` |
|     479 | 11353 | `		pGen->nErr++;` |
|     479 | 11354 | `		if( pGen->nErr > 15 ){` |
|       - | 11355 | `			/* Error count limit reached */` |
|       5 | 11356 | `			if( pGen->xErr ){` |
|       5 | 11357 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 11358 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 11359 | `				if( pFile ){` |
|       5 | 11360 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 11361 | `				}` |
|       5 | 11362 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 11363 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 11364 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 11365 | `				}` |
|       2 | 11366 | `			}` |
|       - | 11367 | `			/* Abort immediately */` |
|       5 | 11368 | `			return SXERR_ABORT;` |
|       - | 11369 | `		}` |
|     235 | 11370 | `	}` |
|     581 | 11371 | `	if( pGen->xErr == 0 ){` |
|       - | 11372 | `		/* No available error consumer,return immediately */` |
|       3 | 11373 | `		return SXRET_OK;` |
|       - | 11374 | `	}` |
|     578 | 11375 | `	switch(nErrType){` |
|     472 | 11376 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 11377 | `	case E_WARNING: zErr = "Warning";     break;` |
|      76 | 11378 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      12 | 11379 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 11380 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 11381 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 11382 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 11383 | `	default:` |
|     ! 0 | 11384 | `		break;` |
|       - | 11385 | `	}` |
|     578 | 11386 | `	rc = SXRET_OK;` |
|       - | 11387 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     578 | 11388 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     578 | 11389 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     578 | 11390 | `	va_start(ap,zFormat);` |
|     578 | 11391 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     578 | 11392 | `	va_end(ap);` |
|     578 | 11393 | `	if( pFile ){` |
|     578 | 11394 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     287 | 11395 | `	}` |
|       - | 11396 | `	/* Append a new line */` |
|     578 | 11397 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     578 | 11398 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 11399 | `		/* Consume the generated error message */` |
|     578 | 11400 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     287 | 11401 | `	}` |
|     578 | 11402 | `	return rc;` |
|     295 | 11403 |  |
|       - | 11404 |  |
