# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6180/7642 lines (80.87%)

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
|      97 |   122 | `			aLabel[n].bRef = TRUE;` |
|      97 |   123 | `			if( ppOut ){` |
|      97 |   124 | `				*ppOut = &aLabel[n];` |
|      46 |   125 | `			}` |
|      97 |   126 | `			return SXRET_OK;` |
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
|    4128 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   138 | `{` |
|    4133 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   11796 |   140 | `	for(;;){` |
|   23597 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    4025 |   142 | `			iCount--; /* Decrement nesting level */` |
|    4025 |   143 | `			if( iCount < 1 ){` |
|       - |   144 | `				/* Block meet with the desired criteria */` |
|    3999 |   145 | `				return pBlock;` |
|       - |   146 | `			}` |
|      13 |   147 | `		}` |
|       - |   148 | `		/* Point to the upper block */` |
|   19603 |   149 | `		pBlock = pBlock->pParent;` |
|   19603 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   151 | `			/* Forbidden */` |
|      72 |   152 | `			break;` |
|       - |   153 | `		}` |
|       5 |   154 | `	}` |
|       - |   155 | `	/* No such block */` |
|     139 |   156 | `	return 0;` |
|    2069 |   157 | `}` |
|       - |   158 | `/*` |
|       - |   159 | ` * Initialize a freshly allocated block instance.` |
|       - |   160 | ` */` |
|  989838 |   161 | `static void GenStateInitBlock(` |
|       - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   166 | `	void *pUserData      /* Upper layer private data */` |
|       - |   167 | `	)` |
|       5 |   168 | `{` |
|       - |   169 | `	/* Initialize block fields */` |
|  989843 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  989843 |   171 | `	pBlock->pUserData   = pUserData;` |
|  989843 |   172 | `	pBlock->pGen        = pGen;` |
|  989843 |   173 | `	pBlock->iFlags      = iType;` |
|  989843 |   174 | `	pBlock->pParent     = 0;` |
|  989843 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  989843 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  989843 |   177 | `}` |
|       - |   178 | `/*` |
|       - |   179 | ` * Allocate a new block instance.` |
|       - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   182 | ` * processing on failure.` |
|       - |   183 | ` */` |
|  985994 |   184 | `static sxi32 GenStateEnterBlock(` |
|       - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   190 | `	)` |
|       5 |   191 | `{` |
|       - |   192 | `	GenBlock *pBlock;` |
|       - |   193 | `	/* Allocate a new block instance */` |
|  985999 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  985999 |   195 | `	if( pBlock == 0 ){` |
|       - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   198 | `		 */` |
|     ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   200 | `		/* Abort processing immediately */` |
|     ! 0 |   201 | `		return SXERR_ABORT;` |
|       - |   202 | `	}` |
|       - |   203 | `	/* Zero the structure */` |
|  985999 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  985999 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   206 | `	/* Link to the parent block */` |
|  985999 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   208 | `	/* Mark as the current block */` |
|  985999 |   209 | `	pGen->pCurrent = pBlock;` |
|  985999 |   210 | `	if( ppBlock ){` |
|       - |   211 | `		/* Write a pointer to the new instance */` |
|  474351 |   212 | `		*ppBlock = pBlock;` |
|  237173 |   213 | `	}` |
|  985999 |   214 | `	return SXRET_OK;` |
|  493002 |   215 | `}` |
|       - |   216 | `/*` |
|       - |   217 | ` * Release block fields without freeing the whole instance.` |
|       - |   218 | ` */` |
|  985986 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   220 | `{` |
|  985991 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  985991 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  985991 |   223 | `}` |
|       - |   224 | `/*` |
|       - |   225 | ` * Release a block.` |
|       - |   226 | ` */` |
|  985986 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   228 | `{` |
|  985991 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  985991 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   231 | `	/* Free the instance */` |
|  985991 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  985991 |   233 | `}` |
|       - |   234 | `/*` |
|       - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   236 | ` */` |
|  985986 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   238 | `{` |
|  985991 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  985991 |   240 | `	if( pBlock == 0 ){` |
|       - |   241 | `		/* No more block to pop */` |
|     ! 0 |   242 | `		return SXERR_EMPTY;` |
|       - |   243 | `	}` |
|       - |   244 | `	/* Point to the upper block */` |
|  985991 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  985991 |   246 | `	if( ppBlock ){` |
|       - |   247 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   248 | `		*ppBlock = pBlock;` |
|     ! 0 |   249 | `	}else{` |
|       - |   250 | `		/* Safely release the block */` |
|  985991 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   252 | `	}` |
|  985991 |   253 | `	return SXRET_OK;` |
|  492998 |   254 | `}` |
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
|  336720 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   266 | `{` |
|       - |   267 | `	JumpFixup sJumpFix;` |
|       - |   268 | `	sxi32 rc;` |
|       - |   269 | `	/* Init the JumpFixup structure */` |
|  336725 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  336725 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   272 | `	/* Insert in the jump fixup table */` |
|  336725 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  336725 |   274 | `	return rc;` |
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
|  706604 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   288 | `{` |
|       - |   289 | `	JumpFixup *aFix;` |
|       - |   290 | `	VmInstr *pInstr;` |
|       - |   291 | `	sxu32 nFixed;` |
|       - |   292 | `	sxu32 n;` |
|       - |   293 | `	/* Point to the jump fixup table */` |
|  706609 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   295 | `	/* Fix the desired jumps */` |
| 1351897 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  645293 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   298 | `			/* Already fixed */` |
|  246215 |   299 | `			continue;` |
|       - |   300 | `		}` |
|  399083 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   302 | `			/* Not of our interest */` |
|   62365 |   303 | `			continue;` |
|       - |   304 | `		}` |
|       - |   305 | `		/* Point to the instruction to fix */` |
|  336723 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  336723 |   307 | `		if( pInstr ){` |
|  336723 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  336723 |   309 | `			nFixed++;` |
|       - |   310 | `			/* Mark as fixed */` |
|  336723 |   311 | `			aFix[n].nJumpType = -1;` |
|  168359 |   312 | `		}` |
|  168364 |   313 | `	}` |
|       - |   314 | `	/* Total number of fixed jumps */` |
|  706609 |   315 | `	return nFixed;` |
|       5 |   316 | `}` |
|       - |   317 | `/*` |
|       - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   319 | ` * The goto statement can be used to jump to another section` |
|       - |   320 | ` * in the program.` |
|       - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   322 | ` * statement for more information.` |
|       - |   323 | ` */` |
|  263088 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   325 | `{` |
|       - |   326 | `	JumpFixup *pJump,*aJumps;` |
|       - |   327 | `	Label *pLabel,*aLabel;` |
|       - |   328 | `	VmInstr *pInstr;` |
|       - |   329 | `	sxi32 rc;` |
|       - |   330 | `	sxu32 n;` |
|       - |   331 | `	/* Point to the goto table */` |
|  263093 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   333 | `	/* Fix */` |
|  263239 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|      97 |   347 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|      12 |   348 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|      12 |   349 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   350 | `				return SXERR_ABORT;` |
|       - |   351 | `			}` |
|       4 |   352 | `		}` |
|       - |   353 | `		/* Fix the jump now the destination is resolved */` |
|      97 |   354 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      97 |   355 | `		if( pInstr ){` |
|      97 |   356 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   357 | `		}` |
|      51 |   358 | `	}` |
|  263091 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  263223 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   362 | `			/* Emit a warning */` |
|      40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   365 | `		}` |
|      71 |   366 | `	}` |
|  263091 |   367 | `	return SXRET_OK;` |
|  131549 |   368 | `}` |
|       - |   369 | `/*` |
|       - |   370 | ` * Check if a given token value is installed in the literal table.` |
|       - |   371 | ` */` |
|  926296 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   373 | `{` |
|       - |   374 | `	SyHashEntry *pEntry;` |
|  926301 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  926301 |   376 | `	if( pEntry == 0 ){` |
|  445485 |   377 | `		return SXERR_NOTFOUND;` |
|       - |   378 | `	}` |
|  480821 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  480821 |   380 | `	return SXRET_OK;` |
|  463153 |   381 | `}` |
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
|  445480 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   393 | `{` |
|  445485 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  445485 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  222740 |   396 | `	}` |
|  445485 |   397 | `	return SXRET_OK;` |
|       5 |   398 | `}` |
|       - |   399 | `/*` |
|       - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   401 | ` * in the constant table.` |
|       - |   402 | ` */` |
|  135456 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   404 | `{` |
|       - |   405 | `	ph7_value *pObj;` |
|  135461 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   407 | `	/* Reserve a new constant */` |
|  135461 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  135461 |   409 | `	if( pObj == 0 ){` |
|     ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   411 | `		return 0;` |
|       - |   412 | `	}` |
|  135461 |   413 | `	*pIdx = nIdx;` |
|       - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   416 | `	 */` |
|  135461 |   417 | `	return pObj;` |
|   67733 |   418 | `}` |
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
|  574266 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   434 | `{` |
|       - |   435 | `	VmCallArgMap *pMap;` |
|  574271 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|      39 |   437 | `	if( p3 == 0 ){` |
|      35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      35 |   439 | `		if( pMap == 0 ) return 0;` |
|      35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      35 |   441 | `		p3 = (void *)pMap;` |
|      16 |   442 | `	}` |
|      39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      39 |   444 | `	return p3;` |
|  287138 |   445 | `}` |
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
|  136426 |   504 | `static int GenStateFindBadNumericSeparator(` |
|       - |   505 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   506 | `{` |
|  136431 |   507 | `	const char *z = pRaw->zString;` |
|  136431 |   508 | `	sxu32 n = pRaw->nByte;` |
|  136431 |   509 | `	int base = 10;` |
|       - |   510 | `	sxu32 i, start;` |
|  136431 |   511 | `	if( n < 2 ) return 0;` |
|   11533 |   512 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      80 |   513 | `		base = 16;` |
|   11494 |   514 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     284 |   515 | `		base = 2;` |
|     141 |   516 | `	}` |
|   42957 |   517 | `	for( i = 0; i < n; ++i ){` |
|   31443 |   518 | `		if( z[i] != '_' ) continue;` |
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
|   11519 |   535 | `	return 0;` |
|   68218 |   536 | `}` |
|       - |   537 | `/*` |
|       - |   538 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   539 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   540 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   541 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   542 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   543 | ` * so callers can bail from the current construct).` |
|       - |   544 | ` */` |
|  136426 |   545 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   546 | `{` |
|  136431 |   547 | `	const char *zBad = 0;` |
|  136431 |   548 | `	sxu32 nBad = 0;` |
|       - |   549 | `	SyString sBad;` |
|       - |   550 | `	sxi32 rc;` |
|  136431 |   551 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  136417 |   552 | `		return SXRET_OK;` |
|       - |   553 | `	}` |
|      18 |   554 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   555 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   556 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   557 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   558 | `		return SXERR_ABORT;` |
|       - |   559 | `	}` |
|      18 |   560 | `	return SXERR_SYNTAX;` |
|   68218 |   561 | `}` |
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
|  136412 |   578 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   579 | `	SyMemBackend *pAlloc,` |
|       - |   580 | `	const SyString *pToken,` |
|       - |   581 | `	char *zScratch, sxu32 nScratch,` |
|       - |   582 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   583 | `{` |
|       - |   584 | `	sxu32 i, j;` |
|  136417 |   585 | `	int hasUnderscore = 0;` |
|       - |   586 | `	char *zBuf;` |
|  136417 |   587 | `	*pzAlloc = 0;` |
|  290673 |   588 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  154513 |   589 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   77133 |   590 | `	}` |
|  136417 |   591 | `	if( !hasUnderscore ){` |
|  136165 |   592 | `		SyStringDupPtr(pOut, pToken);` |
|  136165 |   593 | `		return SXRET_OK;` |
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
|   68211 |   610 | `}` |
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
|       - |   627 | `/*` |
|       - |   628 | ` * Determine whether an integer literal token exceeds the signed 64-bit range.` |
|       - |   629 | ` * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->` |
|       - |   630 | ` * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or` |
|       - |   631 | ` * dropping digits. pNum is the separator-stripped token (unsigned; the sign of` |
|       - |   632 | ` * a "-1" is a separate unary operator). Base detection mirrors` |
|       - |   633 | ` * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the` |
|       - |   634 | ` * float value is accumulated into *pReal (dv = dv*base + digit); for decimal` |
|       - |   635 | ` * *pbDecimal is set so the caller reuses strtod on the token for a` |
|       - |   636 | ` * correctly-rounded value. Returns FALSE (value fits) for anything it cannot` |
|       - |   637 | ` * confidently classify, so the int path stays in charge.` |
|       - |   638 | ` *` |
|       - |   639 | ` * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact` |
|       - |   640 | ` * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit` |
|       - |   641 | ` * doubling). Octal/binary overflow values can differ from php by the low bit(s):` |
|       - |   642 | ` * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's` |
|       - |   643 | ` * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a` |
|       - |   644 | ` * residual in PLAN.md; matching php exactly would need a port of those functions.` |
|       - |   645 | ` */` |
|  135478 |   646 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|       5 |   647 | `{` |
|  135483 |   648 | `	const char *z = pNum->zString;` |
|  135483 |   649 | `	const char *zEnd = z + pNum->nByte;` |
|       - |   650 | `	const char *p, *q;` |
|       - |   651 | `	int n;` |
|  135483 |   652 | `	*pbDecimal = FALSE;` |
|  135483 |   653 | `	if( z >= zEnd ){` |
|     ! 0 |   654 | `		return FALSE;` |
|       - |   655 | `	}` |
|  135483 |   656 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|       - |   657 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|      77 |   658 | `		p = z + 2;` |
|      85 |   659 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|     493 |   660 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|      77 |   661 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|      71 |   662 | `			return FALSE;` |
|       - |   663 | `		}` |
|       7 |   664 | `		{ ph7_real dv = 0;` |
|     103 |   665 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|      97 |   666 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|      49 |   667 | `		  }` |
|       7 |   668 | `		  *pReal = dv;` |
|       - |   669 | `		}` |
|       7 |   670 | `		return TRUE;` |
|  135407 |   671 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       - |   672 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|     281 |   673 | `		p = z + 2;` |
|     329 |   674 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|    2149 |   675 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|     281 |   676 | `		if( n <= 63 ){` |
|     279 |   677 | `			return FALSE;` |
|       - |   678 | `		}` |
|       3 |   679 | `		{ ph7_real dv = 0;` |
|     195 |   680 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|     129 |   681 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|      65 |   682 | `		  }` |
|       3 |   683 | `		  *pReal = dv;` |
|       - |   684 | `		}` |
|       3 |   685 | `		return TRUE;` |
|  135127 |   686 | `	}else if( z[0] == '0' ){` |
|       - |   687 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|       - |   688 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|       - |   689 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   55749 |   690 | `		p = z;` |
|  111495 |   691 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   55977 |   692 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   55749 |   693 | `		if( n <= 21 ){` |
|   55747 |   694 | `			return FALSE;` |
|       - |   695 | `		}` |
|       3 |   696 | `		{ ph7_real dv = 0;` |
|      47 |   697 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|      45 |   698 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|      23 |   699 | `		  }` |
|       3 |   700 | `		  *pReal = dv;` |
|       - |   701 | `		}` |
|       3 |   702 | `		return TRUE;` |
|       - |   703 | `	}` |
|       - |   704 | `	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that` |
|       - |   705 | `	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)` |
|       - |   706 | `	 * for php-exact rounding. */` |
|   79383 |   707 | `	p = z;` |
|   79383 |   708 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  171451 |   709 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   79383 |   710 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|      13 |   711 | `		*pbDecimal = TRUE;` |
|      13 |   712 | `		return TRUE;` |
|       - |   713 | `	}` |
|   79371 |   714 | `	return FALSE;` |
|   67744 |   715 | `}` |
|  136398 |   716 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   717 | `{` |
|  136403 |   718 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  136403 |   719 | `	sxu32 nIdx = 0;` |
|       - |   720 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  136403 |   721 | `	char *zAlloc = 0;` |
|       - |   722 | `	SyString sNum;` |
|       - |   723 | `	sxi32 rc;` |
|   68199 |   724 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  136403 |   725 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  136403 |   726 | `	if( rc != SXRET_OK ){` |
|      14 |   727 | `		return rc;` |
|       - |   728 | `	}` |
|  204587 |   729 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   68194 |   730 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  136393 |   731 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   732 | `		return SXERR_ABORT;` |
|       - |   733 | `	}` |
|  136393 |   734 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   735 | `		ph7_value *pObj;` |
|       - |   736 | `		sxi64 iValue;` |
|  135483 |   737 | `		ph7_real rOverflow = 0;` |
|  135483 |   738 | `		int bDecimalOverflow = 0;` |
|  135483 |   739 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|       - |   740 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|       - |   741 | `			 * float instead of wrapping/dropping digits. */` |
|      23 |   742 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      23 |   743 | `			if( pObj == 0 ){` |
|     ! 0 |   744 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   745 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   746 | `				return SXERR_ABORT;` |
|       - |   747 | `			}` |
|      23 |   748 | `			if( bDecimalOverflow ){` |
|       - |   749 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|      13 |   750 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      13 |   751 | `				PH7_MemObjToReal(pObj);` |
|       7 |   752 | `			}else{` |
|      11 |   753 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|       - |   754 | `			}` |
|      12 |   755 | `		}else{` |
|  135461 |   756 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  135461 |   757 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  135461 |   758 | `			if( pObj == 0 ){` |
|     ! 0 |   759 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   760 | `				return SXERR_ABORT;` |
|       - |   761 | `			}` |
|  135461 |   762 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|       - |   763 | `		}` |
|   67744 |   764 | `	}else{` |
|       - |   765 | `		/* Real number */` |
|       - |   766 | `		ph7_value *pObj;` |
|       - |   767 | `		/* Reserve a new constant */` |
|     915 |   768 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     915 |   769 | `		if( pObj == 0 ){` |
|     ! 0 |   770 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   771 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   772 | `			return SXERR_ABORT;` |
|       - |   773 | `		}` |
|     915 |   774 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     915 |   775 | `		PH7_MemObjToReal(pObj);` |
|       - |   776 | `	}` |
|  136393 |   777 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   778 | `	/* Emit the load constant instruction */` |
|  136393 |   779 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   780 | `	/* Node successfully compiled */` |
|  136393 |   781 | `	return SXRET_OK;` |
|   68204 |   782 | `}` |
|       - |   783 | `/*` |
|       - |   784 | ` * Compile a single quoted string.` |
|       - |   785 | ` * According to the PHP language reference manual:` |
|       - |   786 | ` *` |
|       - |   787 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |   788 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |   789 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |   790 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |   791 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |   792 | ` *` |
|       - |   793 | ` */` |
|  172456 |   794 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   795 | `{` |
|  172461 |   796 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   797 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   798 | `	ph7_value *pObj;` |
|       - |   799 | `	sxu32 nIdx;` |
|  172461 |   800 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   801 | `	/* Delimit the string */` |
|  172461 |   802 | `	zIn  = pStr->zString;` |
|  172461 |   803 | `	zEnd = &zIn[pStr->nByte];` |
|  172461 |   804 | `	if( zIn >= zEnd ){` |
|       - |   805 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   806 | `		 * rather than reserving a new object each time. */` |
|    7873 |   807 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7873 |   808 | `		return SXRET_OK;` |
|       - |   809 | `	}` |
|  164593 |   810 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   811 | `		/* Already processed,emit the load constant instruction` |
|       - |   812 | `		 * and return.` |
|       - |   813 | `		 */` |
|   45411 |   814 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   45411 |   815 | `		return SXRET_OK;` |
|       - |   816 | `	}` |
|       - |   817 | `	/* Reserve a new constant */` |
|  119187 |   818 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  119187 |   819 | `	if( pObj == 0 ){` |
|     ! 0 |   820 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   821 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   822 | `		return SXERR_ABORT;` |
|       - |   823 | `	}` |
|  119187 |   824 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   825 | `	/* Compile the node */` |
|  119241 |   826 | `	for(;;){` |
|  238487 |   827 | `		if( zIn >= zEnd ){` |
|       - |   828 | `			/* End of input */` |
|  119187 |   829 | `			break;` |
|       - |   830 | `		}` |
|  119305 |   831 | `		zCur = zIn;` |
| 2468043 |   832 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 2348743 |   833 | `			zIn++;` |
|       5 |   834 | `		}` |
|  119305 |   835 | `		if( zIn > zCur ){` |
|       - |   836 | `			/* Append raw contents*/` |
|  119281 |   837 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   59638 |   838 | `		}` |
|  119305 |   839 | `		zIn++;` |
|  119305 |   840 | `		if( zIn < zEnd ){` |
|     141 |   841 | `			if( zIn[0] == '\\' ){` |
|       - |   842 | `				/* A literal backslash */` |
|      28 |   843 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     127 |   844 | `			}else if( zIn[0] == '\'' ){` |
|       - |   845 | `				/* A single quote */` |
|      11 |   846 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   847 | `			}else{` |
|       - |   848 | `				/* verbatim copy */` |
|     104 |   849 | `				zIn--;` |
|     104 |   850 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     104 |   851 | `				zIn++;` |
|       - |   852 | `			}` |
|      69 |   853 | `		}` |
|       - |   854 | `		/* Advance the stream cursor */` |
|  119305 |   855 | `		zIn++;` |
|       5 |   856 | `	}` |
|       - |   857 | `	/* Emit the load constant instruction */` |
|  119187 |   858 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  119187 |   859 | `	if( pStr->nByte < 1024 ){` |
|       - |   860 | `		/* Install in the literal table */` |
|  119187 |   861 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   59591 |   862 | `	}` |
|       - |   863 | `	/* Node successfully compiled */` |
|  119187 |   864 | `	return SXRET_OK;` |
|   86233 |   865 | `}` |
|       - |   866 | `/*` |
|       - |   867 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |   868 | ` *` |
|       - |   869 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |   870 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |   871 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |   872 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |   873 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |   874 | ` *` |
|       - |   875 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |   876 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |   877 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |   878 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |   879 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |   880 | ` *     whitespace.` |
|       - |   881 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |   882 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |   883 | ` */` |
|     114 |   884 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       5 |   885 | `{` |
|     119 |   886 | `	SyString *pIn = &pGen->pIn->sData;` |
|     119 |   887 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   888 | `	const char *zPrefix;` |
|       - |   889 | `	const char *z, *zEnd;` |
|       - |   890 | `	char *zBuf, *zDst;` |
|     119 |   891 | `	if( nIndent == 0 ){` |
|       - |   892 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      73 |   893 | `		*pOut = *pIn;` |
|      73 |   894 | `		return SXRET_OK;` |
|       - |   895 | `	}` |
|       - |   896 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |   897 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |   898 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |   899 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |   900 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |   901 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      48 |   902 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      48 |   903 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   904 | `		zPrefix += 2;` |
|     ! 0 |   905 | `	}else{` |
|      48 |   906 | `		zPrefix += 1;` |
|       - |   907 | `	}` |
|       - |   908 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      48 |   909 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      48 |   910 | `	if( zBuf == 0 ){` |
|     ! 0 |   911 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   912 | `		return SXERR_ABORT;` |
|       - |   913 | `	}` |
|      48 |   914 | `	zDst = zBuf;` |
|      48 |   915 | `	z = pIn->zString;` |
|      48 |   916 | `	zEnd = z + pIn->nByte;` |
|     130 |   917 | `	while( z < zEnd ){` |
|      72 |   918 | `		const char *zLine = z;` |
|       - |   919 | `		sxu32 nLine;` |
|       - |   920 | `		int bEmpty;` |
|     800 |   921 | `		while( z < zEnd && z[0] != '\n' ){` |
|     732 |   922 | `			z++;` |
|       4 |   923 | `		}` |
|      72 |   924 | `		nLine = (sxu32)(z - zLine);` |
|      72 |   925 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      72 |   926 | `		if( !bEmpty ){` |
|       - |   927 | `			sxu32 i;` |
|      68 |   928 | `			if( nLine < nIndent ){` |
|     ! 0 |   929 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   930 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   931 | `					nIndent);` |
|     ! 0 |   932 | `				return SXERR_ABORT;` |
|       - |   933 | `			}` |
|     270 |   934 | `			for( i = 0; i < nIndent; i++ ){` |
|     214 |   935 | `				if( zLine[i] != zPrefix[i] ){` |
|      11 |   936 | `					unsigned char c = (unsigned char)zLine[i];` |
|      11 |   937 | `					if( c == ' ' \|\| c == '\t' ){` |
|       6 |   938 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   939 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       4 |   940 | `					}else{` |
|       8 |   941 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   942 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   943 | `							nIndent);` |
|       - |   944 | `					}` |
|      11 |   945 | `					return SXERR_ABORT;` |
|       - |   946 | `				}` |
|     104 |   947 | `			}` |
|      57 |   948 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      57 |   949 | `			zDst += nLine - nIndent;` |
|      33 |   950 | `		}else if( nLine == 1 ){` |
|       - |   951 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |   952 | `			*zDst++ = '\r';` |
|     ! 0 |   953 | `		}` |
|      61 |   954 | `		if( z < zEnd ){` |
|      25 |   955 | `			*zDst++ = '\n';` |
|      25 |   956 | `			z++;` |
|      12 |   957 | `		}` |
|       1 |   958 | `	}` |
|      37 |   959 | `	pOut->zString = zBuf;` |
|      37 |   960 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      37 |   961 | `	return SXRET_OK;` |
|      62 |   962 | `}` |
|       - |   963 | `/*` |
|       - |   964 | ` * Compile a nowdoc string.` |
|       - |   965 | ` * According to the PHP language reference manual:` |
|       - |   966 | ` *` |
|       - |   967 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |   968 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |   969 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |   970 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |   971 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |   972 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |   973 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |   974 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |   975 | ` *  of the closing identifier.` |
|       - |   976 | ` */` |
|      48 |   977 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       3 |   978 | `{` |
|       - |   979 | `	SyString sStripped;` |
|       - |   980 | `	SyString *pStr;` |
|       - |   981 | `	ph7_value *pObj;` |
|       - |   982 | `	sxu32 nIdx;` |
|       - |   983 | `	sxi32 rc;` |
|      51 |   984 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      51 |   985 | `	if( rc != SXRET_OK ){` |
|       6 |   986 | `		return rc;` |
|       - |   987 | `	}` |
|      46 |   988 | `	pStr = &sStripped;` |
|      46 |   989 | `	nIdx = 0; /* Prevent compiler warning */` |
|      46 |   990 | `	if( pStr->nByte <= 0 ){` |
|       - |   991 | `		/* Empty string,load NULL */` |
|       7 |   992 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |   993 | `		return SXRET_OK;` |
|       - |   994 | `	}` |
|       - |   995 | `	/* Reserve a new constant */` |
|      40 |   996 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      40 |   997 | `	if( pObj == 0 ){` |
|     ! 0 |   998 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   999 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1000 | `		return SXERR_ABORT;` |
|       - |  1001 | `	}` |
|       - |  1002 | `	/* No processing is done here, simply a memcpy() operation */` |
|      40 |  1003 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |  1004 | `	/* Emit the load constant instruction */` |
|      40 |  1005 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1006 | `	/* Node successfully compiled */` |
|      40 |  1007 | `	return SXRET_OK;` |
|      27 |  1008 | `}` |
|       - |  1009 | `/*` |
|       - |  1010 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |  1011 | ` * According to the PHP language reference manual` |
|       - |  1012 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |  1013 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |  1014 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |  1015 | ` *  property in a string with a minimum of effort.` |
|       - |  1016 | ` *  Simple syntax` |
|       - |  1017 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |  1018 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |  1019 | ` *   the end of the name.` |
|       - |  1020 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |  1021 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |  1022 | ` *   as to simple variables.` |
|       - |  1023 | ` *  Complex (curly) syntax` |
|       - |  1024 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |  1025 | ` *   of complex expressions.` |
|       - |  1026 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |  1027 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |  1028 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |  1029 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |  1030 | ` */` |
|    2382 |  1031 | `static sxi32 GenStateProcessStringExpression(` |
|       - |  1032 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1033 | `	sxu32 nLine,         /* Line number */` |
|       - |  1034 | `	const char *zIn,     /* Raw expression */` |
|       - |  1035 | `	const char *zEnd     /* End of the expression */` |
|       - |  1036 | `	)` |
|       5 |  1037 | `{` |
|       - |  1038 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1039 | `	SySet sToken;` |
|       - |  1040 | `	sxi32 rc;` |
|       - |  1041 | `	/* Initialize the token set */` |
|    2387 |  1042 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  1043 | `	/* Preallocate some slots */` |
|    2387 |  1044 | `	SySetAlloc(&sToken,0x08);` |
|       - |  1045 | `	/* Tokenize the text */` |
|    2387 |  1046 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  1047 | `	/* Swap delimiter */` |
|    2387 |  1048 | `	pTmpIn  = pGen->pIn;` |
|    2387 |  1049 | `	pTmpEnd = pGen->pEnd;` |
|    2387 |  1050 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2387 |  1051 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  1052 | `	/* Compile the expression */` |
|    2387 |  1053 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  1054 | `	/* Restore token stream */` |
|    2387 |  1055 | `	pGen->pIn  = pTmpIn;` |
|    2387 |  1056 | `	pGen->pEnd = pTmpEnd;` |
|       - |  1057 | `	/* Release the token set */` |
|    2387 |  1058 | `	SySetRelease(&sToken);` |
|       - |  1059 | `	/* Compilation result */` |
|    2387 |  1060 | `	return rc;` |
|       5 |  1061 | `}` |
|       - |  1062 | `/*` |
|       - |  1063 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  1064 | ` */` |
|   27658 |  1065 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |  1066 | `{` |
|       - |  1067 | `	ph7_value *pConstObj;` |
|   27663 |  1068 | `	sxu32 nIdx = 0;` |
|       - |  1069 | `	/* Reserve a new constant */` |
|   27663 |  1070 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   27663 |  1071 | `	if( pConstObj == 0 ){` |
|     ! 0 |  1072 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  1073 | `		return 0;` |
|       - |  1074 | `	}` |
|   27663 |  1075 | `	(*pCount)++;` |
|   27663 |  1076 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  1077 | `	/* Emit the load constant instruction */` |
|   27663 |  1078 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   27663 |  1079 | `	return pConstObj;` |
|   13834 |  1080 | `}` |
|       - |  1081 | `/*` |
|       - |  1082 | ` * Compile a double quoted/heredoc string.` |
|       - |  1083 | ` * According to the PHP language reference manual` |
|       - |  1084 | ` * Heredoc` |
|       - |  1085 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |  1086 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |  1087 | ` *  to close the quotation.` |
|       - |  1088 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |  1089 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |  1090 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |  1091 | ` *  Warning` |
|       - |  1092 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |  1093 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |  1094 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |  1095 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |  1096 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |  1097 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |  1098 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |  1099 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |  1100 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |  1101 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |  1102 | ` * Double quoted` |
|       - |  1103 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |  1104 | ` *  Escaped characters Sequence 	Meaning` |
|       - |  1105 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |  1106 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |  1107 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |  1108 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |  1109 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|       - |  1110 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  1111 | ` *  \\ backslash` |
|       - |  1112 | ` *  \$ dollar sign` |
|       - |  1113 | ` *  \" double-quote` |
|       - |  1114 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|       - |  1115 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|       - |  1116 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  1117 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|       - |  1118 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|       - |  1119 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  1120 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|       - |  1121 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  1122 | ` * See string parsing for details.` |
|       - |  1123 | ` */` |
|       - |  1124 | `/*` |
|       - |  1125 | ` * Line number of an escape sequence inside the string body being compiled:` |
|       - |  1126 | ` * the token's line plus every newline before the escape (php reports the` |
|       - |  1127 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|       - |  1128 | ` * on the line after the '<<<' marker, hence the +1.` |
|       - |  1129 | ` */` |
|       6 |  1130 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|       3 |  1131 | `{` |
|       9 |  1132 | `	const char *z = pGen->pIn->sData.zString;` |
|       9 |  1133 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|      15 |  1134 | `	for( ; z < zPos ; z++ ){` |
|       9 |  1135 | `		if( z[0] == '\n' ){` |
|     ! 0 |  1136 | `			nLine++;` |
|     ! 0 |  1137 | `		}` |
|       6 |  1138 | `	}` |
|       9 |  1139 | `	return nLine;` |
|       3 |  1140 | `}` |
|       - |  1141 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|       - |  1142 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|   26098 |  1143 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|       5 |  1144 | `{` |
|   26103 |  1145 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1146 | `	const char *zIn,*zCur,*zEnd;` |
|   26103 |  1147 | `	ph7_value *pObj = 0;` |
|       - |  1148 | `	sxi32 iCons;` |
|       - |  1149 | `	sxi32 rc;` |
|       - |  1150 | `	/* Delimit the string */` |
|   26103 |  1151 | `	zIn  = pStr->zString;` |
|   26103 |  1152 | `	zEnd = &zIn[pStr->nByte];` |
|   26103 |  1153 | `	if( zIn >= zEnd ){` |
|       - |  1154 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1155 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1156 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1157 | `		 */` |
|     317 |  1158 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     317 |  1159 | `		return SXRET_OK;` |
|       - |  1160 | `	}` |
|   25791 |  1161 | `	zCur = 0;` |
|       - |  1162 | `	/* Compile the node */` |
|   25791 |  1163 | `	iCons = 0;` |
|   14084 |  1164 | `	for(;;){` |
|   42497 |  1165 | `		zCur = zIn;` |
|  189157 |  1166 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  149047 |  1167 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      69 |  1168 | `				break;` |
|  148919 |  1169 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2258 |  1170 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1130 |  1171 | `					break;` |
|       - |  1172 | `			}` |
|  146665 |  1173 | `			zIn++;` |
|       5 |  1174 | `		}` |
|   42497 |  1175 | `		if( zIn > zCur ){` |
|   18953 |  1176 | `			if( pObj == 0 ){` |
|   18433 |  1177 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   18433 |  1178 | `				if( pObj == 0 ){` |
|     ! 0 |  1179 | `					return SXERR_ABORT;` |
|       - |  1180 | `				}` |
|    9214 |  1181 | `			}` |
|   18953 |  1182 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    9474 |  1183 | `		}` |
|   42497 |  1184 | `		if( zIn >= zEnd ){` |
|   25789 |  1185 | `			break;` |
|       - |  1186 | `		}` |
|   16713 |  1187 | `		if( zIn[0] == '\\' ){` |
|   14331 |  1188 | `			const char *zPtr = 0;` |
|       - |  1189 | `			sxu32 n;` |
|   14331 |  1190 | `			zIn++;` |
|   14331 |  1191 | `			if( pObj == 0 ){` |
|    9235 |  1192 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    9235 |  1193 | `				if( pObj == 0 ){` |
|     ! 0 |  1194 | `					return SXERR_ABORT;` |
|       - |  1195 | `				}` |
|    4615 |  1196 | `			}` |
|   14331 |  1197 | `			if( zIn >= zEnd ){` |
|       - |  1198 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|       3 |  1199 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       3 |  1200 | `				break;` |
|       - |  1201 | `			}` |
|   14329 |  1202 | `			n = sizeof(char); /* size of conversion */` |
|   14329 |  1203 | `			switch( zIn[0] ){` |
|      11 |  1204 | `			case '$':` |
|       - |  1205 | `				/* Dollar sign */` |
|      25 |  1206 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|      25 |  1207 | `				break;` |
|      56 |  1208 | `			case '\\':` |
|       - |  1209 | `				/* A literal backslash */` |
|     117 |  1210 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     117 |  1211 | `				break;` |
|       1 |  1212 | `			case 'e':` |
|       - |  1213 | `				/* Escape (ESC) ASCII code 27 */` |
|       3 |  1214 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|       3 |  1215 | `				break;` |
|       4 |  1216 | `			case 'f':` |
|       - |  1217 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  1218 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  1219 | `				break;` |
|    6615 |  1220 | `			case 'n':` |
|       - |  1221 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   13235 |  1222 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   13235 |  1223 | `				break;` |
|      19 |  1224 | `			case 'r':` |
|       - |  1225 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      43 |  1226 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      43 |  1227 | `				break;` |
|      26 |  1228 | `			case 't':` |
|       - |  1229 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      57 |  1230 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      57 |  1231 | `				break;` |
|       3 |  1232 | `			case 'v':` |
|       - |  1233 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  1234 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  1235 | `				break;` |
|     112 |  1236 | `			case '"':` |
|     229 |  1237 | `				if( bHeredoc ){` |
|       - |  1238 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|       5 |  1239 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|       3 |  1240 | `				}else{` |
|       - |  1241 | `					/* Double quote */` |
|     225 |  1242 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|       - |  1243 | `				}` |
|     229 |  1244 | `				break;` |
|      24 |  1245 | `			case '0': case '1': case '2': case '3':` |
|       - |  1246 | `			case '4': case '5': case '6': case '7': {` |
|       - |  1247 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|       - |  1248 | `				 * warns and wraps to the low byte, matching php 8. */` |
|      50 |  1249 | `				int c = 0;` |
|       - |  1250 | `				char cOut;` |
|     144 |  1251 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|     122 |  1252 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|      14 |  1253 | `						break;` |
|       - |  1254 | `					}` |
|      96 |  1255 | `					c = c * 8 + (zPtr[0] - '0');` |
|      49 |  1256 | `				}` |
|      50 |  1257 | `				if( c > 0xFF ){` |
|       - |  1258 | `					SyString sSeq;` |
|       3 |  1259 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|       3 |  1260 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - |  1261 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|       3 |  1262 | `					c &= 0xFF;` |
|       1 |  1263 | `				}` |
|      50 |  1264 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|      50 |  1265 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|      50 |  1266 | `				n = (sxu32)(zPtr-zIn);` |
|      50 |  1267 | `				break;` |
|       - |  1268 | `			}` |
|     270 |  1269 | `			case 'x':` |
|     809 |  1270 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|       - |  1271 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|     537 |  1272 | `					int c = SyHexToint(zIn[1]);` |
|       - |  1273 | `					char cOut;` |
|     537 |  1274 | `					n += sizeof(char);` |
|     537 |  1275 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|     533 |  1276 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|     533 |  1277 | `						n += sizeof(char);` |
|     266 |  1278 | `					}` |
|     537 |  1279 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|     537 |  1280 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|     269 |  1281 | `				}else{` |
|       - |  1282 | `					/* Not an escape: keep the backslash, as php does */` |
|       5 |  1283 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|       - |  1284 | `				}` |
|     541 |  1285 | `				break;` |
|       9 |  1286 | `			case 'u':` |
|      18 |  1287 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|      22 |  1288 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|       - |  1289 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|       - |  1290 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|       - |  1291 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|       - |  1292 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|       - |  1293 | `					 * followed by {$...} curly interpolation. */` |
|      15 |  1294 | `					sxu32 nCp = 0;` |
|      15 |  1295 | `					zPtr = &zIn[2];` |
|      59 |  1296 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|      46 |  1297 | `						if( nCp <= 0x10FFFF ){` |
|       - |  1298 | `							/* stop accumulating once out of range: keeps a long` |
|       - |  1299 | `							 * digit run from wrapping sxu32 */` |
|      46 |  1300 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|      22 |  1301 | `						}` |
|      46 |  1302 | `						zPtr++;` |
|       2 |  1303 | `					}` |
|      15 |  1304 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|       - |  1305 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|       - |  1306 | `						 * malformed sequence so later errors are still reported. */` |
|       3 |  1307 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - |  1308 | `							"Invalid UTF-8 codepoint escape sequence");` |
|       3 |  1309 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  1310 | `							return SXERR_ABORT;` |
|       - |  1311 | `						}` |
|       3 |  1312 | `						n = (sxu32)(zPtr-zIn);` |
|       3 |  1313 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|       3 |  1314 | `							n += sizeof(char);` |
|       1 |  1315 | `						}` |
|       3 |  1316 | `						break;` |
|       - |  1317 | `					}` |
|      12 |  1318 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|      12 |  1319 | `					if( nCp > 0x10FFFF ){` |
|       3 |  1320 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - |  1321 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|       3 |  1322 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  1323 | `							return SXERR_ABORT;` |
|       - |  1324 | `						}` |
|       3 |  1325 | `						break;` |
|       - |  1326 | `					}` |
|       - |  1327 | `					{` |
|       - |  1328 | `						char zUtf[4];` |
|       9 |  1329 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|       9 |  1330 | `						SX_WRITE_UTF8(zOut,nCp);` |
|       9 |  1331 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|       - |  1332 | `					}` |
|       5 |  1333 | `				}else{` |
|       - |  1334 | `					/* Not an escape: keep the backslash, as php does */` |
|       7 |  1335 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|       - |  1336 | `				}` |
|      15 |  1337 | `				break;` |
|      12 |  1338 | `			default:` |
|       - |  1339 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|       - |  1340 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|       - |  1341 | `				 * in the source buffer — one batched append. */` |
|      25 |  1342 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|      24 |  1343 | `				break;` |
|       - |  1344 | `			}` |
|       - |  1345 | `			/* Advance the stream cursor */` |
|   14329 |  1346 | `			zIn += n;` |
|   14329 |  1347 | `			continue;` |
|       - |  1348 | `		}` |
|    2387 |  1349 | `		if( zIn[0] == '{' ){` |
|       - |  1350 | `			/* Curly syntax */` |
|       - |  1351 | `			const char *zExpr;` |
|     135 |  1352 | `			sxi32 iNest = 1;` |
|     135 |  1353 | `			zIn++;` |
|     135 |  1354 | `			zExpr = zIn;` |
|       - |  1355 | `			/* Synchronize with the next closing curly braces */` |
|    1383 |  1356 | `			while( zIn < zEnd ){` |
|    1383 |  1357 | `				if( zIn[0] == '{' ){` |
|       - |  1358 | `					/* Increment nesting level */` |
|       9 |  1359 | `					iNest++;` |
|    1379 |  1360 | `				}else if(zIn[0] == '}' ){` |
|       - |  1361 | `					/* Decrement nesting level */` |
|     143 |  1362 | `					iNest--;` |
|     143 |  1363 | `					if( iNest <= 0 ){` |
|     135 |  1364 | `						break;` |
|       - |  1365 | `					}` |
|       4 |  1366 | `				}` |
|    1251 |  1367 | `				zIn++;` |
|       3 |  1368 | `			}` |
|       - |  1369 | `			/* Process the expression */` |
|     135 |  1370 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     135 |  1371 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1372 | `				return SXERR_ABORT;` |
|       - |  1373 | `			}` |
|     135 |  1374 | `			if( rc != SXERR_EMPTY ){` |
|     135 |  1375 | `				++iCons;` |
|      66 |  1376 | `			}` |
|     135 |  1377 | `			if( zIn < zEnd ){` |
|       - |  1378 | `				/* Jump the trailing curly */` |
|     135 |  1379 | `				zIn++;` |
|      66 |  1380 | `			}` |
|      69 |  1381 | `		}else{` |
|       - |  1382 | `			/* Simple syntax */` |
|    2255 |  1383 | `			const char *zExpr = zIn;` |
|       - |  1384 | `			/* Assemble variable name */` |
|    1150 |  1385 | `			for(;;){` |
|       - |  1386 | `				/* Jump leading dollars */` |
|    4555 |  1387 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2255 |  1388 | `					zIn++;` |
|       5 |  1389 | `				}` |
|    1150 |  1390 | `				for(;;){` |
|   12275 |  1391 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8825 |  1392 | `						zIn++;` |
|       5 |  1393 | `					}` |
|    2305 |  1394 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1395 | `						/* UTF-8 stream */` |
|     ! 0 |  1396 | `						zIn++;` |
|     ! 0 |  1397 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1398 | `							zIn++;` |
|     ! 0 |  1399 | `						}` |
|     ! 0 |  1400 | `						continue;` |
|       - |  1401 | `					}` |
|    2305 |  1402 | `					break;` |
|     ! 0 |  1403 | `				}` |
|    2305 |  1404 | `				if( zIn >= zEnd ){` |
|     226 |  1405 | `					break;` |
|       - |  1406 | `				}` |
|    2083 |  1407 | `				if( zIn[0] == '[' ){` |
|      12 |  1408 | `					sxi32 iSquare = 1;` |
|      12 |  1409 | `					zIn++;` |
|      28 |  1410 | `					while( zIn < zEnd ){` |
|      28 |  1411 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  1412 | `							iSquare++;` |
|      28 |  1413 | `						}else if (zIn[0] == ']' ){` |
|      12 |  1414 | `							iSquare--;` |
|      12 |  1415 | `							if( iSquare <= 0 ){` |
|      12 |  1416 | `								break;` |
|       - |  1417 | `							}` |
|     ! 0 |  1418 | `						}` |
|      18 |  1419 | `						zIn++;` |
|       2 |  1420 | `					}` |
|      12 |  1421 | `					if( zIn < zEnd ){` |
|      12 |  1422 | `						zIn++;` |
|       5 |  1423 | `					}` |
|      12 |  1424 | `					break;` |
|    2073 |  1425 | `				}else if(zIn[0] == '{' ){` |
|       6 |  1426 | `					sxi32 iCurly = 1;` |
|       6 |  1427 | `					zIn++;` |
|      18 |  1428 | `					while( zIn < zEnd ){` |
|      16 |  1429 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  1430 | `							iCurly++;` |
|      16 |  1431 | `						}else if (zIn[0] == '}' ){` |
|       3 |  1432 | `							iCurly--;` |
|       3 |  1433 | `							if( iCurly <= 0 ){` |
|       3 |  1434 | `								break;` |
|       - |  1435 | `							}` |
|     ! 0 |  1436 | `						}` |
|      14 |  1437 | `						zIn++;` |
|       2 |  1438 | `					}` |
|       6 |  1439 | `					if( zIn < zEnd ){` |
|       3 |  1440 | `						zIn++;` |
|       1 |  1441 | `					}` |
|       6 |  1442 | `					break;` |
|    2069 |  1443 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1444 | `					/* Member access operator '->' */` |
|      53 |  1445 | `					zIn += 2;` |
|    2044 |  1446 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1447 | `					/* Static member access operator '::' */` |
|     ! 0 |  1448 | `					zIn += 2;` |
|     ! 0 |  1449 | `				}else{` |
|    1012 |  1450 | `					break;` |
|       - |  1451 | `				}` |
|       3 |  1452 | `			}` |
|       - |  1453 | `			/* Process the expression */` |
|    2255 |  1454 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2255 |  1455 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1456 | `				return SXERR_ABORT;` |
|       - |  1457 | `			}` |
|    2255 |  1458 | `			if( rc != SXERR_EMPTY ){` |
|    2253 |  1459 | `				++iCons;` |
|    1124 |  1460 | `			}` |
|       - |  1461 | `		}` |
|       - |  1462 | `		/* Invalidate the previously used constant */` |
|    2387 |  1463 | `		pObj = 0;` |
|       5 |  1464 | `	}/*for(;;)*/` |
|   25791 |  1465 | `	if( iCons > 1 ){` |
|       - |  1466 | `		/* Concatenate all compiled constants */` |
|    1759 |  1467 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     877 |  1468 | `	}` |
|       - |  1469 | `	/* Node successfully compiled */` |
|   25791 |  1470 | `	return SXRET_OK;` |
|   13054 |  1471 | `}` |
|       - |  1472 | `/*` |
|       - |  1473 | ` * Compile a double quoted string.` |
|       - |  1474 | ` *  See the block-comment above for more information.` |
|       - |  1475 | ` */` |
|   26036 |  1476 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1477 | `{` |
|       - |  1478 | `	sxi32 rc;` |
|   26041 |  1479 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|   13018 |  1480 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1481 | `	/* Compilation result */` |
|   26041 |  1482 | `	return rc;` |
|       5 |  1483 | `}` |
|       - |  1484 | `/*` |
|       - |  1485 | ` * Compile a Heredoc string.` |
|       - |  1486 | ` *  See the block-comment above for more information.` |
|       - |  1487 | ` */` |
|      66 |  1488 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1489 | `{` |
|       - |  1490 | `	SyString sOrig, sStripped;` |
|       - |  1491 | `	sxi32 rc;` |
|      71 |  1492 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      71 |  1493 | `	if( rc != SXRET_OK ){` |
|       6 |  1494 | `		return rc;` |
|       - |  1495 | `	}` |
|       - |  1496 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - |  1497 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - |  1498 | `	 * Restore before returning so downstream code that references pIn is` |
|       - |  1499 | `	 * unaffected, including on the error path. */` |
|      65 |  1500 | `	sOrig = pGen->pIn->sData;` |
|      65 |  1501 | `	pGen->pIn->sData = sStripped;` |
|      65 |  1502 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|      65 |  1503 | `	pGen->pIn->sData = sOrig;` |
|      31 |  1504 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      65 |  1505 | `	return rc;` |
|      38 |  1506 | `}` |
|       - |  1507 | `/*` |
|       - |  1508 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  1509 | ` *  Notes on array entries.` |
|       - |  1510 | ` *  According to the PHP language reference manual` |
|       - |  1511 | ` *  An array can be created by the array() language construct.` |
|       - |  1512 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  1513 | ` *  array(  key =>  value` |
|       - |  1514 | ` *    , ...` |
|       - |  1515 | ` *    )` |
|       - |  1516 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - |  1517 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - |  1518 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - |  1519 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - |  1520 | ` *  contain integer and string indices.` |
|       - |  1521 | ` *  A value can be any PHP type.` |
|       - |  1522 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - |  1523 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - |  1524 | ` *  is specified, that value will be overwritten.` |
|       - |  1525 | ` */` |
|   23946 |  1526 | `static sxi32 GenStateCompileArrayEntry(` |
|       - |  1527 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1528 | `	SyToken *pIn,        /* Token stream */` |
|       - |  1529 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - |  1530 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - |  1531 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - |  1532 | `	)` |
|       5 |  1533 | `{` |
|       - |  1534 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1535 | `	sxi32 rc;` |
|       - |  1536 | `	/* Swap token stream */` |
|   23951 |  1537 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1538 | `	/* Compile the expression*/` |
|   23951 |  1539 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1540 | `	/* Restore token stream */` |
|   23951 |  1541 | `	RE_SWAP_DELIMITER(pGen);` |
|   23951 |  1542 | `	return rc;` |
|       5 |  1543 | `}` |
|       - |  1544 | `/*` |
|       - |  1545 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - |  1546 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1547 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1548 | ` * error message.` |
|       - |  1549 | ` * See the routine responible of compiling the array language construct` |
|       - |  1550 | ` * for more inforation.` |
|       - |  1551 | ` */` |
|      36 |  1552 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  1553 | `{` |
|      41 |  1554 | `	sxi32 rc = SXRET_OK;` |
|      41 |  1555 | `	if( pRoot->pOp ){` |
|      14 |  1556 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1557 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      17 |  1558 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1559 | `			/* Unexpected expression */` |
|      14 |  1560 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1561 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      14 |  1562 | `			if( rc != SXERR_ABORT ){` |
|      14 |  1563 | `				rc = SXERR_INVALID;` |
|       5 |  1564 | `			}` |
|      10 |  1565 | `		}` |
|      31 |  1566 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1567 | `		/* Unexpected expression */` |
|       3 |  1568 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1569 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1570 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1571 | `			rc = SXERR_INVALID;` |
|       1 |  1572 | `		}` |
|       1 |  1573 | `	}` |
|      41 |  1574 | `	return rc;` |
|       5 |  1575 | `}` |
|       - |  1576 | `/*` |
|       - |  1577 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - |  1578 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - |  1579 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - |  1580 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - |  1581 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - |  1582 | ` */` |
|   34192 |  1583 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1584 | `{` |
|   34197 |  1585 | `	SyToken *pCur = pStart;` |
|   34197 |  1586 | `	sxi32 iNest = 0;` |
|   98139 |  1587 | `	while( pCur < pEnd ){` |
|   69851 |  1588 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5905 |  1589 | `			return pCur;` |
|       - |  1590 | `		}` |
|       - |  1591 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1592 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1593 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1594 | `		 */` |
|   63951 |  1595 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      95 |  1596 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      95 |  1597 | `			SyToken *pFn = pCur;` |
|      92 |  1598 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|     ! 0 |  1599 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3 |  1600 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1601 | `				pFn = &pCur[1];` |
|     ! 0 |  1602 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1603 | `			}` |
|      95 |  1604 | `			if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1605 | `				pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1606 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1607 | `					pCur++;` |
|     ! 0 |  1608 | `				}` |
|       5 |  1609 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1610 | `					pCur++;` |
|       5 |  1611 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1612 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1613 | `					if( pCur < pEnd ){` |
|       5 |  1614 | `						pCur++;` |
|       2 |  1615 | `					}` |
|       2 |  1616 | `				}` |
|       5 |  1617 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1618 | `					pCur++;` |
|     ! 0 |  1619 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1620 | `						&& pCur->sData.nByte == 1` |
|     ! 0 |  1621 | `						&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1622 | `						pCur++;` |
|     ! 0 |  1623 | `					}` |
|     ! 0 |  1624 | `					if( pCur < pEnd` |
|     ! 0 |  1625 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1626 | `						pCur++;` |
|     ! 0 |  1627 | `					}` |
|     ! 0 |  1628 | `				}` |
|       - |  1629 | `				/* The rest of the entry is the arrow-function body — no outer` |
|       - |  1630 | `				 * key to extract. */` |
|       5 |  1631 | `				return pEnd;` |
|       - |  1632 | `			}` |
|       - |  1633 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|       - |  1634 | `			 * entry separator. Skip past the full match span. */` |
|      91 |  1635 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1636 | `				pCur++; /* past 'match' */` |
|       3 |  1637 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1638 | `					pCur++;` |
|       3 |  1639 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1640 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1641 | `					if( pCur < pEnd ){` |
|       3 |  1642 | `						pCur++;` |
|       1 |  1643 | `					}` |
|       1 |  1644 | `				}` |
|       3 |  1645 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1646 | `					pCur++;` |
|       3 |  1647 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1648 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1649 | `					if( pCur < pEnd ){` |
|       3 |  1650 | `						pCur++;` |
|       1 |  1651 | `					}` |
|       1 |  1652 | `				}` |
|       3 |  1653 | `				continue;` |
|       - |  1654 | `			}` |
|      43 |  1655 | `		}` |
|   63945 |  1656 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     421 |  1657 | `			iNest++;` |
|   63736 |  1658 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1659 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1660 | `			 * parser will shortly detect any syntax error. */` |
|     421 |  1661 | `			iNest--;` |
|     209 |  1662 | `		}` |
|   63945 |  1663 | `		pCur++;` |
|       5 |  1664 | `	}` |
|   28293 |  1665 | `	return pEnd;` |
|   17101 |  1666 | `}` |
|       - |  1667 | `/*` |
|       - |  1668 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1669 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1670 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1671 | ` */` |
|   33960 |  1672 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1673 | `{` |
|       - |  1674 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1675 | `	SyToken *pKey,*pCur;` |
|   33965 |  1676 | `	sxi32 iEmitRef = 0;` |
|   33965 |  1677 | `	sxi32 iSpread = 0;` |
|   33965 |  1678 | `	sxi32 nPair = 0;` |
|       - |  1679 | `	sxi32 rc;` |
|   33965 |  1680 | `	xValidator = 0;` |
|   27940 |  1681 | `	for(;;){` |
|       - |  1682 | `		/* Jump leading commas */` |
|   63573 |  1683 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    7693 |  1684 | `			pGen->pIn++;` |
|       5 |  1685 | `		}` |
|   55885 |  1686 | `		pCur = pGen->pIn;` |
|   55885 |  1687 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1688 | `			/* No more entry to process */` |
|   33949 |  1689 | `			break;` |
|       - |  1690 | `		}` |
|   21941 |  1691 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1692 | `			continue;` |
|       - |  1693 | `		}` |
|       - |  1694 | `		/* Compile the key if available */` |
|   21941 |  1695 | `		pKey = pCur;` |
|   21941 |  1696 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   21941 |  1697 | `		rc = SXERR_EMPTY;` |
|   21941 |  1698 | `		if( pCur < pGen->pIn ){` |
|    1771 |  1699 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1700 | `				/* Missing value */` |
|      13 |  1701 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1702 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1703 | `					return SXERR_ABORT;` |
|       - |  1704 | `				}` |
|      13 |  1705 | `				return SXRET_OK;` |
|       - |  1706 | `			}` |
|       - |  1707 | `			/* Compile the expression holding the key */` |
|    1761 |  1708 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1709 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1761 |  1710 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1711 | `				return SXERR_ABORT;` |
|       - |  1712 | `			}` |
|    1761 |  1713 | `			pCur++; /* Jump the '=>' operator */` |
|   21053 |  1714 | `		}else if( pKey == pCur ){` |
|       - |  1715 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1716 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1717 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1718 | `		}else{` |
|       - |  1719 | `			/* Reset back the cursor and point to the entry value */` |
|   20175 |  1720 | `			pCur = pKey;` |
|       - |  1721 | `		}` |
|   21931 |  1722 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1723 | `			/* No available key,load NULL */` |
|   20177 |  1724 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|   10086 |  1725 | `		}` |
|   21931 |  1726 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1727 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      45 |  1728 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      45 |  1729 | `			iEmitRef = 1;` |
|      45 |  1730 | `			pCur++; /* Jump the '&' token */` |
|      45 |  1731 | `			if( pCur >= pGen->pIn ){` |
|       - |  1732 | `				/* Missing value */` |
|       3 |  1733 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1734 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1735 | `					return SXERR_ABORT;` |
|       - |  1736 | `				}` |
|       3 |  1737 | `				return SXRET_OK;` |
|       - |  1738 | `			}` |
|      19 |  1739 | `		}` |
|       - |  1740 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - |  1741 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - |  1742 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - |  1743 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - |  1744 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   21929 |  1745 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   21929 |  1746 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - |  1747 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - |  1748 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - |  1749 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - |  1750 | `			 * output is engine-portable. */` |
|       6 |  1751 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - |  1752 | `				"syntax error, unexpected token \"...\"");` |
|       6 |  1753 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1754 | `				return SXERR_ABORT;` |
|       - |  1755 | `			}` |
|       6 |  1756 | `			return SXRET_OK;` |
|       - |  1757 | `		}` |
|       - |  1758 | `		/* Compile indice value */` |
|   21925 |  1759 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   21925 |  1760 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1761 | `			return SXERR_ABORT;` |
|       - |  1762 | `		}` |
|   21925 |  1763 | `		if( iSpread ){` |
|       - |  1764 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      64 |  1765 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   21894 |  1766 | `		}else if( iEmitRef ){` |
|       - |  1767 | `			/* Emit the load reference instruction */` |
|      41 |  1768 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1769 | `		}` |
|   21925 |  1770 | `		xValidator = 0;` |
|   21925 |  1771 | `		iEmitRef = 0;` |
|   21925 |  1772 | `		iSpread = 0;` |
|   21925 |  1773 | `		nPair++;` |
|       5 |  1774 | `	}` |
|       - |  1775 | `	/* Emit the load map instruction */` |
|   33949 |  1776 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1777 | `	/* Node successfully compiled */` |
|   33949 |  1778 | `	return SXRET_OK;` |
|   16985 |  1779 | `}` |
|       - |  1780 | `/*` |
|       - |  1781 | ` * Compile the 'array' language construct.` |
|       - |  1782 | ` *	 According to the PHP language reference manual` |
|       - |  1783 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1784 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1785 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1786 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1787 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1788 | ` */` |
|   32578 |  1789 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1790 | `{` |
|       - |  1791 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   32583 |  1792 | `	pGen->pIn += 2;` |
|   32583 |  1793 | `	pGen->pEnd--;` |
|   16289 |  1794 | `	SXUNUSED(iCompileFlag);` |
|   32583 |  1795 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1796 | `}` |
|       - |  1797 | `/*` |
|       - |  1798 | ` * Compile the PHP 8.5 clone(...) call form:` |
|       - |  1799 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|       - |  1800 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|       - |  1801 | ` *                                              property updates as scope-aware writes` |
|       - |  1802 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|       - |  1803 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|       - |  1804 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|       - |  1805 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|       - |  1806 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|       - |  1807 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|       - |  1808 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|       - |  1809 | ` */` |
|      22 |  1810 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1811 | `{` |
|       - |  1812 | `	SyToken *pIn,*pEnd,*pNext;` |
|      24 |  1813 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|      24 |  1814 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|      24 |  1815 | `	int nArg = 0;` |
|       - |  1816 | `	sxi32 rc;` |
|      11 |  1817 | `	SXUNUSED(iCompileFlag);` |
|       - |  1818 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|      24 |  1819 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|      24 |  1820 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|       - |  1821 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|      24 |  1822 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|     ! 0 |  1823 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  1824 | `			"clone(...) first-class callable form is not yet supported");` |
|       - |  1825 | `	}` |
|       - |  1826 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|      62 |  1827 | `	while( pIn < pEnd ){` |
|      40 |  1828 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|      40 |  1829 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|     ! 0 |  1830 | `			break;` |
|       - |  1831 | `		}` |
|      40 |  1832 | `		pArgStart = pIn;` |
|      40 |  1833 | `		pArgEnd   = pNext;` |
|       - |  1834 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|       - |  1835 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|      38 |  1836 | `		if( (pArgEnd - pArgStart) >= 2` |
|      37 |  1837 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      23 |  1838 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       5 |  1839 | `			pName = pArgStart;` |
|       5 |  1840 | `			pArgStart += 2;` |
|       2 |  1841 | `		}` |
|      40 |  1842 | `		if( pName ){` |
|       - |  1843 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|       - |  1844 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|       4 |  1845 | `			if( pName->sData.nByte == sizeof("object")-1` |
|       4 |  1846 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|       3 |  1847 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|       4 |  1848 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|       3 |  1849 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|       3 |  1850 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|       2 |  1851 | `			}else{` |
|     ! 0 |  1852 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|     ! 0 |  1853 | `					"Unknown named parameter $%z",&pName->sData);` |
|       1 |  1854 | `			}` |
|      38 |  1855 | `		}else if( nArg == 0 ){` |
|      22 |  1856 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|      25 |  1857 | `		}else if( nArg == 1 ){` |
|      15 |  1858 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|       8 |  1859 | `		}else{` |
|     ! 0 |  1860 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|       - |  1861 | `				"clone() expects at most 2 arguments");` |
|       - |  1862 | `		}` |
|      40 |  1863 | `		nArg++;` |
|      40 |  1864 | `		pIn = pNext;` |
|      40 |  1865 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|      17 |  1866 | `			pIn++; /* step over the argument separator */` |
|       8 |  1867 | `		}` |
|       2 |  1868 | `	}` |
|      24 |  1869 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|     ! 0 |  1870 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  1871 | `			"clone() expects at least 1 argument, 0 given");` |
|       - |  1872 | `	}` |
|       - |  1873 | `	/* Object argument -> clone (+ __clone()). */` |
|      24 |  1874 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|      24 |  1875 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1876 | `		return SXERR_ABORT;` |
|       - |  1877 | `	}` |
|      24 |  1878 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|       - |  1879 | `	/* Property updates (evaluated after __clone runs). */` |
|      24 |  1880 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|      17 |  1881 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|      17 |  1882 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1883 | `			return SXERR_ABORT;` |
|       - |  1884 | `		}` |
|      17 |  1885 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|       8 |  1886 | `	}` |
|      24 |  1887 | `	return SXRET_OK;` |
|      13 |  1888 | `}` |
|       - |  1889 | `/*` |
|       - |  1890 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1891 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1892 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1893 | ` */` |
|    1382 |  1894 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1895 | `{` |
|       - |  1896 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    1387 |  1897 | `	pGen->pIn++;` |
|    1387 |  1898 | `	pGen->pEnd--;` |
|     691 |  1899 | `	SXUNUSED(iCompileFlag);` |
|    1387 |  1900 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1901 | `}` |
|       - |  1902 | `/*` |
|       - |  1903 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1904 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1905 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1906 | ` * error message.` |
|       - |  1907 | ` * See the routine responible of compiling the list language construct` |
|       - |  1908 | ` * for more inforation.` |
|       - |  1909 | ` */` |
|     190 |  1910 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  1911 | `{` |
|     195 |  1912 | `	sxi32 rc = SXRET_OK;` |
|     195 |  1913 | `	if( pRoot->pOp ){` |
|       4 |  1914 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       2 |  1915 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1916 | `				/* Unexpected expression */` |
|     ! 0 |  1917 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1918 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1919 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1920 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1921 | `				}` |
|       1 |  1922 | `		}` |
|     193 |  1923 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1924 | `		/* Unexpected expression */` |
|       6 |  1925 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1926 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1927 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1928 | `			rc = SXERR_INVALID;` |
|       2 |  1929 | `		}` |
|       2 |  1930 | `	}` |
|     195 |  1931 | `	return rc;` |
|       5 |  1932 | `}` |
|       - |  1933 | `/*` |
|       - |  1934 | ` * Compile the 'list' language construct.` |
|       - |  1935 | ` *  According to the PHP language reference` |
|       - |  1936 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1937 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1938 | ` *  Description` |
|       - |  1939 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1940 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1941 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1942 | ` *  Parameters` |
|       - |  1943 | ` *   $varname: A variable.` |
|       - |  1944 | ` *  Return Values` |
|       - |  1945 | ` *   The assigned array.` |
|       - |  1946 | ` */` |
|       - |  1947 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1948 | `struct NestedListEntry {` |
|       - |  1949 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1950 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1951 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1952 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1953 | `};` |
|       - |  1954 | `/*` |
|       - |  1955 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|       - |  1956 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|       - |  1957 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|       - |  1958 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|       - |  1959 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|       - |  1960 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|       - |  1961 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|       - |  1962 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|       - |  1963 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|       - |  1964 | ` */` |
|      28 |  1965 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       2 |  1966 | `{` |
|       - |  1967 | `	SyToken *pNext;` |
|       - |  1968 | `	sxi32 rc;` |
|      66 |  1969 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - |  1970 | `		SyToken *pArrow,*pTarget;` |
|       - |  1971 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      38 |  1972 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      38 |  1973 | `		pTarget = &pArrow[1];` |
|      38 |  1974 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - |  1975 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - |  1976 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 |  1977 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1978 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1979 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1980 | `		}` |
|       - |  1981 | `		/* DUP the source array (it is on the stack top) */` |
|      38 |  1982 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1983 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      38 |  1984 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      38 |  1985 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1986 | `			return SXERR_ABORT;` |
|       - |  1987 | `		}` |
|       - |  1988 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - |  1989 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|       - |  1990 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|       - |  1991 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|       - |  1992 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|       - |  1993 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|      38 |  1994 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|      38 |  1995 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      34 |  1996 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      18 |  1997 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - |  1998 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - |  1999 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - |  2000 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 |  2001 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 |  2002 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 |  2003 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 |  2004 | `			pGen->pIn = pTarget;` |
|       5 |  2005 | `			pGen->pEnd = pNext;` |
|       5 |  2006 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 |  2007 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 |  2008 | `			pGen->pIn = pSavedIn;` |
|       5 |  2009 | `			pGen->pEnd = pSavedEnd;` |
|       5 |  2010 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2011 | `				return SXERR_ABORT;` |
|       - |  2012 | `			}` |
|       5 |  2013 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 |  2014 | `		}else{` |
|       - |  2015 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|       - |  2016 | `			 * is already on the stack as the value; compiling the target appends` |
|       - |  2017 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|       - |  2018 | `			 * assignment does. */` |
|       - |  2019 | `			VmInstr *pInstr;` |
|      34 |  2020 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|      34 |  2021 | `			sxi32 iP1 = 0, iP2 = 0;` |
|      34 |  2022 | `			void *p3 = 0;` |
|      34 |  2023 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|       - |  2024 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      34 |  2025 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  2026 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  2027 | `			}` |
|      34 |  2028 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      34 |  2029 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       3 |  2030 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|      33 |  2031 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 |  2032 | `					iVmOp = PH7_OP_STORE_IDX;` |
|       3 |  2033 | `					iP1 = pInstr->iP1;` |
|       3 |  2034 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 |  2035 | `				}else{` |
|      30 |  2036 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|      30 |  2037 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  2038 | `				}` |
|      16 |  2039 | `			}` |
|      34 |  2040 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - |  2041 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|       - |  2042 | `			 * source array is back on top for the next entry. */` |
|      34 |  2043 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       - |  2044 | `		}` |
|      38 |  2045 | `		pGen->pIn = &pNext[1];` |
|       2 |  2046 | `	}` |
|      30 |  2047 | `	return SXRET_OK;` |
|      16 |  2048 | `}` |
|       - |  2049 | `/*` |
|       - |  2050 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  2051 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  2052 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  2053 | ` */` |
|     116 |  2054 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       5 |  2055 | `{` |
|       - |  2056 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  2057 | `	SyToken *pNext;` |
|       - |  2058 | `	SyToken *pClassifyIn;` |
|     121 |  2059 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - |  2060 | `	sxi32 nExpr;` |
|       - |  2061 | `	sxi32 rc;` |
|       - |  2062 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - |  2063 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - |  2064 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - |  2065 | `	 * list. */` |
|     121 |  2066 | `	pClassifyIn = pGen->pIn;` |
|     341 |  2067 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     225 |  2068 | `		if( pGen->pIn >= pNext ){` |
|      13 |  2069 | `			nEmpty++;` |
|     219 |  2070 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      38 |  2071 | `			nKeyed++;` |
|      20 |  2072 | `		}else{` |
|     177 |  2073 | `			nPositional++;` |
|       - |  2074 | `		}` |
|     225 |  2075 | `		pGen->pIn = &pNext[1];` |
|       5 |  2076 | `	}` |
|     121 |  2077 | `	pGen->pIn = pClassifyIn;` |
|     121 |  2078 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 |  2079 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  2080 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  2081 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  2082 | `	}` |
|     121 |  2083 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 |  2084 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  2085 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 |  2086 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  2087 | `	}` |
|     121 |  2088 | `	if( nKeyed > 0 ){` |
|      30 |  2089 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - |  2090 | `	}` |
|      93 |  2091 | `	nExpr = 0;` |
|      93 |  2092 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     277 |  2093 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     189 |  2094 | `		if( pGen->pIn < pNext ){` |
|       - |  2095 | `			/* Check for nested list() */` |
|     177 |  2096 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  2097 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  2098 | `				/* Record this nested list for post-processing */` |
|       3 |  2099 | `				SyToken *pListEnd = 0;` |
|       3 |  2100 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  2101 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  2102 | `				}` |
|       3 |  2103 | `				if( pListEnd ){` |
|       - |  2104 | `					struct NestedListEntry sEntry;` |
|       3 |  2105 | `					sEntry.nIndex = nExpr;` |
|       3 |  2106 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  2107 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  2108 | `					sEntry.isShort = 0;` |
|       3 |  2109 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  2110 | `				}` |
|       - |  2111 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  2112 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     176 |  2113 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  2114 | `				/* Nested short destructuring [...] */` |
|      13 |  2115 | `				SyToken *pBracketEnd = 0;` |
|      13 |  2116 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  2117 | `				if( pBracketEnd ){` |
|       - |  2118 | `					struct NestedListEntry sEntry;` |
|      13 |  2119 | `					sEntry.nIndex = nExpr;` |
|      13 |  2120 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  2121 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  2122 | `					sEntry.isShort = 1;` |
|      13 |  2123 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  2124 | `				}` |
|       - |  2125 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  2126 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  2127 | `			}else{` |
|       - |  2128 | `				/* Compile the expression holding the variable */` |
|     163 |  2129 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     163 |  2130 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  2131 | `					SySetRelease(&sNested);` |
|     ! 0 |  2132 | `					return SXRET_OK;` |
|       - |  2133 | `				}` |
|       - |  2134 | `			}` |
|      91 |  2135 | `		}else{` |
|       - |  2136 | `			/* Empty entry,load NULL */` |
|      13 |  2137 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  2138 | `		}` |
|     189 |  2139 | `		nExpr++;` |
|       - |  2140 | `		/* Advance the stream cursor */` |
|     189 |  2141 | `		pGen->pIn = &pNext[1];` |
|       5 |  2142 | `	}` |
|       - |  2143 | `	/* Emit the LOAD_LIST instruction */` |
|      93 |  2144 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  2145 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  2146 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  2147 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  2148 | `	 */` |
|      93 |  2149 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  2150 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  2151 | `		sxu32 i;` |
|      27 |  2152 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  2153 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  2154 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  2155 | `			ph7_value *pIdx;` |
|       - |  2156 | `			sxu32 nConstIdx;` |
|       - |  2157 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  2158 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  2159 | `			/* Push the integer index for this nested entry */` |
|      15 |  2160 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  2161 | `			if( pIdx == 0 ){` |
|     ! 0 |  2162 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2163 | `				SySetRelease(&sNested);` |
|     ! 0 |  2164 | `				return SXERR_ABORT;` |
|       - |  2165 | `			}` |
|      15 |  2166 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  2167 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  2168 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  2169 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  2170 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  2171 | `			 */` |
|      15 |  2172 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  2173 | `			/* Recursively compile the inner list */` |
|      15 |  2174 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  2175 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  2176 | `			if( apNested[i].isShort ){` |
|      13 |  2177 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  2178 | `			}else{` |
|       3 |  2179 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  2180 | `			}` |
|      15 |  2181 | `			pGen->pIn = pSavedIn;` |
|      15 |  2182 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  2183 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2184 | `				SySetRelease(&sNested);` |
|     ! 0 |  2185 | `				return SXERR_ABORT;` |
|       - |  2186 | `			}` |
|       - |  2187 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  2188 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  2189 | `		}` |
|       6 |  2190 | `	}` |
|      93 |  2191 | `	SySetRelease(&sNested);` |
|       - |  2192 | `	/* Node successfully compiled */` |
|      93 |  2193 | `	return SXRET_OK;` |
|      63 |  2194 | `}` |
|      38 |  2195 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2196 | `{` |
|       - |  2197 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      43 |  2198 | `	pGen->pIn += 2;` |
|      43 |  2199 | `	pGen->pEnd--;` |
|      19 |  2200 | `	SXUNUSED(iCompileFlag);` |
|      43 |  2201 | `	return GenStateCompileListBody(pGen);` |
|       5 |  2202 | `}` |
|      78 |  2203 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2204 | `{` |
|       - |  2205 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      82 |  2206 | `	pGen->pIn++;` |
|      82 |  2207 | `	pGen->pEnd--;` |
|      39 |  2208 | `	SXUNUSED(iCompileFlag);` |
|      82 |  2209 | `	return GenStateCompileListBody(pGen);` |
|       4 |  2210 | `}` |
|       - |  2211 | `/* Forward declarations */` |
|       - |  2212 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  2213 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  2214 | `static int GenStateIsReadonly(SyToken *pTok);` |
|       - |  2215 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|       - |  2216 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|       - |  2217 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  2218 | `/*` |
|       - |  2219 | ` * Compile an annoynmous function or a closure.` |
|       - |  2220 | ` * According to the PHP language reference` |
|       - |  2221 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  2222 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  2223 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  2224 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  2225 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  2226 | ` *  Example Anonymous function variable assignment example` |
|       - |  2227 | ` * <?php` |
|       - |  2228 | ` * $greet = function($name)` |
|       - |  2229 | ` * {` |
|       - |  2230 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  2231 | ` * };` |
|       - |  2232 | ` * $greet('World');` |
|       - |  2233 | ` * $greet('PHP');` |
|       - |  2234 | ` * ?>` |
|       - |  2235 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  2236 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  2237 | ` */` |
|     324 |  2238 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2239 | `{` |
|       - |  2240 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  2241 | `	char zName[512];         /* Unique lambda name */` |
|       - |  2242 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  2243 | `							  * one thread is allowed to compile the script.` |
|       - |  2244 | `						      */` |
|       - |  2245 | `	SyString sName;` |
|       - |  2246 | `	sxu32 nLen;` |
|       - |  2247 | `	sxi32 rc;` |
|     162 |  2248 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2249 |  |
|     329 |  2250 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     329 |  2251 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  2252 | `		pGen->pIn++;` |
|     ! 0 |  2253 | `	}` |
|       - |  2254 | `	/* Generate a unique name */` |
|     329 |  2255 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  2256 | `	/* Make sure the generated name is unique */` |
|     329 |  2257 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  2258 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  2259 | `	}` |
|     329 |  2260 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  2261 | `	/* Compile the lambda body */` |
|     329 |  2262 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     329 |  2263 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2264 | `		return SXERR_ABORT;` |
|       - |  2265 | `	}` |
|       - |  2266 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  2267 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  2268 | `	 * the handler wraps either in a Closure instance. */` |
|     329 |  2269 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  2270 | `	/* Node successfully compiled */` |
|     329 |  2271 | `	return SXRET_OK;` |
|     167 |  2272 | `}` |
|       - |  2273 | `/*` |
|       - |  2274 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  2275 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  2276 | ` * enclosing arrow level, or has already been captured.` |
|       - |  2277 | ` */` |
|     186 |  2278 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  2279 | `	ph7_gen_state *pGen,` |
|       - |  2280 | `	ph7_vm_func *pFunc,` |
|       - |  2281 | `	const char *zName,` |
|       - |  2282 | `	sxu32 nByte,` |
|       - |  2283 | `	SyString *aShadow,` |
|       - |  2284 | `	sxu32 nShadow)` |
|       3 |  2285 | `{` |
|       - |  2286 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2287 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2288 | `	sxu32 n, nEnv;` |
|       - |  2289 | `	char *zDup;` |
|     189 |  2290 | `	if( nByte == 0 ){` |
|     ! 0 |  2291 | `		return SXRET_OK;` |
|       - |  2292 | `	}` |
|     186 |  2293 | `	if( nByte == sizeof("this")-1` |
|     102 |  2294 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2295 | `		return SXRET_OK;` |
|       - |  2296 | `	}` |
|     235 |  2297 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     174 |  2298 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     168 |  2299 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     129 |  2300 | `			return SXRET_OK;` |
|       - |  2301 | `		}` |
|      26 |  2302 | `	}` |
|      59 |  2303 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      59 |  2304 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      87 |  2305 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  2306 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  2307 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  2308 | `			return SXRET_OK;` |
|       - |  2309 | `		}` |
|      15 |  2310 | `	}` |
|      59 |  2311 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      59 |  2312 | `	if( zDup == 0 ){` |
|     ! 0 |  2313 | `		return SXERR_ABORT;` |
|       - |  2314 | `	}` |
|      59 |  2315 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      59 |  2316 | `	sEnv.iFlags = 0;` |
|      59 |  2317 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      59 |  2318 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      59 |  2319 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      59 |  2320 | `	return SXRET_OK;` |
|      96 |  2321 | `}` |
|       - |  2322 | `/*` |
|       - |  2323 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2324 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2325 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2326 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2327 | ` */` |
|      46 |  2328 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2329 | `	ph7_gen_state *pGen,` |
|       - |  2330 | `	ph7_vm_func *pFunc,` |
|       - |  2331 | `	const char *zIn,` |
|       - |  2332 | `	const char *zEnd,` |
|       - |  2333 | `	SyString *aShadow,` |
|       - |  2334 | `	sxu32 nShadow)` |
|       2 |  2335 | `{` |
|       - |  2336 | `	sxi32 rc;` |
|     342 |  2337 | `	while( zIn < zEnd ){` |
|     296 |  2338 | `		if( zIn[0] == '\\' ){` |
|       5 |  2339 | `			zIn++;` |
|       5 |  2340 | `			if( zIn < zEnd ){` |
|       5 |  2341 | `				zIn++;` |
|       2 |  2342 | `			}` |
|       5 |  2343 | `			continue;` |
|       - |  2344 | `		}` |
|     290 |  2345 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      22 |  2346 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      20 |  2347 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2348 | `			const char *zName;` |
|      22 |  2349 | `			zIn++; /* skip '$' */` |
|      22 |  2350 | `			zName = zIn;` |
|      74 |  2351 | `			while( zIn < zEnd ){` |
|      70 |  2352 | `				unsigned char c = (unsigned char)zIn[0];` |
|      70 |  2353 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2354 | `					zIn++;` |
|     ! 0 |  2355 | `					while( zIn < zEnd` |
|     ! 0 |  2356 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2357 | `						zIn++;` |
|     ! 0 |  2358 | `					}` |
|     ! 0 |  2359 | `					continue;` |
|       - |  2360 | `				}` |
|      70 |  2361 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      18 |  2362 | `					break;` |
|       - |  2363 | `				}` |
|      54 |  2364 | `				zIn++;` |
|       2 |  2365 | `			}` |
|      22 |  2366 | `			if( zIn > zName ){` |
|      32 |  2367 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      20 |  2368 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      22 |  2369 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2370 | `					return SXERR_ABORT;` |
|       - |  2371 | `				}` |
|      10 |  2372 | `			}` |
|      22 |  2373 | `			continue;` |
|       - |  2374 | `		}` |
|     272 |  2375 | `		zIn++;` |
|       2 |  2376 | `	}` |
|      48 |  2377 | `	return SXRET_OK;` |
|      25 |  2378 | `}` |
|       - |  2379 | `/*` |
|       - |  2380 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  2381 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  2382 | ` *   - plain $<id> pairs` |
|       - |  2383 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  2384 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  2385 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  2386 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  2387 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  2388 | ` *     are never mistakenly captured.` |
|       - |  2389 | ` */` |
|     250 |  2390 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2391 | `	ph7_gen_state *pGen,` |
|       - |  2392 | `	ph7_vm_func *pFunc,` |
|       - |  2393 | `	SyToken *pStart,` |
|       - |  2394 | `	SyToken *pEnd,` |
|       - |  2395 | `	SyString *aShadow,` |
|       - |  2396 | `	sxu32 nShadow)` |
|       4 |  2397 | `{` |
|     254 |  2398 | `	SyToken *pScan = pStart;` |
|       - |  2399 | `	sxi32 rc;` |
|    1274 |  2400 | `	while( pScan < pEnd ){` |
|    1024 |  2401 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      71 |  2402 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|      23 |  2403 | `				pScan->sData.zString,` |
|      46 |  2404 | `				pScan->sData.zString + pScan->sData.nByte,` |
|      23 |  2405 | `				aShadow,nShadow);` |
|      48 |  2406 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2407 | `				return SXERR_ABORT;` |
|       - |  2408 | `			}` |
|      48 |  2409 | `			pScan++;` |
|      48 |  2410 | `			continue;` |
|       - |  2411 | `		}` |
|     978 |  2412 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      24 |  2413 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      24 |  2414 | `			SyToken *pFnKw = pScan;` |
|      22 |  2415 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2416 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       2 |  2417 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2418 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2419 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2420 | `			}` |
|      24 |  2421 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2422 | `				SyToken *pInnerSigStart;` |
|       - |  2423 | `				SyToken *pInnerSigEnd;` |
|       - |  2424 | `				SyToken *pInnerBodyEnd;` |
|       - |  2425 | `				SyString *aInnerShadow;` |
|       - |  2426 | `				sxu32 nInnerShadow;` |
|       - |  2427 | `				sxu32 nInnerParamMax;` |
|       - |  2428 | `				SyToken *p;` |
|       - |  2429 | `				int iNestInner;` |
|      19 |  2430 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2431 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2432 | `					pScan++;` |
|     ! 0 |  2433 | `				}` |
|      19 |  2434 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2435 | `					pScan++;` |
|     ! 0 |  2436 | `					continue;` |
|       - |  2437 | `				}` |
|      19 |  2438 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2439 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2440 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2441 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2442 | `					pScan = pEnd;` |
|     ! 0 |  2443 | `					continue;` |
|       - |  2444 | `				}` |
|       - |  2445 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2446 | `				nInnerParamMax = 0;` |
|      57 |  2447 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2448 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2449 | `						nInnerParamMax++;` |
|       6 |  2450 | `					}` |
|      20 |  2451 | `				}` |
|      19 |  2452 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2453 | `					&pGen->pVm->sAllocator,` |
|      18 |  2454 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2455 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2456 | `					return SXERR_ABORT;` |
|       - |  2457 | `				}` |
|      19 |  2458 | `				nInnerShadow = 0;` |
|      25 |  2459 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2460 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2461 | `				}` |
|      57 |  2462 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2463 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2464 | `						continue;` |
|       - |  2465 | `					}` |
|      13 |  2466 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2467 | `						break;` |
|       - |  2468 | `					}` |
|      13 |  2469 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2470 | `						continue;` |
|       - |  2471 | `					}` |
|      13 |  2472 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2473 | `				}` |
|      19 |  2474 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2475 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2476 | `					pScan++;` |
|     ! 0 |  2477 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2478 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2479 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2480 | `						pScan++;` |
|     ! 0 |  2481 | `					}` |
|     ! 0 |  2482 | `					if( pScan < pEnd` |
|     ! 0 |  2483 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2484 | `						pScan++;` |
|     ! 0 |  2485 | `					}` |
|     ! 0 |  2486 | `				}` |
|      19 |  2487 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2488 | `					pScan++; /* past '=>' */` |
|       9 |  2489 | `				}` |
|      19 |  2490 | `				pInnerBodyEnd = pScan;` |
|      19 |  2491 | `				iNestInner = 0;` |
|     131 |  2492 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2493 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2494 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2495 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2496 | `						break;` |
|       - |  2497 | `					}` |
|     113 |  2498 | `					if( pInnerBodyEnd->nType &` |
|       - |  2499 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2500 | `						iNestInner++;` |
|     112 |  2501 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2502 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2503 | `						iNestInner--;` |
|       1 |  2504 | `					}` |
|     113 |  2505 | `					pInnerBodyEnd++;` |
|       1 |  2506 | `				}` |
|       - |  2507 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2508 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2509 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2510 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2511 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2512 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2513 | `				 *` |
|       - |  2514 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2515 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2516 | `				 * range after the '=' sign. */` |
|       - |  2517 | `				{` |
|      19 |  2518 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2519 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2520 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2521 | `						SyToken *pEq = 0;` |
|      13 |  2522 | `						int iNestArg = 0;` |
|      49 |  2523 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2524 | `							if( iNestArg == 0` |
|      39 |  2525 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2526 | `								break;` |
|       - |  2527 | `							}` |
|      37 |  2528 | `							if( pArgEnd->nType &` |
|       - |  2529 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2530 | `								iNestArg++;` |
|      37 |  2531 | `							}else if( pArgEnd->nType &` |
|       - |  2532 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2533 | `								iNestArg--;` |
|     ! 0 |  2534 | `							}` |
|      36 |  2535 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2536 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2537 | `								pEq = pArgEnd;` |
|       3 |  2538 | `							}` |
|      37 |  2539 | `							pArgEnd++;` |
|       1 |  2540 | `						}` |
|      13 |  2541 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2542 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2543 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2544 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2545 | `								return SXERR_ABORT;` |
|       - |  2546 | `							}` |
|       3 |  2547 | `						}` |
|      13 |  2548 | `						pArgStart = pArgEnd;` |
|      12 |  2549 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2550 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2551 | `							pArgStart++;` |
|       1 |  2552 | `						}` |
|       1 |  2553 | `					}` |
|       - |  2554 | `				}` |
|      28 |  2555 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2556 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2557 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2558 | `					return SXERR_ABORT;` |
|       - |  2559 | `				}` |
|      19 |  2560 | `				pScan = pInnerBodyEnd;` |
|      19 |  2561 | `				continue;` |
|       - |  2562 | `			}` |
|       2 |  2563 | `		}` |
|     960 |  2564 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     794 |  2565 | `			pScan++;` |
|     794 |  2566 | `			continue;` |
|       - |  2567 | `		}` |
|       - |  2568 | `		{` |
|       - |  2569 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     169 |  2570 | `			SyToken *pDollar = pScan;` |
|     249 |  2571 | `			while( &pDollar[1] < pEnd` |
|     169 |  2572 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2573 | `				pDollar++;` |
|     ! 0 |  2574 | `			}` |
|     169 |  2575 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2576 | `				break;` |
|       - |  2577 | `			}` |
|     169 |  2578 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2579 | `				pScan = pDollar + 1;` |
|     ! 0 |  2580 | `				continue;` |
|       - |  2581 | `			}` |
|     252 |  2582 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     166 |  2583 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      83 |  2584 | `				aShadow,nShadow);` |
|     169 |  2585 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2586 | `				return SXERR_ABORT;` |
|       - |  2587 | `			}` |
|     169 |  2588 | `			pScan = pDollar + 2;` |
|       - |  2589 | `		}` |
|       3 |  2590 | `	}` |
|     254 |  2591 | `	return SXRET_OK;` |
|     129 |  2592 | `}` |
|       - |  2593 | `/*` |
|       - |  2594 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2595 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2596 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2597 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2598 | ` * $this is also made available.` |
|       - |  2599 | ` */` |
|     232 |  2600 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2601 | `{` |
|       - |  2602 | `	ph7_vm_func *pFunc;` |
|       - |  2603 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2604 | `	GenBlock *pBlock;` |
|       - |  2605 | `	SySet *pInstrContainer;` |
|       - |  2606 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2607 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2608 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2609 | `	SyToken *pSavedEnd;` |
|       - |  2610 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2611 | `	char zName[512];` |
|       - |  2612 | `	static int iCnt = 1;` |
|       - |  2613 | `	char *zDup;` |
|       - |  2614 | `	sxu32 nLen;` |
|       - |  2615 | `	sxu32 nLine;` |
|     237 |  2616 | `	sxi32 iFlags = 0;` |
|     237 |  2617 | `	int bStatic = 0;` |
|       - |  2618 | `	sxi32 rc;` |
|       - |  2619 | `	sxu32 n;` |
|     116 |  2620 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2621 |  |
|     237 |  2622 | `	nLine = pGen->pIn->nLine;` |
|       - |  2623 | `	/* Optional 'static' prefix */` |
|     232 |  2624 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     237 |  2625 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2626 | `		bStatic = 1;` |
|       3 |  2627 | `		pGen->pIn++;` |
|       1 |  2628 | `	}` |
|       - |  2629 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     232 |  2630 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     237 |  2631 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2632 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2633 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2634 | `		return SXERR_SYNTAX;` |
|       - |  2635 | `	}` |
|     237 |  2636 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2637 | `	/* Optional '&' — return by reference */` |
|     237 |  2638 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2639 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2640 | `		pGen->pIn++;` |
|     ! 0 |  2641 | `	}` |
|       - |  2642 | `	/* Expect '(' */` |
|     237 |  2643 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2644 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2645 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2646 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2647 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2648 | `		}else{` |
|     ! 0 |  2649 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2650 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2651 | `		}` |
|       3 |  2652 | `		return SXERR_SYNTAX;` |
|       - |  2653 | `	}` |
|     235 |  2654 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2655 | `	/* Delimit the parameter list */` |
|     235 |  2656 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     235 |  2657 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2658 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2659 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2660 | `		return SXERR_SYNTAX;` |
|       - |  2661 | `	}` |
|       - |  2662 | `	/* Allocate the function state */` |
|     233 |  2663 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     233 |  2664 | `	if( pFunc == 0 ){` |
|     ! 0 |  2665 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2666 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2667 | `		return SXERR_ABORT;` |
|       - |  2668 | `	}` |
|       - |  2669 | `	/* Generate a unique lambda name */` |
|     233 |  2670 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     289 |  2671 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      58 |  2672 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2673 | `	}` |
|     233 |  2674 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     233 |  2675 | `	if( zDup == 0 ){` |
|     ! 0 |  2676 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2677 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2678 | `		return SXERR_ABORT;` |
|       - |  2679 | `	}` |
|     233 |  2680 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2681 | `	/* Collect function arguments */` |
|     233 |  2682 | `	if( pGen->pIn < pSigEnd ){` |
|     106 |  2683 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     106 |  2684 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2685 | `			return SXERR_ABORT;` |
|       - |  2686 | `		}` |
|      51 |  2687 | `	}` |
|       - |  2688 | `	/* Point past ')' and parse optional return type */` |
|     233 |  2689 | `	pGen->pIn = &pSigEnd[1];` |
|     233 |  2690 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     233 |  2691 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2692 | `		return SXERR_ABORT;` |
|     233 |  2693 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2694 | `		return SXERR_SYNTAX;` |
|       - |  2695 | `	}` |
|       - |  2696 | `	/* Expect '=>' */` |
|     233 |  2697 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2698 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2699 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2700 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2701 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2702 | `		}else{` |
|     ! 0 |  2703 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2704 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2705 | `		}` |
|       3 |  2706 | `		return SXERR_SYNTAX;` |
|       - |  2707 | `	}` |
|     230 |  2708 | `	pGen->pIn++; /* Jump '=>' */` |
|     230 |  2709 | `	pBodyStart = pGen->pIn;` |
|     230 |  2710 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2711 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2712 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2713 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2714 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     230 |  2715 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2716 | `	{` |
|     230 |  2717 | `		SyString *aShadow = 0;` |
|     230 |  2718 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     230 |  2719 | `		if( nShadow > 0 ){` |
|     103 |  2720 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|     100 |  2721 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|     103 |  2722 | `			if( aShadow == 0 ){` |
|     ! 0 |  2723 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2724 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2725 | `				return SXERR_ABORT;` |
|       - |  2726 | `			}` |
|     229 |  2727 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     129 |  2728 | `				aShadow[n] = aArgs[n].sName;` |
|      66 |  2729 | `			}` |
|      50 |  2730 | `		}` |
|     343 |  2731 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|     113 |  2732 | `			aShadow,nShadow);` |
|     230 |  2733 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2734 | `			return SXERR_ABORT;` |
|       - |  2735 | `		}` |
|       - |  2736 | `	}` |
|       - |  2737 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2738 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2739 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2740 | `	 * $this. */` |
|     230 |  2741 | `	if( !bStatic ){` |
|       - |  2742 | `		char *zThisDup;` |
|     228 |  2743 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     228 |  2744 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2745 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2746 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2747 | `			return SXERR_ABORT;` |
|       - |  2748 | `		}` |
|     228 |  2749 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     228 |  2750 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     228 |  2751 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     228 |  2752 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     228 |  2753 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|     112 |  2754 | `	}` |
|       - |  2755 | `	/* Arrow functions are always closures */` |
|     230 |  2756 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2757 | `	/* Compile the body expression as an implicit return */` |
|     343 |  2758 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     113 |  2759 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     230 |  2760 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2761 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2762 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2763 | `		return SXERR_ABORT;` |
|       - |  2764 | `	}` |
|     230 |  2765 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     230 |  2766 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     230 |  2767 | `	pSavedEnd = pGen->pEnd;` |
|     230 |  2768 | `	pGen->pIn = pBodyStart;` |
|     230 |  2769 | `	pGen->pEnd = pBodyEnd;` |
|     230 |  2770 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     230 |  2771 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2772 | `		return SXERR_ABORT;` |
|       - |  2773 | `	}` |
|       - |  2774 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2775 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2776 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2777 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     230 |  2778 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     230 |  2779 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     230 |  2780 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     230 |  2781 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     230 |  2782 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2783 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     230 |  2784 | `	pGen->pIn = pBodyEnd;` |
|     230 |  2785 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2786 | `	/* Emit the load-closure instruction */` |
|     230 |  2787 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     230 |  2788 | `	return SXRET_OK;` |
|     121 |  2789 | `}` |
|       - |  2790 | `/*` |
|       - |  2791 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2792 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2793 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2794 | ` * expression's value.` |
|       - |  2795 | ` */` |
|     346 |  2796 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2797 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2798 | `{` |
|       - |  2799 | `	SySet *pInstrContainer;` |
|       - |  2800 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2801 | `	GenBlock *pArmBlock;` |
|       - |  2802 | `	sxi32 rc;` |
|     349 |  2803 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2804 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2805 | `	pGen->pIn  = pStart;` |
|     349 |  2806 | `	pGen->pEnd = pStop;` |
|     349 |  2807 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2808 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2809 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2810 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2811 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2812 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2813 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2814 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2815 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2816 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2817 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2818 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2819 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2820 | `		return SXERR_ABORT;` |
|       - |  2821 | `	}` |
|     349 |  2822 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2823 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2824 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2825 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2826 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2827 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2828 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2829 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2830 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2831 | `		return SXERR_ABORT;` |
|       - |  2832 | `	}` |
|     349 |  2833 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2834 | `		return SXERR_EMPTY;` |
|       - |  2835 | `	}` |
|     349 |  2836 | `	return SXRET_OK;` |
|     176 |  2837 | `}` |
|       - |  2838 | `/*` |
|       - |  2839 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2840 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2841 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2842 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2843 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2844 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2845 | ` */` |
|       - |  2846 | `/*` |
|       - |  2847 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2848 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2849 | ` * caller can bail out of the current expression.` |
|       - |  2850 | ` */` |
|       2 |  2851 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2852 | `{` |
|       - |  2853 | `	va_list ap;` |
|       - |  2854 | `	sxi32 rc;` |
|       - |  2855 | `	SyBlob sMsg;` |
|       3 |  2856 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2857 | `	va_start(ap,zFmt);` |
|       3 |  2858 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2859 | `	va_end(ap);` |
|       3 |  2860 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2861 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2862 | `	SyBlobRelease(&sMsg);` |
|       3 |  2863 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2864 | `		return SXERR_ABORT;` |
|       - |  2865 | `	}` |
|       3 |  2866 | `	return SXERR_SYNTAX;` |
|       2 |  2867 | `}` |
|       - |  2868 | `/*` |
|       - |  2869 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2870 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2871 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2872 | ` */` |
|     348 |  2873 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2874 | `{` |
|     352 |  2875 | `	SyToken *pCur = pStart;` |
|     352 |  2876 | `	int iNest = 0;` |
|     814 |  2877 | `	while( pCur < pEnd ){` |
|     780 |  2878 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2879 | `			iNest++;` |
|     774 |  2880 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2881 | `			iNest--;` |
|     762 |  2882 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2883 | `			return pCur;` |
|       - |  2884 | `		}` |
|     466 |  2885 | `		pCur++;` |
|       4 |  2886 | `	}` |
|      37 |  2887 | `	return pEnd;` |
|     178 |  2888 | `}` |
|      70 |  2889 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2890 | `{` |
|       - |  2891 | `	ph7_match *pMatch;` |
|       - |  2892 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2893 | `	int bHasDefault = 0;` |
|       - |  2894 | `	sxu32 nLine;` |
|       - |  2895 | `	sxi32 rc;` |
|      35 |  2896 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2897 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2898 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2899 | `	/* Expect '(' */` |
|      75 |  2900 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2901 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2902 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2903 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2904 | `	}` |
|      75 |  2905 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2906 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2907 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2908 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2909 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2910 | `	}` |
|      75 |  2911 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2912 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2913 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2914 | `	}` |
|       - |  2915 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2916 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2917 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2918 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2919 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2920 | `		return SXERR_ABORT;` |
|       - |  2921 | `	}` |
|      75 |  2922 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2923 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2924 | `	/* Expect '{' */` |
|      75 |  2925 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2926 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2927 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2928 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2929 | `	}` |
|      75 |  2930 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2931 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2932 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2933 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2934 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2935 | `	}` |
|       - |  2936 | `	/* Allocate ph7_match container */` |
|      75 |  2937 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2938 | `	if( pMatch == 0 ){` |
|     ! 0 |  2939 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2940 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2941 | `		return SXERR_ABORT;` |
|       - |  2942 | `	}` |
|      75 |  2943 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2944 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2945 | `	/* Iterate arms */` |
|     253 |  2946 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2947 | `		ph7_match_arm sArm;` |
|       - |  2948 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2949 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2950 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2951 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2952 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2953 | `		/* 'default' arm? */` |
|     182 |  2954 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2955 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2956 | `			if( bHasDefault ){` |
|       3 |  2957 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2958 | `					"Match expressions may only contain one default arm");` |
|       4 |  2959 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2960 | `			}` |
|      20 |  2961 | `			sArm.bDefault = 1;` |
|      20 |  2962 | `			bHasDefault = 1;` |
|      20 |  2963 | `			pGen->pIn++;` |
|      20 |  2964 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2965 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2966 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2967 | `			}` |
|      20 |  2968 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2969 | `		}else{` |
|       - |  2970 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2971 | `			pCondStart = pGen->pIn;` |
|     166 |  2972 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2973 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2974 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2975 | `				SySet sCondBc;` |
|       9 |  2976 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2977 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2978 | `						"syntax error, empty match condition expression");` |
|       - |  2979 | `				}` |
|       9 |  2980 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2981 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2982 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2983 | `					return SXERR_ABORT;` |
|       - |  2984 | `				}` |
|       9 |  2985 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2986 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2987 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2988 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2989 | `			}` |
|     166 |  2990 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2991 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2992 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2993 | `			}` |
|     163 |  2994 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2995 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2996 | `					"syntax error, empty match condition expression");` |
|       - |  2997 | `			}` |
|       - |  2998 | `			{` |
|       - |  2999 | `				SySet sCondBc;` |
|     163 |  3000 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  3001 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  3002 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  3003 | `					return SXERR_ABORT;` |
|       - |  3004 | `				}` |
|     163 |  3005 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  3006 | `			}` |
|     163 |  3007 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  3008 | `		}` |
|       - |  3009 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  3010 | `		pResStart = pGen->pIn;` |
|     181 |  3011 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  3012 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  3013 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  3014 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  3015 | `		}` |
|     181 |  3016 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  3017 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3018 | `			return SXERR_ABORT;` |
|       - |  3019 | `		}` |
|     181 |  3020 | `		pGen->pIn = pResEnd;` |
|     181 |  3021 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  3022 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  3023 | `		}` |
|     181 |  3024 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  3025 | `	}` |
|      69 |  3026 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  3027 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  3028 | `	return SXRET_OK;` |
|      40 |  3029 | `}` |
|       - |  3030 | `/*` |
|       - |  3031 | ` * Compile a backtick quoted string.` |
|       - |  3032 | ` */` |
|       4 |  3033 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  3034 | `{` |
|       - |  3035 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  3036 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  3037 | `	 */` |
|       8 |  3038 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  3039 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  3040 | `		ph7_lib_version()` |
|       - |  3041 | `		);` |
|       - |  3042 | `	/* Load NULL */` |
|       6 |  3043 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3044 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  3045 | `	/* Node successfully compiled */` |
|       6 |  3046 | `	return SXRET_OK;` |
|       2 |  3047 | `}` |
|       - |  3048 | `/*` |
|       - |  3049 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  3050 | ` * construct.` |
|       - |  3051 | ` */` |
|      82 |  3052 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3053 | `{` |
|       - |  3054 | `	SyString *pName;` |
|       - |  3055 | `	sxu32 nKeyID;` |
|       - |  3056 | `	sxi32 rc;` |
|       - |  3057 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      87 |  3058 | `	pName = &pGen->pIn->sData;` |
|      87 |  3059 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      87 |  3060 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      87 |  3061 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  3062 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  3063 | `		/* Compile arguments one after one */` |
|       9 |  3064 | `		pTmp = pGen->pEnd;` |
|       - |  3065 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  3066 | `		 * 'echo' can be used in the context of a function which` |
|       - |  3067 | `		 *  mean that the following expression is valid:` |
|       - |  3068 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  3069 | `		 */` |
|       9 |  3070 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  3071 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  3072 | `			if( pGen->pIn < pNext ){` |
|       9 |  3073 | `				pGen->pEnd = pNext;` |
|       9 |  3074 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  3075 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  3076 | `					return SXERR_ABORT;` |
|       - |  3077 | `				}` |
|       9 |  3078 | `				if( rc != SXERR_EMPTY ){` |
|       - |  3079 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  3080 | `					 * without the overhead of a function call.` |
|       - |  3081 | `					 * This is a very powerful optimization that improve` |
|       - |  3082 | `					 * performance greatly.` |
|       - |  3083 | `					 */` |
|       9 |  3084 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  3085 | `				}` |
|       4 |  3086 | `			}` |
|       - |  3087 | `			/* Jump trailing commas */` |
|       9 |  3088 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  3089 | `				pNext++;` |
|     ! 0 |  3090 | `			}` |
|       9 |  3091 | `			pGen->pIn = pNext;` |
|       1 |  3092 | `		}` |
|       - |  3093 | `		/* Restore token stream */` |
|       9 |  3094 | `		pGen->pEnd = pTmp;` |
|       5 |  3095 | `	}else{` |
|      79 |  3096 | `		sxi32 nArg = 0;` |
|      79 |  3097 | `		sxu32 nIdx = 0;` |
|      79 |  3098 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      79 |  3099 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3100 | `			return SXERR_ABORT;` |
|      79 |  3101 | `		}else if(rc != SXERR_EMPTY ){` |
|      79 |  3102 | `			nArg = 1;` |
|      37 |  3103 | `		}` |
|      79 |  3104 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  3105 | `			ph7_value *pObj;` |
|       - |  3106 | `			/* Emit the call instruction */` |
|      31 |  3107 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      31 |  3108 | `			if( pObj == 0 ){` |
|     ! 0 |  3109 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3110 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3111 | `				return SXERR_ABORT;` |
|       - |  3112 | `			}` |
|      31 |  3113 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  3114 | `			/* Install in the literal table */` |
|      31 |  3115 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      13 |  3116 | `		}` |
|       - |  3117 | `		/* Emit the call instruction */` |
|      79 |  3118 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      79 |  3119 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  3120 | `	}` |
|       - |  3121 | `	/* Node successfully compiled */` |
|      87 |  3122 | `	return SXRET_OK;` |
|      46 |  3123 | `}` |
|       - |  3124 | `/*` |
|       - |  3125 | ` * Compile a node holding a variable declaration.` |
|       - |  3126 | ` * According to the PHP language reference` |
|       - |  3127 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  3128 | ` *  The variable name is case-sensitive.` |
|       - |  3129 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  3130 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3131 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  3132 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  3133 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  3134 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  3135 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  3136 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  3137 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  3138 | ` *  the chapter on Expressions.` |
|       - |  3139 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  3140 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  3141 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  3142 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  3143 | ` *  is being assigned (the source variable).` |
|       - |  3144 | ` */` |
| 1275458 |  3145 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3146 | `{` |
| 1275463 |  3147 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3148 | `	sxi32 iVv;` |
|       - |  3149 | `	sxi32 iP1;` |
|       - |  3150 | `	void *p3;` |
|       - |  3151 | `	sxi32 rc;` |
| 1275463 |  3152 | `	iVv = -1; /* Variable variable counter */` |
| 2550933 |  3153 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1275475 |  3154 | `		pGen->pIn++;` |
| 1275475 |  3155 | `		iVv++;` |
|       5 |  3156 | `	}` |
| 1275463 |  3157 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  3158 | `		/* Invalid variable name */` |
|     ! 0 |  3159 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  3160 | `		if( rc == SXERR_ABORT ){` |
|       - |  3161 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3162 | `			return SXERR_ABORT;` |
|       - |  3163 | `		}` |
|     ! 0 |  3164 | `		return SXRET_OK;` |
|       - |  3165 | `	}` |
| 1275463 |  3166 | `	p3  = 0;` |
| 1275463 |  3167 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  3168 | `		/* Dynamic variable creation */` |
|      21 |  3169 | `		pGen->pIn++;  /* Jump the open curly */` |
|      21 |  3170 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      21 |  3171 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3172 | `			/* Empty expression */` |
|       3 |  3173 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  3174 | `			return SXRET_OK;` |
|       - |  3175 | `		}` |
|       - |  3176 | `		/* Compile the expression holding the variable name */` |
|      18 |  3177 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      18 |  3178 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3179 | `			return SXERR_ABORT;` |
|      18 |  3180 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  3181 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  3182 | `			return SXRET_OK;` |
|       - |  3183 | `		}` |
|       8 |  3184 | `	}else{` |
|       - |  3185 | `		SyHashEntry *pEntry;` |
|       - |  3186 | `		SyString *pName;` |
| 1275445 |  3187 | `		char *zName = 0;` |
|       - |  3188 | `		/* Extract variable name */` |
| 1275445 |  3189 | `		pName = &pGen->pIn->sData;` |
|       - |  3190 | `		/* Advance the stream cursor */` |
| 1275445 |  3191 | `		pGen->pIn++;` |
| 1275445 |  3192 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1275445 |  3193 | `		if( pEntry == 0 ){` |
|       - |  3194 | `			/* Duplicate name */` |
|  184649 |  3195 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  184649 |  3196 | `			if( zName == 0 ){` |
|     ! 0 |  3197 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3198 | `				return SXERR_ABORT;` |
|       - |  3199 | `			}` |
|       - |  3200 | `			/* Install in the hashtable */` |
|  184649 |  3201 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   92327 |  3202 | `		}else{` |
|       - |  3203 | `			/* Name already available */` |
| 1090801 |  3204 | `			zName = (char *)pEntry->pUserData;` |
|       - |  3205 | `		}` |
| 1275445 |  3206 | `		p3 = (void *)zName;` |
|       - |  3207 | `	}` |
| 1275459 |  3208 | `	iP1 = 0;` |
| 1275459 |  3209 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  490623 |  3210 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  3211 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  490605 |  3212 | `			iP1 = 1;` |
|  245300 |  3213 | `		}` |
|  245309 |  3214 | `	}` |
|       - |  3215 | `	/* Emit the load instruction */` |
| 1275459 |  3216 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1275471 |  3217 | `	while( iVv > 0 ){` |
|      13 |  3218 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  3219 | `		iVv--;` |
|       1 |  3220 | `	}` |
|       - |  3221 | `	/* Node successfully compiled */` |
| 1275459 |  3222 | `	return SXRET_OK;` |
|  637734 |  3223 | `}` |
|       - |  3224 | `/*` |
|       - |  3225 | ` * Load a literal.` |
|       - |  3226 | ` */` |
|  883888 |  3227 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  3228 | `{` |
|  883893 |  3229 | `	SyToken *pToken = pGen->pIn;` |
|       - |  3230 | `	ph7_value *pObj;` |
|       - |  3231 | `	SyString *pStr;` |
|       - |  3232 | `	sxu32 nIdx;` |
|       - |  3233 | `	/* Extract token value */` |
|  883893 |  3234 | `	pStr = &pToken->sData;` |
|       - |  3235 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  883893 |  3236 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  172231 |  3237 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  3238 | `			/* NULL constant are always indexed at 0 */` |
|   58407 |  3239 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   58407 |  3240 | `			return SXRET_OK;` |
|  113829 |  3241 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  3242 | `			/* TRUE constant are always indexed at 1 */` |
|    8629 |  3243 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    8629 |  3244 | `			return SXRET_OK;` |
|       5 |  3245 | `		}` |
|  817860 |  3246 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  107186 |  3247 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  3248 | `			/* FALSE constant are always indexed at 2 */` |
|   50573 |  3249 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   50573 |  3250 | `			return SXRET_OK;` |
|  725211 |  3251 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  128224 |  3252 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  3253 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   11543 |  3254 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   11543 |  3255 | `			if( pObj == 0 ){` |
|     ! 0 |  3256 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3257 | `				return SXERR_ABORT;` |
|       - |  3258 | `			}` |
|   11543 |  3259 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  3260 | `			/* Emit the load constant instruction */` |
|   11543 |  3261 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   11543 |  3262 | `			return SXRET_OK;` |
|  669010 |  3263 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   38898 |  3264 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  3265 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       8 |  3266 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       8 |  3267 | `			if( pObj == 0 ){` |
|     ! 0 |  3268 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3269 | `				return SXERR_ABORT;` |
|       - |  3270 | `			}` |
|       8 |  3271 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  3272 | `				SyString sNs;` |
|       8 |  3273 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  3274 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       5 |  3275 | `			}else{` |
|     ! 0 |  3276 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  3277 | `			}` |
|       8 |  3278 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       8 |  3279 | `			return SXRET_OK;` |
|  657690 |  3280 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   27066 |  3281 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  660324 |  3282 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   21562 |  3283 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  3284 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  3285 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  3286 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  3287 | `				/* Point to the upper block */` |
|      11 |  3288 | `				pBlock = pBlock->pParent;` |
|       1 |  3289 | `			}` |
|      11 |  3290 | `			if( pBlock == 0 ){` |
|       - |  3291 | `				/* Called in the global scope,load NULL */` |
|       5 |  3292 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  3293 | `			}else{` |
|       - |  3294 | `				/* Extract the target function/method */` |
|       7 |  3295 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  3296 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  3297 | `					/* Not a class method,Load null */` |
|       3 |  3298 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3299 | `				}else{` |
|       5 |  3300 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  3301 | `					if( pObj == 0 ){` |
|     ! 0 |  3302 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3303 | `						return SXERR_ABORT;` |
|       - |  3304 | `					}` |
|       5 |  3305 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  3306 | `					/* Emit the load constant instruction */` |
|       5 |  3307 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  3308 | `				}` |
|       - |  3309 | `			}` |
|      11 |  3310 | `			return SXRET_OK;` |
|       - |  3311 | `	}` |
|       - |  3312 | `	/* Query literal table */` |
|  754745 |  3313 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3314 | `		ph7_value *pLitObj;` |
|       - |  3315 | `		/* Unknown literal,install it in the literal table */` |
|  321975 |  3316 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  321975 |  3317 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3318 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3319 | `			return SXERR_ABORT;` |
|       - |  3320 | `		}` |
|  321975 |  3321 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  321975 |  3322 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  160985 |  3323 | `	}` |
|       - |  3324 | `	/* Emit the load constant instruction */` |
|  754745 |  3325 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  754745 |  3326 | `	return SXRET_OK;` |
|  441949 |  3327 | `}` |
|       - |  3328 | `/*` |
|       - |  3329 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3330 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3331 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3332 | ` * Otherwise, load the simple literal directly.` |
|       - |  3333 | ` */` |
|  887780 |  3334 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3335 | `{` |
|       - |  3336 | `	sxi32 rc;` |
|  887785 |  3337 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3338 | `		return SXRET_OK;` |
|       - |  3339 | `	}` |
|       - |  3340 | `	/* Check if this is a multi-token namespace path */` |
|  887785 |  3341 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3342 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3897 |  3343 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3897 |  3344 | `		int isAbsolute = 0;` |
|    3897 |  3345 | `		SyBlobReset(pWorker);` |
|       - |  3346 | `		/* Check for leading backslash (absolute path) */` |
|    3897 |  3347 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3895 |  3348 | `			isAbsolute = 1;` |
|    3895 |  3349 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1945 |  3350 | `		}` |
|       - |  3351 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3897 |  3352 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3353 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3354 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3355 | `		}` |
|       - |  3356 | `		/* Collect all path components */` |
|    4005 |  3357 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    4005 |  3358 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      59 |  3359 | `				SyBlobAppend(pWorker,"\\",1);` |
|      32 |  3360 | `			}else{` |
|    3951 |  3361 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3362 | `			}` |
|    4005 |  3363 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3897 |  3364 | `				pGen->pIn++;` |
|    3897 |  3365 | `				break;` |
|       - |  3366 | `			}` |
|     113 |  3367 | `			pGen->pIn++;` |
|       5 |  3368 | `		}` |
|    3897 |  3369 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3370 | `			ph7_value *pObj;` |
|       - |  3371 | `			SyString sPath;` |
|       - |  3372 | `			sxu32 nIdx;` |
|    3897 |  3373 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3374 | `			/* Install in the literal table */` |
|    3897 |  3375 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3869 |  3376 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3869 |  3377 | `				if( pObj == 0 ){` |
|     ! 0 |  3378 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3379 | `					return SXERR_ABORT;` |
|       - |  3380 | `				}` |
|    3869 |  3381 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3869 |  3382 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1932 |  3383 | `			}` |
|       - |  3384 | `			/* Emit the load constant instruction.` |
|       - |  3385 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3386 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5843 |  3387 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1946 |  3388 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1946 |  3389 | `				nIdx,0,0);` |
|    3897 |  3390 | `			return SXRET_OK;` |
|       - |  3391 | `		}` |
|     ! 0 |  3392 | `	}` |
|       - |  3393 | `	/* Single-token literal: load directly */` |
|  883893 |  3394 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  883893 |  3395 | `	return rc;` |
|  443895 |  3396 | `}` |
|       - |  3397 | `/*` |
|       - |  3398 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3399 | ` */` |
|       - |  3400 | `/*` |
|       - |  3401 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - |  3402 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - |  3403 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - |  3404 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - |  3405 | ` */` |
|     ! 0 |  3406 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 |  3407 | `{` |
|     ! 0 |  3408 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 |  3409 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - |  3410 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 |  3411 | `	return SXERR_SYNTAX;` |
|     ! 0 |  3412 | `}` |
|  887780 |  3413 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3414 | `{` |
|       - |  3415 | `	sxi32 rc;` |
|  887785 |  3416 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  887785 |  3417 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3418 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3419 | `		return rc;` |
|       - |  3420 | `	}` |
|       - |  3421 | `	/* Node successfully compiled */` |
|  887785 |  3422 | `	return SXRET_OK;` |
|  443895 |  3423 | `}` |
|       - |  3424 | `/*` |
|       - |  3425 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3426 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3427 | ` */` |
|       8 |  3428 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3429 | `{` |
|       - |  3430 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3431 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3432 | `		pGen->pIn++;` |
|       1 |  3433 | `	}` |
|       9 |  3434 | `	return SXRET_OK;` |
|       1 |  3435 | `}` |
|       - |  3436 | `/*` |
|       - |  3437 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3438 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3439 | ` */` |
|     134 |  3440 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3441 | `{` |
|     139 |  3442 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      34 |  3443 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3444 | `			return TRUE;` |
|      32 |  3445 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3446 | `			return TRUE;` |
|       3 |  3447 | `		}` |
|     121 |  3448 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3449 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3450 | `			return TRUE;` |
|       - |  3451 | `		}` |
|     ! 0 |  3452 | `	}` |
|       - |  3453 | `	/* Not a reserved constant */` |
|     131 |  3454 | `	return FALSE;` |
|      72 |  3455 | `}` |
|       - |  3456 | `/*` |
|       - |  3457 | ` * Compile the 'const' statement.` |
|       - |  3458 | ` * According to the PHP language reference` |
|       - |  3459 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3460 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3461 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3462 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3463 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3464 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3465 | ` *  Syntax` |
|       - |  3466 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3467 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3468 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3469 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3470 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3471 | ` *  to get a list of all defined constants.` |
|       - |  3472 | ` *` |
|       - |  3473 | ` * Symisc eXtension.` |
|       - |  3474 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3475 | ` *  would allow only simple scalar value.` |
|       - |  3476 | ` *  Example` |
|       - |  3477 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3478 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3479 | ` */` |
|      38 |  3480 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3481 | `{` |
|       - |  3482 | `	SySet *pConsCode,*pInstrContainer;` |
|      43 |  3483 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3484 | `	SyString *pName;` |
|       - |  3485 | `	sxi32 rc;` |
|      43 |  3486 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      43 |  3487 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3488 | `		/* Invalid constant name */` |
|       9 |  3489 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       9 |  3490 | `		if( rc == SXERR_ABORT ){` |
|       - |  3491 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3492 | `			return SXERR_ABORT;` |
|       - |  3493 | `		}` |
|       9 |  3494 | `		goto Synchronize;` |
|       - |  3495 | `	}` |
|       - |  3496 | `	/* Peek constant name */` |
|      37 |  3497 | `	pName = &pGen->pIn->sData;` |
|       - |  3498 | `	/* Make sure the constant name isn't reserved */` |
|      37 |  3499 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3500 | `		/* Reserved constant */` |
|      10 |  3501 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3502 | `		if( rc == SXERR_ABORT ){` |
|       - |  3503 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3504 | `			return SXERR_ABORT;` |
|       - |  3505 | `		}` |
|      10 |  3506 | `		goto Synchronize;` |
|       - |  3507 | `	}` |
|      28 |  3508 | `	pGen->pIn++;` |
|      28 |  3509 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3510 | `		/* Invalid statement*/` |
|       6 |  3511 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3512 | `		if( rc == SXERR_ABORT ){` |
|       - |  3513 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3514 | `			return SXERR_ABORT;` |
|       - |  3515 | `		}` |
|       6 |  3516 | `		goto Synchronize;` |
|       - |  3517 | `	}` |
|      22 |  3518 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3519 | `	/* Allocate a new constant value container */` |
|      22 |  3520 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      22 |  3521 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3522 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3523 | `		return SXERR_ABORT;` |
|       - |  3524 | `	}` |
|      22 |  3525 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3526 | `	/* Swap bytecode container */` |
|      22 |  3527 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      22 |  3528 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3529 | `	/* Compile constant value */` |
|      22 |  3530 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3531 | `	/* Emit the done instruction */` |
|      22 |  3532 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      22 |  3533 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      22 |  3534 | `	if( rc == SXERR_ABORT ){` |
|       - |  3535 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3536 | `		return SXERR_ABORT;` |
|       - |  3537 | `	}` |
|      22 |  3538 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3539 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3540 | `	{` |
|       - |  3541 | `		SyBlob sFQN;` |
|       - |  3542 | `		SyString sFQNStr;` |
|      22 |  3543 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      22 |  3544 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      22 |  3545 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      22 |  3546 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      22 |  3547 | `		SyBlobRelease(&sFQN);` |
|       - |  3548 | `	}` |
|      22 |  3549 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3550 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3551 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3552 | `	}` |
|      22 |  3553 | `	return SXRET_OK;` |
|       9 |  3554 | `Synchronize:` |
|       - |  3555 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3556 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      42 |  3557 | `		pGen->pIn++;` |
|       4 |  3558 | `	}` |
|      22 |  3559 | `	return SXRET_OK;` |
|      24 |  3560 | `}` |
|       - |  3561 | `/*` |
|       - |  3562 | ` * Compile the 'continue' statement.` |
|       - |  3563 | ` * According to the PHP language reference` |
|       - |  3564 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3565 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3566 | ` *  iteration.` |
|       - |  3567 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3568 | ` *  the purposes of continue.` |
|       - |  3569 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3570 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3571 | ` *  Note:` |
|       - |  3572 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3573 | ` */` |
|       - |  3574 | `/*` |
|       - |  3575 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3576 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3577 | ` * break/continue crosses a try boundary.` |
|       - |  3578 | ` *` |
|       - |  3579 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3580 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3581 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3582 | ` */` |
|    3990 |  3583 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3584 | `{` |
|    3995 |  3585 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    3995 |  3586 | `	int nInlineTry = 0;` |
|   23443 |  3587 | `	while( pBlock && pBlock != pTarget ){` |
|   19453 |  3588 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       6 |  3589 | `			if( pBlock->pUserData ){` |
|       - |  3590 | `				/* A try block with an exception context. In a generator its catch/finally` |
|       - |  3591 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|       - |  3592 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|       - |  3593 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|       6 |  3594 | `				if( pGen->bInGenerator ){` |
|       3 |  3595 | `					nInlineTry++;` |
|       2 |  3596 | `				}else{` |
|       3 |  3597 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       - |  3598 | `				}` |
|       4 |  3599 | `			}else{` |
|       - |  3600 | `				/* A catch/finally block compiled into a separate bytecode container` |
|       - |  3601 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|     ! 0 |  3602 | `				break;` |
|       - |  3603 | `			}` |
|       2 |  3604 | `		}` |
|   19453 |  3605 | `		pBlock = pBlock->pParent;` |
|       5 |  3606 | `	}` |
|    3995 |  3607 | `	return nInlineTry;` |
|       5 |  3608 | `}` |
|    3892 |  3609 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3610 | `{` |
|       - |  3611 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3612 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3613 | `	sxu32 nLineLocal;` |
|       - |  3614 | `	sxi32 rc;` |
|    3897 |  3615 | `	nLineLocal = pGen->pIn->nLine;` |
|    3897 |  3616 | `	iLevel = 0;` |
|       - |  3617 | `	/* Jump the 'continue' keyword */` |
|    3897 |  3618 | `	pGen->pIn++;` |
|    3897 |  3619 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3620 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3621 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3622 | `		 */` |
|       - |  3623 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3624 | `		char *zAlloc = 0;` |
|       - |  3625 | `		SyString sNum;` |
|      17 |  3626 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3627 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3628 | `			return SXERR_ABORT;` |
|       - |  3629 | `		}` |
|      17 |  3630 | `		if( rc == SXRET_OK ){` |
|      20 |  3631 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3632 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3633 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3634 | `				return SXERR_ABORT;` |
|       - |  3635 | `			}` |
|      14 |  3636 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3637 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3638 | `		}` |
|      17 |  3639 | `		if( iLevel < 2 ){` |
|       3 |  3640 | `			iLevel = 0;` |
|       1 |  3641 | `		}` |
|      17 |  3642 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3643 | `	}` |
|       - |  3644 | `	/* Point to the target loop */` |
|    3897 |  3645 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3897 |  3646 | `	if( pLoop == 0 ){` |
|       - |  3647 | `		/* Illegal continue */` |
|      12 |  3648 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      12 |  3649 | `		if( rc == SXERR_ABORT ){` |
|       - |  3650 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3651 | `			return SXERR_ABORT;` |
|       - |  3652 | `		}` |
|       7 |  3653 | `	}else{` |
|    3887 |  3654 | `		sxu32 nInstrIdx = 0;` |
|       - |  3655 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    3887 |  3656 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|       - |  3657 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|       - |  3658 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    3887 |  3659 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    3887 |  3660 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3661 | `			/* According to the PHP language reference manual` |
|       - |  3662 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3663 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3664 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3665 | `			 */` |
|       5 |  3666 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|       5 |  3667 | `			if( rc == SXRET_OK ){` |
|       5 |  3668 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3669 | `			}` |
|       3 |  3670 | `		}else{` |
|       - |  3671 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3883 |  3672 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3883 |  3673 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3674 | `				JumpFixup sJumpFix;` |
|       - |  3675 | `				/* Post-continue */` |
|      14 |  3676 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3677 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3678 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3679 | `			}` |
|       - |  3680 | `		}` |
|       - |  3681 | `	}` |
|    3897 |  3682 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3683 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3684 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3685 | `	}` |
|       - |  3686 | `	/* Statement successfully compiled */` |
|    3897 |  3687 | `	return SXRET_OK;` |
|    1951 |  3688 | `}` |
|       - |  3689 | `/*` |
|       - |  3690 | ` * Compile the 'break' statement.` |
|       - |  3691 | ` * According to the PHP language reference` |
|       - |  3692 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3693 | ` *  structure.` |
|       - |  3694 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3695 | ` *  enclosing structures are to be broken out of.` |
|       - |  3696 | ` */` |
|     124 |  3697 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3698 | `{` |
|       - |  3699 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3700 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3701 | `	sxi32 rc;` |
|     129 |  3702 | `	iLevel = 0;` |
|       - |  3703 | `	/* Jump the 'break' keyword */` |
|     129 |  3704 | `	pGen->pIn++;` |
|     129 |  3705 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3706 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3707 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3708 | `		 */` |
|       - |  3709 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      18 |  3710 | `		char *zAlloc = 0;` |
|       - |  3711 | `		SyString sNum;` |
|      18 |  3712 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      18 |  3713 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3714 | `			return SXERR_ABORT;` |
|       - |  3715 | `		}` |
|      18 |  3716 | `		if( rc == SXRET_OK ){` |
|      21 |  3717 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3718 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3719 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3720 | `				return SXERR_ABORT;` |
|       - |  3721 | `			}` |
|      15 |  3722 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3723 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3724 | `		}` |
|      18 |  3725 | `		if( iLevel < 2 ){` |
|       3 |  3726 | `			iLevel = 0;` |
|       1 |  3727 | `		}` |
|      18 |  3728 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3729 | `	}` |
|       - |  3730 | `	/* Extract the target loop */` |
|     129 |  3731 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     129 |  3732 | `	if( pLoop == 0 ){` |
|       - |  3733 | `		/* Illegal break */` |
|      19 |  3734 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      19 |  3735 | `		if( rc == SXERR_ABORT ){` |
|       - |  3736 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3737 | `			return SXERR_ABORT;` |
|       - |  3738 | `		}` |
|      11 |  3739 | `	}else{` |
|       - |  3740 | `		sxu32 nInstrIdx;` |
|       - |  3741 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     113 |  3742 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|       - |  3743 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     113 |  3744 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     113 |  3745 | `		if( rc == SXRET_OK ){` |
|       - |  3746 | `			/* Fix the jump later when the jump destination is resolved */` |
|     113 |  3747 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      54 |  3748 | `		}` |
|       - |  3749 | `	}` |
|     129 |  3750 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3751 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3752 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3753 | `	}` |
|       - |  3754 | `	/* Statement successfully compiled */` |
|     129 |  3755 | `	return SXRET_OK;` |
|      67 |  3756 | `}` |
|       - |  3757 | `/*` |
|       - |  3758 | ` * Compile or record a label.` |
|       - |  3759 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3760 | ` * Example` |
|       - |  3761 | ` *  goto LABEL;` |
|       - |  3762 | ` *   echo 'Foo';` |
|       - |  3763 | ` *  LABEL:` |
|       - |  3764 | ` *   echo 'Bar';` |
|       - |  3765 | ` */` |
|     112 |  3766 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3767 | `{` |
|       - |  3768 | `	GenBlock *pBlock;` |
|       - |  3769 | `	Label sLabel;` |
|       - |  3770 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3771 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3772 | `	if( pBlock ){` |
|       - |  3773 | `		sxi32 rc;` |
|       8 |  3774 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3775 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3776 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3777 | `			return SXERR_ABORT;` |
|       - |  3778 | `		}` |
|       4 |  3779 | `	}else{` |
|     113 |  3780 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3781 | `		char *zDup;` |
|       - |  3782 | `		/* Initialize label fields */` |
|     113 |  3783 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3784 | `		/* Duplicate label name */` |
|     113 |  3785 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3786 | `		if( zDup == 0 ){` |
|     ! 0 |  3787 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3788 | `			return SXERR_ABORT;` |
|       - |  3789 | `		}` |
|     113 |  3790 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3791 | `		sLabel.bRef  = FALSE;` |
|     113 |  3792 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3793 | `		pBlock = pGen->pCurrent;` |
|     221 |  3794 | `		while( pBlock ){` |
|     133 |  3795 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      24 |  3796 | `				break;` |
|       - |  3797 | `			}` |
|       - |  3798 | `			/* Point to the upper block */` |
|     113 |  3799 | `			pBlock = pBlock->pParent;` |
|       5 |  3800 | `		}` |
|     113 |  3801 | `		if( pBlock ){` |
|      24 |  3802 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      14 |  3803 | `		}else{` |
|      93 |  3804 | `			sLabel.pFunc = 0;` |
|       - |  3805 | `		}` |
|       - |  3806 | `		/* Insert in label set */` |
|     113 |  3807 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3808 | `	}` |
|     117 |  3809 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3810 | `	return SXRET_OK;` |
|      61 |  3811 | `}` |
|       - |  3812 | `/*` |
|       - |  3813 | ` * Compile the so hated 'goto' statement.` |
|       - |  3814 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3815 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3816 | ` * a compiler it has to do this.` |
|       - |  3817 | ` * According to the PHP language reference manual` |
|       - |  3818 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3819 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3820 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3821 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3822 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3823 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3824 | ` *   of a multi-level break` |
|       - |  3825 | ` */` |
|     152 |  3826 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3827 | `{` |
|       - |  3828 | `	JumpFixup sJump;` |
|       - |  3829 | `	sxi32 rc;` |
|     157 |  3830 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3831 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3832 | `		/* Missing label */` |
|     ! 0 |  3833 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3834 | `		if( rc == SXERR_ABORT ){` |
|       - |  3835 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3836 | `			return SXERR_ABORT;` |
|       - |  3837 | `		}` |
|     ! 0 |  3838 | `		return SXRET_OK;` |
|       - |  3839 | `	}` |
|     157 |  3840 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  3841 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       6 |  3842 | `		if( rc == SXERR_ABORT ){` |
|       - |  3843 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3844 | `			return SXERR_ABORT;` |
|       - |  3845 | `		}` |
|       4 |  3846 | `	}else{` |
|     153 |  3847 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3848 | `		GenBlock *pBlock;` |
|       - |  3849 | `		char *zDup;` |
|       - |  3850 | `		/* Prepare the jump destination */` |
|     153 |  3851 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3852 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3853 | `		/* Duplicate label name */` |
|     153 |  3854 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3855 | `		if( zDup == 0 ){` |
|     ! 0 |  3856 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3857 | `			return SXERR_ABORT;` |
|       - |  3858 | `		}` |
|     153 |  3859 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3860 | `		pBlock = pGen->pCurrent;` |
|     315 |  3861 | `		while( pBlock ){` |
|     199 |  3862 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      36 |  3863 | `				break;` |
|       - |  3864 | `			}` |
|       - |  3865 | `			/* Point to the upper block */` |
|     167 |  3866 | `			pBlock = pBlock->pParent;` |
|       5 |  3867 | `		}` |
|     153 |  3868 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       8 |  3869 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       8 |  3870 | `			if( rc == SXERR_ABORT ){` |
|       - |  3871 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3872 | `				return SXERR_ABORT;` |
|       - |  3873 | `			}` |
|       3 |  3874 | `		}` |
|     153 |  3875 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      29 |  3876 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      16 |  3877 | `		}else{` |
|     127 |  3878 | `			sJump.pFunc = 0;` |
|       - |  3879 | `		}` |
|       - |  3880 | `		/* Emit the unconditional jump */` |
|     153 |  3881 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3882 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3883 | `		}` |
|       - |  3884 | `	}` |
|     157 |  3885 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3886 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3887 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3888 | `	}` |
|       - |  3889 | `	/* Statement successfully compiled */` |
|     157 |  3890 | `	return SXRET_OK;` |
|      81 |  3891 | `}` |
|       - |  3892 | `/*` |
|       - |  3893 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3894 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3895 | ` * failure.` |
|       - |  3896 | ` */` |
|      20 |  3897 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  3898 | `{` |
|       - |  3899 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3900 | `	sxu32 nRawObj;` |
|      10 |  3901 | `	sxu32 nObjIdx;` |
|       - |  3902 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3903 | `	 * a PHP block.` |
|       - |  3904 | `	 */` |
|      10 |  3905 | `Consume:` |
|      22 |  3906 | `	nRawObj = nObjIdx = 0;` |
|      22 |  3907 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3908 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3909 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3910 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3911 | `			return SXERR_ABORT;` |
|       - |  3912 | `		}` |
|       - |  3913 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3914 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3915 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3916 | `		++nRawObj;` |
|     ! 0 |  3917 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3918 | `	}` |
|      22 |  3919 | `	if( nRawObj > 0 ){` |
|       - |  3920 | `		/* Emit the consume instruction */` |
|     ! 0 |  3921 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3922 | `	}` |
|      22 |  3923 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3924 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3925 | `		/* Reset the token set */` |
|     ! 0 |  3926 | `		SySetReset(pTokenSet);` |
|       - |  3927 | `		/* Tokenize input */` |
|     ! 0 |  3928 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3929 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3930 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3931 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3932 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3933 | `		/* Advance the stream cursor */` |
|     ! 0 |  3934 | `		pGen->pRawIn++;` |
|       - |  3935 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3936 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3937 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3938 | `			sxi32 rc;` |
|       - |  3939 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3940 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3941 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3942 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3943 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3944 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3945 | `				return SXERR_ABORT;` |
|     ! 0 |  3946 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3947 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3948 | `			}` |
|     ! 0 |  3949 | `			goto Consume;` |
|       - |  3950 | `		}` |
|     ! 0 |  3951 | `	}else{` |
|       - |  3952 | `		/* No more chunks to process */` |
|      22 |  3953 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  3954 | `		return SXERR_EOF;` |
|       - |  3955 | `	}` |
|     ! 0 |  3956 | `	return SXRET_OK;` |
|      12 |  3957 | `}` |
|       - |  3958 | `/*` |
|       - |  3959 | ` * Compile a PHP block.` |
|       - |  3960 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3961 | ` * optionally delimited by braces {}.` |
|       - |  3962 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3963 | ` * and this function takes care of generating the appropriate error` |
|       - |  3964 | ` * message.` |
|       - |  3965 | ` */` |
|  513090 |  3966 | `static sxi32 PH7_CompileBlock(` |
|       - |  3967 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3968 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3969 | `	)` |
|       5 |  3970 | `{` |
|       - |  3971 | `	sxi32 rc;` |
|       - |  3972 | `	sxu32 nLine;` |
|  513095 |  3973 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  511653 |  3974 | `		nLine = pGen->pIn->nLine;` |
|  511653 |  3975 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  511653 |  3976 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3977 | `			return SXERR_ABORT;` |
|       - |  3978 | `		}` |
|  511653 |  3979 | `		pGen->pIn++;` |
|       - |  3980 | `		/* Compile until we hit the closing braces '}' */` |
|  699011 |  3981 | `		for(;;){` |
| 1398027 |  3982 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  3983 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  3984 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3985 | `			 	   return SXERR_ABORT;` |
|       - |  3986 | `				}` |
|      22 |  3987 | `				if( rc == SXERR_EOF ){` |
|       - |  3988 | `					/* No more token to process. Missing closing braces */` |
|      22 |  3989 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      22 |  3990 | `					break;` |
|       - |  3991 | `				}` |
|     ! 0 |  3992 | `			}` |
| 1398007 |  3993 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3994 | `				/* Closing braces found,break immediately*/` |
|  511633 |  3995 | `				pGen->pIn++;` |
|  511633 |  3996 | `				break;` |
|       - |  3997 | `			}` |
|       - |  3998 | `			/* Compile a single statement */` |
|  886379 |  3999 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  886379 |  4000 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4001 | `				return SXERR_ABORT;` |
|       - |  4002 | `			}` |
|       5 |  4003 | `		}` |
|  511653 |  4004 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  257271 |  4005 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  4006 | `		pGen->pIn++;` |
|     ! 0 |  4007 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  4008 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  4009 | `			return SXERR_ABORT;` |
|       - |  4010 | `		}` |
|       - |  4011 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  4012 | `		for(;;){` |
|     ! 0 |  4013 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  4014 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  4015 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  4016 | `			 	   return SXERR_ABORT;` |
|       - |  4017 | `				}` |
|     ! 0 |  4018 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  4019 | `					/* No more token to process */` |
|     ! 0 |  4020 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  4021 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  4022 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  4023 | `					}` |
|     ! 0 |  4024 | `					break;` |
|       - |  4025 | `				}` |
|     ! 0 |  4026 | `			}` |
|     ! 0 |  4027 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  4028 | `				sxi32 nKwrd;` |
|       - |  4029 | `				/* Keyword found */` |
|     ! 0 |  4030 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  4031 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  4032 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  4033 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  4034 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  4035 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  4036 | `						}` |
|     ! 0 |  4037 | `						break;` |
|       - |  4038 | `				}` |
|     ! 0 |  4039 | `			}` |
|       - |  4040 | `			/* Compile a single statement */` |
|     ! 0 |  4041 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  4042 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4043 | `				return SXERR_ABORT;` |
|       - |  4044 | `			}` |
|     ! 0 |  4045 | `		}` |
|     ! 0 |  4046 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  4047 | `	}else{` |
|       - |  4048 | `		/* Compile a single statement */` |
|    1447 |  4049 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1447 |  4050 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4051 | `			return SXERR_ABORT;` |
|       - |  4052 | `		}` |
|       - |  4053 | `	}` |
|       - |  4054 | `	/* Jump trailing semi-colons ';' */` |
|  513095 |  4055 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4056 | `		pGen->pIn++;` |
|     ! 0 |  4057 | `	}` |
|  513095 |  4058 | `	return SXRET_OK;` |
|  256550 |  4059 | `}` |
|       - |  4060 | `/*` |
|       - |  4061 | ` * Compile the gentle 'while' statement.` |
|       - |  4062 | ` * According to the PHP language reference` |
|       - |  4063 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  4064 | ` *  The basic form of a while statement is:` |
|       - |  4065 | ` *  while (expr)` |
|       - |  4066 | ` *   statement` |
|       - |  4067 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  4068 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  4069 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  4070 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  4071 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  4072 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  4073 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  4074 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  4075 | ` *  while (expr):` |
|       - |  4076 | ` *    statement` |
|       - |  4077 | ` *   endwhile;` |
|       - |  4078 | ` */` |
|    7820 |  4079 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  4080 | `{` |
|    7825 |  4081 | `	GenBlock *pWhileBlock = 0;` |
|    7825 |  4082 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  4083 | `	sxu32 nFalseJump;` |
|       - |  4084 | `	sxu32 nLine;` |
|       - |  4085 | `	sxi32 rc;` |
|    7825 |  4086 | `	nLine = pGen->pIn->nLine;` |
|       - |  4087 | `	/* Jump the 'while' keyword */` |
|    7825 |  4088 | `	pGen->pIn++;` |
|    7825 |  4089 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4090 | `		/* Syntax error */` |
|     ! 0 |  4091 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  4092 | `		if( rc == SXERR_ABORT ){` |
|       - |  4093 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4094 | `			return SXERR_ABORT;` |
|       - |  4095 | `		}` |
|     ! 0 |  4096 | `		goto Synchronize;` |
|       - |  4097 | `	}` |
|       - |  4098 | `	/* Jump the left parenthesis '(' */` |
|    7825 |  4099 | `	pGen->pIn++;` |
|       - |  4100 | `	/* Create the loop block */` |
|    7825 |  4101 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    7825 |  4102 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4103 | `		return SXERR_ABORT;` |
|       - |  4104 | `	}` |
|       - |  4105 | `	/* Delimit the condition */` |
|    7825 |  4106 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    7825 |  4107 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4108 | `		/* Empty expression */` |
|       3 |  4109 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  4110 | `		if( rc == SXERR_ABORT ){` |
|       - |  4111 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4112 | `			return SXERR_ABORT;` |
|       - |  4113 | `		}` |
|       1 |  4114 | `	}` |
|       - |  4115 | `	/* Swap token streams */` |
|    7825 |  4116 | `	pTmp = pGen->pEnd;` |
|    7825 |  4117 | `	pGen->pEnd = pEnd;` |
|       - |  4118 | `	/* Compile the expression */` |
|    7825 |  4119 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    7825 |  4120 | `	if( rc == SXERR_ABORT ){` |
|       - |  4121 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4122 | `		return SXERR_ABORT;` |
|       - |  4123 | `	}` |
|       - |  4124 | `	/* Update token stream */` |
|    7825 |  4125 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  4126 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4127 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4128 | `			return SXERR_ABORT;` |
|       - |  4129 | `		}` |
|     ! 0 |  4130 | `		pGen->pIn++;` |
|     ! 0 |  4131 | `	}` |
|       - |  4132 | `	/* Synchronize pointers */` |
|    7825 |  4133 | `	pGen->pIn  = &pEnd[1];` |
|    7825 |  4134 | `	pGen->pEnd = pTmp;` |
|       - |  4135 | `	/* Emit the false jump */` |
|    7825 |  4136 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4137 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    7825 |  4138 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  4139 | `	/* Compile the loop body */` |
|    7825 |  4140 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    7825 |  4141 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4142 | `		return SXERR_ABORT;` |
|       - |  4143 | `	}` |
|       - |  4144 | `	/* Emit the unconditional jump to the start of the loop */` |
|    7825 |  4145 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  4146 | `	/* Fix all jumps now the destination is resolved */` |
|    7825 |  4147 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4148 | `	/* Release the loop block */` |
|    7825 |  4149 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4150 | `	/* Statement successfully compiled */` |
|    7825 |  4151 | `	return SXRET_OK;` |
|     ! 0 |  4152 | `Synchronize:` |
|       - |  4153 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4154 | `	 * compiling this erroneous block.` |
|       - |  4155 | `	 */` |
|     ! 0 |  4156 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4157 | `		pGen->pIn++;` |
|     ! 0 |  4158 | `	}` |
|     ! 0 |  4159 | `	return SXRET_OK;` |
|    3915 |  4160 | `}` |
|       - |  4161 | `/*` |
|       - |  4162 | ` * Compile the ugly do..while() statement.` |
|       - |  4163 | ` * According to the PHP language reference` |
|       - |  4164 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  4165 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  4166 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  4167 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  4168 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  4169 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  4170 | ` *  would end immediately).` |
|       - |  4171 | ` *  There is just one syntax for do-while loops:` |
|       - |  4172 | ` *  <?php` |
|       - |  4173 | ` *  $i = 0;` |
|       - |  4174 | ` *  do {` |
|       - |  4175 | ` *   echo $i;` |
|       - |  4176 | ` *  } while ($i > 0);` |
|       - |  4177 | ` * ?>` |
|       - |  4178 | ` */` |
|       2 |  4179 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  4180 | `{` |
|       3 |  4181 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  4182 | `	GenBlock *pDoBlock = 0;` |
|       - |  4183 | `	sxu32 nLine;` |
|       - |  4184 | `	sxi32 rc;` |
|       3 |  4185 | `	nLine = pGen->pIn->nLine;` |
|       - |  4186 | `	/* Jump the 'do' keyword */` |
|       3 |  4187 | `	pGen->pIn++;` |
|       - |  4188 | `	/* Create the loop block */` |
|       3 |  4189 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  4190 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4191 | `		return SXERR_ABORT;` |
|       - |  4192 | `	}` |
|       - |  4193 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  4194 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  4195 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  4196 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4197 | `		return SXERR_ABORT;` |
|       - |  4198 | `	}` |
|       3 |  4199 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4200 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  4201 | `	}` |
|       3 |  4202 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  4203 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  4204 | `			/* Missing 'while' statement */` |
|       3 |  4205 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  4206 | `			if( rc == SXERR_ABORT ){` |
|       - |  4207 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4208 | `				return SXERR_ABORT;` |
|       - |  4209 | `			}` |
|       3 |  4210 | `			goto Synchronize;` |
|       - |  4211 | `	}` |
|       - |  4212 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  4213 | `	pGen->pIn++;` |
|     ! 0 |  4214 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4215 | `		/* Syntax error */` |
|     ! 0 |  4216 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  4217 | `		if( rc == SXERR_ABORT ){` |
|       - |  4218 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4219 | `			return SXERR_ABORT;` |
|       - |  4220 | `		}` |
|     ! 0 |  4221 | `		goto Synchronize;` |
|       - |  4222 | `	}` |
|       - |  4223 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  4224 | `	pGen->pIn++;` |
|       - |  4225 | `	/* Delimit the condition */` |
|     ! 0 |  4226 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  4227 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4228 | `		/* Empty expression */` |
|     ! 0 |  4229 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  4230 | `		if( rc == SXERR_ABORT ){` |
|       - |  4231 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4232 | `			return SXERR_ABORT;` |
|       - |  4233 | `		}` |
|     ! 0 |  4234 | `		goto Synchronize;` |
|       - |  4235 | `	}` |
|       - |  4236 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  4237 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  4238 | `		JumpFixup *aPost;` |
|       - |  4239 | `		VmInstr *pInstr;` |
|       - |  4240 | `		sxu32 nJumpDest;` |
|       - |  4241 | `		sxu32 n;` |
|     ! 0 |  4242 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  4243 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  4244 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  4245 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  4246 | `			if( pInstr ){` |
|       - |  4247 | `				/* Fix */` |
|     ! 0 |  4248 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  4249 | `			}` |
|     ! 0 |  4250 | `		}` |
|     ! 0 |  4251 | `	}` |
|       - |  4252 | `	/* Swap token streams */` |
|     ! 0 |  4253 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  4254 | `	pGen->pEnd = pEnd;` |
|       - |  4255 | `	/* Compile the expression */` |
|     ! 0 |  4256 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4257 | `	if( rc == SXERR_ABORT ){` |
|       - |  4258 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4259 | `		return SXERR_ABORT;` |
|       - |  4260 | `	}` |
|       - |  4261 | `	/* Update token stream */` |
|     ! 0 |  4262 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  4263 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4264 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4265 | `			return SXERR_ABORT;` |
|       - |  4266 | `		}` |
|     ! 0 |  4267 | `		pGen->pIn++;` |
|     ! 0 |  4268 | `	}` |
|     ! 0 |  4269 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  4270 | `	pGen->pEnd = pTmp;` |
|       - |  4271 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  4272 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  4273 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  4274 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4275 | `	/* Release the loop block */` |
|     ! 0 |  4276 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4277 | `	/* Statement successfully compiled */` |
|     ! 0 |  4278 | `	return SXRET_OK;` |
|       1 |  4279 | `Synchronize:` |
|       - |  4280 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4281 | `	 * compiling this erroneous block.` |
|       - |  4282 | `	 */` |
|       3 |  4283 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4284 | `		pGen->pIn++;` |
|     ! 0 |  4285 | `	}` |
|       3 |  4286 | `	return SXRET_OK;` |
|       2 |  4287 | `}` |
|       - |  4288 | `/*` |
|       - |  4289 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  4290 | ` * According to the PHP language reference` |
|       - |  4291 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  4292 | ` *  The syntax of a for loop is:` |
|       - |  4293 | ` *  for (expr1; expr2; expr3)` |
|       - |  4294 | ` *   statement` |
|       - |  4295 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  4296 | ` *  the beginning of the loop.` |
|       - |  4297 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  4298 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  4299 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  4300 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  4301 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  4302 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  4303 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  4304 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  4305 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  4306 | ` *  of using the for truth expression.` |
|       - |  4307 | ` */` |
|   15510 |  4308 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4309 | `{` |
|   15515 |  4310 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   15515 |  4311 | `	GenBlock *pForBlock = 0;` |
|       - |  4312 | `	sxu32 nFalseJump;` |
|       - |  4313 | `	sxu32 nLine;` |
|       - |  4314 | `	sxi32 rc;` |
|   15515 |  4315 | `	nLine = pGen->pIn->nLine;` |
|       - |  4316 | `	/* Jump the 'for' keyword */` |
|   15515 |  4317 | `	pGen->pIn++;` |
|   15515 |  4318 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4319 | `		/* Syntax error */` |
|     ! 0 |  4320 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4321 | `		if( rc == SXERR_ABORT ){` |
|       - |  4322 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4323 | `			return SXERR_ABORT;` |
|       - |  4324 | `		}` |
|     ! 0 |  4325 | `		return SXRET_OK;` |
|       - |  4326 | `	}` |
|       - |  4327 | `	/* Jump the left parenthesis '(' */` |
|   15515 |  4328 | `	pGen->pIn++;` |
|       - |  4329 | `	/* Delimit the init-expr;condition;post-expr */` |
|   15515 |  4330 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   15515 |  4331 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4332 | `		/* Empty expression */` |
|     ! 0 |  4333 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4334 | `		if( rc == SXERR_ABORT ){` |
|       - |  4335 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4336 | `			return SXERR_ABORT;` |
|       - |  4337 | `		}` |
|       - |  4338 | `		/* Synchronize */` |
|     ! 0 |  4339 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4340 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4341 | `			pGen->pIn++;` |
|     ! 0 |  4342 | `		}` |
|     ! 0 |  4343 | `		return SXRET_OK;` |
|       - |  4344 | `	}` |
|       - |  4345 | `	/* Swap token streams */` |
|   15515 |  4346 | `	pTmp = pGen->pEnd;` |
|   15515 |  4347 | `	pGen->pEnd = pEnd;` |
|       - |  4348 | `	/* Compile initialization expressions if available */` |
|   15515 |  4349 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4350 | `	/* Pop operand lvalues */` |
|   15515 |  4351 | `	if( rc == SXERR_ABORT ){` |
|       - |  4352 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4353 | `		return SXERR_ABORT;` |
|   15515 |  4354 | `	}else if( rc != SXERR_EMPTY ){` |
|   15513 |  4355 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7754 |  4356 | `	}` |
|   15515 |  4357 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4358 | `		/* Syntax error */` |
|     ! 0 |  4359 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4360 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4361 | `		if( rc == SXERR_ABORT ){` |
|       - |  4362 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4363 | `			return SXERR_ABORT;` |
|       - |  4364 | `		}` |
|     ! 0 |  4365 | `		return SXRET_OK;` |
|       - |  4366 | `	}` |
|       - |  4367 | `	/* Jump the trailing ';' */` |
|   15515 |  4368 | `	pGen->pIn++;` |
|       - |  4369 | `	/* Create the loop block */` |
|   15515 |  4370 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   15515 |  4371 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4372 | `		return SXERR_ABORT;` |
|       - |  4373 | `	}` |
|       - |  4374 | `	/* Deffer continue jumps */` |
|   15515 |  4375 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4376 | `	/* Compile the condition */` |
|   15515 |  4377 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   15515 |  4378 | `	if( rc == SXERR_ABORT ){` |
|       - |  4379 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4380 | `		return SXERR_ABORT;` |
|   15515 |  4381 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4382 | `		/* Emit the false jump */` |
|   15513 |  4383 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4384 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   15513 |  4385 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7754 |  4386 | `	}` |
|   15515 |  4387 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4388 | `		/* Syntax error */` |
|       6 |  4389 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4390 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4391 | `		if( rc == SXERR_ABORT ){` |
|       - |  4392 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4393 | `			return SXERR_ABORT;` |
|       - |  4394 | `		}` |
|       6 |  4395 | `		return SXRET_OK;` |
|       - |  4396 | `	}` |
|       - |  4397 | `	/* Jump the trailing ';' */` |
|   15511 |  4398 | `	pGen->pIn++;` |
|       - |  4399 | `	/* Save the post condition stream */` |
|   15511 |  4400 | `	pPostStart = pGen->pIn;` |
|       - |  4401 | `	/* Compile the loop body */` |
|   15511 |  4402 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   15511 |  4403 | `	pGen->pEnd = pTmp;` |
|   15511 |  4404 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   15511 |  4405 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4406 | `		return SXERR_ABORT;` |
|       - |  4407 | `	}` |
|       - |  4408 | `	/* Fix post-continue jumps */` |
|   15511 |  4409 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4410 | `		JumpFixup *aPost;` |
|       - |  4411 | `		VmInstr *pInstr;` |
|       - |  4412 | `		sxu32 nJumpDest;` |
|       - |  4413 | `		sxu32 n;` |
|      14 |  4414 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4415 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4416 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4417 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4418 | `			if( pInstr ){` |
|       - |  4419 | `				/* Fix jump */` |
|      14 |  4420 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4421 | `			}` |
|       8 |  4422 | `		}` |
|       6 |  4423 | `	}` |
|       - |  4424 | `	/* compile the post-expressions if available */` |
|   15511 |  4425 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4426 | `		pPostStart++;` |
|     ! 0 |  4427 | `	}` |
|   15511 |  4428 | `	if( pPostStart < pEnd ){` |
|       - |  4429 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   15511 |  4430 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   15511 |  4431 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   15511 |  4432 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4433 | `			/* Syntax error */` |
|     ! 0 |  4434 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4435 | `			if( rc == SXERR_ABORT ){` |
|       - |  4436 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4437 | `				return SXERR_ABORT;` |
|       - |  4438 | `			}` |
|     ! 0 |  4439 | `			return SXRET_OK;` |
|       - |  4440 | `		}` |
|   15511 |  4441 | `		RE_SWAP_DELIMITER(pGen);` |
|   15511 |  4442 | `		if( rc == SXERR_ABORT ){` |
|       - |  4443 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4444 | `			return SXERR_ABORT;` |
|   15511 |  4445 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4446 | `			/* Pop operand lvalue */` |
|   15511 |  4447 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7753 |  4448 | `		}` |
|    7753 |  4449 | `	}` |
|       - |  4450 | `	/* Emit the unconditional jump to the start of the loop */` |
|   15511 |  4451 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4452 | `	/* Fix all jumps now the destination is resolved */` |
|   15511 |  4453 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4454 | `	/* Release the loop block */` |
|   15511 |  4455 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4456 | `	/* Statement successfully compiled */` |
|   15511 |  4457 | `	return SXRET_OK;` |
|    7760 |  4458 | `}` |
|       - |  4459 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4460 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4461 | ` * are allowed.` |
|       - |  4462 | ` */` |
|   16054 |  4463 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4464 | `{` |
|   16059 |  4465 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   16059 |  4466 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4467 | `		/* Unexpected expression */` |
|     ! 0 |  4468 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4469 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4470 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4471 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4472 | `		}` |
|     ! 0 |  4473 | `	}` |
|   16059 |  4474 | `	return rc;` |
|       5 |  4475 | `}` |
|       - |  4476 | `/*` |
|       - |  4477 | ` * Compile the 'foreach' statement.` |
|       - |  4478 | ` * According to the PHP language reference` |
|       - |  4479 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4480 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4481 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4482 | ` *  is a minor but useful extension of the first:` |
|       - |  4483 | ` *  foreach (array_expression as $value)` |
|       - |  4484 | ` *    statement` |
|       - |  4485 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4486 | ` *   statement` |
|       - |  4487 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4488 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4489 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4490 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4491 | ` *  to the variable $key on each loop.` |
|       - |  4492 | ` *  Note:` |
|       - |  4493 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4494 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4495 | ` *  Note:` |
|       - |  4496 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4497 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4498 | ` *  or after the foreach without resetting it.` |
|       - |  4499 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4500 | ` *  of copying the value.` |
|       - |  4501 | ` */` |
|   12012 |  4502 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4503 | `{` |
|   12017 |  4504 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   12017 |  4505 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   12017 |  4506 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4507 | `	ph7_foreach_info *pInfo;` |
|       - |  4508 | `	sxu32 nFalseJump;` |
|       - |  4509 | `	VmInstr *pInstr;` |
|       - |  4510 | `	sxu32 nLine;` |
|       - |  4511 | `	sxi32 rc;` |
|   12017 |  4512 | `	nLine = pGen->pIn->nLine;` |
|       - |  4513 | `	/* Jump the 'foreach' keyword */` |
|   12017 |  4514 | `	pGen->pIn++;` |
|   12017 |  4515 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4516 | `		/* Syntax error */` |
|     ! 0 |  4517 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4518 | `		if( rc == SXERR_ABORT ){` |
|       - |  4519 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4520 | `			return SXERR_ABORT;` |
|       - |  4521 | `		}` |
|     ! 0 |  4522 | `		goto Synchronize;` |
|       - |  4523 | `	}` |
|       - |  4524 | `	/* Jump the left parenthesis '(' */` |
|   12017 |  4525 | `	pGen->pIn++;` |
|       - |  4526 | `	/* Create the loop block */` |
|   12017 |  4527 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   12017 |  4528 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4529 | `		return SXERR_ABORT;` |
|       - |  4530 | `	}` |
|       - |  4531 | `	/* Delimit the expression */` |
|   12017 |  4532 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   12017 |  4533 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4534 | `		/* Empty expression */` |
|     ! 0 |  4535 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4536 | `		if( rc == SXERR_ABORT ){` |
|       - |  4537 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4538 | `			return SXERR_ABORT;` |
|       - |  4539 | `		}` |
|       - |  4540 | `		/* Synchronize */` |
|     ! 0 |  4541 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4542 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4543 | `			pGen->pIn++;` |
|     ! 0 |  4544 | `		}` |
|     ! 0 |  4545 | `		return SXRET_OK;` |
|       - |  4546 | `	}` |
|       - |  4547 | `	/* Compile the array expression */` |
|   12017 |  4548 | `	pCur = pGen->pIn;` |
|   75975 |  4549 | `	while( pCur < pEnd ){` |
|   75975 |  4550 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   12031 |  4551 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   12031 |  4552 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4553 | `				/* Break with the first 'as' found */` |
|   12017 |  4554 | `				break;` |
|       - |  4555 | `			}` |
|       7 |  4556 | `		}` |
|       - |  4557 | `		/* Advance the stream cursor */` |
|   63963 |  4558 | `		pCur++;` |
|       5 |  4559 | `	}` |
|   12017 |  4560 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4561 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4562 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4563 | `		if( rc == SXERR_ABORT ){` |
|       - |  4564 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4565 | `			return SXERR_ABORT;` |
|       - |  4566 | `		}` |
|     ! 0 |  4567 | `		goto Synchronize;` |
|       - |  4568 | `	}` |
|       - |  4569 | `	/* Swap token streams */` |
|   12017 |  4570 | `	pTmp = pGen->pEnd;` |
|   12017 |  4571 | `	pGen->pEnd = pCur;` |
|   12017 |  4572 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12017 |  4573 | `	if( rc == SXERR_ABORT ){` |
|       - |  4574 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4575 | `		return SXERR_ABORT;` |
|       - |  4576 | `	}` |
|       - |  4577 | `	/* Update token stream */` |
|   12017 |  4578 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4579 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4580 | `		if( rc == SXERR_ABORT ){` |
|       - |  4581 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4582 | `			return SXERR_ABORT;` |
|       - |  4583 | `		}` |
|     ! 0 |  4584 | `		pGen->pIn++;` |
|     ! 0 |  4585 | `	}` |
|   12017 |  4586 | `	pCur++; /* Jump the 'as' keyword */` |
|   12017 |  4587 | `	pGen->pIn = pCur;` |
|   12017 |  4588 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4589 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4590 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4591 | `			return SXERR_ABORT;` |
|       - |  4592 | `		}` |
|     ! 0 |  4593 | `	}` |
|       - |  4594 | `	/* Create the foreach context */` |
|   12017 |  4595 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   12017 |  4596 | `	if( pInfo == 0 ){` |
|     ! 0 |  4597 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4598 | `		return SXERR_ABORT;` |
|       - |  4599 | `	}` |
|       - |  4600 | `	/* Zero the structure */` |
|   12017 |  4601 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4602 | `	/* Initialize structure fields */` |
|   12017 |  4603 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4604 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4605 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4606 | `	 * '=>'. */` |
|   12017 |  4607 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   12017 |  4608 | `	if( pCur < pEnd ){` |
|       - |  4609 | `		/* Compile the expression holding the key name */` |
|    4067 |  4610 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4611 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4612 | `			if( rc == SXERR_ABORT ){` |
|       - |  4613 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4614 | `				return SXERR_ABORT;` |
|       - |  4615 | `			}` |
|     ! 0 |  4616 | `		}else{` |
|    4067 |  4617 | `			pGen->pEnd = pCur;` |
|    4067 |  4618 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    4067 |  4619 | `			if( rc == SXERR_ABORT ){` |
|       - |  4620 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4621 | `				return SXERR_ABORT;` |
|       - |  4622 | `			}` |
|    4067 |  4623 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    4067 |  4624 | `			if( pInstr->p3 ){` |
|       - |  4625 | `				/* Record key name */` |
|    4067 |  4626 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    2031 |  4627 | `			}` |
|    4067 |  4628 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4629 | `		}` |
|    4067 |  4630 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    2031 |  4631 | `	}` |
|   12017 |  4632 | `	pGen->pEnd = pEnd;` |
|   12017 |  4633 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4634 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4635 | `		if( rc == SXERR_ABORT ){` |
|       - |  4636 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4637 | `			return SXERR_ABORT;` |
|       - |  4638 | `		}` |
|     ! 0 |  4639 | `		goto Synchronize;` |
|       - |  4640 | `	}` |
|   12017 |  4641 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4642 | `		pGen->pIn++;` |
|       - |  4643 | `		/* Pass by reference  */` |
|      11 |  4644 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4645 | `	}` |
|       - |  4646 | `	/* Check if the value target is list() */` |
|   12017 |  4647 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4648 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4649 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4650 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4651 | `		 */` |
|       - |  4652 | `		static int iForeachListCnt = 0;` |
|       - |  4653 | `		char zTmp[128];` |
|       - |  4654 | `		sxu32 nLen;` |
|       - |  4655 | `		char *zDup;` |
|      10 |  4656 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4657 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4658 | `		if( zDup == 0 ){` |
|     ! 0 |  4659 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4660 | `			return SXERR_ABORT;` |
|       - |  4661 | `		}` |
|      10 |  4662 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4663 | `		/* Save list() token boundaries */` |
|      10 |  4664 | `		pListStart = pGen->pIn;` |
|       - |  4665 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4666 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4667 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4668 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4669 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4670 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4671 | `				return SXERR_ABORT;` |
|       - |  4672 | `			}` |
|       3 |  4673 | `			goto Synchronize;` |
|       - |  4674 | `		}` |
|       7 |  4675 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4676 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4677 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4678 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4679 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4680 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4681 | `				return SXERR_ABORT;` |
|       - |  4682 | `			}` |
|     ! 0 |  4683 | `			goto Synchronize;` |
|       - |  4684 | `		}` |
|       7 |  4685 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4686 | `		pListEnd = pGen->pIn;` |
|       7 |  4687 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   12012 |  4688 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4689 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4690 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4691 | `		 */` |
|       - |  4692 | `		static int iForeachShortListCnt = 0;` |
|       - |  4693 | `		char zTmp[128];` |
|       - |  4694 | `		sxu32 nLen;` |
|       - |  4695 | `		char *zDup;` |
|      13 |  4696 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|      13 |  4697 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      13 |  4698 | `		if( zDup == 0 ){` |
|     ! 0 |  4699 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4700 | `			return SXERR_ABORT;` |
|       - |  4701 | `		}` |
|      13 |  4702 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4703 | `		/* Save [...] token boundaries */` |
|      13 |  4704 | `		pListStart = pGen->pIn;` |
|       - |  4705 | `		/* Advance past [...] */` |
|      13 |  4706 | `		pGen->pIn++; /* Jump '[' */` |
|      13 |  4707 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|      13 |  4708 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4709 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4710 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4711 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4712 | `				return SXERR_ABORT;` |
|       - |  4713 | `			}` |
|     ! 0 |  4714 | `			goto Synchronize;` |
|       - |  4715 | `		}` |
|      13 |  4716 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|      13 |  4717 | `		pListEnd = pGen->pIn;` |
|      13 |  4718 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       7 |  4719 | `	}else{` |
|       - |  4720 | `		/* Compile the expression holding the value name */` |
|   11997 |  4721 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   11997 |  4722 | `		if( rc == SXERR_ABORT ){` |
|       - |  4723 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4724 | `			return SXERR_ABORT;` |
|       - |  4725 | `		}` |
|   11997 |  4726 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   11997 |  4727 | `		if( pInstr->p3 ){` |
|       - |  4728 | `			/* Record value name */` |
|   11997 |  4729 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    5996 |  4730 | `		}` |
|       - |  4731 | `	}` |
|       - |  4732 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   12015 |  4733 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4734 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   12015 |  4735 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4736 | `	/* Record the first instruction to execute */` |
|   12015 |  4737 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4738 | `	/* Emit the FOREACH_STEP instruction */` |
|   12015 |  4739 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4740 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   12015 |  4741 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4742 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   12015 |  4743 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4744 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4745 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4746 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4747 | `		 */` |
|      19 |  4748 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4749 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4750 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4751 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4752 | `		 */` |
|      19 |  4753 | `		pSavedIn = pGen->pIn;` |
|      19 |  4754 | `		pSavedEnd = pGen->pEnd;` |
|      19 |  4755 | `		pGen->pIn = pListStart;` |
|      19 |  4756 | `		pGen->pEnd = pListEnd;` |
|      19 |  4757 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|      13 |  4758 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  4759 | `		}else{` |
|       7 |  4760 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4761 | `		}` |
|      19 |  4762 | `		pGen->pIn = pSavedIn;` |
|      19 |  4763 | `		pGen->pEnd = pSavedEnd;` |
|      19 |  4764 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4765 | `			return SXERR_ABORT;` |
|       - |  4766 | `		}` |
|       - |  4767 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      19 |  4768 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       9 |  4769 | `	}` |
|       - |  4770 | `	/* Compile the loop body */` |
|   12015 |  4771 | `	pGen->pIn = &pEnd[1];` |
|   12015 |  4772 | `	pGen->pEnd = pTmp;` |
|   12015 |  4773 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   12015 |  4774 | `	if( rc == SXERR_ABORT ){` |
|       - |  4775 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4776 | `		return SXERR_ABORT;` |
|       - |  4777 | `	}` |
|       - |  4778 | `	/* Emit the unconditional jump to the start of the loop */` |
|   12015 |  4779 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4780 | `	/* Fix all jumps now the destination is resolved */` |
|   12015 |  4781 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4782 | `	/* Release the loop block */` |
|   12015 |  4783 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4784 | `	/* Statement successfully compiled */` |
|   12015 |  4785 | `	return SXRET_OK;` |
|       1 |  4786 | `Synchronize:` |
|       - |  4787 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4788 | `	 * compiling this erroneous block.` |
|       - |  4789 | `	 */` |
|       3 |  4790 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4791 | `		pGen->pIn++;` |
|     ! 0 |  4792 | `	}` |
|       3 |  4793 | `	return SXRET_OK;` |
|    6011 |  4794 | `}` |
|       - |  4795 | `/*` |
|       - |  4796 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4797 | ` * According to the PHP language reference` |
|       - |  4798 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4799 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4800 | ` *  that is similar to that of C:` |
|       - |  4801 | ` *  if (expr)` |
|       - |  4802 | ` *   statement` |
|       - |  4803 | ` *  else construct:` |
|       - |  4804 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4805 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4806 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4807 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4808 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4809 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4810 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4811 | ` *  elseif` |
|       - |  4812 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4813 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4814 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4815 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4816 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4817 | ` *   <?php` |
|       - |  4818 | ` *    if ($a > $b) {` |
|       - |  4819 | ` *     echo "a is bigger than b";` |
|       - |  4820 | ` *    } elseif ($a == $b) {` |
|       - |  4821 | ` *     echo "a is equal to b";` |
|       - |  4822 | ` *    } else {` |
|       - |  4823 | ` *     echo "a is smaller than b";` |
|       - |  4824 | ` *    }` |
|       - |  4825 | ` *    ?>` |
|       - |  4826 | ` */` |
|  190936 |  4827 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4828 | `{` |
|  190941 |  4829 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  190941 |  4830 | `	GenBlock *pCondBlock = 0;` |
|       - |  4831 | `	sxu32 nJumpIdx;` |
|       - |  4832 | `	sxu32 nKeyID;` |
|       - |  4833 | `	sxi32 rc;` |
|       - |  4834 | `	/* Jump the 'if' keyword */` |
|  190941 |  4835 | `	pGen->pIn++;` |
|  190941 |  4836 | `	pToken = pGen->pIn;` |
|       - |  4837 | `	/* Create the conditional block */` |
|  190941 |  4838 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  190941 |  4839 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4840 | `		return SXERR_ABORT;` |
|       - |  4841 | `	}` |
|       - |  4842 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  107059 |  4843 | `	for(;;){` |
|  214123 |  4844 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4845 | `			/* Syntax error */` |
|     ! 0 |  4846 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4847 | `				pToken--;` |
|     ! 0 |  4848 | `			}` |
|     ! 0 |  4849 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4850 | `			if( rc == SXERR_ABORT ){` |
|       - |  4851 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4852 | `				return SXERR_ABORT;` |
|       - |  4853 | `			}` |
|     ! 0 |  4854 | `			goto Synchronize;` |
|       - |  4855 | `		}` |
|       - |  4856 | `		/* Jump the left parenthesis '(' */` |
|  214123 |  4857 | `		pToken++;` |
|       - |  4858 | `		/* Delimit the condition */` |
|  214123 |  4859 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  214123 |  4860 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4861 | `			/* Syntax error */` |
|     ! 0 |  4862 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4863 | `				pToken--;` |
|     ! 0 |  4864 | `			}` |
|     ! 0 |  4865 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4866 | `			if( rc == SXERR_ABORT ){` |
|       - |  4867 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4868 | `				return SXERR_ABORT;` |
|       - |  4869 | `			}` |
|     ! 0 |  4870 | `			goto Synchronize;` |
|       - |  4871 | `		}` |
|       - |  4872 | `		/* Swap token streams */` |
|  214123 |  4873 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4874 | `		/* Compile the condition */` |
|  214123 |  4875 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4876 | `		/* Update token stream */` |
|  214123 |  4877 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4878 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4879 | `			pGen->pIn++;` |
|     ! 0 |  4880 | `		}` |
|  214123 |  4881 | `		pGen->pIn  = &pEnd[1];` |
|  214123 |  4882 | `		pGen->pEnd = pTmp;` |
|  214123 |  4883 | `		if( rc == SXERR_ABORT ){` |
|       - |  4884 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4885 | `			return SXERR_ABORT;` |
|       - |  4886 | `		}` |
|       - |  4887 | `		/* Emit the false jump */` |
|  214123 |  4888 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4889 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  214123 |  4890 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4891 | `		/* Compile the body */` |
|  214123 |  4892 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  214123 |  4893 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4894 | `			return SXERR_ABORT;` |
|       - |  4895 | `		}` |
|  214123 |  4896 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   48937 |  4897 | `			break;` |
|       - |  4898 | `		}` |
|       - |  4899 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  116259 |  4900 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  116259 |  4901 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   81071 |  4902 | `			break;` |
|       - |  4903 | `		}` |
|       - |  4904 | `		/* Emit the unconditional jump */` |
|   35193 |  4905 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4906 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   35193 |  4907 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   35193 |  4908 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   27387 |  4909 | `			pToken = &pGen->pIn[1];` |
|   27387 |  4910 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|   15414 |  4911 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    6008 |  4912 | `					break;` |
|       - |  4913 | `			}` |
|   15381 |  4914 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    7688 |  4915 | `		}` |
|   23187 |  4916 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4917 | `		/* Synchronize cursors */` |
|   23187 |  4918 | `		pToken = pGen->pIn;` |
|       - |  4919 | `		/* Fix the false jump */` |
|   23187 |  4920 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4921 | `	} /* For(;;) */` |
|       - |  4922 | `	/* Fix the false jump */` |
|  190941 |  4923 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  190941 |  4924 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   93072 |  4925 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4926 | `			/* Compile the else block */` |
|   12011 |  4927 | `			pGen->pIn++;` |
|   12011 |  4928 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   12011 |  4929 | `			if( rc == SXERR_ABORT ){` |
|       - |  4930 |  |
|     ! 0 |  4931 | `				return SXERR_ABORT;` |
|       - |  4932 | `			}` |
|    6003 |  4933 | `	}` |
|  190941 |  4934 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4935 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  190941 |  4936 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4937 | `	/* Release the conditional block */` |
|  190941 |  4938 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4939 | `	/* Statement successfully compiled */` |
|  190941 |  4940 | `	return SXRET_OK;` |
|     ! 0 |  4941 | `Synchronize:` |
|       - |  4942 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4943 | `	 */` |
|     ! 0 |  4944 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4945 | `		pGen->pIn++;` |
|     ! 0 |  4946 | `	}` |
|     ! 0 |  4947 | `	return SXRET_OK;` |
|   95473 |  4948 | `}` |
|       - |  4949 | `/*` |
|       - |  4950 | ` * Compile the global construct.` |
|       - |  4951 | ` * According to the PHP language reference` |
|       - |  4952 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4953 | ` *  to be used in that function.` |
|       - |  4954 | ` *  Example #1 Using global` |
|       - |  4955 | ` *  <?php` |
|       - |  4956 | ` *   $a = 1;` |
|       - |  4957 | ` *   $b = 2;` |
|       - |  4958 | ` *   function Sum()` |
|       - |  4959 | ` *   {` |
|       - |  4960 | ` *    global $a, $b;` |
|       - |  4961 | ` *    $b = $a + $b;` |
|       - |  4962 | ` *   }` |
|       - |  4963 | ` *   Sum();` |
|       - |  4964 | ` *   echo $b;` |
|       - |  4965 | ` *  ?>` |
|       - |  4966 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4967 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4968 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4969 | ` */` |
|      36 |  4970 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4971 | `{` |
|      41 |  4972 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4973 | `	sxi32 nExpr;` |
|       - |  4974 | `	sxi32 rc;` |
|       - |  4975 | `	/* Jump the 'global' keyword */` |
|      41 |  4976 | `	pGen->pIn++;` |
|      41 |  4977 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4978 | `		/* Nothing to process */` |
|     ! 0 |  4979 | `		return SXRET_OK;` |
|       - |  4980 | `	}` |
|      41 |  4981 | `	pTmp = pGen->pEnd;` |
|      41 |  4982 | `	nExpr = 0;` |
|      87 |  4983 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4984 | `		if( pGen->pIn < pNext ){` |
|      51 |  4985 | `			pGen->pEnd = pNext;` |
|      51 |  4986 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4987 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4988 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4989 | `					return SXERR_ABORT;` |
|       - |  4990 | `				}` |
|     ! 0 |  4991 | `			}else{` |
|      51 |  4992 | `				pGen->pIn++;` |
|      51 |  4993 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4994 | `					/* Emit a warning */` |
|     ! 0 |  4995 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4996 | `				}else{` |
|      51 |  4997 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4998 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4999 | `						return SXERR_ABORT;` |
|      51 |  5000 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  5001 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  5002 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  5003 | `							/* Variable name, not a constant */` |
|      51 |  5004 | `							pLast->iP1 = 0;` |
|      23 |  5005 | `						}` |
|      51 |  5006 | `						nExpr++;` |
|      23 |  5007 | `					}` |
|       - |  5008 | `				}` |
|       - |  5009 | `			}` |
|      23 |  5010 | `		}` |
|       - |  5011 | `		/* Next expression in the stream */` |
|      51 |  5012 | `		pGen->pIn = pNext;` |
|       - |  5013 | `		/* Jump trailing commas */` |
|      61 |  5014 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  5015 | `			pGen->pIn++;` |
|       5 |  5016 | `		}` |
|       5 |  5017 | `	}` |
|       - |  5018 | `	/* Restore token stream */` |
|      41 |  5019 | `	pGen->pEnd = pTmp;` |
|      41 |  5020 | `	if( nExpr > 0 ){` |
|       - |  5021 | `		/* Emit the uplink instruction */` |
|      41 |  5022 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  5023 | `	}` |
|      41 |  5024 | `	return SXRET_OK;` |
|      23 |  5025 | `}` |
|       - |  5026 | `/*` |
|       - |  5027 | ` * Compile the return statement.` |
|       - |  5028 | ` * According to the PHP language reference` |
|       - |  5029 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  5030 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  5031 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  5032 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  5033 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  5034 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  5035 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  5036 | ` *  from within the main script file, then script execution end.` |
|       - |  5037 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  5038 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  5039 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  5040 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  5041 | ` */` |
|  266868 |  5042 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  5043 | `{` |
|  266873 |  5044 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  5045 | `	sxi32 rc;` |
|  266873 |  5046 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  266873 |  5047 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - |  5048 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - |  5049 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - |  5050 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - |  5051 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - |  5052 | `	 * normally below so token processing stays consistent. */` |
|  665235 |  5053 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  398367 |  5054 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 |  5055 | `	}` |
|  266868 |  5056 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  266841 |  5057 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|       3 |  5058 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5059 | `			"A never-returning function must not return");` |
|       3 |  5060 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5061 | `			return SXERR_ABORT;` |
|       - |  5062 | `		}` |
|       1 |  5063 | `	}` |
|       - |  5064 | `	/* Jump the 'return' keyword */` |
|  266873 |  5065 | `	pGen->pIn++;` |
|  266873 |  5066 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  5067 | `		/* Compile the expression */` |
|  266843 |  5068 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  266843 |  5069 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5070 | `			return SXERR_ABORT;` |
|  266843 |  5071 | `		}else if(rc != SXERR_EMPTY ){` |
|  266843 |  5072 | `			nRet = 1;` |
|  133419 |  5073 | `		}` |
|  133419 |  5074 | `	}` |
|       - |  5075 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|       - |  5076 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|       - |  5077 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|       - |  5078 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  266873 |  5079 | `	if( pGen->bInGenerator ){` |
|      29 |  5080 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      29 |  5081 | `		return SXRET_OK;` |
|       - |  5082 | `	}` |
|       - |  5083 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  5084 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  5085 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  5086 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  5087 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  266847 |  5088 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  266847 |  5089 | `	return SXRET_OK;` |
|  133439 |  5090 | `}` |
|       - |  5091 | `/*` |
|       - |  5092 | ` * Compile a yield expression.` |
|       - |  5093 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  5094 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  5095 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  5096 | ` */` |
|     328 |  5097 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  5098 | `{` |
|       - |  5099 | `	SyToken *pTmp, *pSplit;` |
|     333 |  5100 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     333 |  5101 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  5102 | `	sxi32 rc;` |
|     164 |  5103 | `	(void)iCompileFlag;` |
|       - |  5104 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     333 |  5105 | `	pGen->pIn++;` |
|       - |  5106 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  5107 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  5108 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  5109 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  5110 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     328 |  5111 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     194 |  5112 | `		&& pGen->pIn->sData.nByte == 4` |
|      66 |  5113 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      64 |  5114 | `		pGen->pIn++; /* Skip 'from' */` |
|      64 |  5115 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      64 |  5116 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5117 | `			return SXERR_ABORT;` |
|       - |  5118 | `		}` |
|      64 |  5119 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  5120 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  5121 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  5122 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  5123 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5124 | `				return SXERR_ABORT;` |
|       - |  5125 | `			}` |
|     ! 0 |  5126 | `		}` |
|      64 |  5127 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      64 |  5128 | `		return SXRET_OK;` |
|       - |  5129 | `	}` |
|     273 |  5130 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5131 | `		/* Bare yield — no value */` |
|       3 |  5132 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  5133 | `		return SXRET_OK;` |
|       - |  5134 | `	}` |
|       - |  5135 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     271 |  5136 | `	pSplit = 0;` |
|       - |  5137 | `	{` |
|     271 |  5138 | `		SyToken *pCur = pGen->pIn;` |
|     271 |  5139 | `		sxi32 nNest = 0;` |
|     569 |  5140 | `		while( pCur < pGen->pEnd ){` |
|     317 |  5141 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  5142 | `				nNest++;` |
|     316 |  5143 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  5144 | `				nNest--;` |
|     314 |  5145 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  5146 | `				pSplit = pCur;` |
|      16 |  5147 | `				break;` |
|       - |  5148 | `			}` |
|     303 |  5149 | `			pCur++;` |
|       5 |  5150 | `		}` |
|       - |  5151 | `	}` |
|     271 |  5152 | `	pTmp = pGen->pEnd;` |
|     271 |  5153 | `	if( pSplit ){` |
|       - |  5154 | `		/* yield $key => $value */` |
|      16 |  5155 | `		pGen->pEnd = pSplit;` |
|      16 |  5156 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  5157 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  5158 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  5159 | `		pGen->pEnd = pTmp;` |
|      16 |  5160 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  5161 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  5162 | `		iP1 = 1;` |
|      16 |  5163 | `		iP2 = 1;` |
|       9 |  5164 | `	}else{` |
|       - |  5165 | `		/* yield $value */` |
|     257 |  5166 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     257 |  5167 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     257 |  5168 | `		if( rc != SXERR_EMPTY ){` |
|     257 |  5169 | `			iP1 = 1;` |
|     126 |  5170 | `		}` |
|       - |  5171 | `	}` |
|     271 |  5172 | `	pGen->pEnd = pTmp;` |
|     271 |  5173 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     271 |  5174 | `	return SXRET_OK;` |
|     169 |  5175 | `}` |
|       - |  5176 | `/*` |
|       - |  5177 | ` * Compile the die/exit language construct.` |
|       - |  5178 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  5179 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  5180 | ` */` |
|     122 |  5181 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  5182 | `{` |
|     127 |  5183 | `	sxi32 nExpr = 0;` |
|       - |  5184 | `	sxi32 rc;` |
|       - |  5185 | `	/* Jump the die/exit keyword */` |
|     127 |  5186 | `	pGen->pIn++;` |
|     127 |  5187 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  5188 | `		/* Compile the expression */` |
|     127 |  5189 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     127 |  5190 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5191 | `			return SXERR_ABORT;` |
|     127 |  5192 | `		}else if(rc != SXERR_EMPTY ){` |
|     127 |  5193 | `			nExpr = 1;` |
|      61 |  5194 | `		}` |
|      61 |  5195 | `	}` |
|       - |  5196 | `	/* Emit the HALT instruction */` |
|     127 |  5197 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     127 |  5198 | `	return SXRET_OK;` |
|      66 |  5199 | `}` |
|       - |  5200 | `/*` |
|       - |  5201 | ` * Compile the 'echo' language construct.` |
|       - |  5202 | ` */` |
|   15246 |  5203 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  5204 | `{` |
|   15251 |  5205 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  5206 | `	sxi32 rc;` |
|       - |  5207 | `	/* Jump the 'echo' keyword */` |
|   15251 |  5208 | `	pGen->pIn++;` |
|       - |  5209 | `	/* Compile arguments one after one */` |
|   15251 |  5210 | `	pTmp = pGen->pEnd;` |
|   35603 |  5211 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   20357 |  5212 | `		if( pGen->pIn < pNext ){` |
|   20357 |  5213 | `			pGen->pEnd = pNext;` |
|   20357 |  5214 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   20357 |  5215 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5216 | `				return SXERR_ABORT;` |
|   20357 |  5217 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  5218 | `				/* Emit the consume instruction */` |
|   20333 |  5219 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|   10164 |  5220 | `			}` |
|   10176 |  5221 | `		}` |
|       - |  5222 | `		/* Jump trailing commas */` |
|   25463 |  5223 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    5111 |  5224 | `			pNext++;` |
|       5 |  5225 | `		}` |
|   20357 |  5226 | `		pGen->pIn = pNext;` |
|       5 |  5227 | `	}` |
|       - |  5228 | `	/* Restore token stream */` |
|   15251 |  5229 | `	pGen->pEnd = pTmp;` |
|   15251 |  5230 | `	return SXRET_OK;` |
|    7628 |  5231 | `}` |
|       - |  5232 | `/*` |
|       - |  5233 | ` * Compile the static statement.` |
|       - |  5234 | ` * According to the PHP language reference` |
|       - |  5235 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  5236 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  5237 | ` *  when program execution leaves this scope.` |
|       - |  5238 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  5239 | ` * Symisc eXtension.` |
|       - |  5240 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  5241 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  5242 | ` *  Example` |
|       - |  5243 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  5244 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  5245 | ` */` |
|       8 |  5246 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       3 |  5247 | `{` |
|       - |  5248 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  5249 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  5250 | `	GenBlock *pBlock;` |
|       - |  5251 | `	SyString *pName;` |
|       - |  5252 | `	char *zDup;` |
|       - |  5253 | `	sxu32 nLine;` |
|       - |  5254 | `	sxi32 rc;` |
|       - |  5255 | `	/* Jump the static keyword */` |
|      11 |  5256 | `	nLine = pGen->pIn->nLine;` |
|      11 |  5257 | `	pGen->pIn++;` |
|       - |  5258 | `	/* Extract the enclosing function if any */` |
|      11 |  5259 | `	pBlock = pGen->pCurrent;` |
|      19 |  5260 | `	while( pBlock ){` |
|      19 |  5261 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      11 |  5262 | `			break;` |
|       - |  5263 | `		}` |
|       - |  5264 | `		/* Point to the upper block */` |
|      11 |  5265 | `		pBlock = pBlock->pParent;` |
|       3 |  5266 | `	}` |
|      11 |  5267 | `	if( pBlock == 0 ){` |
|       - |  5268 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  5269 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  5270 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  5271 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5272 | `				return SXERR_ABORT;` |
|       - |  5273 | `			}` |
|     ! 0 |  5274 | `			goto Synchronize;` |
|       - |  5275 | `		}` |
|       - |  5276 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  5277 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  5278 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5279 | `			return SXERR_ABORT;` |
|     ! 0 |  5280 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  5281 | `			/* Emit the POP instruction */` |
|     ! 0 |  5282 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  5283 | `		}` |
|     ! 0 |  5284 | `		return SXRET_OK;` |
|       - |  5285 | `	}` |
|      11 |  5286 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  5287 | `	/* Make sure we are dealing with a valid statement */` |
|      11 |  5288 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       6 |  5289 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  5290 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  5291 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5292 | `				return SXERR_ABORT;` |
|       - |  5293 | `			}` |
|       3 |  5294 | `			goto Synchronize;` |
|       - |  5295 | `	}` |
|       8 |  5296 | `	pGen->pIn++;` |
|       - |  5297 | `	/* Extract variable name */` |
|       8 |  5298 | `	pName = &pGen->pIn->sData;` |
|       8 |  5299 | `	pGen->pIn++; /* Jump the var name */` |
|       8 |  5300 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  5301 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  5302 | `		goto Synchronize;` |
|       - |  5303 | `	}` |
|       - |  5304 | `	/* Initialize the structure describing the static variable */` |
|       8 |  5305 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       8 |  5306 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  5307 | `	/* Duplicate variable name */` |
|       8 |  5308 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       8 |  5309 | `	if( zDup == 0 ){` |
|     ! 0 |  5310 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  5311 | `		return SXERR_ABORT;` |
|       - |  5312 | `	}` |
|       8 |  5313 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  5314 | `	/* Check if we have an expression to compile */` |
|       8 |  5315 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  5316 | `		SySet *pInstrContainer;` |
|       - |  5317 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  5318 | `		 * Static variable can take any complex expression including function` |
|       - |  5319 | `		 * call as their initialization value.` |
|       - |  5320 | `		 * Example:` |
|       - |  5321 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  5322 | `		 */` |
|       8 |  5323 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5324 | `		/* Swap bytecode container */` |
|       8 |  5325 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       8 |  5326 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5327 | `		/* Compile the expression */` |
|       8 |  5328 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5329 | `		/* Emit the done instruction */` |
|       8 |  5330 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5331 | `		/* Restore default bytecode container */` |
|       8 |  5332 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       3 |  5333 | `	}` |
|       - |  5334 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       8 |  5335 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       8 |  5336 | `	return SXRET_OK;` |
|       1 |  5337 | `Synchronize:` |
|       - |  5338 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5339 | `	 * statement.` |
|       - |  5340 | `	 */` |
|       5 |  5341 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5342 | `		pGen->pIn++;` |
|       1 |  5343 | `	}` |
|       3 |  5344 | `	return SXRET_OK;` |
|       7 |  5345 | `}` |
|       - |  5346 | `/*` |
|       - |  5347 | ` * Compile the var statement.` |
|       - |  5348 | ` * Symisc Extension:` |
|       - |  5349 | ` *      var statement can be used outside of a class definition.` |
|       - |  5350 | ` */` |
|       4 |  5351 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5352 | `{` |
|       - |  5353 | `	sxu32 nLine;` |
|       - |  5354 | `	sxi32 rc;` |
|       5 |  5355 | `	nLine = pGen->pIn->nLine;` |
|       - |  5356 | `	/* Jump the 'var' keyword */` |
|       5 |  5357 | `	pGen->pIn++;` |
|       5 |  5358 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5359 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5360 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5361 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5362 | `			pGen->pIn++;` |
|     ! 0 |  5363 | `		}` |
|     ! 0 |  5364 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5365 | `			return SXERR_ABORT;` |
|       - |  5366 | `		}` |
|     ! 0 |  5367 | `	}else{` |
|       - |  5368 | `		/* Compile the expression */` |
|       5 |  5369 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5370 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5371 | `			return SXERR_ABORT;` |
|       5 |  5372 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5373 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5374 | `		}` |
|       - |  5375 | `	}` |
|       5 |  5376 | `	return SXRET_OK;` |
|       3 |  5377 | `}` |
|       - |  5378 | `/*` |
|       - |  5379 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5380 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5381 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5382 | ` */` |
|       - |  5383 | `/*` |
|       - |  5384 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5385 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5386 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5387 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5388 | ` *` |
|       - |  5389 | ` * Resolution order:` |
|       - |  5390 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5391 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5392 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5393 | ` *` |
|       - |  5394 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5395 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5396 | ` * Returns the (possibly new) literal index.` |
|       - |  5397 | ` */` |
|  553910 |  5398 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5399 | `{` |
|       - |  5400 | `	ph7_value *pLit;` |
|       - |  5401 | `	const char *zLit;` |
|       - |  5402 | `	SyString sQualified;` |
|       - |  5403 | `	sxu32 nLit;` |
|       - |  5404 | `	sxu32 k;` |
|       - |  5405 | `	sxu32 nNewIdx;` |
|       - |  5406 | `	int hasNsSep;` |
|       - |  5407 | `	SyHashEntry *pImport;` |
|       - |  5408 | `	ph7_value *pNew;` |
|  553915 |  5409 | `	if( pFromImport ){` |
|  509473 |  5410 | `		*pFromImport = 0;` |
|  254734 |  5411 | `	}` |
|  553915 |  5412 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  553915 |  5413 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5414 | `		return nOrigIdx;` |
|       - |  5415 | `	}` |
|  553915 |  5416 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  553915 |  5417 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5418 | `	/* Skip if already qualified (contains backslash) */` |
|  553915 |  5419 | `	hasNsSep = 0;` |
| 6292959 |  5420 | `	for( k = 0; k < nLit; k++ ){` |
| 5739057 |  5421 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2869527 |  5422 | `	}` |
|  553915 |  5423 | `	if( hasNsSep ){` |
|      10 |  5424 | `		return nOrigIdx;` |
|       - |  5425 | `	}` |
|       - |  5426 | `	/* Check use imports first (works even outside namespaces) */` |
|  553907 |  5427 | `	SyBlobReset(&pGen->sWorker);` |
|  553907 |  5428 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  553907 |  5429 | `	if( pImport ){` |
|      41 |  5430 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5431 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5432 | `		if( pFromImport ){` |
|      18 |  5433 | `			*pFromImport = 1;` |
|       8 |  5434 | `		}` |
|      23 |  5435 | `	}else{` |
|  553871 |  5436 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  553781 |  5437 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5438 | `		}` |
|       - |  5439 | `		/* Prepend current namespace */` |
|      95 |  5440 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5441 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5442 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5443 | `	}` |
|       - |  5444 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5445 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5446 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5447 | `		return nNewIdx; /* Already interned */` |
|       - |  5448 | `	}` |
|      79 |  5449 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5450 | `	if( pNew == 0 ){` |
|     ! 0 |  5451 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5452 | `	}` |
|      79 |  5453 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5454 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5455 | `	return nNewIdx;` |
|  276960 |  5456 | `}` |
|       - |  5457 | `/*` |
|       - |  5458 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5459 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5460 | ` */` |
|  104862 |  5461 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5462 | `{` |
|       - |  5463 | `	SyHashEntry *pImport;` |
|       - |  5464 | `	/* Check use imports first */` |
|  104867 |  5465 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|  104867 |  5466 | `	if( pImport ){` |
|      19 |  5467 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      19 |  5468 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      19 |  5469 | `		return;` |
|       - |  5470 | `	}` |
|       - |  5471 | `	/* Prepend current namespace if active */` |
|  104851 |  5472 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5473 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5474 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5475 | `	}` |
|  104851 |  5476 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   52436 |  5477 | `}` |
|       - |  5478 | `/*` |
|       - |  5479 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5480 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5481 | ` * The caller must release pOut when done.` |
|       - |  5482 | ` */` |
|  155242 |  5483 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5484 | `{` |
|  155247 |  5485 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|    3907 |  5486 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|    3907 |  5487 | `		SyBlobAppend(pOut,"\\",1);` |
|    1951 |  5488 | `	}` |
|  155247 |  5489 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  155247 |  5490 | `}` |
|       - |  5491 | `/*` |
|       - |  5492 | ` * Compile a namespace statement` |
|       - |  5493 | ` * According to the PHP language reference manual` |
|       - |  5494 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5495 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5496 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5497 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5498 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5499 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5500 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5501 | ` *  programming world.` |
|       - |  5502 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5503 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5504 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5505 | ` *  classes/functions/constants.` |
|       - |  5506 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5507 | ` *  readability of source code.` |
|       - |  5508 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5509 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5510 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5511 | ` *       class MyClass {}` |
|       - |  5512 | ` *       function myfunction() {}` |
|       - |  5513 | ` *       const MYCONST = 1;` |
|       - |  5514 | ` *       $a = new MyClass;` |
|       - |  5515 | ` *       $c = new \my\name\MyClass;` |
|       - |  5516 | ` *       $a = strlen('hi');` |
|       - |  5517 | ` *       $d = namespace\MYCONST;` |
|       - |  5518 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5519 | ` *       echo constant($d);` |
|       - |  5520 | ` * NOTE` |
|       - |  5521 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5522 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5523 | ` */` |
|       - |  5524 | `/*` |
|       - |  5525 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5526 | ` */` |
|      14 |  5527 | `static const char * TokenTypeName(sxu32 nType)` |
|       3 |  5528 | `{` |
|      17 |  5529 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 |  5530 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 |  5531 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 |  5532 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 |  5533 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 |  5534 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5535 | `	return "token";` |
|      10 |  5536 | `}` |
|    3950 |  5537 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5538 | `{` |
|       - |  5539 | `	sxu32 nLine;` |
|       - |  5540 | `	sxi32 rc;` |
|    3955 |  5541 | `	nLine = pGen->pIn->nLine;` |
|    3955 |  5542 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5543 | `	/* Reset namespace and clear previous use imports */` |
|    3955 |  5544 | `	SyBlobReset(&pGen->sNamespace);` |
|    3955 |  5545 | `	SyHashRelease(&pGen->hUseImports);` |
|    3955 |  5546 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|    3955 |  5547 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    3955 |  5548 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|    3955 |  5549 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    3955 |  5550 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|    3955 |  5551 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5552 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5553 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5554 | `		return SXRET_OK;` |
|       - |  5555 | `	}` |
|    3955 |  5556 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5557 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5558 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5559 | `		return SXRET_OK;` |
|       - |  5560 | `	}` |
|    3955 |  5561 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5562 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5563 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5564 | `		return SXRET_OK;` |
|       - |  5565 | `	}` |
|       - |  5566 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|    7947 |  5567 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|    3997 |  5568 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5569 | `			/* Append backslash separator */` |
|      26 |  5570 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      26 |  5571 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5572 | `			}` |
|      15 |  5573 | `		}else{` |
|       - |  5574 | `			/* Append identifier */` |
|    3975 |  5575 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5576 | `		}` |
|    3997 |  5577 | `		pGen->pIn++;` |
|       5 |  5578 | `	}` |
|       - |  5579 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5580 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5581 | `	{` |
|    3955 |  5582 | `		char *zNsDup = 0;` |
|    3955 |  5583 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|    5927 |  5584 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    3948 |  5585 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|    1974 |  5586 | `		}` |
|    3955 |  5587 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5588 | `	}` |
|    3955 |  5589 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5590 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5591 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5592 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5593 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5594 | `			return SXERR_ABORT;` |
|       - |  5595 | `		}` |
|       2 |  5596 | `	}` |
|    3955 |  5597 | `	return SXRET_OK;` |
|    1980 |  5598 | `}` |
|       - |  5599 | `/*` |
|       - |  5600 | ` * Compile the 'use' statement` |
|       - |  5601 | ` * According to the PHP language reference manual` |
|       - |  5602 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5603 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5604 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5605 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5606 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5607 | ` *  a function or constant is not supported.` |
|       - |  5608 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5609 | ` * NOTE` |
|       - |  5610 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5611 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5612 | ` */` |
|      72 |  5613 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5614 | `{` |
|       - |  5615 | `	sxu32 nLine;` |
|       - |  5616 | `	sxi32 rc;` |
|       - |  5617 | `	SyBlob sPath;` |
|       - |  5618 | `	SyString sAlias;` |
|       - |  5619 | `	SyToken *pLast;` |
|       - |  5620 | `	char *zDup;` |
|       - |  5621 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5622 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5623 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      77 |  5624 | `	nLine = pGen->pIn->nLine;` |
|      77 |  5625 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5626 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      77 |  5627 | `	iUseType = 0;` |
|      77 |  5628 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5629 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5630 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5631 | `			iUseType = 1;` |
|      16 |  5632 | `			pGen->pIn++;` |
|      23 |  5633 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5634 | `			iUseType = 2;` |
|      16 |  5635 | `			pGen->pIn++;` |
|       7 |  5636 | `		}` |
|      14 |  5637 | `	}` |
|       - |  5638 | `	/* Select target hash tables based on import type */` |
|      77 |  5639 | `	switch( iUseType ){` |
|       7 |  5640 | `		case 1:` |
|      16 |  5641 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5642 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5643 | `			break;` |
|       7 |  5644 | `		case 2:` |
|      16 |  5645 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5646 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5647 | `			break;` |
|      22 |  5648 | `		default:` |
|      49 |  5649 | `			pGenHash = &pGen->hUseImports;` |
|      49 |  5650 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      44 |  5651 | `			break;` |
|       - |  5652 | `	}` |
|      77 |  5653 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5654 | `	/* Process one or more use declarations separated by commas */` |
|      37 |  5655 | `	for(;;){` |
|      79 |  5656 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5657 | `			break;` |
|       - |  5658 | `		}` |
|      79 |  5659 | `		SyBlobReset(&sPath);` |
|      79 |  5660 | `		pLast = 0;` |
|       - |  5661 | `		/* Collect the full namespace path */` |
|     269 |  5662 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     195 |  5663 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     135 |  5664 | `				pLast = pGen->pIn;` |
|     135 |  5665 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5666 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5667 | `				}` |
|     135 |  5668 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      65 |  5669 | `			}` |
|     195 |  5670 | `			pGen->pIn++;` |
|       5 |  5671 | `		}` |
|      79 |  5672 | `		if( pLast == 0 ){` |
|       - |  5673 | `			/* Empty path */` |
|       6 |  5674 | `			break;` |
|       - |  5675 | `		}` |
|       - |  5676 | `		/* Default alias is the last component of the path */` |
|      75 |  5677 | `		sAlias = pLast->sData;` |
|       - |  5678 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      70 |  5679 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      50 |  5680 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      24 |  5681 | `			pGen->pIn++; /* Jump 'as' */` |
|      24 |  5682 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      24 |  5683 | `				sAlias = pGen->pIn->sData;` |
|      24 |  5684 | `				pGen->pIn++;` |
|      10 |  5685 | `			}` |
|      10 |  5686 | `		}` |
|       - |  5687 | `		/* Check for duplicate import alias (per-type) */` |
|      75 |  5688 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5689 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5690 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5691 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5692 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5693 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5694 | `				return SXERR_ABORT;` |
|       - |  5695 | `			}` |
|       2 |  5696 | `		}` |
|       - |  5697 | `		/* Register the import: alias -> FQN.` |
|       - |  5698 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5699 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5700 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     110 |  5701 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      70 |  5702 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      75 |  5703 | `		if( zDup ){` |
|      75 |  5704 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      75 |  5705 | `			if( pVmHash ){` |
|       - |  5706 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5707 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      47 |  5708 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      47 |  5709 | `				if( zAliasDup ){` |
|      47 |  5710 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      21 |  5711 | `				}` |
|      21 |  5712 | `			}` |
|      75 |  5713 | `			if( iUseType == 2 ){` |
|       - |  5714 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5715 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5716 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5717 | `				if( zAliasDup ){` |
|       - |  5718 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5719 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5720 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5721 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5722 | `					if( azPair ){` |
|      16 |  5723 | `						azPair[0] = zAliasDup;` |
|      16 |  5724 | `						azPair[1] = zDup;` |
|      16 |  5725 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5726 | `					}` |
|       7 |  5727 | `				}` |
|       7 |  5728 | `			}` |
|      35 |  5729 | `		}` |
|       - |  5730 | `		/* Check for comma (multiple use declarations) */` |
|      75 |  5731 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5732 | `			pGen->pIn++;` |
|       2 |  5733 | `		}else{` |
|      39 |  5734 | `			break;` |
|       - |  5735 | `		}` |
|       1 |  5736 | `	}` |
|      77 |  5737 | `	SyBlobRelease(&sPath);` |
|      77 |  5738 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5739 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5740 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5741 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5742 | `			return SXERR_ABORT;` |
|       - |  5743 | `		}` |
|       1 |  5744 | `	}` |
|      77 |  5745 | `	return SXRET_OK;` |
|      41 |  5746 | `}` |
|       - |  5747 | `/*` |
|       - |  5748 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5749 | ` *` |
|       - |  5750 | ` * According to the PHP language reference manual.` |
|       - |  5751 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5752 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5753 | ` *  declare (directive)` |
|       - |  5754 | ` *   statement` |
|       - |  5755 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5756 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5757 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5758 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5759 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5760 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5761 | ` * <?php` |
|       - |  5762 | ` * // these are the same:` |
|       - |  5763 | ` * // you can use this:` |
|       - |  5764 | ` * declare(ticks=1) {` |
|       - |  5765 | ` *   // entire script here` |
|       - |  5766 | ` * }` |
|       - |  5767 | ` * // or you can use this:` |
|       - |  5768 | ` * declare(ticks=1);` |
|       - |  5769 | ` * // entire script here` |
|       - |  5770 | ` * ?>` |
|       - |  5771 | ` *` |
|       - |  5772 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5773 | ` */` |
|       - |  5774 | `/*` |
|       - |  5775 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5776 | ` */` |
|      72 |  5777 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5778 | `{` |
|     109 |  5779 | `	return SyStringLength(pName) == nWant` |
|      72 |  5780 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5781 | `}` |
|       - |  5782 |  |
|      42 |  5783 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5784 | `{` |
|      47 |  5785 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      47 |  5786 | `	SyToken *pBodyEnd = 0;` |
|       - |  5787 | `	SyToken *pBodyStart;` |
|       - |  5788 | `	SyToken *pCursor;` |
|       - |  5789 | `	int bHasStrictTypes;` |
|       - |  5790 | `	int bBlockForm;` |
|       - |  5791 | `	int bPlacementOk;` |
|       - |  5792 | `	sxi32 rc;` |
|      47 |  5793 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      47 |  5794 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5795 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5796 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5797 | `			return SXERR_ABORT;` |
|       - |  5798 | `		}` |
|       6 |  5799 | `		goto Synchro;` |
|       - |  5800 | `	}` |
|      43 |  5801 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      43 |  5802 | `	pBodyStart = pGen->pIn;` |
|       - |  5803 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      43 |  5804 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      43 |  5805 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5806 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5807 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5808 | `			return SXERR_ABORT;` |
|       - |  5809 | `		}` |
|     ! 0 |  5810 | `		return SXRET_OK;` |
|       - |  5811 | `	}` |
|       - |  5812 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5813 | `	 * now delimits the comma-separated directive list. */` |
|      43 |  5814 | `	pGen->pIn = &pBodyEnd[1];` |
|      43 |  5815 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5816 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5817 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5818 | `			return SXERR_ABORT;` |
|       - |  5819 | `		}` |
|     ! 0 |  5820 | `	}` |
|      43 |  5821 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      43 |  5822 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      43 |  5823 | `	bHasStrictTypes = 0;` |
|       - |  5824 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5825 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5826 | `	 * directive appears anywhere in the list, before validating values. */` |
|      43 |  5827 | `	pCursor = pBodyStart;` |
|      55 |  5828 | `	while( pCursor < pBodyEnd ){` |
|      51 |  5829 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      43 |  5830 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      39 |  5831 | `				bHasStrictTypes = 1;` |
|      39 |  5832 | `				break;` |
|       - |  5833 | `			}` |
|       2 |  5834 | `		}` |
|      14 |  5835 | `		pCursor++;` |
|       2 |  5836 | `	}` |
|      43 |  5837 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5838 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5839 | `			"strict_types declaration must not use block mode");` |
|       3 |  5840 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5841 | `		return SXRET_OK;` |
|       - |  5842 | `	}` |
|      41 |  5843 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5844 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5845 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5846 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5847 | `		return SXRET_OK;` |
|       - |  5848 | `	}` |
|       - |  5849 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      37 |  5850 | `	pCursor = pBodyStart;` |
|      69 |  5851 | `	while( pCursor < pBodyEnd ){` |
|       - |  5852 | `		SyToken *pNameTok;` |
|       - |  5853 | `		SyToken *pEqTok;` |
|       - |  5854 | `		SyToken *pValTok;` |
|       - |  5855 | `		SyString *pDirName;` |
|       - |  5856 | `		int bIsStrict;` |
|       - |  5857 | `		int iStrictValue;` |
|      39 |  5858 | `		pNameTok = pCursor;` |
|      39 |  5859 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5860 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5861 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5862 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5863 | `			return SXRET_OK;` |
|       - |  5864 | `		}` |
|      39 |  5865 | `		pEqTok = pNameTok + 1;` |
|      39 |  5866 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5867 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5868 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5869 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5870 | `			return SXRET_OK;` |
|       - |  5871 | `		}` |
|      39 |  5872 | `		pValTok = pEqTok + 1;` |
|      39 |  5873 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5874 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5875 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5876 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5877 | `			return SXRET_OK;` |
|       - |  5878 | `		}` |
|      39 |  5879 | `		pDirName = &pNameTok->sData;` |
|      39 |  5880 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      39 |  5881 | `		if( bIsStrict ){` |
|       - |  5882 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5883 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      35 |  5884 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5885 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5886 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5887 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5888 | `				return SXRET_OK;` |
|       - |  5889 | `			}` |
|      35 |  5890 | `			iStrictValue = -1;` |
|      35 |  5891 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      35 |  5892 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      35 |  5893 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      35 |  5894 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      33 |  5895 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      15 |  5896 | `			}` |
|      35 |  5897 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5898 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5899 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5900 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5901 | `				return SXRET_OK;` |
|       - |  5902 | `			}` |
|      32 |  5903 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      18 |  5904 | `		}else{` |
|       - |  5905 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5906 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5907 | `			 * behavior don't regress. */` |
|       8 |  5908 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5909 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5910 | `				ph7_lib_version()` |
|       - |  5911 | `				);` |
|       - |  5912 | `		}` |
|      36 |  5913 | `		pCursor = pValTok + 1;` |
|       - |  5914 | `		/* Consume separating comma (or end). */` |
|      36 |  5915 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5916 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5917 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5918 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5919 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5920 | `				return SXRET_OK;` |
|       - |  5921 | `			}` |
|       3 |  5922 | `			pCursor++;` |
|       1 |  5923 | `		}` |
|       4 |  5924 | `	}` |
|       - |  5925 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5926 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5927 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      34 |  5928 | `	return SXRET_OK;` |
|       2 |  5929 | `Synchro:` |
|       - |  5930 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5931 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5932 | `		pGen->pIn++;` |
|       2 |  5933 | `	}` |
|       6 |  5934 | `	return SXRET_OK;` |
|      26 |  5935 | `}` |
|       - |  5936 | `/*` |
|       - |  5937 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5938 | ` * as follows:` |
|       - |  5939 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5940 | ` * {` |
|       - |  5941 | ` *   return "Making a cup of $type.\n";` |
|       - |  5942 | ` * }` |
|       - |  5943 | ` * Symisc eXtension.` |
|       - |  5944 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5945 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5946 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5947 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5948 | ` *      {` |
|       - |  5949 | ` *       var_dump($a);` |
|       - |  5950 | ` *      }` |
|       - |  5951 | ` *     //call test without args` |
|       - |  5952 | ` *      test();` |
|       - |  5953 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5954 | ` *      Example:` |
|       - |  5955 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5956 | ` * 3 -) Function overloading!!` |
|       - |  5957 | ` *      Example:` |
|       - |  5958 | ` *      function foo($a) {` |
|       - |  5959 | ` *   	  return $a.PHP_EOL;` |
|       - |  5960 | ` *	    }` |
|       - |  5961 | ` *	    function foo($a, $b) {` |
|       - |  5962 | ` *   	  return $a + $b;` |
|       - |  5963 | ` *	    }` |
|       - |  5964 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5965 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5966 | ` *      // Same arg` |
|       - |  5967 | ` *	   function foo(string $a)` |
|       - |  5968 | ` *	   {` |
|       - |  5969 | ` *	     echo "a is a string\n";` |
|       - |  5970 | ` *	     var_dump($a);` |
|       - |  5971 | ` *	   }` |
|       - |  5972 | ` *	  function foo(int $a)` |
|       - |  5973 | ` *	  {` |
|       - |  5974 | ` *	    echo "a is integer\n";` |
|       - |  5975 | ` *	    var_dump($a);` |
|       - |  5976 | ` *	  }` |
|       - |  5977 | ` *	  function foo(array $a)` |
|       - |  5978 | ` *	  {` |
|       - |  5979 | ` * 	    echo "a is an array\n";` |
|       - |  5980 | ` * 	    var_dump($a);` |
|       - |  5981 | ` *	  }` |
|       - |  5982 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5983 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5984 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5985 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5986 | ` * introduced by the PH7 engine.` |
|       - |  5987 | ` */` |
|   80798 |  5988 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5989 | `{` |
|       - |  5990 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5991 | `	SySet *pInstrContainer;` |
|       - |  5992 | `	sxi32 rc;` |
|       - |  5993 | `	/* Swap token stream */` |
|   80803 |  5994 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   80803 |  5995 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   80803 |  5996 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5997 | `	/* Compile the expression holding the argument value */` |
|   80803 |  5998 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5999 | `	/* Emit the done instruction */` |
|   80803 |  6000 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   80803 |  6001 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   80803 |  6002 | `	RE_SWAP_DELIMITER(pGen);` |
|   80803 |  6003 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6004 | `		return SXERR_ABORT;` |
|       - |  6005 | `	}` |
|   80803 |  6006 | `	return SXRET_OK;` |
|   40404 |  6007 | `}` |
|       - |  6008 | `/*` |
|       - |  6009 | ` * Collect function arguments one after one.` |
|       - |  6010 | ` * According to the PHP language reference manual.` |
|       - |  6011 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  6012 | ` * list of expressions.` |
|       - |  6013 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  6014 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  6015 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  6016 | ` * for more information.` |
|       - |  6017 | ` * Example #1 Passing arrays to functions` |
|       - |  6018 | ` * <?php` |
|       - |  6019 | ` * function takes_array($input)` |
|       - |  6020 | ` * {` |
|       - |  6021 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  6022 | ` * }` |
|       - |  6023 | ` * ?>` |
|       - |  6024 | ` * Making arguments be passed by reference` |
|       - |  6025 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  6026 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  6027 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  6028 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  6029 | ` * to the argument name in the function definition:` |
|       - |  6030 | ` * Example #2 Passing function parameters by reference` |
|       - |  6031 | ` * <?php` |
|       - |  6032 | ` * function add_some_extra(&$string)` |
|       - |  6033 | ` * {` |
|       - |  6034 | ` *   $string .= 'and something extra.';` |
|       - |  6035 | ` * }` |
|       - |  6036 | ` * $str = 'This is a string, ';` |
|       - |  6037 | ` * add_some_extra($str);` |
|       - |  6038 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  6039 | ` * ?>` |
|       - |  6040 | ` *` |
|       - |  6041 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  6042 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  6043 | ` * on these extension.` |
|       - |  6044 | ` */` |
|  116864 |  6045 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  6046 | `{` |
|       - |  6047 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  6048 | `	SyToken *pIn;  /* Token stream */` |
|       - |  6049 | `	SyBlob sSig;         /* Function signature */` |
|       - |  6050 | `	char *zDup;          /* Copy of argument name */` |
|       - |  6051 | `	sxi32 rc;` |
|       - |  6052 |  |
|  116869 |  6053 | `	pIn = pGen->pIn;` |
|  116869 |  6054 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  6055 | `	/* Process arguments one after one */` |
|  149929 |  6056 | `	for(;;){` |
|  299863 |  6057 | `		if( pIn >= pEnd ){` |
|       - |  6058 | `			/* No more arguments to process */` |
|  116853 |  6059 | `			break;` |
|       - |  6060 | `		}` |
|  183015 |  6061 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  183015 |  6062 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  183015 |  6063 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  183015 |  6064 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  6065 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  6066 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  6067 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  6068 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  6069 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  6070 | `		{` |
|  183015 |  6071 | `			int bReadonly = 0, bVisSeen = 0;` |
|  183015 |  6072 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  183015 |  6073 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  6074 | `				bReadonly = 1;` |
|       3 |  6075 | `				pIn++;` |
|       1 |  6076 | `			}` |
|  183015 |  6077 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   69507 |  6078 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   69507 |  6079 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      79 |  6080 | `					bVisSeen = 1;` |
|      79 |  6081 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|     105 |  6082 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      34 |  6083 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      79 |  6084 | `					pIn++;` |
|      79 |  6085 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      18 |  6086 | `						bReadonly = 1;` |
|      18 |  6087 | `						pIn++;` |
|       7 |  6088 | `					}` |
|      37 |  6089 | `				}` |
|   34751 |  6090 | `			}` |
|  183015 |  6091 | `			if( bVisSeen \|\| bReadonly ){` |
|      81 |  6092 | `				if( !bCtorCtx ){` |
|       6 |  6093 | `					if( bAbstractCtx ){` |
|       3 |  6094 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  6095 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  6096 | `					}else{` |
|       3 |  6097 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  6098 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  6099 | `					}` |
|       6 |  6100 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6101 | `						return SXERR_ABORT;` |
|       - |  6102 | `					}` |
|       6 |  6103 | `					return SXERR_SYNTAX;` |
|       - |  6104 | `				}` |
|      77 |  6105 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      77 |  6106 | `				sArg.iPromoteVis = iVis;` |
|      77 |  6107 | `				if( bReadonly ){` |
|      20 |  6108 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       8 |  6109 | `				}` |
|      36 |  6110 | `			}` |
|       - |  6111 | `		}` |
|       - |  6112 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  183006 |  6113 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  137863 |  6114 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   90789 |  6115 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   84977 |  6116 | `			sxu32 nLineLocal = pIn->nLine;` |
|   84977 |  6117 | `			sxi32 iTFlags = 0;` |
|   84977 |  6118 | `			pGen->pIn = pIn;` |
|   84977 |  6119 | `			rc = GenStateParseUnionTypeDecl(` |
|   42486 |  6120 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   42486 |  6121 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  6122 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  6123 | `				/* bAllowVoid */ 0,` |
|   42486 |  6124 | `						nLineLocal);` |
|   84977 |  6125 | `			pIn = pGen->pIn;` |
|   84977 |  6126 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6127 | `				return SXERR_ABORT;` |
|   84977 |  6128 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  6129 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  6130 | `				return SXERR_SYNTAX;` |
|   84975 |  6131 | `			}else if( rc == SXERR_SYNTAX ){` |
|      11 |  6132 | `				if( pIn < pEnd ){` |
|      15 |  6133 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  6134 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       4 |  6135 | `						&pIn->sData);` |
|       7 |  6136 | `				}else{` |
|     ! 0 |  6137 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  6138 | `						"syntax error, unexpected end of file");` |
|       - |  6139 | `				}` |
|      11 |  6140 | `				return SXERR_SYNTAX;` |
|       - |  6141 | `			}` |
|   84967 |  6142 | `			sArg.iFlags \|= iTFlags;` |
|   42481 |  6143 | `		}` |
|  183001 |  6144 | `		if( pIn >= pEnd ){` |
|     ! 0 |  6145 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  6146 | `			return rc;` |
|       - |  6147 | `		}` |
|  183001 |  6148 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  6149 | `			/* Pass by reference,record that */` |
|    3881 |  6150 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3881 |  6151 | `			pIn++;` |
|    1938 |  6152 | `		}` |
|  183001 |  6153 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  6154 | `			/* Variadic parameter: ...$args */` |
|    3901 |  6155 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    3901 |  6156 | `			pIn++;` |
|    1948 |  6157 | `		}` |
|  183001 |  6158 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6159 | `			/* Invalid argument */` |
|     ! 0 |  6160 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  6161 | `			return rc;` |
|       - |  6162 | `		}` |
|  183001 |  6163 | `		pIn++; /* Jump the dollar sign */` |
|       - |  6164 | `		/* Copy argument name */` |
|  183001 |  6165 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  183001 |  6166 | `		if( zDup == 0 ){` |
|     ! 0 |  6167 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  6168 | `			return SXERR_ABORT;` |
|       - |  6169 | `		}` |
|  183001 |  6170 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  183001 |  6171 | `		pIn++;` |
|  183001 |  6172 | `		if( pIn < pEnd ){` |
|  108501 |  6173 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  6174 | `				SyToken *pDefend;` |
|   80805 |  6175 | `				sxi32 iNest = 0;` |
|   80805 |  6176 | `				pIn++; /* Jump the equal sign */` |
|   80805 |  6177 | `				pDefend = pIn;` |
|       - |  6178 | `				/* Process the default value associated with this argument */` |
|  169305 |  6179 | `				while( pDefend < pEnd ){` |
|  126961 |  6180 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   38461 |  6181 | `						break;` |
|       - |  6182 | `					}` |
|   88505 |  6183 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  6184 | `						/* Increment nesting level */` |
|    3855 |  6185 | `						iNest++;` |
|   86580 |  6186 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  6187 | `						/* Decrement nesting level */` |
|    3855 |  6188 | `						iNest--;` |
|    1925 |  6189 | `					}` |
|   88505 |  6190 | `					pDefend++;` |
|       5 |  6191 | `				}` |
|   80805 |  6192 | `				if( pIn >= pDefend ){` |
|       3 |  6193 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  6194 | `					return rc;` |
|       - |  6195 | `				}` |
|       - |  6196 | `				/* Process default value */` |
|   80803 |  6197 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   80803 |  6198 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  6199 | `					return rc;` |
|       - |  6200 | `				}` |
|       - |  6201 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|       - |  6202 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|       - |  6203 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|       - |  6204 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|       - |  6205 | `				 * arg-type check lets null through. */` |
|   80798 |  6206 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|   63476 |  6207 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|   63475 |  6208 | `					&& &pIn[1] == pDefend` |
|   44230 |  6209 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|   34611 |  6210 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|   21153 |  6211 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|   15387 |  6212 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|    7691 |  6213 | `				}` |
|       - |  6214 | `				/* Point beyond the default value */` |
|   80803 |  6215 | `				pIn = pDefend;` |
|   40399 |  6216 | `			}` |
|  108499 |  6217 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  6218 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  6219 | `				return rc;` |
|       - |  6220 | `			}` |
|  108499 |  6221 | `			pIn++; /* Jump the trailing comma */` |
|   54247 |  6222 | `		}` |
|       - |  6223 | `		/* Append argument signature */` |
|  182999 |  6224 | `		if( sArg.nType > 0 ){` |
|   84911 |  6225 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  6226 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   15449 |  6227 | `				int marker = 'o';` |
|   15449 |  6228 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   15449 |  6229 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7727 |  6230 | `			}else{` |
|       - |  6231 | `				int c;` |
|   69467 |  6232 | `				c = 'n'; /* cc warning */` |
|       - |  6233 | `				/* Type leading character */` |
|   69467 |  6234 | `				switch(sArg.nType){` |
|       4 |  6235 | `				case MEMOBJ_HASHMAP:` |
|       - |  6236 | `					/* Hashmap aka 'array' */` |
|       9 |  6237 | `					c = 'h';` |
|       9 |  6238 | `					break;` |
|    9700 |  6239 | `				case MEMOBJ_INT:` |
|       - |  6240 | `					/* Integer */` |
|   19405 |  6241 | `					c = 'i';` |
|   19405 |  6242 | `					break;` |
|       2 |  6243 | `				case MEMOBJ_BOOL:` |
|       - |  6244 | `					/* Bool */` |
|       5 |  6245 | `					c = 'b';` |
|       5 |  6246 | `					break;` |
|       5 |  6247 | `				case MEMOBJ_REAL:` |
|       - |  6248 | `					/* Float */` |
|      12 |  6249 | `					c = 'f';` |
|      12 |  6250 | `					break;` |
|   25012 |  6251 | `				case MEMOBJ_STRING:` |
|       - |  6252 | `					/* String */` |
|   50029 |  6253 | `					c = 's';` |
|   50029 |  6254 | `					break;` |
|       7 |  6255 | `				case MEMOBJ_OBJ:` |
|       - |  6256 | `					/* Object */` |
|      16 |  6257 | `					c = 'o';` |
|      14 |  6258 | `					break;` |
|       1 |  6259 | `				default:` |
|       2 |  6260 | `					break;` |
|       - |  6261 | `				}` |
|   69467 |  6262 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  6263 | `			}` |
|   42458 |  6264 | `		}else{` |
|       - |  6265 | `			/* No type is associated with this parameter which mean` |
|       - |  6266 | `			 * that this function is not condidate for overloading.` |
|       - |  6267 | `			 */` |
|   98093 |  6268 | `			SyBlobRelease(&sSig);` |
|       - |  6269 | `		}` |
|       - |  6270 | `		/* Save in the argument set */` |
|  182999 |  6271 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  6272 | `	}` |
|  116853 |  6273 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  6274 | `		/* Save function signature */` |
|   54105 |  6275 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   27050 |  6276 | `	}` |
|  116853 |  6277 | `	return SXRET_OK;` |
|   58437 |  6278 | `}` |
|       - |  6279 | `/*` |
|       - |  6280 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|       - |  6281 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|       - |  6282 | ` * the enclosing function. Returns the token just past the nested construct.` |
|       - |  6283 | ` */` |
|      20 |  6284 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|       3 |  6285 | `{` |
|      23 |  6286 | `	sxi32 iParen = 0;` |
|      23 |  6287 | `	pIn++; /* past 'function'/'fn' */` |
|       - |  6288 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|       - |  6289 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|       - |  6290 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|      83 |  6291 | `	while( pIn < pEnd ){` |
|      83 |  6292 | `		sxu32 t = pIn->nType;` |
|      83 |  6293 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|      63 |  6294 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|      43 |  6295 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|      23 |  6296 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|      63 |  6297 | `		pIn++;` |
|       3 |  6298 | `	}` |
|      23 |  6299 | `	if( pIn >= pEnd ){ return pIn; }` |
|       - |  6300 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|       - |  6301 | `	{` |
|      23 |  6302 | `		sxi32 d = 0;` |
|     211 |  6303 | `		while( pIn < pEnd ){` |
|     211 |  6304 | `			sxu32 t = pIn->nType;` |
|     211 |  6305 | `			if( t & PH7_TK_OCB ){ d++; }` |
|     187 |  6306 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|     191 |  6307 | `			pIn++;` |
|       3 |  6308 | `		}` |
|       - |  6309 | `	}` |
|      23 |  6310 | `	return pIn;` |
|      13 |  6311 | `}` |
|       - |  6312 | `/*` |
|       - |  6313 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|       - |  6314 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|       - |  6315 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|       - |  6316 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|       - |  6317 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|       - |  6318 | ` * detached-mini-program path untouched.` |
|       - |  6319 | ` */` |
|       - |  6320 | `/*` |
|       - |  6321 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|       - |  6322 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|       - |  6323 | ` * mixed, object.` |
|       - |  6324 | ` */` |
|      28 |  6325 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|       3 |  6326 | `{` |
|       - |  6327 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|       - |  6328 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|       - |  6329 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|       - |  6330 | `	};` |
|       - |  6331 | `	sxu32 i;` |
|      31 |  6332 | `	if( nName > 0 && zName[0] == '\\' ){` |
|     ! 0 |  6333 | `		zName++;` |
|     ! 0 |  6334 | `		nName--;` |
|     ! 0 |  6335 | `	}` |
|      63 |  6336 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|      59 |  6337 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|      27 |  6338 | `			return 1;` |
|       - |  6339 | `		}` |
|      17 |  6340 | `	}` |
|       5 |  6341 | `	return 0;` |
|      17 |  6342 | `}` |
|       - |  6343 | `/*` |
|       - |  6344 | ` * One atom of a generator's declared return type: is it a supertype of` |
|       - |  6345 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|       - |  6346 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|       - |  6347 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|       - |  6348 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|       - |  6349 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|       - |  6350 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|       - |  6351 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|       - |  6352 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|       - |  6353 | ` * both rather than fatal on valid code (divergence recorded in PLAN.md).` |
|       - |  6354 | ` */` |
|      26 |  6355 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|       4 |  6356 | `{` |
|      30 |  6357 | `	if( nType == MEMOBJ_OBJ ){` |
|     ! 0 |  6358 | ``		return 1; /* bare `object` */`` |
|       - |  6359 | `	}` |
|      30 |  6360 | `	if( nType != SXU32_HIGH ){` |
|       3 |  6361 | `		return 0; /* scalar/array/void/never/null/... */` |
|       - |  6362 | `	}` |
|      27 |  6363 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|      23 |  6364 | `		return 1;` |
|       - |  6365 | `	}` |
|       - |  6366 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|       - |  6367 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|       - |  6368 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|       - |  6369 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|       - |  6370 | `	{` |
|       - |  6371 | `		SyBlob sFQN;` |
|       - |  6372 | `		int bOk;` |
|       5 |  6373 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       5 |  6374 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|       5 |  6375 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       5 |  6376 | `		SyBlobRelease(&sFQN);` |
|       5 |  6377 | `		return bOk;` |
|       - |  6378 | `	}` |
|      17 |  6379 | `}` |
|       - |  6380 | `/*` |
|       - |  6381 | ` * php 8: a generator function may only declare a return type that is a` |
|       - |  6382 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|       - |  6383 | ` * group qualifies only if every member does. Anything else is php's exact` |
|       - |  6384 | ` * compile-time fatal "Generator return type must be a supertype of` |
|       - |  6385 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|       - |  6386 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|       - |  6387 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|       - |  6388 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|       - |  6389 | ` */` |
|     212 |  6390 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|       5 |  6391 | `{` |
|     217 |  6392 | `	int bOk = 0;` |
|       - |  6393 | `	sxu32 nLine;` |
|       - |  6394 | `	sxi32 rc;` |
|     217 |  6395 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|     191 |  6396 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|       - |  6397 | `	}` |
|      30 |  6398 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|     ! 0 |  6399 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|     ! 0 |  6400 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|       - |  6401 | `		sxu32 i,j;` |
|     ! 0 |  6402 | `		for( i = 0; i < n && !bOk; i++ ){` |
|       - |  6403 | `			int bGroupOk;` |
|     ! 0 |  6404 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|     ! 0 |  6405 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|       - |  6406 | `			}` |
|     ! 0 |  6407 | `			bGroupOk = 1;` |
|     ! 0 |  6408 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|     ! 0 |  6409 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|     ! 0 |  6410 | `					bGroupOk = 0;` |
|     ! 0 |  6411 | `					break;` |
|       - |  6412 | `				}` |
|     ! 0 |  6413 | `			}` |
|     ! 0 |  6414 | `			bOk = bGroupOk;` |
|     ! 0 |  6415 | `		}` |
|     ! 0 |  6416 | `	}else{` |
|      30 |  6417 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|       - |  6418 | `	}` |
|      30 |  6419 | `	if( bOk ){` |
|      27 |  6420 | `		return SXRET_OK;` |
|       - |  6421 | `	}` |
|       - |  6422 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|       - |  6423 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|       - |  6424 | `	 * token of this stream — its line is the function's closing brace. php` |
|       - |  6425 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|       - |  6426 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|       3 |  6427 | `	nLine = pGen->pIn[-1].nLine;` |
|       - |  6428 | `	{` |
|       3 |  6429 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|       3 |  6430 | `		if( sGiven.nByte < 1 ){` |
|     ! 0 |  6431 | `			sGiven = pFunc->sReturnClass;` |
|     ! 0 |  6432 | `		}` |
|       3 |  6433 | `		if( sGiven.nByte < 1 ){` |
|       - |  6434 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|       - |  6435 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|       - |  6436 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|     ! 0 |  6437 | `			const char *zScalar =` |
|     ! 0 |  6438 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|     ! 0 |  6439 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|     ! 0 |  6440 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|     ! 0 |  6441 | `		}` |
|       3 |  6442 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  6443 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|       - |  6444 | `	}` |
|       3 |  6445 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|     111 |  6446 | `}` |
|  244940 |  6447 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|       5 |  6448 | `{` |
|  244945 |  6449 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  244945 |  6450 | `	SyToken *pEnd = pGen->pEnd;` |
|  244945 |  6451 | `	sxi32 iDepth = 0;` |
|  244945 |  6452 | `	int bStarted = 0;` |
| 8589839 |  6453 | `	while( pIn < pEnd ){` |
| 8589839 |  6454 | `		sxu32 t = pIn->nType;` |
| 8589839 |  6455 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 8086427 |  6456 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 7583331 |  6457 | `		if( t & PH7_TK_KEYWORD ){` |
|  650321 |  6458 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  650321 |  6459 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  650109 |  6460 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|       - |  6461 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  325042 |  6462 | `		}` |
| 7583099 |  6463 | `		pIn++;` |
|       5 |  6464 | `	}` |
|  244733 |  6465 | `	return FALSE;` |
|  122475 |  6466 | `}` |
|       - |  6467 | `/*` |
|       - |  6468 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  6469 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  6470 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  6471 | ` */` |
|  244940 |  6472 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  6473 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  6474 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  6475 | `	)` |
|       5 |  6476 | `{` |
|       - |  6477 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  6478 | `	GenBlock *pBlock;` |
|       - |  6479 | `	sxu32 nGotoOfft;` |
|       - |  6480 | `	sxi32 rc;` |
|       - |  6481 | `	/* Attach the new function */` |
|  244945 |  6482 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  244945 |  6483 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6484 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  6485 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6486 | `		return SXERR_ABORT;` |
|       - |  6487 | `	}` |
|  244945 |  6488 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  6489 | `	/* Swap bytecode containers */` |
|  244945 |  6490 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  244945 |  6491 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  6492 | `	/* Emit constructor property promotion prologue:` |
|       - |  6493 | `	 *   $this->NAME = $NAME;` |
|       - |  6494 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  6495 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  6496 | `	{` |
|  244945 |  6497 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  6498 | `		sxu32 i;` |
|  397037 |  6499 | `		for( i = 0; i < nArg; i++ ){` |
|  152097 |  6500 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  6501 | `			char *zSrc;` |
|       - |  6502 | `			sxu32 nSrc,nName;` |
|       - |  6503 | `			SySet sToken;` |
|       - |  6504 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  6505 | `			sxi32 rcPromote;` |
|  152097 |  6506 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  152035 |  6507 | `				continue;` |
|       - |  6508 | `			}` |
|       - |  6509 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  6510 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  6511 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  6512 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  6513 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      67 |  6514 | `			nName = SyStringLength(&pArg->sName);` |
|      67 |  6515 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      67 |  6516 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      67 |  6517 | `			if( zSrc == 0 ){` |
|     ! 0 |  6518 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6519 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6520 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  6521 | `				return SXERR_ABORT;` |
|       - |  6522 | `			}` |
|       - |  6523 | `			{` |
|      67 |  6524 | `				char *z = zSrc;` |
|      67 |  6525 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      67 |  6526 | `				z += sizeof("$this->")-1;` |
|      67 |  6527 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      67 |  6528 | `				z += nName;` |
|      67 |  6529 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      67 |  6530 | `				z += sizeof(" = $")-1;` |
|      67 |  6531 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      67 |  6532 | `				z += nName;` |
|      67 |  6533 | `				*z = 0;` |
|       - |  6534 | `			}` |
|      67 |  6535 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      67 |  6536 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      67 |  6537 | `			pTmpIn = pGen->pIn;` |
|      67 |  6538 | `			pTmpEnd = pGen->pEnd;` |
|      67 |  6539 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      67 |  6540 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      67 |  6541 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      67 |  6542 | `			pGen->pIn = pTmpIn;` |
|      67 |  6543 | `			pGen->pEnd = pTmpEnd;` |
|      67 |  6544 | `			SySetRelease(&sToken);` |
|      67 |  6545 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6546 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6547 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6548 | `				return SXERR_ABORT;` |
|       - |  6549 | `			}` |
|       - |  6550 | `			/* Discard the assignment result — this is a statement expression. */` |
|      67 |  6551 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      36 |  6552 | `		}` |
|       - |  6553 | `	}` |
|       - |  6554 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|       - |  6555 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|       - |  6556 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|       - |  6557 | `	 * generator — and vice versa — is classified independently. */` |
|       - |  6558 | `	{` |
|  244945 |  6559 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  244945 |  6560 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|       - |  6561 | `		/* Compile the body */` |
|  244945 |  6562 | `		PH7_CompileBlock(&(*pGen),0);` |
|  244945 |  6563 | `		pGen->bInGenerator = bSavedGen;` |
|       - |  6564 | `	}` |
|       - |  6565 | `	/* Fix exception jumps now the destination is resolved */` |
|  244945 |  6566 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6567 | `	/* Emit the final return if not yet done */` |
|  244945 |  6568 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6569 | `	/* Fix gotos jumps now the destination is resolved */` |
|  244945 |  6570 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6571 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6572 | `	}` |
|  244945 |  6573 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6574 | `	/* Restore the default container */` |
|  244945 |  6575 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6576 | `	/* Leave function block */` |
|  244945 |  6577 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  244945 |  6578 | `	if( rc == SXERR_ABORT ){` |
|       - |  6579 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6580 | `		return SXERR_ABORT;` |
|       - |  6581 | `	}` |
|       - |  6582 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6583 | `	{` |
|  244945 |  6584 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6585 | `		sxu32 i;` |
| 5026133 |  6586 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4781405 |  6587 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     217 |  6588 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     217 |  6589 | `				break;` |
|       - |  6590 | `			}` |
| 2390599 |  6591 | `		}` |
|       - |  6592 | `	}` |
|  244945 |  6593 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|       - |  6594 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     217 |  6595 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|     ! 0 |  6596 | `			return SXERR_ABORT;` |
|       - |  6597 | `		}` |
|     106 |  6598 | `	}` |
|       - |  6599 | `	/* All done, function body compiled */` |
|  244945 |  6600 | `	return SXRET_OK;` |
|  122475 |  6601 | `}` |
|       - |  6602 | `/*` |
|       - |  6603 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6604 | ` * According to the PHP language reference manual.` |
|       - |  6605 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6606 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6607 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6608 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6609 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6610 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6611 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6612 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6613 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6614 | ` *` |
|       - |  6615 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6616 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6617 | ` * on these extension.` |
|       - |  6618 | ` */` |
|       - |  6619 | `/*` |
|       - |  6620 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6621 | ` */` |
|     532 |  6622 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6623 | `{` |
|       - |  6624 | `	sxu32 i;` |
|    1507 |  6625 | `	for( i = 0; i < n; i++ ){` |
|    1293 |  6626 | `		int a = zA[i], b = zB[i];` |
|    1293 |  6627 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1293 |  6628 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1293 |  6629 | `		if( a != b ) return a - b;` |
|     490 |  6630 | `	}` |
|     219 |  6631 | `	return 0;` |
|     271 |  6632 | `}` |
|       - |  6633 | `/*` |
|       - |  6634 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6635 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6636 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6637 | ` */` |
|       - |  6638 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6639 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6640 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6641 |  |
|       - |  6642 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6643 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6644 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6645 |  |
|       - |  6646 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6647 | `struct PhlTypeAtom {` |
|       - |  6648 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6649 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6650 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6651 | `	sxu32 nCanon;` |
|       - |  6652 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6653 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6654 | `};` |
|       - |  6655 |  |
|       - |  6656 | `/*` |
|       - |  6657 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6658 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6659 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6660 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6661 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6662 | ` * already be consumed by the caller.` |
|       - |  6663 | ` */` |
|   85940 |  6664 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6665 | `{` |
|   85945 |  6666 | `	SyToken *pIn = pGen->pIn;` |
|   85945 |  6667 | `	SyZero(pOut, sizeof(*pOut));` |
|   85945 |  6668 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   85945 |  6669 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6670 | `		return SXERR_SYNTAX;` |
|       - |  6671 | `	}` |
|       - |  6672 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   85945 |  6673 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6674 | `		pIn++;` |
|       8 |  6675 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6676 | `			return SXERR_SYNTAX;` |
|       - |  6677 | `		}` |
|       3 |  6678 | `	}` |
|   85945 |  6679 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6680 | `		return SXERR_SYNTAX;` |
|       - |  6681 | `	}` |
|   85945 |  6682 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   70075 |  6683 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   70075 |  6684 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      34 |  6685 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   70060 |  6686 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      77 |  6687 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   70009 |  6688 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   19691 |  6689 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   60130 |  6690 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   50209 |  6691 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   25185 |  6692 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      39 |  6693 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      65 |  6694 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6695 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      35 |  6696 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|      12 |  6697 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      21 |  6698 | `			pOut->nType = SXU32_HIGH;` |
|      21 |  6699 | `			pOut->sClass = pIn->sData;` |
|      12 |  6700 | `		}else{` |
|       3 |  6701 | `			return SXERR_SYNTAX;` |
|       - |  6702 | `		}` |
|   70073 |  6703 | `		pIn++;` |
|   35039 |  6704 | `	}else{` |
|       - |  6705 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6706 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   15875 |  6707 | `		SyString *pT = &pIn->sData;` |
|   15875 |  6708 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6709 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6710 | `			pIn++;` |
|   15861 |  6711 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     165 |  6712 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     165 |  6713 | `			pIn++;` |
|   15767 |  6714 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|      24 |  6715 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|      24 |  6716 | `			pIn++;` |
|      14 |  6717 | `		}else{` |
|       - |  6718 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   15667 |  6719 | `			SyToken *pFirst = pIn;` |
|   15667 |  6720 | `			SyToken *pLast = pIn;` |
|   15667 |  6721 | `			pOut->nType = SXU32_HIGH;` |
|   15667 |  6722 | `			pOut->sClass = pIn->sData;` |
|   15667 |  6723 | `			pIn++;` |
|   23496 |  6724 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   15670 |  6725 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6726 | `				pLast = &pIn[1];` |
|       3 |  6727 | `				pIn += 2;` |
|       1 |  6728 | `			}` |
|   15667 |  6729 | `			if( pLast != pFirst ){` |
|       3 |  6730 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6731 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6732 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6733 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6734 | `			}` |
|       - |  6735 | `		}` |
|       - |  6736 | `	}` |
|   85943 |  6737 | `	pGen->pIn = pIn;` |
|   85943 |  6738 | `	return SXRET_OK;` |
|   42975 |  6739 | `}` |
|       - |  6740 |  |
|       - |  6741 | `/*` |
|       - |  6742 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6743 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6744 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6745 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6746 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6747 | ` */` |
|   85774 |  6748 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6749 | `{` |
|       - |  6750 | `	int i;` |
|   85779 |  6751 | `	int nNonNull = 0;` |
|   85779 |  6752 | `	int bAnyIntersection = 0;` |
|       - |  6753 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   85779 |  6754 | `	sxu32 nMaxGroup = 0;` |
| 2830547 |  6755 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  171693 |  6756 | `	for( i = 0; i < nAtoms; i++ ){` |
|   85919 |  6757 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   85891 |  6758 | `			nNonNull++;` |
|   85891 |  6759 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   85891 |  6760 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   85891 |  6761 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   42943 |  6762 | `			}` |
|   42943 |  6763 | `		}` |
|   42962 |  6764 | `	}` |
|  171651 |  6765 | `	for( i = 0; i < nAtoms; i++ ){` |
|   85897 |  6766 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      24 |  6767 | `			bAnyIntersection = 1;` |
|      24 |  6768 | `			break;` |
|       - |  6769 | `		}` |
|   42941 |  6770 | `	}` |
|   85779 |  6771 | `	if( bAnyIntersection ){` |
|       - |  6772 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6773 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6774 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      24 |  6775 | `		sxu32 g, nGroups = 0;` |
|      24 |  6776 | `		int bFirstGroup = 1;` |
|      48 |  6777 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      48 |  6778 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      28 |  6779 | `			int bFirstMember = 1;` |
|       - |  6780 | `			int bWrap;` |
|      28 |  6781 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6782 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6783 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6784 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6785 | `			 * parens, matching PHP's canonical text. */` |
|      38 |  6786 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      28 |  6787 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      28 |  6788 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      84 |  6789 | `			for( i = 0; i < nAtoms; i++ ){` |
|      60 |  6790 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      48 |  6791 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      48 |  6792 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      46 |  6793 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      25 |  6794 | `				}else{` |
|       3 |  6795 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6796 | `				}` |
|      48 |  6797 | `				bFirstMember = 0;` |
|      26 |  6798 | `			}` |
|      28 |  6799 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      28 |  6800 | `			bFirstGroup = 0;` |
|      16 |  6801 | `		}` |
|      24 |  6802 | `		if( bNullable ){` |
|     ! 0 |  6803 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6804 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6805 | `		}` |
|      64 |  6806 | `		return;` |
|       - |  6807 | `	}` |
|   85759 |  6808 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6809 | `		/* Shorthand: ?T */` |
|      84 |  6810 | `		for( i = 0; i < nAtoms; i++ ){` |
|      84 |  6811 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      84 |  6812 | `			SyBlobAppend(pBlob, "?", 1);` |
|      84 |  6813 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      22 |  6814 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      13 |  6815 | `			}else{` |
|      66 |  6816 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6817 | `			}` |
|      84 |  6818 | `			return;` |
|     ! 0 |  6819 | `		}` |
|     ! 0 |  6820 | `	}` |
|       - |  6821 | `	{` |
|   85679 |  6822 | `		int bFirst = 1;` |
|       - |  6823 | `		/* 1) Classes in declaration order */` |
|  171457 |  6824 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85783 |  6825 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   15625 |  6826 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   15625 |  6827 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   15625 |  6828 | `				bFirst = 0;` |
|    7810 |  6829 | `			}` |
|   42894 |  6830 | `		}` |
|       - |  6831 | `		/* 2) Built-ins in canonical order */` |
|       - |  6832 | `		{` |
|       - |  6833 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6834 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6835 | `			int k;` |
|  599723 |  6836 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  958639 |  6837 | `				for( i = 0; i < nAtoms; i++ ){` |
|  514565 |  6838 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   69975 |  6839 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   69975 |  6840 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   69975 |  6841 | `						bFirst = 0;` |
|   69975 |  6842 | `						break;` |
|       - |  6843 | `					}` |
|  222300 |  6844 | `				}` |
|  257027 |  6845 | `			}` |
|       - |  6846 | `		}` |
|       - |  6847 | `		/* 3) null suffix */` |
|   85679 |  6848 | `		if( bNullable ){` |
|      19 |  6849 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      19 |  6850 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6851 | `		}` |
|       - |  6852 | `	}` |
|   42892 |  6853 | `}` |
|       - |  6854 |  |
|       - |  6855 | `/*` |
|       - |  6856 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6857 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6858 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6859 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6860 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6861 | ` * whether it was parenthesized.` |
|       - |  6862 | ` *` |
|       - |  6863 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6864 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6865 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6866 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6867 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6868 | ` */` |
|   85918 |  6869 | `static sxi32 GenStateParsePart(` |
|       - |  6870 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6871 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6872 | `{` |
|       - |  6873 | `	sxi32 rc;` |
|   85923 |  6874 | `	int nMembers = 0;` |
|   85923 |  6875 | `	int bParen = 0;` |
|   85923 |  6876 | `	*pnMembers = 0;` |
|   85923 |  6877 | `	*pbParen = 0;` |
|   85923 |  6878 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6879 | `		bParen = 1;` |
|       6 |  6880 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6881 | `	}` |
|   42959 |  6882 | `	for(;;){` |
|   85945 |  6883 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6884 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6885 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6886 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6887 | `		}` |
|   85945 |  6888 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   85945 |  6889 | `		if( rc != SXRET_OK ){` |
|       3 |  6890 | `			return rc;` |
|       - |  6891 | `		}` |
|   85943 |  6892 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   85943 |  6893 | `		(*pnAtoms)++;` |
|   85943 |  6894 | `		nMembers++;` |
|       - |  6895 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   85943 |  6896 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      30 |  6897 | `			SyToken *pNext = &pGen->pIn[1];` |
|      26 |  6898 | `			if( pNext < pGen->pEnd` |
|      30 |  6899 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      26 |  6900 | `				pGen->pIn++; /* skip '&' */` |
|      26 |  6901 | `				continue;` |
|       - |  6902 | `			}` |
|       2 |  6903 | `		}` |
|   85921 |  6904 | `		break;` |
|     ! 0 |  6905 | `	}` |
|   85921 |  6906 | `	if( bParen ){` |
|       6 |  6907 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6908 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6909 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6910 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6911 | `		}` |
|       6 |  6912 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6913 | `		if( nMembers < 2 ){` |
|     ! 0 |  6914 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6915 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6916 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6917 | `		}` |
|       2 |  6918 | `	}` |
|   85921 |  6919 | `	*pnMembers = nMembers;` |
|   85921 |  6920 | `	*pbParen = bParen;` |
|   85921 |  6921 | `	return SXRET_OK;` |
|   42964 |  6922 | `}` |
|       - |  6923 |  |
|       - |  6924 | `/*` |
|       - |  6925 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6926 | ` *` |
|       - |  6927 | ` * Outputs:` |
|       - |  6928 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6929 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6930 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6931 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6932 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6933 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6934 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6935 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6936 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6937 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6938 | ` *` |
|       - |  6939 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6940 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6941 | ` */` |
|   85790 |  6942 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6943 | `	ph7_gen_state *pGen,` |
|       - |  6944 | `	sxu32 *pnType,` |
|       - |  6945 | `	SyString *pClass,` |
|       - |  6946 | `	SySet *pAlts,` |
|       - |  6947 | `	sxi32 *piTypeFlags,` |
|       - |  6948 | `	SyString *pTypeText,` |
|       - |  6949 | `	int iNullableFlag,` |
|       - |  6950 | `	int iUnionFlag,` |
|       - |  6951 | `	int bAllowVoid,` |
|       - |  6952 | `	sxu32 nLine` |
|       5 |  6953 | `){` |
|       - |  6954 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   85795 |  6955 | `	int nAtoms = 0;` |
|   85795 |  6956 | `	int bShortNullable = 0;` |
|   85795 |  6957 | `	int bExplicitNull = 0;` |
|       - |  6958 | `	sxi32 rc;` |
|   85795 |  6959 | `	*pnType = 0;` |
|   85795 |  6960 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   85795 |  6961 | `	*piTypeFlags = 0;` |
|   85795 |  6962 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6963 |  |
|   85795 |  6964 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6965 | `		return SXRET_OK;` |
|       - |  6966 | `	}` |
|       - |  6967 | ``	/* Optional `?` shorthand prefix */`` |
|   85790 |  6968 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      75 |  6969 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      74 |  6970 | `		bShortNullable = 1;` |
|      74 |  6971 | `		pGen->pIn++;` |
|      74 |  6972 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6973 | `			return SXERR_SYNTAX;` |
|       - |  6974 | `		}` |
|      35 |  6975 | `	}` |
|       - |  6976 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6977 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6978 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6979 | `	{` |
|       - |  6980 | `		int nMembers, bParen;` |
|   85795 |  6981 | `		sxu32 iGroup = 0;` |
|   85795 |  6982 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   85795 |  6983 | `		if( rc != SXRET_OK ){` |
|       4 |  6984 | `			return rc;` |
|       - |  6985 | `		}` |
|       - |  6986 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6987 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6988 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6989 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6990 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  128877 |  6991 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   85988 |  6992 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     135 |  6993 | `			if( bShortNullable ){` |
|       - |  6994 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6995 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6996 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6997 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6998 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6999 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  7000 | `			}` |
|     133 |  7001 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  7002 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  7003 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  7004 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  7005 | `			}` |
|     133 |  7006 | ``			pGen->pIn++; /* skip `\|` */`` |
|     133 |  7007 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     133 |  7008 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7009 | `				return rc;` |
|       - |  7010 | `			}` |
|       5 |  7011 | `		}` |
|   85791 |  7012 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  7013 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  7014 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  7015 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  7016 | `		}` |
|       - |  7017 | `	}` |
|       - |  7018 | `	/* Validation pass.` |
|       - |  7019 | `	 *` |
|       - |  7020 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  7021 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  7022 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  7023 | `	 */` |
|       - |  7024 | `	{` |
|       - |  7025 | `		int i, j;` |
|   85791 |  7026 | `		int bHasNonNull = 0;` |
|   85791 |  7027 | `		int bAnyIntersection = 0;` |
|       - |  7028 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  7029 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  7030 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2830943 |  7031 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  171727 |  7032 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85941 |  7033 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   42973 |  7034 | `		}` |
|  171681 |  7035 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85917 |  7036 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   42950 |  7037 | `		}` |
|       - |  7038 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  7039 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   85791 |  7040 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  7041 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  7042 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  7043 | `			return SXERR_SYNTAX;` |
|       - |  7044 | `		}` |
|  171713 |  7045 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  7046 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  7047 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  7048 | ``			 * `true`/`false` in an intersection). */`` |
|   85939 |  7049 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      46 |  7050 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      46 |  7051 | `				if( bClassLike ){` |
|      44 |  7052 | `					SyString *pC = &aAtoms[i].sClass;` |
|      40 |  7053 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      40 |  7054 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      40 |  7055 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      44 |  7056 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  7057 | `						bClassLike = 0;` |
|     ! 0 |  7058 | `					}` |
|      20 |  7059 | `				}` |
|      46 |  7060 | `				if( !bClassLike ){` |
|       - |  7061 | `					const char *zName; sxu32 nName;` |
|       3 |  7062 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  7063 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  7064 | `					}else{` |
|       3 |  7065 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  7066 | `					}` |
|       4 |  7067 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  7068 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  7069 | `						(int)nName, zName);` |
|       3 |  7070 | `					return SXERR_SYNTAX;` |
|       - |  7071 | `				}` |
|      20 |  7072 | `			}` |
|   85937 |  7073 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     165 |  7074 | `				if( nAtoms > 1 ){` |
|       3 |  7075 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  7076 | `						"Void can only be used as a standalone type");` |
|       3 |  7077 | `					return SXERR_SYNTAX;` |
|       - |  7078 | `				}` |
|     163 |  7079 | `				if( !bAllowVoid ){` |
|     ! 0 |  7080 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  7081 | `						"void cannot be used here");` |
|     ! 0 |  7082 | `					return SXERR_SYNTAX;` |
|       - |  7083 | `				}` |
|     163 |  7084 | `				if( bShortNullable ){` |
|     ! 0 |  7085 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  7086 | `						"Void type cannot be nullable");` |
|     ! 0 |  7087 | `					return SXERR_SYNTAX;` |
|       - |  7088 | `				}` |
|      79 |  7089 | `			}` |
|   85935 |  7090 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  7091 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|       - |  7092 | `				 * type (never = the function does not return). Mirrors the void` |
|       - |  7093 | `				 * validation above; accepted here and enforced at compile time` |
|       - |  7094 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|      24 |  7095 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|       - |  7096 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|       - |  7097 | `					 * same as any other non-standalone use. */` |
|       5 |  7098 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  7099 | `						"never can only be used as a standalone type");` |
|       5 |  7100 | `					return SXERR_SYNTAX;` |
|       - |  7101 | `				}` |
|      19 |  7102 | `				if( !bAllowVoid ){` |
|       - |  7103 | `					/* Return-only: params call with bAllowVoid=0. */` |
|       3 |  7104 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  7105 | `						"never cannot be used as a parameter type");` |
|       3 |  7106 | `					return SXERR_SYNTAX;` |
|       - |  7107 | `				}` |
|       7 |  7108 | `			}` |
|   85929 |  7109 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  7110 | `				bExplicitNull = 1;` |
|      18 |  7111 | `			}else{` |
|   85901 |  7112 | `				bHasNonNull = 1;` |
|       - |  7113 | `			}` |
|       - |  7114 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  7115 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  7116 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  7117 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  7118 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   86115 |  7119 | `			for( j = 0; j < i; j++ ){` |
|     193 |  7120 | `				int bDup = 0;` |
|     193 |  7121 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     369 |  7122 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     188 |  7123 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     193 |  7124 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     185 |  7125 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      47 |  7126 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      40 |  7127 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      42 |  7128 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      17 |  7129 | `								aAtoms[j].sClass.zString,` |
|      34 |  7130 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  7131 | `							bDup = 1;` |
|     ! 0 |  7132 | `						}` |
|      25 |  7133 | `					}else{` |
|       3 |  7134 | `						bDup = 1;` |
|       - |  7135 | `					}` |
|      21 |  7136 | `				}` |
|     185 |  7137 | `				if( bDup ){` |
|       - |  7138 | `					const char *zName;` |
|       - |  7139 | `					sxu32 nName;` |
|       3 |  7140 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  7141 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  7142 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  7143 | `					}else{` |
|       3 |  7144 | `						zName = aAtoms[i].zCanon;` |
|       3 |  7145 | `						nName = aAtoms[i].nCanon;` |
|       - |  7146 | `					}` |
|       4 |  7147 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  7148 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  7149 | `					return SXERR_SYNTAX;` |
|       - |  7150 | `				}` |
|      94 |  7151 | `			}` |
|   42966 |  7152 | `		}` |
|   85779 |  7153 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  7154 | `			if( bShortNullable ){` |
|       - |  7155 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  7156 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  7157 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  7158 | `				return SXERR_SYNTAX;` |
|       - |  7159 | `			}` |
|       - |  7160 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  7161 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  7162 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  7163 | `			 * atom, so set it here. */` |
|       7 |  7164 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  7165 | `		}` |
|       - |  7166 | `	}` |
|       - |  7167 | `	/* Compute nullability flag */` |
|   85779 |  7168 | `	if( bShortNullable \|\| bExplicitNull ){` |
|     100 |  7169 | `		*piTypeFlags \|= iNullableFlag;` |
|      48 |  7170 | `	}` |
|       - |  7171 | `	/* Build canonical type text */` |
|   85779 |  7172 | `	if( pTypeText ){` |
|       - |  7173 | `		SyBlob sBlob;` |
|   85779 |  7174 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  128632 |  7175 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   42887 |  7176 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   85779 |  7177 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  128408 |  7178 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   85602 |  7179 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   85607 |  7180 | `			if( zDup ){` |
|   85607 |  7181 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   42801 |  7182 | `			}` |
|   42801 |  7183 | `		}` |
|   85779 |  7184 | `		SyBlobRelease(&sBlob);` |
|   42887 |  7185 | `	}` |
|       - |  7186 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  7187 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  7188 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  7189 | `	{` |
|   85779 |  7190 | `		int nNonNull = 0;` |
|   85779 |  7191 | `		int iNonNullIdx = -1;` |
|       - |  7192 | `		int i;` |
|  171693 |  7193 | `		for( i = 0; i < nAtoms; i++ ){` |
|   85919 |  7194 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   85891 |  7195 | `				nNonNull++;` |
|   85891 |  7196 | `				iNonNullIdx = i;` |
|   42943 |  7197 | `			}` |
|   42962 |  7198 | `		}` |
|   85779 |  7199 | `		if( nNonNull <= 1 ){` |
|       - |  7200 | `			/* Fast path: store as single type. */` |
|   85681 |  7201 | `			if( iNonNullIdx >= 0 ){` |
|   85675 |  7202 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   85675 |  7203 | `				if( pA->nType == SXU32_HIGH ){` |
|   23396 |  7204 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7797 |  7205 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   15599 |  7206 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   15599 |  7207 | `					*pnType = SXU32_HIGH;` |
|   15599 |  7208 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   77878 |  7209 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     163 |  7210 | `					*pnType = MEMOBJ_VOID;` |
|   70002 |  7211 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|      16 |  7212 | `					*pnType = MEMOBJ_NEVER;` |
|       9 |  7213 | `				}else{` |
|   69909 |  7214 | `					*pnType = pA->nType;` |
|       - |  7215 | `				}` |
|   42835 |  7216 | `			}` |
|   42843 |  7217 | `		}else{` |
|       - |  7218 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|     103 |  7219 | `			*piTypeFlags \|= iUnionFlag;` |
|     329 |  7220 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  7221 | `				ph7_type_alt sAlt;` |
|     231 |  7222 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     221 |  7223 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     221 |  7224 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     221 |  7225 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     134 |  7226 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      43 |  7227 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      91 |  7228 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      91 |  7229 | `					sAlt.nType = SXU32_HIGH;` |
|      91 |  7230 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      48 |  7231 | `				}else{` |
|     135 |  7232 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  7233 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  7234 | `				}` |
|     221 |  7235 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     113 |  7236 | `			}` |
|       - |  7237 | `		}` |
|       - |  7238 | `	}` |
|   85779 |  7239 | `	return SXRET_OK;` |
|   42900 |  7240 | `}` |
|       - |  7241 |  |
|       - |  7242 | `/*` |
|       - |  7243 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  7244 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  7245 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  7246 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  7247 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  7248 | `` *          and union types `: T\|U`.`` |
|       - |  7249 | ` */` |
|  345192 |  7250 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  7251 | `{` |
|  345197 |  7252 | `	sxi32 iFlags = 0;` |
|       - |  7253 | `	sxi32 rc;` |
|       - |  7254 | `	sxu32 nLine;` |
|  345197 |  7255 | `	pFunc->nReturnType = 0;` |
|  345197 |  7256 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  345197 |  7257 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|       - |  7258 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|       - |  7259 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|       - |  7260 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|       - |  7261 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|       - |  7262 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|  345197 |  7263 | `	SySetReset(&pFunc->aReturnUnion);` |
|  345197 |  7264 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|  345197 |  7265 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  344603 |  7266 | `		return SXRET_OK;` |
|       - |  7267 | `	}` |
|     599 |  7268 | `	pGen->pIn++; /* Skip ':' */` |
|     599 |  7269 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7270 | `		return SXRET_OK;` |
|       - |  7271 | `	}` |
|     599 |  7272 | `	nLine = pGen->pIn->nLine;` |
|     599 |  7273 | `	rc = GenStateParseUnionTypeDecl(` |
|     297 |  7274 | `		pGen,` |
|     297 |  7275 | `		&pFunc->nReturnType,` |
|     297 |  7276 | `		&pFunc->sReturnClass,` |
|     297 |  7277 | `		&pFunc->aReturnUnion,` |
|       - |  7278 | `		&iFlags,` |
|     297 |  7279 | `		&pFunc->sReturnTypeName,` |
|       - |  7280 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  7281 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  7282 | `		/* iUnionFlag */ 0,` |
|       - |  7283 | `		/* bAllowVoid */ 1,` |
|     297 |  7284 | `		nLine);` |
|     599 |  7285 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7286 | `		return SXERR_ABORT;` |
|       - |  7287 | `	}` |
|     599 |  7288 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  7289 | `		/* Error already reported */` |
|     ! 0 |  7290 | `		return SXERR_SYNTAX;` |
|       - |  7291 | `	}` |
|     599 |  7292 | `	if( rc == SXERR_SYNTAX ){` |
|       8 |  7293 | `		if( pGen->pIn < pGen->pEnd ){` |
|      11 |  7294 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  7295 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       6 |  7296 | `				&pGen->pIn->sData);` |
|       5 |  7297 | `		}else{` |
|     ! 0 |  7298 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  7299 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  7300 | `		}` |
|       8 |  7301 | `		return SXERR_SYNTAX;` |
|       - |  7302 | `	}` |
|     593 |  7303 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     593 |  7304 | `	return SXRET_OK;` |
|  172601 |  7305 | `}` |
|       - |  7306 |  |
|   55454 |  7307 | `static sxi32 GenStateCompileFunc(` |
|       - |  7308 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7309 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  7310 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  7311 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  7312 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  7313 | `	)` |
|       5 |  7314 | `{` |
|       - |  7315 | `	ph7_vm_func *pFunc;` |
|       - |  7316 | `	SyToken *pEnd;` |
|       - |  7317 | `	sxu32 nLine;` |
|       - |  7318 | `	char *zName;` |
|       - |  7319 | `	sxi32 rc;` |
|       - |  7320 | `	/* Extract line number */` |
|   55459 |  7321 | `	nLine = pGen->pIn->nLine;` |
|       - |  7322 | `	/* Jump the left parenthesis '(' */` |
|   55459 |  7323 | `	pGen->pIn++;` |
|       - |  7324 | `	/* Delimit the function signature */` |
|   55459 |  7325 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   55459 |  7326 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7327 | `		/* Syntax error */` |
|       9 |  7328 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       9 |  7329 | `		if( rc == SXERR_ABORT ){` |
|       - |  7330 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7331 | `			return SXERR_ABORT;` |
|       - |  7332 | `		}` |
|       9 |  7333 | `		pGen->pIn = pGen->pEnd;` |
|       9 |  7334 | `		return SXRET_OK;` |
|       - |  7335 | `	}` |
|       - |  7336 | `	/* Create the function state */` |
|   55453 |  7337 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   55453 |  7338 | `	if( pFunc == 0 ){` |
|     ! 0 |  7339 | `		goto OutOfMem;` |
|       - |  7340 | `	}` |
|       - |  7341 | `	/* Build the function name, prepending namespace if active */` |
|   55460 |  7342 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  7343 | `		SyBlob sFQN;` |
|       - |  7344 | `		sxu32 nLen;` |
|      16 |  7345 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  7346 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  7347 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  7348 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  7349 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  7350 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  7351 | `		SyBlobRelease(&sFQN);` |
|      16 |  7352 | `		if( zName == 0 ){` |
|     ! 0 |  7353 | `			goto OutOfMem;` |
|       - |  7354 | `		}` |
|      16 |  7355 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  7356 | `	}else{` |
|   55439 |  7357 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   55439 |  7358 | `		if( zName == 0 ){` |
|     ! 0 |  7359 | `			goto OutOfMem;` |
|       - |  7360 | `		}` |
|   55439 |  7361 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  7362 | `	}` |
|   55453 |  7363 | `	if( pGen->pIn < pEnd ){` |
|       - |  7364 | `		/* Collect function arguments */` |
|   39381 |  7365 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   39381 |  7366 | `		if( rc == SXERR_ABORT ){` |
|       - |  7367 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7368 | `			return SXERR_ABORT;` |
|       - |  7369 | `		}` |
|   19688 |  7370 | `	}` |
|       - |  7371 | `	/* Point past ')' and parse optional return type ': type' */` |
|   55453 |  7372 | `	pGen->pIn = &pEnd[1];` |
|       - |  7373 | `	{` |
|   55453 |  7374 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   55453 |  7375 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7376 | `			return SXERR_ABORT;` |
|   55453 |  7377 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       8 |  7378 | `			return SXERR_SYNTAX;` |
|       - |  7379 | `		}` |
|       - |  7380 | `	}` |
|   55447 |  7381 | `	if( bHandleClosure ){` |
|       - |  7382 | `		ph7_vm_func_closure_env sEnv;` |
|     329 |  7383 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     324 |  7384 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     179 |  7385 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      29 |  7386 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  7387 | `				/* Closure,record environment variable */` |
|      29 |  7388 | `				pGen->pIn++;` |
|      29 |  7389 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  7390 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  7391 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7392 | `						return SXERR_ABORT;` |
|       - |  7393 | `					}` |
|     ! 0 |  7394 | `				}` |
|      29 |  7395 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  7396 | `				/* Compile until we hit the first closing parenthesis */` |
|      57 |  7397 | `				while( pGen->pIn < pGen->pEnd ){` |
|      57 |  7398 | `					int iFlagsLocal = 0;` |
|      57 |  7399 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      29 |  7400 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      29 |  7401 | `						break;` |
|       - |  7402 | `					}` |
|      33 |  7403 | `					nLineLocal = pGen->pIn->nLine;` |
|      33 |  7404 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  7405 | `						/* Pass by reference,record that */` |
|     ! 0 |  7406 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  7407 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  7408 | `							);` |
|     ! 0 |  7409 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  7410 | `						pGen->pIn++;` |
|     ! 0 |  7411 | `					}` |
|      28 |  7412 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      33 |  7413 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7414 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  7415 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  7416 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  7417 | `								return SXERR_ABORT;` |
|       - |  7418 | `							}` |
|       - |  7419 | `							/* Find the closing parenthesis */` |
|     ! 0 |  7420 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  7421 | `								pGen->pIn++;` |
|     ! 0 |  7422 | `							}` |
|     ! 0 |  7423 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  7424 | `								pGen->pIn++;` |
|     ! 0 |  7425 | `							}` |
|     ! 0 |  7426 | `							break;` |
|       - |  7427 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  7428 | `					}else{` |
|       - |  7429 | `						SyString *pNameLocal;` |
|       - |  7430 | `						char *zDup;` |
|       - |  7431 | `						/* Duplicate variable name */` |
|      33 |  7432 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      33 |  7433 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      33 |  7434 | `						if( zDup ){` |
|       - |  7435 | `							/* Zero the structure */` |
|      33 |  7436 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      33 |  7437 | `							sEnv.iFlags = iFlagsLocal;` |
|      33 |  7438 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      33 |  7439 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      33 |  7440 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  7441 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  7442 | `									got_this = 1;` |
|     ! 0 |  7443 | `							}` |
|       - |  7444 | `							/* Save imported variable */` |
|      33 |  7445 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      19 |  7446 | `						}else{` |
|     ! 0 |  7447 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7448 | `							 return SXERR_ABORT;` |
|       - |  7449 | `						}` |
|       - |  7450 | `					}` |
|      33 |  7451 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      39 |  7452 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7453 | `						/* Ignore trailing commas */` |
|       7 |  7454 | `						pGen->pIn++;` |
|       1 |  7455 | `					}` |
|       5 |  7456 | `				}` |
|      29 |  7457 | `				if( !got_this ){` |
|       - |  7458 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  7459 | `					 * available to the closure environment.` |
|       - |  7460 | `					 */` |
|      29 |  7461 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      29 |  7462 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      29 |  7463 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      29 |  7464 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      29 |  7465 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      12 |  7466 | `				}` |
|      29 |  7467 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  7468 | `					/* Mark as closure */` |
|      29 |  7469 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|      12 |  7470 | `				}` |
|       - |  7471 | `				/* php 7.1+: the return type follows the use clause —` |
|       - |  7472 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|       - |  7473 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|       - |  7474 | `				 * so an unconditional call would wipe a type parsed at the` |
|       - |  7475 | `				 * legacy pre-use position. */` |
|      29 |  7476 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|       7 |  7477 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|       7 |  7478 | `					if( rcRt2 == SXERR_ABORT ){` |
|     ! 0 |  7479 | `						return SXERR_ABORT;` |
|       7 |  7480 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|     ! 0 |  7481 | `						return SXERR_SYNTAX;` |
|       - |  7482 | `					}` |
|       3 |  7483 | `				}` |
|      12 |  7484 | `		}` |
|     162 |  7485 | `	}` |
|       - |  7486 | `	/* Compile the body */` |
|   55447 |  7487 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   55447 |  7488 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7489 | `		return SXERR_ABORT;` |
|       - |  7490 | `	}` |
|   55447 |  7491 | `	if( ppFunc ){` |
|     329 |  7492 | `		*ppFunc = pFunc;` |
|     162 |  7493 | `	}` |
|   55447 |  7494 | `	rc = SXRET_OK;` |
|   55447 |  7495 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  7496 | `		/* Finally register the function */` |
|   55423 |  7497 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   27709 |  7498 | `	}` |
|   55447 |  7499 | `	if( rc == SXRET_OK ){` |
|   55447 |  7500 | `		return SXRET_OK;` |
|       - |  7501 | `	}` |
|       - |  7502 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  7503 | `OutOfMem:` |
|       - |  7504 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  7505 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  7506 | `	 */` |
|     ! 0 |  7507 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  7508 | `	return SXERR_ABORT;` |
|   27732 |  7509 | `}` |
|       - |  7510 | `/*` |
|       - |  7511 | ` * Compile a standard PHP function.` |
|       - |  7512 | ` *  Refer to the block-comment above for more information.` |
|       - |  7513 | ` */` |
|   55138 |  7514 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  7515 | `{` |
|       - |  7516 | `	SyString *pName;` |
|       - |  7517 | `	sxi32 iFlags;` |
|       - |  7518 | `	sxu32 nLine;` |
|       - |  7519 | `	sxi32 rc;` |
|       - |  7520 |  |
|   55143 |  7521 | `	nLine = pGen->pIn->nLine;` |
|   55143 |  7522 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   55143 |  7523 | `	iFlags = 0;` |
|   55143 |  7524 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7525 | `		/* Return by reference,remember that */` |
|      10 |  7526 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7527 | `		/* Jump the '&' token */` |
|      10 |  7528 | `		pGen->pIn++;` |
|       4 |  7529 | `	}` |
|   55143 |  7530 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7531 | `		/* Invalid function name */` |
|       7 |  7532 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       7 |  7533 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7534 | `			return SXERR_ABORT;` |
|       - |  7535 | `		}` |
|       - |  7536 | `		/* Sychronize with the next semi-colon or braces*/` |
|      21 |  7537 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      15 |  7538 | `			pGen->pIn++;` |
|       1 |  7539 | `		}` |
|       7 |  7540 | `		return SXRET_OK;` |
|       - |  7541 | `	}` |
|   55137 |  7542 | `	pName = &pGen->pIn->sData;` |
|   55137 |  7543 | `	nLine = pGen->pIn->nLine;` |
|       - |  7544 | `	/* Jump the function name */` |
|   55137 |  7545 | `	pGen->pIn++;` |
|   55137 |  7546 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7547 | `		/* Syntax error */` |
|       3 |  7548 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  7549 | `		if( rc == SXERR_ABORT ){` |
|       - |  7550 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7551 | `			return SXERR_ABORT;` |
|       - |  7552 | `		}` |
|       - |  7553 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  7554 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  7555 | `			pGen->pIn++;` |
|     ! 0 |  7556 | `		}` |
|       3 |  7557 | `		return SXRET_OK;` |
|       - |  7558 | `	}` |
|       - |  7559 | `	/* Compile function body */` |
|   55135 |  7560 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   55135 |  7561 | `	return rc;` |
|   27574 |  7562 | `}` |
|       - |  7563 | `/*` |
|       - |  7564 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7565 | ` * According to the PHP language reference manual` |
|       - |  7566 | ` *  Visibility:` |
|       - |  7567 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7568 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7569 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7570 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7571 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7572 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7573 | ` */` |
|  371046 |  7574 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7575 | `{` |
|  371051 |  7576 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   23183 |  7577 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  347873 |  7578 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   50031 |  7579 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7580 | `	}` |
|       - |  7581 | `	/* Assume public by default */` |
|  297847 |  7582 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  185528 |  7583 | `}` |
|       - |  7584 | `/*` |
|       - |  7585 | ` * Compile a class constant.` |
|       - |  7586 | ` * According to the PHP language reference manual` |
|       - |  7587 | ` *  Class Constants` |
|       - |  7588 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7589 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7590 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7591 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7592 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7593 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7594 | ` * Symisc eXtension.` |
|       - |  7595 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7596 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7597 | ` *  Example:` |
|       - |  7598 | ` *   class Test{` |
|       - |  7599 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7600 | ` *   };` |
|       - |  7601 | ` *   var_dump(TEST::MyConst);` |
|       - |  7602 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7603 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7604 | ` */` |
|       - |  7605 | `/*` |
|       - |  7606 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7607 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7608 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7609 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7610 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7611 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7612 | ` */` |
|     100 |  7613 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7614 | `{` |
|       - |  7615 | `	SyToken *p0, *p1;` |
|     105 |  7616 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7617 | `		return 0;` |
|       - |  7618 | `	}` |
|     105 |  7619 | `	p0 = pGen->pIn;` |
|       - |  7620 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|     105 |  7621 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7622 | `		return 1;` |
|       - |  7623 | `	}` |
|     105 |  7624 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7625 | `		return 1;` |
|       - |  7626 | `	}` |
|       - |  7627 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7628 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7629 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|     101 |  7630 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     101 |  7631 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|     101 |  7632 | `		if( p1 ){` |
|     101 |  7633 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      34 |  7634 | `				return 1;` |
|       - |  7635 | `			}` |
|      70 |  7636 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7637 | `				return 1;` |
|       - |  7638 | `			}` |
|      31 |  7639 | `		}` |
|      31 |  7640 | `	}` |
|      66 |  7641 | `	return 0;` |
|      55 |  7642 | `}` |
|       - |  7643 | `/*` |
|       - |  7644 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|       - |  7645 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|       - |  7646 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|       - |  7647 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|       - |  7648 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|       - |  7649 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|       - |  7650 | ` * Peek only; never consumes tokens.` |
|       - |  7651 | ` */` |
|      24 |  7652 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|       4 |  7653 | `{` |
|      28 |  7654 | `	SyToken *p = pGen->pIn;` |
|      39 |  7655 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      20 |  7656 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|       3 |  7657 | `		p++; /* skip leading unary sign(s) */` |
|       1 |  7658 | `	}` |
|      28 |  7659 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|      23 |  7660 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|       - |  7661 | `	}` |
|       6 |  7662 | `	p++;` |
|       - |  7663 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|       6 |  7664 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|      16 |  7665 | `}` |
|       - |  7666 | `/*` |
|       - |  7667 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|       - |  7668 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|       - |  7669 | `` * `$o->new`), not a `new` expression.`` |
|       - |  7670 | ` */` |
|       6 |  7671 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|       3 |  7672 | `{` |
|       - |  7673 | `	sxi32 iOp;` |
|       9 |  7674 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|     ! 0 |  7675 | `		return 0;` |
|       - |  7676 | `	}` |
|       9 |  7677 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       9 |  7678 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|       6 |  7679 | `}` |
|       - |  7680 | `/*` |
|       - |  7681 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|       - |  7682 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|       - |  7683 | ` * interface-constant and (instance/static) property-default initializers` |
|       - |  7684 | ` * ("New expressions are not supported in this context") while still allowing it` |
|       - |  7685 | ` * in global constants, parameter defaults and static-local initializers (which` |
|       - |  7686 | ` * are compiled by different functions and left untouched). The scan is` |
|       - |  7687 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|       - |  7688 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|       - |  7689 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|       - |  7690 | ` *` |
|       - |  7691 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|       - |  7692 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|       - |  7693 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|       - |  7694 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|       - |  7695 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|       - |  7696 | ` */` |
|   23672 |  7697 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|       5 |  7698 | `{` |
|   23677 |  7699 | `	SyToken *p = pGen->pIn;` |
|   23677 |  7700 | `	int iDepth = 0;` |
|   71223 |  7701 | `	while( p < pGen->pEnd ){` |
|   71223 |  7702 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   23669 |  7703 | `			break; /* end of this initializer */` |
|       - |  7704 | `		}` |
|   47554 |  7705 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   23787 |  7706 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      10 |  7707 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  7708 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|       - |  7709 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|       - |  7710 | `			 * expression. */` |
|       3 |  7711 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|       3 |  7712 | `			p++;` |
|       3 |  7713 | `			if( bArrow ){` |
|       - |  7714 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|       - |  7715 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|       3 |  7716 | `				int iBase = iDepth;` |
|      17 |  7717 | `				while( p < pGen->pEnd ){` |
|      17 |  7718 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       5 |  7719 | `						iDepth++;` |
|      15 |  7720 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       5 |  7721 | `						if( iDepth <= iBase ){` |
|     ! 0 |  7722 | `							break; /* closes an enclosing group, not the fn's own */` |
|       - |  7723 | `						}` |
|       5 |  7724 | `						iDepth--;` |
|      11 |  7725 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|       3 |  7726 | `						break;` |
|       - |  7727 | `					}` |
|      15 |  7728 | `					p++;` |
|       1 |  7729 | `				}` |
|       2 |  7730 | `			}else{` |
|       - |  7731 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|       - |  7732 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|       - |  7733 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|       - |  7734 | `				 * then skip the balanced brace block. */` |
|     ! 0 |  7735 | `				int iLocal = 0;` |
|     ! 0 |  7736 | `				while( p < pGen->pEnd ){` |
|     ! 0 |  7737 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|     ! 0 |  7738 | `						break; /* body brace */` |
|       - |  7739 | `					}` |
|     ! 0 |  7740 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  7741 | `						iLocal++;` |
|     ! 0 |  7742 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  7743 | `						if( iLocal > 0 ){` |
|     ! 0 |  7744 | `							iLocal--;` |
|     ! 0 |  7745 | `						}` |
|     ! 0 |  7746 | `					}` |
|     ! 0 |  7747 | `					p++;` |
|     ! 0 |  7748 | `				}` |
|     ! 0 |  7749 | `				if( p < pGen->pEnd ){` |
|     ! 0 |  7750 | `					int iBrace = 0; /* p is on the body '{' */` |
|     ! 0 |  7751 | `					while( p < pGen->pEnd ){` |
|     ! 0 |  7752 | `						if( p->nType & PH7_TK_OCB ){` |
|     ! 0 |  7753 | `							iBrace++;` |
|     ! 0 |  7754 | `						}else if( p->nType & PH7_TK_CCB ){` |
|     ! 0 |  7755 | `							iBrace--;` |
|     ! 0 |  7756 | `							if( iBrace == 0 ){` |
|     ! 0 |  7757 | `								p++;` |
|     ! 0 |  7758 | `								break;` |
|       - |  7759 | `							}` |
|     ! 0 |  7760 | `						}` |
|     ! 0 |  7761 | `						p++;` |
|     ! 0 |  7762 | `					}` |
|     ! 0 |  7763 | `				}` |
|       - |  7764 | `			}` |
|       3 |  7765 | `			continue;` |
|       - |  7766 | `		}` |
|   47557 |  7767 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      69 |  7768 | `			iDepth++;` |
|   47525 |  7769 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      67 |  7770 | `			if( iDepth > 0 ){` |
|      67 |  7771 | `				iDepth--;` |
|      31 |  7772 | `			}` |
|   47462 |  7773 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   23647 |  7774 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|       - |  7775 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|       - |  7776 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|       - |  7777 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|      11 |  7778 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|      11 |  7779 | `				return 1;` |
|       - |  7780 | `			}` |
|     ! 0 |  7781 | `		}` |
|   47549 |  7782 | `		p++;` |
|       5 |  7783 | `	}` |
|   23669 |  7784 | `	return 0;` |
|   11841 |  7785 | `}` |
|       - |  7786 | `/*` |
|       - |  7787 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7788 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7789 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7790 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7791 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7792 | ` * share the same backing.` |
|       - |  7793 | ` */` |
|     214 |  7794 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7795 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7796 | `{` |
|     219 |  7797 | `	pAttr->nType = nType;` |
|     219 |  7798 | `	pAttr->sClass = *pClass;` |
|     219 |  7799 | `	pAttr->sTypeName = *pTypeName;` |
|     219 |  7800 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7801 | `		sxu32 i;` |
|      67 |  7802 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      47 |  7803 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      47 |  7804 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      26 |  7805 | `		}` |
|      10 |  7806 | `	}` |
|     219 |  7807 | `}` |
|     100 |  7808 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7809 | `{` |
|     105 |  7810 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7811 | `	SySet *pInstrContainer;` |
|       - |  7812 | `	ph7_class_attr *pCons;` |
|       - |  7813 | `	SyString *pName;` |
|       - |  7814 | `	sxi32 rc;` |
|     105 |  7815 | `	sxu32 nType = 0;` |
|       - |  7816 | `	SyString sTypeClass;` |
|       - |  7817 | `	SyString sTypeText;` |
|       - |  7818 | `	SySet aUnionAlts;` |
|     105 |  7819 | `	sxi32 iTypeFlags = 0;` |
|     105 |  7820 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|     105 |  7821 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|     105 |  7822 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7823 | `	/* Extract visibility level */` |
|     105 |  7824 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7825 | `	/* Mark as constant */` |
|     105 |  7826 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|     105 |  7827 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7828 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7829 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|     124 |  7830 | `	if( GenStateClassConstHasType(pGen) ){` |
|      61 |  7831 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      38 |  7832 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7833 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7834 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7835 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7836 | `		 * and success paths release. */` |
|      42 |  7837 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7838 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7839 | `			goto Synchronize;` |
|      42 |  7840 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7841 | `			return SXERR_ABORT;` |
|      42 |  7842 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7843 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7844 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7845 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7846 | `				return SXERR_ABORT;` |
|       - |  7847 | `			}` |
|     ! 0 |  7848 | `			goto Synchronize;` |
|       - |  7849 | `		}` |
|      42 |  7850 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      19 |  7851 | `	}` |
|      50 |  7852 | `loop:` |
|     107 |  7853 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7854 | `		/* Invalid constant name */` |
|     ! 0 |  7855 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7856 | `		if( rc == SXERR_ABORT ){` |
|       - |  7857 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7858 | `			return SXERR_ABORT;` |
|       - |  7859 | `		}` |
|     ! 0 |  7860 | `		goto Synchronize;` |
|       - |  7861 | `	}` |
|       - |  7862 | `	/* Peek constant name */` |
|     107 |  7863 | `	pName = &pGen->pIn->sData;` |
|       - |  7864 | `	/* Make sure the constant name isn't reserved */` |
|     107 |  7865 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7866 | `		/* Reserved constant name */` |
|     ! 0 |  7867 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7868 | `		if( rc == SXERR_ABORT ){` |
|       - |  7869 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7870 | `			return SXERR_ABORT;` |
|       - |  7871 | `		}` |
|     ! 0 |  7872 | `		goto Synchronize;` |
|       - |  7873 | `	}` |
|       - |  7874 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|     107 |  7875 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      61 |  7876 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      38 |  7877 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      19 |  7878 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      42 |  7879 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7880 | `			return SXERR_ABORT;` |
|      42 |  7881 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7882 | `			goto Synchronize;` |
|       - |  7883 | `		}` |
|      18 |  7884 | `	}` |
|       - |  7885 | `	/* Advance the stream cursor */` |
|     105 |  7886 | `	pGen->pIn++;` |
|     105 |  7887 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7888 | `		/* Invalid declaration */` |
|     ! 0 |  7889 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7890 | `		if( rc == SXERR_ABORT ){` |
|       - |  7891 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7892 | `			return SXERR_ABORT;` |
|       - |  7893 | `		}` |
|     ! 0 |  7894 | `		goto Synchronize;` |
|       - |  7895 | `	}` |
|     105 |  7896 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7897 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|       - |  7898 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|       - |  7899 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|       - |  7900 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|     100 |  7901 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|      39 |  7902 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|       8 |  7903 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7904 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|       2 |  7905 | `			&pClass->sName,pName,&sTypeText);` |
|       6 |  7906 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7907 | `			return SXERR_ABORT;` |
|       - |  7908 | `		}` |
|       6 |  7909 | `		goto Synchronize;` |
|       - |  7910 | `	}` |
|       - |  7911 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|       - |  7912 | `	 * constant initializer ("New expressions are not supported in this context").` |
|       - |  7913 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|     101 |  7914 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|       5 |  7915 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7916 | `			"New expressions are not supported in this context");` |
|       5 |  7917 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7918 | `			return SXERR_ABORT;` |
|       - |  7919 | `		}` |
|       5 |  7920 | `		goto Synchronize;` |
|       - |  7921 | `	}` |
|       - |  7922 | `	/* Allocate a new class attribute */` |
|      97 |  7923 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      97 |  7924 | `	if( pCons == 0 ){` |
|     ! 0 |  7925 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7926 | `		return SXERR_ABORT;` |
|       - |  7927 | `	}` |
|      97 |  7928 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      35 |  7929 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      16 |  7930 | `	}` |
|       - |  7931 | `	/* Swap bytecode container */` |
|      97 |  7932 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 |  7933 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7934 | `	/* Compile constant value.` |
|       - |  7935 | `	 */` |
|      97 |  7936 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      97 |  7937 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7938 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7939 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7940 | `			return SXERR_ABORT;` |
|       - |  7941 | `		}` |
|       1 |  7942 | `	}` |
|       - |  7943 | `	/* Emit the done instruction */` |
|      97 |  7944 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      97 |  7945 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      97 |  7946 | `	if( rc == SXERR_ABORT ){` |
|       - |  7947 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7948 | `		return SXERR_ABORT;` |
|       - |  7949 | `	}` |
|       - |  7950 | `	/* All done,install the constant */` |
|      97 |  7951 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      97 |  7952 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7953 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7954 | `		return SXERR_ABORT;` |
|       - |  7955 | `	}` |
|      97 |  7956 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7957 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7958 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7959 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7960 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7961 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7962 | `				pTok--;` |
|     ! 0 |  7963 | `			}` |
|     ! 0 |  7964 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7965 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7966 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7967 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7968 | `				return SXERR_ABORT;` |
|       - |  7969 | `			}` |
|     ! 0 |  7970 | `		}else{` |
|       3 |  7971 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7972 | `				goto loop;` |
|       - |  7973 | `			}` |
|       - |  7974 | `		}` |
|     ! 0 |  7975 | `	}` |
|      95 |  7976 | `	SySetRelease(&aUnionAlts);` |
|      95 |  7977 | `	return SXRET_OK;` |
|       5 |  7978 | `Synchronize:` |
|      13 |  7979 | `	SySetRelease(&aUnionAlts);` |
|       - |  7980 | `	/* Synchronize with the first semi-colon */` |
|      45 |  7981 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      35 |  7982 | `		pGen->pIn++;` |
|       3 |  7983 | `	}` |
|      13 |  7984 | `	return SXERR_CORRUPT;` |
|      55 |  7985 | `}` |
|       - |  7986 | `/*` |
|       - |  7987 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7988 | ` * According to the PHP language reference manual` |
|       - |  7989 | ` *  Properties` |
|       - |  7990 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7991 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7992 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7993 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7994 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7995 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7996 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7997 | ` * Symisc eXtension.` |
|       - |  7998 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7999 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  8000 | ` *  Example:` |
|       - |  8001 | ` *   class Test{` |
|       - |  8002 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  8003 | ` *   };` |
|       - |  8004 | ` *   var_dump(TEST::myVar);` |
|       - |  8005 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  8006 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  8007 | ` */` |
|       - |  8008 | `/*` |
|       - |  8009 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  8010 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  8011 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  8012 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  8013 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  8014 | ` */` |
|  201062 |  8015 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  8016 | `{` |
|  201067 |  8017 | `	SyToken *p = pStart;` |
|  201067 |  8018 | `	int bFirst = 1;` |
|  201067 |  8019 | `	if( p >= pEnd ) return 0;` |
|       - |  8020 | ``	/* Optional nullable `?` shorthand. */`` |
|  201067 |  8021 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      19 |  8022 | `		p++;` |
|      19 |  8023 | `		if( p >= pEnd ) return 0;` |
|       8 |  8024 | `	}` |
|       - |  8025 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  8026 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  8027 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  8028 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|  100531 |  8029 | `	for(;;){` |
|  201085 |  8030 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  8031 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  8032 | `			p++;` |
|       9 |  8033 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  8034 | `			if( p >= pEnd ) return 0;` |
|       3 |  8035 | `			p++; /* skip ')' */` |
|       2 |  8036 | `		}else{` |
|       - |  8037 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  8038 | ``			 * then any `&`-joined intersection members. */`` |
|  201083 |  8039 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  201083 |  8040 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  8041 | `				return 0;` |
|       - |  8042 | `			}` |
|       - |  8043 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  8044 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  8045 | `			 * may still appear at the initial dispatch site). */` |
|  201083 |  8046 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  201037 |  8047 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  201032 |  8048 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   11762 |  8049 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  200881 |  8050 | `					return 0;` |
|       - |  8051 | `				}` |
|      78 |  8052 | `			}` |
|     207 |  8053 | `			p++;` |
|     209 |  8054 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  8055 | `				p += 2;` |
|       1 |  8056 | `			}` |
|     306 |  8057 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     210 |  8058 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  8059 | `				p++; /* skip '&' */` |
|       3 |  8060 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  8061 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  8062 | `				p++;` |
|       3 |  8063 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  8064 | `					p += 2;` |
|     ! 0 |  8065 | `				}` |
|       1 |  8066 | `			}` |
|       - |  8067 | `		}` |
|     209 |  8068 | `		bFirst = 0;` |
|     204 |  8069 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  8070 | `			&& p->sData.zString[0] == '\|' ){` |
|      23 |  8071 | ``			p++; /* next `\|`-separated part */`` |
|      23 |  8072 | `			continue;` |
|       - |  8073 | `		}` |
|     191 |  8074 | `		break;` |
|     ! 0 |  8075 | `	}` |
|     191 |  8076 | `	if( p >= pEnd ) return 0;` |
|     191 |  8077 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|  100536 |  8078 | `}` |
|       - |  8079 |  |
|       - |  8080 | `/*` |
|       - |  8081 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  8082 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  8083 | ` * if not). Recognized forms:` |
|       - |  8084 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  8085 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  8086 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  8087 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  8088 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  8089 | ` * on unrecoverable error.` |
|       - |  8090 | ` *` |
|       - |  8091 | ` * When a type is parsed:` |
|       - |  8092 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  8093 | ` *   *pClass is set to the class name (for class types)` |
|       - |  8094 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  8095 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  8096 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  8097 | ` */` |
|     186 |  8098 | `static sxi32 GenStateParsePropertyType(` |
|       - |  8099 | `	ph7_gen_state *pGen,` |
|       - |  8100 | `	sxu32 *pnType,` |
|       - |  8101 | `	SyString *pClass,` |
|       - |  8102 | `	sxi32 *piTypeFlags,` |
|       - |  8103 | `	SyString *pTypeText,` |
|       - |  8104 | `	SySet *pAlts` |
|       5 |  8105 | `){` |
|     191 |  8106 | `	sxi32 iFlags = 0;` |
|       - |  8107 | `	sxi32 rc;` |
|     191 |  8108 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  8109 | `		return SXRET_OK;` |
|       - |  8110 | `	}` |
|       - |  8111 | `	/* If the first token is '$', there's no type */` |
|     191 |  8112 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  8113 | `		return SXRET_OK;` |
|       - |  8114 | `	}` |
|     191 |  8115 | `	rc = GenStateParseUnionTypeDecl(` |
|      93 |  8116 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  8117 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  8118 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  8119 | `		/* bAllowVoid */ 0,` |
|     186 |  8120 | `		pGen->pIn->nLine);` |
|     191 |  8121 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8122 | `		return rc;` |
|       - |  8123 | `	}` |
|       - |  8124 | `	/* Verify next token is '$' (start of property name) */` |
|     191 |  8125 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8126 | `		return SXERR_SYNTAX;` |
|       - |  8127 | `	}` |
|     191 |  8128 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     191 |  8129 | `	return SXRET_OK;` |
|      98 |  8130 | `}` |
|       - |  8131 |  |
|       - |  8132 | `/*` |
|       - |  8133 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  8134 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  8135 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  8136 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  8137 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  8138 | ` * by the type parser itself before reaching here.` |
|       - |  8139 | ` *` |
|       - |  8140 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  8141 | ` * use in the error message.` |
|       - |  8142 | ` */` |
|     346 |  8143 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  8144 | `	sxu32 nType,` |
|       - |  8145 | `	const SyString *pClass,` |
|       - |  8146 | `	const char **pzName,` |
|       - |  8147 | `	sxu32 *pnName)` |
|       5 |  8148 | `{` |
|       - |  8149 | `	const char *z;` |
|       - |  8150 | `	sxu32 n;` |
|     351 |  8151 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     297 |  8152 | `		return 0;` |
|       - |  8153 | `	}` |
|      59 |  8154 | `	z = pClass->zString;` |
|      59 |  8155 | `	n = pClass->nByte;` |
|      59 |  8156 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  8157 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  8158 | `	}` |
|       - |  8159 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  8160 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  8161 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      52 |  8162 | `	return 0;` |
|     178 |  8163 | `}` |
|       - |  8164 |  |
|       - |  8165 | `/*` |
|       - |  8166 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  8167 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  8168 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  8169 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  8170 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  8171 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  8172 | ` *` |
|       - |  8173 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  8174 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  8175 | ` */` |
|     288 |  8176 | `static sxi32 GenStateValidateMemberType(` |
|       - |  8177 | `	ph7_gen_state *pGen,` |
|       - |  8178 | `	ph7_class *pClass,` |
|       - |  8179 | `	const SyString *pMemberName,` |
|       - |  8180 | `	sxu32 nType,` |
|       - |  8181 | `	const SyString *pTypeClass,` |
|       - |  8182 | `	const SyString *pTypeText,` |
|       - |  8183 | `	SySet *pUnionAlts,` |
|       - |  8184 | `	const char *zErrFmt,` |
|       - |  8185 | `	sxu32 nLine)` |
|       5 |  8186 | `{` |
|     293 |  8187 | `	const char *zBad = 0;` |
|     293 |  8188 | `	sxu32 nBad = 0;` |
|       - |  8189 | `	SyString sFallback;` |
|       - |  8190 | `	const SyString *pBad;` |
|       - |  8191 | `	sxi32 rc;` |
|     293 |  8192 | `	int bDisallowed = 0;` |
|     293 |  8193 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  8194 | `		bDisallowed = 1;` |
|     291 |  8195 | `	}else if( pUnionAlts ){` |
|       - |  8196 | `		sxu32 i;` |
|      89 |  8197 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      63 |  8198 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      63 |  8199 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  8200 | `				bDisallowed = 1;` |
|       3 |  8201 | `				break;` |
|       - |  8202 | `			}` |
|      33 |  8203 | `		}` |
|      14 |  8204 | `	}` |
|     293 |  8205 | `	if( !bDisallowed ){` |
|     287 |  8206 | `		return SXRET_OK;` |
|       - |  8207 | `	}` |
|       - |  8208 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  8209 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  8210 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  8211 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  8212 | `		pBad = pTypeText;` |
|       5 |  8213 | `	}else{` |
|     ! 0 |  8214 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  8215 | `		pBad = &sFallback;` |
|       - |  8216 | `	}` |
|      11 |  8217 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  8218 | `		zErrFmt,` |
|       3 |  8219 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  8220 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8221 | `		return SXERR_ABORT;` |
|       - |  8222 | `	}` |
|       8 |  8223 | `	return SXERR_SYNTAX;` |
|     149 |  8224 | `}` |
|       - |  8225 | `/*` |
|       - |  8226 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  8227 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  8228 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  8229 | ` * than promoted to a lexer keyword.` |
|       - |  8230 | ` */` |
| 1867832 |  8231 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  8232 | `{` |
| 1900673 |  8233 | `	return (pTok->nType & PH7_TK_ID)` |
|  966752 |  8234 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1900668 |  8235 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  8236 | `}` |
|   81434 |  8237 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  8238 | `{` |
|   81439 |  8239 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8240 | `	ph7_class_attr *pAttr;` |
|       - |  8241 | `	SyString *pName;` |
|       - |  8242 | `	sxi32 rc;` |
|   81439 |  8243 | `	sxu32 nType = 0;` |
|       - |  8244 | `	SyString sTypeClass;` |
|       - |  8245 | `	SyString sTypeText;` |
|       - |  8246 | `	SySet aUnionAlts;` |
|   81439 |  8247 | `	sxi32 iTypeFlags = 0;` |
|   81439 |  8248 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   81439 |  8249 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   81439 |  8250 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  8251 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  8252 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  8253 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   81439 |  8254 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  8255 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  8256 | `	}` |
|       - |  8257 | `	/* Extract visibility level */` |
|   81439 |  8258 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  8259 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   81532 |  8260 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     191 |  8261 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     191 |  8262 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  8263 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  8264 | `			goto Synchronize;` |
|     191 |  8265 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  8266 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8267 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  8268 | `				&pGen->pIn->sData);` |
|     ! 0 |  8269 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8270 | `				return SXERR_ABORT;` |
|       - |  8271 | `			}` |
|     ! 0 |  8272 | `			goto Synchronize;` |
|     191 |  8273 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  8274 | `			return SXERR_ABORT;` |
|       - |  8275 | `		}` |
|      93 |  8276 | `	}` |
|     ! 0 |  8277 | `loop:` |
|   81443 |  8278 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8279 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  8280 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8281 | `			return SXERR_ABORT;` |
|       - |  8282 | `		}` |
|     ! 0 |  8283 | `		goto Synchronize;` |
|       - |  8284 | `	}` |
|   81443 |  8285 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   81443 |  8286 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  8287 | `		/* Invalid attribute name */` |
|     ! 0 |  8288 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  8289 | `		if( rc == SXERR_ABORT ){` |
|       - |  8290 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8291 | `			return SXERR_ABORT;` |
|       - |  8292 | `		}` |
|     ! 0 |  8293 | `		goto Synchronize;` |
|       - |  8294 | `	}` |
|       - |  8295 | `	/* Peek attribute name */` |
|   81443 |  8296 | `	pName = &pGen->pIn->sData;` |
|       - |  8297 | `	/* Advance the stream cursor */` |
|   81443 |  8298 | `	pGen->pIn++;` |
|   81443 |  8299 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  8300 | `		/* Invalid declaration */` |
|       3 |  8301 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  8302 | `		if( rc == SXERR_ABORT ){` |
|       - |  8303 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8304 | `			return SXERR_ABORT;` |
|       - |  8305 | `		}` |
|       3 |  8306 | `		goto Synchronize;` |
|       - |  8307 | `	}` |
|       - |  8308 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  8309 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   81441 |  8310 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  8311 | `		const char *zRoErr = 0;` |
|      39 |  8312 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  8313 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  8314 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  8315 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  8316 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  8317 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  8318 | `		}` |
|      39 |  8319 | `		if( zRoErr ){` |
|      13 |  8320 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  8321 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8322 | `				return SXERR_ABORT;` |
|       - |  8323 | `			}` |
|      13 |  8324 | `			goto Synchronize;` |
|       - |  8325 | `		}` |
|      12 |  8326 | `	}` |
|       - |  8327 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  8328 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  8329 | `	 * by the type parser. */` |
|   81431 |  8330 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     281 |  8331 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  8332 | `			&sTypeText,` |
|     184 |  8333 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      92 |  8334 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     189 |  8335 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8336 | `			return SXERR_ABORT;` |
|     189 |  8337 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  8338 | `			goto Synchronize;` |
|       - |  8339 | `		}` |
|      92 |  8340 | `	}` |
|       - |  8341 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   81431 |  8342 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  8343 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8344 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  8345 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8346 | `			return SXERR_ABORT;` |
|       - |  8347 | `		}` |
|       3 |  8348 | `		goto Synchronize;` |
|       - |  8349 | `	}` |
|       - |  8350 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|       - |  8351 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|       - |  8352 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|       - |  8353 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|       - |  8354 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|       - |  8355 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   81429 |  8356 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|       6 |  8357 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8358 | `			"New expressions are not supported in this context");` |
|       6 |  8359 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8360 | `			return SXERR_ABORT;` |
|       - |  8361 | `		}` |
|       6 |  8362 | `		goto Synchronize;` |
|       - |  8363 | `	}` |
|       - |  8364 | `	/* Allocate a new class attribute */` |
|   81425 |  8365 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   81425 |  8366 | `	if( pAttr == 0 ){` |
|     ! 0 |  8367 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  8368 | `		return SXERR_ABORT;` |
|       - |  8369 | `	}` |
|   81425 |  8370 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     187 |  8371 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      91 |  8372 | `	}` |
|   81425 |  8373 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  8374 | `		SySet *pInstrContainer;` |
|   23577 |  8375 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  8376 | `		/* Swap bytecode container */` |
|   23577 |  8377 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   23577 |  8378 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  8379 | `		/* Compile attribute value.` |
|       - |  8380 | `		 */` |
|   23577 |  8381 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   23577 |  8382 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  8383 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  8384 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8385 | `				return SXERR_ABORT;` |
|       - |  8386 | `			}` |
|     ! 0 |  8387 | `		}` |
|       - |  8388 | `		/* Emit the done instruction */` |
|   23577 |  8389 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   23577 |  8390 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   11786 |  8391 | `	}` |
|       - |  8392 | `	/* All done,install the attribute */` |
|   81425 |  8393 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   81425 |  8394 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8395 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8396 | `		return SXERR_ABORT;` |
|       - |  8397 | `	}` |
|   81425 |  8398 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  8399 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  8400 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  8401 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  8402 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  8403 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  8404 | `				pTok--;` |
|     ! 0 |  8405 | `			}` |
|     ! 0 |  8406 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8407 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8408 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  8409 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8410 | `				return SXERR_ABORT;` |
|       - |  8411 | `			}` |
|     ! 0 |  8412 | `		}else{` |
|       5 |  8413 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  8414 | `				goto loop;` |
|       - |  8415 | `			}` |
|       - |  8416 | `		}` |
|     ! 0 |  8417 | `	}` |
|   81421 |  8418 | `	SySetRelease(&aUnionAlts);` |
|   81421 |  8419 | `	return SXRET_OK;` |
|       9 |  8420 | `Synchronize:` |
|       - |  8421 | `	/* Synchronize with the first semi-colon */` |
|      56 |  8422 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      37 |  8423 | `		pGen->pIn++;` |
|       3 |  8424 | `	}` |
|      22 |  8425 | `	SySetRelease(&aUnionAlts);` |
|      22 |  8426 | `	return SXERR_CORRUPT;` |
|   40722 |  8427 | `}` |
|       - |  8428 | `/*` |
|       - |  8429 | ` * Compile a class method.` |
|       - |  8430 | ` *` |
|       - |  8431 | ` * Refer to the official documentation for more information` |
|       - |  8432 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  8433 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  8434 | ` * overloading and many more.` |
|       - |  8435 | ` */` |
|  289512 |  8436 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  8437 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  8438 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  8439 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  8440 | `	int doBody,          /* TRUE to process method body */` |
|       - |  8441 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  8442 | `	)` |
|       5 |  8443 | `{` |
|  289517 |  8444 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8445 | `	ph7_class_method *pMeth;` |
|       - |  8446 | `	sxi32 iFuncFlags;` |
|       - |  8447 | `	SyString *pName;` |
|       - |  8448 | `	SyToken *pEnd;` |
|       - |  8449 | `	sxi32 rc;` |
|       - |  8450 | `	/* Extract visibility level */` |
|  289517 |  8451 | `	iProtection = GetProtectionLevel(iProtection);` |
|  289517 |  8452 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  289517 |  8453 | `	iFuncFlags = 0;` |
|  289517 |  8454 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8455 | `		/* Invalid method name */` |
|     ! 0 |  8456 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8457 | `		if( rc == SXERR_ABORT ){` |
|       - |  8458 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8459 | `			return SXERR_ABORT;` |
|       - |  8460 | `		}` |
|     ! 0 |  8461 | `		goto Synchronize;` |
|       - |  8462 | `	}` |
|  289517 |  8463 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  8464 | `		/* Return by reference,remember that */` |
|     ! 0 |  8465 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  8466 | `		/* Jump the '&' token */` |
|     ! 0 |  8467 | `		pGen->pIn++;` |
|     ! 0 |  8468 | `	}` |
|  289517 |  8469 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  8470 | `		/* Invalid method name */` |
|     ! 0 |  8471 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8472 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8473 | `			return SXERR_ABORT;` |
|       - |  8474 | `		}` |
|     ! 0 |  8475 | `		goto Synchronize;` |
|       - |  8476 | `	}` |
|       - |  8477 | `	/* Peek method name */` |
|  289517 |  8478 | `	pName = &pGen->pIn->sData;` |
|  289517 |  8479 | `	nLine = pGen->pIn->nLine;` |
|       - |  8480 | `	/* Jump the method name */` |
|  289517 |  8481 | `	pGen->pIn++;` |
|  289517 |  8482 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  8483 | `		/* Abstract method */` |
|  100007 |  8484 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  8485 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8486 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  8487 | `				&pClass->sName,pName);` |
|     ! 0 |  8488 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8489 | `				return SXERR_ABORT;` |
|       - |  8490 | `			}` |
|     ! 0 |  8491 | `		}` |
|       - |  8492 | `		/* Assemble method signature only */` |
|  100007 |  8493 | `		doBody = FALSE;` |
|   50001 |  8494 | `	}` |
|  289517 |  8495 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  8496 | `		/* Syntax error */` |
|     ! 0 |  8497 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  8498 | `		if( rc == SXERR_ABORT ){` |
|       - |  8499 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8500 | `			return SXERR_ABORT;` |
|       - |  8501 | `		}` |
|     ! 0 |  8502 | `		goto Synchronize;` |
|       - |  8503 | `	}` |
|       - |  8504 | `	/* Allocate a new class_method instance */` |
|  289517 |  8505 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  289517 |  8506 | `	if( pMeth == 0 ){` |
|     ! 0 |  8507 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8508 | `		return SXERR_ABORT;` |
|       - |  8509 | `	}` |
|       - |  8510 | `	/* Jump the left parenthesis '(' */` |
|  289517 |  8511 | `	pGen->pIn++;` |
|  289517 |  8512 | `	pEnd = 0; /* cc warning */` |
|       - |  8513 | `	/* Delimit the method signature */` |
|  289517 |  8514 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  289517 |  8515 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8516 | `		/* Syntax error */` |
|       3 |  8517 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  8518 | `		if( rc == SXERR_ABORT ){` |
|       - |  8519 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8520 | `			return SXERR_ABORT;` |
|       - |  8521 | `		}` |
|       3 |  8522 | `		goto Synchronize;` |
|       - |  8523 | `	}` |
|       - |  8524 | `	{` |
|  289515 |  8525 | `		int bIsCtor = 0;` |
|  289515 |  8526 | `		int bAbstractCtor = 0;` |
|  289510 |  8527 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  171795 |  8528 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  277898 |  8529 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   23239 |  8530 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  8531 | `				bAbstractCtor = 1;` |
|       2 |  8532 | `			}else{` |
|   23237 |  8533 | `				bIsCtor = 1;` |
|       - |  8534 | `			}` |
|   11617 |  8535 | `		}` |
|  289515 |  8536 | `		if( pGen->pIn < pEnd ){` |
|       - |  8537 | `			/* Collect method arguments */` |
|   77391 |  8538 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   77391 |  8539 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8540 | `				return SXERR_ABORT;` |
|       - |  8541 | `			}` |
|   38693 |  8542 | `		}` |
|       - |  8543 | `	}` |
|       - |  8544 | `	/* Point past ')' and parse optional return type ': type' */` |
|  289515 |  8545 | `	pGen->pIn = &pEnd[1];` |
|       - |  8546 | `	{` |
|  289515 |  8547 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  289515 |  8548 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  8549 | `			return SXERR_ABORT;` |
|  289515 |  8550 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  8551 | `			goto Synchronize;` |
|       - |  8552 | `		}` |
|       - |  8553 | `	}` |
|       - |  8554 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  8555 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  8556 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  8557 | `	{` |
|  289515 |  8558 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  8559 | `		sxu32 i;` |
|  420837 |  8560 | `		for( i = 0; i < nArg; i++ ){` |
|  131337 |  8561 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  8562 | `			ph7_class_attr *pAttr;` |
|  131337 |  8563 | `			sxi32 iAttrFlags = 0;` |
|       - |  8564 | `			int bArgTyped;` |
|  131337 |  8565 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  131265 |  8566 | `				continue;` |
|       - |  8567 | `			}` |
|       - |  8568 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  8569 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  8570 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      53 |  8571 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      78 |  8572 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      77 |  8573 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  8574 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8575 | `					"Cannot declare variadic promoted property");` |
|       3 |  8576 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8577 | `					return SXERR_ABORT;` |
|       - |  8578 | `				}` |
|       3 |  8579 | `				goto Synchronize;` |
|       - |  8580 | `			}` |
|       - |  8581 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  8582 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  8583 | `			 * appear as an alternative of a union type. */` |
|      75 |  8584 | `			if( bArgTyped ){` |
|     104 |  8585 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      66 |  8586 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      66 |  8587 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      33 |  8588 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      71 |  8589 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8590 | `					return SXERR_ABORT;` |
|      71 |  8591 | `				}else if( rc != SXRET_OK ){` |
|       6 |  8592 | `					goto Synchronize;` |
|       - |  8593 | `				}` |
|      31 |  8594 | `			}` |
|       - |  8595 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      71 |  8596 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  8597 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8598 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  8599 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8600 | `					return SXERR_ABORT;` |
|       - |  8601 | `				}` |
|       3 |  8602 | `				goto Synchronize;` |
|       - |  8603 | `			}` |
|      69 |  8604 | `			if( bArgTyped ){` |
|      65 |  8605 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      30 |  8606 | `			}` |
|      69 |  8607 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  8608 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  8609 | `			}` |
|      69 |  8610 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  8611 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  8612 | `			}` |
|      69 |  8613 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  8614 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  8615 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      26 |  8616 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  8617 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8618 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  8619 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8620 | `						return SXERR_ABORT;` |
|       - |  8621 | `					}` |
|       3 |  8622 | `					goto Synchronize;` |
|       - |  8623 | `				}` |
|      24 |  8624 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|      10 |  8625 | `			}` |
|      67 |  8626 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      67 |  8627 | `			if( pAttr == 0 ){` |
|     ! 0 |  8628 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8629 | `				return SXERR_ABORT;` |
|       - |  8630 | `			}` |
|      67 |  8631 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      65 |  8632 | `				pAttr->nType = pArg->nType;` |
|      65 |  8633 | `				pAttr->sClass = pArg->sClass;` |
|      65 |  8634 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      65 |  8635 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  8636 | `					sxu32 k;` |
|      20 |  8637 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  8638 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  8639 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  8640 | `					}` |
|       3 |  8641 | `				}` |
|      30 |  8642 | `			}` |
|      67 |  8643 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      67 |  8644 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8645 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8646 | `				return SXERR_ABORT;` |
|       - |  8647 | `			}` |
|      36 |  8648 | `		}` |
|       - |  8649 | `	}` |
|  289505 |  8650 | `	if( doBody ){` |
|       - |  8651 | `		/* Compile method body */` |
|  189503 |  8652 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  189503 |  8653 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8654 | `			return SXERR_ABORT;` |
|       - |  8655 | `		}` |
|   94754 |  8656 | `	}else{` |
|       - |  8657 | `		/* Only method signature is allowed */` |
|  100007 |  8658 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  8659 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8660 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  8661 | `				if( rc == SXERR_ABORT ){` |
|       - |  8662 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8663 | `					return SXERR_ABORT;` |
|       - |  8664 | `				}` |
|     ! 0 |  8665 | `				return SXERR_CORRUPT;` |
|       - |  8666 | `			}` |
|       - |  8667 | `	}` |
|       - |  8668 | `	/* All done,install the method */` |
|  289505 |  8669 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  289505 |  8670 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8671 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8672 | `		return SXERR_ABORT;` |
|       - |  8673 | `	}` |
|  289505 |  8674 | `	return SXRET_OK;` |
|       6 |  8675 | `Synchronize:` |
|       - |  8676 | `	/* Synchronize with the first semi-colon */` |
|      40 |  8677 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  8678 | `		pGen->pIn++;` |
|       4 |  8679 | `	}` |
|      16 |  8680 | `	return SXERR_CORRUPT;` |
|  144761 |  8681 | `}` |
|       - |  8682 | `/*` |
|       - |  8683 | ` * Compile an object interface.` |
|       - |  8684 | ` *  According to the PHP language reference manual` |
|       - |  8685 | ` *   Object Interfaces:` |
|       - |  8686 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  8687 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  8688 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  8689 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  8690 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  8691 | ` */` |
|   42368 |  8692 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  8693 | `{` |
|   42373 |  8694 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8695 | `	ph7_class *pClass,*pBase;` |
|       - |  8696 | `	SyToken *pEnd,*pTmp;` |
|       - |  8697 | `	SyString *pName;` |
|       - |  8698 | `	sxi32 nKwrd;` |
|       - |  8699 | `	sxi32 rc;` |
|       - |  8700 | `	/* Jump the 'interface' keyword */` |
|   42373 |  8701 | `	pGen->pIn++;` |
|       - |  8702 | `	/* Extract interface name */` |
|   42373 |  8703 | `	pName = &pGen->pIn->sData;` |
|       - |  8704 | `	/* Advance the stream cursor */` |
|   42373 |  8705 | `	pGen->pIn++;` |
|       - |  8706 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8707 | `		SyBlob sFQN;` |
|       - |  8708 | `		SyString sFQNStr;` |
|   42373 |  8709 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   42373 |  8710 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   42373 |  8711 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   42373 |  8712 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   42373 |  8713 | `		SyBlobRelease(&sFQN);` |
|       - |  8714 | `	}` |
|   42373 |  8715 | `	if( pClass == 0 ){` |
|     ! 0 |  8716 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8717 | `		return SXERR_ABORT;` |
|       - |  8718 | `	}` |
|       - |  8719 | `	/* Mark as an interface */` |
|   42373 |  8720 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  8721 | `	/* Assume no base class is given */` |
|   42373 |  8722 | `	pBase = 0;` |
|   42373 |  8723 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   11545 |  8724 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   11545 |  8725 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  8726 | `			SyBlob sResolved;` |
|       - |  8727 | `			SyString sBaseName;` |
|       - |  8728 | `			sxu32 nRefLine;` |
|       - |  8729 | `			/* Extract base interface */` |
|   11545 |  8730 | `			pGen->pIn++;` |
|   11545 |  8731 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11545 |  8732 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11545 |  8733 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8734 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  8735 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8736 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  8737 | `					pName);` |
|     ! 0 |  8738 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8739 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8740 | `					return SXERR_ABORT;` |
|       - |  8741 | `				}` |
|     ! 0 |  8742 | `				return SXRET_OK;` |
|       - |  8743 | `			}` |
|   17315 |  8744 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   11540 |  8745 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11545 |  8746 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8747 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8748 | `			/* Only interfaces is allowed */` |
|   11545 |  8749 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8750 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8751 | `			}` |
|   11545 |  8752 | `			if( pBase == 0 ){` |
|     ! 0 |  8753 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8754 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8755 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8756 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8757 | `					return SXERR_ABORT;` |
|       - |  8758 | `				}` |
|     ! 0 |  8759 | `			}` |
|   11545 |  8760 | `			SyBlobRelease(&sResolved);` |
|    5770 |  8761 | `		}` |
|    5770 |  8762 | `	}` |
|   42373 |  8763 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8764 | `		/* Syntax error */` |
|     ! 0 |  8765 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8766 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8767 | `		if( rc == SXERR_ABORT ){` |
|       - |  8768 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8769 | `			return SXERR_ABORT;` |
|       - |  8770 | `		}` |
|     ! 0 |  8771 | `		return SXRET_OK;` |
|       - |  8772 | `	}` |
|   42373 |  8773 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   42373 |  8774 | `	pEnd = 0; /* cc warning */` |
|       - |  8775 | `	/* Delimit the interface body */` |
|   42373 |  8776 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   42373 |  8777 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8778 | `		/* Syntax error */` |
|     ! 0 |  8779 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8780 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8781 | `		if( rc == SXERR_ABORT ){` |
|       - |  8782 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8783 | `			return SXERR_ABORT;` |
|       - |  8784 | `		}` |
|     ! 0 |  8785 | `		return SXRET_OK;` |
|       - |  8786 | `	}` |
|       - |  8787 | `	/* Swap token stream */` |
|   42373 |  8788 | `	pTmp = pGen->pEnd;` |
|   42373 |  8789 | `	pGen->pEnd = pEnd;` |
|       - |  8790 | `	/* Start the parse process` |
|       - |  8791 | `	 * Note (According to the PHP reference manual):` |
|       - |  8792 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8793 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8794 | `	 */` |
|   71180 |  8795 | `	for(;;){` |
|       - |  8796 | `		/* Jump leading/trailing semi-colons */` |
|  242357 |  8797 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   99997 |  8798 | `			pGen->pIn++;` |
|       5 |  8799 | `		}` |
|  142365 |  8800 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8801 | `			/* End of interface body */` |
|   42369 |  8802 | `			break;` |
|       - |  8803 | `		}` |
|  100001 |  8804 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8805 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8806 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8807 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8808 | `			if( rc == SXERR_ABORT ){` |
|       - |  8809 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8810 | `				return SXERR_ABORT;` |
|       - |  8811 | `			}` |
|     ! 0 |  8812 | `			goto done;` |
|       - |  8813 | `		}` |
|       - |  8814 | `		/* Extract the current keyword */` |
|  100001 |  8815 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  100001 |  8816 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8817 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8818 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8819 | `			const char *zKind = "member";` |
|       3 |  8820 | `			SyString *pMemberName = 0;` |
|       3 |  8821 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8822 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8823 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8824 | `					zKind = "constant";` |
|       3 |  8825 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8826 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8827 | `					}` |
|       1 |  8828 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8829 | `					zKind = "method";` |
|     ! 0 |  8830 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8831 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8832 | `					}` |
|     ! 0 |  8833 | `				}` |
|       1 |  8834 | `			}` |
|       3 |  8835 | `			if( pMemberName ){` |
|       4 |  8836 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8837 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8838 | `			}else{` |
|     ! 0 |  8839 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8840 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8841 | `			}` |
|       3 |  8842 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8843 | `				return SXERR_ABORT;` |
|       - |  8844 | `			}` |
|       3 |  8845 | `			goto done;` |
|       - |  8846 | `		}` |
|   99999 |  8847 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8848 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8849 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8850 | `			if( rc == SXERR_ABORT ){` |
|       - |  8851 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8852 | `				return SXERR_ABORT;` |
|       - |  8853 | `			}` |
|     ! 0 |  8854 | `			goto done;` |
|       - |  8855 | `		}` |
|   99999 |  8856 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8857 | `			/* Advance the stream cursor */` |
|   99987 |  8858 | `			pGen->pIn++;` |
|   99987 |  8859 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8860 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8861 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8862 | `				if( rc == SXERR_ABORT ){` |
|       - |  8863 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8864 | `					return SXERR_ABORT;` |
|       - |  8865 | `				}` |
|     ! 0 |  8866 | `				goto done;` |
|       - |  8867 | `			}` |
|   99987 |  8868 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   99987 |  8869 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8870 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8871 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8872 | `				if( rc == SXERR_ABORT ){` |
|       - |  8873 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8874 | `					return SXERR_ABORT;` |
|       - |  8875 | `				}` |
|     ! 0 |  8876 | `				goto done;` |
|       - |  8877 | `			}` |
|   49991 |  8878 | `		}` |
|   99999 |  8879 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8880 | `			/* Parse constant */` |
|      10 |  8881 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|      10 |  8882 | `			if( rc != SXRET_OK ){` |
|       3 |  8883 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8884 | `					return SXERR_ABORT;` |
|       - |  8885 | `				}` |
|       3 |  8886 | `				goto done;` |
|       - |  8887 | `			}` |
|       4 |  8888 | `		}else{` |
|   99991 |  8889 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   99991 |  8890 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8891 | `				/* Static method,record that */` |
|   11537 |  8892 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8893 | `				/* Advance the stream cursor */` |
|   11537 |  8894 | `				pGen->pIn++;` |
|   11532 |  8895 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   11537 |  8896 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8897 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8898 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8899 | `						if( rc == SXERR_ABORT ){` |
|       - |  8900 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8901 | `							return SXERR_ABORT;` |
|       - |  8902 | `						}` |
|     ! 0 |  8903 | `						goto done;` |
|       - |  8904 | `				}` |
|    5766 |  8905 | `			}` |
|       - |  8906 | `			/* Process method signature (no body for interface methods) */` |
|   99991 |  8907 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   99991 |  8908 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8909 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8910 | `					return SXERR_ABORT;` |
|       - |  8911 | `				}` |
|     ! 0 |  8912 | `				goto done;` |
|       - |  8913 | `			}` |
|       - |  8914 | `		}` |
|       5 |  8915 | `	}` |
|       - |  8916 | `	/* Install the interface */` |
|   42369 |  8917 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   42369 |  8918 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8919 | `		/* Inherit from the base interface */` |
|   11545 |  8920 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5770 |  8921 | `	}` |
|   42369 |  8922 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8923 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8924 | `		return SXERR_ABORT;` |
|       - |  8925 | `	}` |
|   21182 |  8926 | `done:` |
|       - |  8927 | `	/* Point beyond the interface body */` |
|   42373 |  8928 | `	pGen->pIn  = &pEnd[1];` |
|   42373 |  8929 | `	pGen->pEnd = pTmp;` |
|   42373 |  8930 | `	return PH7_OK;` |
|   21189 |  8931 | `}` |
|       - |  8932 | `/*` |
|       - |  8933 | ` * Compile a user-defined class.` |
|       - |  8934 | ` * According to the PHP language reference manual` |
|       - |  8935 | ` *  class` |
|       - |  8936 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8937 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8938 | ` *  of the properties and methods belonging to the class.` |
|       - |  8939 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8940 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8941 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8942 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8943 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8944 | ` *  (called "methods").` |
|       - |  8945 | ` */` |
|       - |  8946 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8947 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8948 | `struct TraitUseEntry {` |
|       - |  8949 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8950 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8951 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8952 | `};` |
|       - |  8953 | `/*` |
|       - |  8954 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8955 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8956 | ` */` |
|  112776 |  8957 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8958 | `{` |
|       - |  8959 | `	ph7_class **apIface;` |
|       - |  8960 | `	sxu32 nIface,i;` |
|       - |  8961 | `	sxi32 rc;` |
|  112781 |  8962 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8963 | `		return SXRET_OK;` |
|       - |  8964 | `	}` |
|  112781 |  8965 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  112781 |  8966 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  216831 |  8967 | `	for(i = 0; i < nIface; i++){` |
|  104055 |  8968 | `		ph7_class *pIface = apIface[i];` |
|       - |  8969 | `		SyHashEntry *pEntry;` |
|  104055 |  8970 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  277509 |  8971 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  173459 |  8972 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8973 | `			ph7_class_method *pImplMeth;` |
|  173459 |  8974 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8975 | `			/* Find the implementing method in the class */` |
|  173459 |  8976 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  173459 |  8977 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8978 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8979 | `			}` |
|       - |  8980 | `			/* Check visibility: interface methods must be implemented as public */` |
|  173445 |  8981 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8982 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8983 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8984 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8985 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8986 | `					return SXERR_ABORT;` |
|       - |  8987 | `				}` |
|       1 |  8988 | `			}` |
|       - |  8989 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8990 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8991 | `			 */` |
|       - |  8992 | `			{` |
|  173445 |  8993 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  173445 |  8994 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  173445 |  8995 | `				int sigError = 0;` |
|  173445 |  8996 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8997 | `					sigError = 1;` |
|  173444 |  8998 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8999 | `					/* Extra parameters must all have default values */` |
|       6 |  9000 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  9001 | `					sxu32 k;` |
|       8 |  9002 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  9003 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  9004 | `							sigError = 1;` |
|       3 |  9005 | `							break;` |
|       - |  9006 | `						}` |
|       2 |  9007 | `					}` |
|       2 |  9008 | `				}` |
|  173445 |  9009 | `				if( sigError ){` |
|       - |  9010 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  9011 | `					ph7_vm_func_arg *aArgs;` |
|       - |  9012 | `					sxu32 j;` |
|       6 |  9013 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  9014 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  9015 | `					/* Build implementing method signature */` |
|       6 |  9016 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  9017 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  9018 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  9019 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  9020 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  9021 | `					}` |
|       - |  9022 | `					/* Build interface method signature */` |
|       6 |  9023 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  9024 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  9025 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  9026 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  9027 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  9028 | `					}` |
|       8 |  9029 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  9030 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  9031 | `						&pClass->sName,pMName,` |
|       4 |  9032 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  9033 | `						&pIface->sName,pMName,` |
|       4 |  9034 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  9035 | `					SyBlobRelease(&sImplSig);` |
|       6 |  9036 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  9037 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9038 | `						return SXERR_ABORT;` |
|       - |  9039 | `					}` |
|       2 |  9040 | `				}` |
|       - |  9041 | `			}` |
|       5 |  9042 | `		}` |
|   52030 |  9043 | `	}` |
|  112781 |  9044 | `	return SXRET_OK;` |
|   56393 |  9045 | `}` |
|       - |  9046 | `/*` |
|       - |  9047 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  9048 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  9049 | ` */` |
|  112776 |  9050 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  9051 | `{` |
|       - |  9052 | `	ph7_class_method *pMeth;` |
|       - |  9053 | `	SyHashEntry *pEntry;` |
|       - |  9054 | `	sxu32 nAbstract;` |
|       - |  9055 | `	SyBlob sMsg;` |
|       - |  9056 | `	sxi32 rc;` |
|       - |  9057 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  112781 |  9058 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      37 |  9059 | `		return SXRET_OK;` |
|       - |  9060 | `	}` |
|       - |  9061 | `	/* Count abstract methods */` |
|  112749 |  9062 | `	nAbstract = 0;` |
|  112749 |  9063 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
| 1060175 |  9064 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  947431 |  9065 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  947431 |  9066 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  9067 | `			nAbstract++;` |
|       8 |  9068 | `		}` |
|       5 |  9069 | `	}` |
|  112749 |  9070 | `	if( nAbstract == 0 ){` |
|  112735 |  9071 | `		return SXRET_OK;` |
|       - |  9072 | `	}` |
|       - |  9073 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  9074 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  9075 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  9076 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  9077 | `		&pClass->sName,nAbstract,` |
|       7 |  9078 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  9079 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  9080 | `	/* Second pass: list methods with origins */` |
|       - |  9081 | `	{` |
|      18 |  9082 | `		sxu32 nListed = 0;` |
|      18 |  9083 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  9084 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  9085 | `			ph7_class *pOrigin = 0;` |
|       - |  9086 | `			SyString *pMName;` |
|      22 |  9087 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  9088 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  9089 | `				continue;` |
|       - |  9090 | `			}` |
|      20 |  9091 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  9092 | `			if( nListed > 0 ){` |
|       3 |  9093 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  9094 | `			}` |
|       - |  9095 | `			/* Find the origin of this abstract method.` |
|       - |  9096 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  9097 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  9098 | `			 * methods. Abstract class methods only win when the class` |
|       - |  9099 | `			 * itself declared the abstract method (not inherited from` |
|       - |  9100 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  9101 | `			 * class's namespace.` |
|       - |  9102 | `			 */` |
|       - |  9103 | `			{` |
|       - |  9104 | `				ph7_class **apIface;` |
|       - |  9105 | `				ph7_class **apTrait;` |
|       - |  9106 | `				ph7_class *pWalk;` |
|       - |  9107 | `				sxu32 i;` |
|       - |  9108 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  9109 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  9110 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  9111 | `				 */` |
|      20 |  9112 | `				if( pClass->pBase ){` |
|      11 |  9113 | `					pWalk = pClass->pBase;` |
|      19 |  9114 | `					while( pWalk ){` |
|       - |  9115 | `						ph7_class_method *pParentMeth;` |
|      13 |  9116 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  9117 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  9118 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  9119 | `							 * in this class's ancestor chain.` |
|       - |  9120 | `							 */` |
|      13 |  9121 | `							int fromIface = 0;` |
|      13 |  9122 | `							ph7_class *pAnc = pWalk;` |
|      17 |  9123 | `							while( pAnc ){` |
|       - |  9124 | `								ph7_class **apPI;` |
|       - |  9125 | `								sxu32 j;` |
|      15 |  9126 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  9127 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  9128 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  9129 | `										fromIface = 1;` |
|      10 |  9130 | `										break;` |
|       - |  9131 | `									}` |
|     ! 0 |  9132 | `								}` |
|      15 |  9133 | `								if( fromIface ) break;` |
|       6 |  9134 | `								pAnc = pAnc->pBase;` |
|       2 |  9135 | `							}` |
|      13 |  9136 | `							if( !fromIface ){` |
|       3 |  9137 | `								pOrigin = pWalk;` |
|       3 |  9138 | `								break;` |
|       - |  9139 | `							}` |
|       4 |  9140 | `						}` |
|      10 |  9141 | `						pWalk = pWalk->pBase;` |
|       2 |  9142 | `					}` |
|       4 |  9143 | `				}` |
|       - |  9144 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  9145 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  9146 | `				 */` |
|      20 |  9147 | `				if( !pOrigin ){` |
|      18 |  9148 | `					pWalk = pClass;` |
|      40 |  9149 | `					while( pWalk && !pOrigin ){` |
|      26 |  9150 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  9151 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  9152 | `							ph7_class *pIface = apIface[i];` |
|      16 |  9153 | `							ph7_class *pDeepest = 0;` |
|      28 |  9154 | `							while( pIface ){` |
|      16 |  9155 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  9156 | `									pDeepest = pIface;` |
|       6 |  9157 | `								}` |
|      16 |  9158 | `								pIface = pIface->pBase;` |
|       4 |  9159 | `							}` |
|      16 |  9160 | `							if( pDeepest ){` |
|      16 |  9161 | `								pOrigin = pDeepest;` |
|      16 |  9162 | `								break;` |
|       - |  9163 | `							}` |
|     ! 0 |  9164 | `						}` |
|      26 |  9165 | `						pWalk = pWalk->pBase;` |
|       4 |  9166 | `					}` |
|       7 |  9167 | `				}` |
|       - |  9168 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  9169 | `				if( !pOrigin ){` |
|       3 |  9170 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  9171 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  9172 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  9173 | `							pOrigin = pClass;` |
|       3 |  9174 | `							break;` |
|       - |  9175 | `						}` |
|     ! 0 |  9176 | `					}` |
|       1 |  9177 | `				}` |
|       - |  9178 | `			}` |
|      20 |  9179 | `			if( pOrigin ){` |
|      20 |  9180 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  9181 | `			}else{` |
|       - |  9182 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  9183 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  9184 | `			}` |
|      20 |  9185 | `			nListed++;` |
|       4 |  9186 | `		}` |
|       - |  9187 | `	}` |
|      18 |  9188 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  9189 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  9190 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  9191 | `	SyBlobRelease(&sMsg);` |
|      18 |  9192 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9193 | `		return SXERR_ABORT;` |
|       - |  9194 | `	}` |
|      18 |  9195 | `	return SXRET_OK;` |
|   56393 |  9196 | `}` |
|       - |  9197 | `/*` |
|       - |  9198 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  9199 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  9200 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  9201 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  9202 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  9203 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  9204 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  9205 | ` */` |
|  109166 |  9206 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  9207 | `{` |
|  109171 |  9208 | `	int isAbsolute = 0;` |
|  109171 |  9209 | `	SyToken *pStart = pGen->pIn;` |
|       - |  9210 | `	SyBlob sName;` |
|  109171 |  9211 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|    4373 |  9212 | `		isAbsolute = 1;` |
|    4373 |  9213 | `		pGen->pIn++;` |
|    2184 |  9214 | `	}` |
|  109171 |  9215 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       8 |  9216 | `		pGen->pIn = pStart;` |
|       8 |  9217 | `		return SXERR_INVALID;` |
|       - |  9218 | `	}` |
|  109165 |  9219 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|  109165 |  9220 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|  109165 |  9221 | `	pGen->pIn++;` |
|  163761 |  9222 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   54606 |  9223 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      16 |  9224 | `		SyBlobAppend(&sName,"\\",1);` |
|      16 |  9225 | `		pGen->pIn++;` |
|      16 |  9226 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      16 |  9227 | `		pGen->pIn++;` |
|       2 |  9228 | `	}` |
|  109165 |  9229 | `	if( isAbsolute ){` |
|    4371 |  9230 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    2188 |  9231 | `	}else{` |
|       - |  9232 | `		SyString sRaw;` |
|  104799 |  9233 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|  104799 |  9234 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  9235 | `	}` |
|  109165 |  9236 | `	SyBlobRelease(&sName);` |
|  109165 |  9237 | `	return SXRET_OK;` |
|   54588 |  9238 | `}` |
|       - |  9239 | `/*` |
|       - |  9240 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  9241 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  9242 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  9243 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  9244 | ` * either direction cannot run unbounded.` |
|       - |  9245 | ` */` |
|       - |  9246 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   11712 |  9247 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  9248 | `{` |
|       - |  9249 | `	ph7_class **apParent;` |
|       - |  9250 | `	sxu32 n;` |
|   19619 |  9251 | `	while( pInterface ){` |
|   15605 |  9252 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  9253 | `			return FALSE;` |
|       - |  9254 | `		}` |
|   19464 |  9255 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7718 |  9256 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7703 |  9257 | `			return TRUE;` |
|       - |  9258 | `		}` |
|    7907 |  9259 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7907 |  9260 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  9261 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  9262 | `				return TRUE;` |
|       - |  9263 | `			}` |
|     ! 0 |  9264 | `		}` |
|    7907 |  9265 | `		pInterface = pInterface->pBase;` |
|    7907 |  9266 | `		iDepth++;` |
|       5 |  9267 | `	}` |
|    4019 |  9268 | `	return FALSE;` |
|    5861 |  9269 | `}` |
|   11712 |  9270 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  9271 | `{` |
|   11717 |  9272 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  9273 | `}` |
|       - |  9274 | `/*` |
|       - |  9275 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  9276 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  9277 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  9278 | ` */` |
|    7698 |  9279 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  9280 | `{` |
|    7707 |  9281 | `	while( pBase ){` |
|      10 |  9282 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  9283 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  9284 | `			return TRUE;` |
|       - |  9285 | `		}` |
|      10 |  9286 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  9287 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  9288 | `			return TRUE;` |
|       - |  9289 | `		}` |
|       5 |  9290 | `		pBase = pBase->pBase;` |
|       1 |  9291 | `	}` |
|    7699 |  9292 | `	return FALSE;` |
|    3854 |  9293 | `}` |
|       - |  9294 | `/*` |
|       - |  9295 | ` * Compile a class declaration, named or anonymous.` |
|       - |  9296 | ` *` |
|       - |  9297 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  9298 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  9299 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  9300 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  9301 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  9302 | ` * implements, body, install) is shared by both paths.` |
|       - |  9303 | ` */` |
|  112816 |  9304 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  9305 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  9306 | `{` |
|  112821 |  9307 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9308 | `	ph7_class *pClass,*pBase;` |
|       - |  9309 | `	SyToken *pEnd,*pTmp;` |
|       - |  9310 | `	sxi32 iProtection;` |
|       - |  9311 | `	SySet aInterfaces;` |
|       - |  9312 | `	SySet aUseEntries;` |
|       - |  9313 | `	sxi32 iAttrflags;` |
|       - |  9314 | `	SyString *pName;` |
|       - |  9315 | `	sxi32 nKwrd;` |
|       - |  9316 | `	sxi32 rc;` |
|       - |  9317 | `	/* Jump the 'class' keyword */` |
|  112821 |  9318 | `	pGen->pIn++;` |
|  112821 |  9319 | `	if( pAnonName ){` |
|       - |  9320 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  9321 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  9322 | `		 * then use the synthesized name. */` |
|      30 |  9323 | `		*ppArgStart = *ppArgEnd = 0;` |
|      30 |  9324 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  9325 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  9326 | `			*ppArgStart = pGen->pIn;` |
|      10 |  9327 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  9328 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  9329 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  9330 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  9331 | `		}` |
|      30 |  9332 | `		pName = pAnonName;` |
|      30 |  9333 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      17 |  9334 | `	}else{` |
|  112795 |  9335 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  9336 | `			/* Syntax error */` |
|     ! 0 |  9337 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  9338 | `			if( rc == SXERR_ABORT ){` |
|       - |  9339 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  9340 | `				return SXERR_ABORT;` |
|       - |  9341 | `			}` |
|       - |  9342 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  9343 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  9344 | `				pGen->pIn++;` |
|     ! 0 |  9345 | `			}` |
|     ! 0 |  9346 | `			return SXRET_OK;` |
|       - |  9347 | `		}` |
|       - |  9348 | `		/* Extract class name */` |
|  112795 |  9349 | `		pName = &pGen->pIn->sData;` |
|       - |  9350 | `		/* Advance the stream cursor */` |
|  112795 |  9351 | `		pGen->pIn++;` |
|       - |  9352 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  9353 | `			SyBlob sFQN;` |
|       - |  9354 | `			SyString sFQNStr;` |
|  112795 |  9355 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  112795 |  9356 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  112795 |  9357 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  112795 |  9358 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  112795 |  9359 | `			SyBlobRelease(&sFQN);` |
|       - |  9360 | `		}` |
|       - |  9361 | `	}` |
|  112821 |  9362 | `	if( pClass == 0 ){` |
|     ! 0 |  9363 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9364 | `		return SXERR_ABORT;` |
|       - |  9365 | `	}` |
|       - |  9366 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  112821 |  9367 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  112821 |  9368 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  9369 | `	/* Assume a standalone class */` |
|  112821 |  9370 | `	pBase = 0;` |
|  112821 |  9371 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   96455 |  9372 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   96455 |  9373 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  9374 | `			SyBlob sResolved;` |
|       - |  9375 | `			SyString sBaseName;` |
|       - |  9376 | `			sxu32 nRefLine;` |
|   84763 |  9377 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   84763 |  9378 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   84763 |  9379 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   84763 |  9380 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  9381 | `				SyBlobRelease(&sResolved);` |
|       4 |  9382 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  9383 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  9384 | `					pName);` |
|       3 |  9385 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  9386 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9387 | `					return SXERR_ABORT;` |
|       - |  9388 | `				}` |
|       3 |  9389 | `				return SXRET_OK;` |
|       - |  9390 | `			}` |
|  127139 |  9391 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   84756 |  9392 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   84761 |  9393 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  9394 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  9395 | `			/* Interfaces are not allowed */` |
|   84761 |  9396 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  9397 | `				pBase = pBase->pNextName;` |
|     ! 0 |  9398 | `			}` |
|   84761 |  9399 | `			if( pBase == 0 ){` |
|     ! 0 |  9400 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  9401 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  9402 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9403 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  9404 | `					return SXERR_ABORT;` |
|       - |  9405 | `				}` |
|     ! 0 |  9406 | `			}else{` |
|   84761 |  9407 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  9408 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  9409 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  9410 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9411 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  9412 | `						return SXERR_ABORT;` |
|       - |  9413 | `					}` |
|     ! 0 |  9414 | `				}` |
|       - |  9415 | `			}` |
|   84761 |  9416 | `			SyBlobRelease(&sResolved);` |
|   42378 |  9417 | `		}` |
|   96453 |  9418 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  9419 | `			ph7_class *pInterface;` |
|       - |  9420 | `			/* Interface implementation */` |
|   11705 |  9421 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5862 |  9422 | `			for(;;){` |
|       - |  9423 | `				SyBlob sResolved;` |
|       - |  9424 | `				SyString sIntName;` |
|       - |  9425 | `				sxu32 nRefLine;` |
|   11717 |  9426 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11717 |  9427 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11717 |  9428 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  9429 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  9430 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  9431 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  9432 | `						pName);` |
|     ! 0 |  9433 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9434 | `						return SXERR_ABORT;` |
|       - |  9435 | `					}` |
|     ! 0 |  9436 | `					break;` |
|       - |  9437 | `				}` |
|   23429 |  9438 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   11712 |  9439 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11717 |  9440 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  9441 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  9442 | `				/* Only interfaces are allowed */` |
|   11717 |  9443 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  9444 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  9445 | `				}` |
|   11717 |  9446 | `				if( pInterface == 0 ){` |
|     ! 0 |  9447 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  9448 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  9449 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9450 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  9451 | `						return SXERR_ABORT;` |
|       - |  9452 | `					}` |
|     ! 0 |  9453 | `				}else{` |
|       - |  9454 | `					/* Reject user classes that try to implement Throwable` |
|       - |  9455 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  9456 | `					 * unless they already extend Exception or Error.` |
|       - |  9457 | `					 * Exception and Error themselves are compiled from the` |
|       - |  9458 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  9459 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   11717 |  9460 | `					SyString *pFqn = &pClass->sName;` |
|   11717 |  9461 | `					int bIsExceptionOrError =` |
|    9704 |  9462 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   19494 |  9463 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9797 |  9464 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3858 |  9465 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   15561 |  9466 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   11550 |  9467 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3847 |  9468 | `						!bIsExceptionOrError ){` |
|      12 |  9469 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9470 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  9471 | `							&pClass->sName);` |
|       9 |  9472 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9473 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  9474 | `							return SXERR_ABORT;` |
|       - |  9475 | `						}` |
|       - |  9476 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  9477 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  9478 | `					}else{` |
|   11711 |  9479 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  9480 | `					}` |
|       - |  9481 | `				}` |
|   11717 |  9482 | `				SyBlobRelease(&sResolved);` |
|   11717 |  9483 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5855 |  9484 | `					break;` |
|       - |  9485 | `				}` |
|      16 |  9486 | `				pGen->pIn++;/* Jump the comma */` |
|       4 |  9487 | `			}` |
|    5850 |  9488 | `		}` |
|   48224 |  9489 | `	}` |
|  112819 |  9490 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  9491 | `		/* Syntax error */` |
|     ! 0 |  9492 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  9493 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9494 | `		if( rc == SXERR_ABORT ){` |
|       - |  9495 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9496 | `			return SXERR_ABORT;` |
|       - |  9497 | `		}` |
|     ! 0 |  9498 | `		return SXRET_OK;` |
|       - |  9499 | `	}` |
|  112819 |  9500 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  112819 |  9501 | `	pEnd = 0; /* cc warning */` |
|       - |  9502 | `	/* Delimit the class body */` |
|  112819 |  9503 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  112819 |  9504 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  9505 | `		/* Syntax error */` |
|     ! 0 |  9506 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  9507 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9508 | `		if( rc == SXERR_ABORT ){` |
|       - |  9509 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9510 | `			return SXERR_ABORT;` |
|       - |  9511 | `		}` |
|     ! 0 |  9512 | `		return SXRET_OK;` |
|       - |  9513 | `	}` |
|       - |  9514 | `	/* Swap token stream */` |
|  112819 |  9515 | `	pTmp = pGen->pEnd;` |
|  112819 |  9516 | `	pGen->pEnd = pEnd;` |
|       - |  9517 | `	/* Set the inherited flags */` |
|  112819 |  9518 | `	pClass->iFlags = iFlags;` |
|       - |  9519 | `	/* Start the parse process */` |
|  151172 |  9520 | `	for(;;){` |
|       - |  9521 | `		/* Jump leading/trailing semi-colons */` |
|  465343 |  9522 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   81543 |  9523 | `			pGen->pIn++;` |
|       5 |  9524 | `		}` |
|  383805 |  9525 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9526 | `			/* End of class body */` |
|  112781 |  9527 | `			break;` |
|       - |  9528 | `		}` |
|  271024 |  9529 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  135517 |  9530 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  9531 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9532 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9533 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9534 | `			if( rc == SXERR_ABORT ){` |
|       - |  9535 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  9536 | `				return SXERR_ABORT;` |
|       - |  9537 | `			}` |
|     ! 0 |  9538 | `			goto done;` |
|       - |  9539 | `		}` |
|       - |  9540 | `		/* Assume public visibility */` |
|  271029 |  9541 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  271029 |  9542 | `		iAttrflags = 0;` |
|       - |  9543 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  9544 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  9545 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  9546 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  271029 |  9547 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9548 | `			int bMod = 0;` |
|     ! 0 |  9549 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9550 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  9551 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  9552 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  9553 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  9554 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  9555 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  9556 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  9557 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  9558 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  9559 | `			}` |
|     ! 0 |  9560 | `			if( !bMod ){` |
|     ! 0 |  9561 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9562 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9563 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9564 | `						return SXERR_ABORT;` |
|       - |  9565 | `					}` |
|     ! 0 |  9566 | `					goto done;` |
|       - |  9567 | `				}` |
|     ! 0 |  9568 | `				continue;` |
|       - |  9569 | `			}` |
|     ! 0 |  9570 | `		}` |
|  271029 |  9571 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9572 | `			/* Extract the current keyword */` |
|  271029 |  9573 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  271029 |  9574 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9575 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  9576 | `				TraitUseEntry sUse;` |
|      57 |  9577 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      57 |  9578 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      57 |  9579 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      34 |  9580 | `				for(;;){` |
|       - |  9581 | `					ph7_class *pTrait;` |
|       - |  9582 | `					SyString *pTraitName;` |
|      65 |  9583 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9584 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9585 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  9586 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9587 | `							return SXERR_ABORT;` |
|       - |  9588 | `						}` |
|     ! 0 |  9589 | `						break;` |
|       - |  9590 | `					}` |
|      65 |  9591 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  9592 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  9593 | `						SyBlob sResolved;` |
|      65 |  9594 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      65 |  9595 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     125 |  9596 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      60 |  9597 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      65 |  9598 | `						SyBlobRelease(&sResolved);` |
|       - |  9599 | `					}` |
|       - |  9600 | `					/* Only traits are allowed */` |
|      65 |  9601 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9602 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  9603 | `					}` |
|      65 |  9604 | `					if( pTrait == 0 ){` |
|     ! 0 |  9605 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9606 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  9607 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9608 | `							return SXERR_ABORT;` |
|       - |  9609 | `						}` |
|     ! 0 |  9610 | `					}else{` |
|      65 |  9611 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  9612 | `					}` |
|      65 |  9613 | `					pGen->pIn++; /* Advance past trait name */` |
|      65 |  9614 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      31 |  9615 | `						break;` |
|       - |  9616 | `					}` |
|      10 |  9617 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  9618 | `				}` |
|       - |  9619 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      57 |  9620 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  9621 | `					SyToken *pBlock;` |
|      13 |  9622 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  9623 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  9624 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  9625 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  9626 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  9627 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  9628 | `					}else{` |
|     ! 0 |  9629 | `						pGen->pIn = pGen->pEnd;` |
|       - |  9630 | `					}` |
|       5 |  9631 | `				}` |
|      57 |  9632 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  9633 | `				/* The semicolon will be consumed by the outer loop */` |
|      57 |  9634 | `				continue;` |
|       - |  9635 | `			}` |
|  270977 |  9636 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  270631 |  9637 | `				iProtection = nKwrd;` |
|  270631 |  9638 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  9639 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  270631 |  9640 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  9641 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  9642 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  9643 | `				}` |
|  270626 |  9644 | `				if( pGen->pIn >= pGen->pEnd` |
|  270631 |  9645 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9646 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9647 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9648 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9649 | `					if( rc == SXERR_ABORT ){` |
|       - |  9650 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  9651 | `						return SXERR_ABORT;` |
|       - |  9652 | `					}` |
|     ! 0 |  9653 | `					goto done;` |
|       - |  9654 | `				}` |
|  270631 |  9655 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9656 | `					/* Attribute declaration (untyped) */` |
|   81225 |  9657 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   81225 |  9658 | `					if( rc != SXRET_OK ){` |
|      11 |  9659 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9660 | `							return SXERR_ABORT;` |
|       - |  9661 | `						}` |
|      11 |  9662 | `						goto done;` |
|       - |  9663 | `					}` |
|   81217 |  9664 | `					continue;` |
|       - |  9665 | `				}` |
|  189411 |  9666 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9667 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     175 |  9668 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     175 |  9669 | `					if( rc != SXRET_OK ){` |
|       8 |  9670 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9671 | `							return SXERR_ABORT;` |
|       - |  9672 | `						}` |
|       8 |  9673 | `						goto done;` |
|       - |  9674 | `					}` |
|     169 |  9675 | `					continue;` |
|       - |  9676 | `				}` |
|       - |  9677 | `				/* Extract the keyword */` |
|  189241 |  9678 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   94618 |  9679 | `			}` |
|  189587 |  9680 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  9681 | `				/* Process constant declaration */` |
|      87 |  9682 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      87 |  9683 | `				if( rc != SXRET_OK ){` |
|      11 |  9684 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9685 | `						return SXERR_ABORT;` |
|       - |  9686 | `					}` |
|      11 |  9687 | `					goto done;` |
|       - |  9688 | `				}` |
|      42 |  9689 | `			}else{` |
|  189505 |  9690 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  9691 | `					/* Static method or attribute,record that */` |
|   11611 |  9692 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   11611 |  9693 | `					pGen->pIn++; /* Jump the static keyword */` |
|   11611 |  9694 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9695 | `						/* Extract the keyword */` |
|   11599 |  9696 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   11599 |  9697 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9698 | `							iProtection = nKwrd;` |
|     ! 0 |  9699 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  9700 | `						}` |
|    5797 |  9701 | `					}` |
|       - |  9702 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  9703 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  9704 | `					 * than a generic "expecting method" parse error. */` |
|   11611 |  9705 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9706 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9707 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  9708 | `					}` |
|   11606 |  9709 | `					if( pGen->pIn >= pGen->pEnd` |
|   11611 |  9710 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9711 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9712 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  9713 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9714 | `						if( rc == SXERR_ABORT ){` |
|       - |  9715 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9716 | `							return SXERR_ABORT;` |
|       - |  9717 | `						}` |
|     ! 0 |  9718 | `						goto done;` |
|       - |  9719 | `					}` |
|   11611 |  9720 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9721 | `						/* Attribute declaration */` |
|      13 |  9722 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      13 |  9723 | `						if( rc != SXRET_OK ){` |
|       3 |  9724 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9725 | `								return SXERR_ABORT;` |
|       - |  9726 | `							}` |
|       3 |  9727 | `							goto done;` |
|       - |  9728 | `						}` |
|      10 |  9729 | `						continue;` |
|       - |  9730 | `					}` |
|   11601 |  9731 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9732 | `						/* Typed static attribute declaration */` |
|      15 |  9733 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  9734 | `						if( rc != SXRET_OK ){` |
|       3 |  9735 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9736 | `								return SXERR_ABORT;` |
|       - |  9737 | `							}` |
|       3 |  9738 | `							goto done;` |
|       - |  9739 | `						}` |
|      13 |  9740 | `						continue;` |
|       - |  9741 | `					}` |
|       - |  9742 | `					/* Extract the keyword */` |
|   11589 |  9743 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  183691 |  9744 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  9745 | `					/* Abstract method,record that */` |
|      15 |  9746 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9747 | `					/* Mark the whole class as abstract */` |
|      15 |  9748 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9749 | `					/* Advance the stream cursor */` |
|      15 |  9750 | `					pGen->pIn++;` |
|      15 |  9751 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      15 |  9752 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      15 |  9753 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      13 |  9754 | `							iProtection = nKwrd;` |
|      13 |  9755 | `							pGen->pIn++; /* Jump the visibility token */` |
|       5 |  9756 | `						}` |
|       6 |  9757 | `					}` |
|      15 |  9758 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      12 |  9759 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9760 | `							/* Static method */` |
|     ! 0 |  9761 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9762 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9763 | `					}` |
|      15 |  9764 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      12 |  9765 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9766 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9767 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9768 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9769 | `							if( rc == SXERR_ABORT ){` |
|       - |  9770 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9771 | `								return SXERR_ABORT;` |
|       - |  9772 | `							}` |
|     ! 0 |  9773 | `							goto done;` |
|       - |  9774 | `					}` |
|      15 |  9775 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  177893 |  9776 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9777 | `					/* final method ,record that */` |
|      17 |  9778 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  9779 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  9780 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9781 | `						/* Extract the keyword */` |
|      17 |  9782 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  9783 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  9784 | `							iProtection = nKwrd;` |
|       9 |  9785 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9786 | `						}` |
|       7 |  9787 | `					}` |
|      17 |  9788 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9789 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9790 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9791 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9792 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9793 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9794 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9795 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9796 | `									return SXERR_ABORT;` |
|       - |  9797 | `								}` |
|     ! 0 |  9798 | `								goto done;` |
|       - |  9799 | `							}` |
|      12 |  9800 | `							continue;` |
|       - |  9801 | `					}` |
|       6 |  9802 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9803 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9804 | `							/* Static method */` |
|     ! 0 |  9805 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9806 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9807 | `					}` |
|       6 |  9808 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9809 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9810 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9811 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9812 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9813 | `							if( rc == SXERR_ABORT ){` |
|       - |  9814 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9815 | `								return SXERR_ABORT;` |
|       - |  9816 | `							}` |
|     ! 0 |  9817 | `							goto done;` |
|       - |  9818 | `					}` |
|       6 |  9819 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9820 | `				}` |
|  189473 |  9821 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9822 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9823 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9824 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9825 | `						if( rc == SXERR_ABORT ){` |
|       - |  9826 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9827 | `							return SXERR_ABORT;` |
|       - |  9828 | `						}` |
|     ! 0 |  9829 | `						goto done;` |
|       - |  9830 | `				}` |
|  189473 |  9831 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9832 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9833 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9834 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9835 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9836 | `						if( rc == SXERR_ABORT ){` |
|       - |  9837 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9838 | `							return SXERR_ABORT;` |
|       - |  9839 | `						}` |
|     ! 0 |  9840 | `						goto done;` |
|       - |  9841 | `					}` |
|       - |  9842 | `					/* Attribute declaration */` |
|       7 |  9843 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9844 | `				}else{` |
|       - |  9845 | `					/* Process method declaration */` |
|  189467 |  9846 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9847 | `				}` |
|  189473 |  9848 | `				if( rc != SXRET_OK ){` |
|      16 |  9849 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9850 | `						return SXERR_ABORT;` |
|       - |  9851 | `					}` |
|      16 |  9852 | `					goto done;` |
|       - |  9853 | `				}` |
|       - |  9854 | `			}` |
|   94770 |  9855 | `		}else{` |
|       - |  9856 | `			/* Attribute declaration */` |
|     ! 0 |  9857 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9858 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9859 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9860 | `					return SXERR_ABORT;` |
|       - |  9861 | `				}` |
|     ! 0 |  9862 | `				goto done;` |
|       - |  9863 | `			}` |
|       - |  9864 | `		}` |
|       5 |  9865 | `	}` |
|       - |  9866 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9867 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9868 | `	 */` |
|       - |  9869 | `	{` |
|       - |  9870 | `		TraitUseEntry *apUse;` |
|       - |  9871 | `		sxu32 nU;` |
|  112781 |  9872 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  112833 |  9873 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      57 |  9874 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      57 |  9875 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      57 |  9876 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      57 |  9877 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9878 | `			sxu32 nT;` |
|      57 |  9879 | `			if( !hasResolution ){` |
|       - |  9880 | `				/* No conflict resolution block: use standard trait application */` |
|      95 |  9881 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      53 |  9882 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      53 |  9883 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9884 | `						break;` |
|       - |  9885 | `					}` |
|      29 |  9886 | `				}` |
|      26 |  9887 | `			}else{` |
|       - |  9888 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9889 | `				 * then use the block to resolve method conflicts.` |
|       - |  9890 | `				 */` |
|       - |  9891 | `				SyToken *pR;` |
|      25 |  9892 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9893 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9894 | `					ph7_class_attr *pAR;` |
|       - |  9895 | `					SyHashEntry *pER;` |
|       - |  9896 | `					SyString *pNR;` |
|      15 |  9897 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9898 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9899 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9900 | `						pNR = &pAR->sName;` |
|     ! 0 |  9901 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9902 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9903 | `						}` |
|     ! 0 |  9904 | `					}` |
|      15 |  9905 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9906 | `				}` |
|       - |  9907 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9908 | `				pR = pUse->pResolvStart;` |
|      27 |  9909 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9910 | `					SyString sTrait,sMethod;` |
|       - |  9911 | `					ph7_class *pSrcTrait;` |
|       - |  9912 | `					ph7_class_method *pMeth;` |
|       - |  9913 | `					sxi32 nRKwrd;` |
|      41 |  9914 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9915 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9916 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9917 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9918 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9919 | `					sMethod = pR->sData;` |
|      17 |  9920 | `					pR++;` |
|      17 |  9921 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9922 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9923 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9924 | `							sTrait = sMethod;` |
|       7 |  9925 | `							pR++;` |
|       7 |  9926 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9927 | `							sMethod = pR->sData;` |
|       7 |  9928 | `							pR++;` |
|       3 |  9929 | `						}` |
|       3 |  9930 | `					}` |
|      17 |  9931 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9932 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9933 | `						continue;` |
|       - |  9934 | `					}` |
|      17 |  9935 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9936 | `					pR++;` |
|      17 |  9937 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9938 | `						pSrcTrait = 0;` |
|       7 |  9939 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9940 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9941 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9942 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9943 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9944 | `								break;` |
|       - |  9945 | `							}` |
|       2 |  9946 | `						}` |
|       5 |  9947 | `						if( pSrcTrait ){` |
|       5 |  9948 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9949 | `							if( pMeth ){` |
|       5 |  9950 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9951 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9952 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9953 | `								}` |
|       2 |  9954 | `							}` |
|       2 |  9955 | `						}` |
|       2 |  9956 | `					}` |
|      35 |  9957 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9958 | `				}` |
|       - |  9959 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9960 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9961 | `					ph7_class_method *pMR;` |
|       - |  9962 | `					SyHashEntry *pER;` |
|       - |  9963 | `					SyString *pNR;` |
|      15 |  9964 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9965 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9966 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9967 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9968 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9969 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9970 | `						}` |
|       3 |  9971 | `					}` |
|       9 |  9972 | `				}` |
|       - |  9973 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9974 | `				pR = pUse->pResolvStart;` |
|      27 |  9975 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9976 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9977 | `					ph7_class *pSrcTrait;` |
|       - |  9978 | `					ph7_class_method *pMeth;` |
|      27 |  9979 | `					int hasQual = 0;` |
|       - |  9980 | `					sxi32 nRKwrd;` |
|      41 |  9981 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9982 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9983 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9984 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9985 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9986 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9987 | `					sMethod = pR->sData;` |
|      17 |  9988 | `					pR++;` |
|      17 |  9989 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9990 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9991 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9992 | `							sTrait = sMethod;` |
|       7 |  9993 | `							hasQual = 1;` |
|       7 |  9994 | `							pR++;` |
|       7 |  9995 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9996 | `							sMethod = pR->sData;` |
|       7 |  9997 | `							pR++;` |
|       3 |  9998 | `						}` |
|       3 |  9999 | `					}` |
|      17 | 10000 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 10001 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 10002 | `						continue;` |
|       - | 10003 | `					}` |
|      17 | 10004 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 | 10005 | `					pR++;` |
|      17 | 10006 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 | 10007 | `						sxi32 iNewVis = -1;` |
|      13 | 10008 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 10009 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 10010 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 10011 | `								iNewVis = nAK;` |
|       7 | 10012 | `								pR++;` |
|       3 | 10013 | `							}` |
|       3 | 10014 | `						}` |
|      13 | 10015 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 | 10016 | `							sAlias = pR->sData;` |
|      11 | 10017 | `							pR++;` |
|       4 | 10018 | `						}` |
|      13 | 10019 | `						pMeth = 0;` |
|      13 | 10020 | `						if( hasQual ){` |
|       3 | 10021 | `							pSrcTrait = 0;` |
|       5 | 10022 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 10023 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 10024 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 10025 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 10026 | `									pSrcTrait = apTrait[nT];` |
|       3 | 10027 | `									break;` |
|       - | 10028 | `								}` |
|       2 | 10029 | `							}` |
|       3 | 10030 | `							if( pSrcTrait ){` |
|       3 | 10031 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 10032 | `							}` |
|       2 | 10033 | `						}else{` |
|      10 | 10034 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 10035 | `						}` |
|      13 | 10036 | `						if( pMeth ){` |
|      13 | 10037 | `							if( sAlias.nByte > 0 ){` |
|       - | 10038 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 10039 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 10040 | `								 */` |
|       - | 10041 | `								ph7_class_method *pAlias;` |
|       - | 10042 | `								char *zAliasDup;` |
|      11 | 10043 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 | 10044 | `								if( pAlias ){` |
|      11 | 10045 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 | 10046 | `									if( iNewVis >= 0 ){` |
|       5 | 10047 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 10048 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 10049 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 10050 | `									}` |
|      11 | 10051 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 | 10052 | `									if( zAliasDup ){` |
|      11 | 10053 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 | 10054 | `									}` |
|       7 | 10055 | `								}` |
|       7 | 10056 | `							}else if( iNewVis >= 0 ){` |
|       - | 10057 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 10058 | `								ph7_class_method *pCopy;` |
|       3 | 10059 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 10060 | `								if( pCopy ){` |
|       3 | 10061 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 10062 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 10063 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 10064 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 10065 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 10066 | `									/* Replace the method in the class hash */` |
|       3 | 10067 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 10068 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 10069 | `								}` |
|       1 | 10070 | `							}` |
|       5 | 10071 | `						}` |
|       5 | 10072 | `						SXUNUSED(hasQual);` |
|       5 | 10073 | `					}` |
|      21 | 10074 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 | 10075 | `				}` |
|       - | 10076 | `			}` |
|      57 | 10077 | `			SySetRelease(&pUse->aTraits);` |
|      31 | 10078 | `		}` |
|       - | 10079 | `	}` |
|       - | 10080 | `	/* Install the class */` |
|  112781 | 10081 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  112781 | 10082 | `	if( rc == SXRET_OK ){` |
|       - | 10083 | `		ph7_class **apInterface;` |
|       - | 10084 | `		sxu32 n;` |
|  112781 | 10085 | `		if( pBase ){` |
|       - | 10086 | `			/* Inherit from base class and mark as a subclass */` |
|   84761 | 10087 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   42378 | 10088 | `		}` |
|  112781 | 10089 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  124487 | 10090 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 10091 | `			/* Implements one or more interface */` |
|   11711 | 10092 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   11711 | 10093 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10094 | `				break;` |
|       - | 10095 | `			}` |
|    5858 | 10096 | `		}` |
|       - | 10097 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - | 10098 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  112776 | 10099 | `		if( rc == SXRET_OK` |
|  112776 | 10100 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  112781 | 10101 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   92351 | 10102 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - | 10103 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   92351 | 10104 | `			if( pStringable ){` |
|   92351 | 10105 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   92351 | 10106 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - | 10107 | `				sxu32 i;` |
|   92351 | 10108 | `				int bAlready = 0;` |
|  100043 | 10109 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7699 | 10110 | `					if( apImpl[i] == pStringable ){` |
|       3 | 10111 | `						bAlready = 1;` |
|       3 | 10112 | `						break;` |
|       - | 10113 | `					}` |
|    3851 | 10114 | `				}` |
|   92351 | 10115 | `				if( !bAlready ){` |
|   92349 | 10116 | `					PH7_ClassImplement(pClass,pStringable);` |
|   46172 | 10117 | `				}` |
|   46173 | 10118 | `			}` |
|   46173 | 10119 | `		}` |
|       - | 10120 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  112781 | 10121 | `		if( rc == SXRET_OK ){` |
|  112781 | 10122 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  112781 | 10123 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 10124 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 10125 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 10126 | `				return SXERR_ABORT;` |
|       - | 10127 | `			}` |
|   56388 | 10128 | `		}` |
|       - | 10129 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  112781 | 10130 | `		if( rc == SXRET_OK ){` |
|  112781 | 10131 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  112781 | 10132 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 10133 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 10134 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 10135 | `				return SXERR_ABORT;` |
|       - | 10136 | `			}` |
|   56388 | 10137 | `		}` |
|   56388 | 10138 | `	}` |
|  112781 | 10139 | `	SySetRelease(&aUseEntries);` |
|  112781 | 10140 | `	SySetRelease(&aInterfaces);` |
|  112781 | 10141 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10142 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10143 | `		return SXERR_ABORT;` |
|       - | 10144 | `	}` |
|   56388 | 10145 | `done:` |
|       - | 10146 | `	/* Point beyond the class body */` |
|  112819 | 10147 | `	pGen->pIn = &pEnd[1];` |
|  112819 | 10148 | `	pGen->pEnd = pTmp;` |
|  112819 | 10149 | `	return PH7_OK;` |
|   56413 | 10150 | `}` |
|       - | 10151 | `/* Compile a named class declaration (the common case). */` |
|  112790 | 10152 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 | 10153 | `{` |
|  112795 | 10154 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 | 10155 | `}` |
|       - | 10156 | `/*` |
|       - | 10157 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - | 10158 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - | 10159 | ` * compile + install the class body once (at compile time, like every other` |
|       - | 10160 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - | 10161 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - | 10162 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - | 10163 | ` */` |
|      26 | 10164 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 | 10165 | `{` |
|       - | 10166 | `	char zName[128];         /* Synthesized class name */` |
|       - | 10167 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - | 10168 | `	SyString sName;` |
|       - | 10169 | `	SyToken *pArgStart,*pArgEnd;` |
|       - | 10170 | `	ph7_value *pObj;` |
|      30 | 10171 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10172 | `	sxu32 nIdx,nLen;` |
|       - | 10173 | `	sxi32 nArg,rc;` |
|      13 | 10174 | `	SXUNUSED(iCompileFlag);` |
|       - | 10175 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      30 | 10176 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      30 | 10177 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 10178 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 | 10179 | `	}` |
|      30 | 10180 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - | 10181 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - | 10182 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - | 10183 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      30 | 10184 | `	pArgStart = pArgEnd = 0;` |
|      30 | 10185 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      30 | 10186 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10187 | `		return rc;` |
|       - | 10188 | `	}` |
|       - | 10189 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - | 10190 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      30 | 10191 | `	nArg = 0;` |
|      30 | 10192 | `	if( pArgStart < pArgEnd ){` |
|       7 | 10193 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 | 10194 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 10195 | `		SyToken *pArgNext;` |
|       7 | 10196 | `		pGen->pIn = pArgStart;` |
|       7 | 10197 | `		pGen->pEnd = pArgEnd;` |
|      13 | 10198 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 | 10199 | `			if( pGen->pIn < pArgNext ){` |
|       7 | 10200 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 | 10201 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10202 | `					pGen->pIn = pSavedIn;` |
|     ! 0 | 10203 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 | 10204 | `					return SXERR_ABORT;` |
|       - | 10205 | `				}` |
|       7 | 10206 | `				nArg++;` |
|       3 | 10207 | `			}` |
|       7 | 10208 | `			pGen->pIn = &pArgNext[1];` |
|       1 | 10209 | `		}` |
|       7 | 10210 | `		pGen->pIn = pSavedIn;` |
|       7 | 10211 | `		pGen->pEnd = pSavedEnd;` |
|       3 | 10212 | `	}` |
|       - | 10213 | `	/* Load the synthesized class name */` |
|      30 | 10214 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      30 | 10215 | `	if( pObj == 0 ){` |
|     ! 0 | 10216 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10217 | `		return SXERR_ABORT;` |
|       - | 10218 | `	}` |
|      30 | 10219 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      30 | 10220 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 10221 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      30 | 10222 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      30 | 10223 | `	return SXRET_OK;` |
|      17 | 10224 | `}` |
|       - | 10225 | `/*` |
|       - | 10226 | ` * Compile a user-defined abstract class.` |
|       - | 10227 | ` *  According to the PHP language reference manual` |
|       - | 10228 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 10229 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 10230 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 10231 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 10232 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 10233 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 10234 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 10235 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 10236 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 10237 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 10238 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 10239 | ` *   could differ.` |
|       - | 10240 | ` */` |
|       - | 10241 | `/*` |
|       - | 10242 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - | 10243 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - | 10244 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - | 10245 | ` */` |
| 1139264 | 10246 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 | 10247 | `{` |
| 1139269 | 10248 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  782211 | 10249 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  782211 | 10250 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  774501 | 10251 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  387217 | 10252 | `	}` |
| 1131497 | 10253 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 1131437 | 10254 | `	return FALSE;` |
|  569637 | 10255 | `}` |
|       - | 10256 | `/*` |
|       - | 10257 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - | 10258 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - | 10259 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - | 10260 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - | 10261 | ` */` |
| 1131432 | 10262 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 | 10263 | `{` |
| 1131437 | 10264 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 1131437 | 10265 | `	sxi32 iFlags = 0,iFlag;` |
| 1139269 | 10266 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7837 | 10267 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 | 10268 | `			pDup = pIn;` |
|       2 | 10269 | `		}` |
|    7837 | 10270 | `		iFlags \|= iFlag;` |
|    7837 | 10271 | `		pIn++;` |
|       5 | 10272 | `	}` |
| 1131437 | 10273 | `	*ppIn = pIn;` |
| 1131437 | 10274 | `	if( ppDup ){ *ppDup = pDup; }` |
| 1131437 | 10275 | `	return iFlags;` |
|       5 | 10276 | `}` |
|       - | 10277 | `/*` |
|       - | 10278 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - | 10279 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - | 10280 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - | 10281 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - | 10282 | `` * `readonly`) to their existing handlers.`` |
|       - | 10283 | ` */` |
| 1127526 | 10284 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 | 10285 | `{` |
| 1127531 | 10286 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  567676 | 10287 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 1129481 | 10288 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 | 10289 | `}` |
|       - | 10290 | `/*` |
|       - | 10291 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - | 10292 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - | 10293 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - | 10294 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - | 10295 | `` * `abstract`+`final` pair, like PHP.`` |
|       - | 10296 | ` */` |
|    3906 | 10297 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 | 10298 | `{` |
|       - | 10299 | `	SyToken *pDup;` |
|    3911 | 10300 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - | 10301 | `	sxi32 rc;` |
|    3911 | 10302 | `	if( pDup ){` |
|       4 | 10303 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 | 10304 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 | 10305 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10306 | `			return SXERR_ABORT;` |
|       - | 10307 | `		}` |
|       1 | 10308 | `	}` |
|    3906 | 10309 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1958 | 10310 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 | 10311 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10312 | `			"Cannot use the final modifier on an abstract class");` |
|       3 | 10313 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10314 | `			return SXERR_ABORT;` |
|       - | 10315 | `		}` |
|       1 | 10316 | `	}` |
|    3911 | 10317 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1958 | 10318 | `}` |
|       - | 10319 | `/*` |
|       - | 10320 | ` * Compile a user-defined trait.` |
|       - | 10321 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 10322 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 10323 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 10324 | ` */` |
|      64 | 10325 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 | 10326 | `{` |
|      69 | 10327 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10328 | `	ph7_class *pClass;` |
|       - | 10329 | `	SyToken *pEnd,*pTmp;` |
|       - | 10330 | `	sxi32 iProtection;` |
|       - | 10331 | `	sxi32 iAttrflags;` |
|       - | 10332 | `	SyString *pName;` |
|       - | 10333 | `	sxi32 nKwrd;` |
|       - | 10334 | `	sxi32 rc;` |
|       - | 10335 | `	/* Jump the 'trait' keyword */` |
|      69 | 10336 | `	pGen->pIn++;` |
|      69 | 10337 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 10338 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 10339 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10340 | `			return SXERR_ABORT;` |
|       - | 10341 | `		}` |
|     ! 0 | 10342 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 10343 | `			pGen->pIn++;` |
|     ! 0 | 10344 | `		}` |
|     ! 0 | 10345 | `		return SXRET_OK;` |
|       - | 10346 | `	}` |
|       - | 10347 | `	/* Extract trait name */` |
|      69 | 10348 | `	pName = &pGen->pIn->sData;` |
|      69 | 10349 | `	pGen->pIn++;` |
|       - | 10350 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 10351 | `		SyBlob sFQN;` |
|       - | 10352 | `		SyString sFQNStr;` |
|      69 | 10353 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      69 | 10354 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      69 | 10355 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      69 | 10356 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      69 | 10357 | `		SyBlobRelease(&sFQN);` |
|       - | 10358 | `	}` |
|      69 | 10359 | `	if( pClass == 0 ){` |
|     ! 0 | 10360 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10361 | `		return SXERR_ABORT;` |
|       - | 10362 | `	}` |
|       - | 10363 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      69 | 10364 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 10365 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 10366 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 10367 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10368 | `			return SXERR_ABORT;` |
|       - | 10369 | `		}` |
|     ! 0 | 10370 | `		return SXRET_OK;` |
|       - | 10371 | `	}` |
|      69 | 10372 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      69 | 10373 | `	pEnd = 0;` |
|      69 | 10374 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      69 | 10375 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 10376 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 10377 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 10378 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10379 | `			return SXERR_ABORT;` |
|       - | 10380 | `		}` |
|     ! 0 | 10381 | `		return SXRET_OK;` |
|       - | 10382 | `	}` |
|       - | 10383 | `	/* Swap token stream */` |
|      69 | 10384 | `	pTmp = pGen->pEnd;` |
|      69 | 10385 | `	pGen->pEnd = pEnd;` |
|       - | 10386 | `	/* Mark as trait */` |
|      69 | 10387 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 10388 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      64 | 10389 | `	for(;;){` |
|     177 | 10390 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 | 10391 | `			pGen->pIn++;` |
|       4 | 10392 | `		}` |
|     153 | 10393 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      69 | 10394 | `			break;` |
|       - | 10395 | `		}` |
|      89 | 10396 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 10397 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10398 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 10399 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 10400 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10401 | `				return SXERR_ABORT;` |
|       - | 10402 | `			}` |
|     ! 0 | 10403 | `			goto done;` |
|       - | 10404 | `		}` |
|      89 | 10405 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      89 | 10406 | `		iAttrflags = 0;` |
|      89 | 10407 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      89 | 10408 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      89 | 10409 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 10410 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 10411 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 10412 | `				for(;;){` |
|       - | 10413 | `					ph7_class *pUsedTrait;` |
|       - | 10414 | `					SyString *pUsedName;` |
|       5 | 10415 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 10416 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10417 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 10418 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10419 | `							return SXERR_ABORT;` |
|       - | 10420 | `						}` |
|     ! 0 | 10421 | `						break;` |
|       - | 10422 | `					}` |
|       5 | 10423 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 10424 | `					{` |
|       - | 10425 | `						SyBlob sResolved;` |
|       5 | 10426 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 10427 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 10428 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 10429 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 10430 | `						SyBlobRelease(&sResolved);` |
|       - | 10431 | `					}` |
|       5 | 10432 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 10433 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 10434 | `					}` |
|       5 | 10435 | `					if( pUsedTrait == 0 ){` |
|       4 | 10436 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 10437 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 10438 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10439 | `							return SXERR_ABORT;` |
|       - | 10440 | `						}` |
|       2 | 10441 | `					}else{` |
|       3 | 10442 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 10443 | `					}` |
|       5 | 10444 | `					pGen->pIn++;` |
|       5 | 10445 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 10446 | `						break;` |
|       - | 10447 | `					}` |
|     ! 0 | 10448 | `					pGen->pIn++;` |
|     ! 0 | 10449 | `				}` |
|       5 | 10450 | `				continue;` |
|       - | 10451 | `			}` |
|      85 | 10452 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 | 10453 | `				iProtection = nKwrd;` |
|      73 | 10454 | `				pGen->pIn++;` |
|      68 | 10455 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 | 10456 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10457 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10458 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 10459 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10460 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10461 | `						return SXERR_ABORT;` |
|       - | 10462 | `					}` |
|     ! 0 | 10463 | `					goto done;` |
|       - | 10464 | `				}` |
|      73 | 10465 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 | 10466 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 | 10467 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10468 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10469 | `							return SXERR_ABORT;` |
|       - | 10470 | `						}` |
|     ! 0 | 10471 | `						goto done;` |
|       - | 10472 | `					}` |
|      12 | 10473 | `					continue;` |
|       - | 10474 | `				}` |
|      63 | 10475 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 | 10476 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 10477 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10478 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10479 | `							return SXERR_ABORT;` |
|       - | 10480 | `						}` |
|     ! 0 | 10481 | `						goto done;` |
|       - | 10482 | `					}` |
|       5 | 10483 | `					continue;` |
|       - | 10484 | `				}` |
|      58 | 10485 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 | 10486 | `			}` |
|      71 | 10487 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 10488 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10489 | `					"Traits cannot have constants");` |
|     ! 0 | 10490 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10491 | `					return SXERR_ABORT;` |
|       - | 10492 | `				}` |
|     ! 0 | 10493 | `				goto done;` |
|     ! 0 | 10494 | `			}else{` |
|      71 | 10495 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 10496 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 10497 | `					pGen->pIn++;` |
|       5 | 10498 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 10499 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 10500 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 10501 | `							iProtection = nKwrd;` |
|     ! 0 | 10502 | `							pGen->pIn++;` |
|     ! 0 | 10503 | `						}` |
|       1 | 10504 | `					}` |
|       4 | 10505 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 | 10506 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10507 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10508 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 10509 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10510 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10511 | `							return SXERR_ABORT;` |
|       - | 10512 | `						}` |
|     ! 0 | 10513 | `						goto done;` |
|       - | 10514 | `					}` |
|       5 | 10515 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 10516 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 10517 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10518 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10519 | `								return SXERR_ABORT;` |
|       - | 10520 | `							}` |
|     ! 0 | 10521 | `							goto done;` |
|       - | 10522 | `						}` |
|       3 | 10523 | `						continue;` |
|       - | 10524 | `					}` |
|       3 | 10525 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 | 10526 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10527 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10528 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10529 | `								return SXERR_ABORT;` |
|       - | 10530 | `							}` |
|     ! 0 | 10531 | `							goto done;` |
|       - | 10532 | `						}` |
|     ! 0 | 10533 | `						continue;` |
|       - | 10534 | `					}` |
|       3 | 10535 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      68 | 10536 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 | 10537 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 | 10538 | `					pGen->pIn++;` |
|       6 | 10539 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 | 10540 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 | 10541 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 10542 | `							iProtection = nKwrd;` |
|       6 | 10543 | `							pGen->pIn++;` |
|       2 | 10544 | `						}` |
|       2 | 10545 | `					}` |
|       6 | 10546 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 10547 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 10548 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10549 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 10550 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10551 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10552 | `							return SXERR_ABORT;` |
|       - | 10553 | `						}` |
|     ! 0 | 10554 | `						goto done;` |
|       - | 10555 | `					}` |
|       6 | 10556 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 10557 | `				}` |
|      69 | 10558 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 10559 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10560 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 10561 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10562 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10563 | `						return SXERR_ABORT;` |
|       - | 10564 | `					}` |
|     ! 0 | 10565 | `					goto done;` |
|       - | 10566 | `				}` |
|      69 | 10567 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 10568 | `					pGen->pIn++;` |
|     ! 0 | 10569 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 10570 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10571 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 10572 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10573 | `							return SXERR_ABORT;` |
|       - | 10574 | `						}` |
|     ! 0 | 10575 | `						goto done;` |
|       - | 10576 | `					}` |
|     ! 0 | 10577 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10578 | `				}else{` |
|      69 | 10579 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 10580 | `				}` |
|      69 | 10581 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10582 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10583 | `						return SXERR_ABORT;` |
|       - | 10584 | `					}` |
|     ! 0 | 10585 | `					goto done;` |
|       - | 10586 | `				}` |
|       - | 10587 | `			}` |
|      37 | 10588 | `		}else{` |
|     ! 0 | 10589 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10590 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10591 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10592 | `					return SXERR_ABORT;` |
|       - | 10593 | `				}` |
|     ! 0 | 10594 | `				goto done;` |
|       - | 10595 | `			}` |
|       - | 10596 | `		}` |
|       5 | 10597 | `	}` |
|       - | 10598 | `	/* Install the trait */` |
|      69 | 10599 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      69 | 10600 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10601 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10602 | `		return SXERR_ABORT;` |
|       - | 10603 | `	}` |
|      32 | 10604 | `done:` |
|       - | 10605 | `	/* Point beyond the trait body */` |
|      69 | 10606 | `	pGen->pIn = &pEnd[1];` |
|      69 | 10607 | `	pGen->pEnd = pTmp;` |
|      69 | 10608 | `	return PH7_OK;` |
|      37 | 10609 | `}` |
|       - | 10610 | `/*` |
|       - | 10611 | ` * Compile a user-defined class.` |
|       - | 10612 | ` *  According to the PHP language reference manual` |
|       - | 10613 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 10614 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 10615 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 10616 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 10617 | ` *   and functions (called "methods").` |
|       - | 10618 | ` */` |
|  108884 | 10619 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 | 10620 | `{` |
|       - | 10621 | `	sxi32 rc;` |
|  108889 | 10622 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|  108889 | 10623 | `	return rc;` |
|       5 | 10624 | `}` |
|       - | 10625 | `/*` |
|       - | 10626 | ` * Exception handling.` |
|       - | 10627 | ` *  According to the PHP language reference manual` |
|       - | 10628 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 10629 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 10630 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 10631 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 10632 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 10633 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 10634 | ` *    (or re-thrown) within a catch block.` |
|       - | 10635 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 10636 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 10637 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 10638 | ` *    been defined with set_exception_handler().` |
|       - | 10639 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 10640 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 10641 | ` */` |
|       - | 10642 | `/*` |
|       - | 10643 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 10644 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 10645 | ` * indicates failure.` |
|       - | 10646 | ` */` |
|   38814 | 10647 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 10648 | `{` |
|   38819 | 10649 | `	sxi32 rc = SXRET_OK;` |
|   38819 | 10650 | `	if( pRoot->pOp ){` |
|   38807 | 10651 | `		switch( pRoot->pOp->iOp ){` |
|   19401 | 10652 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - | 10653 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - | 10654 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - | 10655 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - | 10656 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - | 10657 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   38807 | 10658 | `			break;` |
|     ! 0 | 10659 | `		default:` |
|       - | 10660 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - | 10661 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - | 10662 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 | 10663 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10664 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 10665 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 10666 | `				rc = SXERR_INVALID;` |
|     ! 0 | 10667 | `			}` |
|     ! 0 | 10668 | `			break;` |
|       - | 10669 | `		}` |
|   19418 | 10670 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 10671 | `		/* Unexpected expression */` |
|     ! 0 | 10672 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10673 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10674 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 10675 | `			rc = SXERR_INVALID;` |
|     ! 0 | 10676 | `		}` |
|     ! 0 | 10677 | `	}` |
|   38819 | 10678 | `	return rc;` |
|       5 | 10679 | `}` |
|       - | 10680 | `/*` |
|       - | 10681 | ` * Compile a 'throw' statement.` |
|       - | 10682 | ` * throw: This is how you trigger an exception.` |
|       - | 10683 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 10684 | ` */` |
|   38778 | 10685 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 10686 | `{` |
|   38783 | 10687 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10688 | `	GenBlock *pBlock;` |
|       - | 10689 | `	sxu32 nIdx;` |
|       - | 10690 | `	sxi32 rc;` |
|   38783 | 10691 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 10692 | `	/* Compile the expression */` |
|   38783 | 10693 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   38783 | 10694 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10695 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 10696 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10697 | `			return SXERR_ABORT;` |
|       - | 10698 | `		}` |
|     ! 0 | 10699 | `		return SXRET_OK;` |
|       - | 10700 | `	}` |
|   38783 | 10701 | `	pBlock = pGen->pCurrent;` |
|       - | 10702 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  185209 | 10703 | `	while(pBlock->pParent){` |
|  185205 | 10704 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   38779 | 10705 | `			break;` |
|       - | 10706 | `		}` |
|       - | 10707 | `		/* Point to the parent block */` |
|  146431 | 10708 | `		pBlock = pBlock->pParent;` |
|       5 | 10709 | `	}` |
|       - | 10710 | `	/* Emit the throw instruction */` |
|   38783 | 10711 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 10712 | `	/* Emit the jump */` |
|   38783 | 10713 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   38783 | 10714 | `	return SXRET_OK;` |
|   19394 | 10715 | `}` |
|       - | 10716 | `/*` |
|       - | 10717 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 10718 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 10719 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 10720 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 10721 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 10722 | ` */` |
|      36 | 10723 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 10724 | `{` |
|      38 | 10725 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10726 | `	GenBlock *pBlock;` |
|       - | 10727 | `	sxu32 nIdx;` |
|       - | 10728 | `	sxi32 rc;` |
|      18 | 10729 | `	(void)iCompileFlag;` |
|      38 | 10730 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 | 10731 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 10732 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10733 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10734 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10735 | `			return SXERR_ABORT;` |
|       - | 10736 | `		}` |
|     ! 0 | 10737 | `		return SXRET_OK;` |
|       - | 10738 | `	}` |
|      38 | 10739 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 | 10740 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10741 | `		return SXERR_ABORT;` |
|       - | 10742 | `	}` |
|      38 | 10743 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10744 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10745 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10746 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10747 | `			return SXERR_ABORT;` |
|       - | 10748 | `		}` |
|     ! 0 | 10749 | `		return SXRET_OK;` |
|       - | 10750 | `	}` |
|       - | 10751 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10752 | `	pBlock = pGen->pCurrent;` |
|      60 | 10753 | `	while( pBlock->pParent ){` |
|      49 | 10754 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10755 | `			break;` |
|       - | 10756 | `		}` |
|      23 | 10757 | `		pBlock = pBlock->pParent;` |
|       1 | 10758 | `	}` |
|      38 | 10759 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10760 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10761 | `	return SXRET_OK;` |
|      20 | 10762 | `}` |
|       - | 10763 | `/*` |
|       - | 10764 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|       - | 10765 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|       - | 10766 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|       - | 10767 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|       - | 10768 | ` * compile error propagated from the parser.` |
|       - | 10769 | ` */` |
|      46 | 10770 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|       4 | 10771 | `{` |
|       - | 10772 | `	SyString sClassName;` |
|       - | 10773 | `	SyToken *pToken;` |
|       - | 10774 | `	SyString *pName;` |
|       - | 10775 | `	char *zDup;` |
|       - | 10776 | `	sxi32 rc;` |
|      50 | 10777 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|      50 | 10778 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|      50 | 10779 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|      50 | 10780 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      50 | 10781 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 10782 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10783 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10784 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10785 | `		return SXERR_INVALID;` |
|       - | 10786 | `	}` |
|      50 | 10787 | `	pGen->pIn++; /* '(' */` |
|      23 | 10788 | `	for(;;){` |
|       - | 10789 | `		SyBlob sResolved;` |
|      50 | 10790 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      50 | 10791 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 10792 | `			SyBlobRelease(&sResolved);` |
|     ! 0 | 10793 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10794 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10795 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10796 | `			return SXERR_INVALID;` |
|       - | 10797 | `		}` |
|      73 | 10798 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      46 | 10799 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      50 | 10800 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|      50 | 10801 | `		SyBlobRelease(&sResolved);` |
|      50 | 10802 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|      50 | 10803 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|      50 | 10804 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      46 | 10805 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|       4 | 10806 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     ! 0 | 10807 | `			pGen->pIn++; continue;` |
|       - | 10808 | `		}` |
|      50 | 10809 | `		break;` |
|     ! 0 | 10810 | `	}` |
|      46 | 10811 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|      50 | 10812 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 10813 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10814 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10815 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10816 | `		return SXERR_INVALID;` |
|       - | 10817 | `	}` |
|      50 | 10818 | `	pGen->pIn++; /* '$' */` |
|      50 | 10819 | `	pName = &pGen->pIn->sData;` |
|      50 | 10820 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      50 | 10821 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|      50 | 10822 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|      50 | 10823 | `	pGen->pIn++;` |
|      50 | 10824 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 10825 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10826 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10827 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10828 | `		return SXERR_INVALID;` |
|       - | 10829 | `	}` |
|      50 | 10830 | `	pGen->pIn++; /* ')' */` |
|      50 | 10831 | `	return SXRET_OK;` |
|      27 | 10832 | `}` |
|       - | 10833 | `/*` |
|       - | 10834 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|       - | 10835 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|       - | 10836 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|       - | 10837 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|       - | 10838 | ` * VmThrowException):` |
|       - | 10839 | ` *` |
|       - | 10840 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|       - | 10841 | ` *    <try body>` |
|       - | 10842 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|       - | 10843 | ` *    JMP  -> finally\|end` |
|       - | 10844 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|       - | 10845 | ` *    <catch body>` |
|       - | 10846 | ` *    JMP  -> finally\|end` |
|       - | 10847 | ` *    ... more catches ...` |
|       - | 10848 | ` *  Lfin: <finally body>` |
|       - | 10849 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|       - | 10850 | ` *  Lend:` |
|       - | 10851 | ` */` |
|      90 | 10852 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|       4 | 10853 | `{` |
|      94 | 10854 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10855 | `	GenBlock *pTry;` |
|       - | 10856 | `	VmInstr *pInstr;` |
|      94 | 10857 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|       - | 10858 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|       - | 10859 | `	sxi32 rc;` |
|      94 | 10860 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|       - | 10861 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      94 | 10862 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      94 | 10863 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      94 | 10864 | `	pTry->pUserData = pException;` |
|      94 | 10865 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      94 | 10866 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      94 | 10867 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      94 | 10868 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      94 | 10869 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      94 | 10870 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10871 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      94 | 10872 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      94 | 10873 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      94 | 10874 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      94 | 10875 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10876 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      94 | 10877 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|       - | 10878 | `	/* Catch clauses (inline) */` |
|      94 | 10879 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      90 | 10880 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|      50 | 10881 | `		sxu32 k = 0;` |
|      69 | 10882 | `		for(;;){` |
|       - | 10883 | `			ph7_exception_block sCatch;` |
|       - | 10884 | `			GenBlock *pCatchBlk;` |
|      96 | 10885 | `			sxu32 idxJmp = 0;` |
|      92 | 10886 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      88 | 10887 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      27 | 10888 | `				break;` |
|       - | 10889 | `			}` |
|      50 | 10890 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|      50 | 10891 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      50 | 10892 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|      50 | 10893 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|      50 | 10894 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|      50 | 10895 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|      50 | 10896 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       - | 10897 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|       - | 10898 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|       - | 10899 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|      50 | 10900 | `			pCatchBlk->pUserData = pException;` |
|      50 | 10901 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      50 | 10902 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      50 | 10903 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      50 | 10904 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10905 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|       - | 10906 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|      50 | 10907 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      50 | 10908 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|      50 | 10909 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|      50 | 10910 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      50 | 10911 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      50 | 10912 | `			k++;` |
|       4 | 10913 | `		}` |
|      23 | 10914 | `	}` |
|       - | 10915 | `	/* Finally (inline) */` |
|      94 | 10916 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      74 | 10917 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10918 | `		GenBlock *pFinBlk;` |
|      52 | 10919 | `		pGen->pIn++; /* Jump 'finally' */` |
|      52 | 10920 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|      52 | 10921 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|      52 | 10922 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      52 | 10923 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      52 | 10924 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      52 | 10925 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      52 | 10926 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      52 | 10927 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|      52 | 10928 | `		pException->iHasFinally = 1;` |
|      24 | 10929 | `	}` |
|      94 | 10930 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      94 | 10931 | `	pException->iInlined = 1;` |
|       - | 10932 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|       - | 10933 | `	{` |
|      94 | 10934 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|       - | 10935 | `		sxu32 *aJ; sxu32 n;` |
|      94 | 10936 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      94 | 10937 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      94 | 10938 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|     140 | 10939 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|      50 | 10940 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|      50 | 10941 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      27 | 10942 | `		}` |
|       - | 10943 | `	}` |
|      94 | 10944 | `	SySetRelease(&aCatchJmp);` |
|      94 | 10945 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|     ! 0 | 10946 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|     ! 0 | 10947 | `	}` |
|      94 | 10948 | `	return SXRET_OK;` |
|      49 | 10949 | `}` |
|       - | 10950 | `/*` |
|       - | 10951 | ` * Compile a 'catch' block.` |
|       - | 10952 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10953 | ` * an object containing the exception information.` |
|       - | 10954 | ` */` |
|    1082 | 10955 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10956 | `{` |
|    1087 | 10957 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10958 | `	ph7_exception_block sCatch;` |
|       - | 10959 | `	SySet *pInstrContainer;` |
|       - | 10960 | `	SyString sClassName;` |
|       - | 10961 | `	GenBlock *pCatch;` |
|       - | 10962 | `	SyToken *pToken;` |
|       - | 10963 | `	SyString *pName;` |
|       - | 10964 | `	char *zDup;` |
|       - | 10965 | `	sxi32 rc;` |
|    1087 | 10966 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10967 | `	/* Zero the structure */` |
|    1087 | 10968 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10969 | `	/* Initialize fields */` |
|    1087 | 10970 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|    1087 | 10971 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    1087 | 10972 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10973 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10974 | `			pToken = pGen->pIn;` |
|     ! 0 | 10975 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10976 | `				pToken--;` |
|     ! 0 | 10977 | `			}` |
|     ! 0 | 10978 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10979 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10980 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10981 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10982 | `				return SXERR_ABORT;` |
|       - | 10983 | `			}` |
|     ! 0 | 10984 | `			return SXERR_INVALID;` |
|       - | 10985 | `	}` |
|       - | 10986 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    1087 | 10987 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     555 | 10988 | `	for(;;){` |
|       - | 10989 | `		SyBlob sResolved;` |
|    1115 | 10990 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    1115 | 10991 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10992 | `			SyBlobRelease(&sResolved);` |
|       6 | 10993 | `			pToken = pGen->pIn;` |
|       6 | 10994 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10995 | `				pToken--;` |
|     ! 0 | 10996 | `			}` |
|       8 | 10997 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10998 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10999 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 11000 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11001 | `				return SXERR_ABORT;` |
|       - | 11002 | `			}` |
|       6 | 11003 | `			return SXERR_INVALID;` |
|       - | 11004 | `		}` |
|       - | 11005 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 11006 | `		 * transient SyBlob allocation. */` |
|    1664 | 11007 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    1106 | 11008 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    1111 | 11009 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    1111 | 11010 | `		SyBlobRelease(&sResolved);` |
|    1111 | 11011 | `		if( zDup == 0 ){` |
|     ! 0 | 11012 | `			goto Mem;` |
|       - | 11013 | `		}` |
|    1111 | 11014 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    1111 | 11015 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11016 | `			goto Mem;` |
|       - | 11017 | `		}` |
|       - | 11018 | `		/* Check for '\|' (multi-catch separator) */` |
|    1106 | 11019 | `		if( pGen->pIn < pGen->pEnd &&` |
|    1106 | 11020 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 11021 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 11022 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 11023 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 11024 | `			continue;` |
|       - | 11025 | `		}` |
|    1083 | 11026 | `		break;` |
|     ! 0 | 11027 | `	}` |
|    1078 | 11028 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    1083 | 11029 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 11030 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 11031 | `			pToken = pGen->pIn;` |
|     ! 0 | 11032 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 11033 | `				pToken--;` |
|     ! 0 | 11034 | `			}` |
|     ! 0 | 11035 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 11036 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 11037 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 11038 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11039 | `				return SXERR_ABORT;` |
|       - | 11040 | `			}` |
|     ! 0 | 11041 | `			return SXERR_INVALID;` |
|       - | 11042 | `	}` |
|    1083 | 11043 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 11044 | `	/* Duplicate instance name */` |
|    1083 | 11045 | `	pName = &pGen->pIn->sData;` |
|    1083 | 11046 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    1083 | 11047 | `	if( zDup == 0 ){` |
|     ! 0 | 11048 | `		goto Mem;` |
|       - | 11049 | `	}` |
|    1083 | 11050 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    1083 | 11051 | `	pGen->pIn++;` |
|    1083 | 11052 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 11053 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 11054 | `		pToken = pGen->pIn;` |
|     ! 0 | 11055 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 11056 | `			pToken--;` |
|     ! 0 | 11057 | `		}` |
|     ! 0 | 11058 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 11059 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 11060 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 11061 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11062 | `			return SXERR_ABORT;` |
|       - | 11063 | `		}` |
|     ! 0 | 11064 | `		return SXERR_INVALID;` |
|       - | 11065 | `	}` |
|       - | 11066 | `	/* Compile the block */` |
|    1083 | 11067 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 11068 | `	/* Create the catch block */` |
|    1083 | 11069 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    1083 | 11070 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 11071 | `		return SXERR_ABORT;` |
|       - | 11072 | `	}` |
|       - | 11073 | `	/* Swap bytecode container */` |
|    1083 | 11074 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    1083 | 11075 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 11076 | `	/* Compile the block */` |
|    1083 | 11077 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 11078 | `	/* Fix forward jumps now the destination is resolved  */` |
|    1083 | 11079 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11080 | `	/* Emit the DONE instruction */` |
|    1083 | 11081 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 11082 | `	/* Leave the block */` |
|    1083 | 11083 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 11084 | `	/* Restore the default container */` |
|    1083 | 11085 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 11086 | `	/* Install the catch block */` |
|    1083 | 11087 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    1083 | 11088 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 11089 | `		goto Mem;` |
|       - | 11090 | `	}` |
|    1083 | 11091 | `	return SXRET_OK;` |
|     ! 0 | 11092 | `Mem:` |
|     ! 0 | 11093 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 11094 | `	return SXERR_ABORT;` |
|     546 | 11095 | `}` |
|       - | 11096 | `/*` |
|       - | 11097 | ` * Compile a 'try' block.` |
|       - | 11098 | ` * A function using an exception should be in a "try" block.` |
|       - | 11099 | ` * If the exception does not trigger, the code will continue` |
|       - | 11100 | ` * as normal. However if the exception triggers, an exception` |
|       - | 11101 | ` * is "thrown".` |
|       - | 11102 | ` */` |
|    1230 | 11103 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 11104 | `{` |
|       - | 11105 | `	ph7_exception *pException;` |
|    1235 | 11106 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 11107 | `	GenBlock *pTry;` |
|       - | 11108 | `	sxu32 nJmpIdx;` |
|       - | 11109 | `	sxi32 rc;` |
|       - | 11110 | `	/* Create the exception container */` |
|    1235 | 11111 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    1235 | 11112 | `	if( pException == 0 ){` |
|     ! 0 | 11113 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 11114 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 11115 | `		return SXERR_ABORT;` |
|       - | 11116 | `	}` |
|       - | 11117 | `	/* Zero the structure */` |
|    1235 | 11118 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 11119 | `	/* Initialize fields */` |
|    1235 | 11120 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    1235 | 11121 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    1235 | 11122 | `	pException->iHasFinally = 0;` |
|    1235 | 11123 | `	pException->iFinallyDone = 0;` |
|    1235 | 11124 | `	pException->pVm = pGen->pVm;` |
|       - | 11125 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|       - | 11126 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|       - | 11127 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|       - | 11128 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|       - | 11129 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|       - | 11130 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    1235 | 11131 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      94 | 11132 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|       - | 11133 | `	}` |
|       - | 11134 | `	/* Create the try block */` |
|    1145 | 11135 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    1145 | 11136 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 11137 | `		return SXERR_ABORT;` |
|       - | 11138 | `	}` |
|       - | 11139 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    1145 | 11140 | `	pTry->pUserData = pException;` |
|       - | 11141 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    1145 | 11142 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 11143 | `	/* Fix the jump later when the destination is resolved */` |
|    1145 | 11144 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    1145 | 11145 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 11146 | `	/* Compile the block */` |
|    1145 | 11147 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    1145 | 11148 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 11149 | `		return SXERR_ABORT;` |
|       - | 11150 | `	}` |
|       - | 11151 | `	/* Fix forward jumps now the destination is resolved */` |
|    1145 | 11152 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11153 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    1145 | 11154 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 11155 | `	/* Leave the block */` |
|    1145 | 11156 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 11157 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    1145 | 11158 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    1138 | 11159 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 11160 | `		/* Compile one or more catch blocks */` |
|    1078 | 11161 | `		for(;;){` |
|    2156 | 11162 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    1577 | 11163 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     542 | 11164 | `					break;` |
|       - | 11165 | `			}` |
|    1087 | 11166 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    1087 | 11167 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11168 | `				return SXERR_ABORT;` |
|       - | 11169 | `			}` |
|       5 | 11170 | `		}` |
|     537 | 11171 | `	}` |
|       - | 11172 | `	/* Compile optional finally block */` |
|    1145 | 11173 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     470 | 11174 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 11175 | `		SySet *pInstrContainer;` |
|       - | 11176 | `		GenBlock *pFinBlock;` |
|     129 | 11177 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 11178 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     129 | 11179 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     129 | 11180 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11181 | `			return SXERR_ABORT;` |
|       - | 11182 | `		}` |
|       - | 11183 | `		/* Swap bytecode container */` |
|     129 | 11184 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     129 | 11185 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 11186 | `		/* Compile the finally body */` |
|     129 | 11187 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     129 | 11188 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11189 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 11190 | `			return SXERR_ABORT;` |
|       - | 11191 | `		}` |
|       - | 11192 | `		/* Fix forward jumps now the destination is resolved */` |
|     129 | 11193 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11194 | `		/* Emit DONE to terminate the finally block */` |
|     129 | 11195 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 11196 | `		/* Leave the block */` |
|     129 | 11197 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 11198 | `		/* Restore the default container */` |
|     129 | 11199 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     129 | 11200 | `		pException->iHasFinally = 1;` |
|      62 | 11201 | `	}` |
|       - | 11202 | `	/* Must have at least one catch or finally */` |
|    1145 | 11203 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       9 | 11204 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 11205 | `			"Cannot use try without catch or finally");` |
|       9 | 11206 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11207 | `			return SXERR_ABORT;` |
|       - | 11208 | `		}` |
|       3 | 11209 | `	}` |
|    1145 | 11210 | `	return SXRET_OK;` |
|     620 | 11211 | `}` |
|       - | 11212 | `/*` |
|       - | 11213 | ` * Compile a switch block.` |
|       - | 11214 | ` *  (See block-comment below for more information)` |
|       - | 11215 | ` */` |
|     112 | 11216 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 11217 | `{` |
|     117 | 11218 | `	sxi32 rc = SXRET_OK;` |
|     117 | 11219 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 11220 | `		/* Unexpected token */` |
|     ! 0 | 11221 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 11222 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11223 | `			return SXERR_ABORT;` |
|       - | 11224 | `		}` |
|     ! 0 | 11225 | `		pGen->pIn++;` |
|     ! 0 | 11226 | `	}` |
|     117 | 11227 | `	pGen->pIn++;` |
|       - | 11228 | `	/* First instruction to execute in this block. */` |
|     117 | 11229 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 11230 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 11231 | `	 * or the '}' token */` |
|     206 | 11232 | `	for(;;){` |
|     417 | 11233 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11234 | `			/* No more input to process */` |
|     ! 0 | 11235 | `			break;` |
|       - | 11236 | `		}` |
|     417 | 11237 | `		rc = SXRET_OK;` |
|     417 | 11238 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 11239 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 11240 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 11241 | `					/* Unexpected token */` |
|     ! 0 | 11242 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 11243 | `						&pGen->pIn->sData);` |
|     ! 0 | 11244 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11245 | `						return SXERR_ABORT;` |
|       - | 11246 | `					}` |
|       - | 11247 | `					/* FALL THROUGH */` |
|     ! 0 | 11248 | `				}` |
|      31 | 11249 | `				rc = SXERR_EOF;` |
|      31 | 11250 | `				break;` |
|       - | 11251 | `			}` |
|      32 | 11252 | `		}else{` |
|       - | 11253 | `			sxi32 nKwrd;` |
|       - | 11254 | `			/* Extract the keyword */` |
|     337 | 11255 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 11256 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 11257 | `				break;` |
|       - | 11258 | `			}` |
|     253 | 11259 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 11260 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 11261 | `					/* Unexpected token */` |
|     ! 0 | 11262 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 11263 | `						&pGen->pIn->sData);` |
|     ! 0 | 11264 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11265 | `						return SXERR_ABORT;` |
|       - | 11266 | `					}` |
|       - | 11267 | `					/* FALL THROUGH */` |
|     ! 0 | 11268 | `				}` |
|       - | 11269 | `				/* Block compiled */` |
|       3 | 11270 | `				break;` |
|       - | 11271 | `			}` |
|       - | 11272 | `		}` |
|       - | 11273 | `		/* Compile block */` |
|     305 | 11274 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 11275 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11276 | `			return SXERR_ABORT;` |
|       - | 11277 | `		}` |
|       5 | 11278 | `	}` |
|     117 | 11279 | `	return rc;` |
|      61 | 11280 | `}` |
|       - | 11281 | `/*` |
|       - | 11282 | ` * Compile a case eXpression.` |
|       - | 11283 | ` *  (See block-comment below for more information)` |
|       - | 11284 | ` */` |
|      92 | 11285 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 11286 | `{` |
|       - | 11287 | `	SySet *pInstrContainer;` |
|       - | 11288 | `	SyToken *pEnd,*pTmp;` |
|      97 | 11289 | `	sxi32 iNest = 0;` |
|       - | 11290 | `	sxi32 rc;` |
|       - | 11291 | `	/* Delimit the expression */` |
|      97 | 11292 | `	pEnd = pGen->pIn;` |
|     197 | 11293 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 11294 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 11295 | `			/* Increment nesting level */` |
|       3 | 11296 | `			iNest++;` |
|     196 | 11297 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 11298 | `			/* Decrement nesting level */` |
|       3 | 11299 | `			iNest--;` |
|     194 | 11300 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 11301 | `			break;` |
|       - | 11302 | `		}` |
|     105 | 11303 | `		pEnd++;` |
|       5 | 11304 | `	}` |
|      97 | 11305 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 11306 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 11307 | `		if( rc == SXERR_ABORT ){` |
|       - | 11308 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11309 | `			return SXERR_ABORT;` |
|       - | 11310 | `		}` |
|     ! 0 | 11311 | `	}` |
|       - | 11312 | `	/* Swap token stream */` |
|      97 | 11313 | `	pTmp = pGen->pEnd;` |
|      97 | 11314 | `	pGen->pEnd = pEnd;` |
|      97 | 11315 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 11316 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 11317 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 11318 | `	/* Emit the done instruction */` |
|      97 | 11319 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 11320 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 11321 | `	/* Update token stream */` |
|      97 | 11322 | `	pGen->pIn  = pEnd;` |
|      97 | 11323 | `	pGen->pEnd = pTmp;` |
|      97 | 11324 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 11325 | `		return SXERR_ABORT;` |
|       - | 11326 | `	}` |
|      97 | 11327 | `	return SXRET_OK;` |
|      51 | 11328 | `}` |
|       - | 11329 | `/*` |
|       - | 11330 | ` * Compile the smart switch statement.` |
|       - | 11331 | ` * According to the PHP language reference manual` |
|       - | 11332 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 11333 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 11334 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 11335 | ` *  This is exactly what the switch statement is for.` |
|       - | 11336 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 11337 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 11338 | ` *  of the outer loop, use continue 2.` |
|       - | 11339 | ` *  Note that switch/case does loose comparision.` |
|       - | 11340 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 11341 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 11342 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 11343 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 11344 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 11345 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 11346 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 11347 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 11348 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 11349 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 11350 | ` *  list for the next case.` |
|       - | 11351 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 11352 | ` *  or floating-point numbers and strings.` |
|       - | 11353 | ` */` |
|      28 | 11354 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 11355 | `{` |
|       - | 11356 | `	GenBlock *pSwitchBlock;` |
|       - | 11357 | `	SyToken *pTmp,*pEnd;` |
|       - | 11358 | `	ph7_switch *pSwitch;` |
|       - | 11359 | `	sxu32 nToken;` |
|       - | 11360 | `	sxu32 nLine;` |
|       - | 11361 | `	sxi32 rc;` |
|      33 | 11362 | `	nLine = pGen->pIn->nLine;` |
|       - | 11363 | `	/* Jump the 'switch' keyword */` |
|      33 | 11364 | `	pGen->pIn++;` |
|      33 | 11365 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 11366 | `		/* Syntax error */` |
|     ! 0 | 11367 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 11368 | `		if( rc == SXERR_ABORT ){` |
|       - | 11369 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11370 | `			return SXERR_ABORT;` |
|       - | 11371 | `		}` |
|     ! 0 | 11372 | `		goto Synchronize;` |
|       - | 11373 | `	}` |
|       - | 11374 | `	/* Jump the left parenthesis '(' */` |
|      33 | 11375 | `	pGen->pIn++;` |
|      33 | 11376 | `	pEnd = 0; /* cc warning */` |
|       - | 11377 | `	/* Create the loop block */` |
|      47 | 11378 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 11379 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 11380 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 11381 | `		return SXERR_ABORT;` |
|       - | 11382 | `	}` |
|       - | 11383 | `	/* Delimit the condition */` |
|      33 | 11384 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 11385 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 11386 | `		/* Empty expression */` |
|     ! 0 | 11387 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 11388 | `		if( rc == SXERR_ABORT ){` |
|       - | 11389 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11390 | `			return SXERR_ABORT;` |
|       - | 11391 | `		}` |
|     ! 0 | 11392 | `	}` |
|       - | 11393 | `	/* Swap token streams */` |
|      33 | 11394 | `	pTmp = pGen->pEnd;` |
|      33 | 11395 | `	pGen->pEnd = pEnd;` |
|       - | 11396 | `	/* Compile the expression */` |
|      33 | 11397 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 11398 | `	if( rc == SXERR_ABORT ){` |
|       - | 11399 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 11400 | `		return SXERR_ABORT;` |
|       - | 11401 | `	}` |
|       - | 11402 | `	/* Update token stream */` |
|      33 | 11403 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 11404 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 11405 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 11406 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11407 | `			return SXERR_ABORT;` |
|       - | 11408 | `		}` |
|     ! 0 | 11409 | `		pGen->pIn++;` |
|     ! 0 | 11410 | `	}` |
|      33 | 11411 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 11412 | `	pGen->pEnd = pTmp;` |
|      33 | 11413 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 11414 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 11415 | `			pTmp = pGen->pIn;` |
|     ! 0 | 11416 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 11417 | `				pTmp--;` |
|     ! 0 | 11418 | `			}` |
|       - | 11419 | `			/* Unexpected token */` |
|     ! 0 | 11420 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 11421 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11422 | `				return SXERR_ABORT;` |
|       - | 11423 | `			}` |
|     ! 0 | 11424 | `			goto Synchronize;` |
|       - | 11425 | `	}` |
|       - | 11426 | `	/* Set the delimiter token */` |
|      33 | 11427 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 11428 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 11429 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 11430 | `	}else{` |
|      31 | 11431 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 11432 | `	}` |
|      33 | 11433 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 11434 | `	/* Create the switch blocks container */` |
|      33 | 11435 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 11436 | `	if( pSwitch == 0 ){` |
|       - | 11437 | `		/* Abort compilation */` |
|     ! 0 | 11438 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 11439 | `		return SXERR_ABORT;` |
|       - | 11440 | `	}` |
|       - | 11441 | `	/* Zero the structure */` |
|      33 | 11442 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 11443 | `	/* Initialize fields */` |
|      33 | 11444 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 11445 | `	/* Emit the switch instruction */` |
|      33 | 11446 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 11447 | `	/* Compile case blocks */` |
|     100 | 11448 | `	for(;;){` |
|       - | 11449 | `		sxu32 nKwrd;` |
|     119 | 11450 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11451 | `			/* No more input to process */` |
|     ! 0 | 11452 | `			break;` |
|       - | 11453 | `		}` |
|     119 | 11454 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 11455 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 11456 | `				/* Unexpected token */` |
|     ! 0 | 11457 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11458 | `					&pGen->pIn->sData);` |
|     ! 0 | 11459 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11460 | `					return SXERR_ABORT;` |
|       - | 11461 | `				}` |
|       - | 11462 | `				/* FALL THROUGH */` |
|     ! 0 | 11463 | `			}` |
|       - | 11464 | `			/* Block compiled */` |
|     ! 0 | 11465 | `			break;` |
|       - | 11466 | `		}` |
|       - | 11467 | `		/* Extract the keyword */` |
|     119 | 11468 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 11469 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 11470 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 11471 | `				/* Unexpected token */` |
|     ! 0 | 11472 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11473 | `					&pGen->pIn->sData);` |
|     ! 0 | 11474 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11475 | `					return SXERR_ABORT;` |
|       - | 11476 | `				}` |
|       - | 11477 | `				/* FALL THROUGH */` |
|     ! 0 | 11478 | `			}` |
|       - | 11479 | `			/* Block compiled */` |
|       3 | 11480 | `			break;` |
|       - | 11481 | `		}` |
|     117 | 11482 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 11483 | `			/*` |
|       - | 11484 | `			 * Accroding to the PHP language reference manual` |
|       - | 11485 | `			 *  A special case is the default case. This case matches anything` |
|       - | 11486 | `			 *  that wasn't matched by the other cases.` |
|       - | 11487 | `			 */` |
|      25 | 11488 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 11489 | `				/* Default case already compiled */` |
|     ! 0 | 11490 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 11491 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11492 | `					return SXERR_ABORT;` |
|       - | 11493 | `				}` |
|     ! 0 | 11494 | `			}` |
|      25 | 11495 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 11496 | `			/* Compile the default block */` |
|      25 | 11497 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 11498 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11499 | `				return SXERR_ABORT;` |
|      25 | 11500 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 11501 | `				break;` |
|       1 | 11502 | `			}` |
|      98 | 11503 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 11504 | `			ph7_case_expr sCase;` |
|       - | 11505 | `			/* Standard case block */` |
|      97 | 11506 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 11507 | `			/* initialize the structure */` |
|      97 | 11508 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 11509 | `			/* Compile the case expression */` |
|      97 | 11510 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 11511 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11512 | `				return SXERR_ABORT;` |
|       - | 11513 | `			}` |
|       - | 11514 | `			/* Compile the case block */` |
|      97 | 11515 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 11516 | `			/* Insert in the switch container */` |
|      97 | 11517 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 11518 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11519 | `				return SXERR_ABORT;` |
|      97 | 11520 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 11521 | `				break;` |
|       - | 11522 | `			}` |
|      47 | 11523 | `		}else{` |
|       - | 11524 | `			/* Unexpected token */` |
|     ! 0 | 11525 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11526 | `				&pGen->pIn->sData);` |
|     ! 0 | 11527 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11528 | `				return SXERR_ABORT;` |
|       - | 11529 | `			}` |
|     ! 0 | 11530 | `			break;` |
|       - | 11531 | `		}` |
|       5 | 11532 | `	}` |
|       - | 11533 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 11534 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 11535 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11536 | `	/* Release the loop block */` |
|      33 | 11537 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 11538 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 11539 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 11540 | `		pGen->pIn++;` |
|      14 | 11541 | `	}` |
|       - | 11542 | `	/* Statement successfully compiled */` |
|      33 | 11543 | `	return SXRET_OK;` |
|     ! 0 | 11544 | `Synchronize:` |
|       - | 11545 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 11546 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 11547 | `		pGen->pIn++;` |
|     ! 0 | 11548 | `	}` |
|     ! 0 | 11549 | `	return SXRET_OK;` |
|      19 | 11550 | `}` |
|       - | 11551 | `/*` |
|       - | 11552 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 11553 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 11554 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 11555 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 11556 | ` */` |
|       - | 11557 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 11558 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 11559 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 11560 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 11561 |  |
|       - | 11562 | `/*` |
|       - | 11563 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 11564 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 11565 | ` * patched entries from the pending set.` |
|       - | 11566 | ` */` |
| 3044040 | 11567 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 11568 | `{` |
| 3044045 | 11569 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 11570 | `	sxu32 nTarget;` |
|       - | 11571 | `	sxu32 *aIdx;` |
|       - | 11572 | `	sxu32 i;` |
| 3044045 | 11573 | `	if( nCur <= nBaseline ){` |
| 3043949 | 11574 | `		return;` |
|       - | 11575 | `	}` |
|     100 | 11576 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|     100 | 11577 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     204 | 11578 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     108 | 11579 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     108 | 11580 | `		if( pInstr ){` |
|     108 | 11581 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      52 | 11582 | `		}` |
|      56 | 11583 | `	}` |
|     100 | 11584 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1522025 | 11585 | `}` |
|       - | 11586 |  |
|       - | 11587 | `/*` |
|       - | 11588 | ` * By-reference out-parameters of builtin functions.` |
|       - | 11589 | ` *` |
|       - | 11590 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 11591 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 11592 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 11593 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 11594 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 11595 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 11596 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 11597 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 11598 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 11599 | ` * creates it" behaviour).` |
|       - | 11600 | ` *` |
|       - | 11601 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 11602 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 11603 | ` */` |
|  515608 | 11604 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 11605 | `{` |
|       - | 11606 | `	static const struct {` |
|       - | 11607 | `		const char *zName;` |
|       - | 11608 | `		sxu32 nByte;` |
|       - | 11609 | `		sxu32 mask;` |
|       - | 11610 | `	} aByRef[] = {` |
|       - | 11611 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11612 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11613 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11614 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11615 | `	};` |
|       - | 11616 | `	sxu32 i;` |
|  515613 | 11617 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    2427 | 11618 | `		return 0;` |
|       - | 11619 | `	}` |
| 2565663 | 11620 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 2052566 | 11621 | `		if( pName->nByte == aByRef[i].nByte` |
| 1054347 | 11622 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      99 | 11623 | `			return aByRef[i].mask;` |
|       - | 11624 | `		}` |
| 1026241 | 11625 | `	}` |
|  513097 | 11626 | `	return 0;` |
|  257809 | 11627 | `}` |
|       - | 11628 | `/*` |
|       - | 11629 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 11630 | ` *` |
|       - | 11631 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 11632 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 11633 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 11634 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 11635 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 11636 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 11637 | ` */` |
|  515608 | 11638 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 11639 | `{` |
|       - | 11640 | `	SyToken *p, *pEnd;` |
|  515613 | 11641 | `	pOut->zString = 0;` |
|  515613 | 11642 | `	pOut->nByte = 0;` |
|  515613 | 11643 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 11644 | `		return;` |
|       - | 11645 | `	}` |
|  515613 | 11646 | `	p = pLeft->pStart;` |
|  515613 | 11647 | `	pEnd = pLeft->pEnd;` |
|       - | 11648 | `	/* Optional single leading namespace separator (absolute path). */` |
|  515613 | 11649 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3877 | 11650 | `		p++;` |
|    1936 | 11651 | `	}` |
|  515613 | 11652 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    2391 | 11653 | `		return;` |
|       - | 11654 | `	}` |
|       - | 11655 | `	/* Must be a single component: nothing follows the name token. */` |
|  513227 | 11656 | `	if( p + 1 != pEnd ){` |
|      41 | 11657 | `		return;` |
|       - | 11658 | `	}` |
|  513191 | 11659 | `	*pOut = p->sData;` |
|  257809 | 11660 | `}` |
|       - | 11661 | `/*` |
|       - | 11662 | ` * Generate bytecode for a given expression tree.` |
|       - | 11663 | ` * If something goes wrong while generating bytecode` |
|       - | 11664 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 11665 | ` * this function takes care of generating the appropriate` |
|       - | 11666 | ` * error message.` |
|       - | 11667 | ` */` |
| 4047028 | 11668 | `static sxi32 GenStateEmitExprCode(` |
|       - | 11669 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11670 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 11671 | `	sxi32 iFlags /* Control flags */` |
|       - | 11672 | `	)` |
|       5 | 11673 | `{` |
|       - | 11674 | `	VmInstr *pInstr;` |
|       - | 11675 | `	sxu32 nJmpIdx;` |
| 4047033 | 11676 | `	sxi32 iP1 = 0;` |
| 4047033 | 11677 | `	sxu32 iP2 = 0;` |
| 4047033 | 11678 | `	void *p3  = 0;` |
|       - | 11679 | `	sxi32 iVmOp;` |
|       - | 11680 | `	sxi32 rc;` |
| 4047033 | 11681 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 4047033 | 11682 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 4047033 | 11683 | `	sxu32 nRhsNsBase = 0;` |
| 4047033 | 11684 | `	if( pNode->xCode ){` |
|       - | 11685 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 11686 | `		/* Compile node */` |
| 2533411 | 11687 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2533411 | 11688 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2533411 | 11689 | `		RE_SWAP_DELIMITER(pGen);` |
| 2533411 | 11690 | `		return rc;` |
|       - | 11691 | `	}` |
| 1513627 | 11692 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 11693 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11694 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 11695 | `		return SXERR_ABORT;` |
|       - | 11696 | `	}` |
| 1513627 | 11697 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1513627 | 11698 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|       - | 11699 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|       - | 11700 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|       - | 11701 | `		 * and later errors are still reported. */` |
|       3 | 11702 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11703 | `			"The (unset) cast is no longer supported");` |
|       3 | 11704 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11705 | `			return SXERR_ABORT;` |
|       - | 11706 | `		}` |
|       1 | 11707 | `	}` |
| 1513627 | 11708 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      65 | 11709 | `		sxu32 nJmp = 0;` |
|       - | 11710 | `		sxu32 nNcNsBase;` |
|       - | 11711 | `		VmInstr *pInstrFix;` |
|       - | 11712 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 11713 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 11714 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 11715 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 11716 | `		 * stack slot carries a writable nIdx. */` |
|      65 | 11717 | `		if( pNode->pRight ){` |
|      65 | 11718 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11719 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      65 | 11720 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11721 | `				return rc;` |
|       - | 11722 | `			}` |
|      65 | 11723 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 11724 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 11725 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 11726 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 11727 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 11728 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 11729 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 11730 | `			 * cascade for the actual write path stays correct. */` |
|      65 | 11731 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11732 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      31 | 11733 | `				pInstrFix->iP2 = 3;` |
|      14 | 11734 | `			}` |
|      31 | 11735 | `		}` |
|       - | 11736 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      65 | 11737 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 11738 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      65 | 11739 | `		if( pNode->pLeft ){` |
|      65 | 11740 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11741 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      65 | 11742 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11743 | `				return rc;` |
|       - | 11744 | `			}` |
|      65 | 11745 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      31 | 11746 | `		}` |
|       - | 11747 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      65 | 11748 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 11749 | `		/* Patch the short-circuit jump to land after the store. */` |
|      65 | 11750 | `		if( nJmp > 0 ){` |
|      65 | 11751 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      65 | 11752 | `			if( pInstrFix ){` |
|      65 | 11753 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      31 | 11754 | `			}` |
|      31 | 11755 | `		}` |
|      65 | 11756 | `		return SXRET_OK;` |
|       - | 11757 | `	}` |
| 1513565 | 11758 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 11759 | `		sxu32 nJz,nJmp;` |
|       - | 11760 | `		sxu32 nTernaryNsBase;` |
|       - | 11761 | `		/* Ternary operator require special handling */` |
|       - | 11762 | `		/* Phase#1: Compile the condition */` |
|    6581 | 11763 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    6581 | 11764 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    6581 | 11765 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11766 | `			return rc;` |
|       - | 11767 | `		}` |
|       - | 11768 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 11769 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 11770 | `		 * condition expression, not leak past the ternary. */` |
|    6581 | 11771 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    6581 | 11772 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    6581 | 11773 | `		if( pNode->pLeft ){` |
|       - | 11774 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 11775 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    6513 | 11776 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11777 | `			/* Phase#3: Compile the 'then' expression  */` |
|    6513 | 11778 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    6513 | 11779 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    6513 | 11780 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11781 | `				return rc;` |
|       - | 11782 | `			}` |
|    6513 | 11783 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    3259 | 11784 | `		}else{` |
|       - | 11785 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 11786 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 11787 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 11788 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 11789 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11790 | `		}` |
|       - | 11791 | `		/* Phase#4: Emit the unconditional jump */` |
|    6581 | 11792 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 11793 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    6581 | 11794 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    6581 | 11795 | `		if( pInstr ){` |
|    6581 | 11796 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    3288 | 11797 | `		}` |
|    6581 | 11798 | `		if( !pNode->pLeft ){` |
|       - | 11799 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 11800 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 11801 | `		}` |
|       - | 11802 | `		/* Phase#6: Compile the 'else' expression */` |
|    6581 | 11803 | `		if( pNode->pRight ){` |
|    6581 | 11804 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    6581 | 11805 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    6581 | 11806 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11807 | `				return rc;` |
|       - | 11808 | `			}` |
|    6581 | 11809 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    3288 | 11810 | `		}` |
|    6581 | 11811 | `		if( nJmp > 0 ){` |
|       - | 11812 | `			/* Phase#7: Fix the unconditional jump */` |
|    6581 | 11813 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    6581 | 11814 | `			if( pInstr ){` |
|    6581 | 11815 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    3288 | 11816 | `			}` |
|    3288 | 11817 | `		}` |
|       - | 11818 | `		/* All done */` |
|    6581 | 11819 | `		return SXRET_OK;` |
|       - | 11820 | `	}` |
| 1506989 | 11821 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|       - | 11822 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|       - | 11823 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|       - | 11824 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|       - | 11825 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|       - | 11826 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|       - | 11827 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|       - | 11828 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|       - | 11829 | `		sxu32 nPipeNsBase;` |
|      27 | 11830 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|      27 | 11831 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|     ! 0 | 11832 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11833 | `				"'\|>': Missing operand");` |
|     ! 0 | 11834 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 11835 | `		}` |
|       - | 11836 | `		/* Argument: the LHS value. */` |
|      27 | 11837 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      27 | 11838 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|      27 | 11839 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11840 | `			return rc;` |
|       - | 11841 | `		}` |
|      27 | 11842 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|       - | 11843 | `		/* Callable: the RHS. */` |
|      27 | 11844 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      27 | 11845 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|      27 | 11846 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11847 | `			return rc;` |
|       - | 11848 | `		}` |
|      27 | 11849 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|       - | 11850 | `		/* Invoke the callable with the single piped argument. */` |
|      27 | 11851 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      27 | 11852 | `		return SXRET_OK;` |
|       - | 11853 | `	}` |
| 1506963 | 11854 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 11855 | `	/* Generate code for the left tree */` |
| 1506963 | 11856 | `	if( pNode->pLeft ){` |
| 1506931 | 11857 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1506931 | 11858 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 11859 | `			ph7_expr_node **apNode;` |
|  519625 | 11860 | `			int hasSpread = 0;` |
|  519625 | 11861 | `			int hasNamed = 0;` |
|  519625 | 11862 | `			int bAnySpread = 0;` |
|  519625 | 11863 | `			sxu32 byRefMask = 0;` |
|       - | 11864 | `			sxi32 nArgs;` |
|       - | 11865 | `			sxi32 n;` |
|       - | 11866 | `			/* Recurse and generate bytecodes for function arguments */` |
|  519625 | 11867 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  519625 | 11868 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 11869 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|       - | 11870 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|       - | 11871 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  519625 | 11872 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      77 | 11873 | `				bFcc = 1;` |
|      77 | 11874 | `				nArgs = 0;` |
|      38 | 11875 | `			}` |
|       - | 11876 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 11877 | `			{` |
|  519625 | 11878 | `				int seenNamed = 0;` |
| 1053431 | 11879 | `				for( n = 0; n < nArgs; ++n ){` |
|  533813 | 11880 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     253 | 11881 | `						seenNamed = 1;` |
|     253 | 11882 | `						hasNamed = 1;` |
|  533689 | 11883 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|    3877 | 11884 | `						bAnySpread = 1;` |
|  531629 | 11885 | `					}else if( seenNamed ){` |
|       3 | 11886 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 11887 | `							"Cannot use positional argument after named argument");` |
|       3 | 11888 | `						return SXERR_SYNTAX;` |
|       - | 11889 | `					}` |
|  266908 | 11890 | `				}` |
|       - | 11891 | `			}` |
|       - | 11892 | `			/* Read-only load */` |
|  519623 | 11893 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 11894 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 11895 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 11896 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 11897 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  519623 | 11898 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  519623 | 11899 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  519618 | 11900 | `				if( pCallName->nByte == 5` |
|  278238 | 11901 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   23403 | 11902 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  507924 | 11903 | `				}else if( pCallName->nByte == 5` |
|  254840 | 11904 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      99 | 11905 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      47 | 11906 | `				}` |
|       - | 11907 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 11908 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 11909 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 11910 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 11911 | `				 * the compile-time positional index no longer maps to the` |
|       - | 11912 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  519623 | 11913 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 11914 | `					SyString sBuiltin;` |
|  515613 | 11915 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  515613 | 11916 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  257804 | 11917 | `				}` |
|  259809 | 11918 | `			}` |
| 1053427 | 11919 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  533809 | 11920 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  533809 | 11921 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11922 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 11923 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 11924 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 11925 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 11926 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 11927 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  533809 | 11928 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      55 | 11929 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      55 | 11930 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      25 | 11931 | `				}` |
|  533809 | 11932 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  533809 | 11933 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11934 | `					return rc;` |
|       - | 11935 | `				}` |
|       - | 11936 | `				/* Each argument is an independent nullsafe scope. */` |
|  533809 | 11937 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  533809 | 11938 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 11939 | `					/* Emit spread opcode to unpack this array argument */` |
|    3877 | 11940 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|    3877 | 11941 | `					hasSpread = 1;` |
|    1936 | 11942 | `				}` |
|  266907 | 11943 | `			}` |
|       - | 11944 | `			/* Total number of given arguments */` |
|  519623 | 11945 | `			iP1 = nArgs;` |
|  519623 | 11946 | `			iP2 = hasSpread;` |
|       - | 11947 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 11948 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  519623 | 11949 | `			if( hasNamed ){` |
|     142 | 11950 | `				sxu32 nStrBytes = 0;` |
|       - | 11951 | `				char *zBuf;` |
|     424 | 11952 | `				for( n = 0; n < nArgs; ++n ){` |
|     286 | 11953 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     250 | 11954 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|     123 | 11955 | `					}` |
|     145 | 11956 | `				}` |
|       - | 11957 | `				{` |
|     142 | 11958 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     142 | 11959 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     138 | 11960 | `					&pGen->pVm->sAllocator, mapSize);` |
|     142 | 11961 | `				if( pMap ){` |
|     142 | 11962 | `					SyZero(pMap, mapSize);` |
|     142 | 11963 | `					pMap->bHasNamed = 1;` |
|     142 | 11964 | `					pMap->nTotal = (sxu32)nArgs;` |
|     142 | 11965 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     142 | 11966 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     424 | 11967 | `					for( n = 0; n < nArgs; ++n ){` |
|     286 | 11968 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     250 | 11969 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     250 | 11970 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     250 | 11971 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     250 | 11972 | `							zBuf += nb;` |
|     123 | 11973 | `						}` |
|       - | 11974 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     145 | 11975 | `					}` |
|     142 | 11976 | `					p3 = (void *)pMap;` |
|      69 | 11977 | `				}` |
|       - | 11978 | `				}` |
|      69 | 11979 | `			}` |
|       - | 11980 | `			/* Remove stale flags now */` |
|  519623 | 11981 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  259809 | 11982 | `		}` |
|       - | 11983 | `		{` |
|       - | 11984 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|       - | 11985 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|       - | 11986 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|       - | 11987 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|       - | 11988 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|       - | 11989 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|       - | 11990 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|       - | 11991 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 1506929 | 11992 | `			sxi32 iLeftFlags = iFlags;` |
| 1506924 | 11993 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 1138780 | 11994 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  385344 | 11995 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  376533 | 11996 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   17845 | 11997 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|    8920 | 11998 | `			}` |
|       - | 11999 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|       - | 12000 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|       - | 12001 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|       - | 12002 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|       - | 12003 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|       - | 12004 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|       - | 12005 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 1506924 | 12006 | `			if( pNode->pOp` |
| 2164451 | 12007 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 1411036 | 12008 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 1315096 | 12009 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  192263 | 12010 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   96129 | 12011 | `			}` |
| 1506929 | 12012 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|       - | 12013 | `		}` |
| 1506929 | 12014 | `		if( rc != SXRET_OK ){` |
|      34 | 12015 | `			return rc;` |
|       - | 12016 | `		}` |
| 1506899 | 12017 | `		if( !bIsChainOp ){` |
|       - | 12018 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 12019 | `			 * target the end of that LHS chain, which is right here. */` |
|  696187 | 12020 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  348091 | 12021 | `		}` |
| 1506899 | 12022 | `		if( iVmOp == PH7_OP_CALL ){` |
|  519623 | 12023 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  519623 | 12024 | `			if( pInstr ){` |
|  519623 | 12025 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  513345 | 12026 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 12027 | `					sxu32 nQual;` |
|  513345 | 12028 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 12029 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 12030 | `					 * so the later NEW handler (if any) can see it. */` |
|  513345 | 12031 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 12032 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 12033 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 12034 | `					 * imports — class imports must NOT affect function` |
|       - | 12035 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 12036 | `					 * before NEW; we store the original literal index in the` |
|       - | 12037 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 12038 | `					 * the unqualified name and re-qualify with class imports. */` |
|  513345 | 12039 | `					if( bAbsolute ){` |
|    3877 | 12040 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1941 | 12041 | `					}else{` |
|  509473 | 12042 | `						int fromImport = 0;` |
|  509473 | 12043 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  509473 | 12044 | `						pInstr->iP2 = (sxi32)nQual;` |
|  509473 | 12045 | `						if( nQual != nOrig ){` |
|       - | 12046 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 12047 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 12048 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 12049 | `							if( !fromImport ){` |
|       - | 12050 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 12051 | `								if( p3 == 0 ){` |
|      67 | 12052 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 12053 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 12054 | `									if( pMap ){` |
|      67 | 12055 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 12056 | `										p3 = (void *)pMap;` |
|      31 | 12057 | `									}` |
|      31 | 12058 | `								}` |
|      67 | 12059 | `								if( p3 ){` |
|      67 | 12060 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 12061 | `								}` |
|      31 | 12062 | `							}` |
|      36 | 12063 | `						}` |
|       5 | 12064 | `					}` |
|  262953 | 12065 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 12066 | `					/* Method call,flag that */` |
|    1975 | 12067 | `					pInstr->iP2 = 1;` |
|     985 | 12068 | `				}` |
|  259814 | 12069 | `			}` |
| 1247090 | 12070 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 12071 | `			ph7_expr_node **apNode;` |
|       - | 12072 | `			sxi32 n;` |
|   98841 | 12073 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 12074 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 12075 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 12076 | `			/* Recurse and generate bytecodes for array index */` |
|   98841 | 12077 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  178345 | 12078 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   79509 | 12079 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   79509 | 12080 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   79509 | 12081 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 12082 | `					return rc;` |
|       - | 12083 | `				}` |
|       - | 12084 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   79509 | 12085 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   39757 | 12086 | `			}` |
|   98841 | 12087 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   79509 | 12088 | `				iP1 = 1; /* Node have an index associated with it */` |
|   39752 | 12089 | `			}` |
|   98841 | 12090 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 12091 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     245 | 12092 | `				iP2 = 4;` |
|   98721 | 12093 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 12094 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 12095 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      64 | 12096 | `				iP2 = 5;` |
|   98571 | 12097 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 12098 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 12099 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 12100 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 12101 | `				iP2 = 6;` |
|   98529 | 12102 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 12103 | `				/* Create an empty entry when the desired index is not found */` |
|   39001 | 12104 | `				iP2 = 1;` |
|   19503 | 12105 | `			}` |
|  937863 | 12106 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 12107 | `			/* POP the left node */` |
|      32 | 12108 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 12109 | `		}` |
|  753447 | 12110 | `	}` |
| 1506931 | 12111 | `	rc = SXRET_OK;` |
| 1506931 | 12112 | `	nJmpIdx = 0;` |
|       - | 12113 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 12114 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 12115 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1506931 | 12116 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     415 | 12117 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     415 | 12118 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     415 | 12119 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     415 | 12120 | `			int isSpecial = 0;` |
|     415 | 12121 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     323 | 12122 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     323 | 12123 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     318 | 12124 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     304 | 12125 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     146 | 12126 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|     111 | 12127 | `					isSpecial = 1;` |
|      53 | 12128 | `				}` |
|     182 | 12129 | `			}` |
|     461 | 12130 | `			pInstr->iP1 = 0;` |
|     461 | 12131 | `			if( !isSpecial ){` |
|     263 | 12132 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     129 | 12133 | `			}` |
|       - | 12134 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 12135 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     369 | 12136 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     263 | 12137 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     263 | 12138 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      46 | 12139 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      48 | 12140 | `					return SXRET_OK;` |
|       - | 12141 | `				}` |
|     107 | 12142 | `			}` |
|     160 | 12143 | `		}` |
|     231 | 12144 | `	}` |
|       - | 12145 | `	/* Generate code for the right tree */` |
| 1506855 | 12146 | `	if( pNode->pRight ){` |
|  788317 | 12147 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 12148 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   12063 | 12149 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  782288 | 12150 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 12151 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    4029 | 12152 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  774247 | 12153 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 12154 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     135 | 12155 | `			iVmOp = 0; /* No binary operator to emit */` |
|     135 | 12156 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  772222 | 12157 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 12158 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 12159 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 12160 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 12161 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 12162 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 12163 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     108 | 12164 | `			sxu32 nNsJmp = 0;` |
|     108 | 12165 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     108 | 12166 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  772053 | 12167 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|       - | 12168 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|       - | 12169 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|       - | 12170 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  329185 | 12171 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  164590 | 12172 | `		}` |
|  788317 | 12173 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  788317 | 12174 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  788317 | 12175 | `		if( !bIsChainOp ){` |
|       - | 12176 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 12177 | `			 * operator instruction is emitted. */` |
|  596103 | 12178 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  298049 | 12179 | `		}` |
|  788317 | 12180 | `		if( iVmOp == PH7_OP_STORE ){` |
|  325059 | 12181 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  325024 | 12182 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 12183 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 12184 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 12185 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 12186 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 12187 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 12188 | `				 */` |
|      85 | 12189 | `				iVmOp = 0;` |
|  325019 | 12190 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  324979 | 12191 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 12192 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   85083 | 12193 | `					iP2 = 1;` |
|   42544 | 12194 | `				}else{` |
|  239901 | 12195 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 12196 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   38919 | 12197 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   38919 | 12198 | `						iP1 = pInstr->iP1;` |
|   19462 | 12199 | `					}else{` |
|  200987 | 12200 | `						p3 = pInstr->p3;` |
|       - | 12201 | `					}` |
|       - | 12202 | `					/* POP the last dynamic load instruction */` |
|  239901 | 12203 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 12204 | `				}` |
|  162492 | 12205 | `			}` |
|  625790 | 12206 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      61 | 12207 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      61 | 12208 | `			if( pInstr ){` |
|      61 | 12209 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 12210 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 12211 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 12212 | `					 */` |
|      19 | 12213 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      19 | 12214 | `					iP1 = pInstr->iP1;` |
|      19 | 12215 | `					iP2 = pInstr->iP2;` |
|      19 | 12216 | `					p3  = pInstr->p3;` |
|      10 | 12217 | `				}else{` |
|      43 | 12218 | `					p3 = pInstr->p3;` |
|       - | 12219 | `				}` |
|      29 | 12220 | `			}` |
|      29 | 12221 | `		}` |
|  394156 | 12222 | `	}` |
| 1506850 | 12223 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   24078 | 12224 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 12225 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 12226 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      30 | 12227 | `		iVmOp = 0;` |
|      13 | 12228 | `	}` |
| 1506855 | 12229 | `	if( iVmOp > 0 ){` |
| 1506589 | 12230 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   15795 | 12231 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 12232 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   11557 | 12233 | `				iP1 = 1;` |
|    5781 | 12234 | `			}` |
| 1498694 | 12235 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 12236 | `			/* Namespace-qualify the class name for NEW */ {` |
|   47883 | 12237 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   47883 | 12238 | `				VmInstr *pCallInstr = 0;` |
|   47883 | 12239 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   47667 | 12240 | `					pCallInstr = pPeek;` |
|   47667 | 12241 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   23831 | 12242 | `				}` |
|   47883 | 12243 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   47879 | 12244 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 12245 | `					sxu32 nLitForClass;` |
|       - | 12246 | `					/* If the CALL handler already qualified the name using` |
|       - | 12247 | `					 * function imports, recover the original unqualified` |
|       - | 12248 | `					 * literal so we can re-qualify with class imports. */` |
|   47879 | 12249 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 12250 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 12251 | `					}else{` |
|   47847 | 12252 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 12253 | `					}` |
|   47879 | 12254 | `					pPeek->iP1 = 0;` |
|   47879 | 12255 | `					if( !bAbsolute ){` |
|   44011 | 12256 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   22008 | 12257 | `					}else{` |
|    3873 | 12258 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 12259 | `					}` |
|   23937 | 12260 | `				}` |
|       - | 12261 | `			}` |
|   47883 | 12262 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   47883 | 12263 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 12264 | `				VmInstr *pPrev;` |
|   47667 | 12265 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   47667 | 12266 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 12267 | `					/* Pop the call instruction, preserve named-arg map */` |
|   47667 | 12268 | `					iP1 = pInstr->iP1;` |
|   47667 | 12269 | `					if( pInstr->p3 ){` |
|      43 | 12270 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 12271 | `					}` |
|   47667 | 12272 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   23831 | 12273 | `				}` |
|   23836 | 12274 | `			}` |
| 1466860 | 12275 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 12276 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 12277 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     203 | 12278 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     203 | 12279 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     203 | 12280 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     203 | 12281 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     203 | 12282 | `				int isSpecialIs = 0;` |
|     203 | 12283 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     203 | 12284 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     203 | 12285 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     198 | 12286 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     201 | 12287 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      99 | 12288 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 12289 | `						isSpecialIs = 1;` |
|       5 | 12290 | `					}` |
|      99 | 12291 | `				}` |
|     203 | 12292 | `				pInstr->iP1 = 0;` |
|     203 | 12293 | `				if( !isSpecialIs && !bAbsolute ){` |
|     183 | 12294 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      89 | 12295 | `				}` |
|     104 | 12296 | `			}` |
| 1442822 | 12297 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 12298 | `			/* Prevent constant expansion for member/property names.` |
|       - | 12299 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 12300 | `			 * should not trigger constant lookup. */` |
|  192219 | 12301 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  192219 | 12302 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  192169 | 12303 | `				pInstr->iP1 = 0;` |
|   96082 | 12304 | `			}` |
|  192219 | 12305 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 12306 | `				/* Static member access,remember that */` |
|     339 | 12307 | `				iP1 = 1;` |
|     339 | 12308 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     339 | 12309 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      44 | 12310 | `					p3 = pInstr->p3;` |
|      44 | 12311 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      20 | 12312 | `				}` |
|     167 | 12313 | `			}` |
|       - | 12314 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|       - | 12315 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|       - | 12316 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|       - | 12317 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  192219 | 12318 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  192219 | 12319 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|      30 | 12320 | `					iP2 = PH7_MEMBER_UNSET;` |
|  192205 | 12321 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      77 | 12322 | `					iP2 = PH7_MEMBER_ISSET;` |
|  192155 | 12323 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|      13 | 12324 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  192113 | 12325 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|       - | 12326 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   85163 | 12327 | `					iP2 = PH7_MEMBER_WRITE;` |
|   42579 | 12328 | `				}` |
|   96107 | 12329 | `			}` |
|   96107 | 12330 | `		}` |
|       - | 12331 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|       - | 12332 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|       - | 12333 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|       - | 12334 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|       - | 12335 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 1506589 | 12336 | `		if( bFcc ){` |
|      77 | 12337 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|      77 | 12338 | `			iP2 = 0;` |
|      77 | 12339 | `			p3 = 0;` |
|      77 | 12340 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      77 | 12341 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 12342 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|       - | 12343 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|       - | 12344 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|       - | 12345 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      35 | 12346 | `				void *pMemberName = pInstr->p3;` |
|      35 | 12347 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      35 | 12348 | `				if( pMemberName ){` |
|       3 | 12349 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       1 | 12350 | `				}` |
|      35 | 12351 | `				iP1 = 2;` |
|      18 | 12352 | `			}else{` |
|      43 | 12353 | `				iP1 = 1;` |
|       - | 12354 | `			}` |
|      38 | 12355 | `		}` |
|       - | 12356 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 12357 | `		 * This is the primary emit path for user-visible calls. */` |
| 1506589 | 12358 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  567425 | 12359 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  283710 | 12360 | `		}` |
|       - | 12361 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1506589 | 12362 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  753292 | 12363 | `	}` |
| 1506855 | 12364 | `	if( nJmpIdx > 0 ){` |
|       - | 12365 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   16217 | 12366 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   16217 | 12367 | `		if( pInstr ){` |
|   16217 | 12368 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    8106 | 12369 | `		}` |
|    8106 | 12370 | `	}` |
| 1506855 | 12371 | `	return rc;` |
| 2023503 | 12372 | `}` |
|       - | 12373 | `/*` |
|       - | 12374 | ` * Compile a PHP expression.` |
|       - | 12375 | ` * According to the PHP language reference manual:` |
|       - | 12376 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 12377 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 12378 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 12379 | ` *  is "anything that has a value".` |
|       - | 12380 | ` * If something goes wrong while compiling the expression,this` |
|       - | 12381 | ` * function takes care of generating the appropriate error` |
|       - | 12382 | ` * message.` |
|       - | 12383 | ` */` |
| 1118808 | 12384 | `static sxi32 PH7_CompileExpr(` |
|       - | 12385 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 12386 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 12387 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 12388 | `	)` |
|       5 | 12389 | `{` |
|       - | 12390 | `	ph7_expr_node *pRoot;` |
|       - | 12391 | `	SySet sExprNode;` |
|       - | 12392 | `	SyToken *pEnd;` |
|       - | 12393 | `	sxi32 nExpr;` |
|       - | 12394 | `	sxi32 iNest;` |
|       - | 12395 | `	sxi32 rc;` |
|       - | 12396 | `	sxu32 nNullsafeBase;` |
|       - | 12397 | `	/* Initialize worker variables */` |
| 1118813 | 12398 | `	nExpr = 0;` |
| 1118813 | 12399 | `	pRoot = 0;` |
|       - | 12400 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 12401 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
| 1118813 | 12402 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1118813 | 12403 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
| 1118813 | 12404 | `	SySetAlloc(&sExprNode,0x10);` |
| 1118813 | 12405 | `	rc = SXRET_OK;` |
|       - | 12406 | `	/* Delimit the expression */` |
| 1118813 | 12407 | `	pEnd = pGen->pIn;` |
| 1118813 | 12408 | `	iNest = 0;` |
| 7331717 | 12409 | `	while( pEnd < pGen->pEnd ){` |
| 6930645 | 12410 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 12411 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     563 | 12412 | `			iNest++;` |
| 6930366 | 12413 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     571 | 12414 | `			iNest--;` |
| 6929804 | 12415 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  718177 | 12416 | `			if( iNest <= 0 ){` |
|  717741 | 12417 | `				break;` |
|       - | 12418 | `			}` |
|     218 | 12419 | `		}` |
| 6212909 | 12420 | `		pEnd++;` |
|       5 | 12421 | `	}` |
| 1118813 | 12422 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   23669 | 12423 | `		SyToken *pEnd2 = pGen->pIn;` |
|   23669 | 12424 | `		iNest = 0;` |
|       - | 12425 | `		/* Stop at the first comma */` |
|   47651 | 12426 | `		while( pEnd2 < pEnd ){` |
|   23993 | 12427 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      71 | 12428 | `				iNest++;` |
|   23960 | 12429 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      71 | 12430 | `				iNest--;` |
|   23894 | 12431 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 12432 | `				if( iNest <= 0 ){` |
|       7 | 12433 | `					break;` |
|       - | 12434 | `				}` |
|      23 | 12435 | `			}` |
|   23987 | 12436 | `			pEnd2++;` |
|       5 | 12437 | `		}` |
|   23669 | 12438 | `		if( pEnd2 <pEnd ){` |
|       7 | 12439 | `			pEnd = pEnd2;` |
|       3 | 12440 | `		}` |
|   11832 | 12441 | `	}` |
| 1118813 | 12442 | `	if( pEnd > pGen->pIn ){` |
| 1118803 | 12443 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 12444 | `		/* Swap delimiter */` |
| 1118803 | 12445 | `		pGen->pEnd = pEnd;` |
|       - | 12446 | `		/* Try to get an expression tree */` |
| 1118803 | 12447 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
| 1118803 | 12448 | `		if( rc == SXRET_OK && pRoot ){` |
| 1118621 | 12449 | `			rc = SXRET_OK;` |
| 1118621 | 12450 | `			if( xTreeValidator ){` |
|       - | 12451 | `				/* Call the upper layer validator callback */` |
|   61819 | 12452 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   30907 | 12453 | `			}` |
| 1118621 | 12454 | `			if( rc != SXERR_ABORT ){` |
|       - | 12455 | `				/* Generate code for the given tree */` |
| 1118621 | 12456 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 12457 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 12458 | `				 * expression so they short-circuit to its end. */` |
| 1118621 | 12459 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  559308 | 12460 | `			}` |
| 1118621 | 12461 | `			nExpr = 1;` |
|  559308 | 12462 | `		}` |
|       - | 12463 | `		/* Release the whole tree */` |
| 1118803 | 12464 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 12465 | `		/* Synchronize token stream */` |
| 1118803 | 12466 | `		pGen->pEnd = pTmp;` |
| 1118803 | 12467 | `		pGen->pIn  = pEnd;` |
| 1118803 | 12468 | `		if( rc == SXERR_ABORT ){` |
|      13 | 12469 | `			SySetRelease(&sExprNode);` |
|      13 | 12470 | `			return SXERR_ABORT;` |
|       - | 12471 | `		}` |
|  559394 | 12472 | `	}` |
| 1118803 | 12473 | `	SySetRelease(&sExprNode);` |
| 1118803 | 12474 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  559409 | 12475 | `}` |
|       - | 12476 | `/*` |
|       - | 12477 | ` * Return a pointer to the node construct handler associated` |
|       - | 12478 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 12479 | ` */` |
|  335106 | 12480 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 12481 | `{` |
|  335111 | 12482 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 12483 | `		/* Numeric literal: Either real or integer */` |
|  136493 | 12484 | `		return PH7_CompileNumLiteral;` |
|  198623 | 12485 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 12486 | `		/* Double quoted string */` |
|   26047 | 12487 | `		return PH7_CompileString;` |
|  172581 | 12488 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 12489 | `		/* Single quoted string */` |
|  172461 | 12490 | `		return PH7_CompileSimpleString;` |
|     125 | 12491 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 12492 | `		/* Heredoc */` |
|      71 | 12493 | `		return PH7_CompileHereDoc;` |
|      58 | 12494 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 12495 | `		/* Nowdoc */` |
|      51 | 12496 | `		return PH7_CompileNowDoc;` |
|       9 | 12497 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 12498 | `		/* Backtick quoted string */` |
|       6 | 12499 | `		return PH7_CompileBacktic;` |
|       - | 12500 | `	}` |
|       3 | 12501 | `	return 0;` |
|  167558 | 12502 | `}` |
|       - | 12503 | `/*` |
|       - | 12504 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 12505 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 12506 | ` * in write context" parse error.` |
|       - | 12507 | ` */` |
|    6720 | 12508 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 12509 | `{` |
|       - | 12510 | `	sxi32 rc;` |
|    6725 | 12511 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6723 | 12512 | `		return SXRET_OK;` |
|       - | 12513 | `	}` |
|       5 | 12514 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 12515 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 12516 | `		"Can't use nullsafe operator in write context");` |
|       3 | 12517 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3365 | 12518 | `}` |
|       - | 12519 | `/*` |
|       - | 12520 | ` * Compile an unset() statement.` |
|       - | 12521 | ` * unset($var, $arr[$key], ...);` |
|       - | 12522 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 12523 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 12524 | ` * parent array before extracting the element to unset.` |
|       - | 12525 | ` */` |
|    2876 | 12526 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 12527 | `{` |
|    2881 | 12528 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2881 | 12529 | `	sxu32 nIdx = 0;` |
|       - | 12530 | `	SyString sName;` |
|       - | 12531 | `	sxi32 rc;` |
|       - | 12532 | `	/* Jump the 'unset' keyword */` |
|    2881 | 12533 | `	pGen->pIn++;` |
|       - | 12534 | `	/* Save delimiter */` |
|    2881 | 12535 | `	pTmp = pGen->pEnd;` |
|       - | 12536 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2881 | 12537 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2881 | 12538 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 12539 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 12540 | `		SyToken *pClose;` |
|    2881 | 12541 | `		pGen->pIn++;   /* Skip '(' */` |
|    2881 | 12542 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2881 | 12543 | `		pEnd = pClose; /* Stop at ')' */` |
|    1438 | 12544 | `	}` |
|    2881 | 12545 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 12546 | `	/* Resolve the 'unset' builtin name once */` |
|    2881 | 12547 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     369 | 12548 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     369 | 12549 | `		if( pObj == 0 ){` |
|     ! 0 | 12550 | `			return SXERR_ABORT;` |
|       - | 12551 | `		}` |
|     369 | 12552 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     369 | 12553 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     182 | 12554 | `	}` |
|       - | 12555 | `	/* Compile each comma-separated argument */` |
|    9603 | 12556 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6727 | 12557 | `		if( pGen->pIn < pNext ){` |
|    6727 | 12558 | `			pGen->pEnd = pNext;` |
|    6727 | 12559 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 12560 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 12561 | `				GenStateUnsetValidator);` |
|    6727 | 12562 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12563 | `				return SXERR_ABORT;` |
|       - | 12564 | `			}` |
|    6727 | 12565 | `			if( rc != SXERR_EMPTY ){` |
|       - | 12566 | `				/* Emit call for this single argument */` |
|    6725 | 12567 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6725 | 12568 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6725 | 12569 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3360 | 12570 | `			}` |
|    3361 | 12571 | `		}` |
|       - | 12572 | `		/* Jump trailing commas */` |
|   10575 | 12573 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3853 | 12574 | `			pNext++;` |
|       5 | 12575 | `		}` |
|    6727 | 12576 | `		pGen->pIn = pNext;` |
|       5 | 12577 | `	}` |
|       - | 12578 | `	/* Skip past the closing ')' if present */` |
|    2881 | 12579 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2881 | 12580 | `		pGen->pIn++;` |
|    1438 | 12581 | `	}` |
|       - | 12582 | `	/* Restore token stream */` |
|    2881 | 12583 | `	pGen->pEnd = pTmp;` |
|    2881 | 12584 | `	return SXRET_OK;` |
|    1443 | 12585 | `}` |
|       - | 12586 | `/*` |
|       - | 12587 | ` * PHP Language construct table.` |
|       - | 12588 | ` */` |
|       - | 12589 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 12590 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 12591 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 12592 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 12593 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 12594 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 12595 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 12596 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 12597 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 12598 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 12599 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 12600 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 12601 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 12602 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 12603 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 12604 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 12605 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 12606 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 12607 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 12608 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 12609 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 12610 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 12611 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 12612 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 12613 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 12614 | `};` |
|       - | 12615 | `/*` |
|       - | 12616 | ` * Return a pointer to the statement handler routine associated` |
|       - | 12617 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 12618 | ` */` |
|  766626 | 12619 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 12620 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 12621 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 12622 | `	)` |
|       5 | 12623 | `{` |
|  766631 | 12624 | `	sxu32 n = 0;` |
| 3964504 | 12625 | `	for(;;){` |
| 7929013 | 12626 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  151747 | 12627 | `			break;` |
|       - | 12628 | `		}` |
| 7777271 | 12629 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  614889 | 12630 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 12631 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 12632 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 12633 | `					/* 'static' (class context),return null */` |
|     ! 0 | 12634 | `					return 0;` |
|       - | 12635 | `				}` |
|     ! 0 | 12636 | `			}` |
|  614884 | 12637 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       8 | 12638 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       9 | 12639 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 12640 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 12641 | `				return 0;` |
|       - | 12642 | `			}` |
|       - | 12643 | `			/* Return a pointer to the handler.` |
|       - | 12644 | `			*/` |
|  614889 | 12645 | `			return aLangConstruct[n].xConstruct;` |
|       - | 12646 | `		}` |
| 7162387 | 12647 | `		n++;` |
|       5 | 12648 | `	}` |
|  151747 | 12649 | `	if( pLookahed ){` |
|  151747 | 12650 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   42373 | 12651 | `			return PH7_CompileClassInterface;` |
|  109379 | 12652 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|  108889 | 12653 | `			return PH7_CompileClass;` |
|     495 | 12654 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      69 | 12655 | `			return PH7_CompileTrait;` |
|       - | 12656 | `		}` |
|       - | 12657 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 12658 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 12659 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 12660 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     213 | 12661 | `	}` |
|       - | 12662 | `	/* Not a language construct */` |
|     431 | 12663 | `	return 0;` |
|  383318 | 12664 | `}` |
|       - | 12665 | `/*` |
|       - | 12666 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 12667 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 12668 | ` */` |
|     426 | 12669 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 12670 | `{` |
|       - | 12671 | `	int rc;` |
|     431 | 12672 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     431 | 12673 | `	if( rc == FALSE ){` |
|     312 | 12674 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     311 | 12675 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 12676 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 12677 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 12678 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 12679 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 12680 | `			*/` |
|       - | 12681 | `			){` |
|     309 | 12682 | `				rc = TRUE;` |
|     152 | 12683 | `		}` |
|     156 | 12684 | `	}` |
|     431 | 12685 | `	return rc;` |
|       5 | 12686 | `}` |
|       - | 12687 | `/*` |
|       - | 12688 | ` * Compile a PHP chunk.` |
|       - | 12689 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12690 | ` * takes care of generating the appropriate error message.` |
|       - | 12691 | ` */` |
|  905964 | 12692 | `static sxi32 GenStateCompileChunk(` |
|       - | 12693 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 12694 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 12695 | `	)` |
|       5 | 12696 | `{` |
|       - | 12697 | `	ProcLangConstruct xCons;` |
|       - | 12698 | `	sxi32 rc;` |
|  905969 | 12699 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  696544 | 12700 | `	for(;;){` |
| 1149531 | 12701 | `		int bStmtIsDeclare = 0;` |
| 1149531 | 12702 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 12703 | `			/* No more input to process */` |
|   18143 | 12704 | `			break;` |
|       - | 12705 | `		}` |
|       - | 12706 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 12707 | `		 * below doesn't fire before the directive has a chance to run. */` |
| 1131393 | 12708 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  770511 | 12709 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  770511 | 12710 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      47 | 12711 | `				bStmtIsDeclare = 1;` |
|      21 | 12712 | `			}` |
|  385253 | 12713 | `		}` |
| 1131393 | 12714 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 12715 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 12716 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  243535 | 12717 | `			pGen->bStrictTypesLocked = 1;` |
|  121765 | 12718 | `		}` |
| 1131393 | 12719 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 12720 | `			/* Compile block */` |
|    3867 | 12721 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|    3867 | 12722 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12723 | `				break;` |
|       - | 12724 | `			}` |
|    1936 | 12725 | `		}else{` |
| 1127531 | 12726 | `			xCons = 0;` |
| 1127531 | 12727 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 12728 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 12729 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 12730 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3911 | 12731 | `				xCons = PH7_CompileClassModifiers;` |
| 1125578 | 12732 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  766631 | 12733 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 12734 | `				/* Try to extract a language construct handler */` |
|  766631 | 12735 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  766631 | 12736 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 12737 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 12738 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 12739 | `						&pGen->pIn->sData);` |
|       9 | 12740 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 12741 | `						break;` |
|       - | 12742 | `					}` |
|       - | 12743 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 12744 | `					 * this erroneous statement.` |
|       - | 12745 | `					 */` |
|       9 | 12746 | `					xCons = PH7_ErrorRecover;` |
|       4 | 12747 | `				}` |
|  740312 | 12748 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   50113 | 12749 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 12750 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 12751 | `				xCons = PH7_CompileLabel;` |
|      56 | 12752 | `			}` |
| 1127531 | 12753 | `			if( xCons == 0 ){` |
|       - | 12754 | `				/* Assume an expression an try to compile it */` |
|  357305 | 12755 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  357305 | 12756 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 12757 | `					/* Pop l-value */` |
|  357155 | 12758 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  178575 | 12759 | `				}` |
|  178655 | 12760 | `			}else{` |
|       - | 12761 | `				/* Go compile the sucker */` |
|  770231 | 12762 | `				rc = xCons(&(*pGen));` |
|       - | 12763 | `			}` |
| 1127531 | 12764 | `			if( rc == SXERR_ABORT ){` |
|       - | 12765 | `				/* Request to abort compilation */` |
|      13 | 12766 | `				break;` |
|       - | 12767 | `			}` |
|       - | 12768 | `		}` |
|       - | 12769 | `		/* Ignore trailing semi-colons ';' */` |
| 1817005 | 12770 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  685627 | 12771 | `			pGen->pIn++;` |
|       5 | 12772 | `		}` |
| 1131383 | 12773 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 12774 | `			/* Compile a single statement and return */` |
|  887821 | 12775 | `			break;` |
|       - | 12776 | `		}` |
|       - | 12777 | `		/* LOOP ONE */` |
|       - | 12778 | `		/* LOOP TWO */` |
|       - | 12779 | `		/* LOOP THREE */` |
|       - | 12780 | `		/* LOOP FOUR */` |
|       5 | 12781 | `	}` |
|       - | 12782 | `	/* Return compilation status */` |
|  905969 | 12783 | `	return rc;` |
|       5 | 12784 | `}` |
|       - | 12785 | `/*` |
|       - | 12786 | ` * Compile a Raw PHP chunk.` |
|       - | 12787 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12788 | ` * takes care of generating the appropriate error message.` |
|       - | 12789 | ` */` |
|   18150 | 12790 | `static sxi32 PH7_CompilePHP(` |
|       - | 12791 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 12792 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 12793 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 12794 | `	)` |
|       5 | 12795 | `{` |
|   18155 | 12796 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 12797 | `	sxi32 rc;` |
|       - | 12798 | `	/* Reset the token set */` |
|   18155 | 12799 | `	SySetReset(&(*pTokenSet));` |
|       - | 12800 | `	/* Mark as the default token set */` |
|   18155 | 12801 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 12802 | `	/* Advance the stream cursor */` |
|   18155 | 12803 | `	pGen->pRawIn++;` |
|       - | 12804 | `	/* Tokenize the PHP chunk first */` |
|   18155 | 12805 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 12806 | `	/* Point to the head and tail of the token stream. */` |
|   18155 | 12807 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   18155 | 12808 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   18155 | 12809 | `	if( is_expr ){` |
|     ! 0 | 12810 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 12811 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 12812 | `			/* A simple expression,compile it */` |
|     ! 0 | 12813 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 12814 | `		}` |
|       - | 12815 | `		/* Emit the DONE instruction */` |
|     ! 0 | 12816 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 12817 | `		return SXRET_OK;` |
|       - | 12818 | `	}` |
|   18155 | 12819 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 12820 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 12821 | `		/*` |
|       - | 12822 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 12823 | `		 * According to the PHP reference manual:` |
|       - | 12824 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 12825 | `		 *  immediately follow` |
|       - | 12826 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 12827 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 12828 | `		 * Symisc extension:` |
|       - | 12829 | `		 *   This short syntax works with all PHP opening` |
|       - | 12830 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 12831 | `		 *   only short tag.` |
|       - | 12832 | `		 */` |
|       - | 12833 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 12834 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 12835 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 12836 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 12837 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 12838 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 12839 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 12840 | `		}` |
|       3 | 12841 | `		return SXRET_OK;` |
|       - | 12842 | `	}` |
|       - | 12843 | `	/* Compile the PHP chunk */` |
|   18153 | 12844 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 12845 | `	/* Fix exceptions jumps */` |
|   18153 | 12846 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 12847 | `	/* Fix gotos now, the jump destination is resolved */` |
|   18153 | 12848 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 12849 | `		rc = SXERR_ABORT;` |
|       1 | 12850 | `	}` |
|       - | 12851 | `	/* Reset container */` |
|   18153 | 12852 | `	SySetReset(&pGen->aGoto);` |
|   18153 | 12853 | `	SySetReset(&pGen->aLabel);` |
|   18153 | 12854 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 12855 | `	/* Compilation result */` |
|   18153 | 12856 | `	return rc;` |
|    9080 | 12857 | `}` |
|       - | 12858 | `/*` |
|       - | 12859 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 12860 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 12861 | ` * This is the only compile interface exported from this file.` |
|       - | 12862 | ` */` |
|   21164 | 12863 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 12864 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 12865 | `	SyString *pScript,  /* Script to compile */` |
|       - | 12866 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 12867 | `	)` |
|       5 | 12868 | `{` |
|       - | 12869 | `	SySet aPhpToken,aRawToken;` |
|       - | 12870 | `	ph7_gen_state *pCodeGen;` |
|       - | 12871 | `	ph7_value *pRawObj;` |
|       - | 12872 | `	sxu32 nObjIdx;` |
|       - | 12873 | `	sxi32 nRawObj;` |
|       - | 12874 | `	int is_expr;` |
|       - | 12875 | `	sxi8 bSavedStrict;` |
|       - | 12876 | `	sxi8 bSavedStrictLocked;` |
|       - | 12877 | `	sxi32 rc;` |
|   21169 | 12878 | `	if( pScript->nByte < 1 ){` |
|       - | 12879 | `		/* Nothing to compile */` |
|     ! 0 | 12880 | `		return PH7_OK;` |
|       - | 12881 | `	}` |
|       - | 12882 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 12883 | `	 * file's flags so include/require restore them on return. */` |
|   21169 | 12884 | `	pCodeGen = &pVm->sCodeGen;` |
|   21169 | 12885 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   21169 | 12886 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   21169 | 12887 | `	pCodeGen->bStrictTypes = 0;` |
|   21169 | 12888 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 12889 | `	/* Initialize the tokens containers */` |
|   21169 | 12890 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   21169 | 12891 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   21169 | 12892 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   21169 | 12893 | `	is_expr = 0;` |
|   21169 | 12894 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 12895 | `		SyToken sTmp;` |
|       - | 12896 | `		/* PHP only: -*/` |
|    7791 | 12897 | `		sTmp.nLine = 1;` |
|    7791 | 12898 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    7791 | 12899 | `		sTmp.pUserData = 0;` |
|    7791 | 12900 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    7791 | 12901 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    7791 | 12902 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 12903 | `			/* A simple PHP expression */` |
|     ! 0 | 12904 | `			is_expr = 1;` |
|     ! 0 | 12905 | `		}` |
|    3898 | 12906 | `	}else{` |
|       - | 12907 | `		/* Tokenize raw text */` |
|   13383 | 12908 | `		SySetAlloc(&aRawToken,32);` |
|   13383 | 12909 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 12910 | `	}` |
|       - | 12911 | `	/* Process high-level tokens */` |
|   21169 | 12912 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   21169 | 12913 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   21169 | 12914 | `	rc = PH7_OK;` |
|   21169 | 12915 | `	if( is_expr ){` |
|       - | 12916 | `		/* Compile the expression */` |
|     ! 0 | 12917 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 12918 | `		goto cleanup;` |
|       - | 12919 | `	}` |
|   21169 | 12920 | `	nObjIdx = 0;` |
|       - | 12921 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 12922 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 12923 | `	 * preventing namespace bleeding across include()d files. */` |
|   21169 | 12924 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 12925 | `	/* Start the compilation process */` |
|   17277 | 12926 | `	for(;;){` |
|   52697 | 12927 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   21157 | 12928 | `			break; /* No more tokens to process */` |
|       - | 12929 | `		}` |
|   31545 | 12930 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 12931 | `			/* Compile the PHP chunk */` |
|   18155 | 12932 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   18155 | 12933 | `			if( rc == SXERR_ABORT ){` |
|      15 | 12934 | `				break;` |
|       - | 12935 | `			}` |
|   18143 | 12936 | `			continue;` |
|       - | 12937 | `		}` |
|       - | 12938 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13395 | 12939 | `		nRawObj = 0;` |
|   26827 | 12940 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 12941 | `			/* Consume the raw chunk without any processing */` |
|   13437 | 12942 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13437 | 12943 | `			if( pRawObj == 0 ){` |
|     ! 0 | 12944 | `				rc = SXERR_MEM;` |
|     ! 0 | 12945 | `				break;` |
|       - | 12946 | `			}` |
|       - | 12947 | `			/* Mark as constant and emit the load constant instruction */` |
|   13437 | 12948 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13437 | 12949 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13437 | 12950 | `			++nRawObj;` |
|   13437 | 12951 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 12952 | `		}` |
|   13395 | 12953 | `		if( nRawObj > 0 ){` |
|       - | 12954 | `			/* Emit the consume instruction */` |
|   13395 | 12955 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6695 | 12956 | `		}` |
|   10587 | 12957 | `	}` |
|   10582 | 12958 | `cleanup:` |
|   21169 | 12959 | `	SySetRelease(&aRawToken);` |
|   21169 | 12960 | `	SySetRelease(&aPhpToken);` |
|       - | 12961 | `	/* Restore outer file's strict_types scope */` |
|   21169 | 12962 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   21169 | 12963 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   21169 | 12964 | `	return rc;` |
|   10587 | 12965 | `}` |
|       - | 12966 | `/*` |
|       - | 12967 | ` * Utility routines.Initialize the code generator.` |
|       - | 12968 | ` */` |
|    3844 | 12969 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 12970 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12971 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12972 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12973 | `	)` |
|       5 | 12974 | `{` |
|    3849 | 12975 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12976 | `	/* Zero the structure */` |
|    3849 | 12977 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 12978 | `	/* Initial state */` |
|    3849 | 12979 | `	pGen->pVm  = &(*pVm);` |
|    3849 | 12980 | `	pGen->xErr = xErr;` |
|    3849 | 12981 | `	pGen->pErrData = pErrData;` |
|    3849 | 12982 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3849 | 12983 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3849 | 12984 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3849 | 12985 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3849 | 12986 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 12987 | `	/* Error log buffer */` |
|    3849 | 12988 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 12989 | `	/* General purpose working buffer */` |
|    3849 | 12990 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 12991 | `	/* Namespace state */` |
|    3849 | 12992 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3849 | 12993 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3849 | 12994 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3849 | 12995 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12996 | `	/* Create the global scope */` |
|    3849 | 12997 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 12998 | `	/* Point to the global scope */` |
|    3849 | 12999 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3849 | 13000 | `	return SXRET_OK;` |
|       5 | 13001 | `}` |
|       - | 13002 | `/*` |
|       - | 13003 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 13004 | ` */` |
|   24636 | 13005 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 13006 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 13007 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 13008 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 13009 | `	)` |
|       5 | 13010 | `{` |
|   24641 | 13011 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 13012 | `	GenBlock *pBlock,*pParent;` |
|       - | 13013 | `	/* Reset state */` |
|   24641 | 13014 | `	SySetReset(&pGen->aLabel);` |
|   24641 | 13015 | `	SySetReset(&pGen->aGoto);` |
|   24641 | 13016 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   24641 | 13017 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   24641 | 13018 | `	SyBlobRelease(&pGen->sWorker);` |
|   24641 | 13019 | `	SyBlobRelease(&pGen->sNamespace);` |
|   24641 | 13020 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   24641 | 13021 | `	SyHashRelease(&pGen->hUseImports);` |
|   24641 | 13022 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   24641 | 13023 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   24641 | 13024 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   24641 | 13025 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   24641 | 13026 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 13027 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 13028 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 13029 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 13030 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 13031 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 13032 | `	 * number of unique names, which is acceptable. */` |
|       - | 13033 | `	/* Point to the global scope */` |
|   24641 | 13034 | `	pBlock = pGen->pCurrent;` |
|   24641 | 13035 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 13036 | `		pParent = pBlock->pParent;` |
|     ! 0 | 13037 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 13038 | `		pBlock = pParent;` |
|     ! 0 | 13039 | `	}` |
|   24641 | 13040 | `	pGen->xErr = xErr;` |
|   24641 | 13041 | `	pGen->pErrData = pErrData;` |
|   24641 | 13042 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   24641 | 13043 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   24641 | 13044 | `	pGen->pIn = pGen->pEnd = 0;` |
|   24641 | 13045 | `	pGen->nErr = 0;` |
|   24641 | 13046 | `	return SXRET_OK;` |
|       5 | 13047 | `}` |
|       - | 13048 | `/*` |
|       - | 13049 | ` * Generate a compile-time error message.` |
|       - | 13050 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 13051 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 13052 | ` * abort compilation immediately.` |
|       - | 13053 | ` */` |
|     642 | 13054 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 13055 | `{` |
|     647 | 13056 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     647 | 13057 | `	const char *zErr = "Error";` |
|       - | 13058 | `	SyString *pFile;` |
|       - | 13059 | `	va_list ap;` |
|       - | 13060 | `	sxi32 rc;` |
|       - | 13061 | `	/* Reset the working buffer */` |
|     647 | 13062 | `	SyBlobReset(pWorker);` |
|       - | 13063 | `	/* Peek the processed file path if available */` |
|     647 | 13064 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     647 | 13065 | `	if( nErrType == E_ERROR ){` |
|       - | 13066 | `		/* Increment the error counter */` |
|     533 | 13067 | `		pGen->nErr++;` |
|     533 | 13068 | `		if( pGen->nErr > 15 ){` |
|       - | 13069 | `			/* Error count limit reached */` |
|       6 | 13070 | `			if( pGen->xErr ){` |
|       6 | 13071 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       6 | 13072 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       6 | 13073 | `				if( pFile ){` |
|       6 | 13074 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 13075 | `				}` |
|       6 | 13076 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       6 | 13077 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       6 | 13078 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 13079 | `				}` |
|       2 | 13080 | `			}` |
|       - | 13081 | `			/* Abort immediately */` |
|       6 | 13082 | `			return SXERR_ABORT;` |
|       - | 13083 | `		}` |
|     262 | 13084 | `	}` |
|     643 | 13085 | `	if( pGen->xErr == 0 ){` |
|       - | 13086 | `		/* No available error consumer,return immediately */` |
|       3 | 13087 | `		return SXRET_OK;` |
|       - | 13088 | `	}` |
|     640 | 13089 | `	switch(nErrType){` |
|     526 | 13090 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      32 | 13091 | `	case E_WARNING: zErr = "Warning";     break;` |
|      82 | 13092 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      12 | 13093 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 13094 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 13095 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 13096 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 13097 | `	default:` |
|     ! 0 | 13098 | `		break;` |
|       - | 13099 | `	}` |
|     640 | 13100 | `	rc = SXRET_OK;` |
|       - | 13101 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     640 | 13102 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     640 | 13103 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     640 | 13104 | `	va_start(ap,zFormat);` |
|     640 | 13105 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     640 | 13106 | `	va_end(ap);` |
|     640 | 13107 | `	if( pFile ){` |
|     640 | 13108 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     318 | 13109 | `	}` |
|       - | 13110 | `	/* Append a new line */` |
|     640 | 13111 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     640 | 13112 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 13113 | `		/* Consume the generated error message */` |
|     640 | 13114 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     318 | 13115 | `	}` |
|     640 | 13116 | `	return rc;` |
|     326 | 13117 | `}` |
|       - | 13118 |  |
