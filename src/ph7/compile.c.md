# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6563/8122 lines (80.81%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits |  Line | Source |
| -------: | ----: | :--- |
|        - |     1 | `/**` |
|        - |     2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |     3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |     4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |     5 | ` */` |
|        - |     6 | `#include "ph7int.h"` |
|        - |     7 | `/*` |
|        - |     8 | ` * This file implement a thread-safe and full-reentrant compiler for the PH7 engine.` |
|        - |     9 | ` * That is, routines defined in this file takes a stream of tokens and output` |
|        - |    10 | ` * PH7 bytecode instructions.` |
|        - |    11 | ` */` |
|        - |    12 | `/* Forward declaration */` |
|        - |    13 | `typedef struct LangConstruct LangConstruct;` |
|        - |    14 | `typedef struct JumpFixup     JumpFixup;` |
|        - |    15 | `typedef struct Label         Label;` |
|        - |    16 | `/* Block [i.e: set of statements] control flags */` |
|        - |    17 | `#define GEN_BLOCK_LOOP        0x001    /* Loop block [i.e: for,while,...] */` |
|        - |    18 | `#define GEN_BLOCK_PROTECTED   0x002    /* Protected block */` |
|        - |    19 | `#define GEN_BLOCK_COND        0x004    /* Conditional block [i.e: if(condition){} ]*/` |
|        - |    20 | `#define GEN_BLOCK_FUNC        0x008    /* Function body */` |
|        - |    21 | `#define GEN_BLOCK_GLOBAL      0x010    /* Global block (always set)*/` |
|        - |    22 | `#define GEN_BLOC_NESTED_FUNC  0x020    /* Nested function body */` |
|        - |    23 | `#define GEN_BLOCK_EXPR        0x040    /* Expression */` |
|        - |    24 | `#define GEN_BLOCK_STD         0x080    /* Standard block */` |
|        - |    25 | `#define GEN_BLOCK_EXCEPTION   0x100    /* Exception block [i.e: try{ } }*/` |
|        - |    26 | `#define GEN_BLOCK_SWITCH      0x200    /* Switch statement */` |
|        - |    27 | `/*` |
|        - |    28 | ` * Each label seen in the input is recorded in an instance` |
|        - |    29 | ` * of the following structure.` |
|        - |    30 | ` * A label is a target point [i.e: a jump destination] that is specified` |
|        - |    31 | ` * by an identifier followed by a colon.` |
|        - |    32 | ` * Example` |
|        - |    33 | ` *  LABEL:` |
|        - |    34 | ` *		echo "hello\n";` |
|        - |    35 | ` */` |
|        - |    36 | `struct Label` |
|        - |    37 | `{` |
|        - |    38 | `	ph7_vm_func *pFunc;  /* Compiled function where the label was declared.NULL otherwise */` |
|        - |    39 | `	sxu32 nJumpDest;     /* Jump destination */` |
|        - |    40 | `	SyString sName;      /* Label name */` |
|        - |    41 | `	sxu32 nLine;         /* Line number this label occurs */` |
|        - |    42 | `	sxu8 bRef;           /* True if the label was referenced */` |
|        - |    43 | `};` |
|        - |    44 | `/*` |
|        - |    45 | ` * Compilation of some PHP constructs such as if, for, while, the logical or` |
|        - |    46 | ` * (\|\|) and logical and (&&) operators in expressions requires the` |
|        - |    47 | ` * generation of forward jumps.` |
|        - |    48 | ` * Since the destination PC target of these jumps isn't known when the jumps` |
|        - |    49 | ` * are emitted, we record each forward jump in an instance of the following` |
|        - |    50 | ` * structure. Those jumps are fixed later when the jump destination is resolved.` |
|        - |    51 | ` */` |
|        - |    52 | `struct JumpFixup` |
|        - |    53 | `{` |
|        - |    54 | `	sxi32 nJumpType;     /* Jump type. Either TRUE jump, FALSE jump or Unconditional jump */` |
|        - |    55 | `	sxu32 nInstrIdx;     /* Instruction index to fix later when the jump destination is resolved. */` |
|        - |    56 | `	/* The following fields are only used by the goto statement */` |
|        - |    57 | `	SyString sLabel;    /* Label name */` |
|        - |    58 | `	ph7_vm_func *pFunc; /* Compiled function inside which the goto was emitted. NULL otherwise */` |
|        - |    59 | `	sxu32 nLine;        /* Track line number */` |
|        - |    60 | `};` |
|        - |    61 | `/*` |
|        - |    62 | ` * Each language construct is represented by an instance` |
|        - |    63 | ` * of the following structure.` |
|        - |    64 | ` */` |
|        - |    65 | `struct LangConstruct` |
|        - |    66 | `{` |
|        - |    67 | `	sxu32 nID;                     /* Language construct ID [i.e: PH7_TKWRD_WHILE,PH7_TKWRD_FOR,PH7_TKWRD_IF...] */` |
|        - |    68 | `	ProcLangConstruct xConstruct;  /* C function implementing the language construct */` |
|        - |    69 | `};` |
|        - |    70 | `/* Compilation flags */` |
|        - |    71 | `#define PH7_COMPILE_SINGLE_STMT 0x001 /* Compile a single statement */` |
|        - |    72 | `/* Token stream synchronization macros */` |
|        - |    73 | `#define SWAP_TOKEN_STREAM(GEN,START,END)\` |
|        - |    74 | `	pTmp  = GEN->pEnd;\` |
|        - |    75 | `	pGen->pIn  = START;\` |
|        - |    76 | `	pGen->pEnd = END` |
|        - |    77 | `#define UPDATE_TOKEN_STREAM(GEN)\` |
|        - |    78 | `	if( GEN->pIn < pTmp ){\` |
|        - |    79 | `	    GEN->pIn++;\` |
|        - |    80 | `	}\` |
|        - |    81 | `	GEN->pEnd = pTmp` |
|        - |    82 | `#define SWAP_DELIMITER(GEN,START,END)\` |
|        - |    83 | `	pTmpIn  = GEN->pIn;\` |
|        - |    84 | `	pTmpEnd = GEN->pEnd;\` |
|        - |    85 | `	GEN->pIn = START;\` |
|        - |    86 | `	GEN->pEnd = END` |
|        - |    87 | `#define RE_SWAP_DELIMITER(GEN)\` |
|        - |    88 | `	GEN->pIn  = pTmpIn;\` |
|        - |    89 | `	GEN->pEnd = pTmpEnd` |
|        - |    90 | `/* Flags related to expression compilation */` |
|        - |    91 | `#define EXPR_FLAG_LOAD_IDX_STORE    0x001 /* Set the iP2 flag when dealing with the LOAD_IDX instruction */` |
|        - |    92 | `#define EXPR_FLAG_RDONLY_LOAD       0x002 /* Read-only load, refer to the 'PH7_OP_LOAD' VM instruction for more information */` |
|        - |    93 | `#define EXPR_FLAG_COMMA_STATEMENT   0x004 /* Treat comma expression as a single statement (used by class attributes) */` |
|        - |    94 | `#define EXPR_FLAG_LOAD_IDX_ISSET    0x008 /* LOAD_IDX argument is the LHS of isset() — emit iP2=4 (offsetExists) */` |
|        - |    95 | `#define EXPR_FLAG_LOAD_IDX_UNSET    0x010 /* LOAD_IDX argument is the LHS of unset() — emit iP2=5 (offsetUnset) */` |
|        - |    96 | `#define EXPR_FLAG_LOAD_IDX_EMPTY    0x020 /* LOAD_IDX argument is the LHS of empty() — emit iP2=6 (offsetExists+offsetGet) */` |
|        - |    97 | `#define EXPR_FLAG_MEMBER_WRITE      0x040 /* Sub-tree is the write lvalue of an assignment: tag a target` |
|        - |    98 | `                                           * OP_MEMBER iP2=PH7_MEMBER_WRITE so the VM auto-creates a missing` |
|        - |    99 | ``                                           * property (e.g. `$o->arr[$k] = v`, `$o->p ??= v`). Propagated`` |
|        - |   100 | `                                           * from the precedence-18 lvalue through SUBSCRIPT to the base` |
|        - |   101 | ``                                            * member; stripped when descending into an intermediate `->` `` |
|        - |   102 | `                                           * container (the container is read, not the write target). */` |
|        - |   103 | `/* Forward declaration */` |
|        - |   104 | `static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));` |
|        - |   105 | `/*` |
|        - |   106 | ` * Local utility routines used in the code generation phase.` |
|        - |   107 | ` */` |
|        - |   108 | `/*` |
|        - |   109 | ` * Check if the given name refer to a valid label.` |
|        - |   110 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|        - |   111 | ` * Any other return value indicates no such label.` |
|        - |   112 | ` */` |
|      148 |   113 | `static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)` |
|        5 |   114 | `{` |
|        - |   115 | `	Label *aLabel;` |
|        - |   116 | `	sxu32 n;` |
|        - |   117 | `	/* Perform a linear scan on the label table */` |
|      153 |   118 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|      333 |   119 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      277 |   120 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|        - |   121 | `			/* Jump destination found */` |
|       96 |   122 | `			aLabel[n].bRef = TRUE;` |
|       96 |   123 | `			if( ppOut ){` |
|       96 |   124 | `				*ppOut = &aLabel[n];` |
|       46 |   125 | `			}` |
|       96 |   126 | `			return SXRET_OK;` |
|        - |   127 | `		}` |
|       93 |   128 | `	}` |
|        - |   129 | `	/* No such destination */` |
|       60 |   130 | `	return SXERR_NOTFOUND;` |
|       79 |   131 | `}` |
|        - |   132 | `/*` |
|        - |   133 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|        - |   134 | ` * compiled blocks.` |
|        - |   135 | ` * Return a pointer to that block on success. NULL otherwise.` |
|        - |   136 | ` */` |
|    58550 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|        5 |   138 | `{` |
|    58555 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   136214 |   140 | `	for(;;){` |
|   272433 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    58447 |   142 | `			iCount--; /* Decrement nesting level */` |
|    58447 |   143 | `			if( iCount < 1 ){` |
|        - |   144 | `				/* Block meet with the desired criteria */` |
|    58421 |   145 | `				return pBlock;` |
|        - |   146 | `			}` |
|       13 |   147 | `		}` |
|        - |   148 | `		/* Point to the upper block */` |
|   214017 |   149 | `		pBlock = pBlock->pParent;` |
|   214017 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|        - |   151 | `			/* Forbidden */` |
|       72 |   152 | `			break;` |
|        - |   153 | `		}` |
|        5 |   154 | `	}` |
|        - |   155 | `	/* No such block */` |
|      139 |   156 | `	return 0;` |
|    29280 |   157 | `}` |
|        - |   158 | `/*` |
|        - |   159 | ` * Initialize a freshly allocated block instance.` |
|        - |   160 | ` */` |
|  5837664 |   161 | `static void GenStateInitBlock(` |
|        - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|        - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|        - |   166 | `	void *pUserData      /* Upper layer private data */` |
|        - |   167 | `	)` |
|        5 |   168 | `{` |
|        - |   169 | `	/* Initialize block fields */` |
|  5837669 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  5837669 |   171 | `	pBlock->pUserData   = pUserData;` |
|  5837669 |   172 | `	pBlock->pGen        = pGen;` |
|  5837669 |   173 | `	pBlock->iFlags      = iType;` |
|  5837669 |   174 | `	pBlock->pParent     = 0;` |
|  5837669 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5837669 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5837669 |   177 | `}` |
|        - |   178 | `/*` |
|        - |   179 | ` * Allocate a new block instance.` |
|        - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|        - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|        - |   182 | ` * processing on failure.` |
|        - |   183 | ` */` |
|  5833780 |   184 | `static sxi32 GenStateEnterBlock(` |
|        - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|        - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|        - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|        - |   190 | `	)` |
|        5 |   191 | `{` |
|        - |   192 | `	GenBlock *pBlock;` |
|        - |   193 | `	/* Allocate a new block instance */` |
|  5833785 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  5833785 |   195 | `	if( pBlock == 0 ){` |
|        - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   198 | `		 */` |
|      ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|        - |   200 | `		/* Abort processing immediately */` |
|      ! 0 |   201 | `		return SXERR_ABORT;` |
|        - |   202 | `	}` |
|        - |   203 | `	/* Zero the structure */` |
|  5833785 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  5833785 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|        - |   206 | `	/* Link to the parent block */` |
|  5833785 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|        - |   208 | `	/* Mark as the current block */` |
|  5833785 |   209 | `	pGen->pCurrent = pBlock;` |
|  5833785 |   210 | `	if( ppBlock ){` |
|        - |   211 | `		/* Write a pointer to the new instance */` |
|  2826223 |   212 | `		*ppBlock = pBlock;` |
|  1413109 |   213 | `	}` |
|  5833785 |   214 | `	return SXRET_OK;` |
|  2916895 |   215 | `}` |
|        - |   216 | `/*` |
|        - |   217 | ` * Release block fields without freeing the whole instance.` |
|        - |   218 | ` */` |
|  5833772 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|        5 |   220 | `{` |
|  5833777 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  5833777 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  5833777 |   223 | `}` |
|        - |   224 | `/*` |
|        - |   225 | ` * Release a block.` |
|        - |   226 | ` */` |
|  5833772 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|        5 |   228 | `{` |
|  5833777 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  5833777 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|        - |   231 | `	/* Free the instance */` |
|  5833777 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  5833777 |   233 | `}` |
|        - |   234 | `/*` |
|        - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|        - |   236 | ` */` |
|  5833772 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|        5 |   238 | `{` |
|  5833777 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  5833777 |   240 | `	if( pBlock == 0 ){` |
|        - |   241 | `		/* No more block to pop */` |
|      ! 0 |   242 | `		return SXERR_EMPTY;` |
|        - |   243 | `	}` |
|        - |   244 | `	/* Point to the upper block */` |
|  5833777 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  5833777 |   246 | `	if( ppBlock ){` |
|        - |   247 | `		/* Write a pointer to the popped block */` |
|      ! 0 |   248 | `		*ppBlock = pBlock;` |
|      ! 0 |   249 | `	}else{` |
|        - |   250 | `		/* Safely release the block */` |
|  5833777 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|        - |   252 | `	}` |
|  5833777 |   253 | `	return SXRET_OK;` |
|  2916891 |   254 | `}` |
|        - |   255 | `/*` |
|        - |   256 | ` * Emit a forward jump.` |
|        - |   257 | ` * Notes on forward jumps` |
|        - |   258 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|        - |   259 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|        - |   260 | ` *  generation of forward jumps.` |
|        - |   261 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|        - |   262 | ` *  are emitted, we record each forward jump in an instance of the following` |
|        - |   263 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|        - |   264 | ` */` |
|  2208880 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|        5 |   266 | `{` |
|        - |   267 | `	JumpFixup sJumpFix;` |
|        - |   268 | `	sxi32 rc;` |
|        - |   269 | `	/* Init the JumpFixup structure */` |
|  2208885 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  2208885 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|        - |   272 | `	/* Insert in the jump fixup table */` |
|  2208885 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  2208885 |   274 | `	return rc;` |
|        5 |   275 | `}` |
|        - |   276 | `/*` |
|        - |   277 | ` * Fix a forward jump now the jump destination is resolved.` |
|        - |   278 | ` * Return the total number of fixed jumps.` |
|        - |   279 | ` * Notes on forward jumps:` |
|        - |   280 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|        - |   281 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|        - |   282 | ` *  generation of forward jumps.` |
|        - |   283 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|        - |   284 | ` *  are emitted, we record each forward jump in an instance of the following` |
|        - |   285 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|        - |   286 | ` */` |
|  4152348 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|        5 |   288 | `{` |
|        - |   289 | `	JumpFixup *aFix;` |
|        - |   290 | `	VmInstr *pInstr;` |
|        - |   291 | `	sxu32 nFixed;` |
|        - |   292 | `	sxu32 n;` |
|        - |   293 | `	/* Point to the jump fixup table */` |
|  4152353 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|        - |   295 | `	/* Fix the desired jumps */` |
|  8083133 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  3930785 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|        - |   298 | `			/* Already fixed */` |
|  1410333 |   299 | `			continue;` |
|        - |   300 | `		}` |
|  2520457 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|        - |   302 | `			/* Not of our interest */` |
|   311579 |   303 | `			continue;` |
|        - |   304 | `		}` |
|        - |   305 | `		/* Point to the instruction to fix */` |
|  2208883 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  2208883 |   307 | `		if( pInstr ){` |
|  2208883 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  2208883 |   309 | `			nFixed++;` |
|        - |   310 | `			/* Mark as fixed */` |
|  2208883 |   311 | `			aFix[n].nJumpType = -1;` |
|  1104439 |   312 | `		}` |
|  1104444 |   313 | `	}` |
|        - |   314 | `	/* Total number of fixed jumps */` |
|  4152353 |   315 | `	return nFixed;` |
|        5 |   316 | `}` |
|        - |   317 | `/*` |
|        - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|        - |   319 | ` * The goto statement can be used to jump to another section` |
|        - |   320 | ` * in the program.` |
|        - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|        - |   322 | ` * statement for more information.` |
|        - |   323 | ` */` |
|  1458662 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|        5 |   325 | `{` |
|        - |   326 | `	JumpFixup *pJump,*aJumps;` |
|        - |   327 | `	Label *pLabel,*aLabel;` |
|        - |   328 | `	VmInstr *pInstr;` |
|        - |   329 | `	sxi32 rc;` |
|        - |   330 | `	sxu32 n;` |
|        - |   331 | `	/* Point to the goto table */` |
|  1458667 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|        - |   333 | `	/* Fix */` |
|  1458813 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|      153 |   335 | `		pJump = &aJumps[n];` |
|        - |   336 | `		/* Extract the target label */` |
|      153 |   337 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|      153 |   338 | `		if( rc != SXRET_OK ){` |
|        - |   339 | `			/* No such label */` |
|       60 |   340 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|       60 |   341 | `			if( rc == SXERR_ABORT ){` |
|        3 |   342 | `				return SXERR_ABORT;` |
|        - |   343 | `			}` |
|       58 |   344 | `			continue;` |
|        - |   345 | `		}` |
|        - |   346 | `		/* Make sure the target label is reachable */` |
|       96 |   347 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|       11 |   348 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|       11 |   349 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |   350 | `				return SXERR_ABORT;` |
|        - |   351 | `			}` |
|        4 |   352 | `		}` |
|        - |   353 | `		/* Fix the jump now the destination is resolved */` |
|       96 |   354 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|       96 |   355 | `		if( pInstr ){` |
|       96 |   356 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|       46 |   357 | `		}` |
|       50 |   358 | `	}` |
|  1458665 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  1458797 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|        - |   362 | `			/* Emit a warning */` |
|       40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|       24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|       12 |   365 | `		}` |
|       71 |   366 | `	}` |
|  1458665 |   367 | `	return SXRET_OK;` |
|   729336 |   368 | `}` |
|        - |   369 | `/*` |
|        - |   370 | ` * Check if a given token value is installed in the literal table.` |
|        - |   371 | ` */` |
|  7295060 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|        5 |   373 | `{` |
|        - |   374 | `	SyHashEntry *pEntry;` |
|  7295065 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  7295065 |   376 | `	if( pEntry == 0 ){` |
|  1922719 |   377 | `		return SXERR_NOTFOUND;` |
|        - |   378 | `	}` |
|  5372351 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  5372351 |   380 | `	return SXRET_OK;` |
|  3647535 |   381 | `}` |
|        - |   382 | `/*` |
|        - |   383 | ` * Install a given constant index in the literal table.` |
|        - |   384 | ` * In order to be installed, the ph7_value must be of type string.` |
|        - |   385 | ` *` |
|        - |   386 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|        - |   387 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|        - |   388 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|        - |   389 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|        - |   390 | ` * many "" literals appear in user code.` |
|        - |   391 | ` */` |
|  1922714 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|        5 |   393 | `{` |
|  1922719 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  1922719 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   961357 |   396 | `	}` |
|  1922719 |   397 | `	return SXRET_OK;` |
|        5 |   398 | `}` |
|        - |   399 | `/*` |
|        - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|        - |   401 | ` * in the constant table.` |
|        - |   402 | ` */` |
|  1287826 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|        5 |   404 | `{` |
|        - |   405 | `	ph7_value *pObj;` |
|  1287831 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|        - |   407 | `	/* Reserve a new constant */` |
|  1287831 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1287831 |   409 | `	if( pObj == 0 ){` |
|      ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   411 | `		return 0;` |
|        - |   412 | `	}` |
|  1287831 |   413 | `	*pIdx = nIdx;` |
|        - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|        - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|        - |   416 | `	 */` |
|  1287831 |   417 | `	return pObj;` |
|   643918 |   418 | `}` |
|        - |   419 | `/*` |
|        - |   420 | ` * Implementation of the PHP language constructs.` |
|        - |   421 | ` */` |
|        - |   422 | `/*` |
|        - |   423 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|        - |   424 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|        - |   425 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|        - |   426 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|        - |   427 | ` *` |
|        - |   428 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|        - |   429 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|        - |   430 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|        - |   431 | ` * surrounding callsites' zero-check fallback pattern.` |
|        - |   432 | ` */` |
|  3679150 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|        5 |   434 | `{` |
|        - |   435 | `	VmCallArgMap *pMap;` |
|  3679155 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|       39 |   437 | `	if( p3 == 0 ){` |
|       35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       35 |   439 | `		if( pMap == 0 ) return 0;` |
|       35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       35 |   441 | `		p3 = (void *)pMap;` |
|       16 |   442 | `	}` |
|       39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|       39 |   444 | `	return p3;` |
|  1839580 |   445 | `}` |
|        - |   446 | `/* Forward declaration */` |
|        - |   447 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|        - |   448 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen);` |
|        - |   449 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut);` |
|        - |   450 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);` |
|        - |   451 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut);` |
|        - |   452 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut);` |
|        - |   453 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);` |
|        - |   454 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn);` |
|        - |   455 | `/* Forward decl: union type parser is defined later in this file. */` |
|        - |   456 | `static sxi32 GenStateParseUnionTypeDecl(` |
|        - |   457 | `	ph7_gen_state *pGen,` |
|        - |   458 | `	sxu32 *pnType,` |
|        - |   459 | `	SyString *pClass,` |
|        - |   460 | `	SySet *pAlts,` |
|        - |   461 | `	sxi32 *piTypeFlags,` |
|        - |   462 | `	SyString *pTypeText,` |
|        - |   463 | `	int iNullableFlag,` |
|        - |   464 | `	int iUnionFlag,` |
|        - |   465 | `	int bAllowVoid,` |
|        - |   466 | `	sxu32 nLine` |
|        - |   467 | `);` |
|        - |   468 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|        - |   469 | `static const char * TokenTypeName(sxu32 nType);` |
|        - |   470 | `/*` |
|        - |   471 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|        - |   472 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|        - |   473 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|        - |   474 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|        - |   475 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|        - |   476 | ` * for anything larger, so correctness is preserved even for pathological` |
|        - |   477 | ` * inputs like a thousand-digit number.` |
|        - |   478 | ` */` |
|        - |   479 | `#define GEN_NUM_SCRATCH 128` |
|        - |   480 | `/*` |
|        - |   481 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|        - |   482 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|        - |   483 | ` *   base  2 => 0 or 1` |
|        - |   484 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|        - |   485 | ` *              decimal scan in the lexer)` |
|        - |   486 | ` */` |
|     1076 |   487 | `static int GenStateIsBaseDigit(int c, int base)` |
|        5 |   488 | `{` |
|     1081 |   489 | `	if( base == 16 ){ return SyisHex(c); }` |
|      982 |   490 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|      703 |   491 | `	return SyisDigit(c);` |
|      543 |   492 | `}` |
|        - |   493 | `/*` |
|        - |   494 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|        - |   495 | ` * underscore separator so the caller can report the malformed portion with` |
|        - |   496 | ` * the exact wording PHP uses:` |
|        - |   497 | ` *` |
|        - |   498 | ` *   syntax error, unexpected identifier "X"` |
|        - |   499 | ` *` |
|        - |   500 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|        - |   501 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|        - |   502 | ` * absorbed by the lexer specifically to let this validator see and report` |
|        - |   503 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|        - |   504 | ` * no forward rescan needed.` |
|        - |   505 | ` *` |
|        - |   506 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|        - |   507 | ` * returns 0 when it is well-formed.` |
|        - |   508 | ` */` |
|  1288814 |   509 | `static int GenStateFindBadNumericSeparator(` |
|        - |   510 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   511 | `{` |
|  1288819 |   512 | `	const char *z = pRaw->zString;` |
|  1288819 |   513 | `	sxu32 n = pRaw->nByte;` |
|  1288819 |   514 | `	int base = 10;` |
|        - |   515 | `	sxu32 i, start;` |
|  1288819 |   516 | `	if( n < 2 ) return 0;` |
|   400307 |   517 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|       80 |   518 | `		base = 16;` |
|   400268 |   519 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      284 |   520 | `		base = 2;` |
|      141 |   521 | `	}` |
|  1295099 |   522 | `	for( i = 0; i < n; ++i ){` |
|   894811 |   523 | `		if( z[i] != '_' ) continue;` |
|      546 |   524 | `		if( i > 0 && i + 1 < n` |
|      543 |   525 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|      543 |   526 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|      533 |   527 | `			continue; /* well-placed separator */` |
|        - |   528 | `		}` |
|        - |   529 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|        - |   530 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|       18 |   531 | `		start = i;` |
|       23 |   532 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|       12 |   533 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|        6 |   534 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|        2 |   535 | `		}` |
|       18 |   536 | `		*pBadStart = &z[start];` |
|       18 |   537 | `		*pBadLen = n - start;` |
|       18 |   538 | `		return 1;` |
|      ! 0 |   539 | `	}` |
|   400293 |   540 | `	return 0;` |
|   644412 |   541 | `}` |
|        - |   542 | `/*` |
|        - |   543 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   544 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   545 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   546 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   547 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   548 | ` * so callers can bail from the current construct).` |
|        - |   549 | ` */` |
|  1288814 |   550 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   551 | `{` |
|  1288819 |   552 | `	const char *zBad = 0;` |
|  1288819 |   553 | `	sxu32 nBad = 0;` |
|        - |   554 | `	SyString sBad;` |
|        - |   555 | `	sxi32 rc;` |
|  1288819 |   556 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  1288805 |   557 | `		return SXRET_OK;` |
|        - |   558 | `	}` |
|       18 |   559 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       18 |   560 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   561 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       18 |   562 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   563 | `		return SXERR_ABORT;` |
|        - |   564 | `	}` |
|       18 |   565 | `	return SXERR_SYNTAX;` |
|   644412 |   566 | `}` |
|        - |   567 | `/*` |
|        - |   568 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|        - |   569 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|        - |   570 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|        - |   571 | ` *` |
|        - |   572 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|        - |   573 | ` * and *pzAlloc is set to NULL.` |
|        - |   574 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|        - |   575 | ` * and *pzAlloc is set to NULL.` |
|        - |   576 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|        - |   577 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|        - |   578 | ` * caller with SyMemBackendFree once the converter is done.` |
|        - |   579 | ` *` |
|        - |   580 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|        - |   581 | ` * case *pOut is left untouched and the caller must not read it).` |
|        - |   582 | ` */` |
|  1288800 |   583 | `static sxi32 GenStateStripNumericSeparators(` |
|        - |   584 | `	SyMemBackend *pAlloc,` |
|        - |   585 | `	const SyString *pToken,` |
|        - |   586 | `	char *zScratch, sxu32 nScratch,` |
|        - |   587 | `	SyString *pOut, char **pzAlloc)` |
|        5 |   588 | `{` |
|        - |   589 | `	sxu32 i, j;` |
|  1288805 |   590 | `	int hasUnderscore = 0;` |
|        - |   591 | `	char *zBuf;` |
|  1288805 |   592 | `	*pzAlloc = 0;` |
|  3070043 |   593 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  1781495 |   594 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   890624 |   595 | `	}` |
|  1288805 |   596 | `	if( !hasUnderscore ){` |
|  1288553 |   597 | `		SyStringDupPtr(pOut, pToken);` |
|  1288553 |   598 | `		return SXRET_OK;` |
|        - |   599 | `	}` |
|      253 |   600 | `	if( pToken->nByte <= nScratch ){` |
|      251 |   601 | `		zBuf = zScratch;` |
|      126 |   602 | `	}else{` |
|        3 |   603 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|        3 |   604 | `		if( zBuf == 0 ){` |
|      ! 0 |   605 | `			return SXERR_ABORT;` |
|        - |   606 | `		}` |
|        3 |   607 | `		*pzAlloc = zBuf;` |
|        - |   608 | `	}` |
|      253 |   609 | `	j = 0;` |
|     2895 |   610 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|     2643 |   611 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|     1322 |   612 | `	}` |
|      253 |   613 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|      253 |   614 | `	return SXRET_OK;` |
|   644405 |   615 | `}` |
|        - |   616 | `/*` |
|        - |   617 | ` * Compile a numeric [i.e: integer or real] literal.` |
|        - |   618 | ` * Notes on the integer type.` |
|        - |   619 | ` *  According to the PHP language reference manual` |
|        - |   620 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|        - |   621 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|        - |   622 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|        - |   623 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|        - |   624 | ` * Symisc eXtension to the integer type.` |
|        - |   625 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|        - |   626 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|        - |   627 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|        - |   628 | ` *  [i.e: either 32bit or 64bit].` |
|        - |   629 | ` *  For more information on this powerfull extension please refer to the official` |
|        - |   630 | ` *  documentation.` |
|        - |   631 | ` */` |
|        - |   632 | `/*` |
|        - |   633 | ` * Determine whether an integer literal token exceeds the signed 64-bit range.` |
|        - |   634 | ` * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->` |
|        - |   635 | ` * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or` |
|        - |   636 | ` * dropping digits. pNum is the separator-stripped token (unsigned; the sign of` |
|        - |   637 | ` * a "-1" is a separate unary operator). Base detection mirrors` |
|        - |   638 | ` * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the` |
|        - |   639 | ` * float value is accumulated into *pReal (dv = dv*base + digit); for decimal` |
|        - |   640 | ` * *pbDecimal is set so the caller reuses strtod on the token for a` |
|        - |   641 | ` * correctly-rounded value. Returns FALSE (value fits) for anything it cannot` |
|        - |   642 | ` * confidently classify, so the int path stays in charge.` |
|        - |   643 | ` *` |
|        - |   644 | ` * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact` |
|        - |   645 | ` * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit` |
|        - |   646 | ` * doubling). Octal/binary overflow values can differ from php by the low bit(s):` |
|        - |   647 | ` * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's` |
|        - |   648 | ` * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a` |
|        - |   649 | ` * residual in PLAN.md; matching php exactly would need a port of those functions.` |
|        - |   650 | ` */` |
|  1287860 |   651 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |   652 | `{` |
|  1287865 |   653 | `	const char *z = pNum->zString;` |
|  1287865 |   654 | `	const char *zEnd = z + pNum->nByte;` |
|        - |   655 | `	const char *p, *q;` |
|        - |   656 | `	int n;` |
|  1287865 |   657 | `	*pbDecimal = FALSE;` |
|  1287865 |   658 | `	if( z >= zEnd ){` |
|      ! 0 |   659 | `		return FALSE;` |
|        - |   660 | `	}` |
|  1287865 |   661 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        - |   662 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|       77 |   663 | `		p = z + 2;` |
|       85 |   664 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|      493 |   665 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|       77 |   666 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|       71 |   667 | `			return FALSE;` |
|        - |   668 | `		}` |
|        7 |   669 | `		{ ph7_real dv = 0;` |
|      103 |   670 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|       97 |   671 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|       49 |   672 | `		  }` |
|        7 |   673 | `		  *pReal = dv;` |
|        - |   674 | `		}` |
|        7 |   675 | `		return TRUE;` |
|  1287789 |   676 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|        - |   677 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|      281 |   678 | `		p = z + 2;` |
|      329 |   679 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|     2149 |   680 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|      281 |   681 | `		if( n <= 63 ){` |
|      279 |   682 | `			return FALSE;` |
|        - |   683 | `		}` |
|        3 |   684 | `		{ ph7_real dv = 0;` |
|      195 |   685 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|      129 |   686 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|       65 |   687 | `		  }` |
|        3 |   688 | `		  *pReal = dv;` |
|        - |   689 | `		}` |
|        3 |   690 | `		return TRUE;` |
|  1287509 |   691 | `	}else if( z[0] == '0' ){` |
|        - |   692 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |   693 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |   694 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   355539 |   695 | `		p = z;` |
|   711075 |   696 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   355767 |   697 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   355539 |   698 | `		if( n <= 21 ){` |
|   355537 |   699 | `			return FALSE;` |
|        - |   700 | `		}` |
|        3 |   701 | `		{ ph7_real dv = 0;` |
|       47 |   702 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|       45 |   703 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|       23 |   704 | `		  }` |
|        3 |   705 | `		  *pReal = dv;` |
|        - |   706 | `		}` |
|        3 |   707 | `		return TRUE;` |
|        - |   708 | `	}` |
|        - |   709 | `	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that` |
|        - |   710 | `	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)` |
|        - |   711 | `	 * for php-exact rounding. */` |
|   931975 |   712 | `	p = z;` |
|   931975 |   713 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  2351215 |   714 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   931975 |   715 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       25 |   716 | `		*pbDecimal = TRUE;` |
|       25 |   717 | `		return TRUE;` |
|        - |   718 | `	}` |
|   931951 |   719 | `	return FALSE;` |
|   643935 |   720 | `}` |
|  1288786 |   721 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   722 | `{` |
|  1288791 |   723 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  1288791 |   724 | `	sxu32 nIdx = 0;` |
|        - |   725 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  1288791 |   726 | `	char *zAlloc = 0;` |
|        - |   727 | `	SyString sNum;` |
|        - |   728 | `	sxi32 rc;` |
|   644393 |   729 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  1288791 |   730 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  1288791 |   731 | `	if( rc != SXRET_OK ){` |
|       14 |   732 | `		return rc;` |
|        - |   733 | `	}` |
|  1933169 |   734 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   644388 |   735 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  1288781 |   736 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   737 | `		return SXERR_ABORT;` |
|        - |   738 | `	}` |
|  1288781 |   739 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |   740 | `		ph7_value *pObj;` |
|        - |   741 | `		sxi64 iValue;` |
|  1287865 |   742 | `		ph7_real rOverflow = 0;` |
|  1287865 |   743 | `		int bDecimalOverflow = 0;` |
|  1287865 |   744 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|        - |   745 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|        - |   746 | `			 * float instead of wrapping/dropping digits. */` |
|       35 |   747 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       35 |   748 | `			if( pObj == 0 ){` |
|      ! 0 |   749 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   750 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   751 | `				return SXERR_ABORT;` |
|        - |   752 | `			}` |
|       35 |   753 | `			if( bDecimalOverflow ){` |
|        - |   754 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|       25 |   755 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|       25 |   756 | `				PH7_MemObjToReal(pObj);` |
|       13 |   757 | `			}else{` |
|       11 |   758 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|        - |   759 | `			}` |
|       18 |   760 | `		}else{` |
|  1287831 |   761 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  1287831 |   762 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  1287831 |   763 | `			if( pObj == 0 ){` |
|      ! 0 |   764 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   765 | `				return SXERR_ABORT;` |
|        - |   766 | `			}` |
|  1287831 |   767 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |   768 | `		}` |
|   643935 |   769 | `	}else{` |
|        - |   770 | `		/* Real number */` |
|        - |   771 | `		ph7_value *pObj;` |
|        - |   772 | `		/* Reserve a new constant */` |
|      920 |   773 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      920 |   774 | `		if( pObj == 0 ){` |
|      ! 0 |   775 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   776 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   777 | `			return SXERR_ABORT;` |
|        - |   778 | `		}` |
|      920 |   779 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      920 |   780 | `		PH7_MemObjToReal(pObj);` |
|        - |   781 | `	}` |
|  1288781 |   782 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |   783 | `	/* Emit the load constant instruction */` |
|  1288781 |   784 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |   785 | `	/* Node successfully compiled */` |
|  1288781 |   786 | `	return SXRET_OK;` |
|   644398 |   787 | `}` |
|        - |   788 | `/*` |
|        - |   789 | ` * Compile a single quoted string.` |
|        - |   790 | ` * According to the PHP language reference manual:` |
|        - |   791 | ` *` |
|        - |   792 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|        - |   793 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|        - |   794 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|        - |   795 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|        - |   796 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|        - |   797 | ` *` |
|        - |   798 | ` */` |
|  2968212 |   799 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   800 | `{` |
|  2968217 |   801 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |   802 | `	const char *zIn,*zCur,*zEnd;` |
|        - |   803 | `	ph7_value *pObj;` |
|        - |   804 | `	sxu32 nIdx;` |
|  2968217 |   805 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |   806 | `	/* Delimit the string */` |
|  2968217 |   807 | `	zIn  = pStr->zString;` |
|  2968217 |   808 | `	zEnd = &zIn[pStr->nByte];` |
|  2968217 |   809 | `	if( zIn >= zEnd ){` |
|        - |   810 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |   811 | `		 * rather than reserving a new object each time. */` |
|   136133 |   812 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   136133 |   813 | `		return SXRET_OK;` |
|        - |   814 | `	}` |
|  2832089 |   815 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |   816 | `		/* Already processed,emit the load constant instruction` |
|        - |   817 | `		 * and return.` |
|        - |   818 | `		 */` |
|  1821863 |   819 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  1821863 |   820 | `		return SXRET_OK;` |
|        - |   821 | `	}` |
|        - |   822 | `	/* Reserve a new constant */` |
|  1010231 |   823 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1010231 |   824 | `	if( pObj == 0 ){` |
|      ! 0 |   825 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   826 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |   827 | `		return SXERR_ABORT;` |
|        - |   828 | `	}` |
|  1010231 |   829 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |   830 | `	/* Compile the node */` |
|  1010285 |   831 | `	for(;;){` |
|  2020575 |   832 | `		if( zIn >= zEnd ){` |
|        - |   833 | `			/* End of input */` |
|  1010231 |   834 | `			break;` |
|        - |   835 | `		}` |
|  1010349 |   836 | `		zCur = zIn;` |
| 19785227 |   837 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 18774883 |   838 | `			zIn++;` |
|        5 |   839 | `		}` |
|  1010349 |   840 | `		if( zIn > zCur ){` |
|        - |   841 | `			/* Append raw contents*/` |
|   979251 |   842 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   489623 |   843 | `		}` |
|  1010349 |   844 | `		zIn++;` |
|  1010349 |   845 | `		if( zIn < zEnd ){` |
|    31217 |   846 | `			if( zIn[0] == '\\' ){` |
|        - |   847 | `				/* A literal backslash */` |
|    31105 |   848 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    15664 |   849 | `			}else if( zIn[0] == '\'' ){` |
|        - |   850 | `				/* A single quote */` |
|       11 |   851 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|        6 |   852 | `			}else{` |
|        - |   853 | `				/* verbatim copy */` |
|      104 |   854 | `				zIn--;` |
|      104 |   855 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      104 |   856 | `				zIn++;` |
|        - |   857 | `			}` |
|    15606 |   858 | `		}` |
|        - |   859 | `		/* Advance the stream cursor */` |
|  1010349 |   860 | `		zIn++;` |
|        5 |   861 | `	}` |
|        - |   862 | `	/* Emit the load constant instruction */` |
|  1010231 |   863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  1010231 |   864 | `	if( pStr->nByte < 1024 ){` |
|        - |   865 | `		/* Install in the literal table */` |
|  1010231 |   866 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   505113 |   867 | `	}` |
|        - |   868 | `	/* Node successfully compiled */` |
|  1010231 |   869 | `	return SXRET_OK;` |
|  1484111 |   870 | `}` |
|        - |   871 | `/*` |
|        - |   872 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|        - |   873 | ` *` |
|        - |   874 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|        - |   875 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|        - |   876 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|        - |   877 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|        - |   878 | ` * original source buffer — the buffer is stable through compilation.` |
|        - |   879 | ` *` |
|        - |   880 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|        - |   881 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|        - |   882 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|        - |   883 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|        - |   884 | ` *     at least N)" — line too short, or first differing byte is not` |
|        - |   885 | ` *     whitespace.` |
|        - |   886 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|        - |   887 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|        - |   888 | ` */` |
|      114 |   889 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|        5 |   890 | `{` |
|      119 |   891 | `	SyString *pIn = &pGen->pIn->sData;` |
|      119 |   892 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - |   893 | `	const char *zPrefix;` |
|        - |   894 | `	const char *z, *zEnd;` |
|        - |   895 | `	char *zBuf, *zDst;` |
|      119 |   896 | `	if( nIndent == 0 ){` |
|        - |   897 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|       73 |   898 | `		*pOut = *pIn;` |
|       73 |   899 | `		return SXRET_OK;` |
|        - |   900 | `	}` |
|        - |   901 | `	/* Recover the marker indent prefix from the original source buffer.` |
|        - |   902 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|        - |   903 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|        - |   904 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|        - |   905 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|        - |   906 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|       48 |   907 | `	zPrefix = pIn->zString + pIn->nByte;` |
|       48 |   908 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|      ! 0 |   909 | `		zPrefix += 2;` |
|      ! 0 |   910 | `	}else{` |
|       48 |   911 | `		zPrefix += 1;` |
|        - |   912 | `	}` |
|        - |   913 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|       48 |   914 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|       48 |   915 | `	if( zBuf == 0 ){` |
|      ! 0 |   916 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |   917 | `		return SXERR_ABORT;` |
|        - |   918 | `	}` |
|       48 |   919 | `	zDst = zBuf;` |
|       48 |   920 | `	z = pIn->zString;` |
|       48 |   921 | `	zEnd = z + pIn->nByte;` |
|      130 |   922 | `	while( z < zEnd ){` |
|       72 |   923 | `		const char *zLine = z;` |
|        - |   924 | `		sxu32 nLine;` |
|        - |   925 | `		int bEmpty;` |
|      800 |   926 | `		while( z < zEnd && z[0] != '\n' ){` |
|      732 |   927 | `			z++;` |
|        4 |   928 | `		}` |
|       72 |   929 | `		nLine = (sxu32)(z - zLine);` |
|       72 |   930 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|       72 |   931 | `		if( !bEmpty ){` |
|        - |   932 | `			sxu32 i;` |
|       68 |   933 | `			if( nLine < nIndent ){` |
|      ! 0 |   934 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |   935 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|      ! 0 |   936 | `					nIndent);` |
|      ! 0 |   937 | `				return SXERR_ABORT;` |
|        - |   938 | `			}` |
|      270 |   939 | `			for( i = 0; i < nIndent; i++ ){` |
|      214 |   940 | `				if( zLine[i] != zPrefix[i] ){` |
|       11 |   941 | `					unsigned char c = (unsigned char)zLine[i];` |
|       11 |   942 | `					if( c == ' ' \|\| c == '\t' ){` |
|        6 |   943 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |   944 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|        4 |   945 | `					}else{` |
|        8 |   946 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |   947 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|        2 |   948 | `							nIndent);` |
|        - |   949 | `					}` |
|       11 |   950 | `					return SXERR_ABORT;` |
|        - |   951 | `				}` |
|      104 |   952 | `			}` |
|       57 |   953 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|       57 |   954 | `			zDst += nLine - nIndent;` |
|       33 |   955 | `		}else if( nLine == 1 ){` |
|        - |   956 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|      ! 0 |   957 | `			*zDst++ = '\r';` |
|      ! 0 |   958 | `		}` |
|       61 |   959 | `		if( z < zEnd ){` |
|       25 |   960 | `			*zDst++ = '\n';` |
|       25 |   961 | `			z++;` |
|       12 |   962 | `		}` |
|        1 |   963 | `	}` |
|       37 |   964 | `	pOut->zString = zBuf;` |
|       37 |   965 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|       37 |   966 | `	return SXRET_OK;` |
|       62 |   967 | `}` |
|        - |   968 | `/*` |
|        - |   969 | ` * Compile a nowdoc string.` |
|        - |   970 | ` * According to the PHP language reference manual:` |
|        - |   971 | ` *` |
|        - |   972 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |   973 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |   974 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|        - |   975 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|        - |   976 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|        - |   977 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|        - |   978 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|        - |   979 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|        - |   980 | ` *  of the closing identifier.` |
|        - |   981 | ` */` |
|       48 |   982 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        3 |   983 | `{` |
|        - |   984 | `	SyString sStripped;` |
|        - |   985 | `	SyString *pStr;` |
|        - |   986 | `	ph7_value *pObj;` |
|        - |   987 | `	sxu32 nIdx;` |
|        - |   988 | `	sxi32 rc;` |
|       51 |   989 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       51 |   990 | `	if( rc != SXRET_OK ){` |
|        6 |   991 | `		return rc;` |
|        - |   992 | `	}` |
|       46 |   993 | `	pStr = &sStripped;` |
|       46 |   994 | `	nIdx = 0; /* Prevent compiler warning */` |
|       46 |   995 | `	if( pStr->nByte <= 0 ){` |
|        - |   996 | `		/* Empty string,load NULL */` |
|        7 |   997 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        7 |   998 | `		return SXRET_OK;` |
|        - |   999 | `	}` |
|        - |  1000 | `	/* Reserve a new constant */` |
|       40 |  1001 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       40 |  1002 | `	if( pObj == 0 ){` |
|      ! 0 |  1003 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  1004 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  1005 | `		return SXERR_ABORT;` |
|        - |  1006 | `	}` |
|        - |  1007 | `	/* No processing is done here, simply a memcpy() operation */` |
|       40 |  1008 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|        - |  1009 | `	/* Emit the load constant instruction */` |
|       40 |  1010 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  1011 | `	/* Node successfully compiled */` |
|       40 |  1012 | `	return SXRET_OK;` |
|       27 |  1013 | `}` |
|        - |  1014 | `/*` |
|        - |  1015 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|        - |  1016 | ` * According to the PHP language reference manual` |
|        - |  1017 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|        - |  1018 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|        - |  1019 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|        - |  1020 | ` *  property in a string with a minimum of effort.` |
|        - |  1021 | ` *  Simple syntax` |
|        - |  1022 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|        - |  1023 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|        - |  1024 | ` *   the end of the name.` |
|        - |  1025 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|        - |  1026 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|        - |  1027 | ` *   as to simple variables.` |
|        - |  1028 | ` *  Complex (curly) syntax` |
|        - |  1029 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|        - |  1030 | ` *   of complex expressions.` |
|        - |  1031 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|        - |  1032 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|        - |  1033 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|        - |  1034 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|        - |  1035 | ` */` |
|     2462 |  1036 | `static sxi32 GenStateProcessStringExpression(` |
|        - |  1037 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  1038 | `	sxu32 nLine,         /* Line number */` |
|        - |  1039 | `	const char *zIn,     /* Raw expression */` |
|        - |  1040 | `	const char *zEnd     /* End of the expression */` |
|        - |  1041 | `	)` |
|        5 |  1042 | `{` |
|        - |  1043 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  1044 | `	SySet sToken;` |
|        - |  1045 | `	sxi32 rc;` |
|        - |  1046 | `	/* Initialize the token set */` |
|     2467 |  1047 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        - |  1048 | `	/* Preallocate some slots */` |
|     2467 |  1049 | `	SySetAlloc(&sToken,0x08);` |
|        - |  1050 | `	/* Tokenize the text */` |
|     2467 |  1051 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|        - |  1052 | `	/* Swap delimiter */` |
|     2467 |  1053 | `	pTmpIn  = pGen->pIn;` |
|     2467 |  1054 | `	pTmpEnd = pGen->pEnd;` |
|     2467 |  1055 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     2467 |  1056 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        - |  1057 | `	/* Compile the expression */` |
|     2467 |  1058 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  1059 | `	/* Restore token stream */` |
|     2467 |  1060 | `	pGen->pIn  = pTmpIn;` |
|     2467 |  1061 | `	pGen->pEnd = pTmpEnd;` |
|        - |  1062 | `	/* Release the token set */` |
|     2467 |  1063 | `	SySetRelease(&sToken);` |
|        - |  1064 | `	/* Compilation result */` |
|     2467 |  1065 | `	return rc;` |
|        5 |  1066 | `}` |
|        - |  1067 | `/*` |
|        - |  1068 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|        - |  1069 | ` */` |
|    38198 |  1070 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  1071 | `{` |
|        - |  1072 | `	ph7_value *pConstObj;` |
|    38203 |  1073 | `	sxu32 nIdx = 0;` |
|        - |  1074 | `	/* Reserve a new constant */` |
|    38203 |  1075 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    38203 |  1076 | `	if( pConstObj == 0 ){` |
|      ! 0 |  1077 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  1078 | `		return 0;` |
|        - |  1079 | `	}` |
|    38203 |  1080 | `	(*pCount)++;` |
|    38203 |  1081 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  1082 | `	/* Emit the load constant instruction */` |
|    38203 |  1083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    38203 |  1084 | `	return pConstObj;` |
|    19104 |  1085 | `}` |
|        - |  1086 | `/*` |
|        - |  1087 | ` * Compile a double quoted/heredoc string.` |
|        - |  1088 | ` * According to the PHP language reference manual` |
|        - |  1089 | ` * Heredoc` |
|        - |  1090 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  1091 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  1092 | ` *  to close the quotation.` |
|        - |  1093 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  1094 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  1095 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  1096 | ` *  Warning` |
|        - |  1097 | ` *  It is very important to note that the line with the closing identifier must contain` |
|        - |  1098 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|        - |  1099 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|        - |  1100 | ` *  It's also important to realize that the first character before the closing identifier must` |
|        - |  1101 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|        - |  1102 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|        - |  1103 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|        - |  1104 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|        - |  1105 | ` *  the end of the current file, a parse error will result at the last line.` |
|        - |  1106 | ` *  Heredocs can not be used for initializing class properties.` |
|        - |  1107 | ` * Double quoted` |
|        - |  1108 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|        - |  1109 | ` *  Escaped characters Sequence 	Meaning` |
|        - |  1110 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|        - |  1111 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|        - |  1112 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|        - |  1113 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|        - |  1114 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|        - |  1115 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|        - |  1116 | ` *  \\ backslash` |
|        - |  1117 | ` *  \$ dollar sign` |
|        - |  1118 | ` *  \" double-quote` |
|        - |  1119 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|        - |  1120 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|        - |  1121 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|        - |  1122 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|        - |  1123 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|        - |  1124 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|        - |  1125 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|        - |  1126 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|        - |  1127 | ` * See string parsing for details.` |
|        - |  1128 | ` */` |
|        - |  1129 | `/*` |
|        - |  1130 | ` * Line number of an escape sequence inside the string body being compiled:` |
|        - |  1131 | ` * the token's line plus every newline before the escape (php reports the` |
|        - |  1132 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|        - |  1133 | ` * on the line after the '<<<' marker, hence the +1.` |
|        - |  1134 | ` */` |
|        6 |  1135 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|        3 |  1136 | `{` |
|        9 |  1137 | `	const char *z = pGen->pIn->sData.zString;` |
|        9 |  1138 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|       15 |  1139 | `	for( ; z < zPos ; z++ ){` |
|        9 |  1140 | `		if( z[0] == '\n' ){` |
|      ! 0 |  1141 | `			nLine++;` |
|      ! 0 |  1142 | `		}` |
|        6 |  1143 | `	}` |
|        9 |  1144 | `	return nLine;` |
|        3 |  1145 | `}` |
|        - |  1146 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|        - |  1147 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|    36684 |  1148 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  1149 | `{` |
|    36689 |  1150 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  1151 | `	const char *zIn,*zCur,*zEnd;` |
|    36689 |  1152 | `	ph7_value *pObj = 0;` |
|        - |  1153 | `	sxi32 iCons;` |
|        - |  1154 | `	sxi32 rc;` |
|        - |  1155 | `	/* Delimit the string */` |
|    36689 |  1156 | `	zIn  = pStr->zString;` |
|    36689 |  1157 | `	zEnd = &zIn[pStr->nByte];` |
|    36689 |  1158 | `	if( zIn >= zEnd ){` |
|        - |  1159 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  1160 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  1161 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  1162 | `		 */` |
|      375 |  1163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      375 |  1164 | `		return SXRET_OK;` |
|        - |  1165 | `	}` |
|    36319 |  1166 | `	zCur = 0;` |
|        - |  1167 | `	/* Compile the node */` |
|    36319 |  1168 | `	iCons = 0;` |
|    19388 |  1169 | `	for(;;){` |
|    62485 |  1170 | `		zCur = zIn;` |
|   214305 |  1171 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   154287 |  1172 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       72 |  1173 | `				break;` |
|   154153 |  1174 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2332 |  1175 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1167 |  1176 | `					break;` |
|        - |  1177 | `			}` |
|   151825 |  1178 | `			zIn++;` |
|        5 |  1179 | `		}` |
|    62485 |  1180 | `		if( zIn > zCur ){` |
|    20285 |  1181 | `			if( pObj == 0 ){` |
|    19755 |  1182 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    19755 |  1183 | `				if( pObj == 0 ){` |
|      ! 0 |  1184 | `					return SXERR_ABORT;` |
|        - |  1185 | `				}` |
|     9875 |  1186 | `			}` |
|    20285 |  1187 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    10140 |  1188 | `		}` |
|    62485 |  1189 | `		if( zIn >= zEnd ){` |
|    36317 |  1190 | `			break;` |
|        - |  1191 | `		}` |
|    26173 |  1192 | `		if( zIn[0] == '\\' ){` |
|    23711 |  1193 | `			const char *zPtr = 0;` |
|        - |  1194 | `			sxu32 n;` |
|    23711 |  1195 | `			zIn++;` |
|    23711 |  1196 | `			if( pObj == 0 ){` |
|    18453 |  1197 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    18453 |  1198 | `				if( pObj == 0 ){` |
|      ! 0 |  1199 | `					return SXERR_ABORT;` |
|        - |  1200 | `				}` |
|     9224 |  1201 | `			}` |
|    23711 |  1202 | `			if( zIn >= zEnd ){` |
|        - |  1203 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  1204 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  1205 | `				break;` |
|        - |  1206 | `			}` |
|    23709 |  1207 | `			n = sizeof(char); /* size of conversion */` |
|    23709 |  1208 | `			switch( zIn[0] ){` |
|       11 |  1209 | `			case '$':` |
|        - |  1210 | `				/* Dollar sign */` |
|       25 |  1211 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       25 |  1212 | `				break;` |
|       57 |  1213 | `			case '\\':` |
|        - |  1214 | `				/* A literal backslash */` |
|      119 |  1215 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      119 |  1216 | `				break;` |
|        1 |  1217 | `			case 'e':` |
|        - |  1218 | `				/* Escape (ESC) ASCII code 27 */` |
|        3 |  1219 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|        3 |  1220 | `				break;` |
|        4 |  1221 | `			case 'f':` |
|        - |  1222 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|        9 |  1223 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|        9 |  1224 | `				break;` |
|    11300 |  1225 | `			case 'n':` |
|        - |  1226 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    22605 |  1227 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    22605 |  1228 | `				break;` |
|       19 |  1229 | `			case 'r':` |
|        - |  1230 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|       43 |  1231 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|       43 |  1232 | `				break;` |
|       27 |  1233 | `			case 't':` |
|        - |  1234 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|       59 |  1235 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|       59 |  1236 | `				break;` |
|        3 |  1237 | `			case 'v':` |
|        - |  1238 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|        7 |  1239 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|        7 |  1240 | `				break;` |
|      113 |  1241 | `			case '"':` |
|      231 |  1242 | `				if( bHeredoc ){` |
|        - |  1243 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|        5 |  1244 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|        3 |  1245 | `				}else{` |
|        - |  1246 | `					/* Double quote */` |
|      227 |  1247 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|        - |  1248 | `				}` |
|      231 |  1249 | `				break;` |
|       25 |  1250 | `			case '0': case '1': case '2': case '3':` |
|        - |  1251 | `			case '4': case '5': case '6': case '7': {` |
|        - |  1252 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|        - |  1253 | `				 * warns and wraps to the low byte, matching php 8. */` |
|       52 |  1254 | `				int c = 0;` |
|        - |  1255 | `				char cOut;` |
|      148 |  1256 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      126 |  1257 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|       15 |  1258 | `						break;` |
|        - |  1259 | `					}` |
|       98 |  1260 | `					c = c * 8 + (zPtr[0] - '0');` |
|       50 |  1261 | `				}` |
|       52 |  1262 | `				if( c > 0xFF ){` |
|        - |  1263 | `					SyString sSeq;` |
|        3 |  1264 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|        3 |  1265 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  1266 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|        3 |  1267 | `					c &= 0xFF;` |
|        1 |  1268 | `				}` |
|       52 |  1269 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|       52 |  1270 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       52 |  1271 | `				n = (sxu32)(zPtr-zIn);` |
|       52 |  1272 | `				break;` |
|        - |  1273 | `			}` |
|      271 |  1274 | `			case 'x':` |
|      812 |  1275 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|        - |  1276 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|      539 |  1277 | `					int c = SyHexToint(zIn[1]);` |
|        - |  1278 | `					char cOut;` |
|      539 |  1279 | `					n += sizeof(char);` |
|      539 |  1280 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|      535 |  1281 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|      535 |  1282 | `						n += sizeof(char);` |
|      267 |  1283 | `					}` |
|      539 |  1284 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|      539 |  1285 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|      270 |  1286 | `				}else{` |
|        - |  1287 | `					/* Not an escape: keep the backslash, as php does */` |
|        5 |  1288 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|        - |  1289 | `				}` |
|      543 |  1290 | `				break;` |
|        9 |  1291 | `			case 'u':` |
|       18 |  1292 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|       22 |  1293 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|        - |  1294 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|        - |  1295 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|        - |  1296 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|        - |  1297 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|        - |  1298 | `					 * followed by {$...} curly interpolation. */` |
|       15 |  1299 | `					sxu32 nCp = 0;` |
|       15 |  1300 | `					zPtr = &zIn[2];` |
|       59 |  1301 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|       46 |  1302 | `						if( nCp <= 0x10FFFF ){` |
|        - |  1303 | `							/* stop accumulating once out of range: keeps a long` |
|        - |  1304 | `							 * digit run from wrapping sxu32 */` |
|       46 |  1305 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|       22 |  1306 | `						}` |
|       46 |  1307 | `						zPtr++;` |
|        2 |  1308 | `					}` |
|       15 |  1309 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|        - |  1310 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|        - |  1311 | `						 * malformed sequence so later errors are still reported. */` |
|        3 |  1312 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  1313 | `							"Invalid UTF-8 codepoint escape sequence");` |
|        3 |  1314 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  1315 | `							return SXERR_ABORT;` |
|        - |  1316 | `						}` |
|        3 |  1317 | `						n = (sxu32)(zPtr-zIn);` |
|        3 |  1318 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|        3 |  1319 | `							n += sizeof(char);` |
|        1 |  1320 | `						}` |
|        3 |  1321 | `						break;` |
|        - |  1322 | `					}` |
|       12 |  1323 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|       12 |  1324 | `					if( nCp > 0x10FFFF ){` |
|        3 |  1325 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  1326 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|        3 |  1327 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  1328 | `							return SXERR_ABORT;` |
|        - |  1329 | `						}` |
|        3 |  1330 | `						break;` |
|        - |  1331 | `					}` |
|        - |  1332 | `					{` |
|        - |  1333 | `						char zUtf[4];` |
|        9 |  1334 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|        9 |  1335 | `						SX_WRITE_UTF8(zOut,nCp);` |
|        9 |  1336 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|        - |  1337 | `					}` |
|        5 |  1338 | `				}else{` |
|        - |  1339 | `					/* Not an escape: keep the backslash, as php does */` |
|        7 |  1340 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|        - |  1341 | `				}` |
|       15 |  1342 | `				break;` |
|       12 |  1343 | `			default:` |
|        - |  1344 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|        - |  1345 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|        - |  1346 | `				 * in the source buffer — one batched append. */` |
|       25 |  1347 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|       24 |  1348 | `				break;` |
|        - |  1349 | `			}` |
|        - |  1350 | `			/* Advance the stream cursor */` |
|    23709 |  1351 | `			zIn += n;` |
|    23709 |  1352 | `			continue;` |
|        - |  1353 | `		}` |
|     2467 |  1354 | `		if( zIn[0] == '{' ){` |
|        - |  1355 | `			/* Curly syntax */` |
|        - |  1356 | `			const char *zExpr;` |
|      141 |  1357 | `			sxi32 iNest = 1;` |
|      141 |  1358 | `			zIn++;` |
|      141 |  1359 | `			zExpr = zIn;` |
|        - |  1360 | `			/* Synchronize with the next closing curly braces */` |
|     1419 |  1361 | `			while( zIn < zEnd ){` |
|     1419 |  1362 | `				if( zIn[0] == '{' ){` |
|        - |  1363 | `					/* Increment nesting level */` |
|        9 |  1364 | `					iNest++;` |
|     1415 |  1365 | `				}else if(zIn[0] == '}' ){` |
|        - |  1366 | `					/* Decrement nesting level */` |
|      149 |  1367 | `					iNest--;` |
|      149 |  1368 | `					if( iNest <= 0 ){` |
|      141 |  1369 | `						break;` |
|        - |  1370 | `					}` |
|        4 |  1371 | `				}` |
|     1281 |  1372 | `				zIn++;` |
|        3 |  1373 | `			}` |
|        - |  1374 | `			/* Process the expression */` |
|      141 |  1375 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      141 |  1376 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1377 | `				return SXERR_ABORT;` |
|        - |  1378 | `			}` |
|      141 |  1379 | `			if( rc != SXERR_EMPTY ){` |
|      141 |  1380 | `				++iCons;` |
|       69 |  1381 | `			}` |
|      141 |  1382 | `			if( zIn < zEnd ){` |
|        - |  1383 | `				/* Jump the trailing curly */` |
|      141 |  1384 | `				zIn++;` |
|       69 |  1385 | `			}` |
|       72 |  1386 | `		}else{` |
|        - |  1387 | `			/* Simple syntax */` |
|     2329 |  1388 | `			const char *zExpr = zIn;` |
|        - |  1389 | `			/* Assemble variable name */` |
|     1187 |  1390 | `			for(;;){` |
|        - |  1391 | `				/* Jump leading dollars */` |
|     4703 |  1392 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|     2329 |  1393 | `					zIn++;` |
|        5 |  1394 | `				}` |
|     1187 |  1395 | `				for(;;){` |
|    12462 |  1396 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|     8901 |  1397 | `						zIn++;` |
|        5 |  1398 | `					}` |
|     2379 |  1399 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|        - |  1400 | `						/* UTF-8 stream */` |
|      ! 0 |  1401 | `						zIn++;` |
|      ! 0 |  1402 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  1403 | `							zIn++;` |
|      ! 0 |  1404 | `						}` |
|      ! 0 |  1405 | `						continue;` |
|        - |  1406 | `					}` |
|     2379 |  1407 | `					break;` |
|      ! 0 |  1408 | `				}` |
|     2379 |  1409 | `				if( zIn >= zEnd ){` |
|      250 |  1410 | `					break;` |
|        - |  1411 | `				}` |
|     2133 |  1412 | `				if( zIn[0] == '[' ){` |
|       12 |  1413 | `					sxi32 iSquare = 1;` |
|       12 |  1414 | `					zIn++;` |
|       28 |  1415 | `					while( zIn < zEnd ){` |
|       28 |  1416 | `						if( zIn[0] == '[' ){` |
|      ! 0 |  1417 | `							iSquare++;` |
|       28 |  1418 | `						}else if (zIn[0] == ']' ){` |
|       12 |  1419 | `							iSquare--;` |
|       12 |  1420 | `							if( iSquare <= 0 ){` |
|       12 |  1421 | `								break;` |
|        - |  1422 | `							}` |
|      ! 0 |  1423 | `						}` |
|       18 |  1424 | `						zIn++;` |
|        2 |  1425 | `					}` |
|       12 |  1426 | `					if( zIn < zEnd ){` |
|       12 |  1427 | `						zIn++;` |
|        5 |  1428 | `					}` |
|       12 |  1429 | `					break;` |
|     2123 |  1430 | `				}else if(zIn[0] == '{' ){` |
|        6 |  1431 | `					sxi32 iCurly = 1;` |
|        6 |  1432 | `					zIn++;` |
|       18 |  1433 | `					while( zIn < zEnd ){` |
|       16 |  1434 | `						if( zIn[0] == '{' ){` |
|      ! 0 |  1435 | `							iCurly++;` |
|       16 |  1436 | `						}else if (zIn[0] == '}' ){` |
|        3 |  1437 | `							iCurly--;` |
|        3 |  1438 | `							if( iCurly <= 0 ){` |
|        3 |  1439 | `								break;` |
|        - |  1440 | `							}` |
|      ! 0 |  1441 | `						}` |
|       14 |  1442 | `						zIn++;` |
|        2 |  1443 | `					}` |
|        6 |  1444 | `					if( zIn < zEnd ){` |
|        3 |  1445 | `						zIn++;` |
|        1 |  1446 | `					}` |
|        6 |  1447 | `					break;` |
|     2119 |  1448 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|        - |  1449 | `					/* Member access operator '->' */` |
|       53 |  1450 | `					zIn += 2;` |
|     2094 |  1451 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|        - |  1452 | `					/* Static member access operator '::' */` |
|      ! 0 |  1453 | `					zIn += 2;` |
|      ! 0 |  1454 | `				}else{` |
|     1037 |  1455 | `					break;` |
|        - |  1456 | `				}` |
|        3 |  1457 | `			}` |
|        - |  1458 | `			/* Process the expression */` |
|     2329 |  1459 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     2329 |  1460 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1461 | `				return SXERR_ABORT;` |
|        - |  1462 | `			}` |
|     2329 |  1463 | `			if( rc != SXERR_EMPTY ){` |
|     2327 |  1464 | `				++iCons;` |
|     1161 |  1465 | `			}` |
|        - |  1466 | `		}` |
|        - |  1467 | `		/* Invalidate the previously used constant */` |
|     2467 |  1468 | `		pObj = 0;` |
|        5 |  1469 | `	}/*for(;;)*/` |
|    36319 |  1470 | `	if( iCons > 1 ){` |
|        - |  1471 | `		/* Concatenate all compiled constants */` |
|     1805 |  1472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|      900 |  1473 | `	}` |
|        - |  1474 | `	/* Node successfully compiled */` |
|    36319 |  1475 | `	return SXRET_OK;` |
|    18347 |  1476 | `}` |
|        - |  1477 | `/*` |
|        - |  1478 | ` * Compile a double quoted string.` |
|        - |  1479 | ` *  See the block-comment above for more information.` |
|        - |  1480 | ` */` |
|    36622 |  1481 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1482 | `{` |
|        - |  1483 | `	sxi32 rc;` |
|    36627 |  1484 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    18311 |  1485 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  1486 | `	/* Compilation result */` |
|    36627 |  1487 | `	return rc;` |
|        5 |  1488 | `}` |
|        - |  1489 | `/*` |
|        - |  1490 | ` * Compile a Heredoc string.` |
|        - |  1491 | ` *  See the block-comment above for more information.` |
|        - |  1492 | ` */` |
|       66 |  1493 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1494 | `{` |
|        - |  1495 | `	SyString sOrig, sStripped;` |
|        - |  1496 | `	sxi32 rc;` |
|       71 |  1497 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       71 |  1498 | `	if( rc != SXRET_OK ){` |
|        6 |  1499 | `		return rc;` |
|        - |  1500 | `	}` |
|        - |  1501 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|        - |  1502 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|        - |  1503 | `	 * Restore before returning so downstream code that references pIn is` |
|        - |  1504 | `	 * unaffected, including on the error path. */` |
|       65 |  1505 | `	sOrig = pGen->pIn->sData;` |
|       65 |  1506 | `	pGen->pIn->sData = sStripped;` |
|       65 |  1507 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|       65 |  1508 | `	pGen->pIn->sData = sOrig;` |
|       31 |  1509 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       65 |  1510 | `	return rc;` |
|       38 |  1511 | `}` |
|        - |  1512 | `/*` |
|        - |  1513 | ` * Compile an array entry whether it is a key or a value.` |
|        - |  1514 | ` *  Notes on array entries.` |
|        - |  1515 | ` *  According to the PHP language reference manual` |
|        - |  1516 | ` *  An array can be created by the array() language construct.` |
|        - |  1517 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|        - |  1518 | ` *  array(  key =>  value` |
|        - |  1519 | ` *    , ...` |
|        - |  1520 | ` *    )` |
|        - |  1521 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|        - |  1522 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|        - |  1523 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|        - |  1524 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|        - |  1525 | ` *  contain integer and string indices.` |
|        - |  1526 | ` *  A value can be any PHP type.` |
|        - |  1527 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|        - |  1528 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|        - |  1529 | ` *  is specified, that value will be overwritten.` |
|        - |  1530 | ` */` |
|   514294 |  1531 | `static sxi32 GenStateCompileArrayEntry(` |
|        - |  1532 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  1533 | `	SyToken *pIn,        /* Token stream */` |
|        - |  1534 | `	SyToken *pEnd,       /* End of the token stream */` |
|        - |  1535 | `	sxi32 iFlags,        /* Compilation flags */` |
|        - |  1536 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|        - |  1537 | `	)` |
|        5 |  1538 | `{` |
|        - |  1539 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  1540 | `	sxi32 rc;` |
|        - |  1541 | `	/* Swap token stream */` |
|   514299 |  1542 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - |  1543 | `	/* Compile the expression*/` |
|   514299 |  1544 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - |  1545 | `	/* Restore token stream */` |
|   514299 |  1546 | `	RE_SWAP_DELIMITER(pGen);` |
|   514299 |  1547 | `	return rc;` |
|        5 |  1548 | `}` |
|        - |  1549 | `/*` |
|        - |  1550 | ` * Expression tree validator callback for the 'array' language construct.` |
|        - |  1551 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - |  1552 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - |  1553 | ` * error message.` |
|        - |  1554 | ` * See the routine responible of compiling the array language construct` |
|        - |  1555 | ` * for more inforation.` |
|        - |  1556 | ` */` |
|       36 |  1557 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  1558 | `{` |
|       41 |  1559 | `	sxi32 rc = SXRET_OK;` |
|       41 |  1560 | `	if( pRoot->pOp ){` |
|       14 |  1561 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|       12 |  1562 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|       17 |  1563 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|        - |  1564 | `			/* Unexpected expression */` |
|       14 |  1565 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1566 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|       14 |  1567 | `			if( rc != SXERR_ABORT ){` |
|       14 |  1568 | `				rc = SXERR_INVALID;` |
|        5 |  1569 | `			}` |
|       10 |  1570 | `		}` |
|       31 |  1571 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  1572 | `		/* Unexpected expression */` |
|        3 |  1573 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1574 | `			"array(): Expecting a variable after reference operator '&'");` |
|        3 |  1575 | `		if( rc != SXERR_ABORT ){` |
|        3 |  1576 | `			rc = SXERR_INVALID;` |
|        1 |  1577 | `		}` |
|        1 |  1578 | `	}` |
|       41 |  1579 | `	return rc;` |
|        5 |  1580 | `}` |
|        - |  1581 | `/*` |
|        - |  1582 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|        - |  1583 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|        - |  1584 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|        - |  1585 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|        - |  1586 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|        - |  1587 | ` */` |
|   551874 |  1588 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 |  1589 | `{` |
|   551879 |  1590 | `	SyToken *pCur = pStart;` |
|   551879 |  1591 | `	sxi32 iNest = 0;` |
|  1677735 |  1592 | `	while( pCur < pEnd ){` |
|  1329985 |  1593 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   204125 |  1594 | `			return pCur;` |
|        - |  1595 | `		}` |
|        - |  1596 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - |  1597 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - |  1598 | `		 * not an entry separator. Skip past the signature.` |
|        - |  1599 | `		 */` |
|  1125865 |  1600 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|    19517 |  1601 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    19517 |  1602 | `			SyToken *pFn = pCur;` |
|    19512 |  1603 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|      ! 0 |  1604 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|        5 |  1605 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  1606 | `				pFn = &pCur[1];` |
|      ! 0 |  1607 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  1608 | `			}` |
|    19517 |  1609 | `			if( nKw == PH7_TKWRD_FN ){` |
|        5 |  1610 | `				pCur = pFn + 1; /* past 'fn' */` |
|        5 |  1611 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  1612 | `					pCur++;` |
|      ! 0 |  1613 | `				}` |
|        5 |  1614 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        5 |  1615 | `					pCur++;` |
|        5 |  1616 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - |  1617 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        5 |  1618 | `					if( pCur < pEnd ){` |
|        5 |  1619 | `						pCur++;` |
|        2 |  1620 | `					}` |
|        2 |  1621 | `				}` |
|        5 |  1622 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|      ! 0 |  1623 | `					pCur++;` |
|      ! 0 |  1624 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|      ! 0 |  1625 | `						&& pCur->sData.nByte == 1` |
|      ! 0 |  1626 | `						&& pCur->sData.zString[0] == '?' ){` |
|      ! 0 |  1627 | `						pCur++;` |
|      ! 0 |  1628 | `					}` |
|      ! 0 |  1629 | `					if( pCur < pEnd` |
|      ! 0 |  1630 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  1631 | `						pCur++;` |
|      ! 0 |  1632 | `					}` |
|      ! 0 |  1633 | `				}` |
|        - |  1634 | `				/* The rest of the entry is the arrow-function body — no outer` |
|        - |  1635 | `				 * key to extract. */` |
|        5 |  1636 | `				return pEnd;` |
|        - |  1637 | `			}` |
|        - |  1638 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|        - |  1639 | `			 * entry separator. Skip past the full match span. */` |
|    19513 |  1640 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|        3 |  1641 | `				pCur++; /* past 'match' */` |
|        3 |  1642 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        3 |  1643 | `					pCur++;` |
|        3 |  1644 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - |  1645 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        3 |  1646 | `					if( pCur < pEnd ){` |
|        3 |  1647 | `						pCur++;` |
|        1 |  1648 | `					}` |
|        1 |  1649 | `				}` |
|        3 |  1650 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|        3 |  1651 | `					pCur++;` |
|        3 |  1652 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - |  1653 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|        3 |  1654 | `					if( pCur < pEnd ){` |
|        3 |  1655 | `						pCur++;` |
|        1 |  1656 | `					}` |
|        1 |  1657 | `				}` |
|        3 |  1658 | `				continue;` |
|        - |  1659 | `			}` |
|     9753 |  1660 | `		}` |
|  1125859 |  1661 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    50927 |  1662 | `			iNest++;` |
|  1100398 |  1663 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - |  1664 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - |  1665 | `			 * parser will shortly detect any syntax error. */` |
|    50927 |  1666 | `			iNest--;` |
|    25461 |  1667 | `		}` |
|  1125859 |  1668 | `		pCur++;` |
|        5 |  1669 | `	}` |
|   347755 |  1670 | `	return pEnd;` |
|   275942 |  1671 | `}` |
|        - |  1672 | `/*` |
|        - |  1673 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - |  1674 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - |  1675 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - |  1676 | ` */` |
|   287080 |  1677 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 |  1678 | `{` |
|        - |  1679 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - |  1680 | `	SyToken *pKey,*pCur;` |
|   287085 |  1681 | `	sxi32 iEmitRef = 0;` |
|   287085 |  1682 | `	sxi32 iSpread = 0;` |
|   287085 |  1683 | `	sxi32 nPair = 0;` |
|        - |  1684 | `	sxi32 rc;` |
|   287085 |  1685 | `	xValidator = 0;` |
|   331662 |  1686 | `	for(;;){` |
|        - |  1687 | `		/* Jump leading commas */` |
|   943365 |  1688 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   280041 |  1689 | `			pGen->pIn++;` |
|        5 |  1690 | `		}` |
|   663329 |  1691 | `		pCur = pGen->pIn;` |
|   663329 |  1692 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - |  1693 | `			/* No more entry to process */` |
|   287069 |  1694 | `			break;` |
|        - |  1695 | `		}` |
|   376265 |  1696 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 |  1697 | `			continue;` |
|        - |  1698 | `		}` |
|        - |  1699 | `		/* Compile the key if available */` |
|   376265 |  1700 | `		pKey = pCur;` |
|   376265 |  1701 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   376265 |  1702 | `		rc = SXERR_EMPTY;` |
|   376265 |  1703 | `		if( pCur < pGen->pIn ){` |
|   137795 |  1704 | `			if( &pCur[1] >= pGen->pIn ){` |
|        - |  1705 | `				/* Missing value */` |
|       13 |  1706 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|       13 |  1707 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  1708 | `					return SXERR_ABORT;` |
|        - |  1709 | `				}` |
|       13 |  1710 | `				return SXRET_OK;` |
|        - |  1711 | `			}` |
|        - |  1712 | `			/* Compile the expression holding the key */` |
|   137785 |  1713 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|        - |  1714 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|   137785 |  1715 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1716 | `				return SXERR_ABORT;` |
|        - |  1717 | `			}` |
|   137785 |  1718 | `			pCur++; /* Jump the '=>' operator */` |
|   307365 |  1719 | `		}else if( pKey == pCur ){` |
|        - |  1720 | `			/* Key is omitted,emit a warning */` |
|      ! 0 |  1721 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|      ! 0 |  1722 | `			pCur++; /* Jump the '=>' operator */` |
|      ! 0 |  1723 | `		}else{` |
|        - |  1724 | `			/* Reset back the cursor and point to the entry value */` |
|   238475 |  1725 | `			pCur = pKey;` |
|        - |  1726 | `		}` |
|   376255 |  1727 | `		if( rc == SXERR_EMPTY ){` |
|        - |  1728 | `			/* No available key,load NULL */` |
|   238477 |  1729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|   119236 |  1730 | `		}` |
|   376255 |  1731 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|        - |  1732 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|       45 |  1733 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|       45 |  1734 | `			iEmitRef = 1;` |
|       45 |  1735 | `			pCur++; /* Jump the '&' token */` |
|       45 |  1736 | `			if( pCur >= pGen->pIn ){` |
|        - |  1737 | `				/* Missing value */` |
|        3 |  1738 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|        3 |  1739 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  1740 | `					return SXERR_ABORT;` |
|        - |  1741 | `				}` |
|        3 |  1742 | `				return SXRET_OK;` |
|        - |  1743 | `			}` |
|       19 |  1744 | `		}` |
|        - |  1745 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|        - |  1746 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|        - |  1747 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|        - |  1748 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|        - |  1749 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   376253 |  1750 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   376253 |  1751 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|        - |  1752 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|        - |  1753 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|        - |  1754 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|        - |  1755 | `			 * output is engine-portable. */` |
|        6 |  1756 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|        - |  1757 | `				"syntax error, unexpected token \"...\"");` |
|        6 |  1758 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1759 | `				return SXERR_ABORT;` |
|        - |  1760 | `			}` |
|        6 |  1761 | `			return SXRET_OK;` |
|        - |  1762 | `		}` |
|        - |  1763 | `		/* Compile indice value */` |
|   376249 |  1764 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   376249 |  1765 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  1766 | `			return SXERR_ABORT;` |
|        - |  1767 | `		}` |
|   376249 |  1768 | `		if( iSpread ){` |
|        - |  1769 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|       69 |  1770 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   376216 |  1771 | `		}else if( iEmitRef ){` |
|        - |  1772 | `			/* Emit the load reference instruction */` |
|       41 |  1773 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       18 |  1774 | `		}` |
|   376249 |  1775 | `		xValidator = 0;` |
|   376249 |  1776 | `		iEmitRef = 0;` |
|   376249 |  1777 | `		iSpread = 0;` |
|   376249 |  1778 | `		nPair++;` |
|        5 |  1779 | `	}` |
|        - |  1780 | `	/* Emit the load map instruction */` |
|   287069 |  1781 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - |  1782 | `	/* Node successfully compiled */` |
|   287069 |  1783 | `	return SXRET_OK;` |
|   143545 |  1784 | `}` |
|        - |  1785 | `/*` |
|        - |  1786 | ` * Compile the 'array' language construct.` |
|        - |  1787 | ` *	 According to the PHP language reference manual` |
|        - |  1788 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|        - |  1789 | ` *   values to keys. This type is optimized for several different uses; it can` |
|        - |  1790 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|        - |  1791 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|        - |  1792 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|        - |  1793 | ` */` |
|   285380 |  1794 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1795 | `{` |
|        - |  1796 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   285385 |  1797 | `	pGen->pIn += 2;` |
|   285385 |  1798 | `	pGen->pEnd--;` |
|   142690 |  1799 | `	SXUNUSED(iCompileFlag);` |
|   285385 |  1800 | `	return GenStateCompileArrayBody(pGen);` |
|        5 |  1801 | `}` |
|        - |  1802 | `/*` |
|        - |  1803 | ` * Compile the PHP 8.5 clone(...) call form:` |
|        - |  1804 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|        - |  1805 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|        - |  1806 | ` *                                              property updates as scope-aware writes` |
|        - |  1807 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|        - |  1808 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|        - |  1809 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|        - |  1810 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|        - |  1811 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|        - |  1812 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|        - |  1813 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|        - |  1814 | ` */` |
|       22 |  1815 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 |  1816 | `{` |
|        - |  1817 | `	SyToken *pIn,*pEnd,*pNext;` |
|       24 |  1818 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|       24 |  1819 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|       24 |  1820 | `	int nArg = 0;` |
|        - |  1821 | `	sxi32 rc;` |
|       11 |  1822 | `	SXUNUSED(iCompileFlag);` |
|        - |  1823 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|       24 |  1824 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|       24 |  1825 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|        - |  1826 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|       24 |  1827 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|      ! 0 |  1828 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  1829 | `			"clone(...) first-class callable form is not yet supported");` |
|        - |  1830 | `	}` |
|        - |  1831 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|       62 |  1832 | `	while( pIn < pEnd ){` |
|       40 |  1833 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|       40 |  1834 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|      ! 0 |  1835 | `			break;` |
|        - |  1836 | `		}` |
|       40 |  1837 | `		pArgStart = pIn;` |
|       40 |  1838 | `		pArgEnd   = pNext;` |
|        - |  1839 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|        - |  1840 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|       38 |  1841 | `		if( (pArgEnd - pArgStart) >= 2` |
|       37 |  1842 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       23 |  1843 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        5 |  1844 | `			pName = pArgStart;` |
|        5 |  1845 | `			pArgStart += 2;` |
|        2 |  1846 | `		}` |
|       40 |  1847 | `		if( pName ){` |
|        - |  1848 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|        - |  1849 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|        4 |  1850 | `			if( pName->sData.nByte == sizeof("object")-1` |
|        4 |  1851 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|        3 |  1852 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        4 |  1853 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|        3 |  1854 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|        3 |  1855 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        2 |  1856 | `			}else{` |
|      ! 0 |  1857 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|      ! 0 |  1858 | `					"Unknown named parameter $%z",&pName->sData);` |
|        1 |  1859 | `			}` |
|       38 |  1860 | `		}else if( nArg == 0 ){` |
|       22 |  1861 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|       25 |  1862 | `		}else if( nArg == 1 ){` |
|       15 |  1863 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        8 |  1864 | `		}else{` |
|      ! 0 |  1865 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|        - |  1866 | `				"clone() expects at most 2 arguments");` |
|        - |  1867 | `		}` |
|       40 |  1868 | `		nArg++;` |
|       40 |  1869 | `		pIn = pNext;` |
|       40 |  1870 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 |  1871 | `			pIn++; /* step over the argument separator */` |
|        8 |  1872 | `		}` |
|        2 |  1873 | `	}` |
|       24 |  1874 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|      ! 0 |  1875 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  1876 | `			"clone() expects at least 1 argument, 0 given");` |
|        - |  1877 | `	}` |
|        - |  1878 | `	/* Object argument -> clone (+ __clone()). */` |
|       24 |  1879 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       24 |  1880 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  1881 | `		return SXERR_ABORT;` |
|        - |  1882 | `	}` |
|       24 |  1883 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|        - |  1884 | `	/* Property updates (evaluated after __clone runs). */` |
|       24 |  1885 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|       17 |  1886 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       17 |  1887 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  1888 | `			return SXERR_ABORT;` |
|        - |  1889 | `		}` |
|       17 |  1890 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|        8 |  1891 | `	}` |
|       24 |  1892 | `	return SXRET_OK;` |
|       13 |  1893 | `}` |
|        - |  1894 | `/*` |
|        - |  1895 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|        - |  1896 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|        - |  1897 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|        - |  1898 | ` */` |
|     1700 |  1899 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1900 | `{` |
|        - |  1901 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     1705 |  1902 | `	pGen->pIn++;` |
|     1705 |  1903 | `	pGen->pEnd--;` |
|      850 |  1904 | `	SXUNUSED(iCompileFlag);` |
|     1705 |  1905 | `	return GenStateCompileArrayBody(pGen);` |
|        5 |  1906 | `}` |
|        - |  1907 | `/*` |
|        - |  1908 | ` * Expression tree validator callback for the 'list' language construct.` |
|        - |  1909 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - |  1910 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - |  1911 | ` * error message.` |
|        - |  1912 | ` * See the routine responible of compiling the list language construct` |
|        - |  1913 | ` * for more inforation.` |
|        - |  1914 | ` */` |
|      190 |  1915 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  1916 | `{` |
|      195 |  1917 | `	sxi32 rc = SXRET_OK;` |
|      195 |  1918 | `	if( pRoot->pOp ){` |
|        4 |  1919 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|        2 |  1920 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|        - |  1921 | `				/* Unexpected expression */` |
|      ! 0 |  1922 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1923 | `					"list(): Expecting a variable not an expression");` |
|      ! 0 |  1924 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  1925 | `					rc = SXERR_INVALID;` |
|      ! 0 |  1926 | `				}` |
|        1 |  1927 | `		}` |
|      193 |  1928 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  1929 | `		/* Unexpected expression */` |
|        6 |  1930 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1931 | `			"list(): Expecting a variable not an expression");` |
|        6 |  1932 | `		if( rc != SXERR_ABORT ){` |
|        6 |  1933 | `			rc = SXERR_INVALID;` |
|        2 |  1934 | `		}` |
|        2 |  1935 | `	}` |
|      195 |  1936 | `	return rc;` |
|        5 |  1937 | `}` |
|        - |  1938 | `/*` |
|        - |  1939 | ` * Compile the 'list' language construct.` |
|        - |  1940 | ` *  According to the PHP language reference` |
|        - |  1941 | ` *  list(): Assign variables as if they were an array.` |
|        - |  1942 | ` *  list() is used to assign a list of variables in one operation.` |
|        - |  1943 | ` *  Description` |
|        - |  1944 | ` *   array list (mixed $varname [, mixed $... ] )` |
|        - |  1945 | ` *   Like array(), this is not really a function, but a language construct.` |
|        - |  1946 | ` *   list() is used to assign a list of variables in one operation.` |
|        - |  1947 | ` *  Parameters` |
|        - |  1948 | ` *   $varname: A variable.` |
|        - |  1949 | ` *  Return Values` |
|        - |  1950 | ` *   The assigned array.` |
|        - |  1951 | ` */` |
|        - |  1952 | `/* Nested list entry recorded during first pass of list body compilation */` |
|        - |  1953 | `struct NestedListEntry {` |
|        - |  1954 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|        - |  1955 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|        - |  1956 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|        - |  1957 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|        - |  1958 | `};` |
|        - |  1959 | `/*` |
|        - |  1960 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|        - |  1961 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|        - |  1962 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|        - |  1963 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|        - |  1964 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|        - |  1965 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|        - |  1966 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|        - |  1967 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|        - |  1968 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|        - |  1969 | ` */` |
|       28 |  1970 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|        2 |  1971 | `{` |
|        - |  1972 | `	SyToken *pNext;` |
|        - |  1973 | `	sxi32 rc;` |
|       66 |  1974 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|        - |  1975 | `		SyToken *pArrow,*pTarget;` |
|        - |  1976 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|       38 |  1977 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|       38 |  1978 | `		pTarget = &pArrow[1];` |
|       38 |  1979 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|        - |  1980 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|        - |  1981 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|      ! 0 |  1982 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  1983 | `				"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 |  1984 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  1985 | `		}` |
|        - |  1986 | `		/* DUP the source array (it is on the stack top) */` |
|       38 |  1987 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - |  1988 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|       38 |  1989 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|       38 |  1990 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  1991 | `			return SXERR_ABORT;` |
|        - |  1992 | `		}` |
|        - |  1993 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|        - |  1994 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|        - |  1995 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|        - |  1996 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|        - |  1997 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|        - |  1998 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|       38 |  1999 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|       38 |  2000 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|       34 |  2001 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|       18 |  2002 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|        - |  2003 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|        - |  2004 | `			 * Treat source[key] as the inner body's source, then drop the` |
|        - |  2005 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|        5 |  2006 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|        5 |  2007 | `			SyToken *pSavedIn = pGen->pIn;` |
|        5 |  2008 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        5 |  2009 | `			pGen->pIn = pTarget;` |
|        5 |  2010 | `			pGen->pEnd = pNext;` |
|        5 |  2011 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|        2 |  2012 | `			             : PH7_CompileList(&(*pGen),0);` |
|        5 |  2013 | `			pGen->pIn = pSavedIn;` |
|        5 |  2014 | `			pGen->pEnd = pSavedEnd;` |
|        5 |  2015 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2016 | `				return SXERR_ABORT;` |
|        - |  2017 | `			}` |
|        5 |  2018 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        3 |  2019 | `		}else{` |
|        - |  2020 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|        - |  2021 | `			 * is already on the stack as the value; compiling the target appends` |
|        - |  2022 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|        - |  2023 | `			 * assignment does. */` |
|        - |  2024 | `			VmInstr *pInstr;` |
|       34 |  2025 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|       34 |  2026 | `			sxi32 iP1 = 0, iP2 = 0;` |
|       34 |  2027 | `			void *p3 = 0;` |
|       34 |  2028 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|        - |  2029 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       34 |  2030 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  2031 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2032 | `			}` |
|       34 |  2033 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|       34 |  2034 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|        3 |  2035 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|       33 |  2036 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        3 |  2037 | `					iVmOp = PH7_OP_STORE_IDX;` |
|        3 |  2038 | `					iP1 = pInstr->iP1;` |
|        3 |  2039 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        2 |  2040 | `				}else{` |
|       30 |  2041 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|       30 |  2042 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - |  2043 | `				}` |
|       16 |  2044 | `			}` |
|       34 |  2045 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|        - |  2046 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|        - |  2047 | `			 * source array is back on top for the next entry. */` |
|       34 |  2048 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        - |  2049 | `		}` |
|       38 |  2050 | `		pGen->pIn = &pNext[1];` |
|        2 |  2051 | `	}` |
|       30 |  2052 | `	return SXRET_OK;` |
|       16 |  2053 | `}` |
|        - |  2054 | `/*` |
|        - |  2055 | ` * Shared body for list() and short list [...] compilation.` |
|        - |  2056 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|        - |  2057 | ` * the opening delimiter and before the closing delimiter.` |
|        - |  2058 | ` */` |
|      116 |  2059 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|        5 |  2060 | `{` |
|        - |  2061 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|        - |  2062 | `	SyToken *pNext;` |
|        - |  2063 | `	SyToken *pClassifyIn;` |
|      121 |  2064 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|        - |  2065 | `	sxi32 nExpr;` |
|        - |  2066 | `	sxi32 rc;` |
|        - |  2067 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|        - |  2068 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|        - |  2069 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|        - |  2070 | `	 * list. */` |
|      121 |  2071 | `	pClassifyIn = pGen->pIn;` |
|      341 |  2072 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      225 |  2073 | `		if( pGen->pIn >= pNext ){` |
|       13 |  2074 | `			nEmpty++;` |
|      219 |  2075 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|       38 |  2076 | `			nKeyed++;` |
|       20 |  2077 | `		}else{` |
|      177 |  2078 | `			nPositional++;` |
|        - |  2079 | `		}` |
|      225 |  2080 | `		pGen->pIn = &pNext[1];` |
|        5 |  2081 | `	}` |
|      121 |  2082 | `	pGen->pIn = pClassifyIn;` |
|      121 |  2083 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|      ! 0 |  2084 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  2085 | `			"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 |  2086 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2087 | `	}` |
|      121 |  2088 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|      ! 0 |  2089 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  2090 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|      ! 0 |  2091 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2092 | `	}` |
|      121 |  2093 | `	if( nKeyed > 0 ){` |
|       30 |  2094 | `		return GenStateCompileKeyedListBody(pGen);` |
|        - |  2095 | `	}` |
|       93 |  2096 | `	nExpr = 0;` |
|       93 |  2097 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|      277 |  2098 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      189 |  2099 | `		if( pGen->pIn < pNext ){` |
|        - |  2100 | `			/* Check for nested list() */` |
|      177 |  2101 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        3 |  2102 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - |  2103 | `				/* Record this nested list for post-processing */` |
|        3 |  2104 | `				SyToken *pListEnd = 0;` |
|        3 |  2105 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|        3 |  2106 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        1 |  2107 | `				}` |
|        3 |  2108 | `				if( pListEnd ){` |
|        - |  2109 | `					struct NestedListEntry sEntry;` |
|        3 |  2110 | `					sEntry.nIndex = nExpr;` |
|        3 |  2111 | `					sEntry.pStart = pGen->pIn;` |
|        3 |  2112 | `					sEntry.pEnd = pListEnd + 1;` |
|        3 |  2113 | `					sEntry.isShort = 0;` |
|        3 |  2114 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        1 |  2115 | `				}` |
|        - |  2116 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        3 |  2117 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|      176 |  2118 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - |  2119 | `				/* Nested short destructuring [...] */` |
|       13 |  2120 | `				SyToken *pBracketEnd = 0;` |
|       13 |  2121 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|       13 |  2122 | `				if( pBracketEnd ){` |
|        - |  2123 | `					struct NestedListEntry sEntry;` |
|       13 |  2124 | `					sEntry.nIndex = nExpr;` |
|       13 |  2125 | `					sEntry.pStart = pGen->pIn;` |
|       13 |  2126 | `					sEntry.pEnd = pBracketEnd + 1;` |
|       13 |  2127 | `					sEntry.isShort = 1;` |
|       13 |  2128 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        6 |  2129 | `				}` |
|        - |  2130 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       13 |  2131 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        7 |  2132 | `			}else{` |
|        - |  2133 | `				/* Compile the expression holding the variable */` |
|      163 |  2134 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      163 |  2135 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  2136 | `					SySetRelease(&sNested);` |
|      ! 0 |  2137 | `					return SXRET_OK;` |
|        - |  2138 | `				}` |
|        - |  2139 | `			}` |
|       91 |  2140 | `		}else{` |
|        - |  2141 | `			/* Empty entry,load NULL */` |
|       13 |  2142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|        - |  2143 | `		}` |
|      189 |  2144 | `		nExpr++;` |
|        - |  2145 | `		/* Advance the stream cursor */` |
|      189 |  2146 | `		pGen->pIn = &pNext[1];` |
|        5 |  2147 | `	}` |
|        - |  2148 | `	/* Emit the LOAD_LIST instruction */` |
|       93 |  2149 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|        - |  2150 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|        - |  2151 | `	 * For each nested entry, emit code to extract the sub-array` |
|        - |  2152 | `	 * at the corresponding index and recursively destructure it.` |
|        - |  2153 | `	 */` |
|       93 |  2154 | `	if( SySetUsed(&sNested) > 0 ){` |
|       13 |  2155 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|        - |  2156 | `		sxu32 i;` |
|       27 |  2157 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|       15 |  2158 | `			SyToken *pSavedIn = pGen->pIn;` |
|       15 |  2159 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        - |  2160 | `			ph7_value *pIdx;` |
|        - |  2161 | `			sxu32 nConstIdx;` |
|        - |  2162 | `			/* DUP the source array (it's on stack top) */` |
|       15 |  2163 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - |  2164 | `			/* Push the integer index for this nested entry */` |
|       15 |  2165 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|       15 |  2166 | `			if( pIdx == 0 ){` |
|      ! 0 |  2167 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2168 | `				SySetRelease(&sNested);` |
|      ! 0 |  2169 | `				return SXERR_ABORT;` |
|        - |  2170 | `			}` |
|       15 |  2171 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|       15 |  2172 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|        - |  2173 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|        - |  2174 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|        - |  2175 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|        - |  2176 | `			 */` |
|       15 |  2177 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|        - |  2178 | `			/* Recursively compile the inner list */` |
|       15 |  2179 | `			pGen->pIn = apNested[i].pStart;` |
|       15 |  2180 | `			pGen->pEnd = apNested[i].pEnd;` |
|       15 |  2181 | `			if( apNested[i].isShort ){` |
|       13 |  2182 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 |  2183 | `			}else{` |
|        3 |  2184 | `				rc = PH7_CompileList(&(*pGen),0);` |
|        - |  2185 | `			}` |
|       15 |  2186 | `			pGen->pIn = pSavedIn;` |
|       15 |  2187 | `			pGen->pEnd = pSavedEnd;` |
|       15 |  2188 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2189 | `				SySetRelease(&sNested);` |
|      ! 0 |  2190 | `				return SXERR_ABORT;` |
|        - |  2191 | `			}` |
|        - |  2192 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|       15 |  2193 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        8 |  2194 | `		}` |
|        6 |  2195 | `	}` |
|       93 |  2196 | `	SySetRelease(&sNested);` |
|        - |  2197 | `	/* Node successfully compiled */` |
|       93 |  2198 | `	return SXRET_OK;` |
|       63 |  2199 | `}` |
|       38 |  2200 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2201 | `{` |
|        - |  2202 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|       43 |  2203 | `	pGen->pIn += 2;` |
|       43 |  2204 | `	pGen->pEnd--;` |
|       19 |  2205 | `	SXUNUSED(iCompileFlag);` |
|       43 |  2206 | `	return GenStateCompileListBody(pGen);` |
|        5 |  2207 | `}` |
|       78 |  2208 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 |  2209 | `{` |
|        - |  2210 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|       82 |  2211 | `	pGen->pIn++;` |
|       82 |  2212 | `	pGen->pEnd--;` |
|       39 |  2213 | `	SXUNUSED(iCompileFlag);` |
|       82 |  2214 | `	return GenStateCompileListBody(pGen);` |
|        4 |  2215 | `}` |
|        - |  2216 | `/* Forward declarations */` |
|        - |  2217 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|        - |  2218 | `static int GenStateIsReservedConstant(SyString *pName);` |
|        - |  2219 | `static int GenStateIsReadonly(SyToken *pTok);` |
|        - |  2220 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|        - |  2221 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|        - |  2222 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|        - |  2223 | `/*` |
|        - |  2224 | ` * Compile an annoynmous function or a closure.` |
|        - |  2225 | ` * According to the PHP language reference` |
|        - |  2226 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  2227 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  2228 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|        - |  2229 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|        - |  2230 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|        - |  2231 | ` *  Example Anonymous function variable assignment example` |
|        - |  2232 | ` * <?php` |
|        - |  2233 | ` * $greet = function($name)` |
|        - |  2234 | ` * {` |
|        - |  2235 | ` *    printf("Hello %s\r\n", $name);` |
|        - |  2236 | ` * };` |
|        - |  2237 | ` * $greet('World');` |
|        - |  2238 | ` * $greet('PHP');` |
|        - |  2239 | ` * ?>` |
|        - |  2240 | ` * Note that the implementation of annoynmous function and closure under` |
|        - |  2241 | ` * PH7 is completely different from the one used by the zend engine.` |
|        - |  2242 | ` */` |
|      364 |  2243 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2244 | `{` |
|      369 |  2245 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |  2246 | `	char zName[512];         /* Unique lambda name */` |
|        - |  2247 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |  2248 | `							  * one thread is allowed to compile the script.` |
|        - |  2249 | `						      */` |
|        - |  2250 | `	SyString sName;` |
|        - |  2251 | `	sxu32 nKwLine;` |
|        - |  2252 | `	sxu32 nLen;` |
|        - |  2253 | `	sxi32 rc;` |
|      182 |  2254 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2255 |  |
|      369 |  2256 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|      369 |  2257 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      369 |  2258 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |  2259 | `		pGen->pIn++;` |
|      ! 0 |  2260 | `	}` |
|        - |  2261 | `	/* Generate a unique name */` |
|      369 |  2262 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |  2263 | `	/* Make sure the generated name is unique */` |
|      369 |  2264 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2265 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2266 | `	}` |
|      369 |  2267 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |  2268 | `	/* Compile the lambda body */` |
|      369 |  2269 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|      369 |  2270 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2271 | `		return SXERR_ABORT;` |
|        - |  2272 | `	}` |
|      369 |  2273 | `	if( pAnnonFunc ){` |
|      369 |  2274 | `		pAnnonFunc->nLine = nKwLine;` |
|      182 |  2275 | `	}` |
|        - |  2276 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |  2277 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |  2278 | `	 * the handler wraps either in a Closure instance. */` |
|      369 |  2279 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |  2280 | `	/* Node successfully compiled */` |
|      369 |  2281 | `	return SXRET_OK;` |
|      187 |  2282 | `}` |
|        - |  2283 | `/*` |
|        - |  2284 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |  2285 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |  2286 | ` * enclosing arrow level, or has already been captured.` |
|        - |  2287 | ` */` |
|      196 |  2288 | `static sxi32 GenStateArrowAddCapture(` |
|        - |  2289 | `	ph7_gen_state *pGen,` |
|        - |  2290 | `	ph7_vm_func *pFunc,` |
|        - |  2291 | `	const char *zName,` |
|        - |  2292 | `	sxu32 nByte,` |
|        - |  2293 | `	SyString *aShadow,` |
|        - |  2294 | `	sxu32 nShadow)` |
|        3 |  2295 | `{` |
|        - |  2296 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2297 | `	ph7_vm_func_closure_env *aEnv;` |
|        - |  2298 | `	sxu32 n, nEnv;` |
|        - |  2299 | `	char *zDup;` |
|      199 |  2300 | `	if( nByte == 0 ){` |
|      ! 0 |  2301 | `		return SXRET_OK;` |
|        - |  2302 | `	}` |
|      196 |  2303 | `	if( nByte == sizeof("this")-1` |
|      107 |  2304 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        3 |  2305 | `		return SXRET_OK;` |
|        - |  2306 | `	}` |
|      247 |  2307 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      182 |  2308 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      176 |  2309 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      135 |  2310 | `			return SXRET_OK;` |
|        - |  2311 | `		}` |
|       27 |  2312 | `	}` |
|       63 |  2313 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       63 |  2314 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|       91 |  2315 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|       30 |  2316 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|       29 |  2317 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|        3 |  2318 | `			return SXRET_OK;` |
|        - |  2319 | `		}` |
|       15 |  2320 | `	}` |
|       61 |  2321 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|       61 |  2322 | `	if( zDup == 0 ){` |
|      ! 0 |  2323 | `		return SXERR_ABORT;` |
|        - |  2324 | `	}` |
|       61 |  2325 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       61 |  2326 | `	sEnv.iFlags = 0;` |
|       61 |  2327 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       61 |  2328 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|       61 |  2329 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       61 |  2330 | `	return SXRET_OK;` |
|      101 |  2331 | `}` |
|        - |  2332 | `/*` |
|        - |  2333 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|        - |  2334 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|        - |  2335 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|        - |  2336 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|        - |  2337 | ` */` |
|       56 |  2338 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|        - |  2339 | `	ph7_gen_state *pGen,` |
|        - |  2340 | `	ph7_vm_func *pFunc,` |
|        - |  2341 | `	const char *zIn,` |
|        - |  2342 | `	const char *zEnd,` |
|        - |  2343 | `	SyString *aShadow,` |
|        - |  2344 | `	sxu32 nShadow)` |
|        2 |  2345 | `{` |
|        - |  2346 | `	sxi32 rc;` |
|      370 |  2347 | `	while( zIn < zEnd ){` |
|      314 |  2348 | `		if( zIn[0] == '\\' ){` |
|        5 |  2349 | `			zIn++;` |
|        5 |  2350 | `			if( zIn < zEnd ){` |
|        5 |  2351 | `				zIn++;` |
|        2 |  2352 | `			}` |
|        5 |  2353 | `			continue;` |
|        - |  2354 | `		}` |
|      308 |  2355 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|       26 |  2356 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|       24 |  2357 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|        - |  2358 | `			const char *zName;` |
|       26 |  2359 | `			zIn++; /* skip '$' */` |
|       26 |  2360 | `			zName = zIn;` |
|       82 |  2361 | `			while( zIn < zEnd ){` |
|       76 |  2362 | `				unsigned char c = (unsigned char)zIn[0];` |
|       76 |  2363 | `				if( c >= 0xc0 ){` |
|      ! 0 |  2364 | `					zIn++;` |
|      ! 0 |  2365 | `					while( zIn < zEnd` |
|      ! 0 |  2366 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  2367 | `						zIn++;` |
|      ! 0 |  2368 | `					}` |
|      ! 0 |  2369 | `					continue;` |
|        - |  2370 | `				}` |
|       76 |  2371 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       20 |  2372 | `					break;` |
|        - |  2373 | `				}` |
|       58 |  2374 | `				zIn++;` |
|        2 |  2375 | `			}` |
|       26 |  2376 | `			if( zIn > zName ){` |
|       38 |  2377 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|       24 |  2378 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|       26 |  2379 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2380 | `					return SXERR_ABORT;` |
|        - |  2381 | `				}` |
|       12 |  2382 | `			}` |
|       26 |  2383 | `			continue;` |
|        - |  2384 | `		}` |
|      286 |  2385 | `		zIn++;` |
|        2 |  2386 | `	}` |
|       58 |  2387 | `	return SXRET_OK;` |
|       30 |  2388 | `}` |
|        - |  2389 | `/*` |
|        - |  2390 | ` * Scan the body token range of an arrow function for free-variable` |
|        - |  2391 | ` * references and record them in pFunc's closure environment. Handles:` |
|        - |  2392 | ` *   - plain $<id> pairs` |
|        - |  2393 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|        - |  2394 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|        - |  2395 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|        - |  2396 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|        - |  2397 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|        - |  2398 | ` *     are never mistakenly captured.` |
|        - |  2399 | ` */` |
|      290 |  2400 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  2401 | `	ph7_gen_state *pGen,` |
|        - |  2402 | `	ph7_vm_func *pFunc,` |
|        - |  2403 | `	SyToken *pStart,` |
|        - |  2404 | `	SyToken *pEnd,` |
|        - |  2405 | `	SyString *aShadow,` |
|        - |  2406 | `	sxu32 nShadow)` |
|        4 |  2407 | `{` |
|      294 |  2408 | `	SyToken *pScan = pStart;` |
|        - |  2409 | `	sxi32 rc;` |
|     1696 |  2410 | `	while( pScan < pEnd ){` |
|     1406 |  2411 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       86 |  2412 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       28 |  2413 | `				pScan->sData.zString,` |
|       56 |  2414 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       28 |  2415 | `				aShadow,nShadow);` |
|       58 |  2416 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2417 | `				return SXERR_ABORT;` |
|        - |  2418 | `			}` |
|       58 |  2419 | `			pScan++;` |
|       58 |  2420 | `			continue;` |
|        - |  2421 | `		}` |
|     1350 |  2422 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|       30 |  2423 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|       30 |  2424 | `			SyToken *pFnKw = pScan;` |
|       28 |  2425 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|      ! 0 |  2426 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|        2 |  2427 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  2428 | `				pFnKw = &pScan[1];` |
|      ! 0 |  2429 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  2430 | `			}` |
|       30 |  2431 | `			if( nKw == PH7_TKWRD_FN ){` |
|        - |  2432 | `				SyToken *pInnerSigStart;` |
|        - |  2433 | `				SyToken *pInnerSigEnd;` |
|        - |  2434 | `				SyToken *pInnerBodyEnd;` |
|        - |  2435 | `				SyString *aInnerShadow;` |
|        - |  2436 | `				sxu32 nInnerShadow;` |
|        - |  2437 | `				sxu32 nInnerParamMax;` |
|        - |  2438 | `				SyToken *p;` |
|        - |  2439 | `				int iNestInner;` |
|       19 |  2440 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|       19 |  2441 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2442 | `					pScan++;` |
|      ! 0 |  2443 | `				}` |
|       19 |  2444 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2445 | `					pScan++;` |
|      ! 0 |  2446 | `					continue;` |
|        - |  2447 | `				}` |
|       19 |  2448 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|       19 |  2449 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|        - |  2450 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|       19 |  2451 | `				if( pInnerSigEnd >= pEnd ){` |
|      ! 0 |  2452 | `					pScan = pEnd;` |
|      ! 0 |  2453 | `					continue;` |
|        - |  2454 | `				}` |
|        - |  2455 | `				/* Build an augmented shadow list: inherited + inner params */` |
|       19 |  2456 | `				nInnerParamMax = 0;` |
|       57 |  2457 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2458 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|       13 |  2459 | `						nInnerParamMax++;` |
|        6 |  2460 | `					}` |
|       20 |  2461 | `				}` |
|       19 |  2462 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       18 |  2463 | `					&pGen->pVm->sAllocator,` |
|       18 |  2464 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|       19 |  2465 | `				if( aInnerShadow == 0 ){` |
|      ! 0 |  2466 | `					return SXERR_ABORT;` |
|        - |  2467 | `				}` |
|       19 |  2468 | `				nInnerShadow = 0;` |
|       25 |  2469 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|        7 |  2470 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|        4 |  2471 | `				}` |
|       57 |  2472 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2473 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       27 |  2474 | `						continue;` |
|        - |  2475 | `					}` |
|       13 |  2476 | `					if( &p[1] >= pInnerSigEnd ){` |
|      ! 0 |  2477 | `						break;` |
|        - |  2478 | `					}` |
|       13 |  2479 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2480 | `						continue;` |
|        - |  2481 | `					}` |
|       13 |  2482 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|        7 |  2483 | `				}` |
|       19 |  2484 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|       19 |  2485 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|      ! 0 |  2486 | `					pScan++;` |
|      ! 0 |  2487 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|      ! 0 |  2488 | `						&& pScan->sData.nByte == 1` |
|      ! 0 |  2489 | `						&& pScan->sData.zString[0] == '?' ){` |
|      ! 0 |  2490 | `						pScan++;` |
|      ! 0 |  2491 | `					}` |
|      ! 0 |  2492 | `					if( pScan < pEnd` |
|      ! 0 |  2493 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  2494 | `						pScan++;` |
|      ! 0 |  2495 | `					}` |
|      ! 0 |  2496 | `				}` |
|       19 |  2497 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|       19 |  2498 | `					pScan++; /* past '=>' */` |
|        9 |  2499 | `				}` |
|       19 |  2500 | `				pInnerBodyEnd = pScan;` |
|       19 |  2501 | `				iNestInner = 0;` |
|      131 |  2502 | `				while( pInnerBodyEnd < pEnd ){` |
|      113 |  2503 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|        - |  2504 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|        - |  2505 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      ! 0 |  2506 | `						break;` |
|        - |  2507 | `					}` |
|      113 |  2508 | `					if( pInnerBodyEnd->nType &` |
|        - |  2509 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        3 |  2510 | `						iNestInner++;` |
|      112 |  2511 | `					}else if( pInnerBodyEnd->nType &` |
|        - |  2512 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        3 |  2513 | `						iNestInner--;` |
|        1 |  2514 | `					}` |
|      113 |  2515 | `					pInnerBodyEnd++;` |
|        1 |  2516 | `				}` |
|        - |  2517 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|        - |  2518 | `				 * the outer's body: a default value is evaluated at call time` |
|        - |  2519 | `				 * in the outer frame, so any free variable it references is` |
|        - |  2520 | `				 * an outer capture. We must NOT scan the parameter-name` |
|        - |  2521 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|        - |  2522 | `				 * or those names leak into the outer's closure environment.` |
|        - |  2523 | `				 *` |
|        - |  2524 | `				 * Walk the signature argument-by-argument, splitting on` |
|        - |  2525 | `				 * top-level commas, and for each argument scan only the token` |
|        - |  2526 | `				 * range after the '=' sign. */` |
|        - |  2527 | `				{` |
|       19 |  2528 | `					SyToken *pArgStart = pInnerSigStart;` |
|       31 |  2529 | `					while( pArgStart < pInnerSigEnd ){` |
|       13 |  2530 | `						SyToken *pArgEnd = pArgStart;` |
|       13 |  2531 | `						SyToken *pEq = 0;` |
|       13 |  2532 | `						int iNestArg = 0;` |
|       49 |  2533 | `						while( pArgEnd < pInnerSigEnd ){` |
|       38 |  2534 | `							if( iNestArg == 0` |
|       39 |  2535 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|        3 |  2536 | `								break;` |
|        - |  2537 | `							}` |
|       37 |  2538 | `							if( pArgEnd->nType &` |
|        - |  2539 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  2540 | `								iNestArg++;` |
|       37 |  2541 | `							}else if( pArgEnd->nType &` |
|        - |  2542 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  2543 | `								iNestArg--;` |
|      ! 0 |  2544 | `							}` |
|       36 |  2545 | `							if( pEq == 0 && iNestArg == 0` |
|       31 |  2546 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|        7 |  2547 | `								pEq = pArgEnd;` |
|        3 |  2548 | `							}` |
|       37 |  2549 | `							pArgEnd++;` |
|        1 |  2550 | `						}` |
|       13 |  2551 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|       10 |  2552 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        3 |  2553 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|        7 |  2554 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  2555 | `								return SXERR_ABORT;` |
|        - |  2556 | `							}` |
|        3 |  2557 | `						}` |
|       13 |  2558 | `						pArgStart = pArgEnd;` |
|       12 |  2559 | `						if( pArgStart < pInnerSigEnd` |
|        8 |  2560 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|        3 |  2561 | `							pArgStart++;` |
|        1 |  2562 | `						}` |
|        1 |  2563 | `					}` |
|        - |  2564 | `				}` |
|       28 |  2565 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        9 |  2566 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|       19 |  2567 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2568 | `					return SXERR_ABORT;` |
|        - |  2569 | `				}` |
|       19 |  2570 | `				pScan = pInnerBodyEnd;` |
|       19 |  2571 | `				continue;` |
|        - |  2572 | `			}` |
|        5 |  2573 | `		}` |
|     1332 |  2574 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     1160 |  2575 | `			pScan++;` |
|     1160 |  2576 | `			continue;` |
|        - |  2577 | `		}` |
|        - |  2578 | `		{` |
|        - |  2579 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      175 |  2580 | `			SyToken *pDollar = pScan;` |
|      258 |  2581 | `			while( &pDollar[1] < pEnd` |
|      175 |  2582 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  2583 | `				pDollar++;` |
|      ! 0 |  2584 | `			}` |
|      175 |  2585 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  2586 | `				break;` |
|        - |  2587 | `			}` |
|      175 |  2588 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2589 | `				pScan = pDollar + 1;` |
|      ! 0 |  2590 | `				continue;` |
|        - |  2591 | `			}` |
|      261 |  2592 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      172 |  2593 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|       86 |  2594 | `				aShadow,nShadow);` |
|      175 |  2595 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2596 | `				return SXERR_ABORT;` |
|        - |  2597 | `			}` |
|      175 |  2598 | `			pScan = pDollar + 2;` |
|        - |  2599 | `		}` |
|        3 |  2600 | `	}` |
|      294 |  2601 | `	return SXRET_OK;` |
|      149 |  2602 | `}` |
|        - |  2603 | `/*` |
|        - |  2604 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  2605 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  2606 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  2607 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  2608 | ` * $this is also made available.` |
|        - |  2609 | ` */` |
|      272 |  2610 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2611 | `{` |
|        - |  2612 | `	ph7_vm_func *pFunc;` |
|        - |  2613 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2614 | `	GenBlock *pBlock;` |
|        - |  2615 | `	SySet *pInstrContainer;` |
|        - |  2616 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|        - |  2617 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|        - |  2618 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|        - |  2619 | `	SyToken *pSavedEnd;` |
|        - |  2620 | `	ph7_vm_func_arg *aArgs;` |
|        - |  2621 | `	char zName[512];` |
|        - |  2622 | `	static int iCnt = 1;` |
|        - |  2623 | `	char *zDup;` |
|        - |  2624 | `	sxu32 nLen;` |
|        - |  2625 | `	sxu32 nLine;` |
|      277 |  2626 | `	sxi32 iFlags = 0;` |
|      277 |  2627 | `	int bStatic = 0;` |
|        - |  2628 | `	sxi32 rc;` |
|        - |  2629 | `	sxu32 n;` |
|      136 |  2630 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2631 |  |
|      277 |  2632 | `	nLine = pGen->pIn->nLine;` |
|        - |  2633 | `	/* Optional 'static' prefix */` |
|      272 |  2634 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      277 |  2635 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  2636 | `		bStatic = 1;` |
|        3 |  2637 | `		pGen->pIn++;` |
|        1 |  2638 | `	}` |
|        - |  2639 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      272 |  2640 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      277 |  2641 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  2642 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2643 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  2644 | `		return SXERR_SYNTAX;` |
|        - |  2645 | `	}` |
|      277 |  2646 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  2647 | `	/* Optional '&' — return by reference */` |
|      277 |  2648 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2649 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  2650 | `		pGen->pIn++;` |
|      ! 0 |  2651 | `	}` |
|        - |  2652 | `	/* Expect '(' */` |
|      277 |  2653 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  2654 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2655 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2656 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|        2 |  2657 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2658 | `		}else{` |
|      ! 0 |  2659 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2660 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|        - |  2661 | `		}` |
|        3 |  2662 | `		return SXERR_SYNTAX;` |
|        - |  2663 | `	}` |
|      275 |  2664 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  2665 | `	/* Delimit the parameter list */` |
|      275 |  2666 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      275 |  2667 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  2668 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2669 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  2670 | `		return SXERR_SYNTAX;` |
|        - |  2671 | `	}` |
|        - |  2672 | `	/* Allocate the function state */` |
|      273 |  2673 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      273 |  2674 | `	if( pFunc == 0 ){` |
|      ! 0 |  2675 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2676 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2677 | `		return SXERR_ABORT;` |
|        - |  2678 | `	}` |
|        - |  2679 | `	/* Generate a unique lambda name */` |
|      273 |  2680 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      311 |  2681 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       40 |  2682 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        2 |  2683 | `	}` |
|      273 |  2684 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      273 |  2685 | `	if( zDup == 0 ){` |
|      ! 0 |  2686 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2687 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2688 | `		return SXERR_ABORT;` |
|        - |  2689 | `	}` |
|      273 |  2690 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  2691 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|      273 |  2692 | `	pFunc->nLine = nLine;` |
|        - |  2693 | `	/* Collect function arguments */` |
|      273 |  2694 | `	if( pGen->pIn < pSigEnd ){` |
|      110 |  2695 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      110 |  2696 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2697 | `			return SXERR_ABORT;` |
|        - |  2698 | `		}` |
|       53 |  2699 | `	}` |
|        - |  2700 | `	/* Point past ')' and parse optional return type */` |
|      273 |  2701 | `	pGen->pIn = &pSigEnd[1];` |
|      273 |  2702 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      273 |  2703 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2704 | `		return SXERR_ABORT;` |
|      273 |  2705 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  2706 | `		return SXERR_SYNTAX;` |
|        - |  2707 | `	}` |
|        - |  2708 | `	/* Expect '=>' */` |
|      273 |  2709 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  2710 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2711 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2712 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|        2 |  2713 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2714 | `		}else{` |
|      ! 0 |  2715 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2716 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|        - |  2717 | `		}` |
|        3 |  2718 | `		return SXERR_SYNTAX;` |
|        - |  2719 | `	}` |
|      270 |  2720 | `	pGen->pIn++; /* Jump '=>' */` |
|      270 |  2721 | `	pBodyStart = pGen->pIn;` |
|      270 |  2722 | `	pBodyEnd = pGen->pEnd;` |
|        - |  2723 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  2724 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  2725 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  2726 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      270 |  2727 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  2728 | `	{` |
|      270 |  2729 | `		SyString *aShadow = 0;` |
|      270 |  2730 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      270 |  2731 | `		if( nShadow > 0 ){` |
|      107 |  2732 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      104 |  2733 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      107 |  2734 | `			if( aShadow == 0 ){` |
|      ! 0 |  2735 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2736 | `					"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2737 | `				return SXERR_ABORT;` |
|        - |  2738 | `			}` |
|      239 |  2739 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      135 |  2740 | `				aShadow[n] = aArgs[n].sName;` |
|       69 |  2741 | `			}` |
|       52 |  2742 | `		}` |
|      403 |  2743 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      133 |  2744 | `			aShadow,nShadow);` |
|      270 |  2745 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2746 | `			return SXERR_ABORT;` |
|        - |  2747 | `		}` |
|        - |  2748 | `	}` |
|        - |  2749 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  2750 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  2751 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  2752 | `	 * $this. */` |
|      270 |  2753 | `	if( !bStatic ){` |
|        - |  2754 | `		char *zThisDup;` |
|      268 |  2755 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      268 |  2756 | `		if( zThisDup == 0 ){` |
|      ! 0 |  2757 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2758 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2759 | `			return SXERR_ABORT;` |
|        - |  2760 | `		}` |
|      268 |  2761 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      268 |  2762 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      268 |  2763 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      268 |  2764 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      268 |  2765 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      132 |  2766 | `	}` |
|        - |  2767 | `	/* Arrow functions are always closures */` |
|      270 |  2768 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|        - |  2769 | `	/* Compile the body expression as an implicit return */` |
|      403 |  2770 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      133 |  2771 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      270 |  2772 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2773 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2774 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  2775 | `		return SXERR_ABORT;` |
|        - |  2776 | `	}` |
|      270 |  2777 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      270 |  2778 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      270 |  2779 | `	pSavedEnd = pGen->pEnd;` |
|      270 |  2780 | `	pGen->pIn = pBodyStart;` |
|      270 |  2781 | `	pGen->pEnd = pBodyEnd;` |
|      270 |  2782 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      270 |  2783 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2784 | `		return SXERR_ABORT;` |
|        - |  2785 | `	}` |
|        - |  2786 | `	/* The cursor stopped just past the body expression */` |
|      270 |  2787 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  2788 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  2789 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  2790 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  2791 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      270 |  2792 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      270 |  2793 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      270 |  2794 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      270 |  2795 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      270 |  2796 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  2797 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      270 |  2798 | `	pGen->pIn = pBodyEnd;` |
|      270 |  2799 | `	pGen->pEnd = pSavedEnd;` |
|        - |  2800 | `	/* Emit the load-closure instruction */` |
|      270 |  2801 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      270 |  2802 | `	return SXRET_OK;` |
|      141 |  2803 | `}` |
|        - |  2804 | `/*` |
|        - |  2805 | ` * Compile a single arm's expression range into a freshly-allocated` |
|        - |  2806 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|        - |  2807 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|        - |  2808 | ` * expression's value.` |
|        - |  2809 | ` */` |
|      354 |  2810 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|        - |  2811 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|        3 |  2812 | `{` |
|        - |  2813 | `	SySet *pInstrContainer;` |
|        - |  2814 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  2815 | `	GenBlock *pArmBlock;` |
|        - |  2816 | `	sxi32 rc;` |
|      357 |  2817 | `	pTmpIn  = pGen->pIn;` |
|      357 |  2818 | `	pTmpEnd = pGen->pEnd;` |
|      357 |  2819 | `	pGen->pIn  = pStart;` |
|      357 |  2820 | `	pGen->pEnd = pStop;` |
|      357 |  2821 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      357 |  2822 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|        - |  2823 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|        - |  2824 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|        - |  2825 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|        - |  2826 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|        - |  2827 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|      534 |  2828 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      177 |  2829 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|      357 |  2830 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2831 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  2832 | `		pGen->pIn  = pTmpIn;` |
|      ! 0 |  2833 | `		pGen->pEnd = pTmpEnd;` |
|      ! 0 |  2834 | `		return SXERR_ABORT;` |
|        - |  2835 | `	}` |
|      357 |  2836 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      357 |  2837 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      357 |  2838 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      357 |  2839 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      357 |  2840 | `	GenStateLeaveBlock(&(*pGen),0);` |
|      357 |  2841 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      357 |  2842 | `	pGen->pIn  = pTmpIn;` |
|      357 |  2843 | `	pGen->pEnd = pTmpEnd;` |
|      357 |  2844 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2845 | `		return SXERR_ABORT;` |
|        - |  2846 | `	}` |
|      357 |  2847 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 |  2848 | `		return SXERR_EMPTY;` |
|        - |  2849 | `	}` |
|      357 |  2850 | `	return SXRET_OK;` |
|      180 |  2851 | `}` |
|        - |  2852 | `/*` |
|        - |  2853 | ` * Compile a PHP 8.0 match expression:` |
|        - |  2854 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|        - |  2855 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|        - |  2856 | ` * Strict comparison (===) is used between the subject and each condition.` |
|        - |  2857 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|        - |  2858 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|        - |  2859 | ` */` |
|        - |  2860 | `/*` |
|        - |  2861 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|        - |  2862 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|        - |  2863 | ` * caller can bail out of the current expression.` |
|        - |  2864 | ` */` |
|        2 |  2865 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|        1 |  2866 | `{` |
|        - |  2867 | `	va_list ap;` |
|        - |  2868 | `	sxi32 rc;` |
|        - |  2869 | `	SyBlob sMsg;` |
|        3 |  2870 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        3 |  2871 | `	va_start(ap,zFmt);` |
|        3 |  2872 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|        3 |  2873 | `	va_end(ap);` |
|        3 |  2874 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|        3 |  2875 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|        3 |  2876 | `	SyBlobRelease(&sMsg);` |
|        3 |  2877 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2878 | `		return SXERR_ABORT;` |
|        - |  2879 | `	}` |
|        3 |  2880 | `	return SXERR_SYNTAX;` |
|        2 |  2881 | `}` |
|        - |  2882 | `/*` |
|        - |  2883 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|        - |  2884 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|        - |  2885 | ` * Returns the stop token pointer (or pEnd if none found).` |
|        - |  2886 | ` */` |
|      356 |  2887 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|        4 |  2888 | `{` |
|      360 |  2889 | `	SyToken *pCur = pStart;` |
|      360 |  2890 | `	int iNest = 0;` |
|      838 |  2891 | `	while( pCur < pEnd ){` |
|      802 |  2892 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       13 |  2893 | `			iNest++;` |
|      796 |  2894 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       13 |  2895 | `			iNest--;` |
|      784 |  2896 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|      323 |  2897 | `			return pCur;` |
|        - |  2898 | `		}` |
|      482 |  2899 | `		pCur++;` |
|        4 |  2900 | `	}` |
|       39 |  2901 | `	return pEnd;` |
|      182 |  2902 | `}` |
|       72 |  2903 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2904 | `{` |
|        - |  2905 | `	ph7_match *pMatch;` |
|        - |  2906 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|       77 |  2907 | `	int bHasDefault = 0;` |
|        - |  2908 | `	sxu32 nLine;` |
|        - |  2909 | `	sxi32 rc;` |
|       36 |  2910 | `	SXUNUSED(iCompileFlag);` |
|       77 |  2911 | `	nLine = pGen->pIn->nLine;` |
|       77 |  2912 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|        - |  2913 | `	/* Expect '(' */` |
|       77 |  2914 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2915 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2916 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|      ! 0 |  2917 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|        - |  2918 | `	}` |
|       77 |  2919 | `	pGen->pIn++; /* Jump '(' */` |
|       77 |  2920 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|       77 |  2921 | `	if( pSubjEnd >= pGen->pEnd ){` |
|      ! 0 |  2922 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2923 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        - |  2924 | `	}` |
|       77 |  2925 | `	if( pGen->pIn >= pSubjEnd ){` |
|      ! 0 |  2926 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2927 | `			"syntax error, unexpected \")\", expecting match subject");` |
|        - |  2928 | `	}` |
|        - |  2929 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|       77 |  2930 | `	pSavedEnd = pGen->pEnd;` |
|       77 |  2931 | `	pGen->pEnd = pSubjEnd;` |
|       77 |  2932 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       77 |  2933 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2934 | `		return SXERR_ABORT;` |
|        - |  2935 | `	}` |
|       77 |  2936 | `	pGen->pEnd = pSavedEnd;` |
|       77 |  2937 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|        - |  2938 | `	/* Expect '{' */` |
|       77 |  2939 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 |  2940 | `		return GenStateMatchError(pGen,` |
|      ! 0 |  2941 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  2942 | `			"syntax error, expecting \"{\" after match subject");` |
|        - |  2943 | `	}` |
|       77 |  2944 | `	pGen->pIn++; /* Jump '{' */` |
|       77 |  2945 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|       77 |  2946 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  2947 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2948 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|        - |  2949 | `	}` |
|        - |  2950 | `	/* Allocate ph7_match container */` |
|       77 |  2951 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|       77 |  2952 | `	if( pMatch == 0 ){` |
|      ! 0 |  2953 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2954 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2955 | `		return SXERR_ABORT;` |
|        - |  2956 | `	}` |
|       77 |  2957 | `	SyZero(pMatch,sizeof(ph7_match));` |
|       77 |  2958 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|        - |  2959 | `	/* Iterate arms */` |
|      259 |  2960 | `	while( pGen->pIn < pBodyEnd ){` |
|        - |  2961 | `		ph7_match_arm sArm;` |
|        - |  2962 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|      190 |  2963 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|      190 |  2964 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|      190 |  2965 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|      190 |  2966 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  2967 | `		/* 'default' arm? */` |
|      186 |  2968 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      107 |  2969 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|       22 |  2970 | `			if( bHasDefault ){` |
|        3 |  2971 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|        - |  2972 | `					"Match expressions may only contain one default arm");` |
|        4 |  2973 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  2974 | `			}` |
|       20 |  2975 | `			sArm.bDefault = 1;` |
|       20 |  2976 | `			bHasDefault = 1;` |
|       20 |  2977 | `			pGen->pIn++;` |
|       20 |  2978 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|      ! 0 |  2979 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  2980 | `					"syntax error, expecting \"=>\" after 'default'");` |
|        - |  2981 | `			}` |
|       20 |  2982 | `			pGen->pIn++; /* Jump '=>' */` |
|       11 |  2983 | `		}else{` |
|        - |  2984 | `			/* Condition list: cond (',' cond)* '=>' */` |
|      170 |  2985 | `			pCondStart = pGen->pIn;` |
|      170 |  2986 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|        - |  2987 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|      178 |  2988 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|        - |  2989 | `				SySet sCondBc;` |
|        9 |  2990 | `				if( pCondStart >= pArrow ){` |
|      ! 0 |  2991 | `					return GenStateMatchError(pGen,nArmLine,` |
|        - |  2992 | `						"syntax error, empty match condition expression");` |
|        - |  2993 | `				}` |
|        9 |  2994 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        9 |  2995 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|        9 |  2996 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2997 | `					return SXERR_ABORT;` |
|        - |  2998 | `				}` |
|        9 |  2999 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        9 |  3000 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|        9 |  3001 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|        - |  3002 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|        1 |  3003 | `			}` |
|      170 |  3004 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  3005 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3006 | `					"syntax error, expecting \"=>\" in match arm");` |
|        - |  3007 | `			}` |
|      167 |  3008 | `			if( pCondStart >= pArrow ){` |
|      ! 0 |  3009 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3010 | `					"syntax error, empty match condition expression");` |
|        - |  3011 | `			}` |
|        - |  3012 | `			{` |
|        - |  3013 | `				SySet sCondBc;` |
|      167 |  3014 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      167 |  3015 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      167 |  3016 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3017 | `					return SXERR_ABORT;` |
|        - |  3018 | `				}` |
|      167 |  3019 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        - |  3020 | `			}` |
|      167 |  3021 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|        - |  3022 | `		}` |
|        - |  3023 | `		/* Compile result expression: up to top-level ',' or body end */` |
|      185 |  3024 | `		pResStart = pGen->pIn;` |
|      185 |  3025 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|      185 |  3026 | `		if( pResStart >= pResEnd ){` |
|      ! 0 |  3027 | `			return GenStateMatchError(pGen,nArmLine,` |
|        - |  3028 | `				"syntax error, expected expression after \"=>\"");` |
|        - |  3029 | `		}` |
|      185 |  3030 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|      185 |  3031 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3032 | `			return SXERR_ABORT;` |
|        - |  3033 | `		}` |
|      185 |  3034 | `		pGen->pIn = pResEnd;` |
|      185 |  3035 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      151 |  3036 | `			pGen->pIn++; /* Skip trailing ',' */` |
|       74 |  3037 | `		}` |
|      185 |  3038 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|        3 |  3039 | `	}` |
|       71 |  3040 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|       71 |  3041 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|       71 |  3042 | `	return SXRET_OK;` |
|       41 |  3043 | `}` |
|        - |  3044 | `/*` |
|        - |  3045 | ` * Compile a backtick quoted string.` |
|        - |  3046 | ` */` |
|        4 |  3047 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 |  3048 | `{` |
|        - |  3049 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|        - |  3050 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|        - |  3051 | `	 */` |
|        8 |  3052 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|        - |  3053 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|        2 |  3054 | `		ph7_lib_version()` |
|        - |  3055 | `		);` |
|        - |  3056 | `	/* Load NULL */` |
|        6 |  3057 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3058 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  3059 | `	/* Node successfully compiled */` |
|        6 |  3060 | `	return SXRET_OK;` |
|        2 |  3061 | `}` |
|        - |  3062 | `/*` |
|        - |  3063 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|        - |  3064 | ` * construct.` |
|        - |  3065 | ` */` |
|       82 |  3066 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3067 | `{` |
|        - |  3068 | `	SyString *pName;` |
|        - |  3069 | `	sxu32 nKeyID;` |
|        - |  3070 | `	sxi32 rc;` |
|        - |  3071 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|       87 |  3072 | `	pName = &pGen->pIn->sData;` |
|       87 |  3073 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       87 |  3074 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|       87 |  3075 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|        9 |  3076 | `		SyToken *pTmp,*pNext = 0;` |
|        - |  3077 | `		/* Compile arguments one after one */` |
|        9 |  3078 | `		pTmp = pGen->pEnd;` |
|        - |  3079 | `		/* Symisc eXtension to the PHP programming language:` |
|        - |  3080 | `		 * 'echo' can be used in the context of a function which` |
|        - |  3081 | `		 *  mean that the following expression is valid:` |
|        - |  3082 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|        - |  3083 | `		 */` |
|        9 |  3084 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|       17 |  3085 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        9 |  3086 | `			if( pGen->pIn < pNext ){` |
|        9 |  3087 | `				pGen->pEnd = pNext;` |
|        9 |  3088 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|        9 |  3089 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3090 | `					return SXERR_ABORT;` |
|        - |  3091 | `				}` |
|        9 |  3092 | `				if( rc != SXERR_EMPTY ){` |
|        - |  3093 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|        - |  3094 | `					 * without the overhead of a function call.` |
|        - |  3095 | `					 * This is a very powerful optimization that improve` |
|        - |  3096 | `					 * performance greatly.` |
|        - |  3097 | `					 */` |
|        9 |  3098 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|        4 |  3099 | `				}` |
|        4 |  3100 | `			}` |
|        - |  3101 | `			/* Jump trailing commas */` |
|        9 |  3102 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      ! 0 |  3103 | `				pNext++;` |
|      ! 0 |  3104 | `			}` |
|        9 |  3105 | `			pGen->pIn = pNext;` |
|        1 |  3106 | `		}` |
|        - |  3107 | `		/* Restore token stream */` |
|        9 |  3108 | `		pGen->pEnd = pTmp;` |
|        5 |  3109 | `	}else{` |
|       79 |  3110 | `		sxi32 nArg = 0;` |
|       79 |  3111 | `		sxu32 nIdx = 0;` |
|       79 |  3112 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       79 |  3113 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3114 | `			return SXERR_ABORT;` |
|       79 |  3115 | `		}else if(rc != SXERR_EMPTY ){` |
|       79 |  3116 | `			nArg = 1;` |
|       37 |  3117 | `		}` |
|       79 |  3118 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|        - |  3119 | `			ph7_value *pObj;` |
|        - |  3120 | `			/* Emit the call instruction */` |
|       31 |  3121 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       31 |  3122 | `			if( pObj == 0 ){` |
|      ! 0 |  3123 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3124 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3125 | `				return SXERR_ABORT;` |
|        - |  3126 | `			}` |
|       31 |  3127 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|        - |  3128 | `			/* Install in the literal table */` |
|       31 |  3129 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       13 |  3130 | `		}` |
|        - |  3131 | `		/* Emit the call instruction */` |
|       79 |  3132 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       79 |  3133 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        - |  3134 | `	}` |
|        - |  3135 | `	/* Node successfully compiled */` |
|       87 |  3136 | `	return SXRET_OK;` |
|       46 |  3137 | `}` |
|        - |  3138 | `/*` |
|        - |  3139 | ` * Compile a node holding a variable declaration.` |
|        - |  3140 | ` * According to the PHP language reference` |
|        - |  3141 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|        - |  3142 | ` *  The variable name is case-sensitive.` |
|        - |  3143 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|        - |  3144 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3145 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|        - |  3146 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|        - |  3147 | ` *  Note: $this is a special variable that can't be assigned.` |
|        - |  3148 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|        - |  3149 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|        - |  3150 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|        - |  3151 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|        - |  3152 | ` *  the chapter on Expressions.` |
|        - |  3153 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|        - |  3154 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|        - |  3155 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|        - |  3156 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|        - |  3157 | ` *  is being assigned (the source variable).` |
|        - |  3158 | ` */` |
|  8775586 |  3159 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3160 | `{` |
|  8775591 |  3161 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3162 | `	sxi32 iVv;` |
|        - |  3163 | `	sxi32 iP1;` |
|        - |  3164 | `	void *p3;` |
|        - |  3165 | `	sxi32 rc;` |
|  8775591 |  3166 | `	iVv = -1; /* Variable variable counter */` |
| 17551189 |  3167 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  8775603 |  3168 | `		pGen->pIn++;` |
|  8775603 |  3169 | `		iVv++;` |
|        5 |  3170 | `	}` |
|  8775591 |  3171 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - |  3172 | `		/* Invalid variable name */` |
|      ! 0 |  3173 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|      ! 0 |  3174 | `		if( rc == SXERR_ABORT ){` |
|        - |  3175 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3176 | `			return SXERR_ABORT;` |
|        - |  3177 | `		}` |
|      ! 0 |  3178 | `		return SXRET_OK;` |
|        - |  3179 | `	}` |
|  8775591 |  3180 | `	p3  = 0;` |
|  8775591 |  3181 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|        - |  3182 | `		/* Dynamic variable creation */` |
|       21 |  3183 | `		pGen->pIn++;  /* Jump the open curly */` |
|       21 |  3184 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|       21 |  3185 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3186 | `			/* Empty expression */` |
|        3 |  3187 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|        3 |  3188 | `			return SXRET_OK;` |
|        - |  3189 | `		}` |
|        - |  3190 | `		/* Compile the expression holding the variable name */` |
|       18 |  3191 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       18 |  3192 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3193 | `			return SXERR_ABORT;` |
|       18 |  3194 | `		}else if( rc == SXERR_EMPTY ){` |
|        3 |  3195 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|        3 |  3196 | `			return SXRET_OK;` |
|        - |  3197 | `		}` |
|        8 |  3198 | `	}else{` |
|        - |  3199 | `		SyHashEntry *pEntry;` |
|        - |  3200 | `		SyString *pName;` |
|  8775573 |  3201 | `		char *zName = 0;` |
|        - |  3202 | `		/* Extract variable name */` |
|  8775573 |  3203 | `		pName = &pGen->pIn->sData;` |
|        - |  3204 | `		/* Advance the stream cursor */` |
|  8775573 |  3205 | `		pGen->pIn++;` |
|  8775573 |  3206 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  8775573 |  3207 | `		if( pEntry == 0 ){` |
|        - |  3208 | `			/* Duplicate name */` |
|   562671 |  3209 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   562671 |  3210 | `			if( zName == 0 ){` |
|      ! 0 |  3211 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3212 | `				return SXERR_ABORT;` |
|        - |  3213 | `			}` |
|        - |  3214 | `			/* Install in the hashtable */` |
|   562671 |  3215 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   281338 |  3216 | `		}else{` |
|        - |  3217 | `			/* Name already available */` |
|  8212907 |  3218 | `			zName = (char *)pEntry->pUserData;` |
|        - |  3219 | `		}` |
|  8775573 |  3220 | `		p3 = (void *)zName;` |
|        - |  3221 | `	}` |
|  8775587 |  3222 | `	iP1 = 0;` |
|  8775587 |  3223 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  2648085 |  3224 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - |  3225 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  2648067 |  3226 | `			iP1 = 1;` |
|  1324031 |  3227 | `		}` |
|  1324040 |  3228 | `	}` |
|        - |  3229 | `	/* Emit the load instruction */` |
|  8775587 |  3230 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  8775599 |  3231 | `	while( iVv > 0 ){` |
|       13 |  3232 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|       13 |  3233 | `		iVv--;` |
|        1 |  3234 | `	}` |
|        - |  3235 | `	/* Node successfully compiled */` |
|  8775587 |  3236 | `	return SXRET_OK;` |
|  4387798 |  3237 | `}` |
|        - |  3238 | `/*` |
|        - |  3239 | ` * Load a literal.` |
|        - |  3240 | ` */` |
|  5573136 |  3241 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 |  3242 | `{` |
|  5573141 |  3243 | `	SyToken *pToken = pGen->pIn;` |
|        - |  3244 | `	ph7_value *pObj;` |
|        - |  3245 | `	SyString *pStr;` |
|        - |  3246 | `	sxu32 nIdx;` |
|        - |  3247 | `	/* Extract token value */` |
|  5573141 |  3248 | `	pStr = &pToken->sData;` |
|        - |  3249 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  5573141 |  3250 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  1347351 |  3251 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - |  3252 | `			/* NULL constant are always indexed at 0 */` |
|   552417 |  3253 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   552417 |  3254 | `			return SXRET_OK;` |
|   794939 |  3255 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - |  3256 | `			/* TRUE constant are always indexed at 1 */` |
|   148569 |  3257 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   148569 |  3258 | `			return SXRET_OK;` |
|        5 |  3259 | `		}` |
|  5024696 |  3260 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   951432 |  3261 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - |  3262 | `			/* FALSE constant are always indexed at 2 */` |
|   404577 |  3263 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   404577 |  3264 | `			return SXRET_OK;` |
|  4101658 |  3265 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   560870 |  3266 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - |  3267 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    11663 |  3268 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    11663 |  3269 | `			if( pObj == 0 ){` |
|      ! 0 |  3270 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3271 | `				return SXERR_ABORT;` |
|        - |  3272 | `			}` |
|    11663 |  3273 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - |  3274 | `			/* Emit the load constant instruction */` |
|    11663 |  3275 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    11663 |  3276 | `			return SXRET_OK;` |
|  3838989 |  3277 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    58848 |  3278 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|        - |  3279 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|        8 |  3280 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        8 |  3281 | `			if( pObj == 0 ){` |
|      ! 0 |  3282 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3283 | `				return SXERR_ABORT;` |
|        - |  3284 | `			}` |
|        8 |  3285 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - |  3286 | `				SyString sNs;` |
|        8 |  3287 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  3288 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|        5 |  3289 | `			}else{` |
|      ! 0 |  3290 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  3291 | `			}` |
|        8 |  3292 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        8 |  3293 | `			return SXRET_OK;` |
|  3831439 |  3294 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   151986 |  3295 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  3917758 |  3296 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   216422 |  3297 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|       11 |  3298 | `			GenBlock *pBlock = pGen->pCurrent;` |
|        - |  3299 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       21 |  3300 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|        - |  3301 | `				/* Point to the upper block */` |
|       11 |  3302 | `				pBlock = pBlock->pParent;` |
|        1 |  3303 | `			}` |
|       11 |  3304 | `			if( pBlock == 0 ){` |
|        - |  3305 | `				/* Called in the global scope,load NULL */` |
|        5 |  3306 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        3 |  3307 | `			}else{` |
|        - |  3308 | `				/* Extract the target function/method */` |
|        7 |  3309 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        7 |  3310 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        - |  3311 | `					/* Not a class method,Load null */` |
|        3 |  3312 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3313 | `				}else{` |
|        5 |  3314 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        5 |  3315 | `					if( pObj == 0 ){` |
|      ! 0 |  3316 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3317 | `						return SXERR_ABORT;` |
|        - |  3318 | `					}` |
|        5 |  3319 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|        - |  3320 | `					/* Emit the load constant instruction */` |
|        5 |  3321 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  3322 | `				}` |
|        - |  3323 | `			}` |
|       11 |  3324 | `			return SXRET_OK;` |
|        - |  3325 | `	}` |
|        - |  3326 | `	/* Query literal table */` |
|  4455919 |  3327 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|        - |  3328 | `		ph7_value *pLitObj;` |
|        - |  3329 | `		/* Unknown literal,install it in the literal table */` |
|   908115 |  3330 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   908115 |  3331 | `		if( pLitObj == 0 ){` |
|      ! 0 |  3332 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3333 | `			return SXERR_ABORT;` |
|        - |  3334 | `		}` |
|   908115 |  3335 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   908115 |  3336 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   454055 |  3337 | `	}` |
|        - |  3338 | `	/* Emit the load constant instruction */` |
|  4455919 |  3339 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  4455919 |  3340 | `	return SXRET_OK;` |
|  2786573 |  3341 | `}` |
|        - |  3342 | `/*` |
|        - |  3343 | ` * Resolve a namespace path or simply load a literal.` |
|        - |  3344 | ` * If the token stream contains namespace separators (backslashes),` |
|        - |  3345 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - |  3346 | ` * Otherwise, load the simple literal directly.` |
|        - |  3347 | ` */` |
|  5577068 |  3348 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 |  3349 | `{` |
|        - |  3350 | `	sxi32 rc;` |
|  5577073 |  3351 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  3352 | `		return SXRET_OK;` |
|        - |  3353 | `	}` |
|        - |  3354 | `	/* Check if this is a multi-token namespace path */` |
|  5577073 |  3355 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - |  3356 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     3937 |  3357 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     3937 |  3358 | `		int isAbsolute = 0;` |
|     3937 |  3359 | `		SyBlobReset(pWorker);` |
|        - |  3360 | `		/* Check for leading backslash (absolute path) */` |
|     3937 |  3361 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     3935 |  3362 | `			isAbsolute = 1;` |
|     3935 |  3363 | `			pGen->pIn++; /* Skip leading backslash */` |
|     1965 |  3364 | `		}` |
|        - |  3365 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|     3937 |  3366 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        3 |  3367 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 |  3368 | `			SyBlobAppend(pWorker,"\\",1);` |
|        1 |  3369 | `		}` |
|        - |  3370 | `		/* Collect all path components */` |
|     4045 |  3371 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4045 |  3372 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       58 |  3373 | `				SyBlobAppend(pWorker,"\\",1);` |
|       31 |  3374 | `			}else{` |
|     3991 |  3375 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  3376 | `			}` |
|     4045 |  3377 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     3937 |  3378 | `				pGen->pIn++;` |
|     3937 |  3379 | `				break;` |
|        - |  3380 | `			}` |
|      112 |  3381 | `			pGen->pIn++;` |
|        4 |  3382 | `		}` |
|     3937 |  3383 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - |  3384 | `			ph7_value *pObj;` |
|        - |  3385 | `			SyString sPath;` |
|        - |  3386 | `			sxu32 nIdx;` |
|     3937 |  3387 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - |  3388 | `			/* Install in the literal table */` |
|     3937 |  3389 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     3909 |  3390 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3909 |  3391 | `				if( pObj == 0 ){` |
|      ! 0 |  3392 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3393 | `					return SXERR_ABORT;` |
|        - |  3394 | `				}` |
|     3909 |  3395 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     3909 |  3396 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1952 |  3397 | `			}` |
|        - |  3398 | `			/* Emit the load constant instruction.` |
|        - |  3399 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - |  3400 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     5903 |  3401 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     1966 |  3402 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     1966 |  3403 | `				nIdx,0,0);` |
|     3937 |  3404 | `			return SXRET_OK;` |
|        - |  3405 | `		}` |
|      ! 0 |  3406 | `	}` |
|        - |  3407 | `	/* Single-token literal: load directly */` |
|  5573141 |  3408 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  5573141 |  3409 | `	return rc;` |
|  2788539 |  3410 | `}` |
|        - |  3411 | `/*` |
|        - |  3412 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|        - |  3413 | ` */` |
|        - |  3414 | `/*` |
|        - |  3415 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|        - |  3416 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|        - |  3417 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|        - |  3418 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|        - |  3419 | ` */` |
|      ! 0 |  3420 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|      ! 0 |  3421 | `{` |
|      ! 0 |  3422 | `	SXUNUSED(iCompileFlag);` |
|      ! 0 |  3423 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - |  3424 | `		"Cannot use the first-class callable syntax '...' here");` |
|      ! 0 |  3425 | `	return SXERR_SYNTAX;` |
|      ! 0 |  3426 | `}` |
|  5577068 |  3427 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3428 | `{` |
|        - |  3429 | `	sxi32 rc;` |
|  5577073 |  3430 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  5577073 |  3431 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3432 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3433 | `		return rc;` |
|        - |  3434 | `	}` |
|        - |  3435 | `	/* Node successfully compiled */` |
|  5577073 |  3436 | `	return SXRET_OK;` |
|  2788539 |  3437 | `}` |
|        - |  3438 | `/*` |
|        - |  3439 | ` * Recover from a compile-time error. In other words synchronize` |
|        - |  3440 | ` * the token stream cursor with the first semi-colon seen.` |
|        - |  3441 | ` */` |
|        8 |  3442 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|        1 |  3443 | `{` |
|        - |  3444 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       17 |  3445 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|        9 |  3446 | `		pGen->pIn++;` |
|        1 |  3447 | `	}` |
|        9 |  3448 | `	return SXRET_OK;` |
|        1 |  3449 | `}` |
|        - |  3450 | `/*` |
|        - |  3451 | ` * Check if the given identifier name is reserved or not.` |
|        - |  3452 | ` * Return TRUE if reserved.FALSE otherwise.` |
|        - |  3453 | ` */` |
|   143912 |  3454 | `static int GenStateIsReservedConstant(SyString *pName)` |
|        5 |  3455 | `{` |
|   143917 |  3456 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|       48 |  3457 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|        3 |  3458 | `			return TRUE;` |
|       46 |  3459 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|        6 |  3460 | `			return TRUE;` |
|        3 |  3461 | `		}` |
|   143892 |  3462 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       16 |  3463 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|        3 |  3464 | `			return TRUE;` |
|        - |  3465 | `		}` |
|        6 |  3466 | `	}` |
|        - |  3467 | `	/* Not a reserved constant */` |
|   143909 |  3468 | `	return FALSE;` |
|    71961 |  3469 | `}` |
|        - |  3470 | `/*` |
|        - |  3471 | ` * Compile the 'const' statement.` |
|        - |  3472 | ` * According to the PHP language reference` |
|        - |  3473 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|        - |  3474 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|        - |  3475 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|        - |  3476 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|        - |  3477 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3478 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|        - |  3479 | ` *  Syntax` |
|        - |  3480 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|        - |  3481 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|        - |  3482 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|        - |  3483 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|        - |  3484 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|        - |  3485 | ` *  to get a list of all defined constants.` |
|        - |  3486 | ` *` |
|        - |  3487 | ` * Symisc eXtension.` |
|        - |  3488 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|        - |  3489 | ` *  would allow only simple scalar value.` |
|        - |  3490 | ` *  Example` |
|        - |  3491 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  3492 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  3493 | ` */` |
|       44 |  3494 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |  3495 | `{` |
|        - |  3496 | `	SySet *pConsCode,*pInstrContainer;` |
|       49 |  3497 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3498 | `	SyString *pName;` |
|        - |  3499 | `	sxi32 rc;` |
|       49 |  3500 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       49 |  3501 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  3502 | `		/* Invalid constant name */` |
|        8 |  3503 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|        8 |  3504 | `		if( rc == SXERR_ABORT ){` |
|        - |  3505 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3506 | `			return SXERR_ABORT;` |
|        - |  3507 | `		}` |
|        8 |  3508 | `		goto Synchronize;` |
|        - |  3509 | `	}` |
|        - |  3510 | `	/* Peek constant name */` |
|       43 |  3511 | `	pName = &pGen->pIn->sData;` |
|        - |  3512 | `	/* Make sure the constant name isn't reserved */` |
|       43 |  3513 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  3514 | `		/* Reserved constant */` |
|       10 |  3515 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       10 |  3516 | `		if( rc == SXERR_ABORT ){` |
|        - |  3517 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3518 | `			return SXERR_ABORT;` |
|        - |  3519 | `		}` |
|       10 |  3520 | `		goto Synchronize;` |
|        - |  3521 | `	}` |
|       34 |  3522 | `	pGen->pIn++;` |
|       34 |  3523 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  3524 | `		/* Invalid statement*/` |
|        6 |  3525 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|        6 |  3526 | `		if( rc == SXERR_ABORT ){` |
|        - |  3527 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3528 | `			return SXERR_ABORT;` |
|        - |  3529 | `		}` |
|        6 |  3530 | `		goto Synchronize;` |
|        - |  3531 | `	}` |
|       28 |  3532 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |  3533 | `	/* Allocate a new constant value container */` |
|       28 |  3534 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       28 |  3535 | `	if( pConsCode == 0 ){` |
|      ! 0 |  3536 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3537 | `		return SXERR_ABORT;` |
|        - |  3538 | `	}` |
|       28 |  3539 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  3540 | `	/* Swap bytecode container */` |
|       28 |  3541 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       28 |  3542 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |  3543 | `	/* Compile constant value */` |
|       28 |  3544 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  3545 | `	/* Emit the done instruction */` |
|       28 |  3546 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       28 |  3547 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       28 |  3548 | `	if( rc == SXERR_ABORT ){` |
|        - |  3549 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  3550 | `		return SXERR_ABORT;` |
|        - |  3551 | `	}` |
|       28 |  3552 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |  3553 | `	/* Register the constant with namespace-qualified name */` |
|        - |  3554 | `	{` |
|        - |  3555 | `		SyBlob sFQN;` |
|        - |  3556 | `		SyString sFQNStr;` |
|       28 |  3557 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       28 |  3558 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       28 |  3559 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       41 |  3560 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|       26 |  3561 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|       28 |  3562 | `		SyBlobRelease(&sFQN);` |
|        - |  3563 | `	}` |
|       28 |  3564 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3565 | `		SySetRelease(pConsCode);` |
|      ! 0 |  3566 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  3567 | `	}` |
|       28 |  3568 | `	return SXRET_OK;` |
|        9 |  3569 | `Synchronize:` |
|        - |  3570 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       60 |  3571 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       42 |  3572 | `		pGen->pIn++;` |
|        4 |  3573 | `	}` |
|       22 |  3574 | `	return SXRET_OK;` |
|       27 |  3575 | `}` |
|        - |  3576 | `/*` |
|        - |  3577 | ` * Compile the 'continue' statement.` |
|        - |  3578 | ` * According to the PHP language reference` |
|        - |  3579 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  3580 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  3581 | ` *  iteration.` |
|        - |  3582 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  3583 | ` *  the purposes of continue.` |
|        - |  3584 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  3585 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  3586 | ` *  Note:` |
|        - |  3587 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  3588 | ` */` |
|        - |  3589 | `/*` |
|        - |  3590 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|        - |  3591 | ` * block and the target loop block. This ensures finally blocks run when` |
|        - |  3592 | ` * break/continue crosses a try boundary.` |
|        - |  3593 | ` *` |
|        - |  3594 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|        - |  3595 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|        - |  3596 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|        - |  3597 | ` */` |
|    58412 |  3598 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  3599 | `{` |
|    58417 |  3600 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    58417 |  3601 | `	int nInlineTry = 0;` |
|   272279 |  3602 | `	while( pBlock && pBlock != pTarget ){` |
|   213867 |  3603 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        6 |  3604 | `			if( pBlock->pUserData ){` |
|        - |  3605 | `				/* A try block with an exception context. In a generator its catch/finally` |
|        - |  3606 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|        - |  3607 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|        - |  3608 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|        6 |  3609 | `				if( pGen->bInGenerator ){` |
|        3 |  3610 | `					nInlineTry++;` |
|        2 |  3611 | `				}else{` |
|        3 |  3612 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|        - |  3613 | `				}` |
|        4 |  3614 | `			}else{` |
|        - |  3615 | `				/* A catch/finally block compiled into a separate bytecode container` |
|        - |  3616 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|      ! 0 |  3617 | `				break;` |
|        - |  3618 | `			}` |
|        2 |  3619 | `		}` |
|   213867 |  3620 | `		pBlock = pBlock->pParent;` |
|        5 |  3621 | `	}` |
|    58417 |  3622 | `	return nInlineTry;` |
|        5 |  3623 | `}` |
|    27238 |  3624 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  3625 | `{` |
|        - |  3626 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3627 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3628 | `	sxu32 nLineLocal;` |
|        - |  3629 | `	sxi32 rc;` |
|    27243 |  3630 | `	nLineLocal = pGen->pIn->nLine;` |
|    27243 |  3631 | `	iLevel = 0;` |
|        - |  3632 | `	/* Jump the 'continue' keyword */` |
|    27243 |  3633 | `	pGen->pIn++;` |
|    27243 |  3634 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3635 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3636 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3637 | `		 */` |
|        - |  3638 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  3639 | `		char *zAlloc = 0;` |
|        - |  3640 | `		SyString sNum;` |
|       17 |  3641 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  3642 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3643 | `			return SXERR_ABORT;` |
|        - |  3644 | `		}` |
|       17 |  3645 | `		if( rc == SXRET_OK ){` |
|       20 |  3646 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3647 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  3648 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3649 | `				return SXERR_ABORT;` |
|        - |  3650 | `			}` |
|       14 |  3651 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  3652 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3653 | `		}` |
|       17 |  3654 | `		if( iLevel < 2 ){` |
|        3 |  3655 | `			iLevel = 0;` |
|        1 |  3656 | `		}` |
|       17 |  3657 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3658 | `	}` |
|        - |  3659 | `	/* Point to the target loop */` |
|    27243 |  3660 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    27243 |  3661 | `	if( pLoop == 0 ){` |
|        - |  3662 | `		/* Illegal continue */` |
|       12 |  3663 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|       12 |  3664 | `		if( rc == SXERR_ABORT ){` |
|        - |  3665 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3666 | `			return SXERR_ABORT;` |
|        - |  3667 | `		}` |
|        7 |  3668 | `	}else{` |
|    27233 |  3669 | `		sxu32 nInstrIdx = 0;` |
|        - |  3670 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    27233 |  3671 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3672 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  3673 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    27233 |  3674 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    27233 |  3675 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  3676 | `			/* According to the PHP language reference manual` |
|        - |  3677 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|        - |  3678 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|        - |  3679 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|        - |  3680 | `			 */` |
|        5 |  3681 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|        5 |  3682 | `			if( rc == SXRET_OK ){` |
|        5 |  3683 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  3684 | `			}` |
|        3 |  3685 | `		}else{` |
|        - |  3686 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    27229 |  3687 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    27229 |  3688 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  3689 | `				JumpFixup sJumpFix;` |
|        - |  3690 | `				/* Post-continue */` |
|       14 |  3691 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       14 |  3692 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       14 |  3693 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|        6 |  3694 | `			}` |
|        - |  3695 | `		}` |
|        - |  3696 | `	}` |
|    27243 |  3697 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3698 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3699 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  3700 | `	}` |
|        - |  3701 | `	/* Statement successfully compiled */` |
|    27243 |  3702 | `	return SXRET_OK;` |
|    13624 |  3703 | `}` |
|        - |  3704 | `/*` |
|        - |  3705 | ` * Compile the 'break' statement.` |
|        - |  3706 | ` * According to the PHP language reference` |
|        - |  3707 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  3708 | ` *  structure.` |
|        - |  3709 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  3710 | ` *  enclosing structures are to be broken out of.` |
|        - |  3711 | ` */` |
|    31200 |  3712 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  3713 | `{` |
|        - |  3714 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3715 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3716 | `	sxi32 rc;` |
|    31205 |  3717 | `	iLevel = 0;` |
|        - |  3718 | `	/* Jump the 'break' keyword */` |
|    31205 |  3719 | `	pGen->pIn++;` |
|    31205 |  3720 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3721 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3722 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3723 | `		 */` |
|        - |  3724 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       18 |  3725 | `		char *zAlloc = 0;` |
|        - |  3726 | `		SyString sNum;` |
|       18 |  3727 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       18 |  3728 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3729 | `			return SXERR_ABORT;` |
|        - |  3730 | `		}` |
|       18 |  3731 | `		if( rc == SXRET_OK ){` |
|       21 |  3732 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3733 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       15 |  3734 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3735 | `				return SXERR_ABORT;` |
|        - |  3736 | `			}` |
|       15 |  3737 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       15 |  3738 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3739 | `		}` |
|       18 |  3740 | `		if( iLevel < 2 ){` |
|        3 |  3741 | `			iLevel = 0;` |
|        1 |  3742 | `		}` |
|       18 |  3743 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3744 | `	}` |
|        - |  3745 | `	/* Extract the target loop */` |
|    31205 |  3746 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    31205 |  3747 | `	if( pLoop == 0 ){` |
|        - |  3748 | `		/* Illegal break */` |
|       19 |  3749 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|       19 |  3750 | `		if( rc == SXERR_ABORT ){` |
|        - |  3751 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3752 | `			return SXERR_ABORT;` |
|        - |  3753 | `		}` |
|       11 |  3754 | `	}else{` |
|        - |  3755 | `		sxu32 nInstrIdx;` |
|        - |  3756 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    31189 |  3757 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3758 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    31189 |  3759 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    31189 |  3760 | `		if( rc == SXRET_OK ){` |
|        - |  3761 | `			/* Fix the jump later when the jump destination is resolved */` |
|    31189 |  3762 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    15592 |  3763 | `		}` |
|        - |  3764 | `	}` |
|    31205 |  3765 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3766 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3767 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  3768 | `	}` |
|        - |  3769 | `	/* Statement successfully compiled */` |
|    31205 |  3770 | `	return SXRET_OK;` |
|    15605 |  3771 | `}` |
|        - |  3772 | `/*` |
|        - |  3773 | ` * Compile or record a label.` |
|        - |  3774 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  3775 | ` * Example` |
|        - |  3776 | ` *  goto LABEL;` |
|        - |  3777 | ` *   echo 'Foo';` |
|        - |  3778 | ` *  LABEL:` |
|        - |  3779 | ` *   echo 'Bar';` |
|        - |  3780 | ` */` |
|      112 |  3781 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  3782 | `{` |
|        - |  3783 | `	GenBlock *pBlock;` |
|        - |  3784 | `	Label sLabel;` |
|        - |  3785 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|      117 |  3786 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|      117 |  3787 | `	if( pBlock ){` |
|        - |  3788 | `		sxi32 rc;` |
|        8 |  3789 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        4 |  3790 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|        6 |  3791 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3792 | `			return SXERR_ABORT;` |
|        - |  3793 | `		}` |
|        4 |  3794 | `	}else{` |
|      113 |  3795 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3796 | `		char *zDup;` |
|        - |  3797 | `		/* Initialize label fields */` |
|      113 |  3798 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  3799 | `		/* Duplicate label name */` |
|      113 |  3800 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      113 |  3801 | `		if( zDup == 0 ){` |
|      ! 0 |  3802 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3803 | `			return SXERR_ABORT;` |
|        - |  3804 | `		}` |
|      113 |  3805 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      113 |  3806 | `		sLabel.bRef  = FALSE;` |
|      113 |  3807 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      113 |  3808 | `		pBlock = pGen->pCurrent;` |
|      221 |  3809 | `		while( pBlock ){` |
|      133 |  3810 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       24 |  3811 | `				break;` |
|        - |  3812 | `			}` |
|        - |  3813 | `			/* Point to the upper block */` |
|      113 |  3814 | `			pBlock = pBlock->pParent;` |
|        5 |  3815 | `		}` |
|      113 |  3816 | `		if( pBlock ){` |
|       24 |  3817 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       14 |  3818 | `		}else{` |
|       93 |  3819 | `			sLabel.pFunc = 0;` |
|        - |  3820 | `		}` |
|        - |  3821 | `		/* Insert in label set */` |
|      113 |  3822 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  3823 | `	}` |
|      117 |  3824 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      117 |  3825 | `	return SXRET_OK;` |
|       61 |  3826 | `}` |
|        - |  3827 | `/*` |
|        - |  3828 | ` * Compile the so hated 'goto' statement.` |
|        - |  3829 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  3830 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  3831 | ` * a compiler it has to do this.` |
|        - |  3832 | ` * According to the PHP language reference manual` |
|        - |  3833 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  3834 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  3835 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  3836 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  3837 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  3838 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  3839 | ` *   of a multi-level break` |
|        - |  3840 | ` */` |
|      152 |  3841 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  3842 | `{` |
|        - |  3843 | `	JumpFixup sJump;` |
|        - |  3844 | `	sxi32 rc;` |
|      157 |  3845 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      157 |  3846 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3847 | `		/* Missing label */` |
|      ! 0 |  3848 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  3849 | `		if( rc == SXERR_ABORT ){` |
|        - |  3850 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3851 | `			return SXERR_ABORT;` |
|        - |  3852 | `		}` |
|      ! 0 |  3853 | `		return SXRET_OK;` |
|        - |  3854 | `	}` |
|      157 |  3855 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        6 |  3856 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|        6 |  3857 | `		if( rc == SXERR_ABORT ){` |
|        - |  3858 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3859 | `			return SXERR_ABORT;` |
|        - |  3860 | `		}` |
|        4 |  3861 | `	}else{` |
|      153 |  3862 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3863 | `		GenBlock *pBlock;` |
|        - |  3864 | `		char *zDup;` |
|        - |  3865 | `		/* Prepare the jump destination */` |
|      153 |  3866 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      153 |  3867 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  3868 | `		/* Duplicate label name */` |
|      153 |  3869 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      153 |  3870 | `		if( zDup == 0 ){` |
|      ! 0 |  3871 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3872 | `			return SXERR_ABORT;` |
|        - |  3873 | `		}` |
|      153 |  3874 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|      153 |  3875 | `		pBlock = pGen->pCurrent;` |
|      315 |  3876 | `		while( pBlock ){` |
|      199 |  3877 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       37 |  3878 | `				break;` |
|        - |  3879 | `			}` |
|        - |  3880 | `			/* Point to the upper block */` |
|      167 |  3881 | `			pBlock = pBlock->pParent;` |
|        5 |  3882 | `		}` |
|      153 |  3883 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        9 |  3884 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|        9 |  3885 | `			if( rc == SXERR_ABORT ){` |
|        - |  3886 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  3887 | `				return SXERR_ABORT;` |
|        - |  3888 | `			}` |
|        3 |  3889 | `		}` |
|      153 |  3890 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       30 |  3891 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       17 |  3892 | `		}else{` |
|      127 |  3893 | `			sJump.pFunc = 0;` |
|        - |  3894 | `		}` |
|        - |  3895 | `		/* Emit the unconditional jump */` |
|      153 |  3896 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      153 |  3897 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|       74 |  3898 | `		}` |
|        - |  3899 | `	}` |
|      157 |  3900 | `	pGen->pIn++; /* Jump the label name */` |
|      157 |  3901 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  3902 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  3903 | `	}` |
|        - |  3904 | `	/* Statement successfully compiled */` |
|      157 |  3905 | `	return SXRET_OK;` |
|       81 |  3906 | `}` |
|        - |  3907 | `/*` |
|        - |  3908 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  3909 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  3910 | ` * failure.` |
|        - |  3911 | ` */` |
|       20 |  3912 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  3913 | `{` |
|        - |  3914 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  3915 | `	sxu32 nRawObj;` |
|       10 |  3916 | `	sxu32 nObjIdx;` |
|        - |  3917 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  3918 | `	 * a PHP block.` |
|        - |  3919 | `	 */` |
|       10 |  3920 | `Consume:` |
|       22 |  3921 | `	nRawObj = nObjIdx = 0;` |
|       22 |  3922 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  3923 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  3924 | `		if( pRawObj == 0 ){` |
|      ! 0 |  3925 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3926 | `			return SXERR_ABORT;` |
|        - |  3927 | `		}` |
|        - |  3928 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  3929 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  3930 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  3931 | `		++nRawObj;` |
|      ! 0 |  3932 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  3933 | `	}` |
|       22 |  3934 | `	if( nRawObj > 0 ){` |
|        - |  3935 | `		/* Emit the consume instruction */` |
|      ! 0 |  3936 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  3937 | `	}` |
|       22 |  3938 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  3939 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  3940 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  3941 | `		SySetReset(pTokenSet);` |
|      ! 0 |  3942 | `		SySetReset(&pGen->aTrivia);` |
|        - |  3943 | `		/* Tokenize input */` |
|      ! 0 |  3944 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  3945 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  3946 | `		/* Point to the fresh token stream */` |
|      ! 0 |  3947 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  3948 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  3949 | `		/* Advance the stream cursor */` |
|      ! 0 |  3950 | `		pGen->pRawIn++;` |
|        - |  3951 | `		/* TICKET 1433-011 */` |
|      ! 0 |  3952 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  3953 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  3954 | `			sxi32 rc;` |
|        - |  3955 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  3956 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  3957 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  3958 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|      ! 0 |  3959 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  3960 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3961 | `				return SXERR_ABORT;` |
|      ! 0 |  3962 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  3963 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  3964 | `			}` |
|      ! 0 |  3965 | `			goto Consume;` |
|        - |  3966 | `		}` |
|      ! 0 |  3967 | `	}else{` |
|        - |  3968 | `		/* No more chunks to process */` |
|       22 |  3969 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  3970 | `		return SXERR_EOF;` |
|        - |  3971 | `	}` |
|      ! 0 |  3972 | `	return SXRET_OK;` |
|       12 |  3973 | `}` |
|        - |  3974 | `/*` |
|        - |  3975 | ` * Compile a PHP block.` |
|        - |  3976 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  3977 | ` * optionally delimited by braces {}.` |
|        - |  3978 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  3979 | ` * and this function takes care of generating the appropriate error` |
|        - |  3980 | ` * message.` |
|        - |  3981 | ` */` |
|  3009020 |  3982 | `static sxi32 PH7_CompileBlock(` |
|        - |  3983 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  3984 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  3985 | `	)` |
|        5 |  3986 | `{` |
|        - |  3987 | `	sxi32 rc;` |
|        - |  3988 | `	sxu32 nLine;` |
|  3009025 |  3989 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  3007567 |  3990 | `		nLine = pGen->pIn->nLine;` |
|  3007567 |  3991 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  3007567 |  3992 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  3993 | `			return SXERR_ABORT;` |
|        - |  3994 | `		}` |
|  3007567 |  3995 | `		pGen->pIn++;` |
|        - |  3996 | `		/* Compile until we hit the closing braces '}' */` |
|  4398993 |  3997 | `		for(;;){` |
|  8797991 |  3998 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  3999 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  4000 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4001 | `			 	   return SXERR_ABORT;` |
|        - |  4002 | `				}` |
|       22 |  4003 | `				if( rc == SXERR_EOF ){` |
|        - |  4004 | `					/* No more token to process. Missing closing braces */` |
|       22 |  4005 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|       22 |  4006 | `					break;` |
|        - |  4007 | `				}` |
|      ! 0 |  4008 | `			}` |
|  8797971 |  4009 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  4010 | `				/* Closing braces found,break immediately*/` |
|  3007547 |  4011 | `				pGen->pIn++;` |
|  3007547 |  4012 | `				break;` |
|        - |  4013 | `			}` |
|        - |  4014 | `			/* Compile a single statement */` |
|  5790429 |  4015 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  5790429 |  4016 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4017 | `				return SXERR_ABORT;` |
|        - |  4018 | `			}` |
|        5 |  4019 | `		}` |
|  3007567 |  4020 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  1505244 |  4021 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      ! 0 |  4022 | `		pGen->pIn++;` |
|      ! 0 |  4023 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      ! 0 |  4024 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4025 | `			return SXERR_ABORT;` |
|        - |  4026 | `		}` |
|        - |  4027 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      ! 0 |  4028 | `		for(;;){` |
|      ! 0 |  4029 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  4030 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  4031 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4032 | `			 	   return SXERR_ABORT;` |
|        - |  4033 | `				}` |
|      ! 0 |  4034 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  4035 | `					/* No more token to process */` |
|      ! 0 |  4036 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  4037 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  4038 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  4039 | `					}` |
|      ! 0 |  4040 | `					break;` |
|        - |  4041 | `				}` |
|      ! 0 |  4042 | `			}` |
|      ! 0 |  4043 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  4044 | `				sxi32 nKwrd;` |
|        - |  4045 | `				/* Keyword found */` |
|      ! 0 |  4046 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  4047 | `				if( nKwrd == nKeywordEnd \|\|` |
|      ! 0 |  4048 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  4049 | `						/* Delimiter keyword found,break */` |
|      ! 0 |  4050 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      ! 0 |  4051 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      ! 0 |  4052 | `						}` |
|      ! 0 |  4053 | `						break;` |
|        - |  4054 | `				}` |
|      ! 0 |  4055 | `			}` |
|        - |  4056 | `			/* Compile a single statement */` |
|      ! 0 |  4057 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      ! 0 |  4058 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4059 | `				return SXERR_ABORT;` |
|        - |  4060 | `			}` |
|      ! 0 |  4061 | `		}` |
|      ! 0 |  4062 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  4063 | `	}else{` |
|        - |  4064 | `		/* Compile a single statement */` |
|     1463 |  4065 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     1463 |  4066 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4067 | `			return SXERR_ABORT;` |
|        - |  4068 | `		}` |
|        - |  4069 | `	}` |
|        - |  4070 | `	/* Jump trailing semi-colons ';' */` |
|  3009025 |  4071 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4072 | `		pGen->pIn++;` |
|      ! 0 |  4073 | `	}` |
|  3009025 |  4074 | `	return SXRET_OK;` |
|  1504515 |  4075 | `}` |
|        - |  4076 | `/*` |
|        - |  4077 | ` * Compile the gentle 'while' statement.` |
|        - |  4078 | ` * According to the PHP language reference` |
|        - |  4079 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  4080 | ` *  The basic form of a while statement is:` |
|        - |  4081 | ` *  while (expr)` |
|        - |  4082 | ` *   statement` |
|        - |  4083 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  4084 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  4085 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  4086 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  4087 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  4088 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  4089 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  4090 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  4091 | ` *  while (expr):` |
|        - |  4092 | ` *    statement` |
|        - |  4093 | ` *   endwhile;` |
|        - |  4094 | ` */` |
|    15670 |  4095 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  4096 | `{` |
|    15675 |  4097 | `	GenBlock *pWhileBlock = 0;` |
|    15675 |  4098 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  4099 | `	sxu32 nFalseJump;` |
|        - |  4100 | `	sxu32 nLine;` |
|        - |  4101 | `	sxi32 rc;` |
|    15675 |  4102 | `	nLine = pGen->pIn->nLine;` |
|        - |  4103 | `	/* Jump the 'while' keyword */` |
|    15675 |  4104 | `	pGen->pIn++;` |
|    15675 |  4105 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4106 | `		/* Syntax error */` |
|      ! 0 |  4107 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4108 | `		if( rc == SXERR_ABORT ){` |
|        - |  4109 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4110 | `			return SXERR_ABORT;` |
|        - |  4111 | `		}` |
|      ! 0 |  4112 | `		goto Synchronize;` |
|        - |  4113 | `	}` |
|        - |  4114 | `	/* Jump the left parenthesis '(' */` |
|    15675 |  4115 | `	pGen->pIn++;` |
|        - |  4116 | `	/* Create the loop block */` |
|    15675 |  4117 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    15675 |  4118 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4119 | `		return SXERR_ABORT;` |
|        - |  4120 | `	}` |
|        - |  4121 | `	/* Delimit the condition */` |
|    15675 |  4122 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    15675 |  4123 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4124 | `		/* Empty expression */` |
|        3 |  4125 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|        3 |  4126 | `		if( rc == SXERR_ABORT ){` |
|        - |  4127 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4128 | `			return SXERR_ABORT;` |
|        - |  4129 | `		}` |
|        1 |  4130 | `	}` |
|        - |  4131 | `	/* Swap token streams */` |
|    15675 |  4132 | `	pTmp = pGen->pEnd;` |
|    15675 |  4133 | `	pGen->pEnd = pEnd;` |
|        - |  4134 | `	/* Compile the expression */` |
|    15675 |  4135 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    15675 |  4136 | `	if( rc == SXERR_ABORT ){` |
|        - |  4137 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4138 | `		return SXERR_ABORT;` |
|        - |  4139 | `	}` |
|        - |  4140 | `	/* Update token stream */` |
|    15675 |  4141 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4142 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4143 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4144 | `			return SXERR_ABORT;` |
|        - |  4145 | `		}` |
|      ! 0 |  4146 | `		pGen->pIn++;` |
|      ! 0 |  4147 | `	}` |
|        - |  4148 | `	/* Synchronize pointers */` |
|    15675 |  4149 | `	pGen->pIn  = &pEnd[1];` |
|    15675 |  4150 | `	pGen->pEnd = pTmp;` |
|        - |  4151 | `	/* Emit the false jump */` |
|    15675 |  4152 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4153 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    15675 |  4154 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  4155 | `	/* Compile the loop body */` |
|    15675 |  4156 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    15675 |  4157 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4158 | `		return SXERR_ABORT;` |
|        - |  4159 | `	}` |
|        - |  4160 | `	/* Emit the unconditional jump to the start of the loop */` |
|    15675 |  4161 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  4162 | `	/* Fix all jumps now the destination is resolved */` |
|    15675 |  4163 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4164 | `	/* Release the loop block */` |
|    15675 |  4165 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4166 | `	/* Statement successfully compiled */` |
|    15675 |  4167 | `	return SXRET_OK;` |
|      ! 0 |  4168 | `Synchronize:` |
|        - |  4169 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4170 | `	 * compiling this erroneous block.` |
|        - |  4171 | `	 */` |
|      ! 0 |  4172 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4173 | `		pGen->pIn++;` |
|      ! 0 |  4174 | `	}` |
|      ! 0 |  4175 | `	return SXRET_OK;` |
|     7840 |  4176 | `}` |
|        - |  4177 | `/*` |
|        - |  4178 | ` * Compile the ugly do..while() statement.` |
|        - |  4179 | ` * According to the PHP language reference` |
|        - |  4180 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  4181 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  4182 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  4183 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  4184 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  4185 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  4186 | ` *  would end immediately).` |
|        - |  4187 | ` *  There is just one syntax for do-while loops:` |
|        - |  4188 | ` *  <?php` |
|        - |  4189 | ` *  $i = 0;` |
|        - |  4190 | ` *  do {` |
|        - |  4191 | ` *   echo $i;` |
|        - |  4192 | ` *  } while ($i > 0);` |
|        - |  4193 | ` * ?>` |
|        - |  4194 | ` */` |
|        2 |  4195 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  4196 | `{` |
|        3 |  4197 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  4198 | `	GenBlock *pDoBlock = 0;` |
|        - |  4199 | `	sxu32 nLine;` |
|        - |  4200 | `	sxi32 rc;` |
|        3 |  4201 | `	nLine = pGen->pIn->nLine;` |
|        - |  4202 | `	/* Jump the 'do' keyword */` |
|        3 |  4203 | `	pGen->pIn++;` |
|        - |  4204 | `	/* Create the loop block */` |
|        3 |  4205 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  4206 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4207 | `		return SXERR_ABORT;` |
|        - |  4208 | `	}` |
|        - |  4209 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  4210 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  4211 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  4212 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4213 | `		return SXERR_ABORT;` |
|        - |  4214 | `	}` |
|        3 |  4215 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4216 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  4217 | `	}` |
|        3 |  4218 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  4219 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  4220 | `			/* Missing 'while' statement */` |
|        3 |  4221 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|        3 |  4222 | `			if( rc == SXERR_ABORT ){` |
|        - |  4223 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4224 | `				return SXERR_ABORT;` |
|        - |  4225 | `			}` |
|        3 |  4226 | `			goto Synchronize;` |
|        - |  4227 | `	}` |
|        - |  4228 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  4229 | `	pGen->pIn++;` |
|      ! 0 |  4230 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4231 | `		/* Syntax error */` |
|      ! 0 |  4232 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4233 | `		if( rc == SXERR_ABORT ){` |
|        - |  4234 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4235 | `			return SXERR_ABORT;` |
|        - |  4236 | `		}` |
|      ! 0 |  4237 | `		goto Synchronize;` |
|        - |  4238 | `	}` |
|        - |  4239 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  4240 | `	pGen->pIn++;` |
|        - |  4241 | `	/* Delimit the condition */` |
|      ! 0 |  4242 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  4243 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4244 | `		/* Empty expression */` |
|      ! 0 |  4245 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|      ! 0 |  4246 | `		if( rc == SXERR_ABORT ){` |
|        - |  4247 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4248 | `			return SXERR_ABORT;` |
|        - |  4249 | `		}` |
|      ! 0 |  4250 | `		goto Synchronize;` |
|        - |  4251 | `	}` |
|        - |  4252 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  4253 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  4254 | `		JumpFixup *aPost;` |
|        - |  4255 | `		VmInstr *pInstr;` |
|        - |  4256 | `		sxu32 nJumpDest;` |
|        - |  4257 | `		sxu32 n;` |
|      ! 0 |  4258 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  4259 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  4260 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  4261 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  4262 | `			if( pInstr ){` |
|        - |  4263 | `				/* Fix */` |
|      ! 0 |  4264 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  4265 | `			}` |
|      ! 0 |  4266 | `		}` |
|      ! 0 |  4267 | `	}` |
|        - |  4268 | `	/* Swap token streams */` |
|      ! 0 |  4269 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  4270 | `	pGen->pEnd = pEnd;` |
|        - |  4271 | `	/* Compile the expression */` |
|      ! 0 |  4272 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  4273 | `	if( rc == SXERR_ABORT ){` |
|        - |  4274 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4275 | `		return SXERR_ABORT;` |
|        - |  4276 | `	}` |
|        - |  4277 | `	/* Update token stream */` |
|      ! 0 |  4278 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4279 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4280 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4281 | `			return SXERR_ABORT;` |
|        - |  4282 | `		}` |
|      ! 0 |  4283 | `		pGen->pIn++;` |
|      ! 0 |  4284 | `	}` |
|      ! 0 |  4285 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  4286 | `	pGen->pEnd = pTmp;` |
|        - |  4287 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  4288 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  4289 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  4290 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4291 | `	/* Release the loop block */` |
|      ! 0 |  4292 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4293 | `	/* Statement successfully compiled */` |
|      ! 0 |  4294 | `	return SXRET_OK;` |
|        1 |  4295 | `Synchronize:` |
|        - |  4296 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4297 | `	 * compiling this erroneous block.` |
|        - |  4298 | `	 */` |
|        3 |  4299 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4300 | `		pGen->pIn++;` |
|      ! 0 |  4301 | `	}` |
|        3 |  4302 | `	return SXRET_OK;` |
|        2 |  4303 | `}` |
|        - |  4304 | `/*` |
|        - |  4305 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  4306 | ` * According to the PHP language reference` |
|        - |  4307 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  4308 | ` *  The syntax of a for loop is:` |
|        - |  4309 | ` *  for (expr1; expr2; expr3)` |
|        - |  4310 | ` *   statement` |
|        - |  4311 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  4312 | ` *  the beginning of the loop.` |
|        - |  4313 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  4314 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  4315 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  4316 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  4317 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  4318 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  4319 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  4320 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  4321 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  4322 | ` *  of using the for truth expression.` |
|        - |  4323 | ` */` |
|    38976 |  4324 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  4325 | `{` |
|    38981 |  4326 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    38981 |  4327 | `	GenBlock *pForBlock = 0;` |
|        - |  4328 | `	sxu32 nFalseJump;` |
|        - |  4329 | `	sxu32 nLine;` |
|        - |  4330 | `	sxi32 rc;` |
|    38981 |  4331 | `	nLine = pGen->pIn->nLine;` |
|        - |  4332 | `	/* Jump the 'for' keyword */` |
|    38981 |  4333 | `	pGen->pIn++;` |
|    38981 |  4334 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4335 | `		/* Syntax error */` |
|      ! 0 |  4336 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  4337 | `		if( rc == SXERR_ABORT ){` |
|        - |  4338 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4339 | `			return SXERR_ABORT;` |
|        - |  4340 | `		}` |
|      ! 0 |  4341 | `		return SXRET_OK;` |
|        - |  4342 | `	}` |
|        - |  4343 | `	/* Jump the left parenthesis '(' */` |
|    38981 |  4344 | `	pGen->pIn++;` |
|        - |  4345 | `	/* Delimit the init-expr;condition;post-expr */` |
|    38981 |  4346 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    38981 |  4347 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4348 | `		/* Empty expression */` |
|      ! 0 |  4349 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  4350 | `		if( rc == SXERR_ABORT ){` |
|        - |  4351 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4352 | `			return SXERR_ABORT;` |
|        - |  4353 | `		}` |
|        - |  4354 | `		/* Synchronize */` |
|      ! 0 |  4355 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4356 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4357 | `			pGen->pIn++;` |
|      ! 0 |  4358 | `		}` |
|      ! 0 |  4359 | `		return SXRET_OK;` |
|        - |  4360 | `	}` |
|        - |  4361 | `	/* Swap token streams */` |
|    38981 |  4362 | `	pTmp = pGen->pEnd;` |
|    38981 |  4363 | `	pGen->pEnd = pEnd;` |
|        - |  4364 | `	/* Compile initialization expressions if available */` |
|    38981 |  4365 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4366 | `	/* Pop operand lvalues */` |
|    38981 |  4367 | `	if( rc == SXERR_ABORT ){` |
|        - |  4368 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4369 | `		return SXERR_ABORT;` |
|    38981 |  4370 | `	}else if( rc != SXERR_EMPTY ){` |
|    38979 |  4371 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19487 |  4372 | `	}` |
|    38981 |  4373 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4374 | `		/* Syntax error */` |
|      ! 0 |  4375 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4376 | `			"for: Expected ';' after initialization expressions");` |
|      ! 0 |  4377 | `		if( rc == SXERR_ABORT ){` |
|        - |  4378 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4379 | `			return SXERR_ABORT;` |
|        - |  4380 | `		}` |
|      ! 0 |  4381 | `		return SXRET_OK;` |
|        - |  4382 | `	}` |
|        - |  4383 | `	/* Jump the trailing ';' */` |
|    38981 |  4384 | `	pGen->pIn++;` |
|        - |  4385 | `	/* Create the loop block */` |
|    38981 |  4386 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    38981 |  4387 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4388 | `		return SXERR_ABORT;` |
|        - |  4389 | `	}` |
|        - |  4390 | `	/* Deffer continue jumps */` |
|    38981 |  4391 | `	pForBlock->bPostContinue = TRUE;` |
|        - |  4392 | `	/* Compile the condition */` |
|    38981 |  4393 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38981 |  4394 | `	if( rc == SXERR_ABORT ){` |
|        - |  4395 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4396 | `		return SXERR_ABORT;` |
|    38981 |  4397 | `	}else if( rc != SXERR_EMPTY ){` |
|        - |  4398 | `		/* Emit the false jump */` |
|    38979 |  4399 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4400 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    38979 |  4401 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    19487 |  4402 | `	}` |
|    38981 |  4403 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4404 | `		/* Syntax error */` |
|        6 |  4405 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4406 | `			"for: Expected ';' after conditionals expressions");` |
|        6 |  4407 | `		if( rc == SXERR_ABORT ){` |
|        - |  4408 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4409 | `			return SXERR_ABORT;` |
|        - |  4410 | `		}` |
|        6 |  4411 | `		return SXRET_OK;` |
|        - |  4412 | `	}` |
|        - |  4413 | `	/* Jump the trailing ';' */` |
|    38977 |  4414 | `	pGen->pIn++;` |
|        - |  4415 | `	/* Save the post condition stream */` |
|    38977 |  4416 | `	pPostStart = pGen->pIn;` |
|        - |  4417 | `	/* Compile the loop body */` |
|    38977 |  4418 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    38977 |  4419 | `	pGen->pEnd = pTmp;` |
|    38977 |  4420 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    38977 |  4421 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4422 | `		return SXERR_ABORT;` |
|        - |  4423 | `	}` |
|        - |  4424 | `	/* Fix post-continue jumps */` |
|    38977 |  4425 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - |  4426 | `		JumpFixup *aPost;` |
|        - |  4427 | `		VmInstr *pInstr;` |
|        - |  4428 | `		sxu32 nJumpDest;` |
|        - |  4429 | `		sxu32 n;` |
|       14 |  4430 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       14 |  4431 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       26 |  4432 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       14 |  4433 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       14 |  4434 | `			if( pInstr ){` |
|        - |  4435 | `				/* Fix jump */` |
|       14 |  4436 | `				pInstr->iP2 = nJumpDest;` |
|        6 |  4437 | `			}` |
|        8 |  4438 | `		}` |
|        6 |  4439 | `	}` |
|        - |  4440 | `	/* compile the post-expressions if available */` |
|    38977 |  4441 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4442 | `		pPostStart++;` |
|      ! 0 |  4443 | `	}` |
|    38977 |  4444 | `	if( pPostStart < pEnd ){` |
|        - |  4445 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    38977 |  4446 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    38977 |  4447 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38977 |  4448 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - |  4449 | `			/* Syntax error */` |
|      ! 0 |  4450 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 |  4451 | `			if( rc == SXERR_ABORT ){` |
|        - |  4452 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4453 | `				return SXERR_ABORT;` |
|        - |  4454 | `			}` |
|      ! 0 |  4455 | `			return SXRET_OK;` |
|        - |  4456 | `		}` |
|    38977 |  4457 | `		RE_SWAP_DELIMITER(pGen);` |
|    38977 |  4458 | `		if( rc == SXERR_ABORT ){` |
|        - |  4459 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4460 | `			return SXERR_ABORT;` |
|    38977 |  4461 | `		}else if( rc != SXERR_EMPTY){` |
|        - |  4462 | `			/* Pop operand lvalue */` |
|    38977 |  4463 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19486 |  4464 | `		}` |
|    19486 |  4465 | `	}` |
|        - |  4466 | `	/* Emit the unconditional jump to the start of the loop */` |
|    38977 |  4467 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - |  4468 | `	/* Fix all jumps now the destination is resolved */` |
|    38977 |  4469 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4470 | `	/* Release the loop block */` |
|    38977 |  4471 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4472 | `	/* Statement successfully compiled */` |
|    38977 |  4473 | `	return SXRET_OK;` |
|    19493 |  4474 | `}` |
|        - |  4475 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - |  4476 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - |  4477 | ` * are allowed.` |
|        - |  4478 | ` */` |
|   241608 |  4479 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  4480 | `{` |
|   241613 |  4481 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   241613 |  4482 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  4483 | `		/* Unexpected expression */` |
|      ! 0 |  4484 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  4485 | `			"foreach: Expecting a variable name");` |
|      ! 0 |  4486 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  4487 | `			rc = SXERR_INVALID;` |
|      ! 0 |  4488 | `		}` |
|      ! 0 |  4489 | `	}` |
|   241613 |  4490 | `	return rc;` |
|        5 |  4491 | `}` |
|        - |  4492 | `/*` |
|        - |  4493 | ` * Compile the 'foreach' statement.` |
|        - |  4494 | ` * According to the PHP language reference` |
|        - |  4495 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - |  4496 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - |  4497 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - |  4498 | ` *  is a minor but useful extension of the first:` |
|        - |  4499 | ` *  foreach (array_expression as $value)` |
|        - |  4500 | ` *    statement` |
|        - |  4501 | ` *  foreach (array_expression as $key => $value)` |
|        - |  4502 | ` *   statement` |
|        - |  4503 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - |  4504 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - |  4505 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - |  4506 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - |  4507 | ` *  to the variable $key on each loop.` |
|        - |  4508 | ` *  Note:` |
|        - |  4509 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - |  4510 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - |  4511 | ` *  Note:` |
|        - |  4512 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - |  4513 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - |  4514 | ` *  or after the foreach without resetting it.` |
|        - |  4515 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - |  4516 | ` *  of copying the value.` |
|        - |  4517 | ` */` |
|   175370 |  4518 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 |  4519 | `{` |
|   175375 |  4520 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   175375 |  4521 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   175375 |  4522 | `	GenBlock *pForeachBlock = 0;` |
|        - |  4523 | `	ph7_foreach_info *pInfo;` |
|        - |  4524 | `	sxu32 nFalseJump;` |
|        - |  4525 | `	VmInstr *pInstr;` |
|        - |  4526 | `	sxu32 nLine;` |
|        - |  4527 | `	sxi32 rc;` |
|   175375 |  4528 | `	nLine = pGen->pIn->nLine;` |
|        - |  4529 | `	/* Jump the 'foreach' keyword */` |
|   175375 |  4530 | `	pGen->pIn++;` |
|   175375 |  4531 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4532 | `		/* Syntax error */` |
|      ! 0 |  4533 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 |  4534 | `		if( rc == SXERR_ABORT ){` |
|        - |  4535 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4536 | `			return SXERR_ABORT;` |
|        - |  4537 | `		}` |
|      ! 0 |  4538 | `		goto Synchronize;` |
|        - |  4539 | `	}` |
|        - |  4540 | `	/* Jump the left parenthesis '(' */` |
|   175375 |  4541 | `	pGen->pIn++;` |
|        - |  4542 | `	/* Create the loop block */` |
|   175375 |  4543 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   175375 |  4544 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4545 | `		return SXERR_ABORT;` |
|        - |  4546 | `	}` |
|        - |  4547 | `	/* Delimit the expression */` |
|   175375 |  4548 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   175375 |  4549 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4550 | `		/* Empty expression */` |
|      ! 0 |  4551 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 |  4552 | `		if( rc == SXERR_ABORT ){` |
|        - |  4553 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4554 | `			return SXERR_ABORT;` |
|        - |  4555 | `		}` |
|        - |  4556 | `		/* Synchronize */` |
|      ! 0 |  4557 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4558 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4559 | `			pGen->pIn++;` |
|      ! 0 |  4560 | `		}` |
|      ! 0 |  4561 | `		return SXRET_OK;` |
|        - |  4562 | `	}` |
|        - |  4563 | `	/* Compile the array expression */` |
|   175375 |  4564 | `	pCur = pGen->pIn;` |
|  1024963 |  4565 | `	while( pCur < pEnd ){` |
|  1024963 |  4566 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   179273 |  4567 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   179273 |  4568 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - |  4569 | `				/* Break with the first 'as' found */` |
|   175375 |  4570 | `				break;` |
|        - |  4571 | `			}` |
|     1949 |  4572 | `		}` |
|        - |  4573 | `		/* Advance the stream cursor */` |
|   849593 |  4574 | `		pCur++;` |
|        5 |  4575 | `	}` |
|   175375 |  4576 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 |  4577 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  4578 | `			"foreach: Missing array/object expression");` |
|      ! 0 |  4579 | `		if( rc == SXERR_ABORT ){` |
|        - |  4580 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4581 | `			return SXERR_ABORT;` |
|        - |  4582 | `		}` |
|      ! 0 |  4583 | `		goto Synchronize;` |
|        - |  4584 | `	}` |
|        - |  4585 | `	/* Swap token streams */` |
|   175375 |  4586 | `	pTmp = pGen->pEnd;` |
|   175375 |  4587 | `	pGen->pEnd = pCur;` |
|   175375 |  4588 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   175375 |  4589 | `	if( rc == SXERR_ABORT ){` |
|        - |  4590 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4591 | `		return SXERR_ABORT;` |
|        - |  4592 | `	}` |
|        - |  4593 | `	/* Update token stream */` |
|   175375 |  4594 | `	while(pGen->pIn < pCur ){` |
|      ! 0 |  4595 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4596 | `		if( rc == SXERR_ABORT ){` |
|        - |  4597 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4598 | `			return SXERR_ABORT;` |
|        - |  4599 | `		}` |
|      ! 0 |  4600 | `		pGen->pIn++;` |
|      ! 0 |  4601 | `	}` |
|   175375 |  4602 | `	pCur++; /* Jump the 'as' keyword */` |
|   175375 |  4603 | `	pGen->pIn = pCur;` |
|   175375 |  4604 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4605 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 |  4606 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4607 | `			return SXERR_ABORT;` |
|        - |  4608 | `		}` |
|      ! 0 |  4609 | `	}` |
|        - |  4610 | `	/* Create the foreach context */` |
|   175375 |  4611 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   175375 |  4612 | `	if( pInfo == 0 ){` |
|      ! 0 |  4613 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  4614 | `		return SXERR_ABORT;` |
|        - |  4615 | `	}` |
|        - |  4616 | `	/* Zero the structure */` |
|   175375 |  4617 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - |  4618 | `	/* Initialize structure fields */` |
|   175375 |  4619 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - |  4620 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - |  4621 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - |  4622 | `	 * '=>'. */` |
|   175375 |  4623 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   175375 |  4624 | `	if( pCur < pEnd ){` |
|        - |  4625 | `		/* Compile the expression holding the key name */` |
|    66263 |  4626 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 |  4627 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 |  4628 | `			if( rc == SXERR_ABORT ){` |
|        - |  4629 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4630 | `				return SXERR_ABORT;` |
|        - |  4631 | `			}` |
|      ! 0 |  4632 | `		}else{` |
|    66263 |  4633 | `			pGen->pEnd = pCur;` |
|    66263 |  4634 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    66263 |  4635 | `			if( rc == SXERR_ABORT ){` |
|        - |  4636 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4637 | `				return SXERR_ABORT;` |
|        - |  4638 | `			}` |
|    66263 |  4639 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    66263 |  4640 | `			if( pInstr->p3 ){` |
|        - |  4641 | `				/* Record key name */` |
|    66263 |  4642 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    33129 |  4643 | `			}` |
|    66263 |  4644 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - |  4645 | `		}` |
|    66263 |  4646 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    33129 |  4647 | `	}` |
|   175375 |  4648 | `	pGen->pEnd = pEnd;` |
|   175375 |  4649 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4650 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 |  4651 | `		if( rc == SXERR_ABORT ){` |
|        - |  4652 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4653 | `			return SXERR_ABORT;` |
|        - |  4654 | `		}` |
|      ! 0 |  4655 | `		goto Synchronize;` |
|        - |  4656 | `	}` |
|   175375 |  4657 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       31 |  4658 | `		pGen->pIn++;` |
|        - |  4659 | `		/* Pass by reference  */` |
|       31 |  4660 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       14 |  4661 | `	}` |
|        - |  4662 | `	/* Check if the value target is list() */` |
|   175375 |  4663 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 |  4664 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - |  4665 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - |  4666 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - |  4667 | `		 */` |
|        - |  4668 | `		static int iForeachListCnt = 0;` |
|        - |  4669 | `		char zTmp[128];` |
|        - |  4670 | `		sxu32 nLen;` |
|        - |  4671 | `		char *zDup;` |
|       10 |  4672 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 |  4673 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 |  4674 | `		if( zDup == 0 ){` |
|      ! 0 |  4675 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4676 | `			return SXERR_ABORT;` |
|        - |  4677 | `		}` |
|       10 |  4678 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4679 | `		/* Save list() token boundaries */` |
|       10 |  4680 | `		pListStart = pGen->pIn;` |
|        - |  4681 | `		/* Advance past list(...) — validate parentheses */` |
|       10 |  4682 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 |  4683 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  4684 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  4685 | `				"foreach: Expected '(' after 'list'");` |
|        3 |  4686 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4687 | `				return SXERR_ABORT;` |
|        - |  4688 | `			}` |
|        3 |  4689 | `			goto Synchronize;` |
|        - |  4690 | `		}` |
|        7 |  4691 | `		pGen->pIn++; /* Jump '(' */` |
|        7 |  4692 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 |  4693 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4694 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4695 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 |  4696 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4697 | `				return SXERR_ABORT;` |
|        - |  4698 | `			}` |
|      ! 0 |  4699 | `			goto Synchronize;` |
|        - |  4700 | `		}` |
|        7 |  4701 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 |  4702 | `		pListEnd = pGen->pIn;` |
|        7 |  4703 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   175370 |  4704 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - |  4705 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - |  4706 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - |  4707 | `		 */` |
|        - |  4708 | `		static int iForeachShortListCnt = 0;` |
|        - |  4709 | `		char zTmp[128];` |
|        - |  4710 | `		sxu32 nLen;` |
|        - |  4711 | `		char *zDup;` |
|       13 |  4712 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       13 |  4713 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       13 |  4714 | `		if( zDup == 0 ){` |
|      ! 0 |  4715 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4716 | `			return SXERR_ABORT;` |
|        - |  4717 | `		}` |
|       13 |  4718 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4719 | `		/* Save [...] token boundaries */` |
|       13 |  4720 | `		pListStart = pGen->pIn;` |
|        - |  4721 | `		/* Advance past [...] */` |
|       13 |  4722 | `		pGen->pIn++; /* Jump '[' */` |
|       13 |  4723 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       13 |  4724 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4725 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4726 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 |  4727 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4728 | `				return SXERR_ABORT;` |
|        - |  4729 | `			}` |
|      ! 0 |  4730 | `			goto Synchronize;` |
|        - |  4731 | `		}` |
|       13 |  4732 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       13 |  4733 | `		pListEnd = pGen->pIn;` |
|       13 |  4734 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        7 |  4735 | `	}else{` |
|        - |  4736 | `		/* Compile the expression holding the value name */` |
|   175355 |  4737 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   175355 |  4738 | `		if( rc == SXERR_ABORT ){` |
|        - |  4739 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4740 | `			return SXERR_ABORT;` |
|        - |  4741 | `		}` |
|   175355 |  4742 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   175355 |  4743 | `		if( pInstr->p3 ){` |
|        - |  4744 | `			/* Record value name */` |
|   175355 |  4745 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    87675 |  4746 | `		}` |
|        - |  4747 | `	}` |
|        - |  4748 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   175373 |  4749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - |  4750 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175373 |  4751 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - |  4752 | `	/* Record the first instruction to execute */` |
|   175373 |  4753 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4754 | `	/* Emit the FOREACH_STEP instruction */` |
|   175373 |  4755 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - |  4756 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175373 |  4757 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - |  4758 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   175373 |  4759 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - |  4760 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - |  4761 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - |  4762 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - |  4763 | `		 */` |
|       19 |  4764 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - |  4765 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - |  4766 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - |  4767 | `		 * picks up the delimiter and the variable names inside.` |
|        - |  4768 | `		 */` |
|       19 |  4769 | `		pSavedIn = pGen->pIn;` |
|       19 |  4770 | `		pSavedEnd = pGen->pEnd;` |
|       19 |  4771 | `		pGen->pIn = pListStart;` |
|       19 |  4772 | `		pGen->pEnd = pListEnd;` |
|       19 |  4773 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       13 |  4774 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 |  4775 | `		}else{` |
|        7 |  4776 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - |  4777 | `		}` |
|       19 |  4778 | `		pGen->pIn = pSavedIn;` |
|       19 |  4779 | `		pGen->pEnd = pSavedEnd;` |
|       19 |  4780 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4781 | `			return SXERR_ABORT;` |
|        - |  4782 | `		}` |
|        - |  4783 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       19 |  4784 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        9 |  4785 | `	}` |
|        - |  4786 | `	/* Compile the loop body */` |
|   175373 |  4787 | `	pGen->pIn = &pEnd[1];` |
|   175373 |  4788 | `	pGen->pEnd = pTmp;` |
|   175373 |  4789 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   175373 |  4790 | `	if( rc == SXERR_ABORT ){` |
|        - |  4791 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4792 | `		return SXERR_ABORT;` |
|        - |  4793 | `	}` |
|        - |  4794 | `	/* Emit the unconditional jump to the start of the loop */` |
|   175373 |  4795 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - |  4796 | `	/* Fix all jumps now the destination is resolved */` |
|   175373 |  4797 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4798 | `	/* Release the loop block */` |
|   175373 |  4799 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4800 | `	/* Statement successfully compiled */` |
|   175373 |  4801 | `	return SXRET_OK;` |
|        1 |  4802 | `Synchronize:` |
|        - |  4803 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4804 | `	 * compiling this erroneous block.` |
|        - |  4805 | `	 */` |
|        3 |  4806 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4807 | `		pGen->pIn++;` |
|      ! 0 |  4808 | `	}` |
|        3 |  4809 | `	return SXRET_OK;` |
|    87690 |  4810 | `}` |
|        - |  4811 | `/*` |
|        - |  4812 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - |  4813 | ` * According to the PHP language reference` |
|        - |  4814 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - |  4815 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - |  4816 | ` *  that is similar to that of C:` |
|        - |  4817 | ` *  if (expr)` |
|        - |  4818 | ` *   statement` |
|        - |  4819 | ` *  else construct:` |
|        - |  4820 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - |  4821 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - |  4822 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - |  4823 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - |  4824 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - |  4825 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - |  4826 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - |  4827 | ` *  elseif` |
|        - |  4828 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - |  4829 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - |  4830 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - |  4831 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - |  4832 | ` *   than b, a equal to b or a is smaller than b:` |
|        - |  4833 | ` *   <?php` |
|        - |  4834 | ` *    if ($a > $b) {` |
|        - |  4835 | ` *     echo "a is bigger than b";` |
|        - |  4836 | ` *    } elseif ($a == $b) {` |
|        - |  4837 | ` *     echo "a is equal to b";` |
|        - |  4838 | ` *    } else {` |
|        - |  4839 | ` *     echo "a is smaller than b";` |
|        - |  4840 | ` *    }` |
|        - |  4841 | ` *    ?>` |
|        - |  4842 | ` */` |
|  1179458 |  4843 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 |  4844 | `{` |
|  1179463 |  4845 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  1179463 |  4846 | `	GenBlock *pCondBlock = 0;` |
|        - |  4847 | `	sxu32 nJumpIdx;` |
|        - |  4848 | `	sxu32 nKeyID;` |
|        - |  4849 | `	sxi32 rc;` |
|        - |  4850 | `	/* Jump the 'if' keyword */` |
|  1179463 |  4851 | `	pGen->pIn++;` |
|  1179463 |  4852 | `	pToken = pGen->pIn;` |
|        - |  4853 | `	/* Create the conditional block */` |
|  1179463 |  4854 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  1179463 |  4855 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4856 | `		return SXERR_ABORT;` |
|        - |  4857 | `	}` |
|        - |  4858 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   636396 |  4859 | `	for(;;){` |
|  1272797 |  4860 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4861 | `			/* Syntax error */` |
|      ! 0 |  4862 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4863 | `				pToken--;` |
|      ! 0 |  4864 | `			}` |
|      ! 0 |  4865 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 |  4866 | `			if( rc == SXERR_ABORT ){` |
|        - |  4867 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4868 | `				return SXERR_ABORT;` |
|        - |  4869 | `			}` |
|      ! 0 |  4870 | `			goto Synchronize;` |
|        - |  4871 | `		}` |
|        - |  4872 | `		/* Jump the left parenthesis '(' */` |
|  1272797 |  4873 | `		pToken++;` |
|        - |  4874 | `		/* Delimit the condition */` |
|  1272797 |  4875 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1272797 |  4876 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - |  4877 | `			/* Syntax error */` |
|      ! 0 |  4878 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4879 | `				pToken--;` |
|      ! 0 |  4880 | `			}` |
|      ! 0 |  4881 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 |  4882 | `			if( rc == SXERR_ABORT ){` |
|        - |  4883 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4884 | `				return SXERR_ABORT;` |
|        - |  4885 | `			}` |
|      ! 0 |  4886 | `			goto Synchronize;` |
|        - |  4887 | `		}` |
|        - |  4888 | `		/* Swap token streams */` |
|  1272797 |  4889 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - |  4890 | `		/* Compile the condition */` |
|  1272797 |  4891 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4892 | `		/* Update token stream */` |
|  1272797 |  4893 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 |  4894 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4895 | `			pGen->pIn++;` |
|      ! 0 |  4896 | `		}` |
|  1272797 |  4897 | `		pGen->pIn  = &pEnd[1];` |
|  1272797 |  4898 | `		pGen->pEnd = pTmp;` |
|  1272797 |  4899 | `		if( rc == SXERR_ABORT ){` |
|        - |  4900 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4901 | `			return SXERR_ABORT;` |
|        - |  4902 | `		}` |
|        - |  4903 | `		/* Emit the false jump */` |
|  1272797 |  4904 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - |  4905 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  1272797 |  4906 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - |  4907 | `		/* Compile the body */` |
|  1272797 |  4908 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  1272797 |  4909 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4910 | `			return SXERR_ABORT;` |
|        - |  4911 | `		}` |
|  1272797 |  4912 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   239764 |  4913 | `			break;` |
|        - |  4914 | `		}` |
|        - |  4915 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   793279 |  4916 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   793279 |  4917 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   614025 |  4918 | `			break;` |
|        - |  4919 | `		}` |
|        - |  4920 | `		/* Emit the unconditional jump */` |
|   179259 |  4921 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - |  4922 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   179259 |  4923 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   179259 |  4924 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   171373 |  4925 | `			pToken = &pGen->pIn[1];` |
|   171373 |  4926 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85486 |  4927 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    42965 |  4928 | `					break;` |
|        - |  4929 | `			}` |
|    85453 |  4930 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42724 |  4931 | `		}` |
|    93339 |  4932 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - |  4933 | `		/* Synchronize cursors */` |
|    93339 |  4934 | `		pToken = pGen->pIn;` |
|        - |  4935 | `		/* Fix the false jump */` |
|    93339 |  4936 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 |  4937 | `	} /* For(;;) */` |
|        - |  4938 | `	/* Fix the false jump */` |
|  1179463 |  4939 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  1179463 |  4940 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   699940 |  4941 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - |  4942 | `			/* Compile the else block */` |
|    85925 |  4943 | `			pGen->pIn++;` |
|    85925 |  4944 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    85925 |  4945 | `			if( rc == SXERR_ABORT ){` |
|        - |  4946 |  |
|      ! 0 |  4947 | `				return SXERR_ABORT;` |
|        - |  4948 | `			}` |
|    42960 |  4949 | `	}` |
|  1179463 |  4950 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4951 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  1179463 |  4952 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - |  4953 | `	/* Release the conditional block */` |
|  1179463 |  4954 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4955 | `	/* Statement successfully compiled */` |
|  1179463 |  4956 | `	return SXRET_OK;` |
|      ! 0 |  4957 | `Synchronize:` |
|        - |  4958 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - |  4959 | `	 */` |
|      ! 0 |  4960 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4961 | `		pGen->pIn++;` |
|      ! 0 |  4962 | `	}` |
|      ! 0 |  4963 | `	return SXRET_OK;` |
|   589734 |  4964 | `}` |
|        - |  4965 | `/*` |
|        - |  4966 | ` * Compile the global construct.` |
|        - |  4967 | ` * According to the PHP language reference` |
|        - |  4968 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - |  4969 | ` *  to be used in that function.` |
|        - |  4970 | ` *  Example #1 Using global` |
|        - |  4971 | ` *  <?php` |
|        - |  4972 | ` *   $a = 1;` |
|        - |  4973 | ` *   $b = 2;` |
|        - |  4974 | ` *   function Sum()` |
|        - |  4975 | ` *   {` |
|        - |  4976 | ` *    global $a, $b;` |
|        - |  4977 | ` *    $b = $a + $b;` |
|        - |  4978 | ` *   }` |
|        - |  4979 | ` *   Sum();` |
|        - |  4980 | ` *   echo $b;` |
|        - |  4981 | ` *  ?>` |
|        - |  4982 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - |  4983 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - |  4984 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - |  4985 | ` */` |
|       36 |  4986 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 |  4987 | `{` |
|       41 |  4988 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  4989 | `	sxi32 nExpr;` |
|        - |  4990 | `	sxi32 rc;` |
|        - |  4991 | `	/* Jump the 'global' keyword */` |
|       41 |  4992 | `	pGen->pIn++;` |
|       41 |  4993 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - |  4994 | `		/* Nothing to process */` |
|      ! 0 |  4995 | `		return SXRET_OK;` |
|        - |  4996 | `	}` |
|       41 |  4997 | `	pTmp = pGen->pEnd;` |
|       41 |  4998 | `	nExpr = 0;` |
|       87 |  4999 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 |  5000 | `		if( pGen->pIn < pNext ){` |
|       51 |  5001 | `			pGen->pEnd = pNext;` |
|       51 |  5002 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5003 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 |  5004 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  5005 | `					return SXERR_ABORT;` |
|        - |  5006 | `				}` |
|      ! 0 |  5007 | `			}else{` |
|       51 |  5008 | `				pGen->pIn++;` |
|       51 |  5009 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5010 | `					/* Emit a warning */` |
|      ! 0 |  5011 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 |  5012 | `				}else{` |
|       51 |  5013 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 |  5014 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  5015 | `						return SXERR_ABORT;` |
|       51 |  5016 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 |  5017 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 |  5018 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - |  5019 | `							/* Variable name, not a constant */` |
|       51 |  5020 | `							pLast->iP1 = 0;` |
|       23 |  5021 | `						}` |
|       51 |  5022 | `						nExpr++;` |
|       23 |  5023 | `					}` |
|        - |  5024 | `				}` |
|        - |  5025 | `			}` |
|       23 |  5026 | `		}` |
|        - |  5027 | `		/* Next expression in the stream */` |
|       51 |  5028 | `		pGen->pIn = pNext;` |
|        - |  5029 | `		/* Jump trailing commas */` |
|       61 |  5030 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 |  5031 | `			pGen->pIn++;` |
|        5 |  5032 | `		}` |
|        5 |  5033 | `	}` |
|        - |  5034 | `	/* Restore token stream */` |
|       41 |  5035 | `	pGen->pEnd = pTmp;` |
|       41 |  5036 | `	if( nExpr > 0 ){` |
|        - |  5037 | `		/* Emit the uplink instruction */` |
|       41 |  5038 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 |  5039 | `	}` |
|       41 |  5040 | `	return SXRET_OK;` |
|       23 |  5041 | `}` |
|        - |  5042 | `/*` |
|        - |  5043 | ` * Compile the return statement.` |
|        - |  5044 | ` * According to the PHP language reference` |
|        - |  5045 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - |  5046 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - |  5047 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - |  5048 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - |  5049 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - |  5050 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - |  5051 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - |  5052 | ` *  from within the main script file, then script execution end.` |
|        - |  5053 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - |  5054 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - |  5055 | ` *  should do so as PHP has less work to do in this case.` |
|        - |  5056 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - |  5057 | ` */` |
|  1621484 |  5058 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 |  5059 | `{` |
|  1621489 |  5060 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - |  5061 | `	sxi32 rc;` |
|  1621489 |  5062 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1621489 |  5063 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - |  5064 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - |  5065 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - |  5066 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - |  5067 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - |  5068 | `	 * normally below so token processing stays consistent. */` |
|  4222539 |  5069 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  2601055 |  5070 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 |  5071 | `	}` |
|  1621484 |  5072 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  1621457 |  5073 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 |  5074 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  5075 | `			"A never-returning function must not return");` |
|        3 |  5076 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5077 | `			return SXERR_ABORT;` |
|        - |  5078 | `		}` |
|        1 |  5079 | `	}` |
|        - |  5080 | `	/* Jump the 'return' keyword */` |
|  1621489 |  5081 | `	pGen->pIn++;` |
|  1621489 |  5082 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5083 | `		/* Compile the expression */` |
|  1605923 |  5084 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  1605923 |  5085 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5086 | `			return SXERR_ABORT;` |
|  1605923 |  5087 | `		}else if(rc != SXERR_EMPTY ){` |
|  1605923 |  5088 | `			nRet = 1;` |
|   802959 |  5089 | `		}` |
|   802959 |  5090 | `	}` |
|        - |  5091 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - |  5092 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - |  5093 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - |  5094 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  1621489 |  5095 | `	if( pGen->bInGenerator ){` |
|       32 |  5096 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|       32 |  5097 | `		return SXRET_OK;` |
|        - |  5098 | `	}` |
|        - |  5099 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - |  5100 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - |  5101 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - |  5102 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - |  5103 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  1621461 |  5104 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  1621461 |  5105 | `	return SXRET_OK;` |
|   810747 |  5106 | `}` |
|        - |  5107 | `/*` |
|        - |  5108 | ` * Compile a yield expression.` |
|        - |  5109 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - |  5110 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - |  5111 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - |  5112 | ` */` |
|      378 |  5113 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 |  5114 | `{` |
|        - |  5115 | `	SyToken *pTmp, *pSplit;` |
|      383 |  5116 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      383 |  5117 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - |  5118 | `	sxi32 rc;` |
|      189 |  5119 | `	(void)iCompileFlag;` |
|        - |  5120 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      383 |  5121 | `	pGen->pIn++;` |
|        - |  5122 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - |  5123 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - |  5124 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - |  5125 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - |  5126 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|      378 |  5127 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      224 |  5128 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 |  5129 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 |  5130 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 |  5131 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 |  5132 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5133 | `			return SXERR_ABORT;` |
|        - |  5134 | `		}` |
|       67 |  5135 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  5136 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 |  5137 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - |  5138 | `				"Missing expression after 'yield from'");` |
|      ! 0 |  5139 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5140 | `				return SXERR_ABORT;` |
|        - |  5141 | `			}` |
|      ! 0 |  5142 | `		}` |
|       67 |  5143 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 |  5144 | `		return SXRET_OK;` |
|        - |  5145 | `	}` |
|      321 |  5146 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5147 | `		/* Bare yield — no value */` |
|        3 |  5148 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 |  5149 | `		return SXRET_OK;` |
|        - |  5150 | `	}` |
|        - |  5151 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      319 |  5152 | `	pSplit = 0;` |
|        - |  5153 | `	{` |
|      319 |  5154 | `		SyToken *pCur = pGen->pIn;` |
|      319 |  5155 | `		sxi32 nNest = 0;` |
|      761 |  5156 | `		while( pCur < pGen->pEnd ){` |
|      461 |  5157 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 |  5158 | `				nNest++;` |
|      453 |  5159 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 |  5160 | `				nNest--;` |
|      437 |  5161 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       16 |  5162 | `				pSplit = pCur;` |
|       16 |  5163 | `				break;` |
|        - |  5164 | `			}` |
|      447 |  5165 | `			pCur++;` |
|        5 |  5166 | `		}` |
|        - |  5167 | `	}` |
|      319 |  5168 | `	pTmp = pGen->pEnd;` |
|      319 |  5169 | `	if( pSplit ){` |
|        - |  5170 | `		/* yield $key => $value */` |
|       16 |  5171 | `		pGen->pEnd = pSplit;` |
|       16 |  5172 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5173 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5174 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       16 |  5175 | `		pGen->pEnd = pTmp;` |
|       16 |  5176 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5177 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5178 | `		iP1 = 1;` |
|       16 |  5179 | `		iP2 = 1;` |
|        9 |  5180 | `	}else{` |
|        - |  5181 | `		/* yield $value */` |
|      305 |  5182 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      305 |  5183 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      305 |  5184 | `		if( rc != SXERR_EMPTY ){` |
|      305 |  5185 | `			iP1 = 1;` |
|      150 |  5186 | `		}` |
|        - |  5187 | `	}` |
|      319 |  5188 | `	pGen->pEnd = pTmp;` |
|      319 |  5189 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      319 |  5190 | `	return SXRET_OK;` |
|      194 |  5191 | `}` |
|        - |  5192 | `/*` |
|        - |  5193 | ` * Compile the die/exit language construct.` |
|        - |  5194 | ` * The role of these constructs is to terminate execution of the script.` |
|        - |  5195 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - |  5196 | ` */` |
|      122 |  5197 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 |  5198 | `{` |
|      127 |  5199 | `	sxi32 nExpr = 0;` |
|        - |  5200 | `	sxi32 rc;` |
|        - |  5201 | `	/* Jump the die/exit keyword */` |
|      127 |  5202 | `	pGen->pIn++;` |
|      127 |  5203 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5204 | `		/* Compile the expression */` |
|      127 |  5205 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      127 |  5206 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5207 | `			return SXERR_ABORT;` |
|      127 |  5208 | `		}else if(rc != SXERR_EMPTY ){` |
|      127 |  5209 | `			nExpr = 1;` |
|       61 |  5210 | `		}` |
|       61 |  5211 | `	}` |
|        - |  5212 | `	/* Emit the HALT instruction */` |
|      127 |  5213 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      127 |  5214 | `	return SXRET_OK;` |
|       66 |  5215 | `}` |
|        - |  5216 | `/*` |
|        - |  5217 | ` * Compile the 'echo' language construct.` |
|        - |  5218 | ` */` |
|    16916 |  5219 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 |  5220 | `{` |
|    16921 |  5221 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5222 | `	sxi32 rc;` |
|        - |  5223 | `	/* Jump the 'echo' keyword */` |
|    16921 |  5224 | `	pGen->pIn++;` |
|        - |  5225 | `	/* Compile arguments one after one */` |
|    16921 |  5226 | `	pTmp = pGen->pEnd;` |
|    41129 |  5227 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    24213 |  5228 | `		if( pGen->pIn < pNext ){` |
|    24213 |  5229 | `			pGen->pEnd = pNext;` |
|    24213 |  5230 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    24213 |  5231 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5232 | `				return SXERR_ABORT;` |
|    24213 |  5233 | `			}else if( rc != SXERR_EMPTY ){` |
|        - |  5234 | `				/* Emit the consume instruction */` |
|    24189 |  5235 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    12092 |  5236 | `			}` |
|    12104 |  5237 | `		}` |
|        - |  5238 | `		/* Jump trailing commas */` |
|    31505 |  5239 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     7297 |  5240 | `			pNext++;` |
|        5 |  5241 | `		}` |
|    24213 |  5242 | `		pGen->pIn = pNext;` |
|        5 |  5243 | `	}` |
|        - |  5244 | `	/* Restore token stream */` |
|    16921 |  5245 | `	pGen->pEnd = pTmp;` |
|    16921 |  5246 | `	return SXRET_OK;` |
|     8463 |  5247 | `}` |
|        - |  5248 | `/*` |
|        - |  5249 | ` * Compile the static statement.` |
|        - |  5250 | ` * According to the PHP language reference` |
|        - |  5251 | ` *  Another important feature of variable scoping is the static variable.` |
|        - |  5252 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - |  5253 | ` *  when program execution leaves this scope.` |
|        - |  5254 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - |  5255 | ` * Symisc eXtension.` |
|        - |  5256 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - |  5257 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  5258 | ` *  Example` |
|        - |  5259 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  5260 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  5261 | ` */` |
|       10 |  5262 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        3 |  5263 | `{` |
|        - |  5264 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - |  5265 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - |  5266 | `	GenBlock *pBlock;` |
|        - |  5267 | `	SyString *pName;` |
|        - |  5268 | `	char *zDup;` |
|        - |  5269 | `	sxu32 nLine;` |
|        - |  5270 | `	sxi32 rc;` |
|        - |  5271 | `	/* Jump the static keyword */` |
|       13 |  5272 | `	nLine = pGen->pIn->nLine;` |
|       13 |  5273 | `	pGen->pIn++;` |
|        - |  5274 | `	/* Extract the enclosing function if any */` |
|       13 |  5275 | `	pBlock = pGen->pCurrent;` |
|       23 |  5276 | `	while( pBlock ){` |
|       23 |  5277 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       13 |  5278 | `			break;` |
|        - |  5279 | `		}` |
|        - |  5280 | `		/* Point to the upper block */` |
|       13 |  5281 | `		pBlock = pBlock->pParent;` |
|        3 |  5282 | `	}` |
|       13 |  5283 | `	if( pBlock == 0 ){` |
|        - |  5284 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 |  5285 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5286 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|      ! 0 |  5287 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5288 | `				return SXERR_ABORT;` |
|        - |  5289 | `			}` |
|      ! 0 |  5290 | `			goto Synchronize;` |
|        - |  5291 | `		}` |
|        - |  5292 | `		/* Compile the expression holding the variable */` |
|      ! 0 |  5293 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  5294 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5295 | `			return SXERR_ABORT;` |
|      ! 0 |  5296 | `		}else if( rc != SXERR_EMPTY ){` |
|        - |  5297 | `			/* Emit the POP instruction */` |
|      ! 0 |  5298 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  5299 | `		}` |
|      ! 0 |  5300 | `		return SXRET_OK;` |
|        - |  5301 | `	}` |
|       13 |  5302 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - |  5303 | `	/* Make sure we are dealing with a valid statement */` |
|       13 |  5304 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        8 |  5305 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        3 |  5306 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|        3 |  5307 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5308 | `				return SXERR_ABORT;` |
|        - |  5309 | `			}` |
|        3 |  5310 | `			goto Synchronize;` |
|        - |  5311 | `	}` |
|       10 |  5312 | `	pGen->pIn++;` |
|        - |  5313 | `	/* Extract variable name */` |
|       10 |  5314 | `	pName = &pGen->pIn->sData;` |
|       10 |  5315 | `	pGen->pIn++; /* Jump the var name */` |
|       10 |  5316 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 |  5317 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  5318 | `		goto Synchronize;` |
|        - |  5319 | `	}` |
|        - |  5320 | `	/* Initialize the structure describing the static variable */` |
|       10 |  5321 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       10 |  5322 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - |  5323 | `	/* Duplicate variable name */` |
|       10 |  5324 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       10 |  5325 | `	if( zDup == 0 ){` |
|      ! 0 |  5326 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  5327 | `		return SXERR_ABORT;` |
|        - |  5328 | `	}` |
|       10 |  5329 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - |  5330 | `	/* Check if we have an expression to compile */` |
|       10 |  5331 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - |  5332 | `		SySet *pInstrContainer;` |
|        - |  5333 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - |  5334 | `		 * Static variable can take any complex expression including function` |
|        - |  5335 | `		 * call as their initialization value.` |
|        - |  5336 | `		 * Example:` |
|        - |  5337 | `		 *		static $var = foo(1,4+5,bar());` |
|        - |  5338 | `		 */` |
|       10 |  5339 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - |  5340 | `		/* Swap bytecode container */` |
|       10 |  5341 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       10 |  5342 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - |  5343 | `		/* Compile the expression */` |
|       10 |  5344 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  5345 | `		/* Emit the done instruction */` |
|       10 |  5346 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - |  5347 | `		/* Restore default bytecode container */` |
|       10 |  5348 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        4 |  5349 | `	}` |
|        - |  5350 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       10 |  5351 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       10 |  5352 | `	return SXRET_OK;` |
|        1 |  5353 | `Synchronize:` |
|        - |  5354 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - |  5355 | `	 * statement.` |
|        - |  5356 | `	 */` |
|        5 |  5357 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 |  5358 | `		pGen->pIn++;` |
|        1 |  5359 | `	}` |
|        3 |  5360 | `	return SXRET_OK;` |
|        8 |  5361 | `}` |
|        - |  5362 | `/*` |
|        - |  5363 | ` * Compile the var statement.` |
|        - |  5364 | ` * Symisc Extension:` |
|        - |  5365 | ` *      var statement can be used outside of a class definition.` |
|        - |  5366 | ` */` |
|        4 |  5367 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 |  5368 | `{` |
|        - |  5369 | `	sxu32 nLine;` |
|        - |  5370 | `	sxi32 rc;` |
|        5 |  5371 | `	nLine = pGen->pIn->nLine;` |
|        - |  5372 | `	/* Jump the 'var' keyword */` |
|        5 |  5373 | `	pGen->pIn++;` |
|        5 |  5374 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  5375 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|        - |  5376 | `		/* Synchronize with the first semi-colon */` |
|      ! 0 |  5377 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|      ! 0 |  5378 | `			pGen->pIn++;` |
|      ! 0 |  5379 | `		}` |
|      ! 0 |  5380 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5381 | `			return SXERR_ABORT;` |
|        - |  5382 | `		}` |
|      ! 0 |  5383 | `	}else{` |
|        - |  5384 | `		/* Compile the expression */` |
|        5 |  5385 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        5 |  5386 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5387 | `			return SXERR_ABORT;` |
|        5 |  5388 | `		}else if( rc != SXERR_EMPTY ){` |
|        5 |  5389 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 |  5390 | `		}` |
|        - |  5391 | `	}` |
|        5 |  5392 | `	return SXRET_OK;` |
|        3 |  5393 | `}` |
|        - |  5394 | `/*` |
|        - |  5395 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - |  5396 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - |  5397 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - |  5398 | ` */` |
|        - |  5399 | `/*` |
|        - |  5400 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - |  5401 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - |  5402 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - |  5403 | ` * qualified name and updates the instruction's operand index.` |
|        - |  5404 | ` *` |
|        - |  5405 | ` * Resolution order:` |
|        - |  5406 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - |  5407 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - |  5408 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - |  5409 | ` *` |
|        - |  5410 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - |  5411 | ` * came from an import (step 1) and 0 otherwise.` |
|        - |  5412 | ` * Returns the (possibly new) literal index.` |
|        - |  5413 | ` */` |
|  2876894 |  5414 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 |  5415 | `{` |
|        - |  5416 | `	ph7_value *pLit;` |
|        - |  5417 | `	const char *zLit;` |
|        - |  5418 | `	SyString sQualified;` |
|        - |  5419 | `	sxu32 nLit;` |
|        - |  5420 | `	sxu32 k;` |
|        - |  5421 | `	sxu32 nNewIdx;` |
|        - |  5422 | `	int hasNsSep;` |
|        - |  5423 | `	SyHashEntry *pImport;` |
|        - |  5424 | `	ph7_value *pNew;` |
|  2876899 |  5425 | `	if( pFromImport ){` |
|  2345793 |  5426 | `		*pFromImport = 0;` |
|  1172894 |  5427 | `	}` |
|  2876899 |  5428 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  2876899 |  5429 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 |  5430 | `		return nOrigIdx;` |
|        - |  5431 | `	}` |
|  2876899 |  5432 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  2876899 |  5433 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - |  5434 | `	/* Skip if already qualified (contains backslash) */` |
|  2876899 |  5435 | `	hasNsSep = 0;` |
| 37066497 |  5436 | `	for( k = 0; k < nLit; k++ ){` |
| 34189611 |  5437 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 17094804 |  5438 | `	}` |
|  2876899 |  5439 | `	if( hasNsSep ){` |
|       10 |  5440 | `		return nOrigIdx;` |
|        - |  5441 | `	}` |
|        - |  5442 | `	/* Check use imports first (works even outside namespaces) */` |
|  2876891 |  5443 | `	SyBlobReset(&pGen->sWorker);` |
|  2876891 |  5444 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  2876891 |  5445 | `	if( pImport ){` |
|       41 |  5446 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 |  5447 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 |  5448 | `		if( pFromImport ){` |
|       18 |  5449 | `			*pFromImport = 1;` |
|        8 |  5450 | `		}` |
|       23 |  5451 | `	}else{` |
|  2876855 |  5452 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  2876765 |  5453 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - |  5454 | `		}` |
|        - |  5455 | `		/* Prepend current namespace */` |
|       95 |  5456 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       95 |  5457 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       95 |  5458 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - |  5459 | `	}` |
|        - |  5460 | `	/* Look up or create a new literal for the qualified name */` |
|      131 |  5461 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      131 |  5462 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       57 |  5463 | `		return nNewIdx; /* Already interned */` |
|        - |  5464 | `	}` |
|       79 |  5465 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|       79 |  5466 | `	if( pNew == 0 ){` |
|      ! 0 |  5467 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - |  5468 | `	}` |
|       79 |  5469 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|       79 |  5470 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|       79 |  5471 | `	return nNewIdx;` |
|  1438452 |  5472 | `}` |
|        - |  5473 | `/*` |
|        - |  5474 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - |  5475 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - |  5476 | ` */` |
|   187730 |  5477 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5478 | `{` |
|        - |  5479 | `	SyHashEntry *pImport;` |
|        - |  5480 | `	/* Check use imports first */` |
|   187735 |  5481 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   187735 |  5482 | `	if( pImport ){` |
|       19 |  5483 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       19 |  5484 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       19 |  5485 | `		return;` |
|        - |  5486 | `	}` |
|        - |  5487 | `	/* Prepend current namespace if active */` |
|   187719 |  5488 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        8 |  5489 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  5490 | `		SyBlobAppend(pOut,"\\",1);` |
|        3 |  5491 | `	}` |
|   187719 |  5492 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    93870 |  5493 | `}` |
|        - |  5494 | `/*` |
|        - |  5495 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - |  5496 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - |  5497 | ` * The caller must release pOut when done.` |
|        - |  5498 | ` */` |
|   261966 |  5499 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5500 | `{` |
|   261971 |  5501 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3947 |  5502 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3947 |  5503 | `		SyBlobAppend(pOut,"\\",1);` |
|     1971 |  5504 | `	}` |
|   261971 |  5505 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   261971 |  5506 | `}` |
|        - |  5507 | `/*` |
|        - |  5508 | ` * Compile a namespace statement` |
|        - |  5509 | ` * According to the PHP language reference manual` |
|        - |  5510 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - |  5511 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - |  5512 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - |  5513 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - |  5514 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - |  5515 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - |  5516 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - |  5517 | ` *  programming world.` |
|        - |  5518 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - |  5519 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - |  5520 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - |  5521 | ` *  classes/functions/constants.` |
|        - |  5522 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - |  5523 | ` *  readability of source code.` |
|        - |  5524 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - |  5525 | ` *  Here is an example of namespace syntax in PHP:` |
|        - |  5526 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - |  5527 | ` *       class MyClass {}` |
|        - |  5528 | ` *       function myfunction() {}` |
|        - |  5529 | ` *       const MYCONST = 1;` |
|        - |  5530 | ` *       $a = new MyClass;` |
|        - |  5531 | ` *       $c = new \my\name\MyClass;` |
|        - |  5532 | ` *       $a = strlen('hi');` |
|        - |  5533 | ` *       $d = namespace\MYCONST;` |
|        - |  5534 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - |  5535 | ` *       echo constant($d);` |
|        - |  5536 | ` * NOTE` |
|        - |  5537 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5538 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5539 | ` */` |
|        - |  5540 | `/*` |
|        - |  5541 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - |  5542 | ` */` |
|       14 |  5543 | `static const char * TokenTypeName(sxu32 nType)` |
|        3 |  5544 | `{` |
|       17 |  5545 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 |  5546 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 |  5547 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 |  5548 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 |  5549 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 |  5550 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 |  5551 | `	return "token";` |
|       10 |  5552 | `}` |
|     3990 |  5553 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 |  5554 | `{` |
|        - |  5555 | `	sxu32 nLine;` |
|        - |  5556 | `	sxi32 rc;` |
|     3995 |  5557 | `	nLine = pGen->pIn->nLine;` |
|     3995 |  5558 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - |  5559 | `	/* Reset namespace and clear previous use imports */` |
|     3995 |  5560 | `	SyBlobReset(&pGen->sNamespace);` |
|     3995 |  5561 | `	SyHashRelease(&pGen->hUseImports);` |
|     3995 |  5562 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5563 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     3995 |  5564 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5565 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     3995 |  5566 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5567 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5568 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 |  5569 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5570 | `		return SXRET_OK;` |
|        - |  5571 | `	}` |
|     3995 |  5572 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - |  5573 | `		/* namespace; — switch to global namespace */` |
|      ! 0 |  5574 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5575 | `		return SXRET_OK;` |
|        - |  5576 | `	}` |
|     3995 |  5577 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - |  5578 | `		/* namespace { } — global namespace block */` |
|      ! 0 |  5579 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5580 | `		return SXRET_OK;` |
|        - |  5581 | `	}` |
|        - |  5582 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8027 |  5583 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4037 |  5584 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - |  5585 | `			/* Append backslash separator */` |
|       26 |  5586 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       26 |  5587 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       11 |  5588 | `			}` |
|       15 |  5589 | `		}else{` |
|        - |  5590 | `			/* Append identifier */` |
|     4015 |  5591 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  5592 | `		}` |
|     4037 |  5593 | `		pGen->pIn++;` |
|        5 |  5594 | `	}` |
|        - |  5595 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - |  5596 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - |  5597 | `	{` |
|     3995 |  5598 | `		char *zNsDup = 0;` |
|     3995 |  5599 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     5987 |  5600 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3988 |  5601 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     1994 |  5602 | `		}` |
|     3995 |  5603 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - |  5604 | `	}` |
|     3995 |  5605 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 |  5606 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  5607 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 |  5608 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 |  5609 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5610 | `			return SXERR_ABORT;` |
|        - |  5611 | `		}` |
|        2 |  5612 | `	}` |
|     3995 |  5613 | `	return SXRET_OK;` |
|     2000 |  5614 | `}` |
|        - |  5615 | `/*` |
|        - |  5616 | ` * Compile the 'use' statement` |
|        - |  5617 | ` * According to the PHP language reference manual` |
|        - |  5618 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - |  5619 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - |  5620 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - |  5621 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - |  5622 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - |  5623 | ` *  a function or constant is not supported.` |
|        - |  5624 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - |  5625 | ` * NOTE` |
|        - |  5626 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5627 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5628 | ` */` |
|       72 |  5629 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 |  5630 | `{` |
|        - |  5631 | `	sxu32 nLine;` |
|        - |  5632 | `	sxi32 rc;` |
|        - |  5633 | `	SyBlob sPath;` |
|        - |  5634 | `	SyString sAlias;` |
|        - |  5635 | `	SyToken *pLast;` |
|        - |  5636 | `	char *zDup;` |
|        - |  5637 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - |  5638 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - |  5639 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       77 |  5640 | `	nLine = pGen->pIn->nLine;` |
|       77 |  5641 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - |  5642 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       77 |  5643 | `	iUseType = 0;` |
|       77 |  5644 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 |  5645 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 |  5646 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 |  5647 | `			iUseType = 1;` |
|       16 |  5648 | `			pGen->pIn++;` |
|       23 |  5649 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 |  5650 | `			iUseType = 2;` |
|       16 |  5651 | `			pGen->pIn++;` |
|        7 |  5652 | `		}` |
|       14 |  5653 | `	}` |
|        - |  5654 | `	/* Select target hash tables based on import type */` |
|       77 |  5655 | `	switch( iUseType ){` |
|        7 |  5656 | `		case 1:` |
|       16 |  5657 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 |  5658 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 |  5659 | `			break;` |
|        7 |  5660 | `		case 2:` |
|       16 |  5661 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 |  5662 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 |  5663 | `			break;` |
|       22 |  5664 | `		default:` |
|       49 |  5665 | `			pGenHash = &pGen->hUseImports;` |
|       49 |  5666 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       44 |  5667 | `			break;` |
|        - |  5668 | `	}` |
|       77 |  5669 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - |  5670 | `	/* Process one or more use declarations separated by commas */` |
|       37 |  5671 | `	for(;;){` |
|       79 |  5672 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  5673 | `			break;` |
|        - |  5674 | `		}` |
|       79 |  5675 | `		SyBlobReset(&sPath);` |
|       79 |  5676 | `		pLast = 0;` |
|        - |  5677 | `		/* Collect the full namespace path */` |
|      269 |  5678 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      195 |  5679 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      135 |  5680 | `				pLast = pGen->pIn;` |
|      135 |  5681 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       65 |  5682 | `					SyBlobAppend(&sPath,"\\",1);` |
|       30 |  5683 | `				}` |
|      135 |  5684 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       65 |  5685 | `			}` |
|      195 |  5686 | `			pGen->pIn++;` |
|        5 |  5687 | `		}` |
|       79 |  5688 | `		if( pLast == 0 ){` |
|        - |  5689 | `			/* Empty path */` |
|        6 |  5690 | `			break;` |
|        - |  5691 | `		}` |
|        - |  5692 | `		/* Default alias is the last component of the path */` |
|       75 |  5693 | `		sAlias = pLast->sData;` |
|        - |  5694 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       70 |  5695 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       50 |  5696 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       24 |  5697 | `			pGen->pIn++; /* Jump 'as' */` |
|       24 |  5698 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       24 |  5699 | `				sAlias = pGen->pIn->sData;` |
|       24 |  5700 | `				pGen->pIn++;` |
|       10 |  5701 | `			}` |
|       10 |  5702 | `		}` |
|        - |  5703 | `		/* Check for duplicate import alias (per-type) */` |
|       75 |  5704 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 |  5705 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  5706 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 |  5707 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 |  5708 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5709 | `				SyBlobRelease(&sPath);` |
|      ! 0 |  5710 | `				return SXERR_ABORT;` |
|        - |  5711 | `			}` |
|        2 |  5712 | `		}` |
|        - |  5713 | `		/* Register the import: alias -> FQN.` |
|        - |  5714 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - |  5715 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - |  5716 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      110 |  5717 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       70 |  5718 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       75 |  5719 | `		if( zDup ){` |
|       75 |  5720 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       75 |  5721 | `			if( pVmHash ){` |
|        - |  5722 | `				/* Class imports: populate VM table directly (class resolution` |
|        - |  5723 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       47 |  5724 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       47 |  5725 | `				if( zAliasDup ){` |
|       47 |  5726 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       21 |  5727 | `				}` |
|       21 |  5728 | `			}` |
|       75 |  5729 | `			if( iUseType == 2 ){` |
|        - |  5730 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - |  5731 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 |  5732 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 |  5733 | `				if( zAliasDup ){` |
|        - |  5734 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - |  5735 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - |  5736 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 |  5737 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 |  5738 | `					if( azPair ){` |
|       16 |  5739 | `						azPair[0] = zAliasDup;` |
|       16 |  5740 | `						azPair[1] = zDup;` |
|       16 |  5741 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 |  5742 | `					}` |
|        7 |  5743 | `				}` |
|        7 |  5744 | `			}` |
|       35 |  5745 | `		}` |
|        - |  5746 | `		/* Check for comma (multiple use declarations) */` |
|       75 |  5747 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 |  5748 | `			pGen->pIn++;` |
|        2 |  5749 | `		}else{` |
|       39 |  5750 | `			break;` |
|        - |  5751 | `		}` |
|        1 |  5752 | `	}` |
|       77 |  5753 | `	SyBlobRelease(&sPath);` |
|       77 |  5754 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 |  5755 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 |  5756 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 |  5757 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5758 | `			return SXERR_ABORT;` |
|        - |  5759 | `		}` |
|        1 |  5760 | `	}` |
|       77 |  5761 | `	return SXRET_OK;` |
|       41 |  5762 | `}` |
|        - |  5763 | `/*` |
|        - |  5764 | ` * Compile the stupid 'declare' language construct.` |
|        - |  5765 | ` *` |
|        - |  5766 | ` * According to the PHP language reference manual.` |
|        - |  5767 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - |  5768 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - |  5769 | ` *  declare (directive)` |
|        - |  5770 | ` *   statement` |
|        - |  5771 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - |  5772 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - |  5773 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - |  5774 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - |  5775 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - |  5776 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - |  5777 | ` * <?php` |
|        - |  5778 | ` * // these are the same:` |
|        - |  5779 | ` * // you can use this:` |
|        - |  5780 | ` * declare(ticks=1) {` |
|        - |  5781 | ` *   // entire script here` |
|        - |  5782 | ` * }` |
|        - |  5783 | ` * // or you can use this:` |
|        - |  5784 | ` * declare(ticks=1);` |
|        - |  5785 | ` * // entire script here` |
|        - |  5786 | ` * ?>` |
|        - |  5787 | ` *` |
|        - |  5788 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - |  5789 | ` */` |
|        - |  5790 | `/*` |
|        - |  5791 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - |  5792 | ` */` |
|       72 |  5793 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 |  5794 | `{` |
|      109 |  5795 | `	return SyStringLength(pName) == nWant` |
|       72 |  5796 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 |  5797 | `}` |
|        - |  5798 |  |
|       42 |  5799 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 |  5800 | `{` |
|       47 |  5801 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       47 |  5802 | `	SyToken *pBodyEnd = 0;` |
|        - |  5803 | `	SyToken *pBodyStart;` |
|        - |  5804 | `	SyToken *pCursor;` |
|        - |  5805 | `	int bHasStrictTypes;` |
|        - |  5806 | `	int bBlockForm;` |
|        - |  5807 | `	int bPlacementOk;` |
|        - |  5808 | `	sxi32 rc;` |
|       47 |  5809 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       47 |  5810 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 |  5811 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|        6 |  5812 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5813 | `			return SXERR_ABORT;` |
|        - |  5814 | `		}` |
|        6 |  5815 | `		goto Synchro;` |
|        - |  5816 | `	}` |
|       43 |  5817 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       43 |  5818 | `	pBodyStart = pGen->pIn;` |
|        - |  5819 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       43 |  5820 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       43 |  5821 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  5822 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|      ! 0 |  5823 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5824 | `			return SXERR_ABORT;` |
|        - |  5825 | `		}` |
|      ! 0 |  5826 | `		return SXRET_OK;` |
|        - |  5827 | `	}` |
|        - |  5828 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - |  5829 | `	 * now delimits the comma-separated directive list. */` |
|       43 |  5830 | `	pGen->pIn = &pBodyEnd[1];` |
|       43 |  5831 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 |  5832 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 |  5833 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5834 | `			return SXERR_ABORT;` |
|        - |  5835 | `		}` |
|      ! 0 |  5836 | `	}` |
|       43 |  5837 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       43 |  5838 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       43 |  5839 | `	bHasStrictTypes = 0;` |
|        - |  5840 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - |  5841 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - |  5842 | `	 * directive appears anywhere in the list, before validating values. */` |
|       43 |  5843 | `	pCursor = pBodyStart;` |
|       55 |  5844 | `	while( pCursor < pBodyEnd ){` |
|       51 |  5845 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       43 |  5846 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       39 |  5847 | `				bHasStrictTypes = 1;` |
|       39 |  5848 | `				break;` |
|        - |  5849 | `			}` |
|        2 |  5850 | `		}` |
|       14 |  5851 | `		pCursor++;` |
|        2 |  5852 | `	}` |
|       43 |  5853 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 |  5854 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5855 | `			"strict_types declaration must not use block mode");` |
|        3 |  5856 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5857 | `		return SXRET_OK;` |
|        - |  5858 | `	}` |
|       41 |  5859 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 |  5860 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5861 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 |  5862 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 |  5863 | `		return SXRET_OK;` |
|        - |  5864 | `	}` |
|        - |  5865 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       37 |  5866 | `	pCursor = pBodyStart;` |
|       69 |  5867 | `	while( pCursor < pBodyEnd ){` |
|        - |  5868 | `		SyToken *pNameTok;` |
|        - |  5869 | `		SyToken *pEqTok;` |
|        - |  5870 | `		SyToken *pValTok;` |
|        - |  5871 | `		SyString *pDirName;` |
|        - |  5872 | `		int bIsStrict;` |
|        - |  5873 | `		int iStrictValue;` |
|       39 |  5874 | `		pNameTok = pCursor;` |
|       39 |  5875 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  5876 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5877 | `				"declare: Expecting a directive name");` |
|      ! 0 |  5878 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5879 | `			return SXRET_OK;` |
|        - |  5880 | `		}` |
|       39 |  5881 | `		pEqTok = pNameTok + 1;` |
|       39 |  5882 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 |  5883 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5884 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 |  5885 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5886 | `			return SXRET_OK;` |
|        - |  5887 | `		}` |
|       39 |  5888 | `		pValTok = pEqTok + 1;` |
|       39 |  5889 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 |  5890 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5891 | `				"declare: Expecting value after '='");` |
|      ! 0 |  5892 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5893 | `			return SXRET_OK;` |
|        - |  5894 | `		}` |
|       39 |  5895 | `		pDirName = &pNameTok->sData;` |
|       39 |  5896 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       39 |  5897 | `		if( bIsStrict ){` |
|        - |  5898 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - |  5899 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       35 |  5900 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 |  5901 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5902 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 |  5903 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5904 | `				return SXRET_OK;` |
|        - |  5905 | `			}` |
|       35 |  5906 | `			iStrictValue = -1;` |
|       35 |  5907 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       35 |  5908 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       35 |  5909 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       35 |  5910 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       33 |  5911 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       15 |  5912 | `			}` |
|       35 |  5913 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 |  5914 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5915 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 |  5916 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5917 | `				return SXRET_OK;` |
|        - |  5918 | `			}` |
|       32 |  5919 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       18 |  5920 | `		}else{` |
|        - |  5921 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|        - |  5922 | `			 * preserve the legacy notice so callers relying on the old` |
|        - |  5923 | `			 * behavior don't regress. */` |
|        8 |  5924 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|        - |  5925 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|        2 |  5926 | `				ph7_lib_version()` |
|        - |  5927 | `				);` |
|        - |  5928 | `		}` |
|       36 |  5929 | `		pCursor = pValTok + 1;` |
|        - |  5930 | `		/* Consume separating comma (or end). */` |
|       36 |  5931 | `		if( pCursor < pBodyEnd ){` |
|        3 |  5932 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  5933 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5934 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 |  5935 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5936 | `				return SXRET_OK;` |
|        - |  5937 | `			}` |
|        3 |  5938 | `			pCursor++;` |
|        1 |  5939 | `		}` |
|        4 |  5940 | `	}` |
|        - |  5941 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - |  5942 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - |  5943 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       34 |  5944 | `	return SXRET_OK;` |
|        2 |  5945 | `Synchro:` |
|        - |  5946 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 |  5947 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 |  5948 | `		pGen->pIn++;` |
|        2 |  5949 | `	}` |
|        6 |  5950 | `	return SXRET_OK;` |
|       26 |  5951 | `}` |
|        - |  5952 | `/*` |
|        - |  5953 | ` * Process default argument values. That is,a function may define C++-style default value` |
|        - |  5954 | ` * as follows:` |
|        - |  5955 | ` * function makecoffee($type = "cappuccino")` |
|        - |  5956 | ` * {` |
|        - |  5957 | ` *   return "Making a cup of $type.\n";` |
|        - |  5958 | ` * }` |
|        - |  5959 | ` * Symisc eXtension.` |
|        - |  5960 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|        - |  5961 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|        - |  5962 | ` *      Example: Work only with PH7,generate error under zend` |
|        - |  5963 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|        - |  5964 | ` *      {` |
|        - |  5965 | ` *       var_dump($a);` |
|        - |  5966 | ` *      }` |
|        - |  5967 | ` *     //call test without args` |
|        - |  5968 | ` *      test();` |
|        - |  5969 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|        - |  5970 | ` *      Example:` |
|        - |  5971 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|        - |  5972 | ` * 3 -) Function overloading!!` |
|        - |  5973 | ` *      Example:` |
|        - |  5974 | ` *      function foo($a) {` |
|        - |  5975 | ` *   	  return $a.PHP_EOL;` |
|        - |  5976 | ` *	    }` |
|        - |  5977 | ` *	    function foo($a, $b) {` |
|        - |  5978 | ` *   	  return $a + $b;` |
|        - |  5979 | ` *	    }` |
|        - |  5980 | ` *	    echo foo(5); // Prints "5"` |
|        - |  5981 | ` *	    echo foo(5, 2); // Prints "7"` |
|        - |  5982 | ` *      // Same arg` |
|        - |  5983 | ` *	   function foo(string $a)` |
|        - |  5984 | ` *	   {` |
|        - |  5985 | ` *	     echo "a is a string\n";` |
|        - |  5986 | ` *	     var_dump($a);` |
|        - |  5987 | ` *	   }` |
|        - |  5988 | ` *	  function foo(int $a)` |
|        - |  5989 | ` *	  {` |
|        - |  5990 | ` *	    echo "a is integer\n";` |
|        - |  5991 | ` *	    var_dump($a);` |
|        - |  5992 | ` *	  }` |
|        - |  5993 | ` *	  function foo(array $a)` |
|        - |  5994 | ` *	  {` |
|        - |  5995 | ` * 	    echo "a is an array\n";` |
|        - |  5996 | ` * 	    var_dump($a);` |
|        - |  5997 | ` *	  }` |
|        - |  5998 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|        - |  5999 | ` *	  foo(52); // a is integer [second foo]` |
|        - |  6000 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|        - |  6001 | ` * Please refer to the official documentation for more information on the powerful extension` |
|        - |  6002 | ` * introduced by the PH7 engine.` |
|        - |  6003 | ` */` |
|   240956 |  6004 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |  6005 | `{` |
|        - |  6006 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6007 | `	SySet *pInstrContainer;` |
|        - |  6008 | `	sxi32 rc;` |
|        - |  6009 | `	/* Swap token stream */` |
|   240961 |  6010 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   240961 |  6011 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   240961 |  6012 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |  6013 | `	/* Compile the expression holding the argument value */` |
|   240961 |  6014 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  6015 | `	/* Emit the done instruction */` |
|   240961 |  6016 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   240961 |  6017 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   240961 |  6018 | `	RE_SWAP_DELIMITER(pGen);` |
|   240961 |  6019 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  6020 | `		return SXERR_ABORT;` |
|        - |  6021 | `	}` |
|   240961 |  6022 | `	return SXRET_OK;` |
|   120483 |  6023 | `}` |
|        - |  6024 | `/*` |
|        - |  6025 | ` * Collect function arguments one after one.` |
|        - |  6026 | ` * According to the PHP language reference manual.` |
|        - |  6027 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|        - |  6028 | ` * list of expressions.` |
|        - |  6029 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|        - |  6030 | ` * and default argument values. Variable-length argument lists are also supported,` |
|        - |  6031 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|        - |  6032 | ` * for more information.` |
|        - |  6033 | ` * Example #1 Passing arrays to functions` |
|        - |  6034 | ` * <?php` |
|        - |  6035 | ` * function takes_array($input)` |
|        - |  6036 | ` * {` |
|        - |  6037 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|        - |  6038 | ` * }` |
|        - |  6039 | ` * ?>` |
|        - |  6040 | ` * Making arguments be passed by reference` |
|        - |  6041 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|        - |  6042 | ` * within the function is changed, it does not get changed outside of the function).` |
|        - |  6043 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|        - |  6044 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|        - |  6045 | ` * to the argument name in the function definition:` |
|        - |  6046 | ` * Example #2 Passing function parameters by reference` |
|        - |  6047 | ` * <?php` |
|        - |  6048 | ` * function add_some_extra(&$string)` |
|        - |  6049 | ` * {` |
|        - |  6050 | ` *   $string .= 'and something extra.';` |
|        - |  6051 | ` * }` |
|        - |  6052 | ` * $str = 'This is a string, ';` |
|        - |  6053 | ` * add_some_extra($str);` |
|        - |  6054 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|        - |  6055 | ` * ?>` |
|        - |  6056 | ` *` |
|        - |  6057 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|        - |  6058 | ` * complex agrument values.Please refer to the official documentation for more information` |
|        - |  6059 | ` * on these extension.` |
|        - |  6060 | ` */` |
|   491182 |  6061 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  6062 | `{` |
|        - |  6063 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  6064 | `	SyToken *pIn;  /* Token stream */` |
|        - |  6065 | `	SyBlob sSig;         /* Function signature */` |
|        - |  6066 | `	char *zDup;          /* Copy of argument name */` |
|        - |  6067 | `	sxi32 rc;` |
|        - |  6068 |  |
|   491187 |  6069 | `	pIn = pGen->pIn;` |
|   491187 |  6070 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  6071 | `	/* Process arguments one after one */` |
|   604269 |  6072 | `	for(;;){` |
|  1208543 |  6073 | `		if( pIn >= pEnd ){` |
|        - |  6074 | `			/* No more arguments to process */` |
|   491171 |  6075 | `			break;` |
|        - |  6076 | `		}` |
|   717377 |  6077 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   717377 |  6078 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   717377 |  6079 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   717377 |  6080 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   717377 |  6081 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  6082 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  6083 | `		 * first token inside the main token stream */` |
|   717377 |  6084 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  6085 | `			return SXERR_ABORT;` |
|        - |  6086 | `		}` |
|        - |  6087 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|        - |  6088 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|        - |  6089 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|        - |  6090 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|        - |  6091 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|        - |  6092 | `		{` |
|   717377 |  6093 | `			int bReadonly = 0, bVisSeen = 0;` |
|   717377 |  6094 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   717377 |  6095 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        3 |  6096 | `				bReadonly = 1;` |
|        3 |  6097 | `				pIn++;` |
|        1 |  6098 | `			}` |
|   717377 |  6099 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    81937 |  6100 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    81937 |  6101 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|       83 |  6102 | `					bVisSeen = 1;` |
|       83 |  6103 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      111 |  6104 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|       36 |  6105 | `						: PH7_CLASS_PROT_PUBLIC;` |
|       83 |  6106 | `					pIn++;` |
|       83 |  6107 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       18 |  6108 | `						bReadonly = 1;` |
|       18 |  6109 | `						pIn++;` |
|        7 |  6110 | `					}` |
|       39 |  6111 | `				}` |
|    40966 |  6112 | `			}` |
|   717377 |  6113 | `			if( bVisSeen \|\| bReadonly ){` |
|       85 |  6114 | `				if( !bCtorCtx ){` |
|        6 |  6115 | `					if( bAbstractCtx ){` |
|        3 |  6116 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6117 | `							"Cannot declare promoted property in an abstract constructor");` |
|        2 |  6118 | `					}else{` |
|        3 |  6119 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6120 | `							"Cannot declare promoted property outside a constructor");` |
|        - |  6121 | `					}` |
|        6 |  6122 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  6123 | `						return SXERR_ABORT;` |
|        - |  6124 | `					}` |
|        6 |  6125 | `					return SXERR_SYNTAX;` |
|        - |  6126 | `				}` |
|       81 |  6127 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|       81 |  6128 | `				sArg.iPromoteVis = iVis;` |
|       81 |  6129 | `				if( bReadonly ){` |
|       20 |  6130 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|        8 |  6131 | `				}` |
|       38 |  6132 | `			}` |
|        - |  6133 | `		}` |
|        - |  6134 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   717368 |  6135 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   419183 |  6136 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   119046 |  6137 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    97589 |  6138 | `			sxu32 nLineLocal = pIn->nLine;` |
|    97589 |  6139 | `			sxi32 iTFlags = 0;` |
|    97589 |  6140 | `			pGen->pIn = pIn;` |
|    97589 |  6141 | `			rc = GenStateParseUnionTypeDecl(` |
|    48792 |  6142 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|    48792 |  6143 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  6144 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  6145 | `				/* bAllowVoid */ 0,` |
|    48792 |  6146 | `						nLineLocal);` |
|    97589 |  6147 | `			pIn = pGen->pIn;` |
|    97589 |  6148 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  6149 | `				return SXERR_ABORT;` |
|    97589 |  6150 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  6151 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  6152 | `				return SXERR_SYNTAX;` |
|    97587 |  6153 | `			}else if( rc == SXERR_SYNTAX ){` |
|       12 |  6154 | `				if( pIn < pEnd ){` |
|       16 |  6155 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  6156 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        4 |  6157 | `						&pIn->sData);` |
|        8 |  6158 | `				}else{` |
|      ! 0 |  6159 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  6160 | `						"syntax error, unexpected end of file");` |
|        - |  6161 | `				}` |
|       12 |  6162 | `				return SXERR_SYNTAX;` |
|        - |  6163 | `			}` |
|    97579 |  6164 | `			sArg.iFlags \|= iTFlags;` |
|    48787 |  6165 | `		}` |
|   717363 |  6166 | `		if( pIn >= pEnd ){` |
|      ! 0 |  6167 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  6168 | `			return rc;` |
|        - |  6169 | `		}` |
|   717363 |  6170 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  6171 | `			/* Pass by reference,record that */` |
|     3927 |  6172 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     3927 |  6173 | `			pIn++;` |
|     1961 |  6174 | `		}` |
|   717363 |  6175 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  6176 | `			/* Variadic parameter: ...$args */` |
|    19527 |  6177 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    19527 |  6178 | `			pIn++;` |
|     9761 |  6179 | `		}` |
|   717363 |  6180 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  6181 | `			/* Invalid argument */` |
|      ! 0 |  6182 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|      ! 0 |  6183 | `			return rc;` |
|        - |  6184 | `		}` |
|   717363 |  6185 | `		pIn++; /* Jump the dollar sign */` |
|        - |  6186 | `		/* Copy argument name */` |
|   717363 |  6187 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   717363 |  6188 | `		if( zDup == 0 ){` |
|      ! 0 |  6189 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  6190 | `			return SXERR_ABORT;` |
|        - |  6191 | `		}` |
|   717363 |  6192 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   717363 |  6193 | `		pIn++;` |
|   717363 |  6194 | `		if( pIn < pEnd ){` |
|   373895 |  6195 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  6196 | `				SyToken *pDefend;` |
|   240963 |  6197 | `				sxi32 iNest = 0;` |
|   240963 |  6198 | `				pIn++; /* Jump the equal sign */` |
|   240963 |  6199 | `				pDefend = pIn;` |
|        - |  6200 | `				/* Process the default value associated with this argument */` |
|   513019 |  6201 | `				while( pDefend < pEnd ){` |
|   365325 |  6202 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    93269 |  6203 | `						break;` |
|        - |  6204 | `					}` |
|   272061 |  6205 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  6206 | `						/* Increment nesting level */` |
|    15549 |  6207 | `						iNest++;` |
|   264289 |  6208 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  6209 | `						/* Decrement nesting level */` |
|    15549 |  6210 | `						iNest--;` |
|     7772 |  6211 | `					}` |
|   272061 |  6212 | `					pDefend++;` |
|        5 |  6213 | `				}` |
|   240963 |  6214 | `				if( pIn >= pDefend ){` |
|        3 |  6215 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|        3 |  6216 | `					return rc;` |
|        - |  6217 | `				}` |
|        - |  6218 | `				/* Process default value */` |
|   240961 |  6219 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   240961 |  6220 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  6221 | `					return rc;` |
|        - |  6222 | `				}` |
|        - |  6223 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  6224 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  6225 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  6226 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  6227 | `				 * arg-type check lets null through. */` |
|   240956 |  6228 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|   145743 |  6229 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|   145740 |  6230 | `					&& &pIn[1] == pDefend` |
|    46639 |  6231 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|    34974 |  6232 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|    21373 |  6233 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|    15547 |  6234 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|     7771 |  6235 | `				}` |
|        - |  6236 | `				/* Point beyond the default value */` |
|   240961 |  6237 | `				pIn = pDefend;` |
|   120478 |  6238 | `			}` |
|   373893 |  6239 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  6240 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  6241 | `				return rc;` |
|        - |  6242 | `			}` |
|   373893 |  6243 | `			pIn++; /* Jump the trailing comma */` |
|   186944 |  6244 | `		}` |
|        - |  6245 | `		/* Append argument signature */` |
|   717361 |  6246 | `		if( sArg.nType > 0 ){` |
|    97517 |  6247 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  6248 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    15621 |  6249 | `				int marker = 'o';` |
|    15621 |  6250 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    15621 |  6251 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     7813 |  6252 | `			}else{` |
|        - |  6253 | `				int c;` |
|    81901 |  6254 | `				c = 'n'; /* cc warning */` |
|        - |  6255 | `				/* Type leading character */` |
|    81901 |  6256 | `				switch(sArg.nType){` |
|     5831 |  6257 | `				case MEMOBJ_HASHMAP:` |
|        - |  6258 | `					/* Hashmap aka 'array' */` |
|    11667 |  6259 | `					c = 'h';` |
|    11667 |  6260 | `					break;` |
|     9821 |  6261 | `				case MEMOBJ_INT:` |
|        - |  6262 | `					/* Integer */` |
|    19647 |  6263 | `					c = 'i';` |
|    19647 |  6264 | `					break;` |
|        2 |  6265 | `				case MEMOBJ_BOOL:` |
|        - |  6266 | `					/* Bool */` |
|        5 |  6267 | `					c = 'b';` |
|        5 |  6268 | `					break;` |
|        5 |  6269 | `				case MEMOBJ_REAL:` |
|        - |  6270 | `					/* Float */` |
|       12 |  6271 | `					c = 'f';` |
|       12 |  6272 | `					break;` |
|    25281 |  6273 | `				case MEMOBJ_STRING:` |
|        - |  6274 | `					/* String */` |
|    50567 |  6275 | `					c = 's';` |
|    50567 |  6276 | `					break;` |
|        7 |  6277 | `				case MEMOBJ_OBJ:` |
|        - |  6278 | `					/* Object */` |
|       16 |  6279 | `					c = 'o';` |
|       14 |  6280 | `					break;` |
|        1 |  6281 | `				default:` |
|        2 |  6282 | `					break;` |
|        - |  6283 | `				}` |
|    81901 |  6284 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  6285 | `			}` |
|    48761 |  6286 | `		}else{` |
|        - |  6287 | `			/* No type is associated with this parameter which mean` |
|        - |  6288 | `			 * that this function is not condidate for overloading.` |
|        - |  6289 | `			 */` |
|   619849 |  6290 | `			SyBlobRelease(&sSig);` |
|        - |  6291 | `		}` |
|        - |  6292 | `		/* Save in the argument set */` |
|   717361 |  6293 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  6294 | `	}` |
|   491171 |  6295 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  6296 | `		/* Save function signature */` |
|    66375 |  6297 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|    33185 |  6298 | `	}` |
|   491171 |  6299 | `	return SXRET_OK;` |
|   245596 |  6300 | `}` |
|        - |  6301 | `/*` |
|        - |  6302 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  6303 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  6304 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  6305 | ` */` |
|    34978 |  6306 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  6307 | `{` |
|    34983 |  6308 | `	sxi32 iParen = 0;` |
|    34983 |  6309 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  6310 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  6311 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  6312 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|   155461 |  6313 | `	while( pIn < pEnd ){` |
|   155461 |  6314 | `		sxu32 t = pIn->nType;` |
|   155461 |  6315 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|   151555 |  6316 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|   104925 |  6317 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|    85483 |  6318 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|   120483 |  6319 | `		pIn++;` |
|        5 |  6320 | `	}` |
|    19447 |  6321 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  6322 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  6323 | `	{` |
|    19447 |  6324 | `		sxi32 d = 0;` |
|   773147 |  6325 | `		while( pIn < pEnd ){` |
|   773147 |  6326 | `			sxu32 t = pIn->nType;` |
|   773147 |  6327 | `			if( t & PH7_TK_OCB ){ d++; }` |
|   742049 |  6328 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|   753705 |  6329 | `			pIn++;` |
|        5 |  6330 | `		}` |
|        - |  6331 | `	}` |
|    19447 |  6332 | `	return pIn;` |
|    17494 |  6333 | `}` |
|        - |  6334 | `/*` |
|        - |  6335 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  6336 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  6337 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  6338 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  6339 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  6340 | ` * detached-mini-program path untouched.` |
|        - |  6341 | ` */` |
|        - |  6342 | `/*` |
|        - |  6343 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  6344 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  6345 | ` * mixed, object.` |
|        - |  6346 | ` */` |
|       28 |  6347 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        3 |  6348 | `{` |
|        - |  6349 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  6350 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  6351 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  6352 | `	};` |
|        - |  6353 | `	sxu32 i;` |
|       31 |  6354 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  6355 | `		zName++;` |
|      ! 0 |  6356 | `		nName--;` |
|      ! 0 |  6357 | `	}` |
|       63 |  6358 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|       59 |  6359 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|       27 |  6360 | `			return 1;` |
|        - |  6361 | `		}` |
|       17 |  6362 | `	}` |
|        5 |  6363 | `	return 0;` |
|       17 |  6364 | `}` |
|        - |  6365 | `/*` |
|        - |  6366 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  6367 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  6368 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  6369 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  6370 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  6371 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  6372 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  6373 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  6374 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  6375 | ` * both rather than fatal on valid code (divergence recorded in PLAN.md).` |
|        - |  6376 | ` */` |
|       26 |  6377 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        4 |  6378 | `{` |
|       30 |  6379 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  6380 | ``		return 1; /* bare `object` */`` |
|        - |  6381 | `	}` |
|       30 |  6382 | `	if( nType != SXU32_HIGH ){` |
|        3 |  6383 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  6384 | `	}` |
|       27 |  6385 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|       23 |  6386 | `		return 1;` |
|        - |  6387 | `	}` |
|        - |  6388 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  6389 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  6390 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  6391 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  6392 | `	{` |
|        - |  6393 | `		SyBlob sFQN;` |
|        - |  6394 | `		int bOk;` |
|        5 |  6395 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        5 |  6396 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|        5 |  6397 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|        5 |  6398 | `		SyBlobRelease(&sFQN);` |
|        5 |  6399 | `		return bOk;` |
|        - |  6400 | `	}` |
|       17 |  6401 | `}` |
|        - |  6402 | `/*` |
|        - |  6403 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  6404 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  6405 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  6406 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  6407 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  6408 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  6409 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  6410 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  6411 | ` */` |
|      258 |  6412 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  6413 | `{` |
|      263 |  6414 | `	int bOk = 0;` |
|        - |  6415 | `	sxu32 nLine;` |
|        - |  6416 | `	sxi32 rc;` |
|      263 |  6417 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      237 |  6418 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  6419 | `	}` |
|       30 |  6420 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  6421 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  6422 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  6423 | `		sxu32 i,j;` |
|      ! 0 |  6424 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  6425 | `			int bGroupOk;` |
|      ! 0 |  6426 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  6427 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  6428 | `			}` |
|      ! 0 |  6429 | `			bGroupOk = 1;` |
|      ! 0 |  6430 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  6431 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  6432 | `					bGroupOk = 0;` |
|      ! 0 |  6433 | `					break;` |
|        - |  6434 | `				}` |
|      ! 0 |  6435 | `			}` |
|      ! 0 |  6436 | `			bOk = bGroupOk;` |
|      ! 0 |  6437 | `		}` |
|      ! 0 |  6438 | `	}else{` |
|       30 |  6439 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  6440 | `	}` |
|       30 |  6441 | `	if( bOk ){` |
|       27 |  6442 | `		return SXRET_OK;` |
|        - |  6443 | `	}` |
|        - |  6444 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  6445 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  6446 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  6447 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|        - |  6448 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  6449 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  6450 | `	{` |
|        3 |  6451 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  6452 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  6453 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  6454 | `		}` |
|        3 |  6455 | `		if( sGiven.nByte < 1 ){` |
|        - |  6456 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|        - |  6457 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|        - |  6458 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|      ! 0 |  6459 | `			const char *zScalar =` |
|      ! 0 |  6460 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|      ! 0 |  6461 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|      ! 0 |  6462 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|      ! 0 |  6463 | `		}` |
|        3 |  6464 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  6465 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  6466 | `	}` |
|        3 |  6467 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      134 |  6468 | `}` |
|  1405316 |  6469 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  6470 | `{` |
|  1405321 |  6471 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  1405321 |  6472 | `	SyToken *pEnd = pGen->pEnd;` |
|  1405321 |  6473 | `	sxi32 iDepth = 0;` |
|  1405321 |  6474 | `	int bStarted = 0;` |
| 63054443 |  6475 | `	while( pIn < pEnd ){` |
| 63054443 |  6476 | `		sxu32 t = pIn->nType;` |
| 63054443 |  6477 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 60086805 |  6478 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 57119537 |  6479 | `		if( t & PH7_TK_KEYWORD ){` |
|  4638545 |  6480 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  4638545 |  6481 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  4638287 |  6482 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  6483 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  2301652 |  6484 | `		}` |
| 57084301 |  6485 | `		pIn++;` |
|        5 |  6486 | `	}` |
|  1405063 |  6487 | `	return FALSE;` |
|   702663 |  6488 | `}` |
|        - |  6489 | `/*` |
|        - |  6490 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  6491 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  6492 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  6493 | ` */` |
|  1405316 |  6494 | `static sxi32 GenStateCompileFuncBody(` |
|        - |  6495 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  6496 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  6497 | `	)` |
|        5 |  6498 | `{` |
|        - |  6499 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  6500 | `	GenBlock *pBlock;` |
|        - |  6501 | `	sxu32 nGotoOfft;` |
|        - |  6502 | `	sxi32 rc;` |
|        - |  6503 | `	/* Attach the new function */` |
|  1405321 |  6504 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  1405321 |  6505 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6506 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  6507 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6508 | `		return SXERR_ABORT;` |
|        - |  6509 | `	}` |
|  1405321 |  6510 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  6511 | `	/* Swap bytecode containers */` |
|  1405321 |  6512 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  1405321 |  6513 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  6514 | `	/* Emit constructor property promotion prologue:` |
|        - |  6515 | `	 *   $this->NAME = $NAME;` |
|        - |  6516 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  6517 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  6518 | `	{` |
|  1405321 |  6519 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  6520 | `		sxu32 i;` |
|  2091449 |  6521 | `		for( i = 0; i < nArg; i++ ){` |
|   686133 |  6522 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  6523 | `			char *zSrc;` |
|        - |  6524 | `			sxu32 nSrc,nName;` |
|        - |  6525 | `			SySet sToken;` |
|        - |  6526 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6527 | `			sxi32 rcPromote;` |
|   686133 |  6528 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   686067 |  6529 | `				continue;` |
|        - |  6530 | `			}` |
|        - |  6531 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  6532 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  6533 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  6534 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  6535 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|       71 |  6536 | `			nName = SyStringLength(&pArg->sName);` |
|       71 |  6537 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|       71 |  6538 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|       71 |  6539 | `			if( zSrc == 0 ){` |
|      ! 0 |  6540 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6541 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6542 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  6543 | `				return SXERR_ABORT;` |
|        - |  6544 | `			}` |
|        - |  6545 | `			{` |
|       71 |  6546 | `				char *z = zSrc;` |
|       71 |  6547 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|       71 |  6548 | `				z += sizeof("$this->")-1;` |
|       71 |  6549 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       71 |  6550 | `				z += nName;` |
|       71 |  6551 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|       71 |  6552 | `				z += sizeof(" = $")-1;` |
|       71 |  6553 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       71 |  6554 | `				z += nName;` |
|       71 |  6555 | `				*z = 0;` |
|        - |  6556 | `			}` |
|       71 |  6557 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       71 |  6558 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|       71 |  6559 | `			pTmpIn = pGen->pIn;` |
|       71 |  6560 | `			pTmpEnd = pGen->pEnd;` |
|       71 |  6561 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       71 |  6562 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       71 |  6563 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|       71 |  6564 | `			pGen->pIn = pTmpIn;` |
|       71 |  6565 | `			pGen->pEnd = pTmpEnd;` |
|       71 |  6566 | `			SySetRelease(&sToken);` |
|       71 |  6567 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  6568 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6569 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6570 | `				return SXERR_ABORT;` |
|        - |  6571 | `			}` |
|        - |  6572 | `			/* Discard the assignment result — this is a statement expression. */` |
|       71 |  6573 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       38 |  6574 | `		}` |
|        - |  6575 | `	}` |
|        - |  6576 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  6577 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  6578 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  6579 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  6580 | `	{` |
|  1405321 |  6581 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  1405321 |  6582 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|        - |  6583 | `		/* Compile the body */` |
|  1405321 |  6584 | `		PH7_CompileBlock(&(*pGen),0);` |
|  1405321 |  6585 | `		pGen->bInGenerator = bSavedGen;` |
|        - |  6586 | `	}` |
|        - |  6587 | `	/* Fix exception jumps now the destination is resolved */` |
|  1405321 |  6588 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  6589 | `	/* Emit the final return if not yet done */` |
|  1405321 |  6590 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  6591 | `	/* Fix gotos jumps now the destination is resolved */` |
|  1405321 |  6592 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  6593 | `		rc = SXERR_ABORT;` |
|      ! 0 |  6594 | `	}` |
|  1405321 |  6595 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  6596 | `	/* Restore the default container */` |
|  1405321 |  6597 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  6598 | `	/* Leave function block */` |
|  1405321 |  6599 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  1405321 |  6600 | `	if( rc == SXERR_ABORT ){` |
|        - |  6601 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6602 | `		return SXERR_ABORT;` |
|        - |  6603 | `	}` |
|        - |  6604 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  6605 | `	{` |
|  1405321 |  6606 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  6607 | `		sxu32 i;` |
| 38312301 |  6608 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 36907243 |  6609 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      263 |  6610 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      263 |  6611 | `				break;` |
|        - |  6612 | `			}` |
| 18453495 |  6613 | `		}` |
|        - |  6614 | `	}` |
|  1405321 |  6615 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6616 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|      263 |  6617 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  6618 | `			return SXERR_ABORT;` |
|        - |  6619 | `		}` |
|      129 |  6620 | `	}` |
|        - |  6621 | `	/* All done, function body compiled */` |
|  1405321 |  6622 | `	return SXRET_OK;` |
|   702663 |  6623 | `}` |
|        - |  6624 | `/*` |
|        - |  6625 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  6626 | ` * According to the PHP language reference manual.` |
|        - |  6627 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  6628 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  6629 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  6630 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  6631 | ` *  Functions need not be defined before they are referenced.` |
|        - |  6632 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  6633 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  6634 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  6635 | ` *  calls with over 32-64 recursion levels.` |
|        - |  6636 | ` *` |
|        - |  6637 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  6638 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  6639 | ` * on these extension.` |
|        - |  6640 | ` */` |
|        - |  6641 | `/*` |
|        - |  6642 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  6643 | ` */` |
|      570 |  6644 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  6645 | `{` |
|        - |  6646 | `	sxu32 i;` |
|     1611 |  6647 | `	for( i = 0; i < n; i++ ){` |
|     1381 |  6648 | `		int a = zA[i], b = zB[i];` |
|     1381 |  6649 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     1381 |  6650 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     1381 |  6651 | `		if( a != b ) return a - b;` |
|      523 |  6652 | `	}` |
|      235 |  6653 | `	return 0;` |
|      290 |  6654 | `}` |
|        - |  6655 | `/*` |
|        - |  6656 | ` * Internal type-atom kinds used during union type parsing.` |
|        - |  6657 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - |  6658 | ` * (which are positive bit values stored in sxu32).` |
|        - |  6659 | ` */` |
|        - |  6660 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - |  6661 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - |  6662 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - |  6663 |  |
|        - |  6664 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - |  6665 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - |  6666 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - |  6667 |  |
|        - |  6668 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - |  6669 | `struct PhlTypeAtom {` |
|        - |  6670 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - |  6671 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - |  6672 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - |  6673 | `	sxu32 nCanon;` |
|        - |  6674 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - |  6675 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - |  6676 | `};` |
|        - |  6677 |  |
|        - |  6678 | `/*` |
|        - |  6679 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - |  6680 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - |  6681 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - |  6682 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - |  6683 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - |  6684 | ` * already be consumed by the caller.` |
|        - |  6685 | ` */` |
|    98634 |  6686 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 |  6687 | `{` |
|    98639 |  6688 | `	SyToken *pIn = pGen->pIn;` |
|    98639 |  6689 | `	SyZero(pOut, sizeof(*pOut));` |
|    98639 |  6690 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    98639 |  6691 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6692 | `		return SXERR_SYNTAX;` |
|        - |  6693 | `	}` |
|        - |  6694 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    98639 |  6695 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        8 |  6696 | `		pIn++;` |
|        8 |  6697 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6698 | `			return SXERR_SYNTAX;` |
|        - |  6699 | `		}` |
|        3 |  6700 | `	}` |
|    98639 |  6701 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  6702 | `		return SXERR_SYNTAX;` |
|        - |  6703 | `	}` |
|    98639 |  6704 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    82563 |  6705 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    82563 |  6706 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|    11691 |  6707 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    76720 |  6708 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       81 |  6709 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    70839 |  6710 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    19947 |  6711 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    60830 |  6712 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|    50777 |  6713 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|    25473 |  6714 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       41 |  6715 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|       68 |  6716 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       27 |  6717 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|       37 |  6718 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       14 |  6719 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       23 |  6720 | `			pOut->nType = SXU32_HIGH;` |
|       23 |  6721 | `			pOut->sClass = pIn->sData;` |
|       13 |  6722 | `		}else{` |
|        3 |  6723 | `			return SXERR_SYNTAX;` |
|        - |  6724 | `		}` |
|    82561 |  6725 | `		pIn++;` |
|    41283 |  6726 | `	}else{` |
|        - |  6727 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - |  6728 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    16081 |  6729 | `		SyString *pT = &pIn->sData;` |
|    16081 |  6730 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       34 |  6731 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       34 |  6732 | `			pIn++;` |
|    16066 |  6733 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      177 |  6734 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      177 |  6735 | `			pIn++;` |
|    15965 |  6736 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       26 |  6737 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       26 |  6738 | `			pIn++;` |
|       15 |  6739 | `		}else{` |
|        - |  6740 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    15857 |  6741 | `			SyToken *pFirst = pIn;` |
|    15857 |  6742 | `			SyToken *pLast = pIn;` |
|    15857 |  6743 | `			pOut->nType = SXU32_HIGH;` |
|    15857 |  6744 | `			pOut->sClass = pIn->sData;` |
|    15857 |  6745 | `			pIn++;` |
|    23781 |  6746 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    15860 |  6747 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|        3 |  6748 | `				pLast = &pIn[1];` |
|        3 |  6749 | `				pIn += 2;` |
|        1 |  6750 | `			}` |
|    15857 |  6751 | `			if( pLast != pFirst ){` |
|        3 |  6752 | `				const char *zFirst = pFirst->sData.zString;` |
|        3 |  6753 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|        3 |  6754 | `				pOut->sClass.zString = zFirst;` |
|        3 |  6755 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|        1 |  6756 | `			}` |
|        - |  6757 | `		}` |
|        - |  6758 | `	}` |
|    98637 |  6759 | `	pGen->pIn = pIn;` |
|    98637 |  6760 | `	return SXRET_OK;` |
|    49322 |  6761 | `}` |
|        - |  6762 |  |
|        - |  6763 | `/*` |
|        - |  6764 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - |  6765 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - |  6766 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|        - |  6767 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - |  6768 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - |  6769 | ` */` |
|    98456 |  6770 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 |  6771 | `{` |
|        - |  6772 | `	int i;` |
|    98461 |  6773 | `	int nNonNull = 0;` |
|    98461 |  6774 | `	int bAnyIntersection = 0;` |
|        - |  6775 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    98461 |  6776 | `	sxu32 nMaxGroup = 0;` |
|  3249053 |  6777 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   197069 |  6778 | `	for( i = 0; i < nAtoms; i++ ){` |
|    98613 |  6779 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    98583 |  6780 | `			nNonNull++;` |
|    98583 |  6781 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    98583 |  6782 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    98583 |  6783 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|    49289 |  6784 | `			}` |
|    49289 |  6785 | `		}` |
|    49309 |  6786 | `	}` |
|   197017 |  6787 | `	for( i = 0; i < nAtoms; i++ ){` |
|    98585 |  6788 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       29 |  6789 | `			bAnyIntersection = 1;` |
|       29 |  6790 | `			break;` |
|        - |  6791 | `		}` |
|    49283 |  6792 | `	}` |
|    98461 |  6793 | `	if( bAnyIntersection ){` |
|        - |  6794 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - |  6795 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - |  6796 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       29 |  6797 | `		sxu32 g, nGroups = 0;` |
|       29 |  6798 | `		int bFirstGroup = 1;` |
|       59 |  6799 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|       59 |  6800 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       35 |  6801 | `			int bFirstMember = 1;` |
|        - |  6802 | `			int bWrap;` |
|       35 |  6803 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - |  6804 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - |  6805 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - |  6806 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - |  6807 | `			 * parens, matching PHP's canonical text. */` |
|       47 |  6808 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       35 |  6809 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       35 |  6810 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      107 |  6811 | `			for( i = 0; i < nAtoms; i++ ){` |
|       77 |  6812 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|       59 |  6813 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|       59 |  6814 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       55 |  6815 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       30 |  6816 | `				}else{` |
|        6 |  6817 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6818 | `				}` |
|       59 |  6819 | `				bFirstMember = 0;` |
|       32 |  6820 | `			}` |
|       35 |  6821 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       35 |  6822 | `			bFirstGroup = 0;` |
|       20 |  6823 | `		}` |
|       29 |  6824 | `		if( bNullable ){` |
|      ! 0 |  6825 | `			SyBlobAppend(pBlob, "\|", 1);` |
|      ! 0 |  6826 | `			SyBlobAppend(pBlob, "null", 4);` |
|      ! 0 |  6827 | `		}` |
|       78 |  6828 | `		return;` |
|        - |  6829 | `	}` |
|    98437 |  6830 | `	if( nNonNull == 1 && bNullable ){` |
|        - |  6831 | `		/* Shorthand: ?T */` |
|      102 |  6832 | `		for( i = 0; i < nAtoms; i++ ){` |
|      102 |  6833 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      102 |  6834 | `			SyBlobAppend(pBlob, "?", 1);` |
|      102 |  6835 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|       24 |  6836 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       14 |  6837 | `			}else{` |
|       82 |  6838 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6839 | `			}` |
|      102 |  6840 | `			return;` |
|      ! 0 |  6841 | `		}` |
|      ! 0 |  6842 | `	}` |
|        - |  6843 | `	{` |
|    98339 |  6844 | `		int bFirst = 1;` |
|        - |  6845 | `		/* 1) Classes in declaration order */` |
|   196781 |  6846 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98447 |  6847 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    15807 |  6848 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    15807 |  6849 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    15807 |  6850 | `				bFirst = 0;` |
|     7901 |  6851 | `			}` |
|    49226 |  6852 | `		}` |
|        - |  6853 | `		/* 2) Built-ins in canonical order */` |
|        - |  6854 | `		{` |
|        - |  6855 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|        - |  6856 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|        - |  6857 | `			int k;` |
|   688343 |  6858 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  1098111 |  6859 | `				for( i = 0; i < nAtoms; i++ ){` |
|   590545 |  6860 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|    82443 |  6861 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    82443 |  6862 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|    82443 |  6863 | `						bFirst = 0;` |
|    82443 |  6864 | `						break;` |
|        - |  6865 | `					}` |
|   254056 |  6866 | `				}` |
|   295007 |  6867 | `			}` |
|        - |  6868 | `		}` |
|        - |  6869 | `		/* 3) null suffix */` |
|    98339 |  6870 | `		if( bNullable ){` |
|       19 |  6871 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       19 |  6872 | `			SyBlobAppend(pBlob, "null", 4);` |
|        8 |  6873 | `		}` |
|        - |  6874 | `	}` |
|    49233 |  6875 | `}` |
|        - |  6876 |  |
|        - |  6877 | `/*` |
|        - |  6878 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - |  6879 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - |  6880 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - |  6881 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - |  6882 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - |  6883 | ` * whether it was parenthesized.` |
|        - |  6884 | ` *` |
|        - |  6885 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - |  6886 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - |  6887 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - |  6888 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - |  6889 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - |  6890 | ` */` |
|    98608 |  6891 | `static sxi32 GenStateParsePart(` |
|        - |  6892 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - |  6893 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 |  6894 | `{` |
|        - |  6895 | `	sxi32 rc;` |
|    98613 |  6896 | `	int nMembers = 0;` |
|    98613 |  6897 | `	int bParen = 0;` |
|    98613 |  6898 | `	*pnMembers = 0;` |
|    98613 |  6899 | `	*pbParen = 0;` |
|    98613 |  6900 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        9 |  6901 | `		bParen = 1;` |
|        9 |  6902 | `		pGen->pIn++; /* skip '(' */` |
|        3 |  6903 | `	}` |
|    49304 |  6904 | `	for(;;){` |
|    98639 |  6905 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 |  6906 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6907 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 |  6908 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6909 | `		}` |
|    98639 |  6910 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    98639 |  6911 | `		if( rc != SXRET_OK ){` |
|        3 |  6912 | `			return rc;` |
|        - |  6913 | `		}` |
|    98637 |  6914 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    98637 |  6915 | `		(*pnAtoms)++;` |
|    98637 |  6916 | `		nMembers++;` |
|        - |  6917 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    98637 |  6918 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       39 |  6919 | `			SyToken *pNext = &pGen->pIn[1];` |
|       34 |  6920 | `			if( pNext < pGen->pEnd` |
|       39 |  6921 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       31 |  6922 | `				pGen->pIn++; /* skip '&' */` |
|       31 |  6923 | `				continue;` |
|        - |  6924 | `			}` |
|        4 |  6925 | `		}` |
|    98611 |  6926 | `		break;` |
|      ! 0 |  6927 | `	}` |
|    98611 |  6928 | `	if( bParen ){` |
|        9 |  6929 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  6930 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6931 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 |  6932 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6933 | `		}` |
|        9 |  6934 | `		pGen->pIn++; /* skip ')' */` |
|        9 |  6935 | `		if( nMembers < 2 ){` |
|      ! 0 |  6936 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6937 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 |  6938 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6939 | `		}` |
|        3 |  6940 | `	}` |
|    98611 |  6941 | `	*pnMembers = nMembers;` |
|    98611 |  6942 | `	*pbParen = bParen;` |
|    98611 |  6943 | `	return SXRET_OK;` |
|    49309 |  6944 | `}` |
|        - |  6945 |  |
|        - |  6946 | `/*` |
|        - |  6947 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - |  6948 | ` *` |
|        - |  6949 | ` * Outputs:` |
|        - |  6950 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - |  6951 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - |  6952 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - |  6953 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - |  6954 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - |  6955 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - |  6956 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - |  6957 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - |  6958 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - |  6959 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - |  6960 | ` *` |
|        - |  6961 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - |  6962 | ` * SXERR_ABORT on fatal compile errors.` |
|        - |  6963 | ` */` |
|    98472 |  6964 | `static sxi32 GenStateParseUnionTypeDecl(` |
|        - |  6965 | `	ph7_gen_state *pGen,` |
|        - |  6966 | `	sxu32 *pnType,` |
|        - |  6967 | `	SyString *pClass,` |
|        - |  6968 | `	SySet *pAlts,` |
|        - |  6969 | `	sxi32 *piTypeFlags,` |
|        - |  6970 | `	SyString *pTypeText,` |
|        - |  6971 | `	int iNullableFlag,` |
|        - |  6972 | `	int iUnionFlag,` |
|        - |  6973 | `	int bAllowVoid,` |
|        - |  6974 | `	sxu32 nLine` |
|        5 |  6975 | `){` |
|        - |  6976 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    98477 |  6977 | `	int nAtoms = 0;` |
|    98477 |  6978 | `	int bShortNullable = 0;` |
|    98477 |  6979 | `	int bExplicitNull = 0;` |
|        - |  6980 | `	sxi32 rc;` |
|    98477 |  6981 | `	*pnType = 0;` |
|    98477 |  6982 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    98477 |  6983 | `	*piTypeFlags = 0;` |
|    98477 |  6984 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - |  6985 |  |
|    98477 |  6986 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  6987 | `		return SXRET_OK;` |
|        - |  6988 | `	}` |
|        - |  6989 | ``	/* Optional `?` shorthand prefix */`` |
|    98472 |  6990 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       91 |  6991 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       90 |  6992 | `		bShortNullable = 1;` |
|       90 |  6993 | `		pGen->pIn++;` |
|       90 |  6994 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  6995 | `			return SXERR_SYNTAX;` |
|        - |  6996 | `		}` |
|       43 |  6997 | `	}` |
|        - |  6998 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - |  6999 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - |  7000 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - |  7001 | `	{` |
|        - |  7002 | `		int nMembers, bParen;` |
|    98477 |  7003 | `		sxu32 iGroup = 0;` |
|    98477 |  7004 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    98477 |  7005 | `		if( rc != SXRET_OK ){` |
|        4 |  7006 | `			return rc;` |
|        - |  7007 | `		}` |
|        - |  7008 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - |  7009 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - |  7010 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - |  7011 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - |  7012 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|   147914 |  7013 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    98684 |  7014 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      143 |  7015 | `			if( bShortNullable ){` |
|        - |  7016 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - |  7017 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - |  7018 | `				 * already reported" so callers skip their own error emission. */` |
|        3 |  7019 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7020 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 |  7021 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - |  7022 | `			}` |
|      141 |  7023 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 |  7024 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - |  7025 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7026 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7027 | `			}` |
|      141 |  7028 | ``			pGen->pIn++; /* skip `\|` */`` |
|      141 |  7029 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|      141 |  7030 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7031 | `				return rc;` |
|        - |  7032 | `			}` |
|        5 |  7033 | `		}` |
|    98473 |  7034 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 |  7035 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7036 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7037 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7038 | `		}` |
|        - |  7039 | `	}` |
|        - |  7040 | `	/* Validation pass.` |
|        - |  7041 | `	 *` |
|        - |  7042 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - |  7043 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - |  7044 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - |  7045 | `	 */` |
|        - |  7046 | `	{` |
|        - |  7047 | `		int i, j;` |
|    98473 |  7048 | `		int bHasNonNull = 0;` |
|    98473 |  7049 | `		int bAnyIntersection = 0;` |
|        - |  7050 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - |  7051 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - |  7052 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|  3249449 |  7053 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   197103 |  7054 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98635 |  7055 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|    49320 |  7056 | `		}` |
|   197047 |  7057 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98605 |  7058 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|    49292 |  7059 | `		}` |
|        - |  7060 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - |  7061 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    98473 |  7062 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 |  7063 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7064 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 |  7065 | `			return SXERR_SYNTAX;` |
|        - |  7066 | `		}` |
|   197089 |  7067 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - |  7068 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - |  7069 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - |  7070 | ``			 * `true`/`false` in an intersection). */`` |
|    98633 |  7071 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       55 |  7072 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       55 |  7073 | `				if( bClassLike ){` |
|       53 |  7074 | `					SyString *pC = &aAtoms[i].sClass;` |
|       48 |  7075 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       48 |  7076 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       48 |  7077 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       53 |  7078 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 |  7079 | `						bClassLike = 0;` |
|      ! 0 |  7080 | `					}` |
|       24 |  7081 | `				}` |
|       55 |  7082 | `				if( !bClassLike ){` |
|        - |  7083 | `					const char *zName; sxu32 nName;` |
|        3 |  7084 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7085 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7086 | `					}else{` |
|        3 |  7087 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - |  7088 | `					}` |
|        4 |  7089 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7090 | `						"Type %.*s cannot be part of an intersection type",` |
|        1 |  7091 | `						(int)nName, zName);` |
|        3 |  7092 | `					return SXERR_SYNTAX;` |
|        - |  7093 | `				}` |
|       24 |  7094 | `			}` |
|    98631 |  7095 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      177 |  7096 | `				if( nAtoms > 1 ){` |
|        3 |  7097 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7098 | `						"Void can only be used as a standalone type");` |
|        3 |  7099 | `					return SXERR_SYNTAX;` |
|        - |  7100 | `				}` |
|      175 |  7101 | `				if( !bAllowVoid ){` |
|      ! 0 |  7102 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7103 | `						"void cannot be used here");` |
|      ! 0 |  7104 | `					return SXERR_SYNTAX;` |
|        - |  7105 | `				}` |
|      175 |  7106 | `				if( bShortNullable ){` |
|      ! 0 |  7107 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7108 | `						"Void type cannot be nullable");` |
|      ! 0 |  7109 | `					return SXERR_SYNTAX;` |
|        - |  7110 | `				}` |
|       85 |  7111 | `			}` |
|    98629 |  7112 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - |  7113 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - |  7114 | `				 * type (never = the function does not return). Mirrors the void` |
|        - |  7115 | `				 * validation above; accepted here and enforced at compile time` |
|        - |  7116 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       26 |  7117 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - |  7118 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - |  7119 | `					 * same as any other non-standalone use. */` |
|        5 |  7120 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7121 | `						"never can only be used as a standalone type");` |
|        5 |  7122 | `					return SXERR_SYNTAX;` |
|        - |  7123 | `				}` |
|       21 |  7124 | `				if( !bAllowVoid ){` |
|        - |  7125 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 |  7126 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7127 | `						"never cannot be used as a parameter type");` |
|        3 |  7128 | `					return SXERR_SYNTAX;` |
|        - |  7129 | `				}` |
|        8 |  7130 | `			}` |
|    98623 |  7131 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       34 |  7132 | `				bExplicitNull = 1;` |
|       19 |  7133 | `			}else{` |
|    98593 |  7134 | `				bHasNonNull = 1;` |
|        - |  7135 | `			}` |
|        - |  7136 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - |  7137 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - |  7138 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - |  7139 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - |  7140 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    98823 |  7141 | `			for( j = 0; j < i; j++ ){` |
|      207 |  7142 | `				int bDup = 0;` |
|      207 |  7143 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|      395 |  7144 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|      202 |  7145 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|      207 |  7146 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|      195 |  7147 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|       51 |  7148 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       44 |  7149 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       44 |  7150 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       17 |  7151 | `								aAtoms[j].sClass.zString,` |
|       34 |  7152 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 |  7153 | `							bDup = 1;` |
|      ! 0 |  7154 | `						}` |
|       27 |  7155 | `					}else{` |
|        3 |  7156 | `						bDup = 1;` |
|        - |  7157 | `					}` |
|       23 |  7158 | `				}` |
|      195 |  7159 | `				if( bDup ){` |
|        - |  7160 | `					const char *zName;` |
|        - |  7161 | `					sxu32 nName;` |
|        3 |  7162 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7163 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 |  7164 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7165 | `					}else{` |
|        3 |  7166 | `						zName = aAtoms[i].zCanon;` |
|        3 |  7167 | `						nName = aAtoms[i].nCanon;` |
|        - |  7168 | `					}` |
|        4 |  7169 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 |  7170 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 |  7171 | `					return SXERR_SYNTAX;` |
|        - |  7172 | `				}` |
|       99 |  7173 | `			}` |
|    49313 |  7174 | `		}` |
|    98461 |  7175 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 |  7176 | `			if( bShortNullable ){` |
|        - |  7177 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 |  7178 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7179 | `					"Null can not be used as a standalone type");` |
|      ! 0 |  7180 | `				return SXERR_SYNTAX;` |
|        - |  7181 | `			}` |
|        - |  7182 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - |  7183 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - |  7184 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - |  7185 | `			 * atom, so set it here. */` |
|        7 |  7186 | `			*pnType = MEMOBJ_NULL;` |
|        3 |  7187 | `		}` |
|        - |  7188 | `	}` |
|        - |  7189 | `	/* Compute nullability flag */` |
|    98461 |  7190 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      118 |  7191 | `		*piTypeFlags \|= iNullableFlag;` |
|       57 |  7192 | `	}` |
|        - |  7193 | `	/* Build canonical type text */` |
|    98461 |  7194 | `	if( pTypeText ){` |
|        - |  7195 | `		SyBlob sBlob;` |
|    98461 |  7196 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   147647 |  7197 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|    49228 |  7198 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    98461 |  7199 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   147410 |  7200 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    98270 |  7201 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    98275 |  7202 | `			if( zDup ){` |
|    98275 |  7203 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|    49135 |  7204 | `			}` |
|    49135 |  7205 | `		}` |
|    98461 |  7206 | `		SyBlobRelease(&sBlob);` |
|    49228 |  7207 | `	}` |
|        - |  7208 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - |  7209 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - |  7210 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - |  7211 | `	{` |
|    98461 |  7212 | `		int nNonNull = 0;` |
|    98461 |  7213 | `		int iNonNullIdx = -1;` |
|        - |  7214 | `		int i;` |
|   197069 |  7215 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98613 |  7216 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    98583 |  7217 | `				nNonNull++;` |
|    98583 |  7218 | `				iNonNullIdx = i;` |
|    49289 |  7219 | `			}` |
|    49309 |  7220 | `		}` |
|    98461 |  7221 | `		if( nNonNull <= 1 ){` |
|        - |  7222 | `			/* Fast path: store as single type. */` |
|    98355 |  7223 | `			if( iNonNullIdx >= 0 ){` |
|    98349 |  7224 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    98349 |  7225 | `				if( pA->nType == SXU32_HIGH ){` |
|    23672 |  7226 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     7889 |  7227 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    15783 |  7228 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    15783 |  7229 | `					*pnType = SXU32_HIGH;` |
|    15783 |  7230 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    90460 |  7231 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      175 |  7232 | `					*pnType = MEMOBJ_VOID;` |
|    82486 |  7233 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       18 |  7234 | `					*pnType = MEMOBJ_NEVER;` |
|       10 |  7235 | `				}else{` |
|    82385 |  7236 | `					*pnType = pA->nType;` |
|        - |  7237 | `				}` |
|    49172 |  7238 | `			}` |
|    49180 |  7239 | `		}else{` |
|        - |  7240 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      111 |  7241 | `			*piTypeFlags \|= iUnionFlag;` |
|      355 |  7242 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - |  7243 | `				ph7_type_alt sAlt;` |
|      249 |  7244 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      239 |  7245 | `				SyZero(&sAlt, sizeof(sAlt));` |
|      239 |  7246 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|      239 |  7247 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      146 |  7248 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       47 |  7249 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       99 |  7250 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|       99 |  7251 | `					sAlt.nType = SXU32_HIGH;` |
|       99 |  7252 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|       52 |  7253 | `				}else{` |
|      145 |  7254 | `					sAlt.nType = aAtoms[i].nType;` |
|      145 |  7255 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - |  7256 | `				}` |
|      239 |  7257 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      122 |  7258 | `			}` |
|        - |  7259 | `		}` |
|        - |  7260 | `	}` |
|    98461 |  7261 | `	return SXRET_OK;` |
|    49241 |  7262 | `}` |
|        - |  7263 |  |
|        - |  7264 | `/*` |
|        - |  7265 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - |  7266 | `` * pGen->pIn should point to the token after `)`.`` |
|        - |  7267 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - |  7268 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - |  7269 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - |  7270 | `` *          and union types `: T\|U`.`` |
|        - |  7271 | ` */` |
|  1506652 |  7272 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 |  7273 | `{` |
|  1506657 |  7274 | `	sxi32 iFlags = 0;` |
|        - |  7275 | `	sxi32 rc;` |
|        - |  7276 | `	sxu32 nLine;` |
|  1506657 |  7277 | `	pFunc->nReturnType = 0;` |
|  1506657 |  7278 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  1506657 |  7279 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - |  7280 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - |  7281 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - |  7282 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - |  7283 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - |  7284 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|  1506657 |  7285 | `	SySetReset(&pFunc->aReturnUnion);` |
|  1506657 |  7286 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|  1506657 |  7287 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  1506005 |  7288 | `		return SXRET_OK;` |
|        - |  7289 | `	}` |
|      657 |  7290 | `	pGen->pIn++; /* Skip ':' */` |
|      657 |  7291 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7292 | `		return SXRET_OK;` |
|        - |  7293 | `	}` |
|      657 |  7294 | `	nLine = pGen->pIn->nLine;` |
|      657 |  7295 | `	rc = GenStateParseUnionTypeDecl(` |
|      326 |  7296 | `		pGen,` |
|      326 |  7297 | `		&pFunc->nReturnType,` |
|      326 |  7298 | `		&pFunc->sReturnClass,` |
|      326 |  7299 | `		&pFunc->aReturnUnion,` |
|        - |  7300 | `		&iFlags,` |
|      326 |  7301 | `		&pFunc->sReturnTypeName,` |
|        - |  7302 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - |  7303 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - |  7304 | `		/* iUnionFlag */ 0,` |
|        - |  7305 | `		/* bAllowVoid */ 1,` |
|      326 |  7306 | `		nLine);` |
|      657 |  7307 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7308 | `		return SXERR_ABORT;` |
|        - |  7309 | `	}` |
|      657 |  7310 | `	if( rc == SXERR_CORRUPT ){` |
|        - |  7311 | `		/* Error already reported */` |
|      ! 0 |  7312 | `		return SXERR_SYNTAX;` |
|        - |  7313 | `	}` |
|      657 |  7314 | `	if( rc == SXERR_SYNTAX ){` |
|        8 |  7315 | `		if( pGen->pIn < pGen->pEnd ){` |
|       11 |  7316 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7317 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|        6 |  7318 | `				&pGen->pIn->sData);` |
|        5 |  7319 | `		}else{` |
|      ! 0 |  7320 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - |  7321 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - |  7322 | `		}` |
|        8 |  7323 | `		return SXERR_SYNTAX;` |
|        - |  7324 | `	}` |
|      651 |  7325 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|      651 |  7326 | `	return SXRET_OK;` |
|   753331 |  7327 | `}` |
|        - |  7328 |  |
|   118332 |  7329 | `static sxi32 GenStateCompileFunc(` |
|        - |  7330 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  7331 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - |  7332 | `	sxi32 iFlags,        /* Control flags */` |
|        - |  7333 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - |  7334 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - |  7335 | `	)` |
|        5 |  7336 | `{` |
|        - |  7337 | `	ph7_vm_func *pFunc;` |
|        - |  7338 | `	SyToken *pEnd;` |
|        - |  7339 | `	sxu32 nLine;` |
|        - |  7340 | `	char *zName;` |
|        - |  7341 | `	sxi32 rc;` |
|        - |  7342 | `	/* Extract line number */` |
|   118337 |  7343 | `	nLine = pGen->pIn->nLine;` |
|        - |  7344 | `	/* Jump the left parenthesis '(' */` |
|   118337 |  7345 | `	pGen->pIn++;` |
|        - |  7346 | `	/* Delimit the function signature */` |
|   118337 |  7347 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   118337 |  7348 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  7349 | `		/* Syntax error */` |
|        8 |  7350 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|        8 |  7351 | `		if( rc == SXERR_ABORT ){` |
|        - |  7352 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7353 | `			return SXERR_ABORT;` |
|        - |  7354 | `		}` |
|        8 |  7355 | `		pGen->pIn = pGen->pEnd;` |
|        8 |  7356 | `		return SXRET_OK;` |
|        - |  7357 | `	}` |
|        - |  7358 | `	/* Create the function state */` |
|   118331 |  7359 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   118331 |  7360 | `	if( pFunc == 0 ){` |
|      ! 0 |  7361 | `		goto OutOfMem;` |
|        - |  7362 | `	}` |
|        - |  7363 | `	/* Build the function name, prepending namespace if active */` |
|   118338 |  7364 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - |  7365 | `		SyBlob sFQN;` |
|        - |  7366 | `		sxu32 nLen;` |
|       16 |  7367 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       16 |  7368 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       16 |  7369 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       16 |  7370 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       16 |  7371 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       16 |  7372 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       16 |  7373 | `		SyBlobRelease(&sFQN);` |
|       16 |  7374 | `		if( zName == 0 ){` |
|      ! 0 |  7375 | `			goto OutOfMem;` |
|        - |  7376 | `		}` |
|       16 |  7377 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        9 |  7378 | `	}else{` |
|   118317 |  7379 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   118317 |  7380 | `		if( zName == 0 ){` |
|      ! 0 |  7381 | `			goto OutOfMem;` |
|        - |  7382 | `		}` |
|   118317 |  7383 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - |  7384 | `	}` |
|        - |  7385 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - |  7386 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   118331 |  7387 | `	pFunc->nLine = nLine;` |
|   118331 |  7388 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   118331 |  7389 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  7390 | `		return SXERR_ABORT;` |
|        - |  7391 | `	}` |
|   118331 |  7392 | `	if( pGen->pIn < pEnd ){` |
|        - |  7393 | `		/* Collect function arguments */` |
|   102055 |  7394 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   102055 |  7395 | `		if( rc == SXERR_ABORT ){` |
|        - |  7396 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  7397 | `			return SXERR_ABORT;` |
|        - |  7398 | `		}` |
|    51025 |  7399 | `	}` |
|        - |  7400 | `	/* Point past ')' and parse optional return type ': type' */` |
|   118331 |  7401 | `	pGen->pIn = &pEnd[1];` |
|        - |  7402 | `	{` |
|   118331 |  7403 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   118331 |  7404 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  7405 | `			return SXERR_ABORT;` |
|   118331 |  7406 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|        8 |  7407 | `			return SXERR_SYNTAX;` |
|        - |  7408 | `		}` |
|        - |  7409 | `	}` |
|   118325 |  7410 | `	if( bHandleClosure ){` |
|        - |  7411 | `		ph7_vm_func_closure_env sEnv;` |
|      369 |  7412 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|      364 |  7413 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      200 |  7414 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|       31 |  7415 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  7416 | `				/* Closure,record environment variable */` |
|       31 |  7417 | `				pGen->pIn++;` |
|       31 |  7418 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  7419 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 |  7420 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  7421 | `						return SXERR_ABORT;` |
|        - |  7422 | `					}` |
|      ! 0 |  7423 | `				}` |
|       31 |  7424 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - |  7425 | `				/* Compile until we hit the first closing parenthesis */` |
|       61 |  7426 | `				while( pGen->pIn < pGen->pEnd ){` |
|       61 |  7427 | `					int iFlagsLocal = 0;` |
|       61 |  7428 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|       31 |  7429 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|       31 |  7430 | `						break;` |
|        - |  7431 | `					}` |
|       35 |  7432 | `					nLineLocal = pGen->pIn->nLine;` |
|       35 |  7433 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - |  7434 | `						/* Pass by reference,record that */` |
|      ! 0 |  7435 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|        - |  7436 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  7437 | `							);` |
|      ! 0 |  7438 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|      ! 0 |  7439 | `						pGen->pIn++;` |
|      ! 0 |  7440 | `					}` |
|       30 |  7441 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       35 |  7442 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  7443 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  7444 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 |  7445 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  7446 | `								return SXERR_ABORT;` |
|        - |  7447 | `							}` |
|        - |  7448 | `							/* Find the closing parenthesis */` |
|      ! 0 |  7449 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  7450 | `								pGen->pIn++;` |
|      ! 0 |  7451 | `							}` |
|      ! 0 |  7452 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 |  7453 | `								pGen->pIn++;` |
|      ! 0 |  7454 | `							}` |
|      ! 0 |  7455 | `							break;` |
|        - |  7456 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 |  7457 | `					}else{` |
|        - |  7458 | `						SyString *pNameLocal;` |
|        - |  7459 | `						char *zDup;` |
|        - |  7460 | `						/* Duplicate variable name */` |
|       35 |  7461 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       35 |  7462 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       35 |  7463 | `						if( zDup ){` |
|        - |  7464 | `							/* Zero the structure */` |
|       35 |  7465 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       35 |  7466 | `							sEnv.iFlags = iFlagsLocal;` |
|       35 |  7467 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       35 |  7468 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       35 |  7469 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|      ! 0 |  7470 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 |  7471 | `									got_this = 1;` |
|      ! 0 |  7472 | `							}` |
|        - |  7473 | `							/* Save imported variable */` |
|       35 |  7474 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       20 |  7475 | `						}else{` |
|      ! 0 |  7476 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  7477 | `							 return SXERR_ABORT;` |
|        - |  7478 | `						}` |
|        - |  7479 | `					}` |
|       35 |  7480 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       41 |  7481 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  7482 | `						/* Ignore trailing commas */` |
|        7 |  7483 | `						pGen->pIn++;` |
|        1 |  7484 | `					}` |
|        5 |  7485 | `				}` |
|       31 |  7486 | `				if( !got_this ){` |
|        - |  7487 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|        - |  7488 | `					 * available to the closure environment.` |
|        - |  7489 | `					 */` |
|       31 |  7490 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       31 |  7491 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       31 |  7492 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       31 |  7493 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       31 |  7494 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       13 |  7495 | `				}` |
|       31 |  7496 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - |  7497 | `					/* Mark as closure */` |
|       31 |  7498 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       13 |  7499 | `				}` |
|        - |  7500 | `				/* php 7.1+: the return type follows the use clause —` |
|        - |  7501 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - |  7502 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - |  7503 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - |  7504 | `				 * legacy pre-use position. */` |
|       31 |  7505 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|        7 |  7506 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|        7 |  7507 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 |  7508 | `						return SXERR_ABORT;` |
|        7 |  7509 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 |  7510 | `						return SXERR_SYNTAX;` |
|        - |  7511 | `					}` |
|        3 |  7512 | `				}` |
|       13 |  7513 | `		}` |
|      182 |  7514 | `	}` |
|        - |  7515 | `	/* Compile the body */` |
|   118325 |  7516 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   118325 |  7517 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7518 | `		return SXERR_ABORT;` |
|        - |  7519 | `	}` |
|        - |  7520 | `	/* The cursor sits just past the body's closing brace */` |
|   118325 |  7521 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   118325 |  7522 | `	if( ppFunc ){` |
|   118325 |  7523 | `		*ppFunc = pFunc;` |
|    59160 |  7524 | `	}` |
|   118325 |  7525 | `	rc = SXRET_OK;` |
|   118325 |  7526 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|        - |  7527 | `		/* Finally register the function */` |
|   118299 |  7528 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    59147 |  7529 | `	}` |
|   118325 |  7530 | `	if( rc == SXRET_OK ){` |
|   118325 |  7531 | `		return SXRET_OK;` |
|        - |  7532 | `	}` |
|        - |  7533 | `	/* Fall through if something goes wrong */` |
|      ! 0 |  7534 | `OutOfMem:` |
|        - |  7535 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  7536 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  7537 | `	 */` |
|      ! 0 |  7538 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  7539 | `	return SXERR_ABORT;` |
|    59171 |  7540 | `}` |
|        - |  7541 | `/*` |
|        - |  7542 | ` * Compile a standard PHP function.` |
|        - |  7543 | ` *  Refer to the block-comment above for more information.` |
|        - |  7544 | ` */` |
|   117976 |  7545 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 |  7546 | `{` |
|        - |  7547 | `	SyString *pName;` |
|        - |  7548 | `	sxi32 iFlags;` |
|        - |  7549 | `	sxu32 nKwLine;` |
|        - |  7550 | `	sxu32 nLine;` |
|        - |  7551 | `	sxi32 rc;` |
|        - |  7552 |  |
|   117981 |  7553 | `	nLine = pGen->pIn->nLine;` |
|   117981 |  7554 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   117981 |  7555 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   117981 |  7556 | `	iFlags = 0;` |
|   117981 |  7557 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  7558 | `		/* Return by reference,remember that */` |
|       12 |  7559 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  7560 | `		/* Jump the '&' token */` |
|       12 |  7561 | `		pGen->pIn++;` |
|        5 |  7562 | `	}` |
|   117981 |  7563 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  7564 | `		/* Invalid function name */` |
|        8 |  7565 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|        8 |  7566 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7567 | `			return SXERR_ABORT;` |
|        - |  7568 | `		}` |
|        - |  7569 | `		/* Sychronize with the next semi-colon or braces*/` |
|       22 |  7570 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       16 |  7571 | `			pGen->pIn++;` |
|        2 |  7572 | `		}` |
|        8 |  7573 | `		return SXRET_OK;` |
|        - |  7574 | `	}` |
|   117975 |  7575 | `	pName = &pGen->pIn->sData;` |
|   117975 |  7576 | `	nLine = pGen->pIn->nLine;` |
|        - |  7577 | `	/* Jump the function name */` |
|   117975 |  7578 | `	pGen->pIn++;` |
|   117975 |  7579 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  7580 | `		/* Syntax error */` |
|        3 |  7581 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|        3 |  7582 | `		if( rc == SXERR_ABORT ){` |
|        - |  7583 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7584 | `			return SXERR_ABORT;` |
|        - |  7585 | `		}` |
|        - |  7586 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 |  7587 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  7588 | `			pGen->pIn++;` |
|      ! 0 |  7589 | `		}` |
|        3 |  7590 | `		return SXRET_OK;` |
|        - |  7591 | `	}` |
|        - |  7592 | `	/* Compile function body */` |
|        - |  7593 | `	{` |
|   117973 |  7594 | `		ph7_vm_func *pFuncState = 0;` |
|   117973 |  7595 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|   117973 |  7596 | `		if( pFuncState ){` |
|        - |  7597 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   117961 |  7598 | `			pFuncState->nLine = nKwLine;` |
|    58978 |  7599 | `		}` |
|        - |  7600 | `	}` |
|   117973 |  7601 | `	return rc;` |
|    58993 |  7602 | `}` |
|        - |  7603 | `/*` |
|        - |  7604 | ` * Extract the visibility level associated with a given keyword.` |
|        - |  7605 | ` * According to the PHP language reference manual` |
|        - |  7606 | ` *  Visibility:` |
|        - |  7607 | ` *  The visibility of a property or method can be defined by prefixing` |
|        - |  7608 | ` *  the declaration with the keywords public, protected or private.` |
|        - |  7609 | ` *  Class members declared public can be accessed everywhere.` |
|        - |  7610 | ` *  Members declared protected can be accessed only within the class` |
|        - |  7611 | ` *  itself and by inherited and parent classes. Members declared as private` |
|        - |  7612 | ` *  may only be accessed by the class that defines the member.` |
|        - |  7613 | ` */` |
|  1742470 |  7614 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |  7615 | `{` |
|  1742475 |  7616 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    23463 |  7617 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  1719017 |  7618 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   182629 |  7619 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  7620 | `	}` |
|        - |  7621 | `	/* Assume public by default */` |
|  1536393 |  7622 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   871240 |  7623 | `}` |
|        - |  7624 | `/*` |
|        - |  7625 | ` * Compile a class constant.` |
|        - |  7626 | ` * According to the PHP language reference manual` |
|        - |  7627 | ` *  Class Constants` |
|        - |  7628 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - |  7629 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - |  7630 | ` *   you don't use the $ symbol to declare or use them.` |
|        - |  7631 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - |  7632 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - |  7633 | ` *   It's also possible for interfaces to have constants.` |
|        - |  7634 | ` * Symisc eXtension.` |
|        - |  7635 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - |  7636 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  7637 | ` *  Example:` |
|        - |  7638 | ` *   class Test{` |
|        - |  7639 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  7640 | ` *   };` |
|        - |  7641 | ` *   var_dump(TEST::MyConst);` |
|        - |  7642 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  7643 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  7644 | ` */` |
|        - |  7645 | `/*` |
|        - |  7646 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |  7647 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |  7648 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |  7649 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |  7650 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |  7651 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |  7652 | ` */` |
|   143872 |  7653 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |  7654 | `{` |
|        - |  7655 | `	SyToken *p0, *p1;` |
|   143877 |  7656 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7657 | `		return 0;` |
|        - |  7658 | `	}` |
|   143877 |  7659 | `	p0 = pGen->pIn;` |
|        - |  7660 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|   143877 |  7661 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|      ! 0 |  7662 | `		return 1;` |
|        - |  7663 | `	}` |
|   143877 |  7664 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  7665 | `		return 1;` |
|        - |  7666 | `	}` |
|        - |  7667 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  7668 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|        - |  7669 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|   143873 |  7670 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   143873 |  7671 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|   143873 |  7672 | `		if( p1 ){` |
|   143873 |  7673 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|       34 |  7674 | `				return 1;` |
|        - |  7675 | `			}` |
|   143843 |  7676 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|        5 |  7677 | `				return 1;` |
|        - |  7678 | `			}` |
|    71917 |  7679 | `		}` |
|    71917 |  7680 | `	}` |
|   143839 |  7681 | `	return 0;` |
|    71941 |  7682 | `}` |
|        - |  7683 | `/*` |
|        - |  7684 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|        - |  7685 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|        - |  7686 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|        - |  7687 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|        - |  7688 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|        - |  7689 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|        - |  7690 | ` * Peek only; never consumes tokens.` |
|        - |  7691 | ` */` |
|       24 |  7692 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|        4 |  7693 | `{` |
|       28 |  7694 | `	SyToken *p = pGen->pIn;` |
|       39 |  7695 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       20 |  7696 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|        3 |  7697 | `		p++; /* skip leading unary sign(s) */` |
|        1 |  7698 | `	}` |
|       28 |  7699 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|       23 |  7700 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|        - |  7701 | `	}` |
|        6 |  7702 | `	p++;` |
|        - |  7703 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|        6 |  7704 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|       16 |  7705 | `}` |
|        - |  7706 | `/*` |
|        - |  7707 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|        - |  7708 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|        - |  7709 | `` * `$o->new`), not a `new` expression.`` |
|        - |  7710 | ` */` |
|        6 |  7711 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|        3 |  7712 | `{` |
|        - |  7713 | `	sxi32 iOp;` |
|        9 |  7714 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|      ! 0 |  7715 | `		return 0;` |
|        - |  7716 | `	}` |
|        9 |  7717 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|        9 |  7718 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        6 |  7719 | `}` |
|        - |  7720 | `/*` |
|        - |  7721 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|        - |  7722 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|        - |  7723 | ` * interface-constant and (instance/static) property-default initializers` |
|        - |  7724 | ` * ("New expressions are not supported in this context") while still allowing it` |
|        - |  7725 | ` * in global constants, parameter defaults and static-local initializers (which` |
|        - |  7726 | ` * are compiled by different functions and left untouched). The scan is` |
|        - |  7727 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|        - |  7728 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|        - |  7729 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|        - |  7730 | ` *` |
|        - |  7731 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|        - |  7732 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|        - |  7733 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|        - |  7734 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|        - |  7735 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|        - |  7736 | ` */` |
|   229894 |  7737 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|        5 |  7738 | `{` |
|   229899 |  7739 | `	SyToken *p = pGen->pIn;` |
|   229899 |  7740 | `	int iDepth = 0;` |
|   561749 |  7741 | `	while( p < pGen->pEnd ){` |
|   561749 |  7742 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   229891 |  7743 | `			break; /* end of this initializer */` |
|        - |  7744 | `		}` |
|   331858 |  7745 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   169834 |  7746 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     7800 |  7747 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  7748 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|        - |  7749 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|        - |  7750 | `			 * expression. */` |
|        3 |  7751 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        3 |  7752 | `			p++;` |
|        3 |  7753 | `			if( bArrow ){` |
|        - |  7754 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|        - |  7755 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|        3 |  7756 | `				int iBase = iDepth;` |
|       17 |  7757 | `				while( p < pGen->pEnd ){` |
|       17 |  7758 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        5 |  7759 | `						iDepth++;` |
|       15 |  7760 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        5 |  7761 | `						if( iDepth <= iBase ){` |
|      ! 0 |  7762 | `							break; /* closes an enclosing group, not the fn's own */` |
|        - |  7763 | `						}` |
|        5 |  7764 | `						iDepth--;` |
|       11 |  7765 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        3 |  7766 | `						break;` |
|        - |  7767 | `					}` |
|       15 |  7768 | `					p++;` |
|        1 |  7769 | `				}` |
|        2 |  7770 | `			}else{` |
|        - |  7771 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|        - |  7772 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|        - |  7773 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|        - |  7774 | `				 * then skip the balanced brace block. */` |
|      ! 0 |  7775 | `				int iLocal = 0;` |
|      ! 0 |  7776 | `				while( p < pGen->pEnd ){` |
|      ! 0 |  7777 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      ! 0 |  7778 | `						break; /* body brace */` |
|        - |  7779 | `					}` |
|      ! 0 |  7780 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  7781 | `						iLocal++;` |
|      ! 0 |  7782 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  7783 | `						if( iLocal > 0 ){` |
|      ! 0 |  7784 | `							iLocal--;` |
|      ! 0 |  7785 | `						}` |
|      ! 0 |  7786 | `					}` |
|      ! 0 |  7787 | `					p++;` |
|      ! 0 |  7788 | `				}` |
|      ! 0 |  7789 | `				if( p < pGen->pEnd ){` |
|      ! 0 |  7790 | `					int iBrace = 0; /* p is on the body '{' */` |
|      ! 0 |  7791 | `					while( p < pGen->pEnd ){` |
|      ! 0 |  7792 | `						if( p->nType & PH7_TK_OCB ){` |
|      ! 0 |  7793 | `							iBrace++;` |
|      ! 0 |  7794 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      ! 0 |  7795 | `							iBrace--;` |
|      ! 0 |  7796 | `							if( iBrace == 0 ){` |
|      ! 0 |  7797 | `								p++;` |
|      ! 0 |  7798 | `								break;` |
|        - |  7799 | `							}` |
|      ! 0 |  7800 | `						}` |
|      ! 0 |  7801 | `						p++;` |
|      ! 0 |  7802 | `					}` |
|      ! 0 |  7803 | `				}` |
|        - |  7804 | `			}` |
|        3 |  7805 | `			continue;` |
|        - |  7806 | `		}` |
|   331861 |  7807 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     7845 |  7808 | `			iDepth++;` |
|   327941 |  7809 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     7843 |  7810 | `			if( iDepth > 0 ){` |
|     7843 |  7811 | `				iDepth--;` |
|     3919 |  7812 | `			}` |
|   320102 |  7813 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    86129 |  7814 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  7815 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|        - |  7816 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|        - |  7817 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|       11 |  7818 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  7819 | `				return 1;` |
|        - |  7820 | `			}` |
|      ! 0 |  7821 | `		}` |
|   331853 |  7822 | `		p++;` |
|        5 |  7823 | `	}` |
|   229891 |  7824 | `	return 0;` |
|   114952 |  7825 | `}` |
|        - |  7826 | `/*` |
|        - |  7827 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  7828 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  7829 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  7830 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  7831 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  7832 | ` * share the same backing.` |
|        - |  7833 | ` */` |
|      226 |  7834 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  7835 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  7836 | `{` |
|      231 |  7837 | `	pAttr->nType = nType;` |
|      231 |  7838 | `	pAttr->sClass = *pClass;` |
|      231 |  7839 | `	pAttr->sTypeName = *pTypeName;` |
|      231 |  7840 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  7841 | `		sxu32 i;` |
|       73 |  7842 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       51 |  7843 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       51 |  7844 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       28 |  7845 | `		}` |
|       11 |  7846 | `	}` |
|      231 |  7847 | `}` |
|   143872 |  7848 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  7849 | `{` |
|   143877 |  7850 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  7851 | `	SySet *pInstrContainer;` |
|        - |  7852 | `	ph7_class_attr *pCons;` |
|        - |  7853 | `	SyString *pName;` |
|        - |  7854 | `	sxi32 rc;` |
|   143877 |  7855 | `	sxu32 nType = 0;` |
|        - |  7856 | `	SyString sTypeClass;` |
|        - |  7857 | `	SyString sTypeText;` |
|        - |  7858 | `	SySet aUnionAlts;` |
|   143877 |  7859 | `	sxi32 iTypeFlags = 0;` |
|   143877 |  7860 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   143877 |  7861 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   143877 |  7862 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  7863 | `	/* Extract visibility level */` |
|   143877 |  7864 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  7865 | `	/* Mark as constant */` |
|   143877 |  7866 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   143877 |  7867 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  7868 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  7869 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   143896 |  7870 | `	if( GenStateClassConstHasType(pGen) ){` |
|       61 |  7871 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|       38 |  7872 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|        - |  7873 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  7874 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  7875 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  7876 | `		 * and success paths release. */` |
|       42 |  7877 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  7878 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  7879 | `			goto Synchronize;` |
|       42 |  7880 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  7881 | `			return SXERR_ABORT;` |
|       42 |  7882 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  7883 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  7884 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|      ! 0 |  7885 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  7886 | `				return SXERR_ABORT;` |
|        - |  7887 | `			}` |
|      ! 0 |  7888 | `			goto Synchronize;` |
|        - |  7889 | `		}` |
|       42 |  7890 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       19 |  7891 | `	}` |
|    71936 |  7892 | `loop:` |
|   143879 |  7893 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  7894 | `		/* Invalid constant name */` |
|      ! 0 |  7895 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  7896 | `		if( rc == SXERR_ABORT ){` |
|        - |  7897 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7898 | `			return SXERR_ABORT;` |
|        - |  7899 | `		}` |
|      ! 0 |  7900 | `		goto Synchronize;` |
|        - |  7901 | `	}` |
|        - |  7902 | `	/* Peek constant name */` |
|   143879 |  7903 | `	pName = &pGen->pIn->sData;` |
|        - |  7904 | `	/* Make sure the constant name isn't reserved */` |
|   143879 |  7905 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  7906 | `		/* Reserved constant name */` |
|      ! 0 |  7907 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  7908 | `		if( rc == SXERR_ABORT ){` |
|        - |  7909 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7910 | `			return SXERR_ABORT;` |
|        - |  7911 | `		}` |
|      ! 0 |  7912 | `		goto Synchronize;` |
|        - |  7913 | `	}` |
|        - |  7914 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   143879 |  7915 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       61 |  7916 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       38 |  7917 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       19 |  7918 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       42 |  7919 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7920 | `			return SXERR_ABORT;` |
|       42 |  7921 | `		}else if( rc != SXRET_OK ){` |
|        3 |  7922 | `			goto Synchronize;` |
|        - |  7923 | `		}` |
|       18 |  7924 | `	}` |
|        - |  7925 | `	/* Advance the stream cursor */` |
|   143877 |  7926 | `	pGen->pIn++;` |
|   143877 |  7927 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  7928 | `		/* Invalid declaration */` |
|      ! 0 |  7929 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  7930 | `		if( rc == SXERR_ABORT ){` |
|        - |  7931 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7932 | `			return SXERR_ABORT;` |
|        - |  7933 | `		}` |
|      ! 0 |  7934 | `		goto Synchronize;` |
|        - |  7935 | `	}` |
|   143877 |  7936 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  7937 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  7938 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  7939 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  7940 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   143872 |  7941 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       39 |  7942 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  7943 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  7944 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  7945 | `			&pClass->sName,pName,&sTypeText);` |
|        6 |  7946 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7947 | `			return SXERR_ABORT;` |
|        - |  7948 | `		}` |
|        6 |  7949 | `		goto Synchronize;` |
|        - |  7950 | `	}` |
|        - |  7951 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|        - |  7952 | `	 * constant initializer ("New expressions are not supported in this context").` |
|        - |  7953 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|   143873 |  7954 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|        5 |  7955 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  7956 | `			"New expressions are not supported in this context");` |
|        5 |  7957 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7958 | `			return SXERR_ABORT;` |
|        - |  7959 | `		}` |
|        5 |  7960 | `		goto Synchronize;` |
|        - |  7961 | `	}` |
|        - |  7962 | `	/* Allocate a new class attribute */` |
|   143869 |  7963 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   143869 |  7964 | `	if( pCons ){` |
|   143869 |  7965 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   143869 |  7966 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  7967 | `			return SXERR_ABORT;` |
|        - |  7968 | `		}` |
|    71932 |  7969 | `	}` |
|   143869 |  7970 | `	if( pCons == 0 ){` |
|      ! 0 |  7971 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  7972 | `		return SXERR_ABORT;` |
|        - |  7973 | `	}` |
|   143869 |  7974 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  7975 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  7976 | `	}` |
|        - |  7977 | `	/* Swap bytecode container */` |
|   143869 |  7978 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   143869 |  7979 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  7980 | `	/* Compile constant value.` |
|        - |  7981 | `	 */` |
|   143869 |  7982 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   143869 |  7983 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  7984 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  7985 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7986 | `			return SXERR_ABORT;` |
|        - |  7987 | `		}` |
|        1 |  7988 | `	}` |
|        - |  7989 | `	/* Emit the done instruction */` |
|   143869 |  7990 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   143869 |  7991 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   143869 |  7992 | `	if( rc == SXERR_ABORT ){` |
|        - |  7993 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  7994 | `		return SXERR_ABORT;` |
|        - |  7995 | `	}` |
|        - |  7996 | `	/* All done,install the constant */` |
|   143869 |  7997 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   143869 |  7998 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7999 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8000 | `		return SXERR_ABORT;` |
|        - |  8001 | `	}` |
|   143869 |  8002 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8003 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        3 |  8004 | `		pGen->pIn++; /* Jump the comma */` |
|        3 |  8005 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  8006 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8007 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8008 | `				pTok--;` |
|      ! 0 |  8009 | `			}` |
|      ! 0 |  8010 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8011 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  8012 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8013 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8014 | `				return SXERR_ABORT;` |
|        - |  8015 | `			}` |
|      ! 0 |  8016 | `		}else{` |
|        3 |  8017 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|        3 |  8018 | `				goto loop;` |
|        - |  8019 | `			}` |
|        - |  8020 | `		}` |
|      ! 0 |  8021 | `	}` |
|   143867 |  8022 | `	SySetRelease(&aUnionAlts);` |
|   143867 |  8023 | `	return SXRET_OK;` |
|        5 |  8024 | `Synchronize:` |
|       13 |  8025 | `	SySetRelease(&aUnionAlts);` |
|        - |  8026 | `	/* Synchronize with the first semi-colon */` |
|       45 |  8027 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  8028 | `		pGen->pIn++;` |
|        3 |  8029 | `	}` |
|       13 |  8030 | `	return SXERR_CORRUPT;` |
|    71941 |  8031 | `}` |
|        - |  8032 | `/*` |
|        - |  8033 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  8034 | ` * According to the PHP language reference manual` |
|        - |  8035 | ` *  Properties` |
|        - |  8036 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  8037 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  8038 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  8039 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  8040 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  8041 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  8042 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  8043 | ` * Symisc eXtension.` |
|        - |  8044 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  8045 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  8046 | ` *  Example:` |
|        - |  8047 | ` *   class Test{` |
|        - |  8048 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  8049 | ` *   };` |
|        - |  8050 | ` *   var_dump(TEST::myVar);` |
|        - |  8051 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  8052 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  8053 | ` */` |
|        - |  8054 | `/*` |
|        - |  8055 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  8056 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  8057 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  8058 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  8059 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  8060 | ` */` |
|  1310344 |  8061 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  8062 | `{` |
|  1310349 |  8063 | `	SyToken *p = pStart;` |
|  1310349 |  8064 | `	int bFirst = 1;` |
|  1310349 |  8065 | `	if( p >= pEnd ) return 0;` |
|        - |  8066 | ``	/* Optional nullable `?` shorthand. */`` |
|  1310349 |  8067 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       25 |  8068 | `		p++;` |
|       25 |  8069 | `		if( p >= pEnd ) return 0;` |
|       11 |  8070 | `	}` |
|        - |  8071 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  8072 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  8073 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  8074 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   655172 |  8075 | `	for(;;){` |
|  1310369 |  8076 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  8077 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  8078 | `			p++;` |
|        9 |  8079 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  8080 | `			if( p >= pEnd ) return 0;` |
|        3 |  8081 | `			p++; /* skip ')' */` |
|        2 |  8082 | `		}else{` |
|        - |  8083 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  8084 | ``			 * then any `&`-joined intersection members. */`` |
|  1310367 |  8085 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  1310367 |  8086 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  8087 | `				return 0;` |
|        - |  8088 | `			}` |
|        - |  8089 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  8090 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  8091 | `			 * may still appear at the initial dispatch site). */` |
|  1310367 |  8092 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  1310319 |  8093 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  1310314 |  8094 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    23588 |  8095 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  1310151 |  8096 | `					return 0;` |
|        - |  8097 | `				}` |
|       84 |  8098 | `			}` |
|      221 |  8099 | `			p++;` |
|      223 |  8100 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8101 | `				p += 2;` |
|        1 |  8102 | `			}` |
|      327 |  8103 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|      224 |  8104 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8105 | `				p++; /* skip '&' */` |
|        3 |  8106 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  8107 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  8108 | `				p++;` |
|        3 |  8109 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  8110 | `					p += 2;` |
|      ! 0 |  8111 | `				}` |
|        1 |  8112 | `			}` |
|        - |  8113 | `		}` |
|      223 |  8114 | `		bFirst = 0;` |
|      218 |  8115 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  8116 | `			&& p->sData.zString[0] == '\|' ){` |
|       25 |  8117 | ``			p++; /* next `\|`-separated part */`` |
|       25 |  8118 | `			continue;` |
|        - |  8119 | `		}` |
|      203 |  8120 | `		break;` |
|      ! 0 |  8121 | `	}` |
|      203 |  8122 | `	if( p >= pEnd ) return 0;` |
|      203 |  8123 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   655177 |  8124 | `}` |
|        - |  8125 |  |
|        - |  8126 | `/*` |
|        - |  8127 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - |  8128 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - |  8129 | ` * if not). Recognized forms:` |
|        - |  8130 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - |  8131 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - |  8132 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - |  8133 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - |  8134 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - |  8135 | ` * on unrecoverable error.` |
|        - |  8136 | ` *` |
|        - |  8137 | ` * When a type is parsed:` |
|        - |  8138 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - |  8139 | ` *   *pClass is set to the class name (for class types)` |
|        - |  8140 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - |  8141 | ` *   *pTypeText is set to the original text span of the type` |
|        - |  8142 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - |  8143 | ` */` |
|      198 |  8144 | `static sxi32 GenStateParsePropertyType(` |
|        - |  8145 | `	ph7_gen_state *pGen,` |
|        - |  8146 | `	sxu32 *pnType,` |
|        - |  8147 | `	SyString *pClass,` |
|        - |  8148 | `	sxi32 *piTypeFlags,` |
|        - |  8149 | `	SyString *pTypeText,` |
|        - |  8150 | `	SySet *pAlts` |
|        5 |  8151 | `){` |
|      203 |  8152 | `	sxi32 iFlags = 0;` |
|        - |  8153 | `	sxi32 rc;` |
|      203 |  8154 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  8155 | `		return SXRET_OK;` |
|        - |  8156 | `	}` |
|        - |  8157 | `	/* If the first token is '$', there's no type */` |
|      203 |  8158 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  8159 | `		return SXRET_OK;` |
|        - |  8160 | `	}` |
|      203 |  8161 | `	rc = GenStateParseUnionTypeDecl(` |
|       99 |  8162 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  8163 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  8164 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  8165 | `		/* bAllowVoid */ 0,` |
|      198 |  8166 | `		pGen->pIn->nLine);` |
|      203 |  8167 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8168 | `		return rc;` |
|        - |  8169 | `	}` |
|        - |  8170 | `	/* Verify next token is '$' (start of property name) */` |
|      203 |  8171 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8172 | `		return SXERR_SYNTAX;` |
|        - |  8173 | `	}` |
|      203 |  8174 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|      203 |  8175 | `	return SXRET_OK;` |
|      104 |  8176 | `}` |
|        - |  8177 |  |
|        - |  8178 | `/*` |
|        - |  8179 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - |  8180 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - |  8181 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - |  8182 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - |  8183 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - |  8184 | ` * by the type parser itself before reaching here.` |
|        - |  8185 | ` *` |
|        - |  8186 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - |  8187 | ` * use in the error message.` |
|        - |  8188 | ` */` |
|      366 |  8189 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  8190 | `	sxu32 nType,` |
|        - |  8191 | `	const SyString *pClass,` |
|        - |  8192 | `	const char **pzName,` |
|        - |  8193 | `	sxu32 *pnName)` |
|        5 |  8194 | `{` |
|        - |  8195 | `	const char *z;` |
|        - |  8196 | `	sxu32 n;` |
|      371 |  8197 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|      317 |  8198 | `		return 0;` |
|        - |  8199 | `	}` |
|       59 |  8200 | `	z = pClass->zString;` |
|       59 |  8201 | `	n = pClass->nByte;` |
|       59 |  8202 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 |  8203 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - |  8204 | `	}` |
|        - |  8205 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - |  8206 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - |  8207 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|       52 |  8208 | `	return 0;` |
|      188 |  8209 | `}` |
|        - |  8210 |  |
|        - |  8211 | `/*` |
|        - |  8212 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - |  8213 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - |  8214 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - |  8215 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - |  8216 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - |  8217 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - |  8218 | ` *` |
|        - |  8219 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - |  8220 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - |  8221 | ` */` |
|      304 |  8222 | `static sxi32 GenStateValidateMemberType(` |
|        - |  8223 | `	ph7_gen_state *pGen,` |
|        - |  8224 | `	ph7_class *pClass,` |
|        - |  8225 | `	const SyString *pMemberName,` |
|        - |  8226 | `	sxu32 nType,` |
|        - |  8227 | `	const SyString *pTypeClass,` |
|        - |  8228 | `	const SyString *pTypeText,` |
|        - |  8229 | `	SySet *pUnionAlts,` |
|        - |  8230 | `	const char *zErrFmt,` |
|        - |  8231 | `	sxu32 nLine)` |
|        5 |  8232 | `{` |
|      309 |  8233 | `	const char *zBad = 0;` |
|      309 |  8234 | `	sxu32 nBad = 0;` |
|        - |  8235 | `	SyString sFallback;` |
|        - |  8236 | `	const SyString *pBad;` |
|        - |  8237 | `	sxi32 rc;` |
|      309 |  8238 | `	int bDisallowed = 0;` |
|      309 |  8239 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  8240 | `		bDisallowed = 1;` |
|      307 |  8241 | `	}else if( pUnionAlts ){` |
|        - |  8242 | `		sxu32 i;` |
|       95 |  8243 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       67 |  8244 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       67 |  8245 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  8246 | `				bDisallowed = 1;` |
|        3 |  8247 | `				break;` |
|        - |  8248 | `			}` |
|       35 |  8249 | `		}` |
|       15 |  8250 | `	}` |
|      309 |  8251 | `	if( !bDisallowed ){` |
|      303 |  8252 | `		return SXRET_OK;` |
|        - |  8253 | `	}` |
|        - |  8254 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - |  8255 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - |  8256 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 |  8257 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 |  8258 | `		pBad = pTypeText;` |
|        5 |  8259 | `	}else{` |
|      ! 0 |  8260 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 |  8261 | `		pBad = &sFallback;` |
|        - |  8262 | `	}` |
|       11 |  8263 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  8264 | `		zErrFmt,` |
|        3 |  8265 | `		&pClass->sName,pMemberName,pBad);` |
|        8 |  8266 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  8267 | `		return SXERR_ABORT;` |
|        - |  8268 | `	}` |
|        8 |  8269 | `	return SXERR_SYNTAX;` |
|      157 |  8270 | `}` |
|        - |  8271 | `/*` |
|        - |  8272 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  8273 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  8274 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  8275 | ` * than promoted to a lexer keyword.` |
|        - |  8276 | ` */` |
| 10110234 |  8277 | `static int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  8278 | `{` |
| 10151362 |  8279 | `	return (pTok->nType & PH7_TK_ID)` |
|  5096240 |  8280 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 10151357 |  8281 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  8282 | `}` |
|   210544 |  8283 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  8284 | `{` |
|   210549 |  8285 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8286 | `	ph7_class_attr *pAttr;` |
|        - |  8287 | `	SyString *pName;` |
|        - |  8288 | `	sxi32 rc;` |
|   210549 |  8289 | `	sxu32 nType = 0;` |
|        - |  8290 | `	SyString sTypeClass;` |
|        - |  8291 | `	SyString sTypeText;` |
|        - |  8292 | `	SySet aUnionAlts;` |
|   210549 |  8293 | `	sxi32 iTypeFlags = 0;` |
|   210549 |  8294 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   210549 |  8295 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   210549 |  8296 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  8297 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  8298 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  8299 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   210549 |  8300 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  8301 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  8302 | `	}` |
|        - |  8303 | `	/* Extract visibility level */` |
|   210549 |  8304 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  8305 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   210648 |  8306 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      203 |  8307 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|      203 |  8308 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  8309 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  8310 | `			goto Synchronize;` |
|      203 |  8311 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  8312 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8313 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  8314 | `				&pGen->pIn->sData);` |
|      ! 0 |  8315 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8316 | `				return SXERR_ABORT;` |
|        - |  8317 | `			}` |
|      ! 0 |  8318 | `			goto Synchronize;` |
|      203 |  8319 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  8320 | `			return SXERR_ABORT;` |
|        - |  8321 | `		}` |
|       99 |  8322 | `	}` |
|      ! 0 |  8323 | `loop:` |
|   210553 |  8324 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8325 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 |  8326 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8327 | `			return SXERR_ABORT;` |
|        - |  8328 | `		}` |
|      ! 0 |  8329 | `		goto Synchronize;` |
|        - |  8330 | `	}` |
|   210553 |  8331 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   210553 |  8332 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - |  8333 | `		/* Invalid attribute name */` |
|      ! 0 |  8334 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 |  8335 | `		if( rc == SXERR_ABORT ){` |
|        - |  8336 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8337 | `			return SXERR_ABORT;` |
|        - |  8338 | `		}` |
|      ! 0 |  8339 | `		goto Synchronize;` |
|        - |  8340 | `	}` |
|        - |  8341 | `	/* Peek attribute name */` |
|   210553 |  8342 | `	pName = &pGen->pIn->sData;` |
|        - |  8343 | `	/* Advance the stream cursor */` |
|   210553 |  8344 | `	pGen->pIn++;` |
|   210553 |  8345 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|        - |  8346 | `		/* Invalid declaration */` |
|        3 |  8347 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|        3 |  8348 | `		if( rc == SXERR_ABORT ){` |
|        - |  8349 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8350 | `			return SXERR_ABORT;` |
|        - |  8351 | `		}` |
|        3 |  8352 | `		goto Synchronize;` |
|        - |  8353 | `	}` |
|        - |  8354 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - |  8355 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   210551 |  8356 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       41 |  8357 | `		const char *zRoErr = 0;` |
|       41 |  8358 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 |  8359 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       40 |  8360 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 |  8361 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       37 |  8362 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 |  8363 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 |  8364 | `		}` |
|       41 |  8365 | `		if( zRoErr ){` |
|       13 |  8366 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 |  8367 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8368 | `				return SXERR_ABORT;` |
|        - |  8369 | `			}` |
|       13 |  8370 | `			goto Synchronize;` |
|        - |  8371 | `		}` |
|       13 |  8372 | `	}` |
|        - |  8373 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - |  8374 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - |  8375 | `	 * by the type parser. */` |
|   210541 |  8376 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      299 |  8377 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - |  8378 | `			&sTypeText,` |
|      196 |  8379 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       98 |  8380 | `			"Property %z::$%z cannot have type %z",nLine);` |
|      201 |  8381 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8382 | `			return SXERR_ABORT;` |
|      201 |  8383 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  8384 | `			goto Synchronize;` |
|        - |  8385 | `		}` |
|       98 |  8386 | `	}` |
|        - |  8387 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   210541 |  8388 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 |  8389 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8390 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 |  8391 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8392 | `			return SXERR_ABORT;` |
|        - |  8393 | `		}` |
|        3 |  8394 | `		goto Synchronize;` |
|        - |  8395 | `	}` |
|        - |  8396 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - |  8397 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - |  8398 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - |  8399 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - |  8400 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - |  8401 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   210539 |  8402 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 |  8403 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8404 | `			"New expressions are not supported in this context");` |
|        6 |  8405 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8406 | `			return SXERR_ABORT;` |
|        - |  8407 | `		}` |
|        6 |  8408 | `		goto Synchronize;` |
|        - |  8409 | `	}` |
|        - |  8410 | `	/* Allocate a new class attribute */` |
|   210535 |  8411 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   210535 |  8412 | `	if( pAttr ){` |
|   210535 |  8413 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   210535 |  8414 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8415 | `			return SXERR_ABORT;` |
|        - |  8416 | `		}` |
|   105265 |  8417 | `	}` |
|   210535 |  8418 | `	if( pAttr == 0 ){` |
|      ! 0 |  8419 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  8420 | `		return SXERR_ABORT;` |
|        - |  8421 | `	}` |
|   210535 |  8422 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      199 |  8423 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       97 |  8424 | `	}` |
|   210535 |  8425 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - |  8426 | `		SySet *pInstrContainer;` |
|    86027 |  8427 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - |  8428 | `		/* Swap bytecode container */` |
|    86027 |  8429 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    86027 |  8430 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - |  8431 | `		/* Compile attribute value.` |
|        - |  8432 | `		 */` |
|    86027 |  8433 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    86027 |  8434 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  8435 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 |  8436 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8437 | `				return SXERR_ABORT;` |
|        - |  8438 | `			}` |
|      ! 0 |  8439 | `		}` |
|        - |  8440 | `		/* Emit the done instruction */` |
|    86027 |  8441 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    86027 |  8442 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    43011 |  8443 | `	}` |
|        - |  8444 | `	/* All done,install the attribute */` |
|   210535 |  8445 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   210535 |  8446 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8447 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8448 | `		return SXERR_ABORT;` |
|        - |  8449 | `	}` |
|   210535 |  8450 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8451 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 |  8452 | `		pGen->pIn++; /* Jump the comma */` |
|        5 |  8453 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  8454 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8455 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8456 | `				pTok--;` |
|      ! 0 |  8457 | `			}` |
|      ! 0 |  8458 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8459 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 |  8460 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8461 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8462 | `				return SXERR_ABORT;` |
|        - |  8463 | `			}` |
|      ! 0 |  8464 | `		}else{` |
|        5 |  8465 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 |  8466 | `				goto loop;` |
|        - |  8467 | `			}` |
|        - |  8468 | `		}` |
|      ! 0 |  8469 | `	}` |
|   210531 |  8470 | `	SySetRelease(&aUnionAlts);` |
|   210531 |  8471 | `	return SXRET_OK;` |
|        9 |  8472 | `Synchronize:` |
|        - |  8473 | `	/* Synchronize with the first semi-colon */` |
|       56 |  8474 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       37 |  8475 | `		pGen->pIn++;` |
|        3 |  8476 | `	}` |
|       22 |  8477 | `	SySetRelease(&aUnionAlts);` |
|       22 |  8478 | `	return SXERR_CORRUPT;` |
|   105277 |  8479 | `}` |
|        - |  8480 | `/*` |
|        - |  8481 | ` * Compile a class method.` |
|        - |  8482 | ` *` |
|        - |  8483 | ` * Refer to the official documentation for more information` |
|        - |  8484 | ` * on the powerful extension introduced by the PH7 engine` |
|        - |  8485 | ` * to the OO subsystem such as full type hinting,method` |
|        - |  8486 | ` * overloading and many more.` |
|        - |  8487 | ` */` |
|  1388054 |  8488 | `static sxi32 GenStateCompileClassMethod(` |
|        - |  8489 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  8490 | `	sxi32 iProtection,   /* Visibility level */` |
|        - |  8491 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - |  8492 | `	int doBody,          /* TRUE to process method body */` |
|        - |  8493 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - |  8494 | `	)` |
|        5 |  8495 | `{` |
|  1388059 |  8496 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1388059 |  8497 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - |  8498 | `	ph7_class_method *pMeth;` |
|        - |  8499 | `	sxi32 iFuncFlags;` |
|        - |  8500 | `	SyString *pName;` |
|        - |  8501 | `	SyToken *pEnd;` |
|        - |  8502 | `	sxi32 rc;` |
|        - |  8503 | `	/* Extract visibility level */` |
|  1388059 |  8504 | `	iProtection = GetProtectionLevel(iProtection);` |
|  1388059 |  8505 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  1388059 |  8506 | `	iFuncFlags = 0;` |
|  1388059 |  8507 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  8508 | `		/* Invalid method name */` |
|      ! 0 |  8509 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8510 | `		if( rc == SXERR_ABORT ){` |
|        - |  8511 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8512 | `			return SXERR_ABORT;` |
|        - |  8513 | `		}` |
|      ! 0 |  8514 | `		goto Synchronize;` |
|        - |  8515 | `	}` |
|  1388059 |  8516 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  8517 | `		/* Return by reference,remember that */` |
|      ! 0 |  8518 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  8519 | `		/* Jump the '&' token */` |
|      ! 0 |  8520 | `		pGen->pIn++;` |
|      ! 0 |  8521 | `	}` |
|  1388059 |  8522 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  8523 | `		/* Invalid method name */` |
|      ! 0 |  8524 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8525 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8526 | `			return SXERR_ABORT;` |
|        - |  8527 | `		}` |
|      ! 0 |  8528 | `		goto Synchronize;` |
|        - |  8529 | `	}` |
|        - |  8530 | `	/* Peek method name */` |
|  1388059 |  8531 | `	pName = &pGen->pIn->sData;` |
|  1388059 |  8532 | `	nLine = pGen->pIn->nLine;` |
|        - |  8533 | `	/* Jump the method name */` |
|  1388059 |  8534 | `	pGen->pIn++;` |
|  1388059 |  8535 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - |  8536 | `		/* Abstract method */` |
|   101051 |  8537 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 |  8538 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8539 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 |  8540 | `				&pClass->sName,pName);` |
|      ! 0 |  8541 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8542 | `				return SXERR_ABORT;` |
|        - |  8543 | `			}` |
|      ! 0 |  8544 | `		}` |
|        - |  8545 | `		/* Assemble method signature only */` |
|   101051 |  8546 | `		doBody = FALSE;` |
|    50523 |  8547 | `	}` |
|  1388059 |  8548 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  8549 | `		/* Syntax error */` |
|      ! 0 |  8550 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 |  8551 | `		if( rc == SXERR_ABORT ){` |
|        - |  8552 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8553 | `			return SXERR_ABORT;` |
|        - |  8554 | `		}` |
|      ! 0 |  8555 | `		goto Synchronize;` |
|        - |  8556 | `	}` |
|        - |  8557 | `	/* Allocate a new class_method instance */` |
|  1388059 |  8558 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  1388059 |  8559 | `	if( pMeth == 0 ){` |
|      ! 0 |  8560 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8561 | `		return SXERR_ABORT;` |
|        - |  8562 | `	}` |
|  1388059 |  8563 | `	pMeth->sFunc.nLine = nKwLine;` |
|  1388059 |  8564 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  1388059 |  8565 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8566 | `		return SXERR_ABORT;` |
|        - |  8567 | `	}` |
|        - |  8568 | `	/* Jump the left parenthesis '(' */` |
|  1388059 |  8569 | `	pGen->pIn++;` |
|  1388059 |  8570 | `	pEnd = 0; /* cc warning */` |
|        - |  8571 | `	/* Delimit the method signature */` |
|  1388059 |  8572 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1388059 |  8573 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8574 | `		/* Syntax error */` |
|        3 |  8575 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 |  8576 | `		if( rc == SXERR_ABORT ){` |
|        - |  8577 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8578 | `			return SXERR_ABORT;` |
|        - |  8579 | `		}` |
|        3 |  8580 | `		goto Synchronize;` |
|        - |  8581 | `	}` |
|        - |  8582 | `	{` |
|  1388057 |  8583 | `		int bIsCtor = 0;` |
|  1388057 |  8584 | `		int bAbstractCtor = 0;` |
|  1388052 |  8585 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   810691 |  8586 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  1335526 |  8587 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   105067 |  8588 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 |  8589 | `				bAbstractCtor = 1;` |
|        2 |  8590 | `			}else{` |
|   105065 |  8591 | `				bIsCtor = 1;` |
|        - |  8592 | `			}` |
|    52531 |  8593 | `		}` |
|  1388057 |  8594 | `		if( pGen->pIn < pEnd ){` |
|        - |  8595 | `			/* Collect method arguments */` |
|   389031 |  8596 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   389031 |  8597 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8598 | `				return SXERR_ABORT;` |
|        - |  8599 | `			}` |
|   194513 |  8600 | `		}` |
|        - |  8601 | `	}` |
|        - |  8602 | `	/* Point past ')' and parse optional return type ': type' */` |
|  1388057 |  8603 | `	pGen->pIn = &pEnd[1];` |
|        - |  8604 | `	{` |
|  1388057 |  8605 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  1388057 |  8606 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  8607 | `			return SXERR_ABORT;` |
|  1388057 |  8608 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 |  8609 | `			goto Synchronize;` |
|        - |  8610 | `		}` |
|        - |  8611 | `	}` |
|        - |  8612 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - |  8613 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - |  8614 | `	 * since we mint real ph7_class_attr entries. */` |
|        - |  8615 | `	{` |
|  1388057 |  8616 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - |  8617 | `		sxu32 i;` |
|  1971439 |  8618 | `		for( i = 0; i < nArg; i++ ){` |
|   583397 |  8619 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - |  8620 | `			ph7_class_attr *pAttr;` |
|   583397 |  8621 | `			sxi32 iAttrFlags = 0;` |
|        - |  8622 | `			int bArgTyped;` |
|   583397 |  8623 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   583321 |  8624 | `				continue;` |
|        - |  8625 | `			}` |
|        - |  8626 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - |  8627 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - |  8628 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       55 |  8629 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       82 |  8630 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       81 |  8631 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 |  8632 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8633 | `					"Cannot declare variadic promoted property");` |
|        3 |  8634 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8635 | `					return SXERR_ABORT;` |
|        - |  8636 | `				}` |
|        3 |  8637 | `				goto Synchronize;` |
|        - |  8638 | `			}` |
|        - |  8639 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - |  8640 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - |  8641 | `			 * appear as an alternative of a union type. */` |
|       79 |  8642 | `			if( bArgTyped ){` |
|      110 |  8643 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       70 |  8644 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       70 |  8645 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       35 |  8646 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       75 |  8647 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8648 | `					return SXERR_ABORT;` |
|       75 |  8649 | `				}else if( rc != SXRET_OK ){` |
|        6 |  8650 | `					goto Synchronize;` |
|        - |  8651 | `				}` |
|       33 |  8652 | `			}` |
|        - |  8653 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       75 |  8654 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 |  8655 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8656 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 |  8657 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8658 | `					return SXERR_ABORT;` |
|        - |  8659 | `				}` |
|        3 |  8660 | `				goto Synchronize;` |
|        - |  8661 | `			}` |
|       73 |  8662 | `			if( bArgTyped ){` |
|       69 |  8663 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       32 |  8664 | `			}` |
|       73 |  8665 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 |  8666 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 |  8667 | `			}` |
|       73 |  8668 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 |  8669 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 |  8670 | `			}` |
|       73 |  8671 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - |  8672 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - |  8673 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 |  8674 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 |  8675 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8676 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 |  8677 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  8678 | `						return SXERR_ABORT;` |
|        - |  8679 | `					}` |
|        3 |  8680 | `					goto Synchronize;` |
|        - |  8681 | `				}` |
|       24 |  8682 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 |  8683 | `			}` |
|       71 |  8684 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       71 |  8685 | `			if( pAttr == 0 ){` |
|      ! 0 |  8686 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8687 | `				return SXERR_ABORT;` |
|        - |  8688 | `			}` |
|       71 |  8689 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       69 |  8690 | `				pAttr->nType = pArg->nType;` |
|       69 |  8691 | `				pAttr->sClass = pArg->sClass;` |
|       69 |  8692 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       69 |  8693 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  8694 | `					sxu32 k;` |
|       20 |  8695 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 |  8696 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 |  8697 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 |  8698 | `					}` |
|        3 |  8699 | `				}` |
|       32 |  8700 | `			}` |
|       71 |  8701 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       71 |  8702 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8703 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8704 | `				return SXERR_ABORT;` |
|        - |  8705 | `			}` |
|       38 |  8706 | `		}` |
|        - |  8707 | `	}` |
|  1388047 |  8708 | `	if( doBody ){` |
|        - |  8709 | `		/* Compile method body */` |
|  1287001 |  8710 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  1287001 |  8711 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8712 | `			return SXERR_ABORT;` |
|        - |  8713 | `		}` |
|        - |  8714 | `		/* The cursor sits just past the body's closing brace */` |
|  1287001 |  8715 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   643503 |  8716 | `	}else{` |
|        - |  8717 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   101051 |  8718 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   101051 |  8719 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    50523 |  8720 | `		}` |
|        - |  8721 | `		/* Only method signature is allowed */` |
|   101051 |  8722 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 |  8723 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8724 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 |  8725 | `				if( rc == SXERR_ABORT ){` |
|        - |  8726 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8727 | `					return SXERR_ABORT;` |
|        - |  8728 | `				}` |
|      ! 0 |  8729 | `				return SXERR_CORRUPT;` |
|        - |  8730 | `			}` |
|        - |  8731 | `	}` |
|        - |  8732 | `	/* All done,install the method */` |
|  1388047 |  8733 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  1388047 |  8734 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8735 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8736 | `		return SXERR_ABORT;` |
|        - |  8737 | `	}` |
|  1388047 |  8738 | `	return SXRET_OK;` |
|        6 |  8739 | `Synchronize:` |
|        - |  8740 | `	/* Synchronize with the first semi-colon */` |
|       40 |  8741 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 |  8742 | `		pGen->pIn++;` |
|        4 |  8743 | `	}` |
|       16 |  8744 | `	return SXERR_CORRUPT;` |
|   694032 |  8745 | `}` |
|        - |  8746 | `/*` |
|        - |  8747 | ` * Compile an object interface.` |
|        - |  8748 | ` *  According to the PHP language reference manual` |
|        - |  8749 | ` *   Object Interfaces:` |
|        - |  8750 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - |  8751 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - |  8752 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  8753 | ` *   class, but without any of the methods having their contents defined.` |
|        - |  8754 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  8755 | ` */` |
|    46708 |  8756 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 |  8757 | `{` |
|    46713 |  8758 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8759 | `	ph7_class *pClass,*pBase;` |
|        - |  8760 | `	SyToken *pEnd,*pTmp;` |
|        - |  8761 | `	SyString *pName;` |
|        - |  8762 | `	sxi32 nKwrd;` |
|        - |  8763 | `	sxi32 rc;` |
|        - |  8764 | `	/* Jump the 'interface' keyword */` |
|    46713 |  8765 | `	pGen->pIn++;` |
|        - |  8766 | `	/* Extract interface name */` |
|    46713 |  8767 | `	pName = &pGen->pIn->sData;` |
|        - |  8768 | `	/* Advance the stream cursor */` |
|    46713 |  8769 | `	pGen->pIn++;` |
|        - |  8770 | `	/* Build FQN and obtain a raw class */ {` |
|        - |  8771 | `		SyBlob sFQN;` |
|        - |  8772 | `		SyString sFQNStr;` |
|    46713 |  8773 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    46713 |  8774 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    46713 |  8775 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    46713 |  8776 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    46713 |  8777 | `		SyBlobRelease(&sFQN);` |
|        - |  8778 | `	}` |
|    46713 |  8779 | `	if( pClass == 0 ){` |
|      ! 0 |  8780 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8781 | `		return SXERR_ABORT;` |
|        - |  8782 | `	}` |
|    46713 |  8783 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    46713 |  8784 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8785 | `		return SXERR_ABORT;` |
|        - |  8786 | `	}` |
|        - |  8787 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    46713 |  8788 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - |  8789 | `	/* Assume no base class is given */` |
|    46713 |  8790 | `	pBase = 0;` |
|    46713 |  8791 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    15551 |  8792 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    15551 |  8793 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|        - |  8794 | `			SyBlob sResolved;` |
|        - |  8795 | `			SyString sBaseName;` |
|        - |  8796 | `			sxu32 nRefLine;` |
|        - |  8797 | `			/* Extract base interface */` |
|    15551 |  8798 | `			pGen->pIn++;` |
|    15551 |  8799 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    15551 |  8800 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15551 |  8801 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  8802 | `				SyBlobRelease(&sResolved);` |
|      ! 0 |  8803 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8804 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 |  8805 | `					pName);` |
|      ! 0 |  8806 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8807 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8808 | `					return SXERR_ABORT;` |
|        - |  8809 | `				}` |
|      ! 0 |  8810 | `				return SXRET_OK;` |
|        - |  8811 | `			}` |
|    23324 |  8812 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    15546 |  8813 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15551 |  8814 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  8815 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  8816 | `			/* Only interfaces is allowed */` |
|    15551 |  8817 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  8818 | `				pBase = pBase->pNextName;` |
|      ! 0 |  8819 | `			}` |
|    15551 |  8820 | `			if( pBase == 0 ){` |
|      ! 0 |  8821 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  8822 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 |  8823 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8824 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  8825 | `					return SXERR_ABORT;` |
|        - |  8826 | `				}` |
|      ! 0 |  8827 | `			}` |
|    15551 |  8828 | `			SyBlobRelease(&sResolved);` |
|     7773 |  8829 | `		}` |
|     7773 |  8830 | `	}` |
|    46713 |  8831 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  8832 | `		/* Syntax error */` |
|      ! 0 |  8833 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 |  8834 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8835 | `		if( rc == SXERR_ABORT ){` |
|        - |  8836 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8837 | `			return SXERR_ABORT;` |
|        - |  8838 | `		}` |
|      ! 0 |  8839 | `		return SXRET_OK;` |
|        - |  8840 | `	}` |
|    46713 |  8841 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    46713 |  8842 | `	pEnd = 0; /* cc warning */` |
|        - |  8843 | `	/* Delimit the interface body */` |
|    46713 |  8844 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    46713 |  8845 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8846 | `		/* Syntax error */` |
|      ! 0 |  8847 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 |  8848 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8849 | `		if( rc == SXERR_ABORT ){` |
|        - |  8850 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8851 | `			return SXERR_ABORT;` |
|        - |  8852 | `		}` |
|      ! 0 |  8853 | `		return SXRET_OK;` |
|        - |  8854 | `	}` |
|        - |  8855 | `	/* The delimiter token is the interface body's closing brace */` |
|    46713 |  8856 | `	pClass->nEndLine = pEnd->nLine;` |
|        - |  8857 | `	/* Swap token stream */` |
|    46713 |  8858 | `	pTmp = pGen->pEnd;` |
|    46713 |  8859 | `	pGen->pEnd = pEnd;` |
|        - |  8860 | `	/* Start the parse process` |
|        - |  8861 | `	 * Note (According to the PHP reference manual):` |
|        - |  8862 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - |  8863 | `	 *  Only 'public' visibility is allowed.` |
|        - |  8864 | `	 */` |
|    73875 |  8865 | `	for(;;){` |
|        - |  8866 | `		/* Jump leading/trailing semi-colons */` |
|   248797 |  8867 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   101047 |  8868 | `			pGen->pIn++;` |
|        5 |  8869 | `		}` |
|   147755 |  8870 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  8871 | `			/* End of interface body */` |
|    46709 |  8872 | `			break;` |
|        - |  8873 | `		}` |
|        - |  8874 | `		/* Bind a directly-preceding docblock to this member */` |
|   101051 |  8875 | `		GenStateSetPendingDoc(&(*pGen));` |
|   101051 |  8876 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  8877 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8878 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 |  8879 | `				&pGen->pIn->sData,pName);` |
|      ! 0 |  8880 | `			if( rc == SXERR_ABORT ){` |
|        - |  8881 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  8882 | `				return SXERR_ABORT;` |
|        - |  8883 | `			}` |
|      ! 0 |  8884 | `			goto done;` |
|        - |  8885 | `		}` |
|        - |  8886 | `		/* Extract the current keyword */` |
|   101051 |  8887 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101051 |  8888 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - |  8889 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - |  8890 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 |  8891 | `			const char *zKind = "member";` |
|        3 |  8892 | `			SyString *pMemberName = 0;` |
|        3 |  8893 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 |  8894 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 |  8895 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 |  8896 | `					zKind = "constant";` |
|        3 |  8897 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 |  8898 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 |  8899 | `					}` |
|        1 |  8900 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  8901 | `					zKind = "method";` |
|      ! 0 |  8902 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 |  8903 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 |  8904 | `					}` |
|      ! 0 |  8905 | `				}` |
|        1 |  8906 | `			}` |
|        3 |  8907 | `			if( pMemberName ){` |
|        4 |  8908 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 |  8909 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 |  8910 | `			}else{` |
|      ! 0 |  8911 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8912 | `					"Access type for interface %s must be public",zKind);` |
|        - |  8913 | `			}` |
|        3 |  8914 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8915 | `				return SXERR_ABORT;` |
|        - |  8916 | `			}` |
|        3 |  8917 | `			goto done;` |
|        - |  8918 | `		}` |
|   101049 |  8919 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  8920 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8921 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  8922 | `			if( rc == SXERR_ABORT ){` |
|        - |  8923 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  8924 | `				return SXERR_ABORT;` |
|        - |  8925 | `			}` |
|      ! 0 |  8926 | `			goto done;` |
|        - |  8927 | `		}` |
|   101049 |  8928 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - |  8929 | `			/* Advance the stream cursor */` |
|   101031 |  8930 | `			pGen->pIn++;` |
|   101031 |  8931 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  8932 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8933 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  8934 | `				if( rc == SXERR_ABORT ){` |
|        - |  8935 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8936 | `					return SXERR_ABORT;` |
|        - |  8937 | `				}` |
|      ! 0 |  8938 | `				goto done;` |
|        - |  8939 | `			}` |
|   101031 |  8940 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101031 |  8941 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  8942 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8943 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  8944 | `				if( rc == SXERR_ABORT ){` |
|        - |  8945 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8946 | `					return SXERR_ABORT;` |
|        - |  8947 | `				}` |
|      ! 0 |  8948 | `				goto done;` |
|        - |  8949 | `			}` |
|    50513 |  8950 | `		}` |
|   101049 |  8951 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - |  8952 | `			/* Parse constant */` |
|       16 |  8953 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       16 |  8954 | `			if( rc != SXRET_OK ){` |
|        3 |  8955 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8956 | `					return SXERR_ABORT;` |
|        - |  8957 | `				}` |
|        3 |  8958 | `				goto done;` |
|        - |  8959 | `			}` |
|        7 |  8960 | `		}else{` |
|   101035 |  8961 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   101035 |  8962 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - |  8963 | `				/* Static method,record that */` |
|    11657 |  8964 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - |  8965 | `				/* Advance the stream cursor */` |
|    11657 |  8966 | `				pGen->pIn++;` |
|    11652 |  8967 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11657 |  8968 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  8969 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8970 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  8971 | `						if( rc == SXERR_ABORT ){` |
|        - |  8972 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 |  8973 | `							return SXERR_ABORT;` |
|        - |  8974 | `						}` |
|      ! 0 |  8975 | `						goto done;` |
|        - |  8976 | `				}` |
|     5826 |  8977 | `			}` |
|        - |  8978 | `			/* Process method signature (no body for interface methods) */` |
|   101035 |  8979 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   101035 |  8980 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8981 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8982 | `					return SXERR_ABORT;` |
|        - |  8983 | `				}` |
|      ! 0 |  8984 | `				goto done;` |
|        - |  8985 | `			}` |
|        - |  8986 | `		}` |
|        5 |  8987 | `	}` |
|        - |  8988 | `	/* Install the interface */` |
|    46709 |  8989 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    46709 |  8990 | `	if( rc == SXRET_OK && pBase ){` |
|        - |  8991 | `		/* Inherit from the base interface */` |
|    15551 |  8992 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     7773 |  8993 | `	}` |
|    46709 |  8994 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8995 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8996 | `		return SXERR_ABORT;` |
|        - |  8997 | `	}` |
|    23352 |  8998 | `done:` |
|        - |  8999 | `	/* Point beyond the interface body */` |
|    46713 |  9000 | `	pGen->pIn  = &pEnd[1];` |
|    46713 |  9001 | `	pGen->pEnd = pTmp;` |
|    46713 |  9002 | `	return PH7_OK;` |
|    23359 |  9003 | `}` |
|        - |  9004 | `/*` |
|        - |  9005 | ` * Compile a user-defined class.` |
|        - |  9006 | ` * According to the PHP language reference manual` |
|        - |  9007 | ` *  class` |
|        - |  9008 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - |  9009 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - |  9010 | ` *  of the properties and methods belonging to the class.` |
|        - |  9011 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - |  9012 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - |  9013 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - |  9014 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  9015 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - |  9016 | ` *  (called "methods").` |
|        - |  9017 | ` */` |
|        - |  9018 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - |  9019 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - |  9020 | `struct TraitUseEntry {` |
|        - |  9021 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - |  9022 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - |  9023 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - |  9024 | `};` |
|        - |  9025 | `/*` |
|        - |  9026 | ` * Validate that methods implementing interface contracts have compatible` |
|        - |  9027 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - |  9028 | ` */` |
|   215144 |  9029 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9030 | `{` |
|        - |  9031 | `	ph7_class **apIface;` |
|        - |  9032 | `	sxu32 nIface,i;` |
|        - |  9033 | `	sxi32 rc;` |
|   215149 |  9034 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 |  9035 | `		return SXRET_OK;` |
|        - |  9036 | `	}` |
|   215149 |  9037 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   215149 |  9038 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   429093 |  9039 | `	for(i = 0; i < nIface; i++){` |
|   213949 |  9040 | `		ph7_class *pIface = apIface[i];` |
|        - |  9041 | `		SyHashEntry *pEntry;` |
|   213949 |  9042 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   498051 |  9043 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   284107 |  9044 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |  9045 | `			ph7_class_method *pImplMeth;` |
|   284107 |  9046 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - |  9047 | `			/* Find the implementing method in the class */` |
|   284107 |  9048 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   284107 |  9049 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       18 |  9050 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - |  9051 | `			}` |
|        - |  9052 | `			/* Check visibility: interface methods must be implemented as public */` |
|   284093 |  9053 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 |  9054 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9055 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 |  9056 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 |  9057 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9058 | `					return SXERR_ABORT;` |
|        - |  9059 | `				}` |
|        1 |  9060 | `			}` |
|        - |  9061 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - |  9062 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - |  9063 | `			 */` |
|        - |  9064 | `			{` |
|   284093 |  9065 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   284093 |  9066 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   284093 |  9067 | `				int sigError = 0;` |
|   284093 |  9068 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 |  9069 | `					sigError = 1;` |
|   284092 |  9070 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - |  9071 | `					/* Extra parameters must all have default values */` |
|        6 |  9072 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - |  9073 | `					sxu32 k;` |
|        8 |  9074 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|        6 |  9075 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 |  9076 | `							sigError = 1;` |
|        3 |  9077 | `							break;` |
|        - |  9078 | `						}` |
|        2 |  9079 | `					}` |
|        2 |  9080 | `				}` |
|   284093 |  9081 | `				if( sigError ){` |
|        - |  9082 | `					SyBlob sImplSig, sIfaceSig;` |
|        - |  9083 | `					ph7_vm_func_arg *aArgs;` |
|        - |  9084 | `					sxu32 j;` |
|        6 |  9085 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 |  9086 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - |  9087 | `					/* Build implementing method signature */` |
|        6 |  9088 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 |  9089 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 |  9090 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 |  9091 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 |  9092 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9093 | `					}` |
|        - |  9094 | `					/* Build interface method signature */` |
|        6 |  9095 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 |  9096 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 |  9097 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 |  9098 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 |  9099 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9100 | `					}` |
|        8 |  9101 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9102 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 |  9103 | `						&pClass->sName,pMName,` |
|        4 |  9104 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 |  9105 | `						&pIface->sName,pMName,` |
|        4 |  9106 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 |  9107 | `					SyBlobRelease(&sImplSig);` |
|        6 |  9108 | `					SyBlobRelease(&sIfaceSig);` |
|        6 |  9109 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9110 | `						return SXERR_ABORT;` |
|        - |  9111 | `					}` |
|        2 |  9112 | `				}` |
|        - |  9113 | `			}` |
|        5 |  9114 | `		}` |
|   106977 |  9115 | `	}` |
|   215149 |  9116 | `	return SXRET_OK;` |
|   107577 |  9117 | `}` |
|        - |  9118 | `/*` |
|        - |  9119 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - |  9120 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - |  9121 | ` */` |
|   215144 |  9122 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9123 | `{` |
|        - |  9124 | `	ph7_class_method *pMeth;` |
|        - |  9125 | `	SyHashEntry *pEntry;` |
|        - |  9126 | `	sxu32 nAbstract;` |
|        - |  9127 | `	SyBlob sMsg;` |
|        - |  9128 | `	sxi32 rc;` |
|        - |  9129 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   215149 |  9130 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     7809 |  9131 | `		return SXRET_OK;` |
|        - |  9132 | `	}` |
|        - |  9133 | `	/* Count abstract methods */` |
|   207345 |  9134 | `	nAbstract = 0;` |
|   207345 |  9135 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  3068033 |  9136 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  2860693 |  9137 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  2860693 |  9138 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       20 |  9139 | `			nAbstract++;` |
|        8 |  9140 | `		}` |
|        5 |  9141 | `	}` |
|   207345 |  9142 | `	if( nAbstract == 0 ){` |
|   207331 |  9143 | `		return SXRET_OK;` |
|        - |  9144 | `	}` |
|        - |  9145 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 |  9146 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 |  9147 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - |  9148 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 |  9149 | `		&pClass->sName,nAbstract,` |
|        7 |  9150 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 |  9151 | `		(nAbstract > 1 ? "s" : ""));` |
|        - |  9152 | `	/* Second pass: list methods with origins */` |
|        - |  9153 | `	{` |
|       18 |  9154 | `		sxu32 nListed = 0;` |
|       18 |  9155 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 |  9156 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 |  9157 | `			ph7_class *pOrigin = 0;` |
|        - |  9158 | `			SyString *pMName;` |
|       22 |  9159 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 |  9160 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 |  9161 | `				continue;` |
|        - |  9162 | `			}` |
|       20 |  9163 | `			pMName = &pMeth->sFunc.sName;` |
|       20 |  9164 | `			if( nListed > 0 ){` |
|        3 |  9165 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 |  9166 | `			}` |
|        - |  9167 | `			/* Find the origin of this abstract method.` |
|        - |  9168 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - |  9169 | `			 * inheritance chains) take precedence for interface-declared` |
|        - |  9170 | `			 * methods. Abstract class methods only win when the class` |
|        - |  9171 | `			 * itself declared the abstract method (not inherited from` |
|        - |  9172 | `			 * an interface). Trait methods are adopted into the using` |
|        - |  9173 | `			 * class's namespace.` |
|        - |  9174 | `			 */` |
|        - |  9175 | `			{` |
|        - |  9176 | `				ph7_class **apIface;` |
|        - |  9177 | `				ph7_class **apTrait;` |
|        - |  9178 | `				ph7_class *pWalk;` |
|        - |  9179 | `				sxu32 i;` |
|        - |  9180 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - |  9181 | `				 * (one that was written in the class body, not inherited from an` |
|        - |  9182 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - |  9183 | `				 */` |
|       20 |  9184 | `				if( pClass->pBase ){` |
|       11 |  9185 | `					pWalk = pClass->pBase;` |
|       19 |  9186 | `					while( pWalk ){` |
|        - |  9187 | `						ph7_class_method *pParentMeth;` |
|       13 |  9188 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       13 |  9189 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - |  9190 | `							/* Exclude methods that came from an interface anywhere` |
|        - |  9191 | `							 * in this class's ancestor chain.` |
|        - |  9192 | `							 */` |
|       13 |  9193 | `							int fromIface = 0;` |
|       13 |  9194 | `							ph7_class *pAnc = pWalk;` |
|       17 |  9195 | `							while( pAnc ){` |
|        - |  9196 | `								ph7_class **apPI;` |
|        - |  9197 | `								sxu32 j;` |
|       15 |  9198 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       15 |  9199 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 |  9200 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 |  9201 | `										fromIface = 1;` |
|       10 |  9202 | `										break;` |
|        - |  9203 | `									}` |
|      ! 0 |  9204 | `								}` |
|       15 |  9205 | `								if( fromIface ) break;` |
|        6 |  9206 | `								pAnc = pAnc->pBase;` |
|        2 |  9207 | `							}` |
|       13 |  9208 | `							if( !fromIface ){` |
|        3 |  9209 | `								pOrigin = pWalk;` |
|        3 |  9210 | `								break;` |
|        - |  9211 | `							}` |
|        4 |  9212 | `						}` |
|       10 |  9213 | `						pWalk = pWalk->pBase;` |
|        2 |  9214 | `					}` |
|        4 |  9215 | `				}` |
|        - |  9216 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - |  9217 | `				 * each interface's own parent chain for the deepest origin.` |
|        - |  9218 | `				 */` |
|       20 |  9219 | `				if( !pOrigin ){` |
|       18 |  9220 | `					pWalk = pClass;` |
|       40 |  9221 | `					while( pWalk && !pOrigin ){` |
|       26 |  9222 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 |  9223 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 |  9224 | `							ph7_class *pIface = apIface[i];` |
|       16 |  9225 | `							ph7_class *pDeepest = 0;` |
|       28 |  9226 | `							while( pIface ){` |
|       16 |  9227 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 |  9228 | `									pDeepest = pIface;` |
|        6 |  9229 | `								}` |
|       16 |  9230 | `								pIface = pIface->pBase;` |
|        4 |  9231 | `							}` |
|       16 |  9232 | `							if( pDeepest ){` |
|       16 |  9233 | `								pOrigin = pDeepest;` |
|       16 |  9234 | `								break;` |
|        - |  9235 | `							}` |
|      ! 0 |  9236 | `						}` |
|       26 |  9237 | `						pWalk = pWalk->pBase;` |
|        4 |  9238 | `					}` |
|        7 |  9239 | `				}` |
|        - |  9240 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 |  9241 | `				if( !pOrigin ){` |
|        3 |  9242 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 |  9243 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 |  9244 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 |  9245 | `							pOrigin = pClass;` |
|        3 |  9246 | `							break;` |
|        - |  9247 | `						}` |
|      ! 0 |  9248 | `					}` |
|        1 |  9249 | `				}` |
|        - |  9250 | `			}` |
|       20 |  9251 | `			if( pOrigin ){` |
|       20 |  9252 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       12 |  9253 | `			}else{` |
|        - |  9254 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 |  9255 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|        - |  9256 | `			}` |
|       20 |  9257 | `			nListed++;` |
|        4 |  9258 | `		}` |
|        - |  9259 | `	}` |
|       18 |  9260 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 |  9261 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 |  9262 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 |  9263 | `	SyBlobRelease(&sMsg);` |
|       18 |  9264 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  9265 | `		return SXERR_ABORT;` |
|        - |  9266 | `	}` |
|       18 |  9267 | `	return SXRET_OK;` |
|   107577 |  9268 | `}` |
|        - |  9269 | `/*` |
|        - |  9270 | ` * Parse a class/interface name reference from the current token stream.` |
|        - |  9271 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - |  9272 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - |  9273 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - |  9274 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - |  9275 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - |  9276 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - |  9277 | ` */` |
|   192130 |  9278 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 |  9279 | `{` |
|   192135 |  9280 | `	int isAbsolute = 0;` |
|   192135 |  9281 | `	SyToken *pStart = pGen->pIn;` |
|        - |  9282 | `	SyBlob sName;` |
|   192135 |  9283 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4473 |  9284 | `		isAbsolute = 1;` |
|     4473 |  9285 | `		pGen->pIn++;` |
|     2234 |  9286 | `	}` |
|   192135 |  9287 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        8 |  9288 | `		pGen->pIn = pStart;` |
|        8 |  9289 | `		return SXERR_INVALID;` |
|        - |  9290 | `	}` |
|   192129 |  9291 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   192129 |  9292 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   192129 |  9293 | `	pGen->pIn++;` |
|   288207 |  9294 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    96088 |  9295 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       16 |  9296 | `		SyBlobAppend(&sName,"\\",1);` |
|       16 |  9297 | `		pGen->pIn++;` |
|       16 |  9298 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       16 |  9299 | `		pGen->pIn++;` |
|        2 |  9300 | `	}` |
|   192129 |  9301 | `	if( isAbsolute ){` |
|     4471 |  9302 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2238 |  9303 | `	}else{` |
|        - |  9304 | `		SyString sRaw;` |
|   187663 |  9305 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   187663 |  9306 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - |  9307 | `	}` |
|   192129 |  9308 | `	SyBlobRelease(&sName);` |
|   192129 |  9309 | `	return SXRET_OK;` |
|    96070 |  9310 | `}` |
|        - |  9311 | `/*` |
|        - |  9312 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - |  9313 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - |  9314 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - |  9315 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - |  9316 | ` * either direction cannot run unbounded.` |
|        - |  9317 | ` */` |
|        - |  9318 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    46804 |  9319 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 |  9320 | `{` |
|        - |  9321 | `	ph7_class **apParent;` |
|        - |  9322 | `	sxu32 n;` |
|   120839 |  9323 | `	while( pInterface ){` |
|    81813 |  9324 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 |  9325 | `			return FALSE;` |
|        - |  9326 | `		}` |
|   101252 |  9327 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    38878 |  9328 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7783 |  9329 | `			return TRUE;` |
|        - |  9330 | `		}` |
|    74035 |  9331 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    74035 |  9332 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|      ! 0 |  9333 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 |  9334 | `				return TRUE;` |
|        - |  9335 | `			}` |
|      ! 0 |  9336 | `		}` |
|    74035 |  9337 | `		pInterface = pInterface->pBase;` |
|    74035 |  9338 | `		iDepth++;` |
|        5 |  9339 | `	}` |
|    39031 |  9340 | `	return FALSE;` |
|    23407 |  9341 | `}` |
|    46804 |  9342 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 |  9343 | `{` |
|    46809 |  9344 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 |  9345 | `}` |
|        - |  9346 | `/*` |
|        - |  9347 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - |  9348 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - |  9349 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - |  9350 | ` */` |
|     7778 |  9351 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 |  9352 | `{` |
|     7787 |  9353 | `	while( pBase ){` |
|       10 |  9354 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 |  9355 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 |  9356 | `			return TRUE;` |
|        - |  9357 | `		}` |
|       10 |  9358 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 |  9359 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 |  9360 | `			return TRUE;` |
|        - |  9361 | `		}` |
|        5 |  9362 | `		pBase = pBase->pBase;` |
|        1 |  9363 | `	}` |
|     7779 |  9364 | `	return FALSE;` |
|     3894 |  9365 | `}` |
|        - |  9366 | `/*` |
|        - |  9367 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - |  9368 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - |  9369 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - |  9370 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - |  9371 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - |  9372 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - |  9373 | ` * pClass->aEnumCases for cases().` |
|        - |  9374 | ` */` |
|       40 |  9375 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9376 | `{` |
|       45 |  9377 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9378 | `	SySet *pInstrContainer;` |
|        - |  9379 | `	ph7_class_attr *pCase;` |
|        - |  9380 | `	SyString *pName;` |
|        - |  9381 | `	sxi32 rc;` |
|       45 |  9382 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|       45 |  9383 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  9384 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9385 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|      ! 0 |  9386 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9387 | `			return SXERR_ABORT;` |
|        - |  9388 | `		}` |
|      ! 0 |  9389 | `		goto Synchronize;` |
|        - |  9390 | `	}` |
|       45 |  9391 | `	pName = &pGen->pIn->sData;` |
|        - |  9392 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|       45 |  9393 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 |  9394 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9395 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 |  9396 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9397 | `			return SXERR_ABORT;` |
|        - |  9398 | `		}` |
|      ! 0 |  9399 | `		goto Synchronize;` |
|        - |  9400 | `	}` |
|       45 |  9401 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9402 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|       45 |  9403 | `	if( pCase == 0 ){` |
|      ! 0 |  9404 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9405 | `		return SXERR_ABORT;` |
|        - |  9406 | `	}` |
|       45 |  9407 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|       45 |  9408 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9409 | `		return SXERR_ABORT;` |
|        - |  9410 | `	}` |
|       45 |  9411 | `	pGen->pIn++; /* Jump the case name */` |
|       45 |  9412 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|       31 |  9413 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 |  9414 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 |  9415 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|        6 |  9416 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9417 | `				return SXERR_ABORT;` |
|        - |  9418 | `			}` |
|        6 |  9419 | `			goto Synchronize;` |
|        - |  9420 | `		}` |
|       25 |  9421 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - |  9422 | `		/* Compile the backing value expression into the case's own container` |
|        - |  9423 | `		 * (same technique as class constants). */` |
|       25 |  9424 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       25 |  9425 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|       25 |  9426 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       25 |  9427 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  9428 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9429 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|      ! 0 |  9430 | `		}` |
|       25 |  9431 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|       25 |  9432 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       25 |  9433 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9434 | `			return SXERR_ABORT;` |
|        - |  9435 | `		}` |
|       13 |  9436 | `	}else{` |
|       15 |  9437 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 |  9438 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9439 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|      ! 0 |  9440 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9441 | `				return SXERR_ABORT;` |
|        - |  9442 | `			}` |
|      ! 0 |  9443 | `			goto Synchronize;` |
|        - |  9444 | `		}` |
|        - |  9445 | `	}` |
|       39 |  9446 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|       39 |  9447 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9448 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9449 | `		return SXERR_ABORT;` |
|        - |  9450 | `	}` |
|       39 |  9451 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|       39 |  9452 | `	return SXRET_OK;` |
|        2 |  9453 | `Synchronize:` |
|        - |  9454 | `	/* Synchronize with the first semi-colon */` |
|       14 |  9455 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       10 |  9456 | `		pGen->pIn++;` |
|        2 |  9457 | `	}` |
|        6 |  9458 | `	return SXERR_CORRUPT;` |
|       25 |  9459 | `}` |
|        - |  9460 | `/*` |
|        - |  9461 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|        - |  9462 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|        - |  9463 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|        - |  9464 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|        - |  9465 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|        - |  9466 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|        - |  9467 | ` * pointers into it (see the constructor-promotion precedent above).` |
|        - |  9468 | ` */` |
|       22 |  9469 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        3 |  9470 | `{` |
|        - |  9471 | `	SyToken *pSaveIn,*pSaveEnd;` |
|        - |  9472 | `	const char *zBack;` |
|        - |  9473 | `	SySet sToken;` |
|        - |  9474 | `	char *zSrc;` |
|        - |  9475 | `	sxu32 nSrc,nMax;` |
|       25 |  9476 | `	sxi32 rc = SXRET_OK;` |
|       25 |  9477 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|       22 |  9478 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|       25 |  9479 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|       25 |  9480 | `	if( zSrc == 0 ){` |
|      ! 0 |  9481 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9482 | `		return SXERR_ABORT;` |
|        - |  9483 | `	}` |
|       25 |  9484 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|       25 |  9485 | `	if( pClass->nEnumBacking != 0 ){` |
|       19 |  9486 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        - |  9487 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|        - |  9488 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|        - |  9489 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|        6 |  9490 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|        7 |  9491 | `	}else{` |
|       18 |  9492 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        5 |  9493 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|        - |  9494 | `	}` |
|       25 |  9495 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       25 |  9496 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|       25 |  9497 | `	pSaveIn = pGen->pIn;` |
|       25 |  9498 | `	pSaveEnd = pGen->pEnd;` |
|       25 |  9499 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       25 |  9500 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       71 |  9501 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|       49 |  9502 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|        3 |  9503 | `	}` |
|       25 |  9504 | `	pGen->pIn = pSaveIn;` |
|       25 |  9505 | `	pGen->pEnd = pSaveEnd;` |
|       25 |  9506 | `	SySetRelease(&sToken);` |
|       25 |  9507 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|       14 |  9508 | `}` |
|        - |  9509 | `/*` |
|        - |  9510 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - |  9511 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - |  9512 | ` */` |
|        - |  9513 | `static const char *azEnumBannedMagic[] = {` |
|        - |  9514 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - |  9515 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - |  9516 | `};` |
|        - |  9517 | `/*` |
|        - |  9518 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - |  9519 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - |  9520 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - |  9521 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - |  9522 | ` * and before the class is installed.` |
|        - |  9523 | ` */` |
|       22 |  9524 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        3 |  9525 | `{` |
|        - |  9526 | `	SyHashEntry *pEntry;` |
|        - |  9527 | `	sxi32 rc;` |
|        - |  9528 | `	sxu32 n;` |
|        - |  9529 | `	/* php: "Enum %s cannot include properties" */` |
|       25 |  9530 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|       65 |  9531 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|       45 |  9532 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       45 |  9533 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 |  9534 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 |  9535 | `				"Enum %z cannot include properties",&pClass->sName);` |
|        3 |  9536 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9537 | `				return SXERR_ABORT;` |
|        - |  9538 | `			}` |
|        3 |  9539 | `			break;` |
|        - |  9540 | `		}` |
|        2 |  9541 | `	}` |
|        - |  9542 | `	/* php: "Enum %s cannot include magic method %s" */` |
|      311 |  9543 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|      429 |  9544 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|      289 |  9545 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 |  9546 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9547 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|      ! 0 |  9548 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9549 | `				return SXERR_ABORT;` |
|        - |  9550 | `			}` |
|      ! 0 |  9551 | `		}` |
|      146 |  9552 | `	}` |
|        - |  9553 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - |  9554 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - |  9555 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - |  9556 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - |  9557 | `	{` |
|        - |  9558 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - |  9559 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - |  9560 | `		ph7_class_attr *pAttr;` |
|       25 |  9561 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9562 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|       25 |  9563 | `		if( pAttr == 0 ){` |
|      ! 0 |  9564 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9565 | `			return SXERR_ABORT;` |
|        - |  9566 | `		}` |
|       25 |  9567 | `		pAttr->nType = MEMOBJ_STRING;` |
|       25 |  9568 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|       25 |  9569 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|       25 |  9570 | `		if( pClass->nEnumBacking != 0 ){` |
|       13 |  9571 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9572 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|       13 |  9573 | `			if( pAttr == 0 ){` |
|      ! 0 |  9574 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9575 | `				return SXERR_ABORT;` |
|        - |  9576 | `			}` |
|       13 |  9577 | `			pAttr->nType = pClass->nEnumBacking;` |
|       13 |  9578 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 |  9579 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|        4 |  9580 | `			}else{` |
|        7 |  9581 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - |  9582 | `			}` |
|       13 |  9583 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|        6 |  9584 | `		}` |
|        - |  9585 | `	}` |
|       25 |  9586 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|       14 |  9587 | `}` |
|        - |  9588 | `/*` |
|        - |  9589 | ` * Compile a class declaration, named or anonymous.` |
|        - |  9590 | ` *` |
|        - |  9591 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - |  9592 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - |  9593 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - |  9594 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - |  9595 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - |  9596 | ` * implements, body, install) is shared by both paths.` |
|        - |  9597 | ` */` |
|   215188 |  9598 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - |  9599 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 |  9600 | `{` |
|   215193 |  9601 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9602 | `	ph7_class *pClass,*pBase;` |
|        - |  9603 | `	SyToken *pEnd,*pTmp;` |
|        - |  9604 | `	sxi32 iProtection;` |
|        - |  9605 | `	SySet aInterfaces;` |
|        - |  9606 | `	SySet aUseEntries;` |
|        - |  9607 | `	sxi32 iAttrflags;` |
|        - |  9608 | `	SyString *pName;` |
|        - |  9609 | `	sxi32 nKwrd;` |
|        - |  9610 | `	sxi32 rc;` |
|        - |  9611 | `	/* Jump the 'class' keyword */` |
|   215193 |  9612 | `	pGen->pIn++;` |
|   215193 |  9613 | `	if( pAnonName ){` |
|        - |  9614 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - |  9615 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - |  9616 | `		 * then use the synthesized name. */` |
|       30 |  9617 | `		*ppArgStart = *ppArgEnd = 0;` |
|       30 |  9618 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 |  9619 | `			pGen->pIn++; /* Jump '(' */` |
|        7 |  9620 | `			*ppArgStart = pGen->pIn;` |
|       10 |  9621 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 |  9622 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 |  9623 | `			pGen->pIn = *ppArgEnd;` |
|        7 |  9624 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 |  9625 | `		}` |
|       30 |  9626 | `		pName = pAnonName;` |
|       30 |  9627 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       17 |  9628 | `	}else{` |
|   215167 |  9629 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  9630 | `			/* Syntax error */` |
|      ! 0 |  9631 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 |  9632 | `			if( rc == SXERR_ABORT ){` |
|        - |  9633 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9634 | `				return SXERR_ABORT;` |
|        - |  9635 | `			}` |
|        - |  9636 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 |  9637 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 |  9638 | `				pGen->pIn++;` |
|      ! 0 |  9639 | `			}` |
|      ! 0 |  9640 | `			return SXRET_OK;` |
|        - |  9641 | `		}` |
|        - |  9642 | `		/* Extract class name */` |
|   215167 |  9643 | `		pName = &pGen->pIn->sData;` |
|        - |  9644 | `		/* Advance the stream cursor */` |
|   215167 |  9645 | `		pGen->pIn++;` |
|        - |  9646 | `		/* Build FQN and obtain a raw class */ {` |
|        - |  9647 | `			SyBlob sFQN;` |
|        - |  9648 | `			SyString sFQNStr;` |
|   215167 |  9649 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   215167 |  9650 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   215167 |  9651 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   215167 |  9652 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   215167 |  9653 | `			SyBlobRelease(&sFQN);` |
|        - |  9654 | `		}` |
|        - |  9655 | `	}` |
|   215193 |  9656 | `	if( pClass == 0 ){` |
|      ! 0 |  9657 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9658 | `		return SXERR_ABORT;` |
|        - |  9659 | `	}` |
|   215188 |  9660 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|       31 |  9661 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - |  9662 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|       16 |  9663 | `		pGen->pIn++; /* Jump ':' */` |
|       14 |  9664 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       16 |  9665 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        7 |  9666 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        7 |  9667 | `			pGen->pIn++;` |
|       12 |  9668 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       10 |  9669 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|        7 |  9670 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|        7 |  9671 | `			pGen->pIn++;` |
|        4 |  9672 | `		}else{` |
|        3 |  9673 | `			SyToken *pTok = pGen->pIn;` |
|        3 |  9674 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 |  9675 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 |  9676 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 |  9677 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9678 | `				return SXERR_ABORT;` |
|        - |  9679 | `			}` |
|        3 |  9680 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 |  9681 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 |  9682 | `			}` |
|        - |  9683 | `		}` |
|        7 |  9684 | `	}` |
|   215193 |  9685 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   215193 |  9686 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9687 | `		return SXERR_ABORT;` |
|        - |  9688 | `	}` |
|        - |  9689 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   215193 |  9690 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   215193 |  9691 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - |  9692 | `	/* Assume a standalone class */` |
|   215193 |  9693 | `	pBase = 0;` |
|   215193 |  9694 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   171283 |  9695 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   171283 |  9696 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - |  9697 | `			SyBlob sResolved;` |
|        - |  9698 | `			SyString sBaseName;` |
|        - |  9699 | `			sxu32 nRefLine;` |
|   124503 |  9700 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - |  9701 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 |  9702 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9703 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|      ! 0 |  9704 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9705 | `					return SXERR_ABORT;` |
|        - |  9706 | `				}` |
|      ! 0 |  9707 | `			}` |
|   124503 |  9708 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   124503 |  9709 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   124503 |  9710 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   124503 |  9711 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 |  9712 | `				SyBlobRelease(&sResolved);` |
|        4 |  9713 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9714 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 |  9715 | `					pName);` |
|        3 |  9716 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 |  9717 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9718 | `					return SXERR_ABORT;` |
|        - |  9719 | `				}` |
|        3 |  9720 | `				return SXRET_OK;` |
|        - |  9721 | `			}` |
|   186749 |  9722 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   124496 |  9723 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   124501 |  9724 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  9725 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9726 | `			/* Interfaces are not allowed */` |
|   124501 |  9727 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 |  9728 | `				pBase = pBase->pNextName;` |
|      ! 0 |  9729 | `			}` |
|   124501 |  9730 | `			if( pBase == 0 ){` |
|      ! 0 |  9731 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9732 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 |  9733 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9734 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9735 | `					return SXERR_ABORT;` |
|        - |  9736 | `				}` |
|      ! 0 |  9737 | `			}else{` |
|   124501 |  9738 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 |  9739 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  9740 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|        3 |  9741 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9742 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9743 | `						return SXERR_ABORT;` |
|        - |  9744 | `					}` |
|        3 |  9745 | `					pBase = 0; /* Never inherit from an enum */` |
|   124500 |  9746 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 |  9747 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9748 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 |  9749 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9750 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9751 | `						return SXERR_ABORT;` |
|        - |  9752 | `					}` |
|      ! 0 |  9753 | `				}` |
|        - |  9754 | `			}` |
|   124501 |  9755 | `			SyBlobRelease(&sResolved);` |
|   124501 |  9756 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 |  9757 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 |  9758 | `			}` |
|    62248 |  9759 | `		}` |
|   171281 |  9760 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - |  9761 | `			ph7_class *pInterface;` |
|        - |  9762 | `			/* Interface implementation */` |
|    46797 |  9763 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    23408 |  9764 | `			for(;;){` |
|        - |  9765 | `				SyBlob sResolved;` |
|        - |  9766 | `				SyString sIntName;` |
|        - |  9767 | `				sxu32 nRefLine;` |
|    46809 |  9768 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    46809 |  9769 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    46809 |  9770 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  9771 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9772 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9773 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 |  9774 | `						pName);` |
|      ! 0 |  9775 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9776 | `						return SXERR_ABORT;` |
|        - |  9777 | `					}` |
|      ! 0 |  9778 | `					break;` |
|        - |  9779 | `				}` |
|    93613 |  9780 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    46804 |  9781 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    46809 |  9782 | `				SyStringInitFromBuf(&sIntName,` |
|        - |  9783 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9784 | `				/* Only interfaces are allowed */` |
|    46809 |  9785 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  9786 | `					pInterface = pInterface->pNextName;` |
|      ! 0 |  9787 | `				}` |
|    46809 |  9788 | `				if( pInterface == 0 ){` |
|      ! 0 |  9789 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9790 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 |  9791 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9792 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9793 | `						return SXERR_ABORT;` |
|        - |  9794 | `					}` |
|      ! 0 |  9795 | `				}else{` |
|        - |  9796 | `					/* Reject user classes that try to implement Throwable` |
|        - |  9797 | `					 * directly (or via an interface that extends Throwable)` |
|        - |  9798 | `					 * unless they already extend Exception or Error.` |
|        - |  9799 | `					 * Exception and Error themselves are compiled from the` |
|        - |  9800 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - |  9801 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    46809 |  9802 | `					SyString *pFqn = &pClass->sName;` |
|    46809 |  9803 | `					int bIsExceptionOrError =` |
|    27290 |  9804 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    72152 |  9805 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    44869 |  9806 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3898 |  9807 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    50693 |  9808 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11670 |  9809 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3887 |  9810 | `						!bIsExceptionOrError ){` |
|       12 |  9811 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9812 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 |  9813 | `							&pClass->sName);` |
|        9 |  9814 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9815 | `							SyBlobRelease(&sResolved);` |
|      ! 0 |  9816 | `							return SXERR_ABORT;` |
|        - |  9817 | `						}` |
|        - |  9818 | `						/* Skip registration so the follow-up abstract-method` |
|        - |  9819 | `						 * check does not produce a duplicate fatal. */` |
|        6 |  9820 | `					}else{` |
|    46803 |  9821 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - |  9822 | `					}` |
|        - |  9823 | `				}` |
|    46809 |  9824 | `				SyBlobRelease(&sResolved);` |
|    46809 |  9825 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    23401 |  9826 | `					break;` |
|        - |  9827 | `				}` |
|       16 |  9828 | `				pGen->pIn++;/* Jump the comma */` |
|        4 |  9829 | `			}` |
|    23396 |  9830 | `		}` |
|    85638 |  9831 | `	}` |
|   215191 |  9832 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  9833 | `		/* Syntax error */` |
|      ! 0 |  9834 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 |  9835 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9836 | `		if( rc == SXERR_ABORT ){` |
|        - |  9837 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9838 | `			return SXERR_ABORT;` |
|        - |  9839 | `		}` |
|      ! 0 |  9840 | `		return SXRET_OK;` |
|        - |  9841 | `	}` |
|   215191 |  9842 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   215191 |  9843 | `	pEnd = 0; /* cc warning */` |
|        - |  9844 | `	/* Delimit the class body */` |
|   215191 |  9845 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   215191 |  9846 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  9847 | `		/* Syntax error */` |
|      ! 0 |  9848 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 |  9849 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9850 | `		if( rc == SXERR_ABORT ){` |
|        - |  9851 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9852 | `			return SXERR_ABORT;` |
|        - |  9853 | `		}` |
|      ! 0 |  9854 | `		return SXRET_OK;` |
|        - |  9855 | `	}` |
|        - |  9856 | `	/* The delimiter token is the class body's closing brace */` |
|   215191 |  9857 | `	pClass->nEndLine = pEnd->nLine;` |
|        - |  9858 | `	/* Swap token stream */` |
|   215191 |  9859 | `	pTmp = pGen->pEnd;` |
|   215191 |  9860 | `	pGen->pEnd = pEnd;` |
|        - |  9861 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   215191 |  9862 | `	pClass->iFlags \|= iFlags;` |
|        - |  9863 | `	/* Start the parse process */` |
|   822964 |  9864 | `	for(;;){` |
|        - |  9865 | `		/* Jump leading/trailing semi-colons */` |
|  2210995 |  9866 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   354459 |  9867 | `			pGen->pIn++;` |
|        5 |  9868 | `		}` |
|  1856541 |  9869 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  9870 | `			/* End of class body */` |
|   215149 |  9871 | `			break;` |
|        - |  9872 | `		}` |
|        - |  9873 | `		/* Bind a directly-preceding docblock to this member */` |
|  1641397 |  9874 | `		GenStateSetPendingDoc(&(*pGen));` |
|  1641392 |  9875 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   820701 |  9876 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 |  9877 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9878 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 |  9879 | `				&pGen->pIn->sData,pName);` |
|      ! 0 |  9880 | `			if( rc == SXERR_ABORT ){` |
|        - |  9881 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9882 | `				return SXERR_ABORT;` |
|        - |  9883 | `			}` |
|      ! 0 |  9884 | `			goto done;` |
|        - |  9885 | `		}` |
|        - |  9886 | `		/* Assume public visibility */` |
|  1641397 |  9887 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  1641397 |  9888 | `		iAttrflags = 0;` |
|        - |  9889 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - |  9890 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - |  9891 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - |  9892 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  1641397 |  9893 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 |  9894 | `			int bMod = 0;` |
|      ! 0 |  9895 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 |  9896 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - |  9897 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - |  9898 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - |  9899 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - |  9900 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 |  9901 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 |  9902 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  9903 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 |  9904 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 |  9905 | `			}` |
|      ! 0 |  9906 | `			if( !bMod ){` |
|      ! 0 |  9907 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 |  9908 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  9909 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9910 | `						return SXERR_ABORT;` |
|        - |  9911 | `					}` |
|      ! 0 |  9912 | `					goto done;` |
|        - |  9913 | `				}` |
|      ! 0 |  9914 | `				continue;` |
|        - |  9915 | `			}` |
|      ! 0 |  9916 | `		}` |
|  1641397 |  9917 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - |  9918 | `			/* Extract the current keyword */` |
|  1641397 |  9919 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1641397 |  9920 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - |  9921 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|       45 |  9922 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|       45 |  9923 | `				if( rc != SXRET_OK ){` |
|        6 |  9924 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9925 | `						return SXERR_ABORT;` |
|        - |  9926 | `					}` |
|        6 |  9927 | `					goto done;` |
|        - |  9928 | `				}` |
|       39 |  9929 | `				continue;` |
|        - |  9930 | `			}` |
|  1641357 |  9931 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - |  9932 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - |  9933 | `				TraitUseEntry sUse;` |
|       61 |  9934 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       61 |  9935 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|       61 |  9936 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|       36 |  9937 | `				for(;;){` |
|        - |  9938 | `					ph7_class *pTrait;` |
|        - |  9939 | `					SyString *pTraitName;` |
|       69 |  9940 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  9941 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9942 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 |  9943 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9944 | `							return SXERR_ABORT;` |
|        - |  9945 | `						}` |
|      ! 0 |  9946 | `						break;` |
|        - |  9947 | `					}` |
|       69 |  9948 | `					pTraitName = &pGen->pIn->sData;` |
|        - |  9949 | `					/* Resolve trait name through namespace/imports */ {` |
|        - |  9950 | `						SyBlob sResolved;` |
|       69 |  9951 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       69 |  9952 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      133 |  9953 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|       64 |  9954 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       69 |  9955 | `						SyBlobRelease(&sResolved);` |
|        - |  9956 | `					}` |
|        - |  9957 | `					/* Only traits are allowed */` |
|       69 |  9958 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 |  9959 | `						pTrait = pTrait->pNextName;` |
|      ! 0 |  9960 | `					}` |
|       69 |  9961 | `					if( pTrait == 0 ){` |
|      ! 0 |  9962 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9963 | `							"'%z' is not a trait",pTraitName);` |
|      ! 0 |  9964 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9965 | `							return SXERR_ABORT;` |
|        - |  9966 | `						}` |
|      ! 0 |  9967 | `					}else{` |
|       69 |  9968 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - |  9969 | `					}` |
|       69 |  9970 | `					pGen->pIn++; /* Advance past trait name */` |
|       69 |  9971 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       33 |  9972 | `						break;` |
|        - |  9973 | `					}` |
|       10 |  9974 | `					pGen->pIn++; /* Jump the comma */` |
|        2 |  9975 | `				}` |
|        - |  9976 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|       61 |  9977 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - |  9978 | `					SyToken *pBlock;` |
|       13 |  9979 | `					pGen->pIn++; /* Jump '{' */` |
|       13 |  9980 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       13 |  9981 | `					sUse.pResolvStart = pGen->pIn;` |
|       13 |  9982 | `					sUse.pResolvEnd = pBlock;` |
|       13 |  9983 | `					if( pBlock < pGen->pEnd ){` |
|       13 |  9984 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        8 |  9985 | `					}else{` |
|      ! 0 |  9986 | `						pGen->pIn = pGen->pEnd;` |
|        - |  9987 | `					}` |
|        5 |  9988 | `				}` |
|       61 |  9989 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - |  9990 | `				/* The semicolon will be consumed by the outer loop */` |
|       61 |  9991 | `				continue;` |
|        - |  9992 | `			}` |
|  1641301 |  9993 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  1497199 |  9994 | `				iProtection = nKwrd;` |
|  1497199 |  9995 | `				pGen->pIn++; /* Jump the visibility token */` |
|        - |  9996 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  1497199 |  9997 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       22 |  9998 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       22 |  9999 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        9 | 10000 | `				}` |
|  1497194 | 10001 | `				if( pGen->pIn >= pGen->pEnd` |
|  1497199 | 10002 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10003 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10004 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 10005 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10006 | `					if( rc == SXERR_ABORT ){` |
|        - | 10007 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 | 10008 | `						return SXERR_ABORT;` |
|        - | 10009 | `					}` |
|      ! 0 | 10010 | `					goto done;` |
|        - | 10011 | `				}` |
|  1497199 | 10012 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10013 | `					/* Attribute declaration (untyped) */` |
|   210309 | 10014 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   210309 | 10015 | `					if( rc != SXRET_OK ){` |
|       11 | 10016 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10017 | `							return SXERR_ABORT;` |
|        - | 10018 | `						}` |
|       11 | 10019 | `						goto done;` |
|        - | 10020 | `					}` |
|   210301 | 10021 | `					continue;` |
|        - | 10022 | `				}` |
|  1286895 | 10023 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10024 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|      187 | 10025 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      187 | 10026 | `					if( rc != SXRET_OK ){` |
|        8 | 10027 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10028 | `							return SXERR_ABORT;` |
|        - | 10029 | `						}` |
|        8 | 10030 | `						goto done;` |
|        - | 10031 | `					}` |
|      181 | 10032 | `					continue;` |
|        - | 10033 | `				}` |
|        - | 10034 | `				/* Extract the keyword */` |
|  1286713 | 10035 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   643354 | 10036 | `			}` |
|  1430815 | 10037 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 10038 | `				/* Process constant declaration */` |
|   143851 | 10039 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   143851 | 10040 | `				if( rc != SXRET_OK ){` |
|       11 | 10041 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10042 | `						return SXERR_ABORT;` |
|        - | 10043 | `					}` |
|       11 | 10044 | `					goto done;` |
|        - | 10045 | `				}` |
|    71924 | 10046 | `			}else{` |
|  1286969 | 10047 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 10048 | `					/* Static method or attribute,record that */` |
|    23419 | 10049 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    23419 | 10050 | `					pGen->pIn++; /* Jump the static keyword */` |
|    23419 | 10051 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10052 | `						/* Extract the keyword */` |
|    23393 | 10053 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    23393 | 10054 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 10055 | `							iProtection = nKwrd;` |
|      ! 0 | 10056 | `							pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 | 10057 | `						}` |
|    11694 | 10058 | `					}` |
|        - | 10059 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - | 10060 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - | 10061 | `					 * than a generic "expecting method" parse error. */` |
|    23419 | 10062 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 10063 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 10064 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 | 10065 | `					}` |
|    23414 | 10066 | `					if( pGen->pIn >= pGen->pEnd` |
|    23419 | 10067 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10068 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10069 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 | 10070 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10071 | `						if( rc == SXERR_ABORT ){` |
|        - | 10072 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10073 | `							return SXERR_ABORT;` |
|        - | 10074 | `						}` |
|      ! 0 | 10075 | `						goto done;` |
|        - | 10076 | `					}` |
|    23419 | 10077 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10078 | `						/* Attribute declaration */` |
|       27 | 10079 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       27 | 10080 | `						if( rc != SXRET_OK ){` |
|        3 | 10081 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10082 | `								return SXERR_ABORT;` |
|        - | 10083 | `							}` |
|        3 | 10084 | `							goto done;` |
|        - | 10085 | `						}` |
|       24 | 10086 | `						continue;` |
|        - | 10087 | `					}` |
|    23395 | 10088 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10089 | `						/* Typed static attribute declaration */` |
|       15 | 10090 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       15 | 10091 | `						if( rc != SXRET_OK ){` |
|        3 | 10092 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10093 | `								return SXERR_ABORT;` |
|        - | 10094 | `							}` |
|        3 | 10095 | `							goto done;` |
|        - | 10096 | `						}` |
|       13 | 10097 | `						continue;` |
|        - | 10098 | `					}` |
|        - | 10099 | `					/* Extract the keyword */` |
|    23383 | 10100 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1275244 | 10101 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - | 10102 | `					/* Abstract method,record that */` |
|       15 | 10103 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - | 10104 | `					/* Mark the whole class as abstract */` |
|       15 | 10105 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|        - | 10106 | `					/* Advance the stream cursor */` |
|       15 | 10107 | `					pGen->pIn++;` |
|       15 | 10108 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       15 | 10109 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       15 | 10110 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       13 | 10111 | `							iProtection = nKwrd;` |
|       13 | 10112 | `							pGen->pIn++; /* Jump the visibility token */` |
|        5 | 10113 | `						}` |
|        6 | 10114 | `					}` |
|       15 | 10115 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       12 | 10116 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10117 | `							/* Static method */` |
|      ! 0 | 10118 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10119 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10120 | `					}` |
|       15 | 10121 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       12 | 10122 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10123 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10124 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 | 10125 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10126 | `							if( rc == SXERR_ABORT ){` |
|        - | 10127 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10128 | `								return SXERR_ABORT;` |
|        - | 10129 | `							}` |
|      ! 0 | 10130 | `							goto done;` |
|        - | 10131 | `					}` |
|       15 | 10132 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  1263549 | 10133 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - | 10134 | `					/* final method ,record that */` |
|       21 | 10135 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       21 | 10136 | `					pGen->pIn++; /* Jump the final keyword */` |
|       21 | 10137 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10138 | `						/* Extract the keyword */` |
|       21 | 10139 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 | 10140 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       11 | 10141 | `							iProtection = nKwrd;` |
|       11 | 10142 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 | 10143 | `						}` |
|        9 | 10144 | `					}` |
|       21 | 10145 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 10146 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - | 10147 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - | 10148 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - | 10149 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 | 10150 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 | 10151 | `							if( rc != SXRET_OK ){` |
|      ! 0 | 10152 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 | 10153 | `									return SXERR_ABORT;` |
|        - | 10154 | `								}` |
|      ! 0 | 10155 | `								goto done;` |
|        - | 10156 | `							}` |
|       14 | 10157 | `							continue;` |
|        - | 10158 | `					}` |
|        9 | 10159 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 | 10160 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10161 | `							/* Static method */` |
|      ! 0 | 10162 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10163 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10164 | `					}` |
|        9 | 10165 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 10166 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10167 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10168 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 | 10169 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10170 | `							if( rc == SXERR_ABORT ){` |
|        - | 10171 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10172 | `								return SXERR_ABORT;` |
|        - | 10173 | `							}` |
|      ! 0 | 10174 | `							goto done;` |
|        - | 10175 | `					}` |
|        9 | 10176 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 10177 | `				}` |
|  1286921 | 10178 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 10179 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10180 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 | 10181 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10182 | `						if( rc == SXERR_ABORT ){` |
|        - | 10183 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10184 | `							return SXERR_ABORT;` |
|        - | 10185 | `						}` |
|      ! 0 | 10186 | `						goto done;` |
|        - | 10187 | `				}` |
|  1286921 | 10188 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 | 10189 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 10190 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 | 10191 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10192 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 10193 | `						if( rc == SXERR_ABORT ){` |
|        - | 10194 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10195 | `							return SXERR_ABORT;` |
|        - | 10196 | `						}` |
|      ! 0 | 10197 | `						goto done;` |
|        - | 10198 | `					}` |
|        - | 10199 | `					/* Attribute declaration */` |
|        7 | 10200 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 | 10201 | `				}else{` |
|        - | 10202 | `					/* Process method declaration */` |
|  1286915 | 10203 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 10204 | `				}` |
|  1286921 | 10205 | `				if( rc != SXRET_OK ){` |
|       16 | 10206 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10207 | `						return SXERR_ABORT;` |
|        - | 10208 | `					}` |
|       16 | 10209 | `					goto done;` |
|        - | 10210 | `				}` |
|        - | 10211 | `			}` |
|   715376 | 10212 | `		}else{` |
|        - | 10213 | `			/* Attribute declaration */` |
|      ! 0 | 10214 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10215 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10216 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10217 | `					return SXERR_ABORT;` |
|        - | 10218 | `				}` |
|      ! 0 | 10219 | `				goto done;` |
|        - | 10220 | `			}` |
|        - | 10221 | `		}` |
|        5 | 10222 | `	}` |
|        - | 10223 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 10224 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 10225 | `	 */` |
|        - | 10226 | `	{` |
|        - | 10227 | `		TraitUseEntry *apUse;` |
|        - | 10228 | `		sxu32 nU;` |
|   215149 | 10229 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   215205 | 10230 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|       61 | 10231 | `			TraitUseEntry *pUse = &apUse[nU];` |
|       61 | 10232 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|       61 | 10233 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|       61 | 10234 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 10235 | `			sxu32 nT;` |
|       61 | 10236 | `			if( !hasResolution ){` |
|        - | 10237 | `				/* No conflict resolution block: use standard trait application */` |
|      103 | 10238 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       57 | 10239 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|       57 | 10240 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10241 | `						break;` |
|        - | 10242 | `					}` |
|       31 | 10243 | `				}` |
|       28 | 10244 | `			}else{` |
|        - | 10245 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 10246 | `				 * then use the block to resolve method conflicts.` |
|        - | 10247 | `				 */` |
|        - | 10248 | `				SyToken *pR;` |
|       25 | 10249 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       15 | 10250 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 10251 | `					ph7_class_attr *pAR;` |
|        - | 10252 | `					SyHashEntry *pER;` |
|        - | 10253 | `					SyString *pNR;` |
|       15 | 10254 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       21 | 10255 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 | 10256 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 10257 | `						pNR = &pAR->sName;` |
|      ! 0 | 10258 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 10259 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 10260 | `						}` |
|      ! 0 | 10261 | `					}` |
|       15 | 10262 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        9 | 10263 | `				}` |
|        - | 10264 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       13 | 10265 | `				pR = pUse->pResolvStart;` |
|       27 | 10266 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 10267 | `					SyString sTrait,sMethod;` |
|        - | 10268 | `					ph7_class *pSrcTrait;` |
|        - | 10269 | `					ph7_class_method *pMeth;` |
|        - | 10270 | `					sxi32 nRKwrd;` |
|       41 | 10271 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 10272 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 10273 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 10274 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 10275 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 10276 | `					sMethod = pR->sData;` |
|       17 | 10277 | `					pR++;` |
|       17 | 10278 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 10279 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 10280 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 10281 | `							sTrait = sMethod;` |
|        7 | 10282 | `							pR++;` |
|        7 | 10283 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 10284 | `							sMethod = pR->sData;` |
|        7 | 10285 | `							pR++;` |
|        3 | 10286 | `						}` |
|        3 | 10287 | `					}` |
|       17 | 10288 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 10289 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 10290 | `						continue;` |
|        - | 10291 | `					}` |
|       17 | 10292 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 10293 | `					pR++;` |
|       17 | 10294 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 10295 | `						pSrcTrait = 0;` |
|        7 | 10296 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 10297 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 10298 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 10299 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 10300 | `								pSrcTrait = apTrait[nT];` |
|        5 | 10301 | `								break;` |
|        - | 10302 | `							}` |
|        2 | 10303 | `						}` |
|        5 | 10304 | `						if( pSrcTrait ){` |
|        5 | 10305 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 10306 | `							if( pMeth ){` |
|        5 | 10307 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 10308 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 10309 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 10310 | `								}` |
|        2 | 10311 | `							}` |
|        2 | 10312 | `						}` |
|        2 | 10313 | `					}` |
|       35 | 10314 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 10315 | `				}` |
|        - | 10316 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       25 | 10317 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 10318 | `					ph7_class_method *pMR;` |
|        - | 10319 | `					SyHashEntry *pER;` |
|        - | 10320 | `					SyString *pNR;` |
|       15 | 10321 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       41 | 10322 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       23 | 10323 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       23 | 10324 | `						pNR = &pMR->sFunc.sName;` |
|       23 | 10325 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       14 | 10326 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 10327 | `						}` |
|        3 | 10328 | `					}` |
|        9 | 10329 | `				}` |
|        - | 10330 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       13 | 10331 | `				pR = pUse->pResolvStart;` |
|       27 | 10332 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 10333 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 10334 | `					ph7_class *pSrcTrait;` |
|        - | 10335 | `					ph7_class_method *pMeth;` |
|       27 | 10336 | `					int hasQual = 0;` |
|        - | 10337 | `					sxi32 nRKwrd;` |
|       41 | 10338 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 10339 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 10340 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 10341 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 10342 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       17 | 10343 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 10344 | `					sMethod = pR->sData;` |
|       17 | 10345 | `					pR++;` |
|       17 | 10346 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 10347 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 10348 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 10349 | `							sTrait = sMethod;` |
|        7 | 10350 | `							hasQual = 1;` |
|        7 | 10351 | `							pR++;` |
|        7 | 10352 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 10353 | `							sMethod = pR->sData;` |
|        7 | 10354 | `							pR++;` |
|        3 | 10355 | `						}` |
|        3 | 10356 | `					}` |
|       17 | 10357 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 10358 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 10359 | `						continue;` |
|        - | 10360 | `					}` |
|       17 | 10361 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 10362 | `					pR++;` |
|       17 | 10363 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       13 | 10364 | `						sxi32 iNewVis = -1;` |
|       13 | 10365 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 10366 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 10367 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 10368 | `								iNewVis = nAK;` |
|        7 | 10369 | `								pR++;` |
|        3 | 10370 | `							}` |
|        3 | 10371 | `						}` |
|       13 | 10372 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       11 | 10373 | `							sAlias = pR->sData;` |
|       11 | 10374 | `							pR++;` |
|        4 | 10375 | `						}` |
|       13 | 10376 | `						pMeth = 0;` |
|       13 | 10377 | `						if( hasQual ){` |
|        3 | 10378 | `							pSrcTrait = 0;` |
|        5 | 10379 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 10380 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 10381 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 10382 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 10383 | `									pSrcTrait = apTrait[nT];` |
|        3 | 10384 | `									break;` |
|        - | 10385 | `								}` |
|        2 | 10386 | `							}` |
|        3 | 10387 | `							if( pSrcTrait ){` |
|        3 | 10388 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 10389 | `							}` |
|        2 | 10390 | `						}else{` |
|       10 | 10391 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 10392 | `						}` |
|       13 | 10393 | `						if( pMeth ){` |
|       13 | 10394 | `							if( sAlias.nByte > 0 ){` |
|        - | 10395 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 10396 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 10397 | `								 */` |
|        - | 10398 | `								ph7_class_method *pAlias;` |
|        - | 10399 | `								char *zAliasDup;` |
|       11 | 10400 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       11 | 10401 | `								if( pAlias ){` |
|       11 | 10402 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       11 | 10403 | `									if( iNewVis >= 0 ){` |
|        5 | 10404 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 10405 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 10406 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 10407 | `									}` |
|       11 | 10408 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       11 | 10409 | `									if( zAliasDup ){` |
|       11 | 10410 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 10411 | `									}` |
|        7 | 10412 | `								}` |
|        7 | 10413 | `							}else if( iNewVis >= 0 ){` |
|        - | 10414 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 10415 | `								ph7_class_method *pCopy;` |
|        3 | 10416 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 10417 | `								if( pCopy ){` |
|        3 | 10418 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 10419 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 10420 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 10421 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 10422 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 10423 | `									/* Replace the method in the class hash */` |
|        3 | 10424 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 10425 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 10426 | `								}` |
|        1 | 10427 | `							}` |
|        5 | 10428 | `						}` |
|        5 | 10429 | `						SXUNUSED(hasQual);` |
|        5 | 10430 | `					}` |
|       21 | 10431 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 10432 | `				}` |
|        - | 10433 | `			}` |
|       61 | 10434 | `			SySetRelease(&pUse->aTraits);` |
|       33 | 10435 | `		}` |
|        - | 10436 | `	}` |
|   215149 | 10437 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 10438 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 10439 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|       25 | 10440 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|       25 | 10441 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10442 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 10443 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 10444 | `			return SXERR_ABORT;` |
|        - | 10445 | `		}` |
|       11 | 10446 | `	}` |
|        - | 10447 | `	/* Install the class */` |
|   215149 | 10448 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   215149 | 10449 | `	if( rc == SXRET_OK ){` |
|        - | 10450 | `		ph7_class **apInterface;` |
|        - | 10451 | `		sxu32 n;` |
|   215149 | 10452 | `		if( pBase ){` |
|        - | 10453 | `			/* Inherit from base class and mark as a subclass */` |
|   124499 | 10454 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    62247 | 10455 | `		}` |
|   215149 | 10456 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   261947 | 10457 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 10458 | `			/* Implements one or more interface */` |
|    46803 | 10459 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    46803 | 10460 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10461 | `				break;` |
|        - | 10462 | `			}` |
|    23404 | 10463 | `		}` |
|        - | 10464 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 10465 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|   215149 | 10466 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|       25 | 10467 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|       25 | 10468 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10469 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 10470 | `			}` |
|       25 | 10471 | `			if( pIntf ){` |
|       25 | 10472 | `				PH7_ClassImplement(pClass,pIntf);` |
|       11 | 10473 | `			}` |
|       25 | 10474 | `			if( pClass->nEnumBacking != 0 ){` |
|       13 | 10475 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|       13 | 10476 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10477 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 10478 | `				}` |
|       13 | 10479 | `				if( pIntf ){` |
|       13 | 10480 | `					PH7_ClassImplement(pClass,pIntf);` |
|        6 | 10481 | `				}` |
|        6 | 10482 | `			}` |
|       11 | 10483 | `		}` |
|        - | 10484 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 10485 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   215144 | 10486 | `		if( rc == SXRET_OK` |
|   215144 | 10487 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   215149 | 10488 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   171003 | 10489 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 10490 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   171003 | 10491 | `			if( pStringable ){` |
|   171003 | 10492 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   171003 | 10493 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 10494 | `				sxu32 i;` |
|   171003 | 10495 | `				int bAlready = 0;` |
|   209847 | 10496 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    42735 | 10497 | `					if( apImpl[i] == pStringable ){` |
|     3891 | 10498 | `						bAlready = 1;` |
|     3891 | 10499 | `						break;` |
|        - | 10500 | `					}` |
|    19427 | 10501 | `				}` |
|   171003 | 10502 | `				if( !bAlready ){` |
|   167117 | 10503 | `					PH7_ClassImplement(pClass,pStringable);` |
|    83556 | 10504 | `				}` |
|    85499 | 10505 | `			}` |
|    85499 | 10506 | `		}` |
|        - | 10507 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   215149 | 10508 | `		if( rc == SXRET_OK ){` |
|   215149 | 10509 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   215149 | 10510 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 10511 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 10512 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 10513 | `				return SXERR_ABORT;` |
|        - | 10514 | `			}` |
|   107572 | 10515 | `		}` |
|        - | 10516 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   215149 | 10517 | `		if( rc == SXRET_OK ){` |
|   215149 | 10518 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   215149 | 10519 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 10520 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 10521 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 10522 | `				return SXERR_ABORT;` |
|        - | 10523 | `			}` |
|   107572 | 10524 | `		}` |
|   107572 | 10525 | `	}` |
|   215149 | 10526 | `	SySetRelease(&aUseEntries);` |
|   215149 | 10527 | `	SySetRelease(&aInterfaces);` |
|   215149 | 10528 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10529 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10530 | `		return SXERR_ABORT;` |
|        - | 10531 | `	}` |
|   107572 | 10532 | `done:` |
|        - | 10533 | `	/* Point beyond the class body */` |
|   215191 | 10534 | `	pGen->pIn = &pEnd[1];` |
|   215191 | 10535 | `	pGen->pEnd = pTmp;` |
|   215191 | 10536 | `	return PH7_OK;` |
|   107599 | 10537 | `}` |
|        - | 10538 | `/* Compile a named class declaration (the common case). */` |
|   215162 | 10539 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 10540 | `{` |
|   215167 | 10541 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 10542 | `}` |
|        - | 10543 | `/*` |
|        - | 10544 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 10545 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 10546 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 10547 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 10548 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 10549 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 10550 | ` */` |
|       26 | 10551 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 10552 | `{` |
|        - | 10553 | `	char zName[128];         /* Synthesized class name */` |
|        - | 10554 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 10555 | `	SyString sName;` |
|        - | 10556 | `	SyToken *pArgStart,*pArgEnd;` |
|        - | 10557 | `	ph7_value *pObj;` |
|       30 | 10558 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10559 | `	sxu32 nIdx,nLen;` |
|        - | 10560 | `	sxi32 nArg,rc;` |
|       13 | 10561 | `	SXUNUSED(iCompileFlag);` |
|        - | 10562 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       30 | 10563 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       30 | 10564 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 10565 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 10566 | `	}` |
|       30 | 10567 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 10568 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 10569 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 10570 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       30 | 10571 | `	pArgStart = pArgEnd = 0;` |
|       30 | 10572 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       30 | 10573 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10574 | `		return rc;` |
|        - | 10575 | `	}` |
|        - | 10576 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 10577 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       30 | 10578 | `	nArg = 0;` |
|       30 | 10579 | `	if( pArgStart < pArgEnd ){` |
|        7 | 10580 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 10581 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 10582 | `		SyToken *pArgNext;` |
|        7 | 10583 | `		pGen->pIn = pArgStart;` |
|        7 | 10584 | `		pGen->pEnd = pArgEnd;` |
|       13 | 10585 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 10586 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 10587 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 10588 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10589 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 10590 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 10591 | `					return SXERR_ABORT;` |
|        - | 10592 | `				}` |
|        7 | 10593 | `				nArg++;` |
|        3 | 10594 | `			}` |
|        7 | 10595 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 10596 | `		}` |
|        7 | 10597 | `		pGen->pIn = pSavedIn;` |
|        7 | 10598 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 10599 | `	}` |
|        - | 10600 | `	/* Load the synthesized class name */` |
|       30 | 10601 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       30 | 10602 | `	if( pObj == 0 ){` |
|      ! 0 | 10603 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 10604 | `		return SXERR_ABORT;` |
|        - | 10605 | `	}` |
|       30 | 10606 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       30 | 10607 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 10608 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       30 | 10609 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       30 | 10610 | `	return SXRET_OK;` |
|       17 | 10611 | `}` |
|        - | 10612 | `/*` |
|        - | 10613 | ` * Compile a user-defined abstract class.` |
|        - | 10614 | ` *  According to the PHP language reference manual` |
|        - | 10615 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 10616 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 10617 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 10618 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 10619 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 10620 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 10621 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 10622 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 10623 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 10624 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 10625 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 10626 | ` *   could differ.` |
|        - | 10627 | ` */` |
|        - | 10628 | `/*` |
|        - | 10629 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 10630 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 10631 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 10632 | ` */` |
|  6285256 | 10633 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 10634 | `{` |
|  6285261 | 10635 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  3907817 | 10636 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  3907817 | 10637 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  3868951 | 10638 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  1926670 | 10639 | `	}` |
|  6230789 | 10640 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  6230729 | 10641 | `	return FALSE;` |
|  3142633 | 10642 | `}` |
|        - | 10643 | `/*` |
|        - | 10644 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 10645 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 10646 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 10647 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 10648 | ` */` |
|  6230724 | 10649 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 10650 | `{` |
|  6230729 | 10651 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  6230729 | 10652 | `	sxi32 iFlags = 0,iFlag;` |
|  6285261 | 10653 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    54537 | 10654 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 10655 | `			pDup = pIn;` |
|        2 | 10656 | `		}` |
|    54537 | 10657 | `		iFlags \|= iFlag;` |
|    54537 | 10658 | `		pIn++;` |
|        5 | 10659 | `	}` |
|  6230729 | 10660 | `	*ppIn = pIn;` |
|  6230729 | 10661 | `	if( ppDup ){ *ppDup = pDup; }` |
|  6230729 | 10662 | `	return iFlags;` |
|        5 | 10663 | `}` |
|        - | 10664 | `/*` |
|        - | 10665 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 10666 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 10667 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 10668 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 10669 | `` * `readonly`) to their existing handlers.`` |
|        - | 10670 | ` */` |
|  6203468 | 10671 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 10672 | `{` |
|  6203473 | 10673 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  3128997 | 10674 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  6217098 | 10675 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 10676 | `}` |
|        - | 10677 | `/*` |
|        - | 10678 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 10679 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 10680 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 10681 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 10682 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 10683 | ` */` |
|    27256 | 10684 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 10685 | `{` |
|        - | 10686 | `	SyToken *pDup;` |
|    27261 | 10687 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 10688 | `	sxi32 rc;` |
|    27261 | 10689 | `	if( pDup ){` |
|        4 | 10690 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 10691 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 10692 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10693 | `			return SXERR_ABORT;` |
|        - | 10694 | `		}` |
|        1 | 10695 | `	}` |
|    27256 | 10696 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    13633 | 10697 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 10698 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10699 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 10700 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10701 | `			return SXERR_ABORT;` |
|        - | 10702 | `		}` |
|        1 | 10703 | `	}` |
|    27261 | 10704 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    13633 | 10705 | `}` |
|        - | 10706 | `/*` |
|        - | 10707 | ` * Compile a user-defined trait.` |
|        - | 10708 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 10709 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 10710 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 10711 | ` */` |
|       70 | 10712 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 10713 | `{` |
|       75 | 10714 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10715 | `	ph7_class *pClass;` |
|        - | 10716 | `	SyToken *pEnd,*pTmp;` |
|        - | 10717 | `	sxi32 iProtection;` |
|        - | 10718 | `	sxi32 iAttrflags;` |
|        - | 10719 | `	SyString *pName;` |
|        - | 10720 | `	sxi32 nKwrd;` |
|        - | 10721 | `	sxi32 rc;` |
|        - | 10722 | `	/* Jump the 'trait' keyword */` |
|       75 | 10723 | `	pGen->pIn++;` |
|       75 | 10724 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10725 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 10726 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10727 | `			return SXERR_ABORT;` |
|        - | 10728 | `		}` |
|      ! 0 | 10729 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 10730 | `			pGen->pIn++;` |
|      ! 0 | 10731 | `		}` |
|      ! 0 | 10732 | `		return SXRET_OK;` |
|        - | 10733 | `	}` |
|        - | 10734 | `	/* Extract trait name */` |
|       75 | 10735 | `	pName = &pGen->pIn->sData;` |
|       75 | 10736 | `	pGen->pIn++;` |
|        - | 10737 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 10738 | `		SyBlob sFQN;` |
|        - | 10739 | `		SyString sFQNStr;` |
|       75 | 10740 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       75 | 10741 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       75 | 10742 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       75 | 10743 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|       75 | 10744 | `		SyBlobRelease(&sFQN);` |
|        - | 10745 | `	}` |
|       75 | 10746 | `	if( pClass == 0 ){` |
|      ! 0 | 10747 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10748 | `		return SXERR_ABORT;` |
|        - | 10749 | `	}` |
|       75 | 10750 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|       75 | 10751 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10752 | `		return SXERR_ABORT;` |
|        - | 10753 | `	}` |
|        - | 10754 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|       75 | 10755 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 10756 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 10757 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10758 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10759 | `			return SXERR_ABORT;` |
|        - | 10760 | `		}` |
|      ! 0 | 10761 | `		return SXRET_OK;` |
|        - | 10762 | `	}` |
|       75 | 10763 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|       75 | 10764 | `	pEnd = 0;` |
|       75 | 10765 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|       75 | 10766 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 10767 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 10768 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10769 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10770 | `			return SXERR_ABORT;` |
|        - | 10771 | `		}` |
|      ! 0 | 10772 | `		return SXRET_OK;` |
|        - | 10773 | `	}` |
|        - | 10774 | `	/* The delimiter token is the trait body's closing brace */` |
|       75 | 10775 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 10776 | `	/* Swap token stream */` |
|       75 | 10777 | `	pTmp = pGen->pEnd;` |
|       75 | 10778 | `	pGen->pEnd = pEnd;` |
|        - | 10779 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|       75 | 10780 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 10781 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|       69 | 10782 | `	for(;;){` |
|      187 | 10783 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       28 | 10784 | `			pGen->pIn++;` |
|        4 | 10785 | `		}` |
|      163 | 10786 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       75 | 10787 | `			break;` |
|        - | 10788 | `		}` |
|        - | 10789 | `		/* Bind a directly-preceding docblock to this member */` |
|       93 | 10790 | `		GenStateSetPendingDoc(&(*pGen));` |
|       93 | 10791 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 10792 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10793 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 10794 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 10795 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10796 | `				return SXERR_ABORT;` |
|        - | 10797 | `			}` |
|      ! 0 | 10798 | `			goto done;` |
|        - | 10799 | `		}` |
|       93 | 10800 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|       93 | 10801 | `		iAttrflags = 0;` |
|       93 | 10802 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       93 | 10803 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       93 | 10804 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 10805 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 10806 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 10807 | `				for(;;){` |
|        - | 10808 | `					ph7_class *pUsedTrait;` |
|        - | 10809 | `					SyString *pUsedName;` |
|        5 | 10810 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10811 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10812 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 10813 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10814 | `							return SXERR_ABORT;` |
|        - | 10815 | `						}` |
|      ! 0 | 10816 | `						break;` |
|        - | 10817 | `					}` |
|        5 | 10818 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 10819 | `					{` |
|        - | 10820 | `						SyBlob sResolved;` |
|        5 | 10821 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 10822 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 10823 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 10824 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 10825 | `						SyBlobRelease(&sResolved);` |
|        - | 10826 | `					}` |
|        5 | 10827 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 10828 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 10829 | `					}` |
|        5 | 10830 | `					if( pUsedTrait == 0 ){` |
|        4 | 10831 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 10832 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 10833 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10834 | `							return SXERR_ABORT;` |
|        - | 10835 | `						}` |
|        2 | 10836 | `					}else{` |
|        3 | 10837 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 10838 | `					}` |
|        5 | 10839 | `					pGen->pIn++;` |
|        5 | 10840 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 10841 | `						break;` |
|        - | 10842 | `					}` |
|      ! 0 | 10843 | `					pGen->pIn++;` |
|      ! 0 | 10844 | `				}` |
|        5 | 10845 | `				continue;` |
|        - | 10846 | `			}` |
|       89 | 10847 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       77 | 10848 | `				iProtection = nKwrd;` |
|       77 | 10849 | `				pGen->pIn++;` |
|       72 | 10850 | `				if( pGen->pIn >= pGen->pEnd` |
|       77 | 10851 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10852 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10853 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 10854 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10855 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10856 | `						return SXERR_ABORT;` |
|        - | 10857 | `					}` |
|      ! 0 | 10858 | `					goto done;` |
|        - | 10859 | `				}` |
|       77 | 10860 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       12 | 10861 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       12 | 10862 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10863 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10864 | `							return SXERR_ABORT;` |
|        - | 10865 | `						}` |
|      ! 0 | 10866 | `						goto done;` |
|        - | 10867 | `					}` |
|       12 | 10868 | `					continue;` |
|        - | 10869 | `				}` |
|       67 | 10870 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        5 | 10871 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        5 | 10872 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10873 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10874 | `							return SXERR_ABORT;` |
|        - | 10875 | `						}` |
|      ! 0 | 10876 | `						goto done;` |
|        - | 10877 | `					}` |
|        5 | 10878 | `					continue;` |
|        - | 10879 | `				}` |
|       63 | 10880 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       29 | 10881 | `			}` |
|       75 | 10882 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 10883 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10884 | `					"Traits cannot have constants");` |
|      ! 0 | 10885 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10886 | `					return SXERR_ABORT;` |
|        - | 10887 | `				}` |
|      ! 0 | 10888 | `				goto done;` |
|      ! 0 | 10889 | `			}else{` |
|       75 | 10890 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        5 | 10891 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|        5 | 10892 | `					pGen->pIn++;` |
|        5 | 10893 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        3 | 10894 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        3 | 10895 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 10896 | `							iProtection = nKwrd;` |
|      ! 0 | 10897 | `							pGen->pIn++;` |
|      ! 0 | 10898 | `						}` |
|        1 | 10899 | `					}` |
|        4 | 10900 | `					if( pGen->pIn >= pGen->pEnd` |
|        5 | 10901 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10902 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10903 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 10904 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10905 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10906 | `							return SXERR_ABORT;` |
|        - | 10907 | `						}` |
|      ! 0 | 10908 | `						goto done;` |
|        - | 10909 | `					}` |
|        5 | 10910 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 10911 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 10912 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 10913 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10914 | `								return SXERR_ABORT;` |
|        - | 10915 | `							}` |
|      ! 0 | 10916 | `							goto done;` |
|        - | 10917 | `						}` |
|        3 | 10918 | `						continue;` |
|        - | 10919 | `					}` |
|        3 | 10920 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 10921 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10922 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 10923 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10924 | `								return SXERR_ABORT;` |
|        - | 10925 | `							}` |
|      ! 0 | 10926 | `							goto done;` |
|        - | 10927 | `						}` |
|      ! 0 | 10928 | `						continue;` |
|        - | 10929 | `					}` |
|        3 | 10930 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       72 | 10931 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        6 | 10932 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        6 | 10933 | `					pGen->pIn++;` |
|        6 | 10934 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 10935 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 10936 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        6 | 10937 | `							iProtection = nKwrd;` |
|        6 | 10938 | `							pGen->pIn++;` |
|        2 | 10939 | `						}` |
|        2 | 10940 | `					}` |
|        6 | 10941 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        4 | 10942 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10943 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10944 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 10945 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10946 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10947 | `							return SXERR_ABORT;` |
|        - | 10948 | `						}` |
|      ! 0 | 10949 | `						goto done;` |
|        - | 10950 | `					}` |
|        6 | 10951 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        2 | 10952 | `				}` |
|       73 | 10953 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 10954 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10955 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 10956 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10957 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10958 | `						return SXERR_ABORT;` |
|        - | 10959 | `					}` |
|      ! 0 | 10960 | `					goto done;` |
|        - | 10961 | `				}` |
|       73 | 10962 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 10963 | `					pGen->pIn++;` |
|      ! 0 | 10964 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 10965 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10966 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 10967 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10968 | `							return SXERR_ABORT;` |
|        - | 10969 | `						}` |
|      ! 0 | 10970 | `						goto done;` |
|        - | 10971 | `					}` |
|      ! 0 | 10972 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10973 | `				}else{` |
|       73 | 10974 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 10975 | `				}` |
|       73 | 10976 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 10977 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10978 | `						return SXERR_ABORT;` |
|        - | 10979 | `					}` |
|      ! 0 | 10980 | `					goto done;` |
|        - | 10981 | `				}` |
|        - | 10982 | `			}` |
|       39 | 10983 | `		}else{` |
|      ! 0 | 10984 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10985 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10986 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10987 | `					return SXERR_ABORT;` |
|        - | 10988 | `				}` |
|      ! 0 | 10989 | `				goto done;` |
|        - | 10990 | `			}` |
|        - | 10991 | `		}` |
|        5 | 10992 | `	}` |
|        - | 10993 | `	/* Install the trait */` |
|       75 | 10994 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|       75 | 10995 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10996 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10997 | `		return SXERR_ABORT;` |
|        - | 10998 | `	}` |
|       35 | 10999 | `done:` |
|        - | 11000 | `	/* Point beyond the trait body */` |
|       75 | 11001 | `	pGen->pIn = &pEnd[1];` |
|       75 | 11002 | `	pGen->pEnd = pTmp;` |
|       75 | 11003 | `	return PH7_OK;` |
|       40 | 11004 | `}` |
|        - | 11005 | `/*` |
|        - | 11006 | ` * Compile a user-defined class.` |
|        - | 11007 | ` *  According to the PHP language reference manual` |
|        - | 11008 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 11009 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 11010 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 11011 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 11012 | ` *   and functions (called "methods").` |
|        - | 11013 | ` */` |
|   187880 | 11014 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 11015 | `{` |
|        - | 11016 | `	sxi32 rc;` |
|   187885 | 11017 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   187885 | 11018 | `	return rc;` |
|        5 | 11019 | `}` |
|        - | 11020 | `/*` |
|        - | 11021 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 11022 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 11023 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 11024 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 11025 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 11026 | ` */` |
|  6176212 | 11027 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 11028 | `{` |
|  6209471 | 11029 | `	return (pIn->nType & PH7_TK_ID)` |
|  3121360 | 11030 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    37258 | 11031 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  6209466 | 11032 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 11033 | `}` |
|        - | 11034 | `/*` |
|        - | 11035 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 11036 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 11037 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 11038 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 11039 | ` */` |
|       26 | 11040 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 11041 | `{` |
|       31 | 11042 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 11043 | `}` |
|        - | 11044 | `/*` |
|        - | 11045 | ` * Exception handling.` |
|        - | 11046 | ` *  According to the PHP language reference manual` |
|        - | 11047 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 11048 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 11049 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 11050 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 11051 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 11052 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 11053 | ` *    (or re-thrown) within a catch block.` |
|        - | 11054 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 11055 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 11056 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 11057 | ` *    been defined with set_exception_handler().` |
|        - | 11058 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 11059 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 11060 | ` */` |
|        - | 11061 | `/*` |
|        - | 11062 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 11063 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 11064 | ` * indicates failure.` |
|        - | 11065 | ` */` |
|   315008 | 11066 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 11067 | `{` |
|   315013 | 11068 | `	sxi32 rc = SXRET_OK;` |
|   315013 | 11069 | `	if( pRoot->pOp ){` |
|   315001 | 11070 | `		switch( pRoot->pOp->iOp ){` |
|   157498 | 11071 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|        - | 11072 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|        - | 11073 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|        - | 11074 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|        - | 11075 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|        - | 11076 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   315001 | 11077 | `			break;` |
|      ! 0 | 11078 | `		default:` |
|        - | 11079 | `			/* Runtime will still reject non-Throwable values; the set above` |
|        - | 11080 | `			 * covers the common shapes and gives a friendlier compile error` |
|        - | 11081 | ``			 * for obvious mistakes like `throw 5`. */`` |
|      ! 0 | 11082 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11083 | `				"throw: Expecting an exception class instance");` |
|      ! 0 | 11084 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 | 11085 | `				rc = SXERR_INVALID;` |
|      ! 0 | 11086 | `			}` |
|      ! 0 | 11087 | `			break;` |
|        - | 11088 | `		}` |
|   157515 | 11089 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 11090 | `		/* Unexpected expression */` |
|      ! 0 | 11091 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11092 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11093 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 11094 | `			rc = SXERR_INVALID;` |
|      ! 0 | 11095 | `		}` |
|      ! 0 | 11096 | `	}` |
|   315013 | 11097 | `	return rc;` |
|        5 | 11098 | `}` |
|        - | 11099 | `/*` |
|        - | 11100 | ` * Compile a 'throw' statement.` |
|        - | 11101 | ` * throw: This is how you trigger an exception.` |
|        - | 11102 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 11103 | ` */` |
|   314972 | 11104 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 11105 | `{` |
|   314977 | 11106 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11107 | `	GenBlock *pBlock;` |
|        - | 11108 | `	sxu32 nIdx;` |
|        - | 11109 | `	sxi32 rc;` |
|   314977 | 11110 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 11111 | `	/* Compile the expression */` |
|   314977 | 11112 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   314977 | 11113 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11114 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 11115 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11116 | `			return SXERR_ABORT;` |
|        - | 11117 | `		}` |
|      ! 0 | 11118 | `		return SXRET_OK;` |
|        - | 11119 | `	}` |
|   314977 | 11120 | `	pBlock = pGen->pCurrent;` |
|        - | 11121 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  1228101 | 11122 | `	while(pBlock->pParent){` |
|  1228097 | 11123 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   314973 | 11124 | `			break;` |
|        - | 11125 | `		}` |
|        - | 11126 | `		/* Point to the parent block */` |
|   913129 | 11127 | `		pBlock = pBlock->pParent;` |
|        5 | 11128 | `	}` |
|        - | 11129 | `	/* Emit the throw instruction */` |
|   314977 | 11130 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 11131 | `	/* Emit the jump */` |
|   314977 | 11132 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   314977 | 11133 | `	return SXRET_OK;` |
|   157491 | 11134 | `}` |
|        - | 11135 | `/*` |
|        - | 11136 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 11137 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 11138 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 11139 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 11140 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 11141 | ` */` |
|       36 | 11142 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 11143 | `{` |
|       38 | 11144 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11145 | `	GenBlock *pBlock;` |
|        - | 11146 | `	sxu32 nIdx;` |
|        - | 11147 | `	sxi32 rc;` |
|       18 | 11148 | `	(void)iCompileFlag;` |
|       38 | 11149 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 11150 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 11151 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11152 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11153 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11154 | `			return SXERR_ABORT;` |
|        - | 11155 | `		}` |
|      ! 0 | 11156 | `		return SXRET_OK;` |
|        - | 11157 | `	}` |
|       38 | 11158 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 11159 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11160 | `		return SXERR_ABORT;` |
|        - | 11161 | `	}` |
|       38 | 11162 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11163 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11164 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11165 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11166 | `			return SXERR_ABORT;` |
|        - | 11167 | `		}` |
|      ! 0 | 11168 | `		return SXRET_OK;` |
|        - | 11169 | `	}` |
|        - | 11170 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 11171 | `	pBlock = pGen->pCurrent;` |
|       60 | 11172 | `	while( pBlock->pParent ){` |
|       49 | 11173 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 11174 | `			break;` |
|        - | 11175 | `		}` |
|       23 | 11176 | `		pBlock = pBlock->pParent;` |
|        1 | 11177 | `	}` |
|       38 | 11178 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 11179 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 11180 | `	return SXRET_OK;` |
|       20 | 11181 | `}` |
|        - | 11182 | `/*` |
|        - | 11183 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 11184 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 11185 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 11186 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 11187 | ` * compile error propagated from the parser.` |
|        - | 11188 | ` */` |
|       54 | 11189 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 11190 | `{` |
|        - | 11191 | `	SyString sClassName;` |
|        - | 11192 | `	SyToken *pToken;` |
|        - | 11193 | `	SyString *pName;` |
|        - | 11194 | `	char *zDup;` |
|        - | 11195 | `	sxi32 rc;` |
|       59 | 11196 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       59 | 11197 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       59 | 11198 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       59 | 11199 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       59 | 11200 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 11201 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11202 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11203 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11204 | `		return SXERR_INVALID;` |
|        - | 11205 | `	}` |
|       59 | 11206 | `	pGen->pIn++; /* '(' */` |
|       27 | 11207 | `	for(;;){` |
|        - | 11208 | `		SyBlob sResolved;` |
|       59 | 11209 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       59 | 11210 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 11211 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 11212 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11213 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11214 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11215 | `			return SXERR_INVALID;` |
|        - | 11216 | `		}` |
|       86 | 11217 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       54 | 11218 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       59 | 11219 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       59 | 11220 | `		SyBlobRelease(&sResolved);` |
|       59 | 11221 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 11222 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       59 | 11223 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       54 | 11224 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 11225 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 11226 | `			pGen->pIn++; continue;` |
|        - | 11227 | `		}` |
|       59 | 11228 | `		break;` |
|      ! 0 | 11229 | `	}` |
|       54 | 11230 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 11231 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 11232 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11233 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11234 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11235 | `		return SXERR_INVALID;` |
|        - | 11236 | `	}` |
|       59 | 11237 | `	pGen->pIn++; /* '$' */` |
|       59 | 11238 | `	pName = &pGen->pIn->sData;` |
|       59 | 11239 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 11240 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 11241 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 11242 | `	pGen->pIn++;` |
|       59 | 11243 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 11244 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11245 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11246 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11247 | `		return SXERR_INVALID;` |
|        - | 11248 | `	}` |
|       59 | 11249 | `	pGen->pIn++; /* ')' */` |
|       59 | 11250 | `	return SXRET_OK;` |
|       32 | 11251 | `}` |
|        - | 11252 | `/*` |
|        - | 11253 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 11254 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 11255 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 11256 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 11257 | ` * VmThrowException):` |
|        - | 11258 | ` *` |
|        - | 11259 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 11260 | ` *    <try body>` |
|        - | 11261 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 11262 | ` *    JMP  -> finally\|end` |
|        - | 11263 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 11264 | ` *    <catch body>` |
|        - | 11265 | ` *    JMP  -> finally\|end` |
|        - | 11266 | ` *    ... more catches ...` |
|        - | 11267 | ` *  Lfin: <finally body>` |
|        - | 11268 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 11269 | ` *  Lend:` |
|        - | 11270 | ` */` |
|       98 | 11271 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 11272 | `{` |
|      103 | 11273 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11274 | `	GenBlock *pTry;` |
|        - | 11275 | `	VmInstr *pInstr;` |
|      103 | 11276 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 11277 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 11278 | `	sxi32 rc;` |
|      103 | 11279 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 11280 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      103 | 11281 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      103 | 11282 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      103 | 11283 | `	pTry->pUserData = pException;` |
|      103 | 11284 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      103 | 11285 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      103 | 11286 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      103 | 11287 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      103 | 11288 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      103 | 11289 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 11290 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      103 | 11291 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      103 | 11292 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      103 | 11293 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      103 | 11294 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11295 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      103 | 11296 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 11297 | `	/* Catch clauses (inline) */` |
|      103 | 11298 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       98 | 11299 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       59 | 11300 | `		sxu32 k = 0;` |
|       81 | 11301 | `		for(;;){` |
|        - | 11302 | `			ph7_exception_block sCatch;` |
|        - | 11303 | `			GenBlock *pCatchBlk;` |
|      113 | 11304 | `			sxu32 idxJmp = 0;` |
|      108 | 11305 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      104 | 11306 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       32 | 11307 | `				break;` |
|        - | 11308 | `			}` |
|       59 | 11309 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       59 | 11310 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 11311 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       59 | 11312 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       59 | 11313 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       59 | 11314 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       59 | 11315 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 11316 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 11317 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 11318 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       59 | 11319 | `			pCatchBlk->pUserData = pException;` |
|       59 | 11320 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       59 | 11321 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 11322 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       59 | 11323 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11324 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 11325 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       59 | 11326 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       59 | 11327 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       59 | 11328 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       59 | 11329 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       59 | 11330 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       59 | 11331 | `			k++;` |
|        5 | 11332 | `		}` |
|       27 | 11333 | `	}` |
|        - | 11334 | `	/* Finally (inline) */` |
|      103 | 11335 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 11336 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 11337 | `		GenBlock *pFinBlk;` |
|       52 | 11338 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 11339 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 11340 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 11341 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 11342 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 11343 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 11344 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 11345 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 11346 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 11347 | `		pException->iHasFinally = 1;` |
|       24 | 11348 | `	}` |
|      103 | 11349 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      103 | 11350 | `	pException->iInlined = 1;` |
|        - | 11351 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 11352 | `	{` |
|      103 | 11353 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 11354 | `		sxu32 *aJ; sxu32 n;` |
|      103 | 11355 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      103 | 11356 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      103 | 11357 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      157 | 11358 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       59 | 11359 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       59 | 11360 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       32 | 11361 | `		}` |
|        - | 11362 | `	}` |
|      103 | 11363 | `	SySetRelease(&aCatchJmp);` |
|      103 | 11364 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 11365 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 11366 | `	}` |
|      103 | 11367 | `	return SXRET_OK;` |
|       54 | 11368 | `}` |
|        - | 11369 | `/*` |
|        - | 11370 | ` * Compile a 'catch' block.` |
|        - | 11371 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 11372 | ` * an object containing the exception information.` |
|        - | 11373 | ` */` |
|     5200 | 11374 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 11375 | `{` |
|     5205 | 11376 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11377 | `	ph7_exception_block sCatch;` |
|        - | 11378 | `	SySet *pInstrContainer;` |
|        - | 11379 | `	SyString sClassName;` |
|        - | 11380 | `	GenBlock *pCatch;` |
|        - | 11381 | `	SyToken *pToken;` |
|        - | 11382 | `	SyString *pName;` |
|        - | 11383 | `	char *zDup;` |
|        - | 11384 | `	sxi32 rc;` |
|     5205 | 11385 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 11386 | `	/* Zero the structure */` |
|     5205 | 11387 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 11388 | `	/* Initialize fields */` |
|     5205 | 11389 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     5205 | 11390 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     5205 | 11391 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 11392 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 11393 | `			pToken = pGen->pIn;` |
|      ! 0 | 11394 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11395 | `				pToken--;` |
|      ! 0 | 11396 | `			}` |
|      ! 0 | 11397 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11398 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11399 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11400 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11401 | `				return SXERR_ABORT;` |
|        - | 11402 | `			}` |
|      ! 0 | 11403 | `			return SXERR_INVALID;` |
|        - | 11404 | `	}` |
|        - | 11405 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     5205 | 11406 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     2614 | 11407 | `	for(;;){` |
|        - | 11408 | `		SyBlob sResolved;` |
|     5233 | 11409 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     5233 | 11410 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 11411 | `			SyBlobRelease(&sResolved);` |
|        6 | 11412 | `			pToken = pGen->pIn;` |
|        6 | 11413 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11414 | `				pToken--;` |
|      ! 0 | 11415 | `			}` |
|        8 | 11416 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11417 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 11418 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 11419 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11420 | `				return SXERR_ABORT;` |
|        - | 11421 | `			}` |
|        6 | 11422 | `			return SXERR_INVALID;` |
|        - | 11423 | `		}` |
|        - | 11424 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 11425 | `		 * transient SyBlob allocation. */` |
|     7841 | 11426 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     5224 | 11427 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     5229 | 11428 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     5229 | 11429 | `		SyBlobRelease(&sResolved);` |
|     5229 | 11430 | `		if( zDup == 0 ){` |
|      ! 0 | 11431 | `			goto Mem;` |
|        - | 11432 | `		}` |
|     5229 | 11433 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     5229 | 11434 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11435 | `			goto Mem;` |
|        - | 11436 | `		}` |
|        - | 11437 | `		/* Check for '\|' (multi-catch separator) */` |
|     5224 | 11438 | `		if( pGen->pIn < pGen->pEnd &&` |
|     5224 | 11439 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       33 | 11440 | `			pGen->pIn->sData.nByte == 1 &&` |
|       28 | 11441 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       30 | 11442 | `			pGen->pIn++; /* Consume the '\|' */` |
|       30 | 11443 | `			continue;` |
|        - | 11444 | `		}` |
|     5201 | 11445 | `		break;` |
|      ! 0 | 11446 | `	}` |
|     5196 | 11447 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     5201 | 11448 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 11449 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 11450 | `			pToken = pGen->pIn;` |
|      ! 0 | 11451 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11452 | `				pToken--;` |
|      ! 0 | 11453 | `			}` |
|      ! 0 | 11454 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11455 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11456 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11457 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11458 | `				return SXERR_ABORT;` |
|        - | 11459 | `			}` |
|      ! 0 | 11460 | `			return SXERR_INVALID;` |
|        - | 11461 | `	}` |
|     5201 | 11462 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 11463 | `	/* Duplicate instance name */` |
|     5201 | 11464 | `	pName = &pGen->pIn->sData;` |
|     5201 | 11465 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     5201 | 11466 | `	if( zDup == 0 ){` |
|      ! 0 | 11467 | `		goto Mem;` |
|        - | 11468 | `	}` |
|     5201 | 11469 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     5201 | 11470 | `	pGen->pIn++;` |
|     5201 | 11471 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 11472 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 11473 | `		pToken = pGen->pIn;` |
|      ! 0 | 11474 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11475 | `			pToken--;` |
|      ! 0 | 11476 | `		}` |
|      ! 0 | 11477 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11478 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11479 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11480 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11481 | `			return SXERR_ABORT;` |
|        - | 11482 | `		}` |
|      ! 0 | 11483 | `		return SXERR_INVALID;` |
|        - | 11484 | `	}` |
|        - | 11485 | `	/* Compile the block */` |
|     5201 | 11486 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 11487 | `	/* Create the catch block */` |
|     5201 | 11488 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     5201 | 11489 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11490 | `		return SXERR_ABORT;` |
|        - | 11491 | `	}` |
|        - | 11492 | `	/* Swap bytecode container */` |
|     5201 | 11493 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     5201 | 11494 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 11495 | `	/* Compile the block */` |
|     5201 | 11496 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 11497 | `	/* Fix forward jumps now the destination is resolved  */` |
|     5201 | 11498 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11499 | `	/* Emit the DONE instruction */` |
|     5201 | 11500 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 11501 | `	/* Leave the block */` |
|     5201 | 11502 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11503 | `	/* Restore the default container */` |
|     5201 | 11504 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 11505 | `	/* Install the catch block */` |
|     5201 | 11506 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     5201 | 11507 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11508 | `		goto Mem;` |
|        - | 11509 | `	}` |
|     5201 | 11510 | `	return SXRET_OK;` |
|      ! 0 | 11511 | `Mem:` |
|      ! 0 | 11512 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11513 | `	return SXERR_ABORT;` |
|     2605 | 11514 | `}` |
|        - | 11515 | `/*` |
|        - | 11516 | ` * Compile a 'try' block.` |
|        - | 11517 | ` * A function using an exception should be in a "try" block.` |
|        - | 11518 | ` * If the exception does not trigger, the code will continue` |
|        - | 11519 | ` * as normal. However if the exception triggers, an exception` |
|        - | 11520 | ` * is "thrown".` |
|        - | 11521 | ` */` |
|     5356 | 11522 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 11523 | `{` |
|        - | 11524 | `	ph7_exception *pException;` |
|     5361 | 11525 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11526 | `	GenBlock *pTry;` |
|        - | 11527 | `	sxu32 nJmpIdx;` |
|        - | 11528 | `	sxi32 rc;` |
|        - | 11529 | `	/* Create the exception container */` |
|     5361 | 11530 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     5361 | 11531 | `	if( pException == 0 ){` |
|      ! 0 | 11532 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 11533 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11534 | `		return SXERR_ABORT;` |
|        - | 11535 | `	}` |
|        - | 11536 | `	/* Zero the structure */` |
|     5361 | 11537 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 11538 | `	/* Initialize fields */` |
|     5361 | 11539 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     5361 | 11540 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     5361 | 11541 | `	pException->iHasFinally = 0;` |
|     5361 | 11542 | `	pException->iFinallyDone = 0;` |
|     5361 | 11543 | `	pException->pVm = pGen->pVm;` |
|        - | 11544 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 11545 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 11546 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 11547 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 11548 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 11549 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     5361 | 11550 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      103 | 11551 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 11552 | `	}` |
|        - | 11553 | `	/* Create the try block */` |
|     5263 | 11554 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     5263 | 11555 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11556 | `		return SXERR_ABORT;` |
|        - | 11557 | `	}` |
|        - | 11558 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     5263 | 11559 | `	pTry->pUserData = pException;` |
|        - | 11560 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     5263 | 11561 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 11562 | `	/* Fix the jump later when the destination is resolved */` |
|     5263 | 11563 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     5263 | 11564 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 11565 | `	/* Compile the block */` |
|     5263 | 11566 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     5263 | 11567 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11568 | `		return SXERR_ABORT;` |
|        - | 11569 | `	}` |
|        - | 11570 | `	/* Fix forward jumps now the destination is resolved */` |
|     5263 | 11571 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11572 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     5263 | 11573 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 11574 | `	/* Leave the block */` |
|     5263 | 11575 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11576 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     5263 | 11577 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     5256 | 11578 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 11579 | `		/* Compile one or more catch blocks */` |
|     5196 | 11580 | `		for(;;){` |
|    10392 | 11581 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     7830 | 11582 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     2601 | 11583 | `					break;` |
|        - | 11584 | `			}` |
|     5205 | 11585 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     5205 | 11586 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11587 | `				return SXERR_ABORT;` |
|        - | 11588 | `			}` |
|        5 | 11589 | `		}` |
|     2596 | 11590 | `	}` |
|        - | 11591 | `	/* Compile optional finally block */` |
|     5263 | 11592 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      644 | 11593 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 11594 | `		SySet *pInstrContainer;` |
|        - | 11595 | `		GenBlock *pFinBlock;` |
|      129 | 11596 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 11597 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 11598 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 11599 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11600 | `			return SXERR_ABORT;` |
|        - | 11601 | `		}` |
|        - | 11602 | `		/* Swap bytecode container */` |
|      129 | 11603 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 11604 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 11605 | `		/* Compile the finally body */` |
|      129 | 11606 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 11607 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11608 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 11609 | `			return SXERR_ABORT;` |
|        - | 11610 | `		}` |
|        - | 11611 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 11612 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11613 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 11614 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 11615 | `		/* Leave the block */` |
|      129 | 11616 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11617 | `		/* Restore the default container */` |
|      129 | 11618 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 11619 | `		pException->iHasFinally = 1;` |
|       62 | 11620 | `	}` |
|        - | 11621 | `	/* Must have at least one catch or finally */` |
|     5263 | 11622 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        8 | 11623 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11624 | `			"Cannot use try without catch or finally");` |
|        8 | 11625 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11626 | `			return SXERR_ABORT;` |
|        - | 11627 | `		}` |
|        3 | 11628 | `	}` |
|     5263 | 11629 | `	return SXRET_OK;` |
|     2683 | 11630 | `}` |
|        - | 11631 | `/*` |
|        - | 11632 | ` * Compile a switch block.` |
|        - | 11633 | ` *  (See block-comment below for more information)` |
|        - | 11634 | ` */` |
|      112 | 11635 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 11636 | `{` |
|      117 | 11637 | `	sxi32 rc = SXRET_OK;` |
|      117 | 11638 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 11639 | `		/* Unexpected token */` |
|      ! 0 | 11640 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 11641 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11642 | `			return SXERR_ABORT;` |
|        - | 11643 | `		}` |
|      ! 0 | 11644 | `		pGen->pIn++;` |
|      ! 0 | 11645 | `	}` |
|      117 | 11646 | `	pGen->pIn++;` |
|        - | 11647 | `	/* First instruction to execute in this block. */` |
|      117 | 11648 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 11649 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 11650 | `	 * or the '}' token */` |
|      206 | 11651 | `	for(;;){` |
|      417 | 11652 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 11653 | `			/* No more input to process */` |
|      ! 0 | 11654 | `			break;` |
|        - | 11655 | `		}` |
|      417 | 11656 | `		rc = SXRET_OK;` |
|      417 | 11657 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       85 | 11658 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|       31 | 11659 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 11660 | `					/* Unexpected token */` |
|      ! 0 | 11661 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 11662 | `						&pGen->pIn->sData);` |
|      ! 0 | 11663 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11664 | `						return SXERR_ABORT;` |
|        - | 11665 | `					}` |
|        - | 11666 | `					/* FALL THROUGH */` |
|      ! 0 | 11667 | `				}` |
|       31 | 11668 | `				rc = SXERR_EOF;` |
|       31 | 11669 | `				break;` |
|        - | 11670 | `			}` |
|       32 | 11671 | `		}else{` |
|        - | 11672 | `			sxi32 nKwrd;` |
|        - | 11673 | `			/* Extract the keyword */` |
|      337 | 11674 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      337 | 11675 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|       47 | 11676 | `				break;` |
|        - | 11677 | `			}` |
|      253 | 11678 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 11679 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 11680 | `					/* Unexpected token */` |
|      ! 0 | 11681 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 11682 | `						&pGen->pIn->sData);` |
|      ! 0 | 11683 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11684 | `						return SXERR_ABORT;` |
|        - | 11685 | `					}` |
|        - | 11686 | `					/* FALL THROUGH */` |
|      ! 0 | 11687 | `				}` |
|        - | 11688 | `				/* Block compiled */` |
|        3 | 11689 | `				break;` |
|        - | 11690 | `			}` |
|        - | 11691 | `		}` |
|        - | 11692 | `		/* Compile block */` |
|      305 | 11693 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      305 | 11694 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11695 | `			return SXERR_ABORT;` |
|        - | 11696 | `		}` |
|        5 | 11697 | `	}` |
|      117 | 11698 | `	return rc;` |
|       61 | 11699 | `}` |
|        - | 11700 | `/*` |
|        - | 11701 | ` * Compile a case eXpression.` |
|        - | 11702 | ` *  (See block-comment below for more information)` |
|        - | 11703 | ` */` |
|       92 | 11704 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 11705 | `{` |
|        - | 11706 | `	SySet *pInstrContainer;` |
|        - | 11707 | `	SyToken *pEnd,*pTmp;` |
|       97 | 11708 | `	sxi32 iNest = 0;` |
|        - | 11709 | `	sxi32 rc;` |
|        - | 11710 | `	/* Delimit the expression */` |
|       97 | 11711 | `	pEnd = pGen->pIn;` |
|      197 | 11712 | `	while( pEnd < pGen->pEnd ){` |
|      197 | 11713 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 11714 | `			/* Increment nesting level */` |
|        3 | 11715 | `			iNest++;` |
|      196 | 11716 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 11717 | `			/* Decrement nesting level */` |
|        3 | 11718 | `			iNest--;` |
|      194 | 11719 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|       97 | 11720 | `			break;` |
|        - | 11721 | `		}` |
|      105 | 11722 | `		pEnd++;` |
|        5 | 11723 | `	}` |
|       97 | 11724 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 11725 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 11726 | `		if( rc == SXERR_ABORT ){` |
|        - | 11727 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11728 | `			return SXERR_ABORT;` |
|        - | 11729 | `		}` |
|      ! 0 | 11730 | `	}` |
|        - | 11731 | `	/* Swap token stream */` |
|       97 | 11732 | `	pTmp = pGen->pEnd;` |
|       97 | 11733 | `	pGen->pEnd = pEnd;` |
|       97 | 11734 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       97 | 11735 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|       97 | 11736 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 11737 | `	/* Emit the done instruction */` |
|       97 | 11738 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       97 | 11739 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 11740 | `	/* Update token stream */` |
|       97 | 11741 | `	pGen->pIn  = pEnd;` |
|       97 | 11742 | `	pGen->pEnd = pTmp;` |
|       97 | 11743 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11744 | `		return SXERR_ABORT;` |
|        - | 11745 | `	}` |
|       97 | 11746 | `	return SXRET_OK;` |
|       51 | 11747 | `}` |
|        - | 11748 | `/*` |
|        - | 11749 | ` * Compile the smart switch statement.` |
|        - | 11750 | ` * According to the PHP language reference manual` |
|        - | 11751 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 11752 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 11753 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 11754 | ` *  This is exactly what the switch statement is for.` |
|        - | 11755 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 11756 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 11757 | ` *  of the outer loop, use continue 2.` |
|        - | 11758 | ` *  Note that switch/case does loose comparision.` |
|        - | 11759 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 11760 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 11761 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 11762 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 11763 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 11764 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 11765 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 11766 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 11767 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 11768 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 11769 | ` *  list for the next case.` |
|        - | 11770 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 11771 | ` *  or floating-point numbers and strings.` |
|        - | 11772 | ` */` |
|       28 | 11773 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 11774 | `{` |
|        - | 11775 | `	GenBlock *pSwitchBlock;` |
|        - | 11776 | `	SyToken *pTmp,*pEnd;` |
|        - | 11777 | `	ph7_switch *pSwitch;` |
|        - | 11778 | `	sxu32 nToken;` |
|        - | 11779 | `	sxu32 nLine;` |
|        - | 11780 | `	sxi32 rc;` |
|       33 | 11781 | `	nLine = pGen->pIn->nLine;` |
|        - | 11782 | `	/* Jump the 'switch' keyword */` |
|       33 | 11783 | `	pGen->pIn++;` |
|       33 | 11784 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 11785 | `		/* Syntax error */` |
|      ! 0 | 11786 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 11787 | `		if( rc == SXERR_ABORT ){` |
|        - | 11788 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11789 | `			return SXERR_ABORT;` |
|        - | 11790 | `		}` |
|      ! 0 | 11791 | `		goto Synchronize;` |
|        - | 11792 | `	}` |
|        - | 11793 | `	/* Jump the left parenthesis '(' */` |
|       33 | 11794 | `	pGen->pIn++;` |
|       33 | 11795 | `	pEnd = 0; /* cc warning */` |
|        - | 11796 | `	/* Create the loop block */` |
|       47 | 11797 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|       14 | 11798 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|       33 | 11799 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11800 | `		return SXERR_ABORT;` |
|        - | 11801 | `	}` |
|        - | 11802 | `	/* Delimit the condition */` |
|       33 | 11803 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       33 | 11804 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 11805 | `		/* Empty expression */` |
|      ! 0 | 11806 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 11807 | `		if( rc == SXERR_ABORT ){` |
|        - | 11808 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11809 | `			return SXERR_ABORT;` |
|        - | 11810 | `		}` |
|      ! 0 | 11811 | `	}` |
|        - | 11812 | `	/* Swap token streams */` |
|       33 | 11813 | `	pTmp = pGen->pEnd;` |
|       33 | 11814 | `	pGen->pEnd = pEnd;` |
|        - | 11815 | `	/* Compile the expression */` |
|       33 | 11816 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       33 | 11817 | `	if( rc == SXERR_ABORT ){` |
|        - | 11818 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 11819 | `		return SXERR_ABORT;` |
|        - | 11820 | `	}` |
|        - | 11821 | `	/* Update token stream */` |
|       33 | 11822 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 11823 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 11824 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 11825 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11826 | `			return SXERR_ABORT;` |
|        - | 11827 | `		}` |
|      ! 0 | 11828 | `		pGen->pIn++;` |
|      ! 0 | 11829 | `	}` |
|       33 | 11830 | `	pGen->pIn  = &pEnd[1];` |
|       33 | 11831 | `	pGen->pEnd = pTmp;` |
|       33 | 11832 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       28 | 11833 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 11834 | `			pTmp = pGen->pIn;` |
|      ! 0 | 11835 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 11836 | `				pTmp--;` |
|      ! 0 | 11837 | `			}` |
|        - | 11838 | `			/* Unexpected token */` |
|      ! 0 | 11839 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 11840 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11841 | `				return SXERR_ABORT;` |
|        - | 11842 | `			}` |
|      ! 0 | 11843 | `			goto Synchronize;` |
|        - | 11844 | `	}` |
|        - | 11845 | `	/* Set the delimiter token */` |
|       33 | 11846 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 11847 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 11848 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 11849 | `	}else{` |
|       31 | 11850 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 11851 | `	}` |
|       33 | 11852 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 11853 | `	/* Create the switch blocks container */` |
|       33 | 11854 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|       33 | 11855 | `	if( pSwitch == 0 ){` |
|        - | 11856 | `		/* Abort compilation */` |
|      ! 0 | 11857 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11858 | `		return SXERR_ABORT;` |
|        - | 11859 | `	}` |
|        - | 11860 | `	/* Zero the structure */` |
|       33 | 11861 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 11862 | `	/* Initialize fields */` |
|       33 | 11863 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 11864 | `	/* Emit the switch instruction */` |
|       33 | 11865 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 11866 | `	/* Compile case blocks */` |
|      100 | 11867 | `	for(;;){` |
|        - | 11868 | `		sxu32 nKwrd;` |
|      119 | 11869 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 11870 | `			/* No more input to process */` |
|      ! 0 | 11871 | `			break;` |
|        - | 11872 | `		}` |
|      119 | 11873 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 11874 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 11875 | `				/* Unexpected token */` |
|      ! 0 | 11876 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 11877 | `					&pGen->pIn->sData);` |
|      ! 0 | 11878 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11879 | `					return SXERR_ABORT;` |
|        - | 11880 | `				}` |
|        - | 11881 | `				/* FALL THROUGH */` |
|      ! 0 | 11882 | `			}` |
|        - | 11883 | `			/* Block compiled */` |
|      ! 0 | 11884 | `			break;` |
|        - | 11885 | `		}` |
|        - | 11886 | `		/* Extract the keyword */` |
|      119 | 11887 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      119 | 11888 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 11889 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 11890 | `				/* Unexpected token */` |
|      ! 0 | 11891 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 11892 | `					&pGen->pIn->sData);` |
|      ! 0 | 11893 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11894 | `					return SXERR_ABORT;` |
|        - | 11895 | `				}` |
|        - | 11896 | `				/* FALL THROUGH */` |
|      ! 0 | 11897 | `			}` |
|        - | 11898 | `			/* Block compiled */` |
|        3 | 11899 | `			break;` |
|        - | 11900 | `		}` |
|      117 | 11901 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 11902 | `			/*` |
|        - | 11903 | `			 * Accroding to the PHP language reference manual` |
|        - | 11904 | `			 *  A special case is the default case. This case matches anything` |
|        - | 11905 | `			 *  that wasn't matched by the other cases.` |
|        - | 11906 | `			 */` |
|       25 | 11907 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 11908 | `				/* Default case already compiled */` |
|      ! 0 | 11909 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 11910 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11911 | `					return SXERR_ABORT;` |
|        - | 11912 | `				}` |
|      ! 0 | 11913 | `			}` |
|       25 | 11914 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 11915 | `			/* Compile the default block */` |
|       25 | 11916 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|       25 | 11917 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 11918 | `				return SXERR_ABORT;` |
|       25 | 11919 | `			}else if( rc == SXERR_EOF ){` |
|       23 | 11920 | `				break;` |
|        1 | 11921 | `			}` |
|       98 | 11922 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 11923 | `			ph7_case_expr sCase;` |
|        - | 11924 | `			/* Standard case block */` |
|       97 | 11925 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 11926 | `			/* initialize the structure */` |
|       97 | 11927 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 11928 | `			/* Compile the case expression */` |
|       97 | 11929 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|       97 | 11930 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11931 | `				return SXERR_ABORT;` |
|        - | 11932 | `			}` |
|        - | 11933 | `			/* Compile the case block */` |
|       97 | 11934 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 11935 | `			/* Insert in the switch container */` |
|       97 | 11936 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|       97 | 11937 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 11938 | `				return SXERR_ABORT;` |
|       97 | 11939 | `			}else if( rc == SXERR_EOF ){` |
|        9 | 11940 | `				break;` |
|        - | 11941 | `			}` |
|       47 | 11942 | `		}else{` |
|        - | 11943 | `			/* Unexpected token */` |
|      ! 0 | 11944 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 11945 | `				&pGen->pIn->sData);` |
|      ! 0 | 11946 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11947 | `				return SXERR_ABORT;` |
|        - | 11948 | `			}` |
|      ! 0 | 11949 | `			break;` |
|        - | 11950 | `		}` |
|        5 | 11951 | `	}` |
|        - | 11952 | `	/* Fix all jumps now the destination is resolved */` |
|       33 | 11953 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|       33 | 11954 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11955 | `	/* Release the loop block */` |
|       33 | 11956 | `	GenStateLeaveBlock(pGen,0);` |
|       33 | 11957 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 11958 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|       33 | 11959 | `		pGen->pIn++;` |
|       14 | 11960 | `	}` |
|        - | 11961 | `	/* Statement successfully compiled */` |
|       33 | 11962 | `	return SXRET_OK;` |
|      ! 0 | 11963 | `Synchronize:` |
|        - | 11964 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 11965 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 11966 | `		pGen->pIn++;` |
|      ! 0 | 11967 | `	}` |
|      ! 0 | 11968 | `	return SXRET_OK;` |
|       19 | 11969 | `}` |
|        - | 11970 | `/*` |
|        - | 11971 | ` * Chain operators participate in a postfix member-access chain.` |
|        - | 11972 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|        - | 11973 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|        - | 11974 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|        - | 11975 | ` */` |
|        - | 11976 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|        - | 11977 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|        - | 11978 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|        - | 11979 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|        - | 11980 |  |
|        - | 11981 | `/*` |
|        - | 11982 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|        - | 11983 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|        - | 11984 | ` * patched entries from the pending set.` |
|        - | 11985 | ` */` |
| 22756136 | 11986 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|        5 | 11987 | `{` |
| 22756141 | 11988 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|        - | 11989 | `	sxu32 nTarget;` |
|        - | 11990 | `	sxu32 *aIdx;` |
|        - | 11991 | `	sxu32 i;` |
| 22756141 | 11992 | `	if( nCur <= nBaseline ){` |
| 22756045 | 11993 | `		return;` |
|        - | 11994 | `	}` |
|      100 | 11995 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      100 | 11996 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|      204 | 11997 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|      108 | 11998 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|      108 | 11999 | `		if( pInstr ){` |
|      108 | 12000 | `			pInstr->iP2 = (sxi32)nTarget;` |
|       52 | 12001 | `		}` |
|       56 | 12002 | `	}` |
|      100 | 12003 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 11378073 | 12004 | `}` |
|        - | 12005 |  |
|        - | 12006 | `/*` |
|        - | 12007 | ` * By-reference out-parameters of builtin functions.` |
|        - | 12008 | ` *` |
|        - | 12009 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|        - | 12010 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|        - | 12011 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|        - | 12012 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|        - | 12013 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|        - | 12014 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|        - | 12015 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|        - | 12016 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|        - | 12017 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|        - | 12018 | ` * creates it" behaviour).` |
|        - | 12019 | ` *` |
|        - | 12020 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|        - | 12021 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|        - | 12022 | ` */` |
|  3184202 | 12023 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|        5 | 12024 | `{` |
|        - | 12025 | `	static const struct {` |
|        - | 12026 | `		const char *zName;` |
|        - | 12027 | `		sxu32 nByte;` |
|        - | 12028 | `		sxu32 mask;` |
|        - | 12029 | `	} aByRef[] = {` |
|        - | 12030 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12031 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12032 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12033 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12034 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|        - | 12035 | `	};` |
|        - | 12036 | `	sxu32 i;` |
|  3184207 | 12037 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   834781 | 12038 | `		return 0;` |
|        - | 12039 | `	}` |
| 14096167 | 12040 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 11746858 | 12041 | `		if( pName->nByte == aByRef[i].nByte` |
|  6016889 | 12042 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      127 | 12043 | `			return aByRef[i].mask;` |
|        - | 12044 | `		}` |
|  5873373 | 12045 | `	}` |
|  2349309 | 12046 | `	return 0;` |
|  1592106 | 12047 | `}` |
|        - | 12048 | `/*` |
|        - | 12049 | ` * Recover the bare global-builtin name from a call's callee node.` |
|        - | 12050 | ` *` |
|        - | 12051 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|        - | 12052 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|        - | 12053 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|        - | 12054 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|        - | 12055 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|        - | 12056 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|        - | 12057 | ` */` |
|  3184202 | 12058 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|        5 | 12059 | `{` |
|        - | 12060 | `	SyToken *p, *pEnd;` |
|  3184207 | 12061 | `	pOut->zString = 0;` |
|  3184207 | 12062 | `	pOut->nByte = 0;` |
|  3184207 | 12063 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|      ! 0 | 12064 | `		return;` |
|        - | 12065 | `	}` |
|  3184207 | 12066 | `	p = pLeft->pStart;` |
|  3184207 | 12067 | `	pEnd = pLeft->pEnd;` |
|        - | 12068 | `	/* Optional single leading namespace separator (absolute path). */` |
|  3184207 | 12069 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|     3917 | 12070 | `		p++;` |
|     1956 | 12071 | `	}` |
|  3184207 | 12072 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   834745 | 12073 | `		return;` |
|        - | 12074 | `	}` |
|        - | 12075 | `	/* Must be a single component: nothing follows the name token. */` |
|  2349467 | 12076 | `	if( p + 1 != pEnd ){` |
|       40 | 12077 | `		return;` |
|        - | 12078 | `	}` |
|  2349431 | 12079 | `	*pOut = p->sData;` |
|  1592106 | 12080 | `}` |
|        - | 12081 | `/*` |
|        - | 12082 | ` * Generate bytecode for a given expression tree.` |
|        - | 12083 | ` * If something goes wrong while generating bytecode` |
|        - | 12084 | ` * for the expression tree (A very unlikely scenario)` |
|        - | 12085 | ` * this function takes care of generating the appropriate` |
|        - | 12086 | ` * error message.` |
|        - | 12087 | ` */` |
| 31528002 | 12088 | `static sxi32 GenStateEmitExprCode(` |
|        - | 12089 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 12090 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|        - | 12091 | `	sxi32 iFlags /* Control flags */` |
|        - | 12092 | `	)` |
|        5 | 12093 | `{` |
|        - | 12094 | `	VmInstr *pInstr;` |
|        - | 12095 | `	sxu32 nJmpIdx;` |
| 31528007 | 12096 | `	sxi32 iP1 = 0;` |
| 31528007 | 12097 | `	sxu32 iP2 = 0;` |
| 31528007 | 12098 | `	void *p3  = 0;` |
|        - | 12099 | `	sxi32 iVmOp;` |
|        - | 12100 | `	sxi32 rc;` |
| 31528007 | 12101 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 31528007 | 12102 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 31528007 | 12103 | `	sxu32 nRhsNsBase = 0;` |
| 31528007 | 12104 | `	if( pNode->xCode ){` |
|        - | 12105 | `		SyToken *pTmpIn,*pTmpEnd;` |
|        - | 12106 | `		/* Compile node */` |
| 18934809 | 12107 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 18934809 | 12108 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 18934809 | 12109 | `		RE_SWAP_DELIMITER(pGen);` |
| 18934809 | 12110 | `		return rc;` |
|        - | 12111 | `	}` |
| 12593203 | 12112 | `	if( pNode->pOp == 0 ){` |
|      ! 0 | 12113 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12114 | `			"Invalid expression node,PH7 is aborting compilation");` |
|      ! 0 | 12115 | `		return SXERR_ABORT;` |
|        - | 12116 | `	}` |
| 12593203 | 12117 | `	iVmOp = pNode->pOp->iVmOp;` |
| 12593203 | 12118 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|        - | 12119 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|        - | 12120 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|        - | 12121 | `		 * and later errors are still reported. */` |
|        3 | 12122 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12123 | `			"The (unset) cast is no longer supported");` |
|        3 | 12124 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12125 | `			return SXERR_ABORT;` |
|        - | 12126 | `		}` |
|        1 | 12127 | `	}` |
| 12593203 | 12128 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|       65 | 12129 | `		sxu32 nJmp = 0;` |
|        - | 12130 | `		sxu32 nNcNsBase;` |
|        - | 12131 | `		VmInstr *pInstrFix;` |
|        - | 12132 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|        - | 12133 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|        - | 12134 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|        - | 12135 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|        - | 12136 | `		 * stack slot carries a writable nIdx. */` |
|       65 | 12137 | `		if( pNode->pRight ){` |
|       65 | 12138 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       65 | 12139 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       65 | 12140 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12141 | `				return rc;` |
|        - | 12142 | `			}` |
|       65 | 12143 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        - | 12144 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|        - | 12145 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|        - | 12146 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|        - | 12147 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|        - | 12148 | `			 * the store, so the parent array does not need to be copied at` |
|        - | 12149 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|        - | 12150 | `			 * cascade for the actual write path stays correct. */` |
|       65 | 12151 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|       65 | 12152 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       31 | 12153 | `				pInstrFix->iP2 = 3;` |
|       14 | 12154 | `			}` |
|       31 | 12155 | `		}` |
|        - | 12156 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|       65 | 12157 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|        - | 12158 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|       65 | 12159 | `		if( pNode->pLeft ){` |
|       65 | 12160 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       65 | 12161 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|       65 | 12162 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12163 | `				return rc;` |
|        - | 12164 | `			}` |
|       65 | 12165 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       31 | 12166 | `		}` |
|        - | 12167 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|       65 | 12168 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|        - | 12169 | `		/* Patch the short-circuit jump to land after the store. */` |
|       65 | 12170 | `		if( nJmp > 0 ){` |
|       65 | 12171 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|       65 | 12172 | `			if( pInstrFix ){` |
|       65 | 12173 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|       31 | 12174 | `			}` |
|       31 | 12175 | `		}` |
|       65 | 12176 | `		return SXRET_OK;` |
|        - | 12177 | `	}` |
| 12593141 | 12178 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|        - | 12179 | `		sxu32 nJz,nJmp;` |
|        - | 12180 | `		sxu32 nTernaryNsBase;` |
|        - | 12181 | `		/* Ternary operator require special handling */` |
|        - | 12182 | `		/* Phase#1: Compile the condition */` |
|   205123 | 12183 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   205123 | 12184 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|   205123 | 12185 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12186 | `			return rc;` |
|        - | 12187 | `		}` |
|        - | 12188 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|        - | 12189 | `		 * compiling the condition must short-circuit to the end of the` |
|        - | 12190 | `		 * condition expression, not leak past the ternary. */` |
|   205123 | 12191 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   205123 | 12192 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|   205123 | 12193 | `		if( pNode->pLeft ){` |
|        - | 12194 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|        - | 12195 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|   205055 | 12196 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 12197 | `			/* Phase#3: Compile the 'then' expression  */` |
|   205055 | 12198 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   205055 | 12199 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|   205055 | 12200 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12201 | `				return rc;` |
|        - | 12202 | `			}` |
|   205055 | 12203 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   102530 | 12204 | `		}else{` |
|        - | 12205 | `			/* Elvis operator: (expr) ?: (else)` |
|        - | 12206 | `			 * Duplicate condition so original value is the 'then' result.` |
|        - | 12207 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       70 | 12208 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       70 | 12209 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 12210 | `		}` |
|        - | 12211 | `		/* Phase#4: Emit the unconditional jump */` |
|   205123 | 12212 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|        - | 12213 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|   205123 | 12214 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|   205123 | 12215 | `		if( pInstr ){` |
|   205123 | 12216 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   102559 | 12217 | `		}` |
|   205123 | 12218 | `		if( !pNode->pLeft ){` |
|        - | 12219 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       70 | 12220 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       34 | 12221 | `		}` |
|        - | 12222 | `		/* Phase#6: Compile the 'else' expression */` |
|   205123 | 12223 | `		if( pNode->pRight ){` |
|   205123 | 12224 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   205123 | 12225 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|   205123 | 12226 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12227 | `				return rc;` |
|        - | 12228 | `			}` |
|   205123 | 12229 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   102559 | 12230 | `		}` |
|   205123 | 12231 | `		if( nJmp > 0 ){` |
|        - | 12232 | `			/* Phase#7: Fix the unconditional jump */` |
|   205123 | 12233 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|   205123 | 12234 | `			if( pInstr ){` |
|   205123 | 12235 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   102559 | 12236 | `			}` |
|   102559 | 12237 | `		}` |
|        - | 12238 | `		/* All done */` |
|   205123 | 12239 | `		return SXRET_OK;` |
|        - | 12240 | `	}` |
| 12388023 | 12241 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|        - | 12242 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|        - | 12243 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|        - | 12244 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|        - | 12245 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|        - | 12246 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|        - | 12247 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|        - | 12248 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|        - | 12249 | `		sxu32 nPipeNsBase;` |
|       27 | 12250 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|       27 | 12251 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|      ! 0 | 12252 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12253 | `				"'\|>': Missing operand");` |
|      ! 0 | 12254 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 12255 | `		}` |
|        - | 12256 | `		/* Argument: the LHS value. */` |
|       27 | 12257 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 12258 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|       27 | 12259 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12260 | `			return rc;` |
|        - | 12261 | `		}` |
|       27 | 12262 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 12263 | `		/* Callable: the RHS. */` |
|       27 | 12264 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 12265 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|       27 | 12266 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12267 | `			return rc;` |
|        - | 12268 | `		}` |
|       27 | 12269 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 12270 | `		/* Invoke the callable with the single piped argument. */` |
|       27 | 12271 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       27 | 12272 | `		return SXRET_OK;` |
|        - | 12273 | `	}` |
| 12387997 | 12274 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|        - | 12275 | `	/* Generate code for the left tree */` |
| 12387997 | 12276 | `	if( pNode->pLeft ){` |
| 12376313 | 12277 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 12376313 | 12278 | `		if( iVmOp == PH7_OP_CALL ){` |
|        - | 12279 | `			ph7_expr_node **apNode;` |
|  3188405 | 12280 | `			int hasSpread = 0;` |
|  3188405 | 12281 | `			int hasNamed = 0;` |
|  3188405 | 12282 | `			int bAnySpread = 0;` |
|  3188405 | 12283 | `			sxu32 byRefMask = 0;` |
|        - | 12284 | `			sxi32 nArgs;` |
|        - | 12285 | `			sxi32 n;` |
|        - | 12286 | `			/* Recurse and generate bytecodes for function arguments */` |
|  3188405 | 12287 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3188405 | 12288 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|        - | 12289 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|        - | 12290 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|        - | 12291 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  3188405 | 12292 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|       79 | 12293 | `				bFcc = 1;` |
|       79 | 12294 | `				nArgs = 0;` |
|       39 | 12295 | `			}` |
|        - | 12296 | `			/* Validate argument order like php: no positional argument after a` |
|        - | 12297 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|        - | 12298 | `			{` |
|  3188405 | 12299 | `				int seenNamed = 0;` |
|  3188405 | 12300 | `				int seenSpread = 0;` |
|  6332547 | 12301 | `				for( n = 0; n < nArgs; ++n ){` |
|  3144149 | 12302 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|     4071 | 12303 | `						bAnySpread = 1;` |
|     4071 | 12304 | `						seenSpread = 1;` |
|     4071 | 12305 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      ! 0 | 12306 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12307 | `								"syntax error, unexpected token \"...\"");` |
|      ! 0 | 12308 | `							return SXERR_SYNTAX;` |
|        5 | 12309 | `						}` |
|  3142116 | 12310 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      289 | 12311 | `						seenNamed = 1;` |
|      289 | 12312 | `						hasNamed = 1;` |
|  3139941 | 12313 | `					}else if( seenNamed ){` |
|        3 | 12314 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12315 | `							"Cannot use positional argument after named argument");` |
|        3 | 12316 | `						return SXERR_SYNTAX;` |
|  3139797 | 12317 | `					}else if( seenSpread ){` |
|      ! 0 | 12318 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12319 | `							"Cannot use positional argument after argument unpacking");` |
|      ! 0 | 12320 | `						return SXERR_SYNTAX;` |
|        - | 12321 | `					}` |
|  1572076 | 12322 | `				}` |
|        - | 12323 | `			}` |
|        - | 12324 | `			/* Read-only load */` |
|  3188403 | 12325 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|        - | 12326 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|        - | 12327 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|        - | 12328 | `			 * objects dispatch to the right method (offsetExists for both;` |
|        - | 12329 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  3188403 | 12330 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  3188403 | 12331 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  3188398 | 12332 | `				if( pCallName->nByte == 5` |
|  1752742 | 12333 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   155713 | 12334 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  3110549 | 12335 | `				}else if( pCallName->nByte == 5` |
|  1597034 | 12336 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      101 | 12337 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       48 | 12338 | `				}` |
|        - | 12339 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|        - | 12340 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|        - | 12341 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|        - | 12342 | `				 * write back through. Skipped when spread/named args are present:` |
|        - | 12343 | `				 * the compile-time positional index no longer maps to the` |
|        - | 12344 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  3188403 | 12345 | `				if( !bAnySpread && !hasNamed ){` |
|        - | 12346 | `					SyString sBuiltin;` |
|  3184207 | 12347 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  3184207 | 12348 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  1592101 | 12349 | `				}` |
|  1594199 | 12350 | `			}` |
|  6332543 | 12351 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  3144145 | 12352 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  3144145 | 12353 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 12354 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|        - | 12355 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|        - | 12356 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|        - | 12357 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|        - | 12358 | `				 * builtin to write back through. A plain $var target is unaffected` |
|        - | 12359 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  3144145 | 12360 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|       61 | 12361 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|       61 | 12362 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|       28 | 12363 | `				}` |
|  3144145 | 12364 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  3144145 | 12365 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 12366 | `					return rc;` |
|        - | 12367 | `				}` |
|        - | 12368 | `				/* Each argument is an independent nullsafe scope. */` |
|  3144145 | 12369 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  3144145 | 12370 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|        - | 12371 | `					/* Emit spread opcode to unpack this array argument */` |
|     4071 | 12372 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|     4071 | 12373 | `					hasSpread = 1;` |
|     2033 | 12374 | `				}` |
|  1572075 | 12375 | `			}` |
|        - | 12376 | `			/* Total number of given arguments */` |
|  3188403 | 12377 | `			iP1 = nArgs;` |
|  3188403 | 12378 | `			iP2 = hasSpread;` |
|        - | 12379 | `			/* Build VmCallArgMap if named arguments are present.` |
|        - | 12380 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  3188403 | 12381 | `			if( hasNamed ){` |
|      178 | 12382 | `				sxu32 nStrBytes = 0;` |
|        - | 12383 | `				char *zBuf;` |
|      534 | 12384 | `				for( n = 0; n < nArgs; ++n ){` |
|      360 | 12385 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 12386 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      141 | 12387 | `					}` |
|      182 | 12388 | `				}` |
|        - | 12389 | `				{` |
|      178 | 12390 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      178 | 12391 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      174 | 12392 | `					&pGen->pVm->sAllocator, mapSize);` |
|      178 | 12393 | `				if( pMap ){` |
|      178 | 12394 | `					SyZero(pMap, mapSize);` |
|      178 | 12395 | `					pMap->bHasNamed = 1;` |
|      178 | 12396 | `					pMap->nTotal = (sxu32)nArgs;` |
|      178 | 12397 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      178 | 12398 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|      534 | 12399 | `					for( n = 0; n < nArgs; ++n ){` |
|      360 | 12400 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 12401 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      286 | 12402 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      286 | 12403 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      286 | 12404 | `							zBuf += nb;` |
|      141 | 12405 | `						}` |
|        - | 12406 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      182 | 12407 | `					}` |
|      178 | 12408 | `					p3 = (void *)pMap;` |
|       87 | 12409 | `				}` |
|        - | 12410 | `				}` |
|       87 | 12411 | `			}` |
|        - | 12412 | `			/* Remove stale flags now */` |
|  3188403 | 12413 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  1594199 | 12414 | `		}` |
|        - | 12415 | `		{` |
|        - | 12416 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|        - | 12417 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|        - | 12418 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|        - | 12419 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|        - | 12420 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|        - | 12421 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|        - | 12422 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|        - | 12423 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 12376311 | 12424 | `			sxi32 iLeftFlags = iFlags;` |
| 12376306 | 12425 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 10333130 | 12426 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  4145003 | 12427 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  3683069 | 12428 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   939757 | 12429 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   469876 | 12430 | `			}` |
|        - | 12431 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|        - | 12432 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|        - | 12433 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|        - | 12434 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|        - | 12435 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|        - | 12436 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|        - | 12437 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 12376306 | 12438 | `			if( pNode->pOp` |
| 17583267 | 12439 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 11395161 | 12440 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 10413964 | 12441 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  1994085 | 12442 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   997040 | 12443 | `			}` |
| 12376311 | 12444 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|        - | 12445 | `		}` |
| 12376311 | 12446 | `		if( rc != SXRET_OK ){` |
|       34 | 12447 | `			return rc;` |
|        - | 12448 | `		}` |
| 12376281 | 12449 | `		if( !bIsChainOp ){` |
|        - | 12450 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|        - | 12451 | `			 * target the end of that LHS chain, which is right here. */` |
|  5610163 | 12452 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  2805079 | 12453 | `		}` |
| 12376281 | 12454 | `		if( iVmOp == PH7_OP_CALL ){` |
|  3188403 | 12455 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  3188403 | 12456 | `			if( pInstr ){` |
|  3188403 | 12457 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  2349705 | 12458 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|        - | 12459 | `					sxu32 nQual;` |
|  2349705 | 12460 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 12461 | `					/* Prevent constant expansion but preserve the absolute flag` |
|        - | 12462 | `					 * so the later NEW handler (if any) can see it. */` |
|  2349705 | 12463 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|        - | 12464 | `					/* Namespace-qualify the function name for CALL, unless the` |
|        - | 12465 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|        - | 12466 | `					 * imports — class imports must NOT affect function` |
|        - | 12467 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|        - | 12468 | `					 * before NEW; we store the original literal index in the` |
|        - | 12469 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|        - | 12470 | `					 * the unqualified name and re-qualify with class imports. */` |
|  2349705 | 12471 | `					if( bAbsolute ){` |
|     3917 | 12472 | `						pInstr->iP2 = (sxi32)nOrig;` |
|     1961 | 12473 | `					}else{` |
|  2345793 | 12474 | `						int fromImport = 0;` |
|  2345793 | 12475 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  2345793 | 12476 | `						pInstr->iP2 = (sxi32)nQual;` |
|  2345793 | 12477 | `						if( nQual != nOrig ){` |
|        - | 12478 | `							/* Record the original literal index in the arg map` |
|        - | 12479 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|        - | 12480 | `							 * flag) so the NEW handler can recover the` |
|        - | 12481 | `							 * unqualified name and re-qualify with CLASS` |
|        - | 12482 | `							 * imports. */` |
|       77 | 12483 | `							if( p3 == 0 ){` |
|       77 | 12484 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       72 | 12485 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       77 | 12486 | `								if( pMap ){` |
|       77 | 12487 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       77 | 12488 | `									p3 = (void *)pMap;` |
|       36 | 12489 | `								}` |
|       36 | 12490 | `							}` |
|       77 | 12491 | `							if( p3 ){` |
|       77 | 12492 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       77 | 12493 | `								if( !fromImport ){` |
|        - | 12494 | `									/* Mark as namespace-qualified */` |
|       67 | 12495 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       31 | 12496 | `								}` |
|       36 | 12497 | `							}` |
|       36 | 12498 | `						}` |
|        5 | 12499 | `					}` |
|  2013553 | 12500 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|        - | 12501 | `					/* Method call,flag that */` |
|   834325 | 12502 | `					pInstr->iP2 = 1;` |
|   417160 | 12503 | `				}` |
|  1594204 | 12504 | `			}` |
| 10782082 | 12505 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|        - | 12506 | `			ph7_expr_node **apNode;` |
|        - | 12507 | `			sxi32 n;` |
|  1583645 | 12508 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 12509 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|        - | 12510 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 12511 | `			/* Recurse and generate bytecodes for array index */` |
|  1583645 | 12512 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3038989 | 12513 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  1455349 | 12514 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1455349 | 12515 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|  1455349 | 12516 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 12517 | `					return rc;` |
|        - | 12518 | `				}` |
|        - | 12519 | `				/* Each subscript index is an independent nullsafe scope. */` |
|  1455349 | 12520 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   727677 | 12521 | `			}` |
|  1583645 | 12522 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|  1455349 | 12523 | `				iP1 = 1; /* Node have an index associated with it */` |
|   727672 | 12524 | `			}` |
|  1583645 | 12525 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|        - | 12526 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|   194445 | 12527 | `				iP2 = 4;` |
|  1486425 | 12528 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 12529 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|        - | 12530 | `				 * so the trailing unset() builtin can drop the slot. */` |
|       72 | 12531 | `				iP2 = 5;` |
|  1389171 | 12532 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        - | 12533 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|        - | 12534 | `				 * short-circuit on missing keys without invoking offsetGet` |
|        - | 12535 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|       29 | 12536 | `				iP2 = 6;` |
|  1389125 | 12537 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|        - | 12538 | `				/* Create an empty entry when the desired index is not found */` |
|   190893 | 12539 | `				iP2 = 1;` |
|    95449 | 12540 | `			}` |
|  8396063 | 12541 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        - | 12542 | `			/* POP the left node */` |
|       32 | 12543 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       15 | 12544 | `		}` |
|  6188138 | 12545 | `	}` |
| 12387965 | 12546 | `	rc = SXRET_OK;` |
| 12387965 | 12547 | `	nJmpIdx = 0;` |
|        - | 12548 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|        - | 12549 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|        - | 12550 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 12387965 | 12551 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    43375 | 12552 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    43375 | 12553 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    43375 | 12554 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    43375 | 12555 | `			int isSpecial = 0;` |
|    43375 | 12556 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    19979 | 12557 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    19979 | 12558 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    19974 | 12559 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    31605 | 12560 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    15802 | 12561 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    11783 | 12562 | `					isSpecial = 1;` |
|     5889 | 12563 | `				}` |
|    15836 | 12564 | `			}` |
|    55073 | 12565 | `			pInstr->iP1 = 0;` |
|    55073 | 12566 | `			if( !isSpecial ){` |
|    19899 | 12567 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     9947 | 12568 | `			}` |
|        - | 12569 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|        - | 12570 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    31677 | 12571 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    19899 | 12572 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    19899 | 12573 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       48 | 12574 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|       50 | 12575 | `					return SXRET_OK;` |
|        - | 12576 | `				}` |
|     9924 | 12577 | `			}` |
|    15813 | 12578 | `		}` |
|    39188 | 12579 | `	}` |
|        - | 12580 | `	/* Generate code for the right tree */` |
| 12376235 | 12581 | `	if( pNode->pRight ){` |
|  6764543 | 12582 | `		if( iVmOp == PH7_OP_LAND ){` |
|        - | 12583 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   136471 | 12584 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6696310 | 12585 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|        - | 12586 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    93399 | 12587 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6581380 | 12588 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|        - | 12589 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      139 | 12590 | `			iVmOp = 0; /* No binary operator to emit */` |
|      139 | 12591 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  6534668 | 12592 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        - | 12593 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|        - | 12594 | `			 * the entire containing postfix chain to null. The jump target is` |
|        - | 12595 | `			 * patched later by the innermost non-chain ancestor (or by` |
|        - | 12596 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|        - | 12597 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|        - | 12598 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|      108 | 12599 | `			sxu32 nNsJmp = 0;` |
|      108 | 12600 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|      108 | 12601 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  6534497 | 12602 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|        - | 12603 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|        - | 12604 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|        - | 12605 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  2306129 | 12606 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  1153062 | 12607 | `		}` |
|  6764543 | 12608 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  6764543 | 12609 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  6764543 | 12610 | `		if( !bIsChainOp ){` |
|        - | 12611 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|        - | 12612 | `			 * operator instruction is emitted. */` |
|  4770509 | 12613 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  2385252 | 12614 | `		}` |
|  6764543 | 12615 | `		if( iVmOp == PH7_OP_STORE ){` |
|  2018409 | 12616 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  2018374 | 12617 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|        - | 12618 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|        - | 12619 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|        - | 12620 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|        - | 12621 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|        - | 12622 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|        - | 12623 | `				 */` |
|       85 | 12624 | `				iVmOp = 0;` |
|  2018369 | 12625 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  2018329 | 12626 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 12627 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   249135 | 12628 | `					iP2 = 1;` |
|   124570 | 12629 | `				}else{` |
|  1769199 | 12630 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 12631 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   190811 | 12632 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   190811 | 12633 | `						iP1 = pInstr->iP1;` |
|    95408 | 12634 | `					}else{` |
|  1578393 | 12635 | `						p3 = pInstr->p3;` |
|        - | 12636 | `					}` |
|        - | 12637 | `					/* POP the last dynamic load instruction */` |
|  1769199 | 12638 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 12639 | `				}` |
|  1009167 | 12640 | `			}` |
|  5755341 | 12641 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|       64 | 12642 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|       64 | 12643 | `			if( pInstr ){` |
|       64 | 12644 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 12645 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|        - | 12646 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|        - | 12647 | `					 */` |
|       19 | 12648 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|       19 | 12649 | `					iP1 = pInstr->iP1;` |
|       19 | 12650 | `					iP2 = pInstr->iP2;` |
|       19 | 12651 | `					p3  = pInstr->p3;` |
|       10 | 12652 | `				}else{` |
|       46 | 12653 | `					p3 = pInstr->p3;` |
|        - | 12654 | `				}` |
|       30 | 12655 | `			}` |
|       30 | 12656 | `		}` |
|  3382269 | 12657 | `	}` |
| 12376230 | 12658 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   242081 | 12659 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|        - | 12660 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|        - | 12661 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       30 | 12662 | `		iVmOp = 0;` |
|       13 | 12663 | `	}` |
| 12376235 | 12664 | `	if( iVmOp > 0 ){` |
| 12375965 | 12665 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    70349 | 12666 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|        - | 12667 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    11677 | 12668 | `				iP1 = 1;` |
|     5841 | 12669 | `			}` |
| 12340793 | 12670 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|        - | 12671 | `			/* Namespace-qualify the class name for NEW */ {` |
|   483857 | 12672 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   483857 | 12673 | `				VmInstr *pCallInstr = 0;` |
|   483857 | 12674 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   483609 | 12675 | `					pCallInstr = pPeek;` |
|   483609 | 12676 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   241802 | 12677 | `				}` |
|   483857 | 12678 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   483853 | 12679 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 12680 | `					sxu32 nLitForClass;` |
|   483853 | 12681 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|        - | 12682 | `					/* If the CALL handler qualified the name with FUNCTION` |
|        - | 12683 | `					 * imports, recover the original literal (recorded in the` |
|        - | 12684 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|        - | 12685 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|        - | 12686 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|        - | 12687 | `					 * with class imports. */` |
|   483853 | 12688 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       37 | 12689 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|       21 | 12690 | `					}else{` |
|   483821 | 12691 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|        - | 12692 | `					}` |
|   483853 | 12693 | `					pPeek->iP1 = 0;` |
|   483853 | 12694 | `					if( !bAbsolute ){` |
|   479945 | 12695 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   239975 | 12696 | `					}else{` |
|     3913 | 12697 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|        - | 12698 | `					}` |
|   241924 | 12699 | `				}` |
|        - | 12700 | `			}` |
|   483857 | 12701 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   483857 | 12702 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|        - | 12703 | `				VmInstr *pPrev;` |
|   483609 | 12704 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   483609 | 12705 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|        - | 12706 | `					/* Pop the call instruction, preserve named-arg map and` |
|        - | 12707 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|        - | 12708 | `					 * accumulator exactly like OP_CALL would have). */` |
|   483609 | 12709 | `					iP1 = pInstr->iP1;` |
|   483609 | 12710 | `					iP2 = pInstr->iP2;` |
|   483609 | 12711 | `					if( pInstr->p3 ){` |
|       47 | 12712 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|       21 | 12713 | `					}` |
|   483609 | 12714 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   241802 | 12715 | `				}` |
|   241807 | 12716 | `			}` |
| 12063695 | 12717 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|        - | 12718 | `			/* instanceof: right operand is a class name, not a constant.` |
|        - | 12719 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|    31297 | 12720 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    31297 | 12721 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    31297 | 12722 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    31297 | 12723 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|    31297 | 12724 | `				int isSpecialIs = 0;` |
|    31297 | 12725 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    31297 | 12726 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    31297 | 12727 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    31292 | 12728 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    31295 | 12729 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    15646 | 12730 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       12 | 12731 | `						isSpecialIs = 1;` |
|        5 | 12732 | `					}` |
|    15646 | 12733 | `				}` |
|    31297 | 12734 | `				pInstr->iP1 = 0;` |
|    31297 | 12735 | `				if( !isSpecialIs && !bAbsolute ){` |
|    31277 | 12736 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    15636 | 12737 | `				}` |
|    15651 | 12738 | `			}` |
| 11806123 | 12739 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|        - | 12740 | `			/* Prevent constant expansion for member/property names.` |
|        - | 12741 | `			 * The right child (member name) was just compiled — its LOADC` |
|        - | 12742 | `			 * should not trigger constant lookup. */` |
|  1994039 | 12743 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  1994039 | 12744 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  1993977 | 12745 | `				pInstr->iP1 = 0;` |
|   996986 | 12746 | `			}` |
|  1994039 | 12747 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|        - | 12748 | `				/* Static member access,remember that */` |
|    31645 | 12749 | `				iP1 = 1;` |
|    31645 | 12750 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    31645 | 12751 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       52 | 12752 | `					p3 = pInstr->p3;` |
|       52 | 12753 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       24 | 12754 | `				}` |
|    15820 | 12755 | `			}` |
|        - | 12756 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|        - | 12757 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|        - | 12758 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|        - | 12759 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  1994039 | 12760 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  1994039 | 12761 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       36 | 12762 | `					iP2 = PH7_MEMBER_UNSET;` |
|  1994022 | 12763 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       91 | 12764 | `					iP2 = PH7_MEMBER_ISSET;` |
|  1993962 | 12765 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       15 | 12766 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  1993912 | 12767 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|        - | 12768 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   249215 | 12769 | `					iP2 = PH7_MEMBER_WRITE;` |
|   124605 | 12770 | `				}` |
|   997017 | 12771 | `			}` |
|   997017 | 12772 | `		}` |
|        - | 12773 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|        - | 12774 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|        - | 12775 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|        - | 12776 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|        - | 12777 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 12375965 | 12778 | `		if( bFcc ){` |
|       79 | 12779 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|       79 | 12780 | `			iP2 = 0;` |
|       79 | 12781 | `			p3 = 0;` |
|       79 | 12782 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       79 | 12783 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 12784 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|        - | 12785 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|        - | 12786 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|        - | 12787 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|       37 | 12788 | `				void *pMemberName = pInstr->p3;` |
|       37 | 12789 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|       37 | 12790 | `				if( pMemberName ){` |
|        3 | 12791 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|        1 | 12792 | `				}` |
|       37 | 12793 | `				iP1 = 2;` |
|       19 | 12794 | `			}else{` |
|       43 | 12795 | `				iP1 = 1;` |
|        - | 12796 | `			}` |
|       39 | 12797 | `		}` |
|        - | 12798 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|        - | 12799 | `		 * This is the primary emit path for user-visible calls. */` |
| 12375965 | 12800 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  3672177 | 12801 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  1836086 | 12802 | `		}` |
|        - | 12803 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 12375965 | 12804 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  6187980 | 12805 | `	}` |
| 12376235 | 12806 | `	if( nJmpIdx > 0 ){` |
|        - | 12807 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   229999 | 12808 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   229999 | 12809 | `		if( pInstr ){` |
|   229999 | 12810 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   114997 | 12811 | `		}` |
|   114997 | 12812 | `	}` |
| 12376235 | 12813 | `	return rc;` |
| 15758164 | 12814 | `}` |
|        - | 12815 | `/*` |
|        - | 12816 | ` * Compile a PHP expression.` |
|        - | 12817 | ` * According to the PHP language reference manual:` |
|        - | 12818 | ` *  Expressions are the most important building stones of PHP.` |
|        - | 12819 | ` *  In PHP, almost anything you write is an expression.` |
|        - | 12820 | ` *  The simplest yet most accurate way to define an expression` |
|        - | 12821 | ` *  is "anything that has a value".` |
|        - | 12822 | ` * If something goes wrong while compiling the expression,this` |
|        - | 12823 | ` * function takes care of generating the appropriate error` |
|        - | 12824 | ` * message.` |
|        - | 12825 | ` */` |
|  7160720 | 12826 | `static sxi32 PH7_CompileExpr(` |
|        - | 12827 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 12828 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 12829 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|        - | 12830 | `	)` |
|        5 | 12831 | `{` |
|        - | 12832 | `	ph7_expr_node *pRoot;` |
|        - | 12833 | `	SySet sExprNode;` |
|        - | 12834 | `	SyToken *pEnd;` |
|        - | 12835 | `	sxi32 nExpr;` |
|        - | 12836 | `	sxi32 iNest;` |
|        - | 12837 | `	sxi32 rc;` |
|        - | 12838 | `	sxu32 nNullsafeBase;` |
|        - | 12839 | `	/* Initialize worker variables */` |
|  7160725 | 12840 | `	nExpr = 0;` |
|  7160725 | 12841 | `	pRoot = 0;` |
|        - | 12842 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|        - | 12843 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  7160725 | 12844 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  7160725 | 12845 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  7160725 | 12846 | `	SySetAlloc(&sExprNode,0x10);` |
|  7160725 | 12847 | `	rc = SXRET_OK;` |
|        - | 12848 | `	/* Delimit the expression */` |
|  7160725 | 12849 | `	pEnd = pGen->pIn;` |
|  7160725 | 12850 | `	iNest = 0;` |
| 55591573 | 12851 | `	while( pEnd < pGen->pEnd ){` |
| 53049109 | 12852 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 12853 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      607 | 12854 | `			iNest++;` |
| 53048808 | 12855 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      615 | 12856 | `			iNest--;` |
| 53048202 | 12857 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  4618741 | 12858 | `			if( iNest <= 0 ){` |
|  4618261 | 12859 | `				break;` |
|        - | 12860 | `			}` |
|      240 | 12861 | `		}` |
| 48430853 | 12862 | `		pEnd++;` |
|        5 | 12863 | `	}` |
|  7160725 | 12864 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   237727 | 12865 | `		SyToken *pEnd2 = pGen->pIn;` |
|   237727 | 12866 | `		iNest = 0;` |
|        - | 12867 | `		/* Stop at the first comma */` |
|   553619 | 12868 | `		while( pEnd2 < pEnd ){` |
|   315903 | 12869 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     7857 | 12870 | `				iNest++;` |
|   311977 | 12871 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     7857 | 12872 | `				iNest--;` |
|   304125 | 12873 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       65 | 12874 | `				if( iNest <= 0 ){` |
|        7 | 12875 | `					break;` |
|        - | 12876 | `				}` |
|       27 | 12877 | `			}` |
|   315897 | 12878 | `			pEnd2++;` |
|        5 | 12879 | `		}` |
|   237727 | 12880 | `		if( pEnd2 <pEnd ){` |
|        7 | 12881 | `			pEnd = pEnd2;` |
|        3 | 12882 | `		}` |
|   118861 | 12883 | `	}` |
|  7160725 | 12884 | `	if( pEnd > pGen->pIn ){` |
|  7160715 | 12885 | `		SyToken *pTmp = pGen->pEnd;` |
|        - | 12886 | `		/* Swap delimiter */` |
|  7160715 | 12887 | `		pGen->pEnd = pEnd;` |
|        - | 12888 | `		/* Try to get an expression tree */` |
|  7160715 | 12889 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  7160715 | 12890 | `		if( rc == SXRET_OK && pRoot ){` |
|  7160533 | 12891 | `			rc = SXRET_OK;` |
|  7160533 | 12892 | `			if( xTreeValidator ){` |
|        - | 12893 | `				/* Call the upper layer validator callback */` |
|   563699 | 12894 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   281847 | 12895 | `			}` |
|  7160533 | 12896 | `			if( rc != SXERR_ABORT ){` |
|        - | 12897 | `				/* Generate code for the given tree */` |
|  7160533 | 12898 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|        - | 12899 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|        - | 12900 | `				 * expression so they short-circuit to its end. */` |
|  7160533 | 12901 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  3580264 | 12902 | `			}` |
|  7160533 | 12903 | `			nExpr = 1;` |
|  3580264 | 12904 | `		}` |
|        - | 12905 | `		/* Release the whole tree */` |
|  7160715 | 12906 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|        - | 12907 | `		/* Synchronize token stream */` |
|  7160715 | 12908 | `		pGen->pEnd = pTmp;` |
|  7160715 | 12909 | `		pGen->pIn  = pEnd;` |
|  7160715 | 12910 | `		if( rc == SXERR_ABORT ){` |
|       13 | 12911 | `			SySetRelease(&sExprNode);` |
|       13 | 12912 | `			return SXERR_ABORT;` |
|        - | 12913 | `		}` |
|  3580350 | 12914 | `	}` |
|  7160715 | 12915 | `	SySetRelease(&sExprNode);` |
|  7160715 | 12916 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  3580365 | 12917 | `}` |
|        - | 12918 | `/*` |
|        - | 12919 | ` * Return a pointer to the node construct handler associated` |
|        - | 12920 | ` * with a given node type [i.e: string,integer,float,...].` |
|        - | 12921 | ` */` |
|  4293836 | 12922 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|        5 | 12923 | `{` |
|  4293841 | 12924 | `	if( nNodeType & PH7_TK_NUM ){` |
|        - | 12925 | `		/* Numeric literal: Either real or integer */` |
|  1288881 | 12926 | `		return PH7_CompileNumLiteral;` |
|  3004965 | 12927 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|        - | 12928 | `		/* Double quoted string */` |
|    36633 | 12929 | `		return PH7_CompileString;` |
|  2968337 | 12930 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|        - | 12931 | `		/* Single quoted string */` |
|  2968217 | 12932 | `		return PH7_CompileSimpleString;` |
|      125 | 12933 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|        - | 12934 | `		/* Heredoc */` |
|       71 | 12935 | `		return PH7_CompileHereDoc;` |
|       58 | 12936 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|        - | 12937 | `		/* Nowdoc */` |
|       51 | 12938 | `		return PH7_CompileNowDoc;` |
|        9 | 12939 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|        - | 12940 | `		/* Backtick quoted string */` |
|        6 | 12941 | `		return PH7_CompileBacktic;` |
|        - | 12942 | `	}` |
|        3 | 12943 | `	return 0;` |
|  2146923 | 12944 | `}` |
|        - | 12945 | `/*` |
|        - | 12946 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|        - | 12947 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|        - | 12948 | ` * in write context" parse error.` |
|        - | 12949 | ` */` |
|     6852 | 12950 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|        5 | 12951 | `{` |
|        - | 12952 | `	sxi32 rc;` |
|     6857 | 12953 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     6855 | 12954 | `		return SXRET_OK;` |
|        - | 12955 | `	}` |
|        5 | 12956 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|        2 | 12957 | `		pNode ? pNode->pStart->nLine : 1,` |
|        - | 12958 | `		"Can't use nullsafe operator in write context");` |
|        3 | 12959 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     3431 | 12960 | `}` |
|        - | 12961 | `/*` |
|        - | 12962 | ` * Compile an unset() statement.` |
|        - | 12963 | ` * unset($var, $arr[$key], ...);` |
|        - | 12964 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|        - | 12965 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|        - | 12966 | ` * parent array before extracting the element to unset.` |
|        - | 12967 | ` */` |
|     2930 | 12968 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|        5 | 12969 | `{` |
|     2935 | 12970 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     2935 | 12971 | `	sxu32 nIdx = 0;` |
|        - | 12972 | `	SyString sName;` |
|        - | 12973 | `	sxi32 rc;` |
|        - | 12974 | `	/* Jump the 'unset' keyword */` |
|     2935 | 12975 | `	pGen->pIn++;` |
|        - | 12976 | `	/* Save delimiter */` |
|     2935 | 12977 | `	pTmp = pGen->pEnd;` |
|        - | 12978 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     2935 | 12979 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     2935 | 12980 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 12981 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|        - | 12982 | `		SyToken *pClose;` |
|     2935 | 12983 | `		pGen->pIn++;   /* Skip '(' */` |
|     2935 | 12984 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     2935 | 12985 | `		pEnd = pClose; /* Stop at ')' */` |
|     1465 | 12986 | `	}` |
|     2935 | 12987 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|        - | 12988 | `	/* Resolve the 'unset' builtin name once */` |
|     2935 | 12989 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      379 | 12990 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      379 | 12991 | `		if( pObj == 0 ){` |
|      ! 0 | 12992 | `			return SXERR_ABORT;` |
|        - | 12993 | `		}` |
|      379 | 12994 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      379 | 12995 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      187 | 12996 | `	}` |
|        - | 12997 | `	/* Compile each comma-separated argument */` |
|     9789 | 12998 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     6859 | 12999 | `		if( pGen->pIn < pNext ){` |
|     6859 | 13000 | `			pGen->pEnd = pNext;` |
|     6859 | 13001 | `			rc = PH7_CompileExpr(&(*pGen),` |
|        - | 13002 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|        - | 13003 | `				GenStateUnsetValidator);` |
|     6859 | 13004 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13005 | `				return SXERR_ABORT;` |
|        - | 13006 | `			}` |
|     6859 | 13007 | `			if( rc != SXERR_EMPTY ){` |
|        - | 13008 | `				/* Emit call for this single argument */` |
|     6857 | 13009 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     6857 | 13010 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     6857 | 13011 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     3426 | 13012 | `			}` |
|     3427 | 13013 | `		}` |
|        - | 13014 | `		/* Jump trailing commas */` |
|    10785 | 13015 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|     3931 | 13016 | `			pNext++;` |
|        5 | 13017 | `		}` |
|     6859 | 13018 | `		pGen->pIn = pNext;` |
|        5 | 13019 | `	}` |
|        - | 13020 | `	/* Skip past the closing ')' if present */` |
|     2935 | 13021 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     2935 | 13022 | `		pGen->pIn++;` |
|     1465 | 13023 | `	}` |
|        - | 13024 | `	/* Restore token stream */` |
|     2935 | 13025 | `	pGen->pEnd = pTmp;` |
|     2935 | 13026 | `	return SXRET_OK;` |
|     1470 | 13027 | `}` |
|        - | 13028 | `/*` |
|        - | 13029 | ` * PHP Language construct table.` |
|        - | 13030 | ` */` |
|        - | 13031 | `static const LangConstruct aLangConstruct[] = {` |
|        - | 13032 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|        - | 13033 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|        - | 13034 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|        - | 13035 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|        - | 13036 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|        - | 13037 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|        - | 13038 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|        - | 13039 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|        - | 13040 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|        - | 13041 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|        - | 13042 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|        - | 13043 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|        - | 13044 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|        - | 13045 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|        - | 13046 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|        - | 13047 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|        - | 13048 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|        - | 13049 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|        - | 13050 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|        - | 13051 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|        - | 13052 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|        - | 13053 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|        - | 13054 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|        - | 13055 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|        - | 13056 | `};` |
|        - | 13057 | `/*` |
|        - | 13058 | ` * Return a pointer to the statement handler routine associated` |
|        - | 13059 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|        - | 13060 | ` */` |
|  3798832 | 13061 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|        - | 13062 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|        - | 13063 | `	SyToken *pLookahed  /* Look-ahead token */` |
|        - | 13064 | `	)` |
|        5 | 13065 | `{` |
|  3798837 | 13066 | `	sxu32 n = 0;` |
| 15473276 | 13067 | `	for(;;){` |
| 30946557 | 13068 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   246789 | 13069 | `			break;` |
|        - | 13070 | `		}` |
| 30699773 | 13071 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  3552053 | 13072 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|      ! 0 | 13073 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|      ! 0 | 13074 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|        - | 13075 | `					/* 'static' (class context),return null */` |
|      ! 0 | 13076 | `					return 0;` |
|        - | 13077 | `				}` |
|      ! 0 | 13078 | `			}` |
|  3552048 | 13079 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       10 | 13080 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       10 | 13081 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|        - | 13082 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|      ! 0 | 13083 | `				return 0;` |
|        - | 13084 | `			}` |
|        - | 13085 | `			/* Return a pointer to the handler.` |
|        - | 13086 | `			*/` |
|  3552053 | 13087 | `			return aLangConstruct[n].xConstruct;` |
|        - | 13088 | `		}` |
| 27147725 | 13089 | `		n++;` |
|        5 | 13090 | `	}` |
|   246789 | 13091 | `	if( pLookahed ){` |
|   246789 | 13092 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    46713 | 13093 | `			return PH7_CompileClassInterface;` |
|   200081 | 13094 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   187885 | 13095 | `			return PH7_CompileClass;` |
|    12201 | 13096 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|       75 | 13097 | `			return PH7_CompileTrait;` |
|        - | 13098 | `		}` |
|        - | 13099 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|        - | 13100 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|        - | 13101 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|        - | 13102 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     6063 | 13103 | `	}` |
|        - | 13104 | `	/* Not a language construct */` |
|    12131 | 13105 | `	return 0;` |
|  1899421 | 13106 | `}` |
|        - | 13107 | `/*` |
|        - | 13108 | ` * Check if the given keyword is in fact a PHP language construct.` |
|        - | 13109 | ` * Return TRUE on success. FALSE otheriwse.` |
|        - | 13110 | ` */` |
|    12126 | 13111 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|        5 | 13112 | `{` |
|        - | 13113 | `	int rc;` |
|    12131 | 13114 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    12131 | 13115 | `	if( rc == FALSE ){` |
|    12012 | 13116 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      359 | 13117 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|        - | 13118 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|        - | 13119 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|        - | 13120 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|        - | 13121 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|        - | 13122 | `			*/` |
|        - | 13123 | `			){` |
|    12009 | 13124 | `				rc = TRUE;` |
|     6002 | 13125 | `		}` |
|     6006 | 13126 | `	}` |
|    12131 | 13127 | `	return rc;` |
|        5 | 13128 | `}` |
|        - | 13129 | `/*` |
|        - | 13130 | ` * Compile a PHP chunk.` |
|        - | 13131 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13132 | ` * takes care of generating the appropriate error message.` |
|        - | 13133 | ` */` |
|        - | 13134 | `/*` |
|        - | 13135 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|        - | 13136 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|        - | 13137 | ` * the chunk token set it becomes the pending docblock. An existing` |
|        - | 13138 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|        - | 13139 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|        - | 13140 | ` * intervening non-declaration statements.` |
|        - | 13141 | ` */` |
|  7949896 | 13142 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|        5 | 13143 | `{` |
|  7949901 | 13144 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  7949901 | 13145 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  7949901 | 13146 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13147 | `	sxu32 nIdx, n;` |
|  7949896 | 13148 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|  1536871 | 13149 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|        - | 13150 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|        - | 13151 | `		 * indexes do not map to the sidecar */` |
|  6413035 | 13152 | `		return;` |
|        - | 13153 | `	}` |
|  1536871 | 13154 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|        - | 13155 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|        - | 13156 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|  1536871 | 13157 | `	SySetReset(&pGen->aPendingAttrs);` |
|  4610869 | 13158 | `	for( n = 0 ; n < nT ; n++ ){` |
|  3074003 | 13159 | `		if( aT[n].nTokIdx != nIdx ){` |
|  3066105 | 13160 | `			continue;` |
|        - | 13161 | `		}` |
|     7903 | 13162 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       29 | 13163 | `			pGen->sPendingDoc = aT[n].sText;` |
|     7891 | 13164 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     7879 | 13165 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|     3937 | 13166 | `		}` |
|     3954 | 13167 | `	}` |
|  3974953 | 13168 | `}` |
|        - | 13169 | `/*` |
|        - | 13170 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|        - | 13171 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|        - | 13172 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|        - | 13173 | ` */` |
|  2122780 | 13174 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|        5 | 13175 | `{` |
|        - | 13176 | `	char *zDup;` |
|  2122785 | 13177 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|  2122765 | 13178 | `		return;` |
|        - | 13179 | `	}` |
|       35 | 13180 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       10 | 13181 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       25 | 13182 | `	if( zDup ){` |
|       25 | 13183 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       10 | 13184 | `	}` |
|       25 | 13185 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|  1061395 | 13186 | `}` |
|        - | 13187 | `/*` |
|        - | 13188 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|        - | 13189 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|        - | 13190 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|        - | 13191 | ` * names may point into the token text, which must outlive the raw script` |
|        - | 13192 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|        - | 13193 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|        - | 13194 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|        - | 13195 | ` */` |
|     7878 | 13196 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|        5 | 13197 | `{` |
|        - | 13198 | `	SySet *pToken;` |
|        - | 13199 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|        - | 13200 | `	char *zSpan;` |
|     7883 | 13201 | `	sxi32 rc = SXRET_OK;` |
|     7883 | 13202 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|      ! 0 | 13203 | `		return SXRET_OK;` |
|        - | 13204 | `	}` |
|    11822 | 13205 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3939 | 13206 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|     7883 | 13207 | `	if( zSpan == 0 ){` |
|      ! 0 | 13208 | `		return SXRET_OK;` |
|        - | 13209 | `	}` |
|        - | 13210 | `	/* The token set must outlive compilation too: interned operands may` |
|        - | 13211 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|        - | 13212 | `	 * the number of attribute declarations in the program. */` |
|     7883 | 13213 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|     7883 | 13214 | `	if( pToken == 0 ){` |
|      ! 0 | 13215 | `		return SXRET_OK;` |
|        - | 13216 | `	}` |
|     7883 | 13217 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     7883 | 13218 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|     7883 | 13219 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|     7883 | 13220 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|     7883 | 13221 | `	pSavedIn = pGen->pIn;` |
|     7883 | 13222 | `	pSavedEnd = pGen->pEnd;` |
|     7887 | 13223 | `	while( pIn < pEnd ){` |
|        - | 13224 | `		ph7_attribute sAttr;` |
|        - | 13225 | `		SyBlob sFQN;` |
|     7887 | 13226 | `		int bAbsolute = 0;` |
|     7887 | 13227 | `		SyZero(&sAttr,sizeof(sAttr));` |
|     7887 | 13228 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|     7887 | 13229 | `		sAttr.nLine = pIn->nLine;` |
|     7887 | 13230 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       49 | 13231 | `			bAbsolute = 1;` |
|       49 | 13232 | `			pIn++;` |
|       22 | 13233 | `		}` |
|     7887 | 13234 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7887 | 13235 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     7887 | 13236 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|     7887 | 13237 | `			pIn++;` |
|     7887 | 13238 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      ! 0 | 13239 | `				SyBlobAppend(&sFQN,"\\",1);` |
|      ! 0 | 13240 | `				pIn++;` |
|      ! 0 | 13241 | `				continue;` |
|        - | 13242 | `			}` |
|     7887 | 13243 | `			break;` |
|      ! 0 | 13244 | `		}` |
|     7887 | 13245 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|        - | 13246 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|        - | 13247 | `			 * this feature; never turn it into a new fatal) */` |
|      ! 0 | 13248 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 13249 | `			break;` |
|        - | 13250 | `		}` |
|        - | 13251 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|        - | 13252 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|        - | 13253 | `		{` |
|     7887 | 13254 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|     7887 | 13255 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|     7887 | 13256 | `			char *zDup = 0;` |
|     7887 | 13257 | `			if( !bAbsolute ){` |
|     7843 | 13258 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|     7843 | 13259 | `				if( pImp ){` |
|      ! 0 | 13260 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|      ! 0 | 13261 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|      ! 0 | 13262 | `					if( zDup ){` |
|      ! 0 | 13263 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|      ! 0 | 13264 | `					}` |
|     7843 | 13265 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 13266 | `					SyBlob sTmp;` |
|      ! 0 | 13267 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|      ! 0 | 13268 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      ! 0 | 13269 | `					SyBlobAppend(&sTmp,"\\",1);` |
|      ! 0 | 13270 | `					SyBlobAppend(&sTmp,zName,nName);` |
|      ! 0 | 13271 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      ! 0 | 13272 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|      ! 0 | 13273 | `					if( zDup ){` |
|      ! 0 | 13274 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|      ! 0 | 13275 | `					}` |
|      ! 0 | 13276 | `					SyBlobRelease(&sTmp);` |
|      ! 0 | 13277 | `				}` |
|     3919 | 13278 | `			}` |
|     7887 | 13279 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|     7887 | 13280 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|     7887 | 13281 | `				if( zDup ){` |
|     7887 | 13282 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|     3941 | 13283 | `				}` |
|     3941 | 13284 | `			}` |
|        - | 13285 | `		}` |
|     7887 | 13286 | `		SyBlobRelease(&sFQN);` |
|     7887 | 13287 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 13288 | `			SyToken *pArgsEnd;` |
|     7809 | 13289 | `			pIn++;` |
|     7809 | 13290 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|    15621 | 13291 | `			while( pIn < pArgsEnd ){` |
|     7817 | 13292 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|     7817 | 13293 | `				sxi32 iDepth = 0;` |
|        - | 13294 | `				ph7_attr_arg sArgRec;` |
|    77849 | 13295 | `				while( pArgStop < pArgsEnd ){` |
|    70047 | 13296 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       11 | 13297 | `						iDepth++;` |
|    70042 | 13298 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       11 | 13299 | `						iDepth--;` |
|    70032 | 13300 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|       11 | 13301 | `						break;` |
|        - | 13302 | `					}` |
|    70037 | 13303 | `					pArgStop++;` |
|        5 | 13304 | `				}` |
|     7817 | 13305 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|     7817 | 13306 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     7812 | 13307 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|     7801 | 13308 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        7 | 13309 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        2 | 13310 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        5 | 13311 | `					if( zN ){` |
|        5 | 13312 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|        2 | 13313 | `					}` |
|        5 | 13314 | `					pArgStart += 2;` |
|        2 | 13315 | `				}` |
|     7817 | 13316 | `				if( pArgStart < pArgStop ){` |
|        - | 13317 | `					SySet *pInstrContainer;` |
|     7817 | 13318 | `					pGen->pIn = pArgStart;` |
|     7817 | 13319 | `					pGen->pEnd = pArgStop;` |
|     7817 | 13320 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7817 | 13321 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|     7817 | 13322 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7817 | 13323 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7817 | 13324 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7817 | 13325 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 13326 | `						pGen->pIn = pSavedIn;` |
|      ! 0 | 13327 | `						pGen->pEnd = pSavedEnd;` |
|      ! 0 | 13328 | `						return SXERR_ABORT;` |
|        - | 13329 | `					}` |
|     7817 | 13330 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|     3906 | 13331 | `				}` |
|     7817 | 13332 | `				pIn = pArgStop;` |
|     7817 | 13333 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       11 | 13334 | `					pIn++;` |
|        5 | 13335 | `				}` |
|        5 | 13336 | `			}` |
|     7809 | 13337 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|     3902 | 13338 | `		}` |
|     7887 | 13339 | `		SySetPut(pOut,(const void *)&sAttr);` |
|     7887 | 13340 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        5 | 13341 | `			pIn++;` |
|        5 | 13342 | `			continue;` |
|        - | 13343 | `		}` |
|     7883 | 13344 | `		break;` |
|      ! 0 | 13345 | `	}` |
|     7883 | 13346 | `	pGen->pIn = pSavedIn;` |
|     7883 | 13347 | `	pGen->pEnd = pSavedEnd;` |
|     7883 | 13348 | `	return SXRET_OK;` |
|     3944 | 13349 | `}` |
|        - | 13350 | `/*` |
|        - | 13351 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|        - | 13352 | ` * every recorded group into pOut and clear the pending list.` |
|        - | 13353 | ` */` |
|  2122780 | 13354 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|        5 | 13355 | `{` |
|  2122785 | 13356 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        - | 13357 | `	sxu32 n;` |
|        - | 13358 | `	sxi32 rc;` |
|  2130659 | 13359 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|     7879 | 13360 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|     7879 | 13361 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13362 | `			return SXERR_ABORT;` |
|        - | 13363 | `		}` |
|     3942 | 13364 | `	}` |
|  2122785 | 13365 | `	SySetReset(&pGen->aPendingAttrs);` |
|  2122785 | 13366 | `	return SXRET_OK;` |
|  1061395 | 13367 | `}` |
|        - | 13368 | `/*` |
|        - | 13369 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|        - | 13370 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|        - | 13371 | ` * the main token stream, so the sidecar indexes map directly.` |
|        - | 13372 | ` */` |
|   717372 | 13373 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|        5 | 13374 | `{` |
|   717377 | 13375 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   717377 | 13376 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   717377 | 13377 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13378 | `	sxu32 nIdx, n;` |
|        - | 13379 | `	sxi32 rc;` |
|   717372 | 13380 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   194501 | 13381 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   522881 | 13382 | `		return SXRET_OK;` |
|        - | 13383 | `	}` |
|   194501 | 13384 | `	nIdx = (sxu32)(pTok - pBase);` |
|   583339 | 13385 | `	for( n = 0 ; n < nT ; n++ ){` |
|   388843 | 13386 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        5 | 13387 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        5 | 13388 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13389 | `				return SXERR_ABORT;` |
|        - | 13390 | `			}` |
|        2 | 13391 | `		}` |
|   194424 | 13392 | `	}` |
|   194501 | 13393 | `	return SXRET_OK;` |
|   358691 | 13394 | `}` |
|  5845228 | 13395 | `static sxi32 GenStateCompileChunk(` |
|        - | 13396 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 13397 | `	sxi32 iFlags         /* Compile flags */` |
|        - | 13398 | `	)` |
|        5 | 13399 | `{` |
|        - | 13400 | `	ProcLangConstruct xCons;` |
|        - | 13401 | `	sxi32 rc;` |
|  5845233 | 13402 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  3338092 | 13403 | `	for(;;){` |
|  6260711 | 13404 | `		int bStmtIsDeclare = 0;` |
|  6260711 | 13405 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 13406 | `			/* No more input to process */` |
|    53341 | 13407 | `			break;` |
|        - | 13408 | `		}` |
|        - | 13409 | `		/* Bind a directly-preceding docblock to this statement */` |
|  6207375 | 13410 | `		GenStateSetPendingDoc(&(*pGen));` |
|        - | 13411 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|        - | 13412 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  6207375 | 13413 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3826067 | 13414 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  3826067 | 13415 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       47 | 13416 | `				bStmtIsDeclare = 1;` |
|       21 | 13417 | `			}` |
|  1913031 | 13418 | `		}` |
|  6207375 | 13419 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|        - | 13420 | `			/* Any non-declare top-level statement locks the strict_types` |
|        - | 13421 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   415451 | 13422 | `			pGen->bStrictTypesLocked = 1;` |
|   207723 | 13423 | `		}` |
|  6207375 | 13424 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 13425 | `			/* Compile block */` |
|     3907 | 13426 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|     3907 | 13427 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13428 | `				break;` |
|        - | 13429 | `			}` |
|     1956 | 13430 | `		}else{` |
|  6203473 | 13431 | `			xCons = 0;` |
|  6203473 | 13432 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|        - | 13433 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|        - | 13434 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|        - | 13435 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    27261 | 13436 | `				xCons = PH7_CompileClassModifiers;` |
|  6189845 | 13437 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        - | 13438 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|        - | 13439 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|       31 | 13440 | `				xCons = PH7_CompileEnum;` |
|  6176204 | 13441 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3798837 | 13442 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - | 13443 | `				/* Try to extract a language construct handler */` |
|  3798837 | 13444 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  3798837 | 13445 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|       13 | 13446 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 13447 | `						"Syntax error: Unexpected keyword '%z'",` |
|        8 | 13448 | `						&pGen->pIn->sData);` |
|        9 | 13449 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 13450 | `						break;` |
|        - | 13451 | `					}` |
|        - | 13452 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|        - | 13453 | `					 * this erroneous statement.` |
|        - | 13454 | `					 */` |
|        9 | 13455 | `					xCons = PH7_ErrorRecover;` |
|        4 | 13456 | `				}` |
|  4276775 | 13457 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    66487 | 13458 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|        - | 13459 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|      117 | 13460 | `				xCons = PH7_CompileLabel;` |
|       56 | 13461 | `			}` |
|  6203473 | 13462 | `			if( xCons == 0 ){` |
|        - | 13463 | `				/* Assume an expression an try to compile it */` |
|  2389365 | 13464 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  2389365 | 13465 | `				if(  rc != SXERR_EMPTY ){` |
|        - | 13466 | `					/* Pop l-value */` |
|  2389215 | 13467 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  1194605 | 13468 | `				}` |
|  1194685 | 13469 | `			}else{` |
|        - | 13470 | `				/* Go compile the sucker */` |
|  3814113 | 13471 | `				rc = xCons(&(*pGen));` |
|        - | 13472 | `			}` |
|  6203473 | 13473 | `			if( rc == SXERR_ABORT ){` |
|        - | 13474 | `				/* Request to abort compilation */` |
|       13 | 13475 | `				break;` |
|        - | 13476 | `			}` |
|        - | 13477 | `		}` |
|        - | 13478 | `		/* Ignore trailing semi-colons ';' */` |
| 10612011 | 13479 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  4404651 | 13480 | `			pGen->pIn++;` |
|        5 | 13481 | `		}` |
|  6207365 | 13482 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|        - | 13483 | `			/* Compile a single statement and return */` |
|  5791887 | 13484 | `			break;` |
|        - | 13485 | `		}` |
|        - | 13486 | `		/* LOOP ONE */` |
|        - | 13487 | `		/* LOOP TWO */` |
|        - | 13488 | `		/* LOOP THREE */` |
|        - | 13489 | `		/* LOOP FOUR */` |
|        5 | 13490 | `	}` |
|        - | 13491 | `	/* Return compilation status */` |
|  5845233 | 13492 | `	return rc;` |
|        5 | 13493 | `}` |
|        - | 13494 | `/*` |
|        - | 13495 | ` * Compile a Raw PHP chunk.` |
|        - | 13496 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13497 | ` * takes care of generating the appropriate error message.` |
|        - | 13498 | ` */` |
|    53348 | 13499 | `static sxi32 PH7_CompilePHP(` |
|        - | 13500 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 13501 | `	SySet *pTokenSet,     /* Token set */` |
|        - | 13502 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|        - | 13503 | `	)` |
|        5 | 13504 | `{` |
|    53353 | 13505 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|        - | 13506 | `	sxi32 rc;` |
|        - | 13507 | `	/* Reset the token set (and its trivia sidecar) */` |
|    53353 | 13508 | `	SySetReset(&(*pTokenSet));` |
|    53353 | 13509 | `	SySetReset(&pGen->aTrivia);` |
|        - | 13510 | `	/* Mark as the default token set */` |
|    53353 | 13511 | `	pGen->pTokenSet = &(*pTokenSet);` |
|        - | 13512 | `	/* Advance the stream cursor */` |
|    53353 | 13513 | `	pGen->pRawIn++;` |
|        - | 13514 | `	/* Tokenize the PHP chunk first */` |
|    53353 | 13515 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|        - | 13516 | `	/* Point to the head and tail of the token stream. */` |
|    53353 | 13517 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    53353 | 13518 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    53353 | 13519 | `	if( is_expr ){` |
|      ! 0 | 13520 | `		rc = SXERR_EMPTY;` |
|      ! 0 | 13521 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 13522 | `			/* A simple expression,compile it */` |
|      ! 0 | 13523 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 | 13524 | `		}` |
|        - | 13525 | `		/* Emit the DONE instruction */` |
|      ! 0 | 13526 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      ! 0 | 13527 | `		return SXRET_OK;` |
|        - | 13528 | `	}` |
|    53353 | 13529 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - | 13530 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - | 13531 | `		/*` |
|        - | 13532 | `		 * Shortcut syntax for the 'echo' language construct.` |
|        - | 13533 | `		 * According to the PHP reference manual:` |
|        - | 13534 | `		 *  echo() also has a shortcut syntax, where you can` |
|        - | 13535 | `		 *  immediately follow` |
|        - | 13536 | `		 *  the opening tag with an equals sign as follows:` |
|        - | 13537 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|        - | 13538 | `		 * Symisc extension:` |
|        - | 13539 | `		 *   This short syntax works with all PHP opening` |
|        - | 13540 | `		 *   tags unlike the default PHP engine that handle` |
|        - | 13541 | `		 *   only short tag.` |
|        - | 13542 | `		 */` |
|        - | 13543 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|        3 | 13544 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        3 | 13545 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        3 | 13546 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        3 | 13547 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|        3 | 13548 | `		if( rc != SXERR_EMPTY ){` |
|        3 | 13549 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 13550 | `		}` |
|        3 | 13551 | `		return SXRET_OK;` |
|        - | 13552 | `	}` |
|        - | 13553 | `	/* Compile the PHP chunk */` |
|    53351 | 13554 | `	rc = GenStateCompileChunk(pGen,0);` |
|        - | 13555 | `	/* Fix exceptions jumps */` |
|    53351 | 13556 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - | 13557 | `	/* Fix gotos now, the jump destination is resolved */` |
|    53351 | 13558 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|        3 | 13559 | `		rc = SXERR_ABORT;` |
|        1 | 13560 | `	}` |
|        - | 13561 | `	/* Reset container */` |
|    53351 | 13562 | `	SySetReset(&pGen->aGoto);` |
|    53351 | 13563 | `	SySetReset(&pGen->aLabel);` |
|    53351 | 13564 | `	SySetReset(&pGen->aNullsafeJmp);` |
|        - | 13565 | `	/* Compilation result */` |
|    53351 | 13566 | `	return rc;` |
|    26679 | 13567 | `}` |
|        - | 13568 | `/*` |
|        - | 13569 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|        - | 13570 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|        - | 13571 | ` * This is the only compile interface exported from this file.` |
|        - | 13572 | ` */` |
|    56408 | 13573 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|        - | 13574 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|        - | 13575 | `	SyString *pScript,  /* Script to compile */` |
|        - | 13576 | `	sxi32 iFlags        /* Compile flags */` |
|        - | 13577 | `	)` |
|        5 | 13578 | `{` |
|        - | 13579 | `	SySet aPhpToken,aRawToken;` |
|        - | 13580 | `	ph7_gen_state *pCodeGen;` |
|        - | 13581 | `	ph7_value *pRawObj;` |
|        - | 13582 | `	sxu32 nObjIdx;` |
|        - | 13583 | `	sxi32 nRawObj;` |
|        - | 13584 | `	int is_expr;` |
|        - | 13585 | `	sxi8 bSavedStrict;` |
|        - | 13586 | `	sxi8 bSavedStrictLocked;` |
|        - | 13587 | `	sxi32 rc;` |
|    56413 | 13588 | `	if( pScript->nByte < 1 ){` |
|        - | 13589 | `		/* Nothing to compile */` |
|      ! 0 | 13590 | `		return PH7_OK;` |
|        - | 13591 | `	}` |
|        - | 13592 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|        - | 13593 | `	 * file's flags so include/require restore them on return. */` |
|    56413 | 13594 | `	pCodeGen = &pVm->sCodeGen;` |
|    56413 | 13595 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    56413 | 13596 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    56413 | 13597 | `	pCodeGen->bStrictTypes = 0;` |
|    56413 | 13598 | `	pCodeGen->bStrictTypesLocked = 0;` |
|        - | 13599 | `	/* Initialize the tokens containers */` |
|    56413 | 13600 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    56413 | 13601 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    56413 | 13602 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    56413 | 13603 | `	is_expr = 0;` |
|    56413 | 13604 | `	if( iFlags & PH7_PHP_ONLY ){` |
|        - | 13605 | `		SyToken sTmp;` |
|        - | 13606 | `		/* PHP only: -*/` |
|    42827 | 13607 | `		sTmp.nLine = 1;` |
|    42827 | 13608 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    42827 | 13609 | `		sTmp.pUserData = 0;` |
|    42827 | 13610 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    42827 | 13611 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    42827 | 13612 | `		if( iFlags & PH7_PHP_EXPR ){` |
|        - | 13613 | `			/* A simple PHP expression */` |
|      ! 0 | 13614 | `			is_expr = 1;` |
|      ! 0 | 13615 | `		}` |
|    21416 | 13616 | `	}else{` |
|        - | 13617 | `		/* Tokenize raw text */` |
|    13591 | 13618 | `		SySetAlloc(&aRawToken,32);` |
|    13591 | 13619 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|        - | 13620 | `	}` |
|        - | 13621 | `	/* Process high-level tokens */` |
|    56413 | 13622 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    56413 | 13623 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    56413 | 13624 | `	rc = PH7_OK;` |
|    56413 | 13625 | `	if( is_expr ){` |
|        - | 13626 | `		/* Compile the expression */` |
|      ! 0 | 13627 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|      ! 0 | 13628 | `		goto cleanup;` |
|        - | 13629 | `	}` |
|    56413 | 13630 | `	nObjIdx = 0;` |
|        - | 13631 | `	/* Each compilation unit starts in the global namespace.` |
|        - | 13632 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|        - | 13633 | `	 * preventing namespace bleeding across include()d files. */` |
|    56413 | 13634 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        - | 13635 | `	/* Start the compilation process */` |
|    35003 | 13636 | `	for(;;){` |
|   123347 | 13637 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    56401 | 13638 | `			break; /* No more tokens to process */` |
|        - | 13639 | `		}` |
|    66951 | 13640 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|        - | 13641 | `			/* Compile the PHP chunk */` |
|    53353 | 13642 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    53353 | 13643 | `			if( rc == SXERR_ABORT ){` |
|       15 | 13644 | `				break;` |
|        - | 13645 | `			}` |
|    53341 | 13646 | `			continue;` |
|        - | 13647 | `		}` |
|        - | 13648 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    13603 | 13649 | `		nRawObj = 0;` |
|    27243 | 13650 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|        - | 13651 | `			/* Consume the raw chunk without any processing */` |
|    13645 | 13652 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    13645 | 13653 | `			if( pRawObj == 0 ){` |
|      ! 0 | 13654 | `				rc = SXERR_MEM;` |
|      ! 0 | 13655 | `				break;` |
|        - | 13656 | `			}` |
|        - | 13657 | `			/* Mark as constant and emit the load constant instruction */` |
|    13645 | 13658 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    13645 | 13659 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    13645 | 13660 | `			++nRawObj;` |
|    13645 | 13661 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|        5 | 13662 | `		}` |
|    13603 | 13663 | `		if( nRawObj > 0 ){` |
|        - | 13664 | `			/* Emit the consume instruction */` |
|    13603 | 13665 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     6799 | 13666 | `		}` |
|    28209 | 13667 | `	}` |
|    28204 | 13668 | `cleanup:` |
|    56413 | 13669 | `	SySetRelease(&aRawToken);` |
|    56413 | 13670 | `	SySetRelease(&aPhpToken);` |
|        - | 13671 | `	/* Restore outer file's strict_types scope */` |
|    56413 | 13672 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    56413 | 13673 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    56413 | 13674 | `	return rc;` |
|    28209 | 13675 | `}` |
|        - | 13676 | `/*` |
|        - | 13677 | ` * Utility routines.Initialize the code generator.` |
|        - | 13678 | ` */` |
|     3884 | 13679 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|        - | 13680 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 13681 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 13682 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 13683 | `	)` |
|        5 | 13684 | `{` |
|     3889 | 13685 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 13686 | `	/* Zero the structure */` |
|     3889 | 13687 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|        - | 13688 | `	/* Initial state */` |
|     3889 | 13689 | `	pGen->pVm  = &(*pVm);` |
|     3889 | 13690 | `	pGen->xErr = xErr;` |
|     3889 | 13691 | `	pGen->pErrData = pErrData;` |
|     3889 | 13692 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|     3889 | 13693 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|     3889 | 13694 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|     3889 | 13695 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3889 | 13696 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3889 | 13697 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|     3889 | 13698 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|        - | 13699 | `	/* Error log buffer */` |
|     3889 | 13700 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        - | 13701 | `	/* General purpose working buffer */` |
|     3889 | 13702 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        - | 13703 | `	/* Namespace state */` |
|     3889 | 13704 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     3889 | 13705 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     3889 | 13706 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     3889 | 13707 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 13708 | `	/* Create the global scope */` |
|     3889 | 13709 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|        - | 13710 | `	/* Point to the global scope */` |
|     3889 | 13711 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     3889 | 13712 | `	return SXRET_OK;` |
|        5 | 13713 | `}` |
|        - | 13714 | `/*` |
|        - | 13715 | ` * Utility routines. Reset the code generator to it's initial state.` |
|        - | 13716 | ` */` |
|    59912 | 13717 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|        - | 13718 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 13719 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 13720 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 13721 | `	)` |
|        5 | 13722 | `{` |
|    59917 | 13723 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 13724 | `	GenBlock *pBlock,*pParent;` |
|        - | 13725 | `	/* Reset state */` |
|    59917 | 13726 | `	SySetReset(&pGen->aLabel);` |
|    59917 | 13727 | `	SySetReset(&pGen->aGoto);` |
|    59917 | 13728 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    59917 | 13729 | `	SySetReset(&pGen->aTrivia);` |
|    59917 | 13730 | `	SySetReset(&pGen->aPendingAttrs);` |
|    59917 | 13731 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    59917 | 13732 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    59917 | 13733 | `	SyBlobRelease(&pGen->sWorker);` |
|    59917 | 13734 | `	SyBlobRelease(&pGen->sNamespace);` |
|    59917 | 13735 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    59917 | 13736 | `	SyHashRelease(&pGen->hUseImports);` |
|    59917 | 13737 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    59917 | 13738 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    59917 | 13739 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    59917 | 13740 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    59917 | 13741 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 13742 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|        - | 13743 | `	 * They intern variable names and literal strings that are referenced by` |
|        - | 13744 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|        - | 13745 | `	 * Releasing them would either leak the interned strings or require freeing` |
|        - | 13746 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|        - | 13747 | `	 * number of unique names, which is acceptable. */` |
|        - | 13748 | `	/* Point to the global scope */` |
|    59917 | 13749 | `	pBlock = pGen->pCurrent;` |
|    59917 | 13750 | `	while( pBlock->pParent != 0 ){` |
|      ! 0 | 13751 | `		pParent = pBlock->pParent;` |
|      ! 0 | 13752 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 13753 | `		pBlock = pParent;` |
|      ! 0 | 13754 | `	}` |
|    59917 | 13755 | `	pGen->xErr = xErr;` |
|    59917 | 13756 | `	pGen->pErrData = pErrData;` |
|    59917 | 13757 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    59917 | 13758 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    59917 | 13759 | `	pGen->pIn = pGen->pEnd = 0;` |
|    59917 | 13760 | `	pGen->nErr = 0;` |
|    59917 | 13761 | `	return SXRET_OK;` |
|        5 | 13762 | `}` |
|        - | 13763 | `/*` |
|        - | 13764 | ` * Generate a compile-time error message.` |
|        - | 13765 | ` * If the error count limit is reached (usually 15 error message)` |
|        - | 13766 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|        - | 13767 | ` * abort compilation immediately.` |
|        - | 13768 | ` */` |
|      652 | 13769 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|        5 | 13770 | `{` |
|      657 | 13771 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|      657 | 13772 | `	const char *zErr = "Error";` |
|        - | 13773 | `	SyString *pFile;` |
|        - | 13774 | `	va_list ap;` |
|        - | 13775 | `	sxi32 rc;` |
|        - | 13776 | `	/* Reset the working buffer */` |
|      657 | 13777 | `	SyBlobReset(pWorker);` |
|        - | 13778 | `	/* Peek the processed file path if available */` |
|      657 | 13779 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      657 | 13780 | `	if( nErrType == E_ERROR ){` |
|        - | 13781 | `		/* Increment the error counter */` |
|      543 | 13782 | `		pGen->nErr++;` |
|      543 | 13783 | `		if( pGen->nErr > 15 ){` |
|        - | 13784 | `			/* Error count limit reached */` |
|        6 | 13785 | `			if( pGen->xErr ){` |
|        6 | 13786 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|        6 | 13787 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|        6 | 13788 | `				if( pFile ){` |
|        6 | 13789 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|        2 | 13790 | `				}` |
|        6 | 13791 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|        6 | 13792 | `				if( SyBlobLength(pWorker) > 0 ){` |
|        6 | 13793 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|        2 | 13794 | `				}` |
|        2 | 13795 | `			}` |
|        - | 13796 | `			/* Abort immediately */` |
|        6 | 13797 | `			return SXERR_ABORT;` |
|        - | 13798 | `		}` |
|      267 | 13799 | `	}` |
|      653 | 13800 | `	if( pGen->xErr == 0 ){` |
|        - | 13801 | `		/* No available error consumer,return immediately */` |
|        3 | 13802 | `		return SXRET_OK;` |
|        - | 13803 | `	}` |
|      650 | 13804 | `	switch(nErrType){` |
|      536 | 13805 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|       32 | 13806 | `	case E_WARNING: zErr = "Warning";     break;` |
|       82 | 13807 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       12 | 13808 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|      ! 0 | 13809 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|      ! 0 | 13810 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|      ! 0 | 13811 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|      ! 0 | 13812 | `	default:` |
|      ! 0 | 13813 | `		break;` |
|        - | 13814 | `	}` |
|      650 | 13815 | `	rc = SXRET_OK;` |
|        - | 13816 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|      650 | 13817 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|      650 | 13818 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|      650 | 13819 | `	va_start(ap,zFormat);` |
|      650 | 13820 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      650 | 13821 | `	va_end(ap);` |
|      650 | 13822 | `	if( pFile ){` |
|      650 | 13823 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|      323 | 13824 | `	}` |
|        - | 13825 | `	/* Append a new line */` |
|      650 | 13826 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|      650 | 13827 | `	if( SyBlobLength(pWorker) > 0 ){` |
|        - | 13828 | `		/* Consume the generated error message */` |
|      650 | 13829 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|      323 | 13830 | `	}` |
|      650 | 13831 | `	return rc;` |
|      331 | 13832 | `}` |
|        - | 13833 |  |
