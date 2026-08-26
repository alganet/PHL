# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6995/8678 lines (80.61%)

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
|  5900676 |   161 | `static void GenStateInitBlock(` |
|        - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|        - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|        - |   166 | `	void *pUserData      /* Upper layer private data */` |
|        - |   167 | `	)` |
|        5 |   168 | `{` |
|        - |   169 | `	/* Initialize block fields */` |
|  5900681 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  5900681 |   171 | `	pBlock->pUserData   = pUserData;` |
|  5900681 |   172 | `	pBlock->pGen        = pGen;` |
|  5900681 |   173 | `	pBlock->iFlags      = iType;` |
|  5900681 |   174 | `	pBlock->pParent     = 0;` |
|  5900681 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5900681 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5900681 |   177 | `}` |
|        - |   178 | `/*` |
|        - |   179 | ` * Allocate a new block instance.` |
|        - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|        - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|        - |   182 | ` * processing on failure.` |
|        - |   183 | ` */` |
|  5896792 |   184 | `static sxi32 GenStateEnterBlock(` |
|        - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|        - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|        - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|        - |   190 | `	)` |
|        5 |   191 | `{` |
|        - |   192 | `	GenBlock *pBlock;` |
|        - |   193 | `	/* Allocate a new block instance */` |
|  5896797 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  5896797 |   195 | `	if( pBlock == 0 ){` |
|        - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   198 | `		 */` |
|      ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|        - |   200 | `		/* Abort processing immediately */` |
|      ! 0 |   201 | `		return SXERR_ABORT;` |
|        - |   202 | `	}` |
|        - |   203 | `	/* Zero the structure */` |
|  5896797 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  5896797 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|        - |   206 | `	/* Link to the parent block */` |
|  5896797 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|        - |   208 | `	/* Mark as the current block */` |
|  5896797 |   209 | `	pGen->pCurrent = pBlock;` |
|  5896797 |   210 | `	if( ppBlock ){` |
|        - |   211 | `		/* Write a pointer to the new instance */` |
|  2857763 |   212 | `		*ppBlock = pBlock;` |
|  1428879 |   213 | `	}` |
|  5896797 |   214 | `	return SXRET_OK;` |
|  2948401 |   215 | `}` |
|        - |   216 | `/*` |
|        - |   217 | ` * Release block fields without freeing the whole instance.` |
|        - |   218 | ` */` |
|  5896784 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|        5 |   220 | `{` |
|  5896789 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  5896789 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  5896789 |   223 | `}` |
|        - |   224 | `/*` |
|        - |   225 | ` * Release a block.` |
|        - |   226 | ` */` |
|  5896784 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|        5 |   228 | `{` |
|  5896789 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  5896789 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|        - |   231 | `	/* Free the instance */` |
|  5896789 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  5896789 |   233 | `}` |
|        - |   234 | `/*` |
|        - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|        - |   236 | ` */` |
|  5896784 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|        5 |   238 | `{` |
|  5896789 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  5896789 |   240 | `	if( pBlock == 0 ){` |
|        - |   241 | `		/* No more block to pop */` |
|      ! 0 |   242 | `		return SXERR_EMPTY;` |
|        - |   243 | `	}` |
|        - |   244 | `	/* Point to the upper block */` |
|  5896789 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  5896789 |   246 | `	if( ppBlock ){` |
|        - |   247 | `		/* Write a pointer to the popped block */` |
|      ! 0 |   248 | `		*ppBlock = pBlock;` |
|      ! 0 |   249 | `	}else{` |
|        - |   250 | `		/* Safely release the block */` |
|  5896789 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|        - |   252 | `	}` |
|  5896789 |   253 | `	return SXRET_OK;` |
|  2948397 |   254 | `}` |
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
|  2228402 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|        5 |   266 | `{` |
|        - |   267 | `	JumpFixup sJumpFix;` |
|        - |   268 | `	sxi32 rc;` |
|        - |   269 | `	/* Init the JumpFixup structure */` |
|  2228407 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  2228407 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|        - |   272 | `	/* Insert in the jump fixup table */` |
|  2228407 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  2228407 |   274 | `	return rc;` |
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
|  4203334 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|        5 |   288 | `{` |
|        - |   289 | `	JumpFixup *aFix;` |
|        - |   290 | `	VmInstr *pInstr;` |
|        - |   291 | `	sxu32 nFixed;` |
|        - |   292 | `	sxu32 n;` |
|        - |   293 | `	/* Point to the jump fixup table */` |
|  4203339 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|        - |   295 | `	/* Fix the desired jumps */` |
|  8173127 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  3969793 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|        - |   298 | `			/* Already fixed */` |
|  1429819 |   299 | `			continue;` |
|        - |   300 | `		}` |
|  2539979 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|        - |   302 | `			/* Not of our interest */` |
|   311579 |   303 | `			continue;` |
|        - |   304 | `		}` |
|        - |   305 | `		/* Point to the instruction to fix */` |
|  2228405 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  2228405 |   307 | `		if( pInstr ){` |
|  2228405 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  2228405 |   309 | `			nFixed++;` |
|        - |   310 | `			/* Mark as fixed */` |
|  2228405 |   311 | `			aFix[n].nJumpType = -1;` |
|  1114200 |   312 | `		}` |
|  1114205 |   313 | `	}` |
|        - |   314 | `	/* Total number of fixed jumps */` |
|  4203339 |   315 | `	return nFixed;` |
|        5 |   316 | `}` |
|        - |   317 | `/*` |
|        - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|        - |   319 | ` * The goto statement can be used to jump to another section` |
|        - |   320 | ` * in the program.` |
|        - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|        - |   322 | ` * statement for more information.` |
|        - |   323 | ` */` |
|  1470600 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|        5 |   325 | `{` |
|        - |   326 | `	JumpFixup *pJump,*aJumps;` |
|        - |   327 | `	Label *pLabel,*aLabel;` |
|        - |   328 | `	VmInstr *pInstr;` |
|        - |   329 | `	sxi32 rc;` |
|        - |   330 | `	sxu32 n;` |
|        - |   331 | `	/* Point to the goto table */` |
|  1470605 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|        - |   333 | `	/* Fix */` |
|  1470751 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  1470603 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  1470735 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|        - |   362 | `			/* Emit a warning */` |
|       40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|       24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|       12 |   365 | `		}` |
|       71 |   366 | `	}` |
|  1470603 |   367 | `	return SXRET_OK;` |
|   735305 |   368 | `}` |
|        - |   369 | `/*` |
|        - |   370 | ` * Check if a given token value is installed in the literal table.` |
|        - |   371 | ` */` |
|  7556400 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|        5 |   373 | `{` |
|        - |   374 | `	SyHashEntry *pEntry;` |
|  7556405 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  7556405 |   376 | `	if( pEntry == 0 ){` |
|  1985071 |   377 | `		return SXERR_NOTFOUND;` |
|        - |   378 | `	}` |
|  5571339 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  5571339 |   380 | `	return SXRET_OK;` |
|  3778205 |   381 | `}` |
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
|  1985066 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|        5 |   393 | `{` |
|  1985071 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  1985071 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   992533 |   396 | `	}` |
|  1985071 |   397 | `	return SXRET_OK;` |
|        5 |   398 | `}` |
|        - |   399 | `/*` |
|        - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|        - |   401 | ` * in the constant table.` |
|        - |   402 | ` */` |
|  1296038 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|        5 |   404 | `{` |
|        - |   405 | `	ph7_value *pObj;` |
|  1296043 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|        - |   407 | `	/* Reserve a new constant */` |
|  1296043 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1296043 |   409 | `	if( pObj == 0 ){` |
|      ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   411 | `		return 0;` |
|        - |   412 | `	}` |
|  1296043 |   413 | `	*pIdx = nIdx;` |
|        - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|        - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|        - |   416 | `	 */` |
|  1296043 |   417 | `	return pObj;` |
|   648024 |   418 | `}` |
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
|  3784926 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|        5 |   434 | `{` |
|        - |   435 | `	VmCallArgMap *pMap;` |
|  3784931 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|       39 |   437 | `	if( p3 == 0 ){` |
|       35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       35 |   439 | `		if( pMap == 0 ) return 0;` |
|       35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       35 |   441 | `		p3 = (void *)pMap;` |
|       16 |   442 | `	}` |
|       39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|       39 |   444 | `	return p3;` |
|  1892468 |   445 | `}` |
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
|  1297028 |   509 | `static int GenStateFindBadNumericSeparator(` |
|        - |   510 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   511 | `{` |
|  1297033 |   512 | `	const char *z = pRaw->zString;` |
|  1297033 |   513 | `	sxu32 n = pRaw->nByte;` |
|  1297033 |   514 | `	int base = 10;` |
|        - |   515 | `	sxu32 i, start;` |
|  1297033 |   516 | `	if( n < 2 ) return 0;` |
|   404251 |   517 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|       80 |   518 | `		base = 16;` |
|   404212 |   519 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      284 |   520 | `		base = 2;` |
|      141 |   521 | `	}` |
|  1306941 |   522 | `	for( i = 0; i < n; ++i ){` |
|   902709 |   523 | `		if( z[i] != '_' ) continue;` |
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
|   404237 |   540 | `	return 0;` |
|   648519 |   541 | `}` |
|        - |   542 | `/*` |
|        - |   543 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   544 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   545 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   546 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   547 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   548 | ` * so callers can bail from the current construct).` |
|        - |   549 | ` */` |
|  1297028 |   550 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   551 | `{` |
|  1297033 |   552 | `	const char *zBad = 0;` |
|  1297033 |   553 | `	sxu32 nBad = 0;` |
|        - |   554 | `	SyString sBad;` |
|        - |   555 | `	sxi32 rc;` |
|  1297033 |   556 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  1297019 |   557 | `		return SXRET_OK;` |
|        - |   558 | `	}` |
|       18 |   559 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       18 |   560 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   561 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       18 |   562 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   563 | `		return SXERR_ABORT;` |
|        - |   564 | `	}` |
|       18 |   565 | `	return SXERR_SYNTAX;` |
|   648519 |   566 | `}` |
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
|  1297014 |   583 | `static sxi32 GenStateStripNumericSeparators(` |
|        - |   584 | `	SyMemBackend *pAlloc,` |
|        - |   585 | `	const SyString *pToken,` |
|        - |   586 | `	char *zScratch, sxu32 nScratch,` |
|        - |   587 | `	SyString *pOut, char **pzAlloc)` |
|        5 |   588 | `{` |
|        - |   589 | `	sxu32 i, j;` |
|  1297019 |   590 | `	int hasUnderscore = 0;` |
|        - |   591 | `	char *zBuf;` |
|  1297019 |   592 | `	*pzAlloc = 0;` |
|  3090425 |   593 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  1793663 |   594 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   896708 |   595 | `	}` |
|  1297019 |   596 | `	if( !hasUnderscore ){` |
|  1296767 |   597 | `		SyStringDupPtr(pOut, pToken);` |
|  1296767 |   598 | `		return SXRET_OK;` |
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
|   648512 |   615 | `}` |
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
|        - |   649 | ` * residual; matching php exactly would need a port of those functions.` |
|        - |   650 | ` */` |
|  1296072 |   651 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |   652 | `{` |
|  1296077 |   653 | `	const char *z = pNum->zString;` |
|  1296077 |   654 | `	const char *zEnd = z + pNum->nByte;` |
|        - |   655 | `	const char *p, *q;` |
|        - |   656 | `	int n;` |
|  1296077 |   657 | `	*pbDecimal = FALSE;` |
|  1296077 |   658 | `	if( z >= zEnd ){` |
|      ! 0 |   659 | `		return FALSE;` |
|        - |   660 | `	}` |
|  1296077 |   661 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
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
|  1296001 |   676 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|  1295721 |   691 | `	}else if( z[0] == '0' ){` |
|        - |   692 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |   693 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |   694 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   359495 |   695 | `		p = z;` |
|   718987 |   696 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   359723 |   697 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   359495 |   698 | `		if( n <= 21 ){` |
|   359493 |   699 | `			return FALSE;` |
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
|   936231 |   712 | `	p = z;` |
|   936231 |   713 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  2363677 |   714 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   936231 |   715 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       25 |   716 | `		*pbDecimal = TRUE;` |
|       25 |   717 | `		return TRUE;` |
|        - |   718 | `	}` |
|   936207 |   719 | `	return FALSE;` |
|   648041 |   720 | `}` |
|  1297000 |   721 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   722 | `{` |
|  1297005 |   723 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  1297005 |   724 | `	sxu32 nIdx = 0;` |
|        - |   725 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  1297005 |   726 | `	char *zAlloc = 0;` |
|        - |   727 | `	SyString sNum;` |
|        - |   728 | `	sxi32 rc;` |
|   648500 |   729 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  1297005 |   730 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  1297005 |   731 | `	if( rc != SXRET_OK ){` |
|       14 |   732 | `		return rc;` |
|        - |   733 | `	}` |
|  1945490 |   734 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   648495 |   735 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  1296995 |   736 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   737 | `		return SXERR_ABORT;` |
|        - |   738 | `	}` |
|  1296995 |   739 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |   740 | `		ph7_value *pObj;` |
|        - |   741 | `		sxi64 iValue;` |
|  1296077 |   742 | `		ph7_real rOverflow = 0;` |
|  1296077 |   743 | `		int bDecimalOverflow = 0;` |
|  1296077 |   744 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|  1296043 |   761 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  1296043 |   762 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  1296043 |   763 | `			if( pObj == 0 ){` |
|      ! 0 |   764 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   765 | `				return SXERR_ABORT;` |
|        - |   766 | `			}` |
|  1296043 |   767 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |   768 | `		}` |
|   648041 |   769 | `	}else{` |
|        - |   770 | `		/* Real number */` |
|        - |   771 | `		ph7_value *pObj;` |
|        - |   772 | `		/* Reserve a new constant */` |
|      922 |   773 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      922 |   774 | `		if( pObj == 0 ){` |
|      ! 0 |   775 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   776 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   777 | `			return SXERR_ABORT;` |
|        - |   778 | `		}` |
|      922 |   779 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      922 |   780 | `		PH7_MemObjToReal(pObj);` |
|        - |   781 | `	}` |
|  1296995 |   782 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |   783 | `	/* Emit the load constant instruction */` |
|  1296995 |   784 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |   785 | `	/* Node successfully compiled */` |
|  1296995 |   786 | `	return SXRET_OK;` |
|   648505 |   787 | `}` |
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
|  3100270 |   799 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   800 | `{` |
|  3100275 |   801 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |   802 | `	const char *zIn,*zCur,*zEnd;` |
|        - |   803 | `	ph7_value *pObj;` |
|        - |   804 | `	sxu32 nIdx;` |
|  3100275 |   805 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |   806 | `	/* Delimit the string */` |
|  3100275 |   807 | `	zIn  = pStr->zString;` |
|  3100275 |   808 | `	zEnd = &zIn[pStr->nByte];` |
|  3100275 |   809 | `	if( zIn >= zEnd ){` |
|        - |   810 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |   811 | `		 * rather than reserving a new object each time. */` |
|   136133 |   812 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   136133 |   813 | `		return SXRET_OK;` |
|        - |   814 | `	}` |
|  2964147 |   815 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |   816 | `		/* Already processed,emit the load constant instruction` |
|        - |   817 | `		 * and return.` |
|        - |   818 | `		 */` |
|  1911217 |   819 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  1911217 |   820 | `		return SXRET_OK;` |
|        - |   821 | `	}` |
|        - |   822 | `	/* Reserve a new constant */` |
|  1052935 |   823 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1052935 |   824 | `	if( pObj == 0 ){` |
|      ! 0 |   825 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   826 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |   827 | `		return SXERR_ABORT;` |
|        - |   828 | `	}` |
|  1052935 |   829 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |   830 | `	/* Compile the node */` |
|  1052989 |   831 | `	for(;;){` |
|  2105983 |   832 | `		if( zIn >= zEnd ){` |
|        - |   833 | `			/* End of input */` |
|  1052935 |   834 | `			break;` |
|        - |   835 | `		}` |
|  1053053 |   836 | `		zCur = zIn;` |
| 20192995 |   837 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 19139947 |   838 | `			zIn++;` |
|        5 |   839 | `		}` |
|  1053053 |   840 | `		if( zIn > zCur ){` |
|        - |   841 | `			/* Append raw contents*/` |
|  1021955 |   842 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   510975 |   843 | `		}` |
|  1053053 |   844 | `		zIn++;` |
|  1053053 |   845 | `		if( zIn < zEnd ){` |
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
|  1053053 |   860 | `		zIn++;` |
|        5 |   861 | `	}` |
|        - |   862 | `	/* Emit the load constant instruction */` |
|  1052935 |   863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  1052935 |   864 | `	if( pStr->nByte < 1024 ){` |
|        - |   865 | `		/* Install in the literal table */` |
|  1052935 |   866 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   526465 |   867 | `	}` |
|        - |   868 | `	/* Node successfully compiled */` |
|  1052935 |   869 | `	return SXRET_OK;` |
|  1550140 |   870 | `}` |
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
|     2468 |  1036 | `static sxi32 GenStateProcessStringExpression(` |
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
|     2473 |  1047 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        - |  1048 | `	/* Preallocate some slots */` |
|     2473 |  1049 | `	SySetAlloc(&sToken,0x08);` |
|        - |  1050 | `	/* Tokenize the text */` |
|     2473 |  1051 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|        - |  1052 | `	/* Swap delimiter */` |
|     2473 |  1053 | `	pTmpIn  = pGen->pIn;` |
|     2473 |  1054 | `	pTmpEnd = pGen->pEnd;` |
|     2473 |  1055 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     2473 |  1056 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        - |  1057 | `	/* Compile the expression */` |
|     2473 |  1058 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  1059 | `	/* Restore token stream */` |
|     2473 |  1060 | `	pGen->pIn  = pTmpIn;` |
|     2473 |  1061 | `	pGen->pEnd = pTmpEnd;` |
|        - |  1062 | `	/* Release the token set */` |
|     2473 |  1063 | `	SySetRelease(&sToken);` |
|        - |  1064 | `	/* Compilation result */` |
|     2473 |  1065 | `	return rc;` |
|        5 |  1066 | `}` |
|        - |  1067 | `/*` |
|        - |  1068 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|        - |  1069 | ` */` |
|    38892 |  1070 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  1071 | `{` |
|        - |  1072 | `	ph7_value *pConstObj;` |
|    38897 |  1073 | `	sxu32 nIdx = 0;` |
|        - |  1074 | `	/* Reserve a new constant */` |
|    38897 |  1075 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    38897 |  1076 | `	if( pConstObj == 0 ){` |
|      ! 0 |  1077 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  1078 | `		return 0;` |
|        - |  1079 | `	}` |
|    38897 |  1080 | `	(*pCount)++;` |
|    38897 |  1081 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  1082 | `	/* Emit the load constant instruction */` |
|    38897 |  1083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    38897 |  1084 | `	return pConstObj;` |
|    19451 |  1085 | `}` |
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
|    37378 |  1148 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  1149 | `{` |
|    37383 |  1150 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  1151 | `	const char *zIn,*zCur,*zEnd;` |
|    37383 |  1152 | `	ph7_value *pObj = 0;` |
|        - |  1153 | `	sxi32 iCons;` |
|        - |  1154 | `	sxi32 rc;` |
|        - |  1155 | `	/* Delimit the string */` |
|    37383 |  1156 | `	zIn  = pStr->zString;` |
|    37383 |  1157 | `	zEnd = &zIn[pStr->nByte];` |
|    37383 |  1158 | `	if( zIn >= zEnd ){` |
|        - |  1159 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  1160 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  1161 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  1162 | `		 */` |
|      377 |  1163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      377 |  1164 | `		return SXRET_OK;` |
|        - |  1165 | `	}` |
|    37011 |  1166 | `	zCur = 0;` |
|        - |  1167 | `	/* Compile the node */` |
|    37011 |  1168 | `	iCons = 0;` |
|    19737 |  1169 | `	for(;;){` |
|    63507 |  1170 | `		zCur = zIn;` |
|   216411 |  1171 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   155377 |  1172 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       72 |  1173 | `				break;` |
|   155243 |  1174 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2338 |  1175 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1170 |  1176 | `					break;` |
|        - |  1177 | `			}` |
|   152909 |  1178 | `			zIn++;` |
|        5 |  1179 | `		}` |
|    63507 |  1180 | `		if( zIn > zCur ){` |
|    20655 |  1181 | `			if( pObj == 0 ){` |
|    20125 |  1182 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    20125 |  1183 | `				if( pObj == 0 ){` |
|      ! 0 |  1184 | `					return SXERR_ABORT;` |
|        - |  1185 | `				}` |
|    10060 |  1186 | `			}` |
|    20655 |  1187 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    10325 |  1188 | `		}` |
|    63507 |  1189 | `		if( zIn >= zEnd ){` |
|    37009 |  1190 | `			break;` |
|        - |  1191 | `		}` |
|    26503 |  1192 | `		if( zIn[0] == '\\' ){` |
|    24035 |  1193 | `			const char *zPtr = 0;` |
|        - |  1194 | `			sxu32 n;` |
|    24035 |  1195 | `			zIn++;` |
|    24035 |  1196 | `			if( pObj == 0 ){` |
|    18777 |  1197 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    18777 |  1198 | `				if( pObj == 0 ){` |
|      ! 0 |  1199 | `					return SXERR_ABORT;` |
|        - |  1200 | `				}` |
|     9386 |  1201 | `			}` |
|    24035 |  1202 | `			if( zIn >= zEnd ){` |
|        - |  1203 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  1204 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  1205 | `				break;` |
|        - |  1206 | `			}` |
|    24033 |  1207 | `			n = sizeof(char); /* size of conversion */` |
|    24033 |  1208 | `			switch( zIn[0] ){` |
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
|    11462 |  1225 | `			case 'n':` |
|        - |  1226 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    22929 |  1227 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    22929 |  1228 | `				break;` |
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
|    24033 |  1351 | `			zIn += n;` |
|    24033 |  1352 | `			continue;` |
|        - |  1353 | `		}` |
|     2473 |  1354 | `		if( zIn[0] == '{' ){` |
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
|     2335 |  1388 | `			const char *zExpr = zIn;` |
|        - |  1389 | `			/* Assemble variable name */` |
|     1190 |  1390 | `			for(;;){` |
|        - |  1391 | `				/* Jump leading dollars */` |
|     4715 |  1392 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|     2335 |  1393 | `					zIn++;` |
|        5 |  1394 | `				}` |
|     1190 |  1395 | `				for(;;){` |
|    12495 |  1396 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|     8925 |  1397 | `						zIn++;` |
|        5 |  1398 | `					}` |
|     2385 |  1399 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|        - |  1400 | `						/* UTF-8 stream */` |
|      ! 0 |  1401 | `						zIn++;` |
|      ! 0 |  1402 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  1403 | `							zIn++;` |
|      ! 0 |  1404 | `						}` |
|      ! 0 |  1405 | `						continue;` |
|        - |  1406 | `					}` |
|     2385 |  1407 | `					break;` |
|      ! 0 |  1408 | `				}` |
|     2385 |  1409 | `				if( zIn >= zEnd ){` |
|      252 |  1410 | `					break;` |
|        - |  1411 | `				}` |
|     2137 |  1412 | `				if( zIn[0] == '[' ){` |
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
|     2127 |  1430 | `				}else if(zIn[0] == '{' ){` |
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
|     2123 |  1448 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|        - |  1449 | `					/* Member access operator '->' */` |
|       53 |  1450 | `					zIn += 2;` |
|     2098 |  1451 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|        - |  1452 | `					/* Static member access operator '::' */` |
|      ! 0 |  1453 | `					zIn += 2;` |
|      ! 0 |  1454 | `				}else{` |
|     1039 |  1455 | `					break;` |
|        - |  1456 | `				}` |
|        3 |  1457 | `			}` |
|        - |  1458 | `			/* Process the expression */` |
|     2335 |  1459 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     2335 |  1460 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1461 | `				return SXERR_ABORT;` |
|        - |  1462 | `			}` |
|     2335 |  1463 | `			if( rc != SXERR_EMPTY ){` |
|     2333 |  1464 | `				++iCons;` |
|     1164 |  1465 | `			}` |
|        - |  1466 | `		}` |
|        - |  1467 | `		/* Invalidate the previously used constant */` |
|     2473 |  1468 | `		pObj = 0;` |
|        5 |  1469 | `	}/*for(;;)*/` |
|    37011 |  1470 | `	if( iCons > 1 ){` |
|        - |  1471 | `		/* Concatenate all compiled constants */` |
|     1807 |  1472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|      901 |  1473 | `	}` |
|        - |  1474 | `	/* Node successfully compiled */` |
|    37011 |  1475 | `	return SXRET_OK;` |
|    18694 |  1476 | `}` |
|        - |  1477 | `/*` |
|        - |  1478 | ` * Compile a double quoted string.` |
|        - |  1479 | ` *  See the block-comment above for more information.` |
|        - |  1480 | ` */` |
|    37316 |  1481 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1482 | `{` |
|        - |  1483 | `	sxi32 rc;` |
|    37321 |  1484 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    18658 |  1485 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  1486 | `	/* Compilation result */` |
|    37321 |  1487 | `	return rc;` |
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
|   529886 |  1531 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   529891 |  1542 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - |  1543 | `	/* Compile the expression*/` |
|   529891 |  1544 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - |  1545 | `	/* Restore token stream */` |
|   529891 |  1546 | `	RE_SWAP_DELIMITER(pGen);` |
|   529891 |  1547 | `	return rc;` |
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
|   567480 |  1588 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 |  1589 | `{` |
|   567485 |  1590 | `	SyToken *pCur = pStart;` |
|   567485 |  1591 | `	sxi32 iNest = 0;` |
|  1720703 |  1592 | `	while( pCur < pEnd ){` |
|  1357353 |  1593 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   204131 |  1594 | `			return pCur;` |
|        - |  1595 | `		}` |
|        - |  1596 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - |  1597 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - |  1598 | `		 * not an entry separator. Skip past the signature.` |
|        - |  1599 | `		 */` |
|  1153227 |  1600 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|    19527 |  1601 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    19527 |  1602 | `			SyToken *pFn = pCur;` |
|    19522 |  1603 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|      ! 0 |  1604 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|        5 |  1605 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  1606 | `				pFn = &pCur[1];` |
|      ! 0 |  1607 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  1608 | `			}` |
|    19527 |  1609 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|    19523 |  1640 | `			if( nKw == PH7_TKWRD_MATCH ){` |
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
|     9758 |  1660 | `		}` |
|  1153221 |  1661 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    50943 |  1662 | `			iNest++;` |
|  1127752 |  1663 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - |  1664 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - |  1665 | `			 * parser will shortly detect any syntax error. */` |
|    50943 |  1666 | `			iNest--;` |
|    25469 |  1667 | `		}` |
|  1153221 |  1668 | `		pCur++;` |
|        5 |  1669 | `	}` |
|   363355 |  1670 | `	return pEnd;` |
|   283745 |  1671 | `}` |
|        - |  1672 | `/*` |
|        - |  1673 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - |  1674 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - |  1675 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - |  1676 | ` */` |
|   290996 |  1677 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 |  1678 | `{` |
|        - |  1679 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - |  1680 | `	SyToken *pKey,*pCur;` |
|   291001 |  1681 | `	sxi32 iEmitRef = 0;` |
|   291001 |  1682 | `	sxi32 iSpread = 0;` |
|   291001 |  1683 | `	sxi32 nPair = 0;` |
|        - |  1684 | `	sxi32 rc;` |
|   291001 |  1685 | `	xValidator = 0;` |
|   341410 |  1686 | `	for(;;){` |
|        - |  1687 | `		/* Jump leading commas */` |
|   974537 |  1688 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   291717 |  1689 | `			pGen->pIn++;` |
|        5 |  1690 | `		}` |
|   682825 |  1691 | `		pCur = pGen->pIn;` |
|   682825 |  1692 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - |  1693 | `			/* No more entry to process */` |
|   290985 |  1694 | `			break;` |
|        - |  1695 | `		}` |
|   391845 |  1696 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 |  1697 | `			continue;` |
|        - |  1698 | `		}` |
|        - |  1699 | `		/* Compile the key if available */` |
|   391845 |  1700 | `		pKey = pCur;` |
|   391845 |  1701 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   391845 |  1702 | `		rc = SXERR_EMPTY;` |
|   391845 |  1703 | `		if( pCur < pGen->pIn ){` |
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
|   322945 |  1719 | `		}else if( pKey == pCur ){` |
|        - |  1720 | `			/* Key is omitted,emit a warning */` |
|      ! 0 |  1721 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|      ! 0 |  1722 | `			pCur++; /* Jump the '=>' operator */` |
|      ! 0 |  1723 | `		}else{` |
|        - |  1724 | `			/* Reset back the cursor and point to the entry value */` |
|   254055 |  1725 | `			pCur = pKey;` |
|        - |  1726 | `		}` |
|   391835 |  1727 | `		if( rc == SXERR_EMPTY ){` |
|        - |  1728 | `			/* No available key,load NULL */` |
|   254057 |  1729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|   127026 |  1730 | `		}` |
|   391835 |  1731 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   391833 |  1750 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   391833 |  1751 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   391829 |  1764 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   391829 |  1765 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  1766 | `			return SXERR_ABORT;` |
|        - |  1767 | `		}` |
|   391829 |  1768 | `		if( iSpread ){` |
|        - |  1769 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|       69 |  1770 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   391796 |  1771 | `		}else if( iEmitRef ){` |
|        - |  1772 | `			/* Emit the load reference instruction */` |
|       41 |  1773 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       18 |  1774 | `		}` |
|   391829 |  1775 | `		xValidator = 0;` |
|   391829 |  1776 | `		iEmitRef = 0;` |
|   391829 |  1777 | `		iSpread = 0;` |
|   391829 |  1778 | `		nPair++;` |
|        5 |  1779 | `	}` |
|        - |  1780 | `	/* Emit the load map instruction */` |
|   290985 |  1781 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - |  1782 | `	/* Node successfully compiled */` |
|   290985 |  1783 | `	return SXRET_OK;` |
|   145503 |  1784 | `}` |
|        - |  1785 | `/*` |
|        - |  1786 | ` * Compile the 'array' language construct.` |
|        - |  1787 | ` *	 According to the PHP language reference manual` |
|        - |  1788 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|        - |  1789 | ` *   values to keys. This type is optimized for several different uses; it can` |
|        - |  1790 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|        - |  1791 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|        - |  1792 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|        - |  1793 | ` */` |
|   289264 |  1794 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1795 | `{` |
|        - |  1796 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   289269 |  1797 | `	pGen->pIn += 2;` |
|   289269 |  1798 | `	pGen->pEnd--;` |
|   144632 |  1799 | `	SXUNUSED(iCompileFlag);` |
|   289269 |  1800 | `	return GenStateCompileArrayBody(pGen);` |
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
|     1732 |  1899 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1900 | `{` |
|        - |  1901 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     1737 |  1902 | `	pGen->pIn++;` |
|     1737 |  1903 | `	pGen->pEnd--;` |
|      866 |  1904 | `	SXUNUSED(iCompileFlag);` |
|     1737 |  1905 | `	return GenStateCompileArrayBody(pGen);` |
|        5 |  1906 | `}` |
|        - |  1907 | `/*` |
|        - |  1908 | ` * Expression tree validator callback for the 'list' language construct.` |
|        - |  1909 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - |  1910 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - |  1911 | ` * error message.` |
|        - |  1912 | ` * See the routine responible of compiling the list language construct` |
|        - |  1913 | ` * for more inforation.` |
|        - |  1914 | ` */` |
|      202 |  1915 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  1916 | `{` |
|      207 |  1917 | `	sxi32 rc = SXRET_OK;` |
|      207 |  1918 | `	if( pRoot->pOp ){` |
|        4 |  1919 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|        2 |  1920 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|        - |  1921 | `				/* Unexpected expression */` |
|      ! 0 |  1922 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1923 | `					"list(): Expecting a variable not an expression");` |
|      ! 0 |  1924 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  1925 | `					rc = SXERR_INVALID;` |
|      ! 0 |  1926 | `				}` |
|        1 |  1927 | `		}` |
|      205 |  1928 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  1929 | `		/* Unexpected expression */` |
|        6 |  1930 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1931 | `			"list(): Expecting a variable not an expression");` |
|        6 |  1932 | `		if( rc != SXERR_ABORT ){` |
|        6 |  1933 | `			rc = SXERR_INVALID;` |
|        2 |  1934 | `		}` |
|        2 |  1935 | `	}` |
|      207 |  1936 | `	return rc;` |
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
|      122 |  2059 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|        5 |  2060 | `{` |
|        - |  2061 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|        - |  2062 | `	SyToken *pNext;` |
|        - |  2063 | `	SyToken *pClassifyIn;` |
|      127 |  2064 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|        - |  2065 | `	sxi32 nExpr;` |
|        - |  2066 | `	sxi32 rc;` |
|        - |  2067 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|        - |  2068 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|        - |  2069 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|        - |  2070 | `	 * list. */` |
|      127 |  2071 | `	pClassifyIn = pGen->pIn;` |
|      359 |  2072 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      237 |  2073 | `		if( pGen->pIn >= pNext ){` |
|       13 |  2074 | `			nEmpty++;` |
|      231 |  2075 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|       38 |  2076 | `			nKeyed++;` |
|       20 |  2077 | `		}else{` |
|      189 |  2078 | `			nPositional++;` |
|        - |  2079 | `		}` |
|      237 |  2080 | `		pGen->pIn = &pNext[1];` |
|        5 |  2081 | `	}` |
|      127 |  2082 | `	pGen->pIn = pClassifyIn;` |
|      127 |  2083 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|      ! 0 |  2084 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  2085 | `			"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 |  2086 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2087 | `	}` |
|      127 |  2088 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|      ! 0 |  2089 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  2090 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|      ! 0 |  2091 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2092 | `	}` |
|      127 |  2093 | `	if( nKeyed > 0 ){` |
|       30 |  2094 | `		return GenStateCompileKeyedListBody(pGen);` |
|        - |  2095 | `	}` |
|       99 |  2096 | `	nExpr = 0;` |
|       99 |  2097 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|      295 |  2098 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      201 |  2099 | `		if( pGen->pIn < pNext ){` |
|        - |  2100 | `			/* Check for nested list() */` |
|      189 |  2101 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|      188 |  2118 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|      175 |  2134 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      175 |  2135 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  2136 | `					SySetRelease(&sNested);` |
|      ! 0 |  2137 | `					return SXRET_OK;` |
|        - |  2138 | `				}` |
|        - |  2139 | `			}` |
|       97 |  2140 | `		}else{` |
|        - |  2141 | `			/* Empty entry,load NULL */` |
|       13 |  2142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|        - |  2143 | `		}` |
|      201 |  2144 | `		nExpr++;` |
|        - |  2145 | `		/* Advance the stream cursor */` |
|      201 |  2146 | `		pGen->pIn = &pNext[1];` |
|        5 |  2147 | `	}` |
|        - |  2148 | `	/* Emit the LOAD_LIST instruction */` |
|       99 |  2149 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|        - |  2150 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|        - |  2151 | `	 * For each nested entry, emit code to extract the sub-array` |
|        - |  2152 | `	 * at the corresponding index and recursively destructure it.` |
|        - |  2153 | `	 */` |
|       99 |  2154 | `	if( SySetUsed(&sNested) > 0 ){` |
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
|       99 |  2196 | `	SySetRelease(&sNested);` |
|        - |  2197 | `	/* Node successfully compiled */` |
|       99 |  2198 | `	return SXRET_OK;` |
|       66 |  2199 | `}` |
|       38 |  2200 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2201 | `{` |
|        - |  2202 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|       43 |  2203 | `	pGen->pIn += 2;` |
|       43 |  2204 | `	pGen->pEnd--;` |
|       19 |  2205 | `	SXUNUSED(iCompileFlag);` |
|       43 |  2206 | `	return GenStateCompileListBody(pGen);` |
|        5 |  2207 | `}` |
|       84 |  2208 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 |  2209 | `{` |
|        - |  2210 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|       88 |  2211 | `	pGen->pIn++;` |
|       88 |  2212 | `	pGen->pEnd--;` |
|       42 |  2213 | `	SXUNUSED(iCompileFlag);` |
|       88 |  2214 | `	return GenStateCompileListBody(pGen);` |
|        4 |  2215 | `}` |
|        - |  2216 | `/* Forward declarations */` |
|        - |  2217 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|        - |  2218 | `static int GenStateIsReservedConstant(SyString *pName);` |
|        - |  2219 | `static int GenStateIsReadonly(SyToken *pTok);` |
|        - |  2220 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);` |
|        - |  2221 | `static sxi32 GenStateSetVisFlag(sxi32 nKw);` |
|        - |  2222 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr);` |
|        - |  2223 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|        - |  2224 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|        - |  2225 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|        - |  2226 | `/*` |
|        - |  2227 | ` * Compile an annoynmous function or a closure.` |
|        - |  2228 | ` * According to the PHP language reference` |
|        - |  2229 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  2230 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  2231 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|        - |  2232 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|        - |  2233 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|        - |  2234 | ` *  Example Anonymous function variable assignment example` |
|        - |  2235 | ` * <?php` |
|        - |  2236 | ` * $greet = function($name)` |
|        - |  2237 | ` * {` |
|        - |  2238 | ` *    printf("Hello %s\r\n", $name);` |
|        - |  2239 | ` * };` |
|        - |  2240 | ` * $greet('World');` |
|        - |  2241 | ` * $greet('PHP');` |
|        - |  2242 | ` * ?>` |
|        - |  2243 | ` * Note that the implementation of annoynmous function and closure under` |
|        - |  2244 | ` * PH7 is completely different from the one used by the zend engine.` |
|        - |  2245 | ` */` |
|      448 |  2246 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2247 | `{` |
|      453 |  2248 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |  2249 | `	char zName[512];         /* Unique lambda name */` |
|        - |  2250 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |  2251 | `							  * one thread is allowed to compile the script.` |
|        - |  2252 | `						      */` |
|        - |  2253 | `	SyString sName;` |
|      453 |  2254 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|        - |  2255 | `	                              * is keyed to this ['static'] 'function' token */` |
|        - |  2256 | `	sxu32 nKwLine;` |
|      453 |  2257 | `	sxi32 iFlags = 0;` |
|        - |  2258 | `	sxu32 nLen;` |
|        - |  2259 | `	sxi32 rc;` |
|      224 |  2260 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2261 |  |
|      453 |  2262 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|      448 |  2263 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      453 |  2264 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |  2265 | `		/* Static closure: no $this auto-capture, bind refused */` |
|        9 |  2266 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        9 |  2267 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|        4 |  2268 | `	}` |
|      453 |  2269 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      453 |  2270 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |  2271 | `		pGen->pIn++;` |
|      ! 0 |  2272 | `	}` |
|        - |  2273 | `	/* Generate a unique name */` |
|      453 |  2274 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |  2275 | `	/* Make sure the generated name is unique */` |
|      453 |  2276 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2277 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2278 | `	}` |
|      453 |  2279 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |  2280 | `	/* Compile the lambda body */` |
|      453 |  2281 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|      453 |  2282 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2283 | `		return SXERR_ABORT;` |
|        - |  2284 | `	}` |
|      453 |  2285 | `	if( pAnnonFunc ){` |
|      453 |  2286 | `		pAnnonFunc->nLine = nKwLine;` |
|        - |  2287 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|        - |  2288 | `		 * sidecar keys them to the closure's first keyword token. */` |
|      453 |  2289 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  2290 | `			return SXERR_ABORT;` |
|        - |  2291 | `		}` |
|      224 |  2292 | `	}` |
|        - |  2293 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |  2294 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |  2295 | `	 * the handler wraps either in a Closure instance. */` |
|      453 |  2296 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |  2297 | `	/* Node successfully compiled */` |
|      453 |  2298 | `	return SXRET_OK;` |
|      229 |  2299 | `}` |
|        - |  2300 | `/*` |
|        - |  2301 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |  2302 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |  2303 | ` * enclosing arrow level, or has already been captured.` |
|        - |  2304 | ` */` |
|      196 |  2305 | `static sxi32 GenStateArrowAddCapture(` |
|        - |  2306 | `	ph7_gen_state *pGen,` |
|        - |  2307 | `	ph7_vm_func *pFunc,` |
|        - |  2308 | `	const char *zName,` |
|        - |  2309 | `	sxu32 nByte,` |
|        - |  2310 | `	SyString *aShadow,` |
|        - |  2311 | `	sxu32 nShadow)` |
|        3 |  2312 | `{` |
|        - |  2313 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2314 | `	ph7_vm_func_closure_env *aEnv;` |
|        - |  2315 | `	sxu32 n, nEnv;` |
|        - |  2316 | `	char *zDup;` |
|      199 |  2317 | `	if( nByte == 0 ){` |
|      ! 0 |  2318 | `		return SXRET_OK;` |
|        - |  2319 | `	}` |
|      196 |  2320 | `	if( nByte == sizeof("this")-1` |
|      107 |  2321 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        3 |  2322 | `		return SXRET_OK;` |
|        - |  2323 | `	}` |
|      247 |  2324 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      182 |  2325 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      176 |  2326 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      135 |  2327 | `			return SXRET_OK;` |
|        - |  2328 | `		}` |
|       27 |  2329 | `	}` |
|       63 |  2330 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       63 |  2331 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|       91 |  2332 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|       30 |  2333 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|       29 |  2334 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|        3 |  2335 | `			return SXRET_OK;` |
|        - |  2336 | `		}` |
|       15 |  2337 | `	}` |
|       61 |  2338 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|       61 |  2339 | `	if( zDup == 0 ){` |
|      ! 0 |  2340 | `		return SXERR_ABORT;` |
|        - |  2341 | `	}` |
|       61 |  2342 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       61 |  2343 | `	sEnv.iFlags = 0;` |
|       61 |  2344 | `	sEnv.nIdx = SXU32_HIGH;` |
|       61 |  2345 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       61 |  2346 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|       61 |  2347 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       61 |  2348 | `	return SXRET_OK;` |
|      101 |  2349 | `}` |
|        - |  2350 | `/*` |
|        - |  2351 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|        - |  2352 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|        - |  2353 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|        - |  2354 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|        - |  2355 | ` */` |
|       56 |  2356 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|        - |  2357 | `	ph7_gen_state *pGen,` |
|        - |  2358 | `	ph7_vm_func *pFunc,` |
|        - |  2359 | `	const char *zIn,` |
|        - |  2360 | `	const char *zEnd,` |
|        - |  2361 | `	SyString *aShadow,` |
|        - |  2362 | `	sxu32 nShadow)` |
|        2 |  2363 | `{` |
|        - |  2364 | `	sxi32 rc;` |
|      370 |  2365 | `	while( zIn < zEnd ){` |
|      314 |  2366 | `		if( zIn[0] == '\\' ){` |
|        5 |  2367 | `			zIn++;` |
|        5 |  2368 | `			if( zIn < zEnd ){` |
|        5 |  2369 | `				zIn++;` |
|        2 |  2370 | `			}` |
|        5 |  2371 | `			continue;` |
|        - |  2372 | `		}` |
|      308 |  2373 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|       26 |  2374 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|       24 |  2375 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|        - |  2376 | `			const char *zName;` |
|       26 |  2377 | `			zIn++; /* skip '$' */` |
|       26 |  2378 | `			zName = zIn;` |
|       82 |  2379 | `			while( zIn < zEnd ){` |
|       76 |  2380 | `				unsigned char c = (unsigned char)zIn[0];` |
|       76 |  2381 | `				if( c >= 0xc0 ){` |
|      ! 0 |  2382 | `					zIn++;` |
|      ! 0 |  2383 | `					while( zIn < zEnd` |
|      ! 0 |  2384 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  2385 | `						zIn++;` |
|      ! 0 |  2386 | `					}` |
|      ! 0 |  2387 | `					continue;` |
|        - |  2388 | `				}` |
|       76 |  2389 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       20 |  2390 | `					break;` |
|        - |  2391 | `				}` |
|       58 |  2392 | `				zIn++;` |
|        2 |  2393 | `			}` |
|       26 |  2394 | `			if( zIn > zName ){` |
|       38 |  2395 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|       24 |  2396 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|       26 |  2397 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2398 | `					return SXERR_ABORT;` |
|        - |  2399 | `				}` |
|       12 |  2400 | `			}` |
|       26 |  2401 | `			continue;` |
|        - |  2402 | `		}` |
|      286 |  2403 | `		zIn++;` |
|        2 |  2404 | `	}` |
|       58 |  2405 | `	return SXRET_OK;` |
|       30 |  2406 | `}` |
|        - |  2407 | `/*` |
|        - |  2408 | ` * Scan the body token range of an arrow function for free-variable` |
|        - |  2409 | ` * references and record them in pFunc's closure environment. Handles:` |
|        - |  2410 | ` *   - plain $<id> pairs` |
|        - |  2411 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|        - |  2412 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|        - |  2413 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|        - |  2414 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|        - |  2415 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|        - |  2416 | ` *     are never mistakenly captured.` |
|        - |  2417 | ` */` |
|      296 |  2418 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  2419 | `	ph7_gen_state *pGen,` |
|        - |  2420 | `	ph7_vm_func *pFunc,` |
|        - |  2421 | `	SyToken *pStart,` |
|        - |  2422 | `	SyToken *pEnd,` |
|        - |  2423 | `	SyString *aShadow,` |
|        - |  2424 | `	sxu32 nShadow)` |
|        4 |  2425 | `{` |
|      300 |  2426 | `	SyToken *pScan = pStart;` |
|        - |  2427 | `	sxi32 rc;` |
|     1708 |  2428 | `	while( pScan < pEnd ){` |
|     1412 |  2429 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       86 |  2430 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       28 |  2431 | `				pScan->sData.zString,` |
|       56 |  2432 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       28 |  2433 | `				aShadow,nShadow);` |
|       58 |  2434 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2435 | `				return SXERR_ABORT;` |
|        - |  2436 | `			}` |
|       58 |  2437 | `			pScan++;` |
|       58 |  2438 | `			continue;` |
|        - |  2439 | `		}` |
|     1356 |  2440 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|       30 |  2441 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|       30 |  2442 | `			SyToken *pFnKw = pScan;` |
|       28 |  2443 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|      ! 0 |  2444 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|        2 |  2445 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  2446 | `				pFnKw = &pScan[1];` |
|      ! 0 |  2447 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  2448 | `			}` |
|       30 |  2449 | `			if( nKw == PH7_TKWRD_FN ){` |
|        - |  2450 | `				SyToken *pInnerSigStart;` |
|        - |  2451 | `				SyToken *pInnerSigEnd;` |
|        - |  2452 | `				SyToken *pInnerBodyEnd;` |
|        - |  2453 | `				SyString *aInnerShadow;` |
|        - |  2454 | `				sxu32 nInnerShadow;` |
|        - |  2455 | `				sxu32 nInnerParamMax;` |
|        - |  2456 | `				SyToken *p;` |
|        - |  2457 | `				int iNestInner;` |
|       19 |  2458 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|       19 |  2459 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2460 | `					pScan++;` |
|      ! 0 |  2461 | `				}` |
|       19 |  2462 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2463 | `					pScan++;` |
|      ! 0 |  2464 | `					continue;` |
|        - |  2465 | `				}` |
|       19 |  2466 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|       19 |  2467 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|        - |  2468 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|       19 |  2469 | `				if( pInnerSigEnd >= pEnd ){` |
|      ! 0 |  2470 | `					pScan = pEnd;` |
|      ! 0 |  2471 | `					continue;` |
|        - |  2472 | `				}` |
|        - |  2473 | `				/* Build an augmented shadow list: inherited + inner params */` |
|       19 |  2474 | `				nInnerParamMax = 0;` |
|       57 |  2475 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2476 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|       13 |  2477 | `						nInnerParamMax++;` |
|        6 |  2478 | `					}` |
|       20 |  2479 | `				}` |
|       19 |  2480 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       18 |  2481 | `					&pGen->pVm->sAllocator,` |
|       18 |  2482 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|       19 |  2483 | `				if( aInnerShadow == 0 ){` |
|      ! 0 |  2484 | `					return SXERR_ABORT;` |
|        - |  2485 | `				}` |
|       19 |  2486 | `				nInnerShadow = 0;` |
|       25 |  2487 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|        7 |  2488 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|        4 |  2489 | `				}` |
|       57 |  2490 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2491 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       27 |  2492 | `						continue;` |
|        - |  2493 | `					}` |
|       13 |  2494 | `					if( &p[1] >= pInnerSigEnd ){` |
|      ! 0 |  2495 | `						break;` |
|        - |  2496 | `					}` |
|       13 |  2497 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2498 | `						continue;` |
|        - |  2499 | `					}` |
|       13 |  2500 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|        7 |  2501 | `				}` |
|       19 |  2502 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|       19 |  2503 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|      ! 0 |  2504 | `					pScan++;` |
|      ! 0 |  2505 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|      ! 0 |  2506 | `						&& pScan->sData.nByte == 1` |
|      ! 0 |  2507 | `						&& pScan->sData.zString[0] == '?' ){` |
|      ! 0 |  2508 | `						pScan++;` |
|      ! 0 |  2509 | `					}` |
|      ! 0 |  2510 | `					if( pScan < pEnd` |
|      ! 0 |  2511 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  2512 | `						pScan++;` |
|      ! 0 |  2513 | `					}` |
|      ! 0 |  2514 | `				}` |
|       19 |  2515 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|       19 |  2516 | `					pScan++; /* past '=>' */` |
|        9 |  2517 | `				}` |
|       19 |  2518 | `				pInnerBodyEnd = pScan;` |
|       19 |  2519 | `				iNestInner = 0;` |
|      131 |  2520 | `				while( pInnerBodyEnd < pEnd ){` |
|      113 |  2521 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|        - |  2522 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|        - |  2523 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      ! 0 |  2524 | `						break;` |
|        - |  2525 | `					}` |
|      113 |  2526 | `					if( pInnerBodyEnd->nType &` |
|        - |  2527 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        3 |  2528 | `						iNestInner++;` |
|      112 |  2529 | `					}else if( pInnerBodyEnd->nType &` |
|        - |  2530 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        3 |  2531 | `						iNestInner--;` |
|        1 |  2532 | `					}` |
|      113 |  2533 | `					pInnerBodyEnd++;` |
|        1 |  2534 | `				}` |
|        - |  2535 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|        - |  2536 | `				 * the outer's body: a default value is evaluated at call time` |
|        - |  2537 | `				 * in the outer frame, so any free variable it references is` |
|        - |  2538 | `				 * an outer capture. We must NOT scan the parameter-name` |
|        - |  2539 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|        - |  2540 | `				 * or those names leak into the outer's closure environment.` |
|        - |  2541 | `				 *` |
|        - |  2542 | `				 * Walk the signature argument-by-argument, splitting on` |
|        - |  2543 | `				 * top-level commas, and for each argument scan only the token` |
|        - |  2544 | `				 * range after the '=' sign. */` |
|        - |  2545 | `				{` |
|       19 |  2546 | `					SyToken *pArgStart = pInnerSigStart;` |
|       31 |  2547 | `					while( pArgStart < pInnerSigEnd ){` |
|       13 |  2548 | `						SyToken *pArgEnd = pArgStart;` |
|       13 |  2549 | `						SyToken *pEq = 0;` |
|       13 |  2550 | `						int iNestArg = 0;` |
|       49 |  2551 | `						while( pArgEnd < pInnerSigEnd ){` |
|       38 |  2552 | `							if( iNestArg == 0` |
|       39 |  2553 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|        3 |  2554 | `								break;` |
|        - |  2555 | `							}` |
|       37 |  2556 | `							if( pArgEnd->nType &` |
|        - |  2557 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  2558 | `								iNestArg++;` |
|       37 |  2559 | `							}else if( pArgEnd->nType &` |
|        - |  2560 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  2561 | `								iNestArg--;` |
|      ! 0 |  2562 | `							}` |
|       36 |  2563 | `							if( pEq == 0 && iNestArg == 0` |
|       31 |  2564 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|        7 |  2565 | `								pEq = pArgEnd;` |
|        3 |  2566 | `							}` |
|       37 |  2567 | `							pArgEnd++;` |
|        1 |  2568 | `						}` |
|       13 |  2569 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|       10 |  2570 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        3 |  2571 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|        7 |  2572 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  2573 | `								return SXERR_ABORT;` |
|        - |  2574 | `							}` |
|        3 |  2575 | `						}` |
|       13 |  2576 | `						pArgStart = pArgEnd;` |
|       12 |  2577 | `						if( pArgStart < pInnerSigEnd` |
|        8 |  2578 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|        3 |  2579 | `							pArgStart++;` |
|        1 |  2580 | `						}` |
|        1 |  2581 | `					}` |
|        - |  2582 | `				}` |
|       28 |  2583 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        9 |  2584 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|       19 |  2585 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2586 | `					return SXERR_ABORT;` |
|        - |  2587 | `				}` |
|       19 |  2588 | `				pScan = pInnerBodyEnd;` |
|       19 |  2589 | `				continue;` |
|        - |  2590 | `			}` |
|        5 |  2591 | `		}` |
|     1338 |  2592 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     1166 |  2593 | `			pScan++;` |
|     1166 |  2594 | `			continue;` |
|        - |  2595 | `		}` |
|        - |  2596 | `		{` |
|        - |  2597 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      175 |  2598 | `			SyToken *pDollar = pScan;` |
|      258 |  2599 | `			while( &pDollar[1] < pEnd` |
|      175 |  2600 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  2601 | `				pDollar++;` |
|      ! 0 |  2602 | `			}` |
|      175 |  2603 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  2604 | `				break;` |
|        - |  2605 | `			}` |
|      175 |  2606 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2607 | `				pScan = pDollar + 1;` |
|      ! 0 |  2608 | `				continue;` |
|        - |  2609 | `			}` |
|      261 |  2610 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      172 |  2611 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|       86 |  2612 | `				aShadow,nShadow);` |
|      175 |  2613 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2614 | `				return SXERR_ABORT;` |
|        - |  2615 | `			}` |
|      175 |  2616 | `			pScan = pDollar + 2;` |
|        - |  2617 | `		}` |
|        3 |  2618 | `	}` |
|      300 |  2619 | `	return SXRET_OK;` |
|      152 |  2620 | `}` |
|        - |  2621 | `/*` |
|        - |  2622 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  2623 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  2624 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  2625 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  2626 | ` * $this is also made available.` |
|        - |  2627 | ` */` |
|      278 |  2628 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2629 | `{` |
|        - |  2630 | `	ph7_vm_func *pFunc;` |
|        - |  2631 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2632 | `	GenBlock *pBlock;` |
|        - |  2633 | `	SySet *pInstrContainer;` |
|        - |  2634 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|        - |  2635 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|        - |  2636 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|        - |  2637 | `	SyToken *pSavedEnd;` |
|        - |  2638 | `	ph7_vm_func_arg *aArgs;` |
|        - |  2639 | `	char zName[512];` |
|        - |  2640 | `	static int iCnt = 1;` |
|        - |  2641 | `	char *zDup;` |
|        - |  2642 | `	SyToken *pTokKw;` |
|        - |  2643 | `	sxu32 nLen;` |
|        - |  2644 | `	sxu32 nLine;` |
|      283 |  2645 | `	sxi32 iFlags = 0;` |
|      283 |  2646 | `	int bStatic = 0;` |
|        - |  2647 | `	sxi32 rc;` |
|        - |  2648 | `	sxu32 n;` |
|      139 |  2649 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2650 |  |
|      283 |  2651 | `	nLine = pGen->pIn->nLine;` |
|        - |  2652 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|      283 |  2653 | `	pTokKw = pGen->pIn;` |
|        - |  2654 | `	/* Optional 'static' prefix */` |
|      278 |  2655 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      283 |  2656 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        7 |  2657 | `		bStatic = 1;` |
|        7 |  2658 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        7 |  2659 | `		pGen->pIn++;` |
|        3 |  2660 | `	}` |
|        - |  2661 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      278 |  2662 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      283 |  2663 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  2664 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2665 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  2666 | `		return SXERR_SYNTAX;` |
|        - |  2667 | `	}` |
|      283 |  2668 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  2669 | `	/* Optional '&' — return by reference */` |
|      283 |  2670 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2671 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  2672 | `		pGen->pIn++;` |
|      ! 0 |  2673 | `	}` |
|        - |  2674 | `	/* Expect '(' */` |
|      283 |  2675 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  2676 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2677 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2678 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|        2 |  2679 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2680 | `		}else{` |
|      ! 0 |  2681 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2682 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|        - |  2683 | `		}` |
|        3 |  2684 | `		return SXERR_SYNTAX;` |
|        - |  2685 | `	}` |
|      281 |  2686 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  2687 | `	/* Delimit the parameter list */` |
|      281 |  2688 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      281 |  2689 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  2690 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2691 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  2692 | `		return SXERR_SYNTAX;` |
|        - |  2693 | `	}` |
|        - |  2694 | `	/* Allocate the function state */` |
|      279 |  2695 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      279 |  2696 | `	if( pFunc == 0 ){` |
|      ! 0 |  2697 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2698 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2699 | `		return SXERR_ABORT;` |
|        - |  2700 | `	}` |
|        - |  2701 | `	/* Generate a unique lambda name */` |
|      279 |  2702 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      279 |  2703 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2704 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2705 | `	}` |
|      279 |  2706 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      279 |  2707 | `	if( zDup == 0 ){` |
|      ! 0 |  2708 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2709 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2710 | `		return SXERR_ABORT;` |
|        - |  2711 | `	}` |
|      279 |  2712 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  2713 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|      279 |  2714 | `	pFunc->nLine = nLine;` |
|        - |  2715 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|      279 |  2716 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  2717 | `		return SXERR_ABORT;` |
|        - |  2718 | `	}` |
|        - |  2719 | `	/* Collect function arguments */` |
|      279 |  2720 | `	if( pGen->pIn < pSigEnd ){` |
|      110 |  2721 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      110 |  2722 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2723 | `			return SXERR_ABORT;` |
|        - |  2724 | `		}` |
|       53 |  2725 | `	}` |
|        - |  2726 | `	/* Point past ')' and parse optional return type */` |
|      279 |  2727 | `	pGen->pIn = &pSigEnd[1];` |
|      279 |  2728 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      279 |  2729 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2730 | `		return SXERR_ABORT;` |
|      279 |  2731 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  2732 | `		return SXERR_SYNTAX;` |
|        - |  2733 | `	}` |
|        - |  2734 | `	/* Expect '=>' */` |
|      279 |  2735 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  2736 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2737 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2738 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|        2 |  2739 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2740 | `		}else{` |
|      ! 0 |  2741 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2742 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|        - |  2743 | `		}` |
|        3 |  2744 | `		return SXERR_SYNTAX;` |
|        - |  2745 | `	}` |
|      276 |  2746 | `	pGen->pIn++; /* Jump '=>' */` |
|      276 |  2747 | `	pBodyStart = pGen->pIn;` |
|      276 |  2748 | `	pBodyEnd = pGen->pEnd;` |
|        - |  2749 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  2750 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  2751 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  2752 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      276 |  2753 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  2754 | `	{` |
|      276 |  2755 | `		SyString *aShadow = 0;` |
|      276 |  2756 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      276 |  2757 | `		if( nShadow > 0 ){` |
|      107 |  2758 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      104 |  2759 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      107 |  2760 | `			if( aShadow == 0 ){` |
|      ! 0 |  2761 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2762 | `					"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2763 | `				return SXERR_ABORT;` |
|        - |  2764 | `			}` |
|      239 |  2765 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      135 |  2766 | `				aShadow[n] = aArgs[n].sName;` |
|       69 |  2767 | `			}` |
|       52 |  2768 | `		}` |
|      412 |  2769 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      136 |  2770 | `			aShadow,nShadow);` |
|      276 |  2771 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2772 | `			return SXERR_ABORT;` |
|        - |  2773 | `		}` |
|        - |  2774 | `	}` |
|        - |  2775 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  2776 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  2777 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  2778 | `	 * $this. */` |
|      276 |  2779 | `	if( !bStatic ){` |
|        - |  2780 | `		char *zThisDup;` |
|      270 |  2781 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      270 |  2782 | `		if( zThisDup == 0 ){` |
|      ! 0 |  2783 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2784 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2785 | `			return SXERR_ABORT;` |
|        - |  2786 | `		}` |
|      270 |  2787 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      270 |  2788 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      270 |  2789 | `		sEnv.nIdx = SXU32_HIGH;` |
|      270 |  2790 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      270 |  2791 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      270 |  2792 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      133 |  2793 | `	}` |
|        - |  2794 | `	/* Arrow functions are always closures */` |
|      276 |  2795 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|        - |  2796 | `	/* Compile the body expression as an implicit return */` |
|      412 |  2797 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      136 |  2798 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      276 |  2799 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2800 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2801 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  2802 | `		return SXERR_ABORT;` |
|        - |  2803 | `	}` |
|      276 |  2804 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      276 |  2805 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      276 |  2806 | `	pSavedEnd = pGen->pEnd;` |
|      276 |  2807 | `	pGen->pIn = pBodyStart;` |
|      276 |  2808 | `	pGen->pEnd = pBodyEnd;` |
|      276 |  2809 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      276 |  2810 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2811 | `		return SXERR_ABORT;` |
|        - |  2812 | `	}` |
|        - |  2813 | `	/* The cursor stopped just past the body expression */` |
|      276 |  2814 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  2815 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  2816 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  2817 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  2818 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      276 |  2819 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      276 |  2820 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      276 |  2821 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      276 |  2822 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      276 |  2823 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  2824 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      276 |  2825 | `	pGen->pIn = pBodyEnd;` |
|      276 |  2826 | `	pGen->pEnd = pSavedEnd;` |
|        - |  2827 | `	/* Emit the load-closure instruction */` |
|      276 |  2828 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      276 |  2829 | `	return SXRET_OK;` |
|      144 |  2830 | `}` |
|        - |  2831 | `/*` |
|        - |  2832 | ` * Compile a single arm's expression range into a freshly-allocated` |
|        - |  2833 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|        - |  2834 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|        - |  2835 | ` * expression's value.` |
|        - |  2836 | ` */` |
|      354 |  2837 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|        - |  2838 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|        3 |  2839 | `{` |
|        - |  2840 | `	SySet *pInstrContainer;` |
|        - |  2841 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  2842 | `	GenBlock *pArmBlock;` |
|        - |  2843 | `	sxi32 rc;` |
|      357 |  2844 | `	pTmpIn  = pGen->pIn;` |
|      357 |  2845 | `	pTmpEnd = pGen->pEnd;` |
|      357 |  2846 | `	pGen->pIn  = pStart;` |
|      357 |  2847 | `	pGen->pEnd = pStop;` |
|      357 |  2848 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      357 |  2849 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|        - |  2850 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|        - |  2851 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|        - |  2852 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|        - |  2853 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|        - |  2854 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|      534 |  2855 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      177 |  2856 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|      357 |  2857 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2858 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  2859 | `		pGen->pIn  = pTmpIn;` |
|      ! 0 |  2860 | `		pGen->pEnd = pTmpEnd;` |
|      ! 0 |  2861 | `		return SXERR_ABORT;` |
|        - |  2862 | `	}` |
|      357 |  2863 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      357 |  2864 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      357 |  2865 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      357 |  2866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      357 |  2867 | `	GenStateLeaveBlock(&(*pGen),0);` |
|      357 |  2868 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      357 |  2869 | `	pGen->pIn  = pTmpIn;` |
|      357 |  2870 | `	pGen->pEnd = pTmpEnd;` |
|      357 |  2871 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2872 | `		return SXERR_ABORT;` |
|        - |  2873 | `	}` |
|      357 |  2874 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 |  2875 | `		return SXERR_EMPTY;` |
|        - |  2876 | `	}` |
|      357 |  2877 | `	return SXRET_OK;` |
|      180 |  2878 | `}` |
|        - |  2879 | `/*` |
|        - |  2880 | ` * Compile a PHP 8.0 match expression:` |
|        - |  2881 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|        - |  2882 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|        - |  2883 | ` * Strict comparison (===) is used between the subject and each condition.` |
|        - |  2884 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|        - |  2885 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|        - |  2886 | ` */` |
|        - |  2887 | `/*` |
|        - |  2888 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|        - |  2889 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|        - |  2890 | ` * caller can bail out of the current expression.` |
|        - |  2891 | ` */` |
|        2 |  2892 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|        1 |  2893 | `{` |
|        - |  2894 | `	va_list ap;` |
|        - |  2895 | `	sxi32 rc;` |
|        - |  2896 | `	SyBlob sMsg;` |
|        3 |  2897 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        3 |  2898 | `	va_start(ap,zFmt);` |
|        3 |  2899 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|        3 |  2900 | `	va_end(ap);` |
|        3 |  2901 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|        3 |  2902 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|        3 |  2903 | `	SyBlobRelease(&sMsg);` |
|        3 |  2904 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2905 | `		return SXERR_ABORT;` |
|        - |  2906 | `	}` |
|        3 |  2907 | `	return SXERR_SYNTAX;` |
|        2 |  2908 | `}` |
|        - |  2909 | `/*` |
|        - |  2910 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|        - |  2911 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|        - |  2912 | ` * Returns the stop token pointer (or pEnd if none found).` |
|        - |  2913 | ` */` |
|      356 |  2914 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|        4 |  2915 | `{` |
|      360 |  2916 | `	SyToken *pCur = pStart;` |
|      360 |  2917 | `	int iNest = 0;` |
|      838 |  2918 | `	while( pCur < pEnd ){` |
|      802 |  2919 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       13 |  2920 | `			iNest++;` |
|      796 |  2921 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       13 |  2922 | `			iNest--;` |
|      784 |  2923 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|      323 |  2924 | `			return pCur;` |
|        - |  2925 | `		}` |
|      482 |  2926 | `		pCur++;` |
|        4 |  2927 | `	}` |
|       39 |  2928 | `	return pEnd;` |
|      182 |  2929 | `}` |
|       72 |  2930 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2931 | `{` |
|        - |  2932 | `	ph7_match *pMatch;` |
|        - |  2933 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|       77 |  2934 | `	int bHasDefault = 0;` |
|        - |  2935 | `	sxu32 nLine;` |
|        - |  2936 | `	sxi32 rc;` |
|       36 |  2937 | `	SXUNUSED(iCompileFlag);` |
|       77 |  2938 | `	nLine = pGen->pIn->nLine;` |
|       77 |  2939 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|        - |  2940 | `	/* Expect '(' */` |
|       77 |  2941 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2942 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2943 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|      ! 0 |  2944 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|        - |  2945 | `	}` |
|       77 |  2946 | `	pGen->pIn++; /* Jump '(' */` |
|       77 |  2947 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|       77 |  2948 | `	if( pSubjEnd >= pGen->pEnd ){` |
|      ! 0 |  2949 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2950 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        - |  2951 | `	}` |
|       77 |  2952 | `	if( pGen->pIn >= pSubjEnd ){` |
|      ! 0 |  2953 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2954 | `			"syntax error, unexpected \")\", expecting match subject");` |
|        - |  2955 | `	}` |
|        - |  2956 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|       77 |  2957 | `	pSavedEnd = pGen->pEnd;` |
|       77 |  2958 | `	pGen->pEnd = pSubjEnd;` |
|       77 |  2959 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       77 |  2960 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2961 | `		return SXERR_ABORT;` |
|        - |  2962 | `	}` |
|       77 |  2963 | `	pGen->pEnd = pSavedEnd;` |
|       77 |  2964 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|        - |  2965 | `	/* Expect '{' */` |
|       77 |  2966 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 |  2967 | `		return GenStateMatchError(pGen,` |
|      ! 0 |  2968 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  2969 | `			"syntax error, expecting \"{\" after match subject");` |
|        - |  2970 | `	}` |
|       77 |  2971 | `	pGen->pIn++; /* Jump '{' */` |
|       77 |  2972 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|       77 |  2973 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  2974 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2975 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|        - |  2976 | `	}` |
|        - |  2977 | `	/* Allocate ph7_match container */` |
|       77 |  2978 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|       77 |  2979 | `	if( pMatch == 0 ){` |
|      ! 0 |  2980 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2981 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2982 | `		return SXERR_ABORT;` |
|        - |  2983 | `	}` |
|       77 |  2984 | `	SyZero(pMatch,sizeof(ph7_match));` |
|       77 |  2985 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|        - |  2986 | `	/* Iterate arms */` |
|      259 |  2987 | `	while( pGen->pIn < pBodyEnd ){` |
|        - |  2988 | `		ph7_match_arm sArm;` |
|        - |  2989 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|      190 |  2990 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|      190 |  2991 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|      190 |  2992 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|      190 |  2993 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  2994 | `		/* 'default' arm? */` |
|      186 |  2995 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      107 |  2996 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|       22 |  2997 | `			if( bHasDefault ){` |
|        3 |  2998 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|        - |  2999 | `					"Match expressions may only contain one default arm");` |
|        4 |  3000 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  3001 | `			}` |
|       20 |  3002 | `			sArm.bDefault = 1;` |
|       20 |  3003 | `			bHasDefault = 1;` |
|       20 |  3004 | `			pGen->pIn++;` |
|       20 |  3005 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|      ! 0 |  3006 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3007 | `					"syntax error, expecting \"=>\" after 'default'");` |
|        - |  3008 | `			}` |
|       20 |  3009 | `			pGen->pIn++; /* Jump '=>' */` |
|       11 |  3010 | `		}else{` |
|        - |  3011 | `			/* Condition list: cond (',' cond)* '=>' */` |
|      170 |  3012 | `			pCondStart = pGen->pIn;` |
|      170 |  3013 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|        - |  3014 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|      178 |  3015 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|        - |  3016 | `				SySet sCondBc;` |
|        9 |  3017 | `				if( pCondStart >= pArrow ){` |
|      ! 0 |  3018 | `					return GenStateMatchError(pGen,nArmLine,` |
|        - |  3019 | `						"syntax error, empty match condition expression");` |
|        - |  3020 | `				}` |
|        9 |  3021 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        9 |  3022 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|        9 |  3023 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3024 | `					return SXERR_ABORT;` |
|        - |  3025 | `				}` |
|        9 |  3026 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        9 |  3027 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|        9 |  3028 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|        - |  3029 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|        1 |  3030 | `			}` |
|      170 |  3031 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  3032 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3033 | `					"syntax error, expecting \"=>\" in match arm");` |
|        - |  3034 | `			}` |
|      167 |  3035 | `			if( pCondStart >= pArrow ){` |
|      ! 0 |  3036 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3037 | `					"syntax error, empty match condition expression");` |
|        - |  3038 | `			}` |
|        - |  3039 | `			{` |
|        - |  3040 | `				SySet sCondBc;` |
|      167 |  3041 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      167 |  3042 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      167 |  3043 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3044 | `					return SXERR_ABORT;` |
|        - |  3045 | `				}` |
|      167 |  3046 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        - |  3047 | `			}` |
|      167 |  3048 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|        - |  3049 | `		}` |
|        - |  3050 | `		/* Compile result expression: up to top-level ',' or body end */` |
|      185 |  3051 | `		pResStart = pGen->pIn;` |
|      185 |  3052 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|      185 |  3053 | `		if( pResStart >= pResEnd ){` |
|      ! 0 |  3054 | `			return GenStateMatchError(pGen,nArmLine,` |
|        - |  3055 | `				"syntax error, expected expression after \"=>\"");` |
|        - |  3056 | `		}` |
|      185 |  3057 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|      185 |  3058 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3059 | `			return SXERR_ABORT;` |
|        - |  3060 | `		}` |
|      185 |  3061 | `		pGen->pIn = pResEnd;` |
|      185 |  3062 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      151 |  3063 | `			pGen->pIn++; /* Skip trailing ',' */` |
|       74 |  3064 | `		}` |
|      185 |  3065 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|        3 |  3066 | `	}` |
|       71 |  3067 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|       71 |  3068 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|       71 |  3069 | `	return SXRET_OK;` |
|       41 |  3070 | `}` |
|        - |  3071 | `/*` |
|        - |  3072 | ` * Compile a backtick quoted string.` |
|        - |  3073 | ` */` |
|        4 |  3074 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 |  3075 | `{` |
|        - |  3076 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|        - |  3077 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|        - |  3078 | `	 */` |
|        8 |  3079 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|        - |  3080 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|        2 |  3081 | `		ph7_lib_version()` |
|        - |  3082 | `		);` |
|        - |  3083 | `	/* Load NULL */` |
|        6 |  3084 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3085 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  3086 | `	/* Node successfully compiled */` |
|        6 |  3087 | `	return SXRET_OK;` |
|        2 |  3088 | `}` |
|        - |  3089 | `/*` |
|        - |  3090 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|        - |  3091 | ` * construct.` |
|        - |  3092 | ` */` |
|       82 |  3093 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3094 | `{` |
|        - |  3095 | `	SyString *pName;` |
|        - |  3096 | `	sxu32 nKeyID;` |
|        - |  3097 | `	sxi32 rc;` |
|        - |  3098 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|       87 |  3099 | `	pName = &pGen->pIn->sData;` |
|       87 |  3100 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       87 |  3101 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|       87 |  3102 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|        9 |  3103 | `		SyToken *pTmp,*pNext = 0;` |
|        - |  3104 | `		/* Compile arguments one after one */` |
|        9 |  3105 | `		pTmp = pGen->pEnd;` |
|        - |  3106 | `		/* Symisc eXtension to the PHP programming language:` |
|        - |  3107 | `		 * 'echo' can be used in the context of a function which` |
|        - |  3108 | `		 *  mean that the following expression is valid:` |
|        - |  3109 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|        - |  3110 | `		 */` |
|        9 |  3111 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|       17 |  3112 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        9 |  3113 | `			if( pGen->pIn < pNext ){` |
|        9 |  3114 | `				pGen->pEnd = pNext;` |
|        9 |  3115 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|        9 |  3116 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3117 | `					return SXERR_ABORT;` |
|        - |  3118 | `				}` |
|        9 |  3119 | `				if( rc != SXERR_EMPTY ){` |
|        - |  3120 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|        - |  3121 | `					 * without the overhead of a function call.` |
|        - |  3122 | `					 * This is a very powerful optimization that improve` |
|        - |  3123 | `					 * performance greatly.` |
|        - |  3124 | `					 */` |
|        9 |  3125 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|        4 |  3126 | `				}` |
|        4 |  3127 | `			}` |
|        - |  3128 | `			/* Jump trailing commas */` |
|        9 |  3129 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      ! 0 |  3130 | `				pNext++;` |
|      ! 0 |  3131 | `			}` |
|        9 |  3132 | `			pGen->pIn = pNext;` |
|        1 |  3133 | `		}` |
|        - |  3134 | `		/* Restore token stream */` |
|        9 |  3135 | `		pGen->pEnd = pTmp;` |
|        5 |  3136 | `	}else{` |
|       79 |  3137 | `		sxi32 nArg = 0;` |
|       79 |  3138 | `		sxu32 nIdx = 0;` |
|       79 |  3139 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       79 |  3140 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3141 | `			return SXERR_ABORT;` |
|       79 |  3142 | `		}else if(rc != SXERR_EMPTY ){` |
|       79 |  3143 | `			nArg = 1;` |
|       37 |  3144 | `		}` |
|       79 |  3145 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|        - |  3146 | `			ph7_value *pObj;` |
|        - |  3147 | `			/* Emit the call instruction */` |
|       31 |  3148 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       31 |  3149 | `			if( pObj == 0 ){` |
|      ! 0 |  3150 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3151 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3152 | `				return SXERR_ABORT;` |
|        - |  3153 | `			}` |
|       31 |  3154 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|        - |  3155 | `			/* Install in the literal table */` |
|       31 |  3156 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       13 |  3157 | `		}` |
|        - |  3158 | `		/* Emit the call instruction */` |
|       79 |  3159 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       79 |  3160 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        - |  3161 | `	}` |
|        - |  3162 | `	/* Node successfully compiled */` |
|       87 |  3163 | `	return SXRET_OK;` |
|       46 |  3164 | `}` |
|        - |  3165 | `/*` |
|        - |  3166 | ` * Compile a node holding a variable declaration.` |
|        - |  3167 | ` * According to the PHP language reference` |
|        - |  3168 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|        - |  3169 | ` *  The variable name is case-sensitive.` |
|        - |  3170 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|        - |  3171 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3172 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|        - |  3173 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|        - |  3174 | ` *  Note: $this is a special variable that can't be assigned.` |
|        - |  3175 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|        - |  3176 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|        - |  3177 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|        - |  3178 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|        - |  3179 | ` *  the chapter on Expressions.` |
|        - |  3180 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|        - |  3181 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|        - |  3182 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|        - |  3183 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|        - |  3184 | ` *  is being assigned (the source variable).` |
|        - |  3185 | ` */` |
|  9033252 |  3186 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3187 | `{` |
|  9033257 |  3188 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3189 | `	sxi32 iVv;` |
|        - |  3190 | `	sxi32 iP1;` |
|        - |  3191 | `	void *p3;` |
|        - |  3192 | `	sxi32 rc;` |
|  9033257 |  3193 | `	iVv = -1; /* Variable variable counter */` |
| 18066521 |  3194 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  9033269 |  3195 | `		pGen->pIn++;` |
|  9033269 |  3196 | `		iVv++;` |
|        5 |  3197 | `	}` |
|  9033257 |  3198 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - |  3199 | `		/* Invalid variable name */` |
|      ! 0 |  3200 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|      ! 0 |  3201 | `		if( rc == SXERR_ABORT ){` |
|        - |  3202 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3203 | `			return SXERR_ABORT;` |
|        - |  3204 | `		}` |
|      ! 0 |  3205 | `		return SXRET_OK;` |
|        - |  3206 | `	}` |
|  9033257 |  3207 | `	p3  = 0;` |
|  9033257 |  3208 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|        - |  3209 | `		/* Dynamic variable creation */` |
|       21 |  3210 | `		pGen->pIn++;  /* Jump the open curly */` |
|       21 |  3211 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|       21 |  3212 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3213 | `			/* Empty expression */` |
|        3 |  3214 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|        3 |  3215 | `			return SXRET_OK;` |
|        - |  3216 | `		}` |
|        - |  3217 | `		/* Compile the expression holding the variable name */` |
|       18 |  3218 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       18 |  3219 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3220 | `			return SXERR_ABORT;` |
|       18 |  3221 | `		}else if( rc == SXERR_EMPTY ){` |
|        3 |  3222 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|        3 |  3223 | `			return SXRET_OK;` |
|        - |  3224 | `		}` |
|        8 |  3225 | `	}else{` |
|        - |  3226 | `		SyHashEntry *pEntry;` |
|        - |  3227 | `		SyString *pName;` |
|  9033239 |  3228 | `		char *zName = 0;` |
|        - |  3229 | `		/* Extract variable name */` |
|  9033239 |  3230 | `		pName = &pGen->pIn->sData;` |
|        - |  3231 | `		/* Advance the stream cursor */` |
|  9033239 |  3232 | `		pGen->pIn++;` |
|  9033239 |  3233 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  9033239 |  3234 | `		if( pEntry == 0 ){` |
|        - |  3235 | `			/* Duplicate name */` |
|   570687 |  3236 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   570687 |  3237 | `			if( zName == 0 ){` |
|      ! 0 |  3238 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3239 | `				return SXERR_ABORT;` |
|        - |  3240 | `			}` |
|        - |  3241 | `			/* Install in the hashtable */` |
|   570687 |  3242 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   285346 |  3243 | `		}else{` |
|        - |  3244 | `			/* Name already available */` |
|  8462557 |  3245 | `			zName = (char *)pEntry->pUserData;` |
|        - |  3246 | `		}` |
|  9033239 |  3247 | `		p3 = (void *)zName;` |
|        - |  3248 | `	}` |
|  9033253 |  3249 | `	iP1 = 0;` |
|  9033253 |  3250 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  2733841 |  3251 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - |  3252 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  2733811 |  3253 | `			iP1 = 1;` |
|  1366903 |  3254 | `		}` |
|  1366918 |  3255 | `	}` |
|        - |  3256 | `	/* Emit the load instruction */` |
|  9033253 |  3257 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  9033265 |  3258 | `	while( iVv > 0 ){` |
|       13 |  3259 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|       13 |  3260 | `		iVv--;` |
|        1 |  3261 | `	}` |
|        - |  3262 | `	/* Node successfully compiled */` |
|  9033253 |  3263 | `	return SXRET_OK;` |
|  4516631 |  3264 | `}` |
|        - |  3265 | `/*` |
|        - |  3266 | ` * Load a literal.` |
|        - |  3267 | ` */` |
|  5702458 |  3268 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 |  3269 | `{` |
|  5702463 |  3270 | `	SyToken *pToken = pGen->pIn;` |
|        - |  3271 | `	ph7_value *pObj;` |
|        - |  3272 | `	SyString *pStr;` |
|        - |  3273 | `	sxu32 nIdx;` |
|        - |  3274 | `	/* Extract token value */` |
|  5702463 |  3275 | `	pStr = &pToken->sData;` |
|        - |  3276 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  5702463 |  3277 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  1370833 |  3278 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - |  3279 | `			/* NULL constant are always indexed at 0 */` |
|   560215 |  3280 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   560215 |  3281 | `			return SXRET_OK;` |
|   810623 |  3282 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - |  3283 | `			/* TRUE constant are always indexed at 1 */` |
|   148581 |  3284 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   148581 |  3285 | `			return SXRET_OK;` |
|        5 |  3286 | `		}` |
|  5157818 |  3287 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   990324 |  3288 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - |  3289 | `			/* FALSE constant are always indexed at 2 */` |
|   396811 |  3290 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   396811 |  3291 | `			return SXRET_OK;` |
|  4230821 |  3292 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   591984 |  3293 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - |  3294 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    11663 |  3295 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    11663 |  3296 | `			if( pObj == 0 ){` |
|      ! 0 |  3297 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3298 | `				return SXERR_ABORT;` |
|        - |  3299 | `			}` |
|    11663 |  3300 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - |  3301 | `			/* Emit the load constant instruction */` |
|    11663 |  3302 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    11663 |  3303 | `			return SXRET_OK;` |
|  3952599 |  3304 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    58856 |  3305 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|        - |  3306 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|        8 |  3307 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        8 |  3308 | `			if( pObj == 0 ){` |
|      ! 0 |  3309 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3310 | `				return SXERR_ABORT;` |
|        - |  3311 | `			}` |
|        8 |  3312 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - |  3313 | `				SyString sNs;` |
|        8 |  3314 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  3315 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|        5 |  3316 | `			}else{` |
|      ! 0 |  3317 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  3318 | `			}` |
|        8 |  3319 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        8 |  3320 | `			return SXRET_OK;` |
|  3945050 |  3321 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   152035 |  3322 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  4031403 |  3323 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   216500 |  3324 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|       11 |  3325 | `			GenBlock *pBlock = pGen->pCurrent;` |
|        - |  3326 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       21 |  3327 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|        - |  3328 | `				/* Point to the upper block */` |
|       11 |  3329 | `				pBlock = pBlock->pParent;` |
|        1 |  3330 | `			}` |
|       11 |  3331 | `			if( pBlock == 0 ){` |
|        - |  3332 | `				/* Called in the global scope,load NULL */` |
|        5 |  3333 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        3 |  3334 | `			}else{` |
|        - |  3335 | `				/* Extract the target function/method */` |
|        7 |  3336 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        7 |  3337 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        - |  3338 | `					/* Not a class method,Load null */` |
|        3 |  3339 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3340 | `				}else{` |
|        5 |  3341 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        5 |  3342 | `					if( pObj == 0 ){` |
|      ! 0 |  3343 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3344 | `						return SXERR_ABORT;` |
|        - |  3345 | `					}` |
|        5 |  3346 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|        - |  3347 | `					/* Emit the load constant instruction */` |
|        5 |  3348 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  3349 | `				}` |
|        - |  3350 | `			}` |
|       11 |  3351 | `			return SXRET_OK;` |
|        - |  3352 | `	}` |
|        - |  3353 | `	/* Query literal table */` |
|  4585197 |  3354 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|        - |  3355 | `		ph7_value *pLitObj;` |
|        - |  3356 | `		/* Unknown literal,install it in the literal table */` |
|   927763 |  3357 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   927763 |  3358 | `		if( pLitObj == 0 ){` |
|      ! 0 |  3359 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3360 | `			return SXERR_ABORT;` |
|        - |  3361 | `		}` |
|   927763 |  3362 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   927763 |  3363 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   463879 |  3364 | `	}` |
|        - |  3365 | `	/* Emit the load constant instruction */` |
|  4585197 |  3366 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  4585197 |  3367 | `	return SXRET_OK;` |
|  2851234 |  3368 | `}` |
|        - |  3369 | `/*` |
|        - |  3370 | ` * Resolve a namespace path or simply load a literal.` |
|        - |  3371 | ` * If the token stream contains namespace separators (backslashes),` |
|        - |  3372 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - |  3373 | ` * Otherwise, load the simple literal directly.` |
|        - |  3374 | ` */` |
|  5706390 |  3375 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 |  3376 | `{` |
|        - |  3377 | `	sxi32 rc;` |
|  5706395 |  3378 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  3379 | `		return SXRET_OK;` |
|        - |  3380 | `	}` |
|        - |  3381 | `	/* Check if this is a multi-token namespace path */` |
|  5706395 |  3382 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - |  3383 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     3937 |  3384 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     3937 |  3385 | `		int isAbsolute = 0;` |
|     3937 |  3386 | `		SyBlobReset(pWorker);` |
|        - |  3387 | `		/* Check for leading backslash (absolute path) */` |
|     3937 |  3388 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     3935 |  3389 | `			isAbsolute = 1;` |
|     3935 |  3390 | `			pGen->pIn++; /* Skip leading backslash */` |
|     1965 |  3391 | `		}` |
|        - |  3392 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|     3937 |  3393 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        3 |  3394 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 |  3395 | `			SyBlobAppend(pWorker,"\\",1);` |
|        1 |  3396 | `		}` |
|        - |  3397 | `		/* Collect all path components */` |
|     4045 |  3398 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4045 |  3399 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       58 |  3400 | `				SyBlobAppend(pWorker,"\\",1);` |
|       31 |  3401 | `			}else{` |
|     3991 |  3402 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  3403 | `			}` |
|     4045 |  3404 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     3937 |  3405 | `				pGen->pIn++;` |
|     3937 |  3406 | `				break;` |
|        - |  3407 | `			}` |
|      112 |  3408 | `			pGen->pIn++;` |
|        4 |  3409 | `		}` |
|     3937 |  3410 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - |  3411 | `			ph7_value *pObj;` |
|        - |  3412 | `			SyString sPath;` |
|        - |  3413 | `			sxu32 nIdx;` |
|     3937 |  3414 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - |  3415 | `			/* Install in the literal table */` |
|     3937 |  3416 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     3909 |  3417 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3909 |  3418 | `				if( pObj == 0 ){` |
|      ! 0 |  3419 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3420 | `					return SXERR_ABORT;` |
|        - |  3421 | `				}` |
|     3909 |  3422 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     3909 |  3423 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1952 |  3424 | `			}` |
|        - |  3425 | `			/* Emit the load constant instruction.` |
|        - |  3426 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - |  3427 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     5903 |  3428 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     1966 |  3429 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     1966 |  3430 | `				nIdx,0,0);` |
|     3937 |  3431 | `			return SXRET_OK;` |
|        - |  3432 | `		}` |
|      ! 0 |  3433 | `	}` |
|        - |  3434 | `	/* Single-token literal: load directly */` |
|  5702463 |  3435 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  5702463 |  3436 | `	return rc;` |
|  2853200 |  3437 | `}` |
|        - |  3438 | `/*` |
|        - |  3439 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|        - |  3440 | ` */` |
|        - |  3441 | `/*` |
|        - |  3442 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|        - |  3443 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|        - |  3444 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|        - |  3445 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|        - |  3446 | ` */` |
|      ! 0 |  3447 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|      ! 0 |  3448 | `{` |
|      ! 0 |  3449 | `	SXUNUSED(iCompileFlag);` |
|      ! 0 |  3450 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - |  3451 | `		"Cannot use the first-class callable syntax '...' here");` |
|      ! 0 |  3452 | `	return SXERR_SYNTAX;` |
|      ! 0 |  3453 | `}` |
|  5706390 |  3454 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3455 | `{` |
|        - |  3456 | `	sxi32 rc;` |
|  5706395 |  3457 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  5706395 |  3458 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3459 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3460 | `		return rc;` |
|        - |  3461 | `	}` |
|        - |  3462 | `	/* Node successfully compiled */` |
|  5706395 |  3463 | `	return SXRET_OK;` |
|  2853200 |  3464 | `}` |
|        - |  3465 | `/*` |
|        - |  3466 | ` * Recover from a compile-time error. In other words synchronize` |
|        - |  3467 | ` * the token stream cursor with the first semi-colon seen.` |
|        - |  3468 | ` */` |
|        8 |  3469 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|        1 |  3470 | `{` |
|        - |  3471 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       17 |  3472 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|        9 |  3473 | `		pGen->pIn++;` |
|        1 |  3474 | `	}` |
|        9 |  3475 | `	return SXRET_OK;` |
|        1 |  3476 | `}` |
|        - |  3477 | `/*` |
|        - |  3478 | ` * Check if the given identifier name is reserved or not.` |
|        - |  3479 | ` * Return TRUE if reserved.FALSE otherwise.` |
|        - |  3480 | ` */` |
|   143928 |  3481 | `static int GenStateIsReservedConstant(SyString *pName)` |
|        5 |  3482 | `{` |
|   143933 |  3483 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|       48 |  3484 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|        3 |  3485 | `			return TRUE;` |
|       46 |  3486 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|        6 |  3487 | `			return TRUE;` |
|        3 |  3488 | `		}` |
|   143908 |  3489 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       22 |  3490 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|        3 |  3491 | `			return TRUE;` |
|        - |  3492 | `		}` |
|        9 |  3493 | `	}` |
|        - |  3494 | `	/* Not a reserved constant */` |
|   143925 |  3495 | `	return FALSE;` |
|    71969 |  3496 | `}` |
|        - |  3497 | `/*` |
|        - |  3498 | ` * Compile the 'const' statement.` |
|        - |  3499 | ` * According to the PHP language reference` |
|        - |  3500 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|        - |  3501 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|        - |  3502 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|        - |  3503 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|        - |  3504 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3505 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|        - |  3506 | ` *  Syntax` |
|        - |  3507 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|        - |  3508 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|        - |  3509 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|        - |  3510 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|        - |  3511 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|        - |  3512 | ` *  to get a list of all defined constants.` |
|        - |  3513 | ` *` |
|        - |  3514 | ` * Symisc eXtension.` |
|        - |  3515 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|        - |  3516 | ` *  would allow only simple scalar value.` |
|        - |  3517 | ` *  Example` |
|        - |  3518 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  3519 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  3520 | ` */` |
|       48 |  3521 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |  3522 | `{` |
|        - |  3523 | `	SySet *pConsCode,*pInstrContainer;` |
|       53 |  3524 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3525 | `	SyString *pName;` |
|        - |  3526 | `	sxi32 rc;` |
|       53 |  3527 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       53 |  3528 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  3529 | `		/* Invalid constant name */` |
|        8 |  3530 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|        8 |  3531 | `		if( rc == SXERR_ABORT ){` |
|        - |  3532 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3533 | `			return SXERR_ABORT;` |
|        - |  3534 | `		}` |
|        8 |  3535 | `		goto Synchronize;` |
|        - |  3536 | `	}` |
|        - |  3537 | `	/* Peek constant name */` |
|       47 |  3538 | `	pName = &pGen->pIn->sData;` |
|        - |  3539 | `	/* Make sure the constant name isn't reserved */` |
|       47 |  3540 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  3541 | `		/* Reserved constant */` |
|       10 |  3542 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       10 |  3543 | `		if( rc == SXERR_ABORT ){` |
|        - |  3544 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3545 | `			return SXERR_ABORT;` |
|        - |  3546 | `		}` |
|       10 |  3547 | `		goto Synchronize;` |
|        - |  3548 | `	}` |
|       38 |  3549 | `	pGen->pIn++;` |
|       38 |  3550 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  3551 | `		/* Invalid statement*/` |
|        6 |  3552 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|        6 |  3553 | `		if( rc == SXERR_ABORT ){` |
|        - |  3554 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3555 | `			return SXERR_ABORT;` |
|        - |  3556 | `		}` |
|        6 |  3557 | `		goto Synchronize;` |
|        - |  3558 | `	}` |
|       32 |  3559 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |  3560 | `	/* Allocate a new constant value container */` |
|       32 |  3561 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       32 |  3562 | `	if( pConsCode == 0 ){` |
|      ! 0 |  3563 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3564 | `		return SXERR_ABORT;` |
|        - |  3565 | `	}` |
|       32 |  3566 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  3567 | `	/* Swap bytecode container */` |
|       32 |  3568 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       32 |  3569 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |  3570 | `	/* Compile constant value */` |
|       32 |  3571 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  3572 | `	/* Emit the done instruction */` |
|       32 |  3573 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       32 |  3574 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       32 |  3575 | `	if( rc == SXERR_ABORT ){` |
|        - |  3576 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  3577 | `		return SXERR_ABORT;` |
|        - |  3578 | `	}` |
|       32 |  3579 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |  3580 | `	/* Register the constant with namespace-qualified name */` |
|        - |  3581 | `	{` |
|        - |  3582 | `		SyBlob sFQN;` |
|        - |  3583 | `		SyString sFQNStr;` |
|       32 |  3584 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       32 |  3585 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       32 |  3586 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       47 |  3587 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|       30 |  3588 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|       32 |  3589 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - |  3590 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|        - |  3591 | `			 * groups to the registered constant record for Reflection. */` |
|        7 |  3592 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|        4 |  3593 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        5 |  3594 | `			if( pCEntry ){` |
|        5 |  3595 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|        5 |  3596 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  3597 | `					SyBlobRelease(&sFQN);` |
|      ! 0 |  3598 | `					return SXERR_ABORT;` |
|        - |  3599 | `				}` |
|        2 |  3600 | `			}` |
|        2 |  3601 | `		}` |
|       32 |  3602 | `		SyBlobRelease(&sFQN);` |
|        - |  3603 | `	}` |
|       32 |  3604 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3605 | `		SySetRelease(pConsCode);` |
|      ! 0 |  3606 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  3607 | `	}` |
|       32 |  3608 | `	return SXRET_OK;` |
|        9 |  3609 | `Synchronize:` |
|        - |  3610 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       60 |  3611 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       42 |  3612 | `		pGen->pIn++;` |
|        4 |  3613 | `	}` |
|       22 |  3614 | `	return SXRET_OK;` |
|       29 |  3615 | `}` |
|        - |  3616 | `/*` |
|        - |  3617 | ` * Compile the 'continue' statement.` |
|        - |  3618 | ` * According to the PHP language reference` |
|        - |  3619 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  3620 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  3621 | ` *  iteration.` |
|        - |  3622 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  3623 | ` *  the purposes of continue.` |
|        - |  3624 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  3625 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  3626 | ` *  Note:` |
|        - |  3627 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  3628 | ` */` |
|        - |  3629 | `/*` |
|        - |  3630 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|        - |  3631 | ` * block and the target loop block. This ensures finally blocks run when` |
|        - |  3632 | ` * break/continue crosses a try boundary.` |
|        - |  3633 | ` *` |
|        - |  3634 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|        - |  3635 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|        - |  3636 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|        - |  3637 | ` */` |
|    58412 |  3638 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  3639 | `{` |
|    58417 |  3640 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    58417 |  3641 | `	int nInlineTry = 0;` |
|   272279 |  3642 | `	while( pBlock && pBlock != pTarget ){` |
|   213867 |  3643 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        6 |  3644 | `			if( pBlock->pUserData ){` |
|        - |  3645 | `				/* A try block with an exception context. In a generator its catch/finally` |
|        - |  3646 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|        - |  3647 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|        - |  3648 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|        6 |  3649 | `				if( pGen->bInGenerator ){` |
|        3 |  3650 | `					nInlineTry++;` |
|        2 |  3651 | `				}else{` |
|        3 |  3652 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|        - |  3653 | `				}` |
|        4 |  3654 | `			}else{` |
|        - |  3655 | `				/* A catch/finally block compiled into a separate bytecode container` |
|        - |  3656 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|      ! 0 |  3657 | `				break;` |
|        - |  3658 | `			}` |
|        2 |  3659 | `		}` |
|   213867 |  3660 | `		pBlock = pBlock->pParent;` |
|        5 |  3661 | `	}` |
|    58417 |  3662 | `	return nInlineTry;` |
|        5 |  3663 | `}` |
|    27238 |  3664 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  3665 | `{` |
|        - |  3666 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3667 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3668 | `	sxu32 nLineLocal;` |
|        - |  3669 | `	sxi32 rc;` |
|    27243 |  3670 | `	nLineLocal = pGen->pIn->nLine;` |
|    27243 |  3671 | `	iLevel = 0;` |
|        - |  3672 | `	/* Jump the 'continue' keyword */` |
|    27243 |  3673 | `	pGen->pIn++;` |
|    27243 |  3674 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3675 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3676 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3677 | `		 */` |
|        - |  3678 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  3679 | `		char *zAlloc = 0;` |
|        - |  3680 | `		SyString sNum;` |
|       17 |  3681 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  3682 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3683 | `			return SXERR_ABORT;` |
|        - |  3684 | `		}` |
|       17 |  3685 | `		if( rc == SXRET_OK ){` |
|       20 |  3686 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3687 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  3688 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3689 | `				return SXERR_ABORT;` |
|        - |  3690 | `			}` |
|       14 |  3691 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  3692 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3693 | `		}` |
|       17 |  3694 | `		if( iLevel < 2 ){` |
|        3 |  3695 | `			iLevel = 0;` |
|        1 |  3696 | `		}` |
|       17 |  3697 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3698 | `	}` |
|        - |  3699 | `	/* Point to the target loop */` |
|    27243 |  3700 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    27243 |  3701 | `	if( pLoop == 0 ){` |
|        - |  3702 | `		/* Illegal continue */` |
|       12 |  3703 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|       12 |  3704 | `		if( rc == SXERR_ABORT ){` |
|        - |  3705 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3706 | `			return SXERR_ABORT;` |
|        - |  3707 | `		}` |
|        7 |  3708 | `	}else{` |
|    27233 |  3709 | `		sxu32 nInstrIdx = 0;` |
|        - |  3710 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    27233 |  3711 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3712 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  3713 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    27233 |  3714 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    27233 |  3715 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  3716 | `			/* According to the PHP language reference manual` |
|        - |  3717 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|        - |  3718 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|        - |  3719 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|        - |  3720 | `			 */` |
|        5 |  3721 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|        5 |  3722 | `			if( rc == SXRET_OK ){` |
|        5 |  3723 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  3724 | `			}` |
|        3 |  3725 | `		}else{` |
|        - |  3726 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    27229 |  3727 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    27229 |  3728 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  3729 | `				JumpFixup sJumpFix;` |
|        - |  3730 | `				/* Post-continue */` |
|       14 |  3731 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       14 |  3732 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       14 |  3733 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|        6 |  3734 | `			}` |
|        - |  3735 | `		}` |
|        - |  3736 | `	}` |
|    27243 |  3737 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3738 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3739 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  3740 | `	}` |
|        - |  3741 | `	/* Statement successfully compiled */` |
|    27243 |  3742 | `	return SXRET_OK;` |
|    13624 |  3743 | `}` |
|        - |  3744 | `/*` |
|        - |  3745 | ` * Compile the 'break' statement.` |
|        - |  3746 | ` * According to the PHP language reference` |
|        - |  3747 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  3748 | ` *  structure.` |
|        - |  3749 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  3750 | ` *  enclosing structures are to be broken out of.` |
|        - |  3751 | ` */` |
|    31200 |  3752 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  3753 | `{` |
|        - |  3754 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3755 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3756 | `	sxi32 rc;` |
|    31205 |  3757 | `	iLevel = 0;` |
|        - |  3758 | `	/* Jump the 'break' keyword */` |
|    31205 |  3759 | `	pGen->pIn++;` |
|    31205 |  3760 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3761 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3762 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3763 | `		 */` |
|        - |  3764 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       18 |  3765 | `		char *zAlloc = 0;` |
|        - |  3766 | `		SyString sNum;` |
|       18 |  3767 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       18 |  3768 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3769 | `			return SXERR_ABORT;` |
|        - |  3770 | `		}` |
|       18 |  3771 | `		if( rc == SXRET_OK ){` |
|       21 |  3772 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3773 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       15 |  3774 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3775 | `				return SXERR_ABORT;` |
|        - |  3776 | `			}` |
|       15 |  3777 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       15 |  3778 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3779 | `		}` |
|       18 |  3780 | `		if( iLevel < 2 ){` |
|        3 |  3781 | `			iLevel = 0;` |
|        1 |  3782 | `		}` |
|       18 |  3783 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3784 | `	}` |
|        - |  3785 | `	/* Extract the target loop */` |
|    31205 |  3786 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    31205 |  3787 | `	if( pLoop == 0 ){` |
|        - |  3788 | `		/* Illegal break */` |
|       19 |  3789 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|       19 |  3790 | `		if( rc == SXERR_ABORT ){` |
|        - |  3791 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3792 | `			return SXERR_ABORT;` |
|        - |  3793 | `		}` |
|       11 |  3794 | `	}else{` |
|        - |  3795 | `		sxu32 nInstrIdx;` |
|        - |  3796 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    31189 |  3797 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3798 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    31189 |  3799 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    31189 |  3800 | `		if( rc == SXRET_OK ){` |
|        - |  3801 | `			/* Fix the jump later when the jump destination is resolved */` |
|    31189 |  3802 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    15592 |  3803 | `		}` |
|        - |  3804 | `	}` |
|    31205 |  3805 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3806 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3807 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  3808 | `	}` |
|        - |  3809 | `	/* Statement successfully compiled */` |
|    31205 |  3810 | `	return SXRET_OK;` |
|    15605 |  3811 | `}` |
|        - |  3812 | `/*` |
|        - |  3813 | ` * Compile or record a label.` |
|        - |  3814 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  3815 | ` * Example` |
|        - |  3816 | ` *  goto LABEL;` |
|        - |  3817 | ` *   echo 'Foo';` |
|        - |  3818 | ` *  LABEL:` |
|        - |  3819 | ` *   echo 'Bar';` |
|        - |  3820 | ` */` |
|      112 |  3821 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  3822 | `{` |
|        - |  3823 | `	GenBlock *pBlock;` |
|        - |  3824 | `	Label sLabel;` |
|        - |  3825 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|      117 |  3826 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|      117 |  3827 | `	if( pBlock ){` |
|        - |  3828 | `		sxi32 rc;` |
|        8 |  3829 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        4 |  3830 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|        6 |  3831 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3832 | `			return SXERR_ABORT;` |
|        - |  3833 | `		}` |
|        4 |  3834 | `	}else{` |
|      113 |  3835 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3836 | `		char *zDup;` |
|        - |  3837 | `		/* Initialize label fields */` |
|      113 |  3838 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  3839 | `		/* Duplicate label name */` |
|      113 |  3840 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      113 |  3841 | `		if( zDup == 0 ){` |
|      ! 0 |  3842 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3843 | `			return SXERR_ABORT;` |
|        - |  3844 | `		}` |
|      113 |  3845 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      113 |  3846 | `		sLabel.bRef  = FALSE;` |
|      113 |  3847 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      113 |  3848 | `		pBlock = pGen->pCurrent;` |
|      221 |  3849 | `		while( pBlock ){` |
|      133 |  3850 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       24 |  3851 | `				break;` |
|        - |  3852 | `			}` |
|        - |  3853 | `			/* Point to the upper block */` |
|      113 |  3854 | `			pBlock = pBlock->pParent;` |
|        5 |  3855 | `		}` |
|      113 |  3856 | `		if( pBlock ){` |
|       24 |  3857 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       14 |  3858 | `		}else{` |
|       93 |  3859 | `			sLabel.pFunc = 0;` |
|        - |  3860 | `		}` |
|        - |  3861 | `		/* Insert in label set */` |
|      113 |  3862 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  3863 | `	}` |
|      117 |  3864 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      117 |  3865 | `	return SXRET_OK;` |
|       61 |  3866 | `}` |
|        - |  3867 | `/*` |
|        - |  3868 | ` * Compile the so hated 'goto' statement.` |
|        - |  3869 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  3870 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  3871 | ` * a compiler it has to do this.` |
|        - |  3872 | ` * According to the PHP language reference manual` |
|        - |  3873 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  3874 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  3875 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  3876 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  3877 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  3878 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  3879 | ` *   of a multi-level break` |
|        - |  3880 | ` */` |
|      152 |  3881 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  3882 | `{` |
|        - |  3883 | `	JumpFixup sJump;` |
|        - |  3884 | `	sxi32 rc;` |
|      157 |  3885 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      157 |  3886 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3887 | `		/* Missing label */` |
|      ! 0 |  3888 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  3889 | `		if( rc == SXERR_ABORT ){` |
|        - |  3890 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3891 | `			return SXERR_ABORT;` |
|        - |  3892 | `		}` |
|      ! 0 |  3893 | `		return SXRET_OK;` |
|        - |  3894 | `	}` |
|      157 |  3895 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        6 |  3896 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|        6 |  3897 | `		if( rc == SXERR_ABORT ){` |
|        - |  3898 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3899 | `			return SXERR_ABORT;` |
|        - |  3900 | `		}` |
|        4 |  3901 | `	}else{` |
|      153 |  3902 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3903 | `		GenBlock *pBlock;` |
|        - |  3904 | `		char *zDup;` |
|        - |  3905 | `		/* Prepare the jump destination */` |
|      153 |  3906 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      153 |  3907 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  3908 | `		/* Duplicate label name */` |
|      153 |  3909 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      153 |  3910 | `		if( zDup == 0 ){` |
|      ! 0 |  3911 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3912 | `			return SXERR_ABORT;` |
|        - |  3913 | `		}` |
|      153 |  3914 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|      153 |  3915 | `		pBlock = pGen->pCurrent;` |
|      315 |  3916 | `		while( pBlock ){` |
|      199 |  3917 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       37 |  3918 | `				break;` |
|        - |  3919 | `			}` |
|        - |  3920 | `			/* Point to the upper block */` |
|      167 |  3921 | `			pBlock = pBlock->pParent;` |
|        5 |  3922 | `		}` |
|      153 |  3923 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        9 |  3924 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|        9 |  3925 | `			if( rc == SXERR_ABORT ){` |
|        - |  3926 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  3927 | `				return SXERR_ABORT;` |
|        - |  3928 | `			}` |
|        3 |  3929 | `		}` |
|      153 |  3930 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       30 |  3931 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       17 |  3932 | `		}else{` |
|      127 |  3933 | `			sJump.pFunc = 0;` |
|        - |  3934 | `		}` |
|        - |  3935 | `		/* Emit the unconditional jump */` |
|      153 |  3936 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      153 |  3937 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|       74 |  3938 | `		}` |
|        - |  3939 | `	}` |
|      157 |  3940 | `	pGen->pIn++; /* Jump the label name */` |
|      157 |  3941 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  3942 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  3943 | `	}` |
|        - |  3944 | `	/* Statement successfully compiled */` |
|      157 |  3945 | `	return SXRET_OK;` |
|       81 |  3946 | `}` |
|        - |  3947 | `/*` |
|        - |  3948 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  3949 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  3950 | ` * failure.` |
|        - |  3951 | ` */` |
|       20 |  3952 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  3953 | `{` |
|        - |  3954 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  3955 | `	sxu32 nRawObj;` |
|       10 |  3956 | `	sxu32 nObjIdx;` |
|        - |  3957 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  3958 | `	 * a PHP block.` |
|        - |  3959 | `	 */` |
|       10 |  3960 | `Consume:` |
|       22 |  3961 | `	nRawObj = nObjIdx = 0;` |
|       22 |  3962 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  3963 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  3964 | `		if( pRawObj == 0 ){` |
|      ! 0 |  3965 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3966 | `			return SXERR_ABORT;` |
|        - |  3967 | `		}` |
|        - |  3968 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  3969 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  3970 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  3971 | `		++nRawObj;` |
|      ! 0 |  3972 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  3973 | `	}` |
|       22 |  3974 | `	if( nRawObj > 0 ){` |
|        - |  3975 | `		/* Emit the consume instruction */` |
|      ! 0 |  3976 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  3977 | `	}` |
|       22 |  3978 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  3979 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  3980 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  3981 | `		SySetReset(pTokenSet);` |
|      ! 0 |  3982 | `		SySetReset(&pGen->aTrivia);` |
|        - |  3983 | `		/* Tokenize input */` |
|      ! 0 |  3984 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  3985 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  3986 | `		/* Point to the fresh token stream */` |
|      ! 0 |  3987 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  3988 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  3989 | `		/* Advance the stream cursor */` |
|      ! 0 |  3990 | `		pGen->pRawIn++;` |
|        - |  3991 | `		/* TICKET 1433-011 */` |
|      ! 0 |  3992 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  3993 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  3994 | `			sxi32 rc;` |
|        - |  3995 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  3996 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  3997 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  3998 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|      ! 0 |  3999 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  4000 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4001 | `				return SXERR_ABORT;` |
|      ! 0 |  4002 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  4003 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  4004 | `			}` |
|      ! 0 |  4005 | `			goto Consume;` |
|        - |  4006 | `		}` |
|      ! 0 |  4007 | `	}else{` |
|        - |  4008 | `		/* No more chunks to process */` |
|       22 |  4009 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  4010 | `		return SXERR_EOF;` |
|        - |  4011 | `	}` |
|      ! 0 |  4012 | `	return SXRET_OK;` |
|       12 |  4013 | `}` |
|        - |  4014 | `/*` |
|        - |  4015 | ` * Compile a PHP block.` |
|        - |  4016 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  4017 | ` * optionally delimited by braces {}.` |
|        - |  4018 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  4019 | ` * and this function takes care of generating the appropriate error` |
|        - |  4020 | ` * message.` |
|        - |  4021 | ` */` |
|  3040492 |  4022 | `static sxi32 PH7_CompileBlock(` |
|        - |  4023 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  4024 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  4025 | `	)` |
|        5 |  4026 | `{` |
|        - |  4027 | `	sxi32 rc;` |
|        - |  4028 | `	sxu32 nLine;` |
|  3040497 |  4029 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  3039039 |  4030 | `		nLine = pGen->pIn->nLine;` |
|  3039039 |  4031 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  3039039 |  4032 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4033 | `			return SXERR_ABORT;` |
|        - |  4034 | `		}` |
|  3039039 |  4035 | `		pGen->pIn++;` |
|        - |  4036 | `		/* Compile until we hit the closing braces '}' */` |
|  4463505 |  4037 | `		for(;;){` |
|  8927015 |  4038 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  4039 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  4040 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4041 | `			 	   return SXERR_ABORT;` |
|        - |  4042 | `				}` |
|       22 |  4043 | `				if( rc == SXERR_EOF ){` |
|        - |  4044 | `					/* No more token to process. Missing closing braces */` |
|       22 |  4045 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|       22 |  4046 | `					break;` |
|        - |  4047 | `				}` |
|      ! 0 |  4048 | `			}` |
|  8926995 |  4049 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  4050 | `				/* Closing braces found,break immediately*/` |
|  3039019 |  4051 | `				pGen->pIn++;` |
|  3039019 |  4052 | `				break;` |
|        - |  4053 | `			}` |
|        - |  4054 | `			/* Compile a single statement */` |
|  5887981 |  4055 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  5887981 |  4056 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4057 | `				return SXERR_ABORT;` |
|        - |  4058 | `			}` |
|        5 |  4059 | `		}` |
|  3039039 |  4060 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  1520980 |  4061 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      ! 0 |  4062 | `		pGen->pIn++;` |
|      ! 0 |  4063 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      ! 0 |  4064 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4065 | `			return SXERR_ABORT;` |
|        - |  4066 | `		}` |
|        - |  4067 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      ! 0 |  4068 | `		for(;;){` |
|      ! 0 |  4069 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  4070 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  4071 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4072 | `			 	   return SXERR_ABORT;` |
|        - |  4073 | `				}` |
|      ! 0 |  4074 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  4075 | `					/* No more token to process */` |
|      ! 0 |  4076 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  4077 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  4078 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  4079 | `					}` |
|      ! 0 |  4080 | `					break;` |
|        - |  4081 | `				}` |
|      ! 0 |  4082 | `			}` |
|      ! 0 |  4083 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  4084 | `				sxi32 nKwrd;` |
|        - |  4085 | `				/* Keyword found */` |
|      ! 0 |  4086 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  4087 | `				if( nKwrd == nKeywordEnd \|\|` |
|      ! 0 |  4088 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  4089 | `						/* Delimiter keyword found,break */` |
|      ! 0 |  4090 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      ! 0 |  4091 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      ! 0 |  4092 | `						}` |
|      ! 0 |  4093 | `						break;` |
|        - |  4094 | `				}` |
|      ! 0 |  4095 | `			}` |
|        - |  4096 | `			/* Compile a single statement */` |
|      ! 0 |  4097 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      ! 0 |  4098 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4099 | `				return SXERR_ABORT;` |
|        - |  4100 | `			}` |
|      ! 0 |  4101 | `		}` |
|      ! 0 |  4102 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  4103 | `	}else{` |
|        - |  4104 | `		/* Compile a single statement */` |
|     1463 |  4105 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     1463 |  4106 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4107 | `			return SXERR_ABORT;` |
|        - |  4108 | `		}` |
|        - |  4109 | `	}` |
|        - |  4110 | `	/* Jump trailing semi-colons ';' */` |
|  3040497 |  4111 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4112 | `		pGen->pIn++;` |
|      ! 0 |  4113 | `	}` |
|  3040497 |  4114 | `	return SXRET_OK;` |
|  1520251 |  4115 | `}` |
|        - |  4116 | `/*` |
|        - |  4117 | ` * Compile the gentle 'while' statement.` |
|        - |  4118 | ` * According to the PHP language reference` |
|        - |  4119 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  4120 | ` *  The basic form of a while statement is:` |
|        - |  4121 | ` *  while (expr)` |
|        - |  4122 | ` *   statement` |
|        - |  4123 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  4124 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  4125 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  4126 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  4127 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  4128 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  4129 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  4130 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  4131 | ` *  while (expr):` |
|        - |  4132 | ` *    statement` |
|        - |  4133 | ` *   endwhile;` |
|        - |  4134 | ` */` |
|    15672 |  4135 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  4136 | `{` |
|    15677 |  4137 | `	GenBlock *pWhileBlock = 0;` |
|    15677 |  4138 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  4139 | `	sxu32 nFalseJump;` |
|        - |  4140 | `	sxu32 nLine;` |
|        - |  4141 | `	sxi32 rc;` |
|    15677 |  4142 | `	nLine = pGen->pIn->nLine;` |
|        - |  4143 | `	/* Jump the 'while' keyword */` |
|    15677 |  4144 | `	pGen->pIn++;` |
|    15677 |  4145 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4146 | `		/* Syntax error */` |
|      ! 0 |  4147 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4148 | `		if( rc == SXERR_ABORT ){` |
|        - |  4149 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4150 | `			return SXERR_ABORT;` |
|        - |  4151 | `		}` |
|      ! 0 |  4152 | `		goto Synchronize;` |
|        - |  4153 | `	}` |
|        - |  4154 | `	/* Jump the left parenthesis '(' */` |
|    15677 |  4155 | `	pGen->pIn++;` |
|        - |  4156 | `	/* Create the loop block */` |
|    15677 |  4157 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    15677 |  4158 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4159 | `		return SXERR_ABORT;` |
|        - |  4160 | `	}` |
|        - |  4161 | `	/* Delimit the condition */` |
|    15677 |  4162 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    15677 |  4163 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4164 | `		/* Empty expression */` |
|        3 |  4165 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|        3 |  4166 | `		if( rc == SXERR_ABORT ){` |
|        - |  4167 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4168 | `			return SXERR_ABORT;` |
|        - |  4169 | `		}` |
|        1 |  4170 | `	}` |
|        - |  4171 | `	/* Swap token streams */` |
|    15677 |  4172 | `	pTmp = pGen->pEnd;` |
|    15677 |  4173 | `	pGen->pEnd = pEnd;` |
|        - |  4174 | `	/* Compile the expression */` |
|    15677 |  4175 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    15677 |  4176 | `	if( rc == SXERR_ABORT ){` |
|        - |  4177 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4178 | `		return SXERR_ABORT;` |
|        - |  4179 | `	}` |
|        - |  4180 | `	/* Update token stream */` |
|    15677 |  4181 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4182 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4183 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4184 | `			return SXERR_ABORT;` |
|        - |  4185 | `		}` |
|      ! 0 |  4186 | `		pGen->pIn++;` |
|      ! 0 |  4187 | `	}` |
|        - |  4188 | `	/* Synchronize pointers */` |
|    15677 |  4189 | `	pGen->pIn  = &pEnd[1];` |
|    15677 |  4190 | `	pGen->pEnd = pTmp;` |
|        - |  4191 | `	/* Emit the false jump */` |
|    15677 |  4192 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4193 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    15677 |  4194 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  4195 | `	/* Compile the loop body */` |
|    15677 |  4196 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    15677 |  4197 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4198 | `		return SXERR_ABORT;` |
|        - |  4199 | `	}` |
|        - |  4200 | `	/* Emit the unconditional jump to the start of the loop */` |
|    15677 |  4201 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  4202 | `	/* Fix all jumps now the destination is resolved */` |
|    15677 |  4203 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4204 | `	/* Release the loop block */` |
|    15677 |  4205 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4206 | `	/* Statement successfully compiled */` |
|    15677 |  4207 | `	return SXRET_OK;` |
|      ! 0 |  4208 | `Synchronize:` |
|        - |  4209 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4210 | `	 * compiling this erroneous block.` |
|        - |  4211 | `	 */` |
|      ! 0 |  4212 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4213 | `		pGen->pIn++;` |
|      ! 0 |  4214 | `	}` |
|      ! 0 |  4215 | `	return SXRET_OK;` |
|     7841 |  4216 | `}` |
|        - |  4217 | `/*` |
|        - |  4218 | ` * Compile the ugly do..while() statement.` |
|        - |  4219 | ` * According to the PHP language reference` |
|        - |  4220 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  4221 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  4222 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  4223 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  4224 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  4225 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  4226 | ` *  would end immediately).` |
|        - |  4227 | ` *  There is just one syntax for do-while loops:` |
|        - |  4228 | ` *  <?php` |
|        - |  4229 | ` *  $i = 0;` |
|        - |  4230 | ` *  do {` |
|        - |  4231 | ` *   echo $i;` |
|        - |  4232 | ` *  } while ($i > 0);` |
|        - |  4233 | ` * ?>` |
|        - |  4234 | ` */` |
|        2 |  4235 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  4236 | `{` |
|        3 |  4237 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  4238 | `	GenBlock *pDoBlock = 0;` |
|        - |  4239 | `	sxu32 nLine;` |
|        - |  4240 | `	sxi32 rc;` |
|        3 |  4241 | `	nLine = pGen->pIn->nLine;` |
|        - |  4242 | `	/* Jump the 'do' keyword */` |
|        3 |  4243 | `	pGen->pIn++;` |
|        - |  4244 | `	/* Create the loop block */` |
|        3 |  4245 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  4246 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4247 | `		return SXERR_ABORT;` |
|        - |  4248 | `	}` |
|        - |  4249 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  4250 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  4251 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  4252 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4253 | `		return SXERR_ABORT;` |
|        - |  4254 | `	}` |
|        3 |  4255 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4256 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  4257 | `	}` |
|        3 |  4258 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  4259 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  4260 | `			/* Missing 'while' statement */` |
|        3 |  4261 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|        3 |  4262 | `			if( rc == SXERR_ABORT ){` |
|        - |  4263 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4264 | `				return SXERR_ABORT;` |
|        - |  4265 | `			}` |
|        3 |  4266 | `			goto Synchronize;` |
|        - |  4267 | `	}` |
|        - |  4268 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  4269 | `	pGen->pIn++;` |
|      ! 0 |  4270 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4271 | `		/* Syntax error */` |
|      ! 0 |  4272 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4273 | `		if( rc == SXERR_ABORT ){` |
|        - |  4274 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4275 | `			return SXERR_ABORT;` |
|        - |  4276 | `		}` |
|      ! 0 |  4277 | `		goto Synchronize;` |
|        - |  4278 | `	}` |
|        - |  4279 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  4280 | `	pGen->pIn++;` |
|        - |  4281 | `	/* Delimit the condition */` |
|      ! 0 |  4282 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  4283 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4284 | `		/* Empty expression */` |
|      ! 0 |  4285 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|      ! 0 |  4286 | `		if( rc == SXERR_ABORT ){` |
|        - |  4287 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4288 | `			return SXERR_ABORT;` |
|        - |  4289 | `		}` |
|      ! 0 |  4290 | `		goto Synchronize;` |
|        - |  4291 | `	}` |
|        - |  4292 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  4293 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  4294 | `		JumpFixup *aPost;` |
|        - |  4295 | `		VmInstr *pInstr;` |
|        - |  4296 | `		sxu32 nJumpDest;` |
|        - |  4297 | `		sxu32 n;` |
|      ! 0 |  4298 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  4299 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  4300 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  4301 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  4302 | `			if( pInstr ){` |
|        - |  4303 | `				/* Fix */` |
|      ! 0 |  4304 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  4305 | `			}` |
|      ! 0 |  4306 | `		}` |
|      ! 0 |  4307 | `	}` |
|        - |  4308 | `	/* Swap token streams */` |
|      ! 0 |  4309 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  4310 | `	pGen->pEnd = pEnd;` |
|        - |  4311 | `	/* Compile the expression */` |
|      ! 0 |  4312 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  4313 | `	if( rc == SXERR_ABORT ){` |
|        - |  4314 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4315 | `		return SXERR_ABORT;` |
|        - |  4316 | `	}` |
|        - |  4317 | `	/* Update token stream */` |
|      ! 0 |  4318 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4319 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4320 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4321 | `			return SXERR_ABORT;` |
|        - |  4322 | `		}` |
|      ! 0 |  4323 | `		pGen->pIn++;` |
|      ! 0 |  4324 | `	}` |
|      ! 0 |  4325 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  4326 | `	pGen->pEnd = pTmp;` |
|        - |  4327 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  4328 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  4329 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  4330 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4331 | `	/* Release the loop block */` |
|      ! 0 |  4332 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4333 | `	/* Statement successfully compiled */` |
|      ! 0 |  4334 | `	return SXRET_OK;` |
|        1 |  4335 | `Synchronize:` |
|        - |  4336 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4337 | `	 * compiling this erroneous block.` |
|        - |  4338 | `	 */` |
|        3 |  4339 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4340 | `		pGen->pIn++;` |
|      ! 0 |  4341 | `	}` |
|        3 |  4342 | `	return SXRET_OK;` |
|        2 |  4343 | `}` |
|        - |  4344 | `/*` |
|        - |  4345 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  4346 | ` * According to the PHP language reference` |
|        - |  4347 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  4348 | ` *  The syntax of a for loop is:` |
|        - |  4349 | ` *  for (expr1; expr2; expr3)` |
|        - |  4350 | ` *   statement` |
|        - |  4351 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  4352 | ` *  the beginning of the loop.` |
|        - |  4353 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  4354 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  4355 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  4356 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  4357 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  4358 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  4359 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  4360 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  4361 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  4362 | ` *  of using the for truth expression.` |
|        - |  4363 | ` */` |
|    38980 |  4364 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  4365 | `{` |
|    38985 |  4366 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    38985 |  4367 | `	GenBlock *pForBlock = 0;` |
|        - |  4368 | `	sxu32 nFalseJump;` |
|        - |  4369 | `	sxu32 nLine;` |
|        - |  4370 | `	sxi32 rc;` |
|    38985 |  4371 | `	nLine = pGen->pIn->nLine;` |
|        - |  4372 | `	/* Jump the 'for' keyword */` |
|    38985 |  4373 | `	pGen->pIn++;` |
|    38985 |  4374 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4375 | `		/* Syntax error */` |
|      ! 0 |  4376 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  4377 | `		if( rc == SXERR_ABORT ){` |
|        - |  4378 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4379 | `			return SXERR_ABORT;` |
|        - |  4380 | `		}` |
|      ! 0 |  4381 | `		return SXRET_OK;` |
|        - |  4382 | `	}` |
|        - |  4383 | `	/* Jump the left parenthesis '(' */` |
|    38985 |  4384 | `	pGen->pIn++;` |
|        - |  4385 | `	/* Delimit the init-expr;condition;post-expr */` |
|    38985 |  4386 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    38985 |  4387 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4388 | `		/* Empty expression */` |
|      ! 0 |  4389 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  4390 | `		if( rc == SXERR_ABORT ){` |
|        - |  4391 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4392 | `			return SXERR_ABORT;` |
|        - |  4393 | `		}` |
|        - |  4394 | `		/* Synchronize */` |
|      ! 0 |  4395 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4396 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4397 | `			pGen->pIn++;` |
|      ! 0 |  4398 | `		}` |
|      ! 0 |  4399 | `		return SXRET_OK;` |
|        - |  4400 | `	}` |
|        - |  4401 | `	/* Swap token streams */` |
|    38985 |  4402 | `	pTmp = pGen->pEnd;` |
|    38985 |  4403 | `	pGen->pEnd = pEnd;` |
|        - |  4404 | `	/* Compile initialization expressions if available */` |
|    38985 |  4405 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4406 | `	/* Pop operand lvalues */` |
|    38985 |  4407 | `	if( rc == SXERR_ABORT ){` |
|        - |  4408 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4409 | `		return SXERR_ABORT;` |
|    38985 |  4410 | `	}else if( rc != SXERR_EMPTY ){` |
|    38983 |  4411 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19489 |  4412 | `	}` |
|    38985 |  4413 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4414 | `		/* Syntax error */` |
|      ! 0 |  4415 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4416 | `			"for: Expected ';' after initialization expressions");` |
|      ! 0 |  4417 | `		if( rc == SXERR_ABORT ){` |
|        - |  4418 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4419 | `			return SXERR_ABORT;` |
|        - |  4420 | `		}` |
|      ! 0 |  4421 | `		return SXRET_OK;` |
|        - |  4422 | `	}` |
|        - |  4423 | `	/* Jump the trailing ';' */` |
|    38985 |  4424 | `	pGen->pIn++;` |
|        - |  4425 | `	/* Create the loop block */` |
|    38985 |  4426 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    38985 |  4427 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4428 | `		return SXERR_ABORT;` |
|        - |  4429 | `	}` |
|        - |  4430 | `	/* Deffer continue jumps */` |
|    38985 |  4431 | `	pForBlock->bPostContinue = TRUE;` |
|        - |  4432 | `	/* Compile the condition */` |
|    38985 |  4433 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38985 |  4434 | `	if( rc == SXERR_ABORT ){` |
|        - |  4435 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4436 | `		return SXERR_ABORT;` |
|    38985 |  4437 | `	}else if( rc != SXERR_EMPTY ){` |
|        - |  4438 | `		/* Emit the false jump */` |
|    38983 |  4439 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4440 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    38983 |  4441 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    19489 |  4442 | `	}` |
|    38985 |  4443 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4444 | `		/* Syntax error */` |
|        6 |  4445 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4446 | `			"for: Expected ';' after conditionals expressions");` |
|        6 |  4447 | `		if( rc == SXERR_ABORT ){` |
|        - |  4448 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4449 | `			return SXERR_ABORT;` |
|        - |  4450 | `		}` |
|        6 |  4451 | `		return SXRET_OK;` |
|        - |  4452 | `	}` |
|        - |  4453 | `	/* Jump the trailing ';' */` |
|    38981 |  4454 | `	pGen->pIn++;` |
|        - |  4455 | `	/* Save the post condition stream */` |
|    38981 |  4456 | `	pPostStart = pGen->pIn;` |
|        - |  4457 | `	/* Compile the loop body */` |
|    38981 |  4458 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    38981 |  4459 | `	pGen->pEnd = pTmp;` |
|    38981 |  4460 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    38981 |  4461 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4462 | `		return SXERR_ABORT;` |
|        - |  4463 | `	}` |
|        - |  4464 | `	/* Fix post-continue jumps */` |
|    38981 |  4465 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - |  4466 | `		JumpFixup *aPost;` |
|        - |  4467 | `		VmInstr *pInstr;` |
|        - |  4468 | `		sxu32 nJumpDest;` |
|        - |  4469 | `		sxu32 n;` |
|       14 |  4470 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       14 |  4471 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       26 |  4472 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       14 |  4473 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       14 |  4474 | `			if( pInstr ){` |
|        - |  4475 | `				/* Fix jump */` |
|       14 |  4476 | `				pInstr->iP2 = nJumpDest;` |
|        6 |  4477 | `			}` |
|        8 |  4478 | `		}` |
|        6 |  4479 | `	}` |
|        - |  4480 | `	/* compile the post-expressions if available */` |
|    38981 |  4481 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4482 | `		pPostStart++;` |
|      ! 0 |  4483 | `	}` |
|    38981 |  4484 | `	if( pPostStart < pEnd ){` |
|        - |  4485 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    38981 |  4486 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    38981 |  4487 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38981 |  4488 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - |  4489 | `			/* Syntax error */` |
|      ! 0 |  4490 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 |  4491 | `			if( rc == SXERR_ABORT ){` |
|        - |  4492 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4493 | `				return SXERR_ABORT;` |
|        - |  4494 | `			}` |
|      ! 0 |  4495 | `			return SXRET_OK;` |
|        - |  4496 | `		}` |
|    38981 |  4497 | `		RE_SWAP_DELIMITER(pGen);` |
|    38981 |  4498 | `		if( rc == SXERR_ABORT ){` |
|        - |  4499 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4500 | `			return SXERR_ABORT;` |
|    38981 |  4501 | `		}else if( rc != SXERR_EMPTY){` |
|        - |  4502 | `			/* Pop operand lvalue */` |
|    38981 |  4503 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19488 |  4504 | `		}` |
|    19488 |  4505 | `	}` |
|        - |  4506 | `	/* Emit the unconditional jump to the start of the loop */` |
|    38981 |  4507 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - |  4508 | `	/* Fix all jumps now the destination is resolved */` |
|    38981 |  4509 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4510 | `	/* Release the loop block */` |
|    38981 |  4511 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4512 | `	/* Statement successfully compiled */` |
|    38981 |  4513 | `	return SXRET_OK;` |
|    19495 |  4514 | `}` |
|        - |  4515 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - |  4516 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - |  4517 | ` * are allowed.` |
|        - |  4518 | ` */` |
|   241628 |  4519 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  4520 | `{` |
|   241633 |  4521 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   241633 |  4522 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  4523 | `		/* Unexpected expression */` |
|      ! 0 |  4524 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  4525 | `			"foreach: Expecting a variable name");` |
|      ! 0 |  4526 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  4527 | `			rc = SXERR_INVALID;` |
|      ! 0 |  4528 | `		}` |
|      ! 0 |  4529 | `	}` |
|   241633 |  4530 | `	return rc;` |
|        5 |  4531 | `}` |
|        - |  4532 | `/*` |
|        - |  4533 | ` * Compile the 'foreach' statement.` |
|        - |  4534 | ` * According to the PHP language reference` |
|        - |  4535 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - |  4536 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - |  4537 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - |  4538 | ` *  is a minor but useful extension of the first:` |
|        - |  4539 | ` *  foreach (array_expression as $value)` |
|        - |  4540 | ` *    statement` |
|        - |  4541 | ` *  foreach (array_expression as $key => $value)` |
|        - |  4542 | ` *   statement` |
|        - |  4543 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - |  4544 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - |  4545 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - |  4546 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - |  4547 | ` *  to the variable $key on each loop.` |
|        - |  4548 | ` *  Note:` |
|        - |  4549 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - |  4550 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - |  4551 | ` *  Note:` |
|        - |  4552 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - |  4553 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - |  4554 | ` *  or after the foreach without resetting it.` |
|        - |  4555 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - |  4556 | ` *  of copying the value.` |
|        - |  4557 | ` */` |
|   175384 |  4558 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 |  4559 | `{` |
|   175389 |  4560 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   175389 |  4561 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   175389 |  4562 | `	GenBlock *pForeachBlock = 0;` |
|        - |  4563 | `	ph7_foreach_info *pInfo;` |
|        - |  4564 | `	sxu32 nFalseJump;` |
|        - |  4565 | `	VmInstr *pInstr;` |
|        - |  4566 | `	sxu32 nLine;` |
|        - |  4567 | `	sxi32 rc;` |
|   175389 |  4568 | `	nLine = pGen->pIn->nLine;` |
|        - |  4569 | `	/* Jump the 'foreach' keyword */` |
|   175389 |  4570 | `	pGen->pIn++;` |
|   175389 |  4571 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4572 | `		/* Syntax error */` |
|      ! 0 |  4573 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 |  4574 | `		if( rc == SXERR_ABORT ){` |
|        - |  4575 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4576 | `			return SXERR_ABORT;` |
|        - |  4577 | `		}` |
|      ! 0 |  4578 | `		goto Synchronize;` |
|        - |  4579 | `	}` |
|        - |  4580 | `	/* Jump the left parenthesis '(' */` |
|   175389 |  4581 | `	pGen->pIn++;` |
|        - |  4582 | `	/* Create the loop block */` |
|   175389 |  4583 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   175389 |  4584 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4585 | `		return SXERR_ABORT;` |
|        - |  4586 | `	}` |
|        - |  4587 | `	/* Delimit the expression */` |
|   175389 |  4588 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   175389 |  4589 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4590 | `		/* Empty expression */` |
|      ! 0 |  4591 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 |  4592 | `		if( rc == SXERR_ABORT ){` |
|        - |  4593 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4594 | `			return SXERR_ABORT;` |
|        - |  4595 | `		}` |
|        - |  4596 | `		/* Synchronize */` |
|      ! 0 |  4597 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4598 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4599 | `			pGen->pIn++;` |
|      ! 0 |  4600 | `		}` |
|      ! 0 |  4601 | `		return SXRET_OK;` |
|        - |  4602 | `	}` |
|        - |  4603 | `	/* Compile the array expression */` |
|   175389 |  4604 | `	pCur = pGen->pIn;` |
|  1025017 |  4605 | `	while( pCur < pEnd ){` |
|  1025017 |  4606 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   179287 |  4607 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   179287 |  4608 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - |  4609 | `				/* Break with the first 'as' found */` |
|   175389 |  4610 | `				break;` |
|        - |  4611 | `			}` |
|     1949 |  4612 | `		}` |
|        - |  4613 | `		/* Advance the stream cursor */` |
|   849633 |  4614 | `		pCur++;` |
|        5 |  4615 | `	}` |
|   175389 |  4616 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 |  4617 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  4618 | `			"foreach: Missing array/object expression");` |
|      ! 0 |  4619 | `		if( rc == SXERR_ABORT ){` |
|        - |  4620 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4621 | `			return SXERR_ABORT;` |
|        - |  4622 | `		}` |
|      ! 0 |  4623 | `		goto Synchronize;` |
|        - |  4624 | `	}` |
|        - |  4625 | `	/* Swap token streams */` |
|   175389 |  4626 | `	pTmp = pGen->pEnd;` |
|   175389 |  4627 | `	pGen->pEnd = pCur;` |
|   175389 |  4628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   175389 |  4629 | `	if( rc == SXERR_ABORT ){` |
|        - |  4630 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4631 | `		return SXERR_ABORT;` |
|        - |  4632 | `	}` |
|        - |  4633 | `	/* Update token stream */` |
|   175389 |  4634 | `	while(pGen->pIn < pCur ){` |
|      ! 0 |  4635 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4636 | `		if( rc == SXERR_ABORT ){` |
|        - |  4637 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4638 | `			return SXERR_ABORT;` |
|        - |  4639 | `		}` |
|      ! 0 |  4640 | `		pGen->pIn++;` |
|      ! 0 |  4641 | `	}` |
|   175389 |  4642 | `	pCur++; /* Jump the 'as' keyword */` |
|   175389 |  4643 | `	pGen->pIn = pCur;` |
|   175389 |  4644 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4645 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 |  4646 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4647 | `			return SXERR_ABORT;` |
|        - |  4648 | `		}` |
|      ! 0 |  4649 | `	}` |
|        - |  4650 | `	/* Create the foreach context */` |
|   175389 |  4651 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   175389 |  4652 | `	if( pInfo == 0 ){` |
|      ! 0 |  4653 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  4654 | `		return SXERR_ABORT;` |
|        - |  4655 | `	}` |
|        - |  4656 | `	/* Zero the structure */` |
|   175389 |  4657 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - |  4658 | `	/* Initialize structure fields */` |
|   175389 |  4659 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - |  4660 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - |  4661 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - |  4662 | `	 * '=>'. */` |
|   175389 |  4663 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   175389 |  4664 | `	if( pCur < pEnd ){` |
|        - |  4665 | `		/* Compile the expression holding the key name */` |
|    66269 |  4666 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 |  4667 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 |  4668 | `			if( rc == SXERR_ABORT ){` |
|        - |  4669 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4670 | `				return SXERR_ABORT;` |
|        - |  4671 | `			}` |
|      ! 0 |  4672 | `		}else{` |
|    66269 |  4673 | `			pGen->pEnd = pCur;` |
|    66269 |  4674 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    66269 |  4675 | `			if( rc == SXERR_ABORT ){` |
|        - |  4676 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4677 | `				return SXERR_ABORT;` |
|        - |  4678 | `			}` |
|    66269 |  4679 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    66269 |  4680 | `			if( pInstr->p3 ){` |
|        - |  4681 | `				/* Record key name */` |
|    66269 |  4682 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    33132 |  4683 | `			}` |
|    66269 |  4684 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - |  4685 | `		}` |
|    66269 |  4686 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    33132 |  4687 | `	}` |
|   175389 |  4688 | `	pGen->pEnd = pEnd;` |
|   175389 |  4689 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4690 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 |  4691 | `		if( rc == SXERR_ABORT ){` |
|        - |  4692 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4693 | `			return SXERR_ABORT;` |
|        - |  4694 | `		}` |
|      ! 0 |  4695 | `		goto Synchronize;` |
|        - |  4696 | `	}` |
|   175389 |  4697 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       33 |  4698 | `		pGen->pIn++;` |
|        - |  4699 | `		/* Pass by reference  */` |
|       33 |  4700 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       15 |  4701 | `	}` |
|        - |  4702 | `	/* Check if the value target is list() */` |
|   175389 |  4703 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 |  4704 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - |  4705 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - |  4706 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - |  4707 | `		 */` |
|        - |  4708 | `		static int iForeachListCnt = 0;` |
|        - |  4709 | `		char zTmp[128];` |
|        - |  4710 | `		sxu32 nLen;` |
|        - |  4711 | `		char *zDup;` |
|       10 |  4712 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 |  4713 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 |  4714 | `		if( zDup == 0 ){` |
|      ! 0 |  4715 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4716 | `			return SXERR_ABORT;` |
|        - |  4717 | `		}` |
|       10 |  4718 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4719 | `		/* Save list() token boundaries */` |
|       10 |  4720 | `		pListStart = pGen->pIn;` |
|        - |  4721 | `		/* Advance past list(...) — validate parentheses */` |
|       10 |  4722 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 |  4723 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  4724 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  4725 | `				"foreach: Expected '(' after 'list'");` |
|        3 |  4726 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4727 | `				return SXERR_ABORT;` |
|        - |  4728 | `			}` |
|        3 |  4729 | `			goto Synchronize;` |
|        - |  4730 | `		}` |
|        7 |  4731 | `		pGen->pIn++; /* Jump '(' */` |
|        7 |  4732 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 |  4733 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4734 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4735 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 |  4736 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4737 | `				return SXERR_ABORT;` |
|        - |  4738 | `			}` |
|      ! 0 |  4739 | `			goto Synchronize;` |
|        - |  4740 | `		}` |
|        7 |  4741 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 |  4742 | `		pListEnd = pGen->pIn;` |
|        7 |  4743 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   175384 |  4744 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - |  4745 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - |  4746 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - |  4747 | `		 */` |
|        - |  4748 | `		static int iForeachShortListCnt = 0;` |
|        - |  4749 | `		char zTmp[128];` |
|        - |  4750 | `		sxu32 nLen;` |
|        - |  4751 | `		char *zDup;` |
|       13 |  4752 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       13 |  4753 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       13 |  4754 | `		if( zDup == 0 ){` |
|      ! 0 |  4755 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4756 | `			return SXERR_ABORT;` |
|        - |  4757 | `		}` |
|       13 |  4758 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4759 | `		/* Save [...] token boundaries */` |
|       13 |  4760 | `		pListStart = pGen->pIn;` |
|        - |  4761 | `		/* Advance past [...] */` |
|       13 |  4762 | `		pGen->pIn++; /* Jump '[' */` |
|       13 |  4763 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       13 |  4764 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4765 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4766 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 |  4767 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4768 | `				return SXERR_ABORT;` |
|        - |  4769 | `			}` |
|      ! 0 |  4770 | `			goto Synchronize;` |
|        - |  4771 | `		}` |
|       13 |  4772 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       13 |  4773 | `		pListEnd = pGen->pIn;` |
|       13 |  4774 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        7 |  4775 | `	}else{` |
|        - |  4776 | `		/* Compile the expression holding the value name */` |
|   175369 |  4777 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   175369 |  4778 | `		if( rc == SXERR_ABORT ){` |
|        - |  4779 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4780 | `			return SXERR_ABORT;` |
|        - |  4781 | `		}` |
|   175369 |  4782 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   175369 |  4783 | `		if( pInstr->p3 ){` |
|        - |  4784 | `			/* Record value name */` |
|   175369 |  4785 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    87682 |  4786 | `		}` |
|        - |  4787 | `	}` |
|        - |  4788 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   175387 |  4789 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - |  4790 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175387 |  4791 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - |  4792 | `	/* Record the first instruction to execute */` |
|   175387 |  4793 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4794 | `	/* Emit the FOREACH_STEP instruction */` |
|   175387 |  4795 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - |  4796 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175387 |  4797 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - |  4798 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   175387 |  4799 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - |  4800 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - |  4801 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - |  4802 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - |  4803 | `		 */` |
|       19 |  4804 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - |  4805 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - |  4806 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - |  4807 | `		 * picks up the delimiter and the variable names inside.` |
|        - |  4808 | `		 */` |
|       19 |  4809 | `		pSavedIn = pGen->pIn;` |
|       19 |  4810 | `		pSavedEnd = pGen->pEnd;` |
|       19 |  4811 | `		pGen->pIn = pListStart;` |
|       19 |  4812 | `		pGen->pEnd = pListEnd;` |
|       19 |  4813 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       13 |  4814 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 |  4815 | `		}else{` |
|        7 |  4816 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - |  4817 | `		}` |
|       19 |  4818 | `		pGen->pIn = pSavedIn;` |
|       19 |  4819 | `		pGen->pEnd = pSavedEnd;` |
|       19 |  4820 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4821 | `			return SXERR_ABORT;` |
|        - |  4822 | `		}` |
|        - |  4823 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       19 |  4824 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        9 |  4825 | `	}` |
|        - |  4826 | `	/* Compile the loop body */` |
|   175387 |  4827 | `	pGen->pIn = &pEnd[1];` |
|   175387 |  4828 | `	pGen->pEnd = pTmp;` |
|   175387 |  4829 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   175387 |  4830 | `	if( rc == SXERR_ABORT ){` |
|        - |  4831 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4832 | `		return SXERR_ABORT;` |
|        - |  4833 | `	}` |
|        - |  4834 | `	/* Emit the unconditional jump to the start of the loop */` |
|   175387 |  4835 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - |  4836 | `	/* Fix all jumps now the destination is resolved */` |
|   175387 |  4837 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4838 | `	/* Release the loop block */` |
|   175387 |  4839 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4840 | `	/* Statement successfully compiled */` |
|   175387 |  4841 | `	return SXRET_OK;` |
|        1 |  4842 | `Synchronize:` |
|        - |  4843 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4844 | `	 * compiling this erroneous block.` |
|        - |  4845 | `	 */` |
|        3 |  4846 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4847 | `		pGen->pIn++;` |
|      ! 0 |  4848 | `	}` |
|        3 |  4849 | `	return SXRET_OK;` |
|    87697 |  4850 | `}` |
|        - |  4851 | `/*` |
|        - |  4852 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - |  4853 | ` * According to the PHP language reference` |
|        - |  4854 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - |  4855 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - |  4856 | ` *  that is similar to that of C:` |
|        - |  4857 | ` *  if (expr)` |
|        - |  4858 | ` *   statement` |
|        - |  4859 | ` *  else construct:` |
|        - |  4860 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - |  4861 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - |  4862 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - |  4863 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - |  4864 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - |  4865 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - |  4866 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - |  4867 | ` *  elseif` |
|        - |  4868 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - |  4869 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - |  4870 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - |  4871 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - |  4872 | ` *   than b, a equal to b or a is smaller than b:` |
|        - |  4873 | ` *   <?php` |
|        - |  4874 | ` *    if ($a > $b) {` |
|        - |  4875 | ` *     echo "a is bigger than b";` |
|        - |  4876 | ` *    } elseif ($a == $b) {` |
|        - |  4877 | ` *     echo "a is equal to b";` |
|        - |  4878 | ` *    } else {` |
|        - |  4879 | ` *     echo "a is smaller than b";` |
|        - |  4880 | ` *    }` |
|        - |  4881 | ` *    ?>` |
|        - |  4882 | ` */` |
|  1198880 |  4883 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 |  4884 | `{` |
|  1198885 |  4885 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  1198885 |  4886 | `	GenBlock *pCondBlock = 0;` |
|        - |  4887 | `	sxu32 nJumpIdx;` |
|        - |  4888 | `	sxu32 nKeyID;` |
|        - |  4889 | `	sxi32 rc;` |
|        - |  4890 | `	/* Jump the 'if' keyword */` |
|  1198885 |  4891 | `	pGen->pIn++;` |
|  1198885 |  4892 | `	pToken = pGen->pIn;` |
|        - |  4893 | `	/* Create the conditional block */` |
|  1198885 |  4894 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  1198885 |  4895 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4896 | `		return SXERR_ABORT;` |
|        - |  4897 | `	}` |
|        - |  4898 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   646107 |  4899 | `	for(;;){` |
|  1292219 |  4900 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4901 | `			/* Syntax error */` |
|      ! 0 |  4902 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4903 | `				pToken--;` |
|      ! 0 |  4904 | `			}` |
|      ! 0 |  4905 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 |  4906 | `			if( rc == SXERR_ABORT ){` |
|        - |  4907 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4908 | `				return SXERR_ABORT;` |
|        - |  4909 | `			}` |
|      ! 0 |  4910 | `			goto Synchronize;` |
|        - |  4911 | `		}` |
|        - |  4912 | `		/* Jump the left parenthesis '(' */` |
|  1292219 |  4913 | `		pToken++;` |
|        - |  4914 | `		/* Delimit the condition */` |
|  1292219 |  4915 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1292219 |  4916 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - |  4917 | `			/* Syntax error */` |
|      ! 0 |  4918 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4919 | `				pToken--;` |
|      ! 0 |  4920 | `			}` |
|      ! 0 |  4921 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 |  4922 | `			if( rc == SXERR_ABORT ){` |
|        - |  4923 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4924 | `				return SXERR_ABORT;` |
|        - |  4925 | `			}` |
|      ! 0 |  4926 | `			goto Synchronize;` |
|        - |  4927 | `		}` |
|        - |  4928 | `		/* Swap token streams */` |
|  1292219 |  4929 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - |  4930 | `		/* Compile the condition */` |
|  1292219 |  4931 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4932 | `		/* Update token stream */` |
|  1292219 |  4933 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 |  4934 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4935 | `			pGen->pIn++;` |
|      ! 0 |  4936 | `		}` |
|  1292219 |  4937 | `		pGen->pIn  = &pEnd[1];` |
|  1292219 |  4938 | `		pGen->pEnd = pTmp;` |
|  1292219 |  4939 | `		if( rc == SXERR_ABORT ){` |
|        - |  4940 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4941 | `			return SXERR_ABORT;` |
|        - |  4942 | `		}` |
|        - |  4943 | `		/* Emit the false jump */` |
|  1292219 |  4944 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - |  4945 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  1292219 |  4946 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - |  4947 | `		/* Compile the body */` |
|  1292219 |  4948 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  1292219 |  4949 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4950 | `			return SXERR_ABORT;` |
|        - |  4951 | `		}` |
|  1292219 |  4952 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   239765 |  4953 | `			break;` |
|        - |  4954 | `		}` |
|        - |  4955 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   812699 |  4956 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   812699 |  4957 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   633445 |  4958 | `			break;` |
|        - |  4959 | `		}` |
|        - |  4960 | `		/* Emit the unconditional jump */` |
|   179259 |  4961 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - |  4962 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   179259 |  4963 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   179259 |  4964 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   171373 |  4965 | `			pToken = &pGen->pIn[1];` |
|   171373 |  4966 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85486 |  4967 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    42965 |  4968 | `					break;` |
|        - |  4969 | `			}` |
|    85453 |  4970 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42724 |  4971 | `		}` |
|    93339 |  4972 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - |  4973 | `		/* Synchronize cursors */` |
|    93339 |  4974 | `		pToken = pGen->pIn;` |
|        - |  4975 | `		/* Fix the false jump */` |
|    93339 |  4976 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 |  4977 | `	} /* For(;;) */` |
|        - |  4978 | `	/* Fix the false jump */` |
|  1198885 |  4979 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  1198885 |  4980 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   719360 |  4981 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - |  4982 | `			/* Compile the else block */` |
|    85925 |  4983 | `			pGen->pIn++;` |
|    85925 |  4984 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    85925 |  4985 | `			if( rc == SXERR_ABORT ){` |
|        - |  4986 |  |
|      ! 0 |  4987 | `				return SXERR_ABORT;` |
|        - |  4988 | `			}` |
|    42960 |  4989 | `	}` |
|  1198885 |  4990 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4991 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  1198885 |  4992 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - |  4993 | `	/* Release the conditional block */` |
|  1198885 |  4994 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4995 | `	/* Statement successfully compiled */` |
|  1198885 |  4996 | `	return SXRET_OK;` |
|      ! 0 |  4997 | `Synchronize:` |
|        - |  4998 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - |  4999 | `	 */` |
|      ! 0 |  5000 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  5001 | `		pGen->pIn++;` |
|      ! 0 |  5002 | `	}` |
|      ! 0 |  5003 | `	return SXRET_OK;` |
|   599445 |  5004 | `}` |
|        - |  5005 | `/*` |
|        - |  5006 | ` * Compile the global construct.` |
|        - |  5007 | ` * According to the PHP language reference` |
|        - |  5008 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - |  5009 | ` *  to be used in that function.` |
|        - |  5010 | ` *  Example #1 Using global` |
|        - |  5011 | ` *  <?php` |
|        - |  5012 | ` *   $a = 1;` |
|        - |  5013 | ` *   $b = 2;` |
|        - |  5014 | ` *   function Sum()` |
|        - |  5015 | ` *   {` |
|        - |  5016 | ` *    global $a, $b;` |
|        - |  5017 | ` *    $b = $a + $b;` |
|        - |  5018 | ` *   }` |
|        - |  5019 | ` *   Sum();` |
|        - |  5020 | ` *   echo $b;` |
|        - |  5021 | ` *  ?>` |
|        - |  5022 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - |  5023 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - |  5024 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - |  5025 | ` */` |
|       38 |  5026 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 |  5027 | `{` |
|       43 |  5028 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5029 | `	sxi32 nExpr;` |
|        - |  5030 | `	sxi32 rc;` |
|        - |  5031 | `	/* Jump the 'global' keyword */` |
|       43 |  5032 | `	pGen->pIn++;` |
|       43 |  5033 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - |  5034 | `		/* Nothing to process */` |
|      ! 0 |  5035 | `		return SXRET_OK;` |
|        - |  5036 | `	}` |
|       43 |  5037 | `	pTmp = pGen->pEnd;` |
|       43 |  5038 | `	nExpr = 0;` |
|       91 |  5039 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       53 |  5040 | `		if( pGen->pIn < pNext ){` |
|       53 |  5041 | `			pGen->pEnd = pNext;` |
|       53 |  5042 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5043 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 |  5044 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  5045 | `					return SXERR_ABORT;` |
|        - |  5046 | `				}` |
|      ! 0 |  5047 | `			}else{` |
|       53 |  5048 | `				pGen->pIn++;` |
|       53 |  5049 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5050 | `					/* Emit a warning */` |
|      ! 0 |  5051 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 |  5052 | `				}else{` |
|       53 |  5053 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       53 |  5054 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  5055 | `						return SXERR_ABORT;` |
|       53 |  5056 | `					}else if(rc != SXERR_EMPTY ){` |
|       53 |  5057 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       53 |  5058 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - |  5059 | `							/* Variable name, not a constant */` |
|       53 |  5060 | `							pLast->iP1 = 0;` |
|       24 |  5061 | `						}` |
|       53 |  5062 | `						nExpr++;` |
|       24 |  5063 | `					}` |
|        - |  5064 | `				}` |
|        - |  5065 | `			}` |
|       24 |  5066 | `		}` |
|        - |  5067 | `		/* Next expression in the stream */` |
|       53 |  5068 | `		pGen->pIn = pNext;` |
|        - |  5069 | `		/* Jump trailing commas */` |
|       63 |  5070 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 |  5071 | `			pGen->pIn++;` |
|        5 |  5072 | `		}` |
|        5 |  5073 | `	}` |
|        - |  5074 | `	/* Restore token stream */` |
|       43 |  5075 | `	pGen->pEnd = pTmp;` |
|       43 |  5076 | `	if( nExpr > 0 ){` |
|        - |  5077 | `		/* Emit the uplink instruction */` |
|       43 |  5078 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       19 |  5079 | `	}` |
|       43 |  5080 | `	return SXRET_OK;` |
|       24 |  5081 | `}` |
|        - |  5082 | `/*` |
|        - |  5083 | ` * Compile the return statement.` |
|        - |  5084 | ` * According to the PHP language reference` |
|        - |  5085 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - |  5086 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - |  5087 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - |  5088 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - |  5089 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - |  5090 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - |  5091 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - |  5092 | ` *  from within the main script file, then script execution end.` |
|        - |  5093 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - |  5094 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - |  5095 | ` *  should do so as PHP has less work to do in this case.` |
|        - |  5096 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - |  5097 | ` */` |
|  1644926 |  5098 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 |  5099 | `{` |
|  1644931 |  5100 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - |  5101 | `	sxi32 rc;` |
|  1644931 |  5102 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1644931 |  5103 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - |  5104 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - |  5105 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - |  5106 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - |  5107 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - |  5108 | `	 * normally below so token processing stays consistent. */` |
|  4292727 |  5109 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  2647801 |  5110 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 |  5111 | `	}` |
|  1644926 |  5112 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  1644899 |  5113 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 |  5114 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  5115 | `			"A never-returning function must not return");` |
|        3 |  5116 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5117 | `			return SXERR_ABORT;` |
|        - |  5118 | `		}` |
|        1 |  5119 | `	}` |
|        - |  5120 | `	/* Jump the 'return' keyword */` |
|  1644931 |  5121 | `	pGen->pIn++;` |
|  1644931 |  5122 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5123 | `		/* Compile the expression */` |
|  1629365 |  5124 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  1629365 |  5125 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5126 | `			return SXERR_ABORT;` |
|  1629365 |  5127 | `		}else if(rc != SXERR_EMPTY ){` |
|  1629365 |  5128 | `			nRet = 1;` |
|   814680 |  5129 | `		}` |
|   814680 |  5130 | `	}` |
|        - |  5131 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - |  5132 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - |  5133 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - |  5134 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  1644931 |  5135 | `	if( pGen->bInGenerator ){` |
|       32 |  5136 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|       32 |  5137 | `		return SXRET_OK;` |
|        - |  5138 | `	}` |
|        - |  5139 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - |  5140 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - |  5141 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - |  5142 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - |  5143 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  1644903 |  5144 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  1644903 |  5145 | `	return SXRET_OK;` |
|   822468 |  5146 | `}` |
|        - |  5147 | `/*` |
|        - |  5148 | ` * Compile a yield expression.` |
|        - |  5149 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - |  5150 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - |  5151 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - |  5152 | ` */` |
|      384 |  5153 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 |  5154 | `{` |
|        - |  5155 | `	SyToken *pTmp, *pSplit;` |
|      389 |  5156 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      389 |  5157 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - |  5158 | `	sxi32 rc;` |
|      192 |  5159 | `	(void)iCompileFlag;` |
|        - |  5160 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      389 |  5161 | `	pGen->pIn++;` |
|        - |  5162 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - |  5163 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - |  5164 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - |  5165 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - |  5166 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|      384 |  5167 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      227 |  5168 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 |  5169 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 |  5170 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 |  5171 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 |  5172 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5173 | `			return SXERR_ABORT;` |
|        - |  5174 | `		}` |
|       67 |  5175 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  5176 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 |  5177 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - |  5178 | `				"Missing expression after 'yield from'");` |
|      ! 0 |  5179 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5180 | `				return SXERR_ABORT;` |
|        - |  5181 | `			}` |
|      ! 0 |  5182 | `		}` |
|       67 |  5183 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 |  5184 | `		return SXRET_OK;` |
|        - |  5185 | `	}` |
|      327 |  5186 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5187 | `		/* Bare yield — no value */` |
|        3 |  5188 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 |  5189 | `		return SXRET_OK;` |
|        - |  5190 | `	}` |
|        - |  5191 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      325 |  5192 | `	pSplit = 0;` |
|        - |  5193 | `	{` |
|      325 |  5194 | `		SyToken *pCur = pGen->pIn;` |
|      325 |  5195 | `		sxi32 nNest = 0;` |
|      781 |  5196 | `		while( pCur < pGen->pEnd ){` |
|      475 |  5197 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 |  5198 | `				nNest++;` |
|      467 |  5199 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 |  5200 | `				nNest--;` |
|      451 |  5201 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       16 |  5202 | `				pSplit = pCur;` |
|       16 |  5203 | `				break;` |
|        - |  5204 | `			}` |
|      461 |  5205 | `			pCur++;` |
|        5 |  5206 | `		}` |
|        - |  5207 | `	}` |
|      325 |  5208 | `	pTmp = pGen->pEnd;` |
|      325 |  5209 | `	if( pSplit ){` |
|        - |  5210 | `		/* yield $key => $value */` |
|       16 |  5211 | `		pGen->pEnd = pSplit;` |
|       16 |  5212 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5213 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5214 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       16 |  5215 | `		pGen->pEnd = pTmp;` |
|       16 |  5216 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5217 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5218 | `		iP1 = 1;` |
|       16 |  5219 | `		iP2 = 1;` |
|        9 |  5220 | `	}else{` |
|        - |  5221 | `		/* yield $value */` |
|      311 |  5222 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      311 |  5223 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      311 |  5224 | `		if( rc != SXERR_EMPTY ){` |
|      311 |  5225 | `			iP1 = 1;` |
|      153 |  5226 | `		}` |
|        - |  5227 | `	}` |
|      325 |  5228 | `	pGen->pEnd = pTmp;` |
|      325 |  5229 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      325 |  5230 | `	return SXRET_OK;` |
|      197 |  5231 | `}` |
|        - |  5232 | `/*` |
|        - |  5233 | ` * Compile the die/exit language construct.` |
|        - |  5234 | ` * The role of these constructs is to terminate execution of the script.` |
|        - |  5235 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - |  5236 | ` */` |
|      122 |  5237 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 |  5238 | `{` |
|      127 |  5239 | `	sxi32 nExpr = 0;` |
|        - |  5240 | `	sxi32 rc;` |
|        - |  5241 | `	/* Jump the die/exit keyword */` |
|      127 |  5242 | `	pGen->pIn++;` |
|      127 |  5243 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5244 | `		/* Compile the expression */` |
|      127 |  5245 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      127 |  5246 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5247 | `			return SXERR_ABORT;` |
|      127 |  5248 | `		}else if(rc != SXERR_EMPTY ){` |
|      127 |  5249 | `			nExpr = 1;` |
|       61 |  5250 | `		}` |
|       61 |  5251 | `	}` |
|        - |  5252 | `	/* Emit the HALT instruction */` |
|      127 |  5253 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      127 |  5254 | `	return SXRET_OK;` |
|       66 |  5255 | `}` |
|        - |  5256 | `/*` |
|        - |  5257 | ` * Compile the 'echo' language construct.` |
|        - |  5258 | ` */` |
|    17316 |  5259 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 |  5260 | `{` |
|    17321 |  5261 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5262 | `	sxi32 rc;` |
|        - |  5263 | `	/* Jump the 'echo' keyword */` |
|    17321 |  5264 | `	pGen->pIn++;` |
|        - |  5265 | `	/* Compile arguments one after one */` |
|    17321 |  5266 | `	pTmp = pGen->pEnd;` |
|    42483 |  5267 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    25167 |  5268 | `		if( pGen->pIn < pNext ){` |
|    25167 |  5269 | `			pGen->pEnd = pNext;` |
|    25167 |  5270 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    25167 |  5271 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5272 | `				return SXERR_ABORT;` |
|    25167 |  5273 | `			}else if( rc != SXERR_EMPTY ){` |
|        - |  5274 | `				/* Emit the consume instruction */` |
|    25143 |  5275 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    12569 |  5276 | `			}` |
|    12581 |  5277 | `		}` |
|        - |  5278 | `		/* Jump trailing commas */` |
|    33013 |  5279 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     7851 |  5280 | `			pNext++;` |
|        5 |  5281 | `		}` |
|    25167 |  5282 | `		pGen->pIn = pNext;` |
|        5 |  5283 | `	}` |
|        - |  5284 | `	/* Restore token stream */` |
|    17321 |  5285 | `	pGen->pEnd = pTmp;` |
|    17321 |  5286 | `	return SXRET_OK;` |
|     8663 |  5287 | `}` |
|        - |  5288 | `/*` |
|        - |  5289 | ` * Compile the static statement.` |
|        - |  5290 | ` * According to the PHP language reference` |
|        - |  5291 | ` *  Another important feature of variable scoping is the static variable.` |
|        - |  5292 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - |  5293 | ` *  when program execution leaves this scope.` |
|        - |  5294 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - |  5295 | ` * Symisc eXtension.` |
|        - |  5296 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - |  5297 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  5298 | ` *  Example` |
|        - |  5299 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  5300 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  5301 | ` */` |
|       12 |  5302 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        3 |  5303 | `{` |
|        - |  5304 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - |  5305 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - |  5306 | `	GenBlock *pBlock;` |
|        - |  5307 | `	SyString *pName;` |
|        - |  5308 | `	char *zDup;` |
|        - |  5309 | `	sxu32 nLine;` |
|        - |  5310 | `	sxi32 rc;` |
|        - |  5311 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - |  5312 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - |  5313 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|       12 |  5314 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|       10 |  5315 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 |  5316 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 |  5317 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 |  5318 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5319 | `			return SXERR_ABORT;` |
|        3 |  5320 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 |  5321 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 |  5322 | `		}` |
|        3 |  5323 | `		return SXRET_OK;` |
|        - |  5324 | `	}` |
|        - |  5325 | `	/* Jump the static keyword */` |
|       13 |  5326 | `	nLine = pGen->pIn->nLine;` |
|       13 |  5327 | `	pGen->pIn++;` |
|        - |  5328 | `	/* Extract the enclosing function if any */` |
|       13 |  5329 | `	pBlock = pGen->pCurrent;` |
|       23 |  5330 | `	while( pBlock ){` |
|       23 |  5331 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       13 |  5332 | `			break;` |
|        - |  5333 | `		}` |
|        - |  5334 | `		/* Point to the upper block */` |
|       13 |  5335 | `		pBlock = pBlock->pParent;` |
|        3 |  5336 | `	}` |
|       13 |  5337 | `	if( pBlock == 0 ){` |
|        - |  5338 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 |  5339 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5340 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|      ! 0 |  5341 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5342 | `				return SXERR_ABORT;` |
|        - |  5343 | `			}` |
|      ! 0 |  5344 | `			goto Synchronize;` |
|        - |  5345 | `		}` |
|        - |  5346 | `		/* Compile the expression holding the variable */` |
|      ! 0 |  5347 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  5348 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5349 | `			return SXERR_ABORT;` |
|      ! 0 |  5350 | `		}else if( rc != SXERR_EMPTY ){` |
|        - |  5351 | `			/* Emit the POP instruction */` |
|      ! 0 |  5352 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  5353 | `		}` |
|      ! 0 |  5354 | `		return SXRET_OK;` |
|        - |  5355 | `	}` |
|       13 |  5356 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - |  5357 | `	/* Make sure we are dealing with a valid statement */` |
|       13 |  5358 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        8 |  5359 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        3 |  5360 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|        3 |  5361 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5362 | `				return SXERR_ABORT;` |
|        - |  5363 | `			}` |
|        3 |  5364 | `			goto Synchronize;` |
|        - |  5365 | `	}` |
|       10 |  5366 | `	pGen->pIn++;` |
|        - |  5367 | `	/* Extract variable name */` |
|       10 |  5368 | `	pName = &pGen->pIn->sData;` |
|       10 |  5369 | `	pGen->pIn++; /* Jump the var name */` |
|       10 |  5370 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 |  5371 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  5372 | `		goto Synchronize;` |
|        - |  5373 | `	}` |
|        - |  5374 | `	/* Initialize the structure describing the static variable */` |
|       10 |  5375 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       10 |  5376 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - |  5377 | `	/* Duplicate variable name */` |
|       10 |  5378 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       10 |  5379 | `	if( zDup == 0 ){` |
|      ! 0 |  5380 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  5381 | `		return SXERR_ABORT;` |
|        - |  5382 | `	}` |
|       10 |  5383 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - |  5384 | `	/* Check if we have an expression to compile */` |
|       10 |  5385 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - |  5386 | `		SySet *pInstrContainer;` |
|        - |  5387 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - |  5388 | `		 * Static variable can take any complex expression including function` |
|        - |  5389 | `		 * call as their initialization value.` |
|        - |  5390 | `		 * Example:` |
|        - |  5391 | `		 *		static $var = foo(1,4+5,bar());` |
|        - |  5392 | `		 */` |
|       10 |  5393 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - |  5394 | `		/* Swap bytecode container */` |
|       10 |  5395 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       10 |  5396 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - |  5397 | `		/* Compile the expression */` |
|       10 |  5398 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  5399 | `		/* Emit the done instruction */` |
|       10 |  5400 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - |  5401 | `		/* Restore default bytecode container */` |
|       10 |  5402 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        4 |  5403 | `	}` |
|        - |  5404 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       10 |  5405 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       10 |  5406 | `	return SXRET_OK;` |
|        1 |  5407 | `Synchronize:` |
|        - |  5408 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - |  5409 | `	 * statement.` |
|        - |  5410 | `	 */` |
|        5 |  5411 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 |  5412 | `		pGen->pIn++;` |
|        1 |  5413 | `	}` |
|        3 |  5414 | `	return SXRET_OK;` |
|        9 |  5415 | `}` |
|        - |  5416 | `/*` |
|        - |  5417 | ` * Compile the var statement.` |
|        - |  5418 | ` * Symisc Extension:` |
|        - |  5419 | ` *      var statement can be used outside of a class definition.` |
|        - |  5420 | ` */` |
|        4 |  5421 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 |  5422 | `{` |
|        - |  5423 | `	sxu32 nLine;` |
|        - |  5424 | `	sxi32 rc;` |
|        5 |  5425 | `	nLine = pGen->pIn->nLine;` |
|        - |  5426 | `	/* Jump the 'var' keyword */` |
|        5 |  5427 | `	pGen->pIn++;` |
|        5 |  5428 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  5429 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|        - |  5430 | `		/* Synchronize with the first semi-colon */` |
|      ! 0 |  5431 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|      ! 0 |  5432 | `			pGen->pIn++;` |
|      ! 0 |  5433 | `		}` |
|      ! 0 |  5434 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5435 | `			return SXERR_ABORT;` |
|        - |  5436 | `		}` |
|      ! 0 |  5437 | `	}else{` |
|        - |  5438 | `		/* Compile the expression */` |
|        5 |  5439 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        5 |  5440 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5441 | `			return SXERR_ABORT;` |
|        5 |  5442 | `		}else if( rc != SXERR_EMPTY ){` |
|        5 |  5443 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 |  5444 | `		}` |
|        - |  5445 | `	}` |
|        5 |  5446 | `	return SXRET_OK;` |
|        3 |  5447 | `}` |
|        - |  5448 | `/*` |
|        - |  5449 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - |  5450 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - |  5451 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - |  5452 | ` */` |
|        - |  5453 | `/*` |
|        - |  5454 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - |  5455 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - |  5456 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - |  5457 | ` * qualified name and updates the instruction's operand index.` |
|        - |  5458 | ` *` |
|        - |  5459 | ` * Resolution order:` |
|        - |  5460 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - |  5461 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - |  5462 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - |  5463 | ` *` |
|        - |  5464 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - |  5465 | ` * came from an import (step 1) and 0 otherwise.` |
|        - |  5466 | ` * Returns the (possibly new) literal index.` |
|        - |  5467 | ` */` |
|  2959128 |  5468 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 |  5469 | `{` |
|        - |  5470 | `	ph7_value *pLit;` |
|        - |  5471 | `	const char *zLit;` |
|        - |  5472 | `	SyString sQualified;` |
|        - |  5473 | `	sxu32 nLit;` |
|        - |  5474 | `	sxu32 k;` |
|        - |  5475 | `	sxu32 nNewIdx;` |
|        - |  5476 | `	int hasNsSep;` |
|        - |  5477 | `	SyHashEntry *pImport;` |
|        - |  5478 | `	ph7_value *pNew;` |
|  2959133 |  5479 | `	if( pFromImport ){` |
|  2412215 |  5480 | `		*pFromImport = 0;` |
|  1206105 |  5481 | `	}` |
|  2959133 |  5482 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  2959133 |  5483 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 |  5484 | `		return nOrigIdx;` |
|        - |  5485 | `	}` |
|  2959133 |  5486 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  2959133 |  5487 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - |  5488 | `	/* Skip if already qualified (contains backslash) */` |
|  2959133 |  5489 | `	hasNsSep = 0;` |
| 38070359 |  5490 | `	for( k = 0; k < nLit; k++ ){` |
| 35111239 |  5491 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 17555618 |  5492 | `	}` |
|  2959133 |  5493 | `	if( hasNsSep ){` |
|       10 |  5494 | `		return nOrigIdx;` |
|        - |  5495 | `	}` |
|        - |  5496 | `	/* Check use imports first (works even outside namespaces) */` |
|  2959125 |  5497 | `	SyBlobReset(&pGen->sWorker);` |
|  2959125 |  5498 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  2959125 |  5499 | `	if( pImport ){` |
|       41 |  5500 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 |  5501 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 |  5502 | `		if( pFromImport ){` |
|       18 |  5503 | `			*pFromImport = 1;` |
|        8 |  5504 | `		}` |
|       23 |  5505 | `	}else{` |
|  2959089 |  5506 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  2958999 |  5507 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - |  5508 | `		}` |
|        - |  5509 | `		/* Prepend current namespace */` |
|       95 |  5510 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       95 |  5511 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       95 |  5512 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - |  5513 | `	}` |
|        - |  5514 | `	/* Look up or create a new literal for the qualified name */` |
|      131 |  5515 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      131 |  5516 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       57 |  5517 | `		return nNewIdx; /* Already interned */` |
|        - |  5518 | `	}` |
|       79 |  5519 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|       79 |  5520 | `	if( pNew == 0 ){` |
|      ! 0 |  5521 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - |  5522 | `	}` |
|       79 |  5523 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|       79 |  5524 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|       79 |  5525 | `	return nNewIdx;` |
|  1479569 |  5526 | `}` |
|        - |  5527 | `/*` |
|        - |  5528 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - |  5529 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - |  5530 | ` */` |
|   187820 |  5531 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5532 | `{` |
|        - |  5533 | `	SyHashEntry *pImport;` |
|        - |  5534 | `	/* Check use imports first */` |
|   187825 |  5535 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   187825 |  5536 | `	if( pImport ){` |
|       19 |  5537 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       19 |  5538 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       19 |  5539 | `		return;` |
|        - |  5540 | `	}` |
|        - |  5541 | `	/* Prepend current namespace if active */` |
|   187809 |  5542 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        8 |  5543 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  5544 | `		SyBlobAppend(pOut,"\\",1);` |
|        3 |  5545 | `	}` |
|   187809 |  5546 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    93915 |  5547 | `}` |
|        - |  5548 | `/*` |
|        - |  5549 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - |  5550 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - |  5551 | ` * The caller must release pOut when done.` |
|        - |  5552 | ` */` |
|   266026 |  5553 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5554 | `{` |
|   266031 |  5555 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3947 |  5556 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3947 |  5557 | `		SyBlobAppend(pOut,"\\",1);` |
|     1971 |  5558 | `	}` |
|   266031 |  5559 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   266031 |  5560 | `}` |
|        - |  5561 | `/*` |
|        - |  5562 | ` * Compile a namespace statement` |
|        - |  5563 | ` * According to the PHP language reference manual` |
|        - |  5564 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - |  5565 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - |  5566 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - |  5567 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - |  5568 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - |  5569 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - |  5570 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - |  5571 | ` *  programming world.` |
|        - |  5572 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - |  5573 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - |  5574 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - |  5575 | ` *  classes/functions/constants.` |
|        - |  5576 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - |  5577 | ` *  readability of source code.` |
|        - |  5578 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - |  5579 | ` *  Here is an example of namespace syntax in PHP:` |
|        - |  5580 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - |  5581 | ` *       class MyClass {}` |
|        - |  5582 | ` *       function myfunction() {}` |
|        - |  5583 | ` *       const MYCONST = 1;` |
|        - |  5584 | ` *       $a = new MyClass;` |
|        - |  5585 | ` *       $c = new \my\name\MyClass;` |
|        - |  5586 | ` *       $a = strlen('hi');` |
|        - |  5587 | ` *       $d = namespace\MYCONST;` |
|        - |  5588 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - |  5589 | ` *       echo constant($d);` |
|        - |  5590 | ` * NOTE` |
|        - |  5591 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5592 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5593 | ` */` |
|        - |  5594 | `/*` |
|        - |  5595 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - |  5596 | ` */` |
|       14 |  5597 | `static const char * TokenTypeName(sxu32 nType)` |
|        3 |  5598 | `{` |
|       17 |  5599 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 |  5600 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 |  5601 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 |  5602 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 |  5603 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 |  5604 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 |  5605 | `	return "token";` |
|       10 |  5606 | `}` |
|     3990 |  5607 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 |  5608 | `{` |
|        - |  5609 | `	sxu32 nLine;` |
|        - |  5610 | `	sxi32 rc;` |
|     3995 |  5611 | `	nLine = pGen->pIn->nLine;` |
|     3995 |  5612 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - |  5613 | `	/* Reset namespace and clear previous use imports */` |
|     3995 |  5614 | `	SyBlobReset(&pGen->sNamespace);` |
|     3995 |  5615 | `	SyHashRelease(&pGen->hUseImports);` |
|     3995 |  5616 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5617 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     3995 |  5618 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5619 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     3995 |  5620 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5621 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5622 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 |  5623 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5624 | `		return SXRET_OK;` |
|        - |  5625 | `	}` |
|     3995 |  5626 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - |  5627 | `		/* namespace; — switch to global namespace */` |
|      ! 0 |  5628 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5629 | `		return SXRET_OK;` |
|        - |  5630 | `	}` |
|     3995 |  5631 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - |  5632 | `		/* namespace { } — global namespace block */` |
|      ! 0 |  5633 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5634 | `		return SXRET_OK;` |
|        - |  5635 | `	}` |
|        - |  5636 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8027 |  5637 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4037 |  5638 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - |  5639 | `			/* Append backslash separator */` |
|       26 |  5640 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       26 |  5641 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       11 |  5642 | `			}` |
|       15 |  5643 | `		}else{` |
|        - |  5644 | `			/* Append identifier */` |
|     4015 |  5645 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  5646 | `		}` |
|     4037 |  5647 | `		pGen->pIn++;` |
|        5 |  5648 | `	}` |
|        - |  5649 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - |  5650 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - |  5651 | `	{` |
|     3995 |  5652 | `		char *zNsDup = 0;` |
|     3995 |  5653 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     5987 |  5654 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3988 |  5655 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     1994 |  5656 | `		}` |
|     3995 |  5657 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - |  5658 | `	}` |
|     3995 |  5659 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 |  5660 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  5661 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 |  5662 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 |  5663 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5664 | `			return SXERR_ABORT;` |
|        - |  5665 | `		}` |
|        2 |  5666 | `	}` |
|     3995 |  5667 | `	return SXRET_OK;` |
|     2000 |  5668 | `}` |
|        - |  5669 | `/*` |
|        - |  5670 | ` * Compile the 'use' statement` |
|        - |  5671 | ` * According to the PHP language reference manual` |
|        - |  5672 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - |  5673 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - |  5674 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - |  5675 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - |  5676 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - |  5677 | ` *  a function or constant is not supported.` |
|        - |  5678 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - |  5679 | ` * NOTE` |
|        - |  5680 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5681 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5682 | ` */` |
|       72 |  5683 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 |  5684 | `{` |
|        - |  5685 | `	sxu32 nLine;` |
|        - |  5686 | `	sxi32 rc;` |
|        - |  5687 | `	SyBlob sPath;` |
|        - |  5688 | `	SyString sAlias;` |
|        - |  5689 | `	SyToken *pLast;` |
|        - |  5690 | `	char *zDup;` |
|        - |  5691 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - |  5692 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - |  5693 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       77 |  5694 | `	nLine = pGen->pIn->nLine;` |
|       77 |  5695 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - |  5696 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       77 |  5697 | `	iUseType = 0;` |
|       77 |  5698 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 |  5699 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 |  5700 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 |  5701 | `			iUseType = 1;` |
|       16 |  5702 | `			pGen->pIn++;` |
|       23 |  5703 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 |  5704 | `			iUseType = 2;` |
|       16 |  5705 | `			pGen->pIn++;` |
|        7 |  5706 | `		}` |
|       14 |  5707 | `	}` |
|        - |  5708 | `	/* Select target hash tables based on import type */` |
|       77 |  5709 | `	switch( iUseType ){` |
|        7 |  5710 | `		case 1:` |
|       16 |  5711 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 |  5712 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 |  5713 | `			break;` |
|        7 |  5714 | `		case 2:` |
|       16 |  5715 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 |  5716 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 |  5717 | `			break;` |
|       22 |  5718 | `		default:` |
|       49 |  5719 | `			pGenHash = &pGen->hUseImports;` |
|       49 |  5720 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       44 |  5721 | `			break;` |
|        - |  5722 | `	}` |
|       77 |  5723 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - |  5724 | `	/* Process one or more use declarations separated by commas */` |
|       37 |  5725 | `	for(;;){` |
|       79 |  5726 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  5727 | `			break;` |
|        - |  5728 | `		}` |
|       79 |  5729 | `		SyBlobReset(&sPath);` |
|       79 |  5730 | `		pLast = 0;` |
|        - |  5731 | `		/* Collect the full namespace path */` |
|      269 |  5732 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      195 |  5733 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      135 |  5734 | `				pLast = pGen->pIn;` |
|      135 |  5735 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       65 |  5736 | `					SyBlobAppend(&sPath,"\\",1);` |
|       30 |  5737 | `				}` |
|      135 |  5738 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       65 |  5739 | `			}` |
|      195 |  5740 | `			pGen->pIn++;` |
|        5 |  5741 | `		}` |
|       79 |  5742 | `		if( pLast == 0 ){` |
|        - |  5743 | `			/* Empty path */` |
|        6 |  5744 | `			break;` |
|        - |  5745 | `		}` |
|        - |  5746 | `		/* Default alias is the last component of the path */` |
|       75 |  5747 | `		sAlias = pLast->sData;` |
|        - |  5748 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       70 |  5749 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       50 |  5750 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       24 |  5751 | `			pGen->pIn++; /* Jump 'as' */` |
|       24 |  5752 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       24 |  5753 | `				sAlias = pGen->pIn->sData;` |
|       24 |  5754 | `				pGen->pIn++;` |
|       10 |  5755 | `			}` |
|       10 |  5756 | `		}` |
|        - |  5757 | `		/* Check for duplicate import alias (per-type) */` |
|       75 |  5758 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 |  5759 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  5760 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 |  5761 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 |  5762 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5763 | `				SyBlobRelease(&sPath);` |
|      ! 0 |  5764 | `				return SXERR_ABORT;` |
|        - |  5765 | `			}` |
|        2 |  5766 | `		}` |
|        - |  5767 | `		/* Register the import: alias -> FQN.` |
|        - |  5768 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - |  5769 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - |  5770 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      110 |  5771 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       70 |  5772 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       75 |  5773 | `		if( zDup ){` |
|       75 |  5774 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       75 |  5775 | `			if( pVmHash ){` |
|        - |  5776 | `				/* Class imports: populate VM table directly (class resolution` |
|        - |  5777 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       47 |  5778 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       47 |  5779 | `				if( zAliasDup ){` |
|       47 |  5780 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       21 |  5781 | `				}` |
|       21 |  5782 | `			}` |
|       75 |  5783 | `			if( iUseType == 2 ){` |
|        - |  5784 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - |  5785 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 |  5786 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 |  5787 | `				if( zAliasDup ){` |
|        - |  5788 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - |  5789 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - |  5790 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 |  5791 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 |  5792 | `					if( azPair ){` |
|       16 |  5793 | `						azPair[0] = zAliasDup;` |
|       16 |  5794 | `						azPair[1] = zDup;` |
|       16 |  5795 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 |  5796 | `					}` |
|        7 |  5797 | `				}` |
|        7 |  5798 | `			}` |
|       35 |  5799 | `		}` |
|        - |  5800 | `		/* Check for comma (multiple use declarations) */` |
|       75 |  5801 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 |  5802 | `			pGen->pIn++;` |
|        2 |  5803 | `		}else{` |
|       39 |  5804 | `			break;` |
|        - |  5805 | `		}` |
|        1 |  5806 | `	}` |
|       77 |  5807 | `	SyBlobRelease(&sPath);` |
|       77 |  5808 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 |  5809 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 |  5810 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 |  5811 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5812 | `			return SXERR_ABORT;` |
|        - |  5813 | `		}` |
|        1 |  5814 | `	}` |
|       77 |  5815 | `	return SXRET_OK;` |
|       41 |  5816 | `}` |
|        - |  5817 | `/*` |
|        - |  5818 | ` * Compile the stupid 'declare' language construct.` |
|        - |  5819 | ` *` |
|        - |  5820 | ` * According to the PHP language reference manual.` |
|        - |  5821 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - |  5822 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - |  5823 | ` *  declare (directive)` |
|        - |  5824 | ` *   statement` |
|        - |  5825 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - |  5826 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - |  5827 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - |  5828 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - |  5829 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - |  5830 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - |  5831 | ` * <?php` |
|        - |  5832 | ` * // these are the same:` |
|        - |  5833 | ` * // you can use this:` |
|        - |  5834 | ` * declare(ticks=1) {` |
|        - |  5835 | ` *   // entire script here` |
|        - |  5836 | ` * }` |
|        - |  5837 | ` * // or you can use this:` |
|        - |  5838 | ` * declare(ticks=1);` |
|        - |  5839 | ` * // entire script here` |
|        - |  5840 | ` * ?>` |
|        - |  5841 | ` *` |
|        - |  5842 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - |  5843 | ` */` |
|        - |  5844 | `/*` |
|        - |  5845 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - |  5846 | ` */` |
|       72 |  5847 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 |  5848 | `{` |
|      109 |  5849 | `	return SyStringLength(pName) == nWant` |
|       72 |  5850 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 |  5851 | `}` |
|        - |  5852 |  |
|       42 |  5853 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 |  5854 | `{` |
|       47 |  5855 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       47 |  5856 | `	SyToken *pBodyEnd = 0;` |
|        - |  5857 | `	SyToken *pBodyStart;` |
|        - |  5858 | `	SyToken *pCursor;` |
|        - |  5859 | `	int bHasStrictTypes;` |
|        - |  5860 | `	int bBlockForm;` |
|        - |  5861 | `	int bPlacementOk;` |
|        - |  5862 | `	sxi32 rc;` |
|       47 |  5863 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       47 |  5864 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 |  5865 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|        6 |  5866 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5867 | `			return SXERR_ABORT;` |
|        - |  5868 | `		}` |
|        6 |  5869 | `		goto Synchro;` |
|        - |  5870 | `	}` |
|       43 |  5871 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       43 |  5872 | `	pBodyStart = pGen->pIn;` |
|        - |  5873 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       43 |  5874 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       43 |  5875 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  5876 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|      ! 0 |  5877 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5878 | `			return SXERR_ABORT;` |
|        - |  5879 | `		}` |
|      ! 0 |  5880 | `		return SXRET_OK;` |
|        - |  5881 | `	}` |
|        - |  5882 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - |  5883 | `	 * now delimits the comma-separated directive list. */` |
|       43 |  5884 | `	pGen->pIn = &pBodyEnd[1];` |
|       43 |  5885 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 |  5886 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 |  5887 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5888 | `			return SXERR_ABORT;` |
|        - |  5889 | `		}` |
|      ! 0 |  5890 | `	}` |
|       43 |  5891 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       43 |  5892 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       43 |  5893 | `	bHasStrictTypes = 0;` |
|        - |  5894 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - |  5895 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - |  5896 | `	 * directive appears anywhere in the list, before validating values. */` |
|       43 |  5897 | `	pCursor = pBodyStart;` |
|       55 |  5898 | `	while( pCursor < pBodyEnd ){` |
|       51 |  5899 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       43 |  5900 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       39 |  5901 | `				bHasStrictTypes = 1;` |
|       39 |  5902 | `				break;` |
|        - |  5903 | `			}` |
|        2 |  5904 | `		}` |
|       14 |  5905 | `		pCursor++;` |
|        2 |  5906 | `	}` |
|       43 |  5907 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 |  5908 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5909 | `			"strict_types declaration must not use block mode");` |
|        3 |  5910 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5911 | `		return SXRET_OK;` |
|        - |  5912 | `	}` |
|       41 |  5913 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 |  5914 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5915 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 |  5916 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 |  5917 | `		return SXRET_OK;` |
|        - |  5918 | `	}` |
|        - |  5919 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       37 |  5920 | `	pCursor = pBodyStart;` |
|       69 |  5921 | `	while( pCursor < pBodyEnd ){` |
|        - |  5922 | `		SyToken *pNameTok;` |
|        - |  5923 | `		SyToken *pEqTok;` |
|        - |  5924 | `		SyToken *pValTok;` |
|        - |  5925 | `		SyString *pDirName;` |
|        - |  5926 | `		int bIsStrict;` |
|        - |  5927 | `		int iStrictValue;` |
|       39 |  5928 | `		pNameTok = pCursor;` |
|       39 |  5929 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  5930 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5931 | `				"declare: Expecting a directive name");` |
|      ! 0 |  5932 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5933 | `			return SXRET_OK;` |
|        - |  5934 | `		}` |
|       39 |  5935 | `		pEqTok = pNameTok + 1;` |
|       39 |  5936 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 |  5937 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5938 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 |  5939 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5940 | `			return SXRET_OK;` |
|        - |  5941 | `		}` |
|       39 |  5942 | `		pValTok = pEqTok + 1;` |
|       39 |  5943 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 |  5944 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5945 | `				"declare: Expecting value after '='");` |
|      ! 0 |  5946 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5947 | `			return SXRET_OK;` |
|        - |  5948 | `		}` |
|       39 |  5949 | `		pDirName = &pNameTok->sData;` |
|       39 |  5950 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       39 |  5951 | `		if( bIsStrict ){` |
|        - |  5952 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - |  5953 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       35 |  5954 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 |  5955 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5956 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 |  5957 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5958 | `				return SXRET_OK;` |
|        - |  5959 | `			}` |
|       35 |  5960 | `			iStrictValue = -1;` |
|       35 |  5961 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       35 |  5962 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       35 |  5963 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       35 |  5964 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       33 |  5965 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       15 |  5966 | `			}` |
|       35 |  5967 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 |  5968 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5969 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 |  5970 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5971 | `				return SXRET_OK;` |
|        - |  5972 | `			}` |
|       32 |  5973 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       18 |  5974 | `		}else{` |
|        - |  5975 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|        - |  5976 | `			 * preserve the legacy notice so callers relying on the old` |
|        - |  5977 | `			 * behavior don't regress. */` |
|        8 |  5978 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|        - |  5979 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|        2 |  5980 | `				ph7_lib_version()` |
|        - |  5981 | `				);` |
|        - |  5982 | `		}` |
|       36 |  5983 | `		pCursor = pValTok + 1;` |
|        - |  5984 | `		/* Consume separating comma (or end). */` |
|       36 |  5985 | `		if( pCursor < pBodyEnd ){` |
|        3 |  5986 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  5987 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5988 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 |  5989 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5990 | `				return SXRET_OK;` |
|        - |  5991 | `			}` |
|        3 |  5992 | `			pCursor++;` |
|        1 |  5993 | `		}` |
|        4 |  5994 | `	}` |
|        - |  5995 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - |  5996 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - |  5997 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       34 |  5998 | `	return SXRET_OK;` |
|        2 |  5999 | `Synchro:` |
|        - |  6000 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 |  6001 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 |  6002 | `		pGen->pIn++;` |
|        2 |  6003 | `	}` |
|        6 |  6004 | `	return SXRET_OK;` |
|       26 |  6005 | `}` |
|        - |  6006 | `/*` |
|        - |  6007 | ` * Process default argument values. That is,a function may define C++-style default value` |
|        - |  6008 | ` * as follows:` |
|        - |  6009 | ` * function makecoffee($type = "cappuccino")` |
|        - |  6010 | ` * {` |
|        - |  6011 | ` *   return "Making a cup of $type.\n";` |
|        - |  6012 | ` * }` |
|        - |  6013 | ` * Symisc eXtension.` |
|        - |  6014 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|        - |  6015 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|        - |  6016 | ` *      Example: Work only with PH7,generate error under zend` |
|        - |  6017 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|        - |  6018 | ` *      {` |
|        - |  6019 | ` *       var_dump($a);` |
|        - |  6020 | ` *      }` |
|        - |  6021 | ` *     //call test without args` |
|        - |  6022 | ` *      test();` |
|        - |  6023 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|        - |  6024 | ` *      Example:` |
|        - |  6025 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|        - |  6026 | ` * 3 -) Function overloading!!` |
|        - |  6027 | ` *      Example:` |
|        - |  6028 | ` *      function foo($a) {` |
|        - |  6029 | ` *   	  return $a.PHP_EOL;` |
|        - |  6030 | ` *	    }` |
|        - |  6031 | ` *	    function foo($a, $b) {` |
|        - |  6032 | ` *   	  return $a + $b;` |
|        - |  6033 | ` *	    }` |
|        - |  6034 | ` *	    echo foo(5); // Prints "5"` |
|        - |  6035 | ` *	    echo foo(5, 2); // Prints "7"` |
|        - |  6036 | ` *      // Same arg` |
|        - |  6037 | ` *	   function foo(string $a)` |
|        - |  6038 | ` *	   {` |
|        - |  6039 | ` *	     echo "a is a string\n";` |
|        - |  6040 | ` *	     var_dump($a);` |
|        - |  6041 | ` *	   }` |
|        - |  6042 | ` *	  function foo(int $a)` |
|        - |  6043 | ` *	  {` |
|        - |  6044 | ` *	    echo "a is integer\n";` |
|        - |  6045 | ` *	    var_dump($a);` |
|        - |  6046 | ` *	  }` |
|        - |  6047 | ` *	  function foo(array $a)` |
|        - |  6048 | ` *	  {` |
|        - |  6049 | ` * 	    echo "a is an array\n";` |
|        - |  6050 | ` * 	    var_dump($a);` |
|        - |  6051 | ` *	  }` |
|        - |  6052 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|        - |  6053 | ` *	  foo(52); // a is integer [second foo]` |
|        - |  6054 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|        - |  6055 | ` * Please refer to the official documentation for more information on the powerful extension` |
|        - |  6056 | ` * introduced by the PH7 engine.` |
|        - |  6057 | ` */` |
|   240966 |  6058 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |  6059 | `{` |
|        - |  6060 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6061 | `	SySet *pInstrContainer;` |
|        - |  6062 | `	sxi32 rc;` |
|        - |  6063 | `	/* Swap token stream */` |
|   240971 |  6064 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   240971 |  6065 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   240971 |  6066 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |  6067 | `	/* Compile the expression holding the argument value */` |
|   240971 |  6068 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  6069 | `	/* Emit the done instruction */` |
|   240971 |  6070 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   240971 |  6071 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   240971 |  6072 | `	RE_SWAP_DELIMITER(pGen);` |
|   240971 |  6073 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  6074 | `		return SXERR_ABORT;` |
|        - |  6075 | `	}` |
|   240971 |  6076 | `	return SXRET_OK;` |
|   120488 |  6077 | `}` |
|        - |  6078 | `/*` |
|        - |  6079 | ` * Collect function arguments one after one.` |
|        - |  6080 | ` * According to the PHP language reference manual.` |
|        - |  6081 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|        - |  6082 | ` * list of expressions.` |
|        - |  6083 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|        - |  6084 | ` * and default argument values. Variable-length argument lists are also supported,` |
|        - |  6085 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|        - |  6086 | ` * for more information.` |
|        - |  6087 | ` * Example #1 Passing arrays to functions` |
|        - |  6088 | ` * <?php` |
|        - |  6089 | ` * function takes_array($input)` |
|        - |  6090 | ` * {` |
|        - |  6091 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|        - |  6092 | ` * }` |
|        - |  6093 | ` * ?>` |
|        - |  6094 | ` * Making arguments be passed by reference` |
|        - |  6095 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|        - |  6096 | ` * within the function is changed, it does not get changed outside of the function).` |
|        - |  6097 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|        - |  6098 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|        - |  6099 | ` * to the argument name in the function definition:` |
|        - |  6100 | ` * Example #2 Passing function parameters by reference` |
|        - |  6101 | ` * <?php` |
|        - |  6102 | ` * function add_some_extra(&$string)` |
|        - |  6103 | ` * {` |
|        - |  6104 | ` *   $string .= 'and something extra.';` |
|        - |  6105 | ` * }` |
|        - |  6106 | ` * $str = 'This is a string, ';` |
|        - |  6107 | ` * add_some_extra($str);` |
|        - |  6108 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|        - |  6109 | ` * ?>` |
|        - |  6110 | ` *` |
|        - |  6111 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|        - |  6112 | ` * complex agrument values.Please refer to the official documentation for more information` |
|        - |  6113 | ` * on these extension.` |
|        - |  6114 | ` */` |
|   499028 |  6115 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  6116 | `{` |
|        - |  6117 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  6118 | `	SyToken *pIn;  /* Token stream */` |
|        - |  6119 | `	SyBlob sSig;         /* Function signature */` |
|        - |  6120 | `	char *zDup;          /* Copy of argument name */` |
|        - |  6121 | `	sxi32 rc;` |
|        - |  6122 |  |
|   499033 |  6123 | `	pIn = pGen->pIn;` |
|   499033 |  6124 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  6125 | `	/* Process arguments one after one */` |
|   612124 |  6126 | `	for(;;){` |
|  1224253 |  6127 | `		if( pIn >= pEnd ){` |
|        - |  6128 | `			/* No more arguments to process */` |
|   499017 |  6129 | `			break;` |
|        - |  6130 | `		}` |
|   725241 |  6131 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   725241 |  6132 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   725241 |  6133 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   725241 |  6134 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   725241 |  6135 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  6136 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  6137 | `		 * first token inside the main token stream */` |
|   725241 |  6138 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  6139 | `			return SXERR_ABORT;` |
|        - |  6140 | `		}` |
|        - |  6141 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|        - |  6142 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|        - |  6143 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|        - |  6144 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|        - |  6145 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|        - |  6146 | `		{` |
|   725241 |  6147 | `			int bReadonly = 0, bVisSeen = 0;` |
|   725241 |  6148 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   725241 |  6149 | `			sxi32 iSetVisFlag = 0;` |
|        - |  6150 | `			int nSetTok;` |
|        - |  6151 | `			sxi32 nSetVis;` |
|   725241 |  6152 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        3 |  6153 | `				bReadonly = 1;` |
|        3 |  6154 | `				pIn++;` |
|        1 |  6155 | `			}` |
|   725241 |  6156 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   725241 |  6157 | `			if( nSetVis ){` |
|        - |  6158 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|        3 |  6159 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        3 |  6160 | `				bVisSeen = 1;` |
|        3 |  6161 | `				pIn += nSetTok;` |
|        3 |  6162 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      ! 0 |  6163 | `					bReadonly = 1;` |
|      ! 0 |  6164 | `					pIn++;` |
|        1 |  6165 | `				}` |
|   725240 |  6166 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    89729 |  6167 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    89729 |  6168 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|       89 |  6169 | `					bVisSeen = 1;` |
|       89 |  6170 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      120 |  6171 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|       39 |  6172 | `						: PH7_CLASS_PROT_PUBLIC;` |
|       89 |  6173 | `					pIn++;` |
|       89 |  6174 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|       89 |  6175 | `					if( nSetVis ){` |
|        - |  6176 | ``						/* `public private(set) T $x` promoted form */`` |
|        3 |  6177 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        3 |  6178 | `						pIn += nSetTok;` |
|        1 |  6179 | `					}` |
|       89 |  6180 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       18 |  6181 | `						bReadonly = 1;` |
|       18 |  6182 | `						pIn++;` |
|        7 |  6183 | `					}` |
|       42 |  6184 | `				}` |
|    44862 |  6185 | `			}` |
|   725241 |  6186 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        5 |  6187 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   725239 |  6188 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|      ! 0 |  6189 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|      ! 0 |  6190 | `			}` |
|   725241 |  6191 | `			if( bVisSeen \|\| bReadonly ){` |
|       93 |  6192 | `				if( !bCtorCtx ){` |
|        6 |  6193 | `					if( bAbstractCtx ){` |
|        3 |  6194 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6195 | `							"Cannot declare promoted property in an abstract constructor");` |
|        2 |  6196 | `					}else{` |
|        3 |  6197 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6198 | `							"Cannot declare promoted property outside a constructor");` |
|        - |  6199 | `					}` |
|        6 |  6200 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  6201 | `						return SXERR_ABORT;` |
|        - |  6202 | `					}` |
|        6 |  6203 | `					return SXERR_SYNTAX;` |
|        - |  6204 | `				}` |
|       89 |  6205 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|       89 |  6206 | `				sArg.iPromoteVis = iVis;` |
|       89 |  6207 | `				if( bReadonly ){` |
|       20 |  6208 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|        8 |  6209 | `				}` |
|       42 |  6210 | `			}` |
|        - |  6211 | `		}` |
|        - |  6212 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   725232 |  6213 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   427014 |  6214 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   126843 |  6215 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   105383 |  6216 | `			sxu32 nLineLocal = pIn->nLine;` |
|   105383 |  6217 | `			sxi32 iTFlags = 0;` |
|   105383 |  6218 | `			pGen->pIn = pIn;` |
|   105383 |  6219 | `			rc = GenStateParseUnionTypeDecl(` |
|    52689 |  6220 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|    52689 |  6221 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  6222 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  6223 | `				/* bAllowVoid */ 0,` |
|    52689 |  6224 | `						nLineLocal);` |
|   105383 |  6225 | `			pIn = pGen->pIn;` |
|   105383 |  6226 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  6227 | `				return SXERR_ABORT;` |
|   105383 |  6228 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  6229 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  6230 | `				return SXERR_SYNTAX;` |
|   105381 |  6231 | `			}else if( rc == SXERR_SYNTAX ){` |
|       12 |  6232 | `				if( pIn < pEnd ){` |
|       16 |  6233 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  6234 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        4 |  6235 | `						&pIn->sData);` |
|        8 |  6236 | `				}else{` |
|      ! 0 |  6237 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  6238 | `						"syntax error, unexpected end of file");` |
|        - |  6239 | `				}` |
|       12 |  6240 | `				return SXERR_SYNTAX;` |
|        - |  6241 | `			}` |
|   105373 |  6242 | `			sArg.iFlags \|= iTFlags;` |
|    52684 |  6243 | `		}` |
|   725227 |  6244 | `		if( pIn >= pEnd ){` |
|      ! 0 |  6245 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  6246 | `			return rc;` |
|        - |  6247 | `		}` |
|   725227 |  6248 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  6249 | `			/* Pass by reference,record that */` |
|     3929 |  6250 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     3929 |  6251 | `			pIn++;` |
|     1962 |  6252 | `		}` |
|   725227 |  6253 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  6254 | `			/* Variadic parameter: ...$args */` |
|    19529 |  6255 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    19529 |  6256 | `			pIn++;` |
|     9762 |  6257 | `		}` |
|   725227 |  6258 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  6259 | `			/* Invalid argument */` |
|      ! 0 |  6260 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|      ! 0 |  6261 | `			return rc;` |
|        - |  6262 | `		}` |
|   725227 |  6263 | `		pIn++; /* Jump the dollar sign */` |
|        - |  6264 | `		/* Copy argument name */` |
|   725227 |  6265 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   725227 |  6266 | `		if( zDup == 0 ){` |
|      ! 0 |  6267 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  6268 | `			return SXERR_ABORT;` |
|        - |  6269 | `		}` |
|   725227 |  6270 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   725227 |  6271 | `		pIn++;` |
|   725227 |  6272 | `		if( pIn < pEnd ){` |
|   373921 |  6273 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  6274 | `				SyToken *pDefend;` |
|   240973 |  6275 | `				sxi32 iNest = 0;` |
|   240973 |  6276 | `				pIn++; /* Jump the equal sign */` |
|   240973 |  6277 | `				pDefend = pIn;` |
|        - |  6278 | `				/* Process the default value associated with this argument */` |
|   513039 |  6279 | `				while( pDefend < pEnd ){` |
|   365337 |  6280 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    93271 |  6281 | `						break;` |
|        - |  6282 | `					}` |
|   272071 |  6283 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  6284 | `						/* Increment nesting level */` |
|    15549 |  6285 | `						iNest++;` |
|   264299 |  6286 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  6287 | `						/* Decrement nesting level */` |
|    15549 |  6288 | `						iNest--;` |
|     7772 |  6289 | `					}` |
|   272071 |  6290 | `					pDefend++;` |
|        5 |  6291 | `				}` |
|   240973 |  6292 | `				if( pIn >= pDefend ){` |
|        3 |  6293 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|        3 |  6294 | `					return rc;` |
|        - |  6295 | `				}` |
|        - |  6296 | `				/* Process default value */` |
|   240971 |  6297 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   240971 |  6298 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  6299 | `					return rc;` |
|        - |  6300 | `				}` |
|        - |  6301 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  6302 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  6303 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  6304 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  6305 | `				 * arg-type check lets null through. */` |
|   240966 |  6306 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|   145752 |  6307 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|   145749 |  6308 | `					&& &pIn[1] == pDefend` |
|    46647 |  6309 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|    34978 |  6310 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|    21373 |  6311 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|    15547 |  6312 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|     7771 |  6313 | `				}` |
|        - |  6314 | `				/* Point beyond the default value */` |
|   240971 |  6315 | `				pIn = pDefend;` |
|   120483 |  6316 | `			}` |
|   373919 |  6317 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  6318 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  6319 | `				return rc;` |
|        - |  6320 | `			}` |
|   373919 |  6321 | `			pIn++; /* Jump the trailing comma */` |
|   186957 |  6322 | `		}` |
|        - |  6323 | `		/* Append argument signature */` |
|   725225 |  6324 | `		if( sArg.nType > 0 ){` |
|   105311 |  6325 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  6326 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    15621 |  6327 | `				int marker = 'o';` |
|    15621 |  6328 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    15621 |  6329 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     7813 |  6330 | `			}else{` |
|        - |  6331 | `				int c;` |
|    89695 |  6332 | `				c = 'n'; /* cc warning */` |
|        - |  6333 | `				/* Type leading character */` |
|    89695 |  6334 | `				switch(sArg.nType){` |
|     5832 |  6335 | `				case MEMOBJ_HASHMAP:` |
|        - |  6336 | `					/* Hashmap aka 'array' */` |
|    11669 |  6337 | `					c = 'h';` |
|    11669 |  6338 | `					break;` |
|     9829 |  6339 | `				case MEMOBJ_INT:` |
|        - |  6340 | `					/* Integer */` |
|    19663 |  6341 | `					c = 'i';` |
|    19663 |  6342 | `					break;` |
|        2 |  6343 | `				case MEMOBJ_BOOL:` |
|        - |  6344 | `					/* Bool */` |
|        5 |  6345 | `					c = 'b';` |
|        5 |  6346 | `					break;` |
|        5 |  6347 | `				case MEMOBJ_REAL:` |
|        - |  6348 | `					/* Float */` |
|       12 |  6349 | `					c = 'f';` |
|       12 |  6350 | `					break;` |
|    29169 |  6351 | `				case MEMOBJ_STRING:` |
|        - |  6352 | `					/* String */` |
|    58343 |  6353 | `					c = 's';` |
|    58343 |  6354 | `					break;` |
|        7 |  6355 | `				case MEMOBJ_OBJ:` |
|        - |  6356 | `					/* Object */` |
|       16 |  6357 | `					c = 'o';` |
|       14 |  6358 | `					break;` |
|        1 |  6359 | `				default:` |
|        2 |  6360 | `					break;` |
|        - |  6361 | `				}` |
|    89695 |  6362 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  6363 | `			}` |
|    52658 |  6364 | `		}else{` |
|        - |  6365 | `			/* No type is associated with this parameter which mean` |
|        - |  6366 | `			 * that this function is not condidate for overloading.` |
|        - |  6367 | `			 */` |
|   619919 |  6368 | `			SyBlobRelease(&sSig);` |
|        - |  6369 | `		}` |
|        - |  6370 | `		/* Save in the argument set */` |
|   725225 |  6371 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  6372 | `	}` |
|   499017 |  6373 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  6374 | `		/* Save function signature */` |
|    74167 |  6375 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|    37081 |  6376 | `	}` |
|   499017 |  6377 | `	return SXRET_OK;` |
|   249519 |  6378 | `}` |
|        - |  6379 | `/*` |
|        - |  6380 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  6381 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  6382 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  6383 | ` */` |
|    34998 |  6384 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  6385 | `{` |
|    35003 |  6386 | `	sxi32 iParen = 0;` |
|    35003 |  6387 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  6388 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  6389 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  6390 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|   155593 |  6391 | `	while( pIn < pEnd ){` |
|   155593 |  6392 | `		sxu32 t = pIn->nType;` |
|   155593 |  6393 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|   151655 |  6394 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|   104993 |  6395 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|    85531 |  6396 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|   120595 |  6397 | `		pIn++;` |
|        5 |  6398 | `	}` |
|    19467 |  6399 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  6400 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  6401 | `	{` |
|    19467 |  6402 | `		sxi32 d = 0;` |
|   773341 |  6403 | `		while( pIn < pEnd ){` |
|   773341 |  6404 | `			sxu32 t = pIn->nType;` |
|   773341 |  6405 | `			if( t & PH7_TK_OCB ){ d++; }` |
|   742223 |  6406 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|   753879 |  6407 | `			pIn++;` |
|        5 |  6408 | `		}` |
|        - |  6409 | `	}` |
|    19467 |  6410 | `	return pIn;` |
|    17504 |  6411 | `}` |
|        - |  6412 | `/*` |
|        - |  6413 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  6414 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  6415 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  6416 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  6417 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  6418 | ` * detached-mini-program path untouched.` |
|        - |  6419 | ` */` |
|        - |  6420 | `/*` |
|        - |  6421 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  6422 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  6423 | ` * mixed, object.` |
|        - |  6424 | ` */` |
|       28 |  6425 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        3 |  6426 | `{` |
|        - |  6427 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  6428 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  6429 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  6430 | `	};` |
|        - |  6431 | `	sxu32 i;` |
|       31 |  6432 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  6433 | `		zName++;` |
|      ! 0 |  6434 | `		nName--;` |
|      ! 0 |  6435 | `	}` |
|       63 |  6436 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|       59 |  6437 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|       27 |  6438 | `			return 1;` |
|        - |  6439 | `		}` |
|       17 |  6440 | `	}` |
|        5 |  6441 | `	return 0;` |
|       17 |  6442 | `}` |
|        - |  6443 | `/*` |
|        - |  6444 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  6445 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  6446 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  6447 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  6448 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  6449 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  6450 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  6451 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  6452 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  6453 | ` * both rather than fatal on valid code (a recorded divergence).` |
|        - |  6454 | ` */` |
|       26 |  6455 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        4 |  6456 | `{` |
|       30 |  6457 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  6458 | ``		return 1; /* bare `object` */`` |
|        - |  6459 | `	}` |
|       30 |  6460 | `	if( nType != SXU32_HIGH ){` |
|        3 |  6461 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  6462 | `	}` |
|       27 |  6463 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|       23 |  6464 | `		return 1;` |
|        - |  6465 | `	}` |
|        - |  6466 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  6467 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  6468 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  6469 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  6470 | `	{` |
|        - |  6471 | `		SyBlob sFQN;` |
|        - |  6472 | `		int bOk;` |
|        5 |  6473 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        5 |  6474 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|        5 |  6475 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|        5 |  6476 | `		SyBlobRelease(&sFQN);` |
|        5 |  6477 | `		return bOk;` |
|        - |  6478 | `	}` |
|       17 |  6479 | `}` |
|        - |  6480 | `/*` |
|        - |  6481 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  6482 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  6483 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  6484 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  6485 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  6486 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  6487 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  6488 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  6489 | ` */` |
|      264 |  6490 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  6491 | `{` |
|      269 |  6492 | `	int bOk = 0;` |
|        - |  6493 | `	sxu32 nLine;` |
|        - |  6494 | `	sxi32 rc;` |
|      269 |  6495 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      243 |  6496 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  6497 | `	}` |
|       30 |  6498 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  6499 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  6500 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  6501 | `		sxu32 i,j;` |
|      ! 0 |  6502 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  6503 | `			int bGroupOk;` |
|      ! 0 |  6504 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  6505 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  6506 | `			}` |
|      ! 0 |  6507 | `			bGroupOk = 1;` |
|      ! 0 |  6508 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  6509 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  6510 | `					bGroupOk = 0;` |
|      ! 0 |  6511 | `					break;` |
|        - |  6512 | `				}` |
|      ! 0 |  6513 | `			}` |
|      ! 0 |  6514 | `			bOk = bGroupOk;` |
|      ! 0 |  6515 | `		}` |
|      ! 0 |  6516 | `	}else{` |
|       30 |  6517 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  6518 | `	}` |
|       30 |  6519 | `	if( bOk ){` |
|       27 |  6520 | `		return SXRET_OK;` |
|        - |  6521 | `	}` |
|        - |  6522 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  6523 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  6524 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  6525 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|        - |  6526 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  6527 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  6528 | `	{` |
|        3 |  6529 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  6530 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  6531 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  6532 | `		}` |
|        3 |  6533 | `		if( sGiven.nByte < 1 ){` |
|        - |  6534 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|        - |  6535 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|        - |  6536 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|      ! 0 |  6537 | `			const char *zScalar =` |
|      ! 0 |  6538 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|      ! 0 |  6539 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|      ! 0 |  6540 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|      ! 0 |  6541 | `		}` |
|        3 |  6542 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  6543 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  6544 | `	}` |
|        3 |  6545 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      137 |  6546 | `}` |
|  1417230 |  6547 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  6548 | `{` |
|  1417235 |  6549 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  1417235 |  6550 | `	SyToken *pEnd = pGen->pEnd;` |
|  1417235 |  6551 | `	sxi32 iDepth = 0;` |
|  1417235 |  6552 | `	int bStarted = 0;` |
| 64734997 |  6553 | `	while( pIn < pEnd ){` |
| 64734997 |  6554 | `		sxu32 t = pIn->nType;` |
| 64734997 |  6555 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 61736021 |  6556 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 58737423 |  6557 | `		if( t & PH7_TK_KEYWORD ){` |
|  4724239 |  6558 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  4724239 |  6559 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  4723975 |  6560 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  6561 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  2344486 |  6562 | `		}` |
| 58702161 |  6563 | `		pIn++;` |
|        5 |  6564 | `	}` |
|  1416971 |  6565 | `	return FALSE;` |
|   708620 |  6566 | `}` |
|        - |  6567 | `/*` |
|        - |  6568 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  6569 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  6570 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  6571 | ` */` |
|  1417230 |  6572 | `static sxi32 GenStateCompileFuncBody(` |
|        - |  6573 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  6574 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  6575 | `	)` |
|        5 |  6576 | `{` |
|        - |  6577 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  6578 | `	GenBlock *pBlock;` |
|        - |  6579 | `	sxu32 nGotoOfft;` |
|        - |  6580 | `	sxi32 rc;` |
|        - |  6581 | `	/* Attach the new function */` |
|  1417235 |  6582 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  1417235 |  6583 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6584 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  6585 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6586 | `		return SXERR_ABORT;` |
|        - |  6587 | `	}` |
|  1417235 |  6588 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  6589 | `	/* Swap bytecode containers */` |
|  1417235 |  6590 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  1417235 |  6591 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  6592 | `	/* Emit constructor property promotion prologue:` |
|        - |  6593 | `	 *   $this->NAME = $NAME;` |
|        - |  6594 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  6595 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  6596 | `	{` |
|  1417235 |  6597 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  6598 | `		sxu32 i;` |
|  2111269 |  6599 | `		for( i = 0; i < nArg; i++ ){` |
|   694039 |  6600 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  6601 | `			char *zSrc;` |
|        - |  6602 | `			sxu32 nSrc,nName;` |
|        - |  6603 | `			SySet sToken;` |
|        - |  6604 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6605 | `			sxi32 rcPromote;` |
|   694039 |  6606 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   693965 |  6607 | `				continue;` |
|        - |  6608 | `			}` |
|        - |  6609 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  6610 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  6611 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  6612 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  6613 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|       79 |  6614 | `			nName = SyStringLength(&pArg->sName);` |
|       79 |  6615 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|       79 |  6616 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|       79 |  6617 | `			if( zSrc == 0 ){` |
|      ! 0 |  6618 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6619 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6620 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  6621 | `				return SXERR_ABORT;` |
|        - |  6622 | `			}` |
|        - |  6623 | `			{` |
|       79 |  6624 | `				char *z = zSrc;` |
|       79 |  6625 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|       79 |  6626 | `				z += sizeof("$this->")-1;` |
|       79 |  6627 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       79 |  6628 | `				z += nName;` |
|       79 |  6629 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|       79 |  6630 | `				z += sizeof(" = $")-1;` |
|       79 |  6631 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       79 |  6632 | `				z += nName;` |
|       79 |  6633 | `				*z = 0;` |
|        - |  6634 | `			}` |
|       79 |  6635 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       79 |  6636 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|       79 |  6637 | `			pTmpIn = pGen->pIn;` |
|       79 |  6638 | `			pTmpEnd = pGen->pEnd;` |
|       79 |  6639 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       79 |  6640 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       79 |  6641 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|       79 |  6642 | `			pGen->pIn = pTmpIn;` |
|       79 |  6643 | `			pGen->pEnd = pTmpEnd;` |
|       79 |  6644 | `			SySetRelease(&sToken);` |
|       79 |  6645 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  6646 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6647 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6648 | `				return SXERR_ABORT;` |
|        - |  6649 | `			}` |
|        - |  6650 | `			/* Discard the assignment result — this is a statement expression. */` |
|       79 |  6651 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       42 |  6652 | `		}` |
|        - |  6653 | `	}` |
|        - |  6654 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  6655 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  6656 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  6657 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  6658 | `	{` |
|  1417235 |  6659 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  1417235 |  6660 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|        - |  6661 | `		/* Compile the body */` |
|  1417235 |  6662 | `		PH7_CompileBlock(&(*pGen),0);` |
|  1417235 |  6663 | `		pGen->bInGenerator = bSavedGen;` |
|        - |  6664 | `	}` |
|        - |  6665 | `	/* Fix exception jumps now the destination is resolved */` |
|  1417235 |  6666 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  6667 | `	/* Emit the final return if not yet done */` |
|  1417235 |  6668 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  6669 | `	/* Fix gotos jumps now the destination is resolved */` |
|  1417235 |  6670 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  6671 | `		rc = SXERR_ABORT;` |
|      ! 0 |  6672 | `	}` |
|  1417235 |  6673 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  6674 | `	/* Restore the default container */` |
|  1417235 |  6675 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  6676 | `	/* Leave function block */` |
|  1417235 |  6677 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  1417235 |  6678 | `	if( rc == SXERR_ABORT ){` |
|        - |  6679 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6680 | `		return SXERR_ABORT;` |
|        - |  6681 | `	}` |
|        - |  6682 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  6683 | `	{` |
|  1417235 |  6684 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  6685 | `		sxu32 i;` |
| 39351373 |  6686 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 37934407 |  6687 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      269 |  6688 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      269 |  6689 | `				break;` |
|        - |  6690 | `			}` |
| 18967074 |  6691 | `		}` |
|        - |  6692 | `	}` |
|  1417235 |  6693 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6694 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|      269 |  6695 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  6696 | `			return SXERR_ABORT;` |
|        - |  6697 | `		}` |
|      132 |  6698 | `	}` |
|        - |  6699 | `	/* All done, function body compiled */` |
|  1417235 |  6700 | `	return SXRET_OK;` |
|   708620 |  6701 | `}` |
|        - |  6702 | `/*` |
|        - |  6703 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  6704 | ` * According to the PHP language reference manual.` |
|        - |  6705 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  6706 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  6707 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  6708 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  6709 | ` *  Functions need not be defined before they are referenced.` |
|        - |  6710 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  6711 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  6712 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  6713 | ` *  calls with over 32-64 recursion levels.` |
|        - |  6714 | ` *` |
|        - |  6715 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  6716 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  6717 | ` * on these extension.` |
|        - |  6718 | ` */` |
|        - |  6719 | `/*` |
|        - |  6720 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  6721 | ` */` |
|      570 |  6722 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  6723 | `{` |
|        - |  6724 | `	sxu32 i;` |
|     1611 |  6725 | `	for( i = 0; i < n; i++ ){` |
|     1381 |  6726 | `		int a = zA[i], b = zB[i];` |
|     1381 |  6727 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     1381 |  6728 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     1381 |  6729 | `		if( a != b ) return a - b;` |
|      523 |  6730 | `	}` |
|      235 |  6731 | `	return 0;` |
|      290 |  6732 | `}` |
|        - |  6733 | `/*` |
|        - |  6734 | ` * Internal type-atom kinds used during union type parsing.` |
|        - |  6735 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - |  6736 | ` * (which are positive bit values stored in sxu32).` |
|        - |  6737 | ` */` |
|        - |  6738 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - |  6739 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - |  6740 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - |  6741 |  |
|        - |  6742 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - |  6743 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - |  6744 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - |  6745 |  |
|        - |  6746 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - |  6747 | `struct PhlTypeAtom {` |
|        - |  6748 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - |  6749 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - |  6750 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - |  6751 | `	sxu32 nCanon;` |
|        - |  6752 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - |  6753 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - |  6754 | `};` |
|        - |  6755 |  |
|        - |  6756 | `/*` |
|        - |  6757 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - |  6758 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - |  6759 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - |  6760 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - |  6761 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - |  6762 | ` * already be consumed by the caller.` |
|        - |  6763 | ` */` |
|   106556 |  6764 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 |  6765 | `{` |
|   106561 |  6766 | `	SyToken *pIn = pGen->pIn;` |
|   106561 |  6767 | `	SyZero(pOut, sizeof(*pOut));` |
|   106561 |  6768 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   106561 |  6769 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6770 | `		return SXERR_SYNTAX;` |
|        - |  6771 | `	}` |
|        - |  6772 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   106561 |  6773 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        8 |  6774 | `		pIn++;` |
|        8 |  6775 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6776 | `			return SXERR_SYNTAX;` |
|        - |  6777 | `		}` |
|        3 |  6778 | `	}` |
|   106561 |  6779 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  6780 | `		return SXERR_SYNTAX;` |
|        - |  6781 | `	}` |
|   106561 |  6782 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    90485 |  6783 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    90485 |  6784 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|    11701 |  6785 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    84637 |  6786 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       81 |  6787 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    78751 |  6788 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    20067 |  6789 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    68682 |  6790 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|    58569 |  6791 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|    29369 |  6792 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       41 |  6793 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|       68 |  6794 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       27 |  6795 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|       37 |  6796 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       14 |  6797 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       23 |  6798 | `			pOut->nType = SXU32_HIGH;` |
|       23 |  6799 | `			pOut->sClass = pIn->sData;` |
|       13 |  6800 | `		}else{` |
|        3 |  6801 | `			return SXERR_SYNTAX;` |
|        - |  6802 | `		}` |
|    90483 |  6803 | `		pIn++;` |
|    45244 |  6804 | `	}else{` |
|        - |  6805 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - |  6806 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    16081 |  6807 | `		SyString *pT = &pIn->sData;` |
|    16081 |  6808 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       34 |  6809 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       34 |  6810 | `			pIn++;` |
|    16066 |  6811 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      177 |  6812 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      177 |  6813 | `			pIn++;` |
|    15965 |  6814 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       26 |  6815 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       26 |  6816 | `			pIn++;` |
|       15 |  6817 | `		}else{` |
|        - |  6818 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    15857 |  6819 | `			SyToken *pFirst = pIn;` |
|    15857 |  6820 | `			SyToken *pLast = pIn;` |
|    15857 |  6821 | `			pOut->nType = SXU32_HIGH;` |
|    15857 |  6822 | `			pOut->sClass = pIn->sData;` |
|    15857 |  6823 | `			pIn++;` |
|    23781 |  6824 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    15860 |  6825 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|        3 |  6826 | `				pLast = &pIn[1];` |
|        3 |  6827 | `				pIn += 2;` |
|        1 |  6828 | `			}` |
|    15857 |  6829 | `			if( pLast != pFirst ){` |
|        3 |  6830 | `				const char *zFirst = pFirst->sData.zString;` |
|        3 |  6831 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|        3 |  6832 | `				pOut->sClass.zString = zFirst;` |
|        3 |  6833 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|        1 |  6834 | `			}` |
|        - |  6835 | `		}` |
|        - |  6836 | `	}` |
|   106559 |  6837 | `	pGen->pIn = pIn;` |
|   106559 |  6838 | `	return SXRET_OK;` |
|    53283 |  6839 | `}` |
|        - |  6840 |  |
|        - |  6841 | `/*` |
|        - |  6842 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - |  6843 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - |  6844 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|        - |  6845 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - |  6846 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - |  6847 | ` */` |
|   106378 |  6848 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 |  6849 | `{` |
|        - |  6850 | `	int i;` |
|   106383 |  6851 | `	int nNonNull = 0;` |
|   106383 |  6852 | `	int bAnyIntersection = 0;` |
|        - |  6853 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   106383 |  6854 | `	sxu32 nMaxGroup = 0;` |
|  3510479 |  6855 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   212913 |  6856 | `	for( i = 0; i < nAtoms; i++ ){` |
|   106535 |  6857 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   106505 |  6858 | `			nNonNull++;` |
|   106505 |  6859 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   106505 |  6860 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   106505 |  6861 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|    53250 |  6862 | `			}` |
|    53250 |  6863 | `		}` |
|    53270 |  6864 | `	}` |
|   212861 |  6865 | `	for( i = 0; i < nAtoms; i++ ){` |
|   106507 |  6866 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       29 |  6867 | `			bAnyIntersection = 1;` |
|       29 |  6868 | `			break;` |
|        - |  6869 | `		}` |
|    53244 |  6870 | `	}` |
|   106383 |  6871 | `	if( bAnyIntersection ){` |
|        - |  6872 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - |  6873 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - |  6874 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       29 |  6875 | `		sxu32 g, nGroups = 0;` |
|       29 |  6876 | `		int bFirstGroup = 1;` |
|       59 |  6877 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|       59 |  6878 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       35 |  6879 | `			int bFirstMember = 1;` |
|        - |  6880 | `			int bWrap;` |
|       35 |  6881 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - |  6882 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - |  6883 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - |  6884 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - |  6885 | `			 * parens, matching PHP's canonical text. */` |
|       47 |  6886 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       35 |  6887 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       35 |  6888 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      107 |  6889 | `			for( i = 0; i < nAtoms; i++ ){` |
|       77 |  6890 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|       59 |  6891 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|       59 |  6892 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       55 |  6893 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       30 |  6894 | `				}else{` |
|        6 |  6895 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6896 | `				}` |
|       59 |  6897 | `				bFirstMember = 0;` |
|       32 |  6898 | `			}` |
|       35 |  6899 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       35 |  6900 | `			bFirstGroup = 0;` |
|       20 |  6901 | `		}` |
|       29 |  6902 | `		if( bNullable ){` |
|      ! 0 |  6903 | `			SyBlobAppend(pBlob, "\|", 1);` |
|      ! 0 |  6904 | `			SyBlobAppend(pBlob, "null", 4);` |
|      ! 0 |  6905 | `		}` |
|       83 |  6906 | `		return;` |
|        - |  6907 | `	}` |
|   106359 |  6908 | `	if( nNonNull == 1 && bNullable ){` |
|        - |  6909 | `		/* Shorthand: ?T */` |
|      112 |  6910 | `		for( i = 0; i < nAtoms; i++ ){` |
|      112 |  6911 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      112 |  6912 | `			SyBlobAppend(pBlob, "?", 1);` |
|      112 |  6913 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|       24 |  6914 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       14 |  6915 | `			}else{` |
|       92 |  6916 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6917 | `			}` |
|      112 |  6918 | `			return;` |
|      ! 0 |  6919 | `		}` |
|      ! 0 |  6920 | `	}` |
|        - |  6921 | `	{` |
|   106251 |  6922 | `		int bFirst = 1;` |
|        - |  6923 | `		/* 1) Classes in declaration order */` |
|   212605 |  6924 | `		for( i = 0; i < nAtoms; i++ ){` |
|   106359 |  6925 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    15807 |  6926 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    15807 |  6927 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    15807 |  6928 | `				bFirst = 0;` |
|     7901 |  6929 | `			}` |
|    53182 |  6930 | `		}` |
|        - |  6931 | `		/* 2) Built-ins in canonical order */` |
|        - |  6932 | `		{` |
|        - |  6933 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|        - |  6934 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|        - |  6935 | `			int k;` |
|   743727 |  6936 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  1185143 |  6937 | `				for( i = 0; i < nAtoms; i++ ){` |
|   638017 |  6938 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|    90355 |  6939 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    90355 |  6940 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|    90355 |  6941 | `						bFirst = 0;` |
|    90355 |  6942 | `						break;` |
|        - |  6943 | `					}` |
|   273836 |  6944 | `				}` |
|   318743 |  6945 | `			}` |
|        - |  6946 | `		}` |
|        - |  6947 | `		/* 3) null suffix */` |
|   106251 |  6948 | `		if( bNullable ){` |
|       19 |  6949 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       19 |  6950 | `			SyBlobAppend(pBlob, "null", 4);` |
|        8 |  6951 | `		}` |
|        - |  6952 | `	}` |
|    53194 |  6953 | `}` |
|        - |  6954 |  |
|        - |  6955 | `/*` |
|        - |  6956 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - |  6957 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - |  6958 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - |  6959 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - |  6960 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - |  6961 | ` * whether it was parenthesized.` |
|        - |  6962 | ` *` |
|        - |  6963 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - |  6964 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - |  6965 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - |  6966 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - |  6967 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - |  6968 | ` */` |
|   106530 |  6969 | `static sxi32 GenStateParsePart(` |
|        - |  6970 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - |  6971 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 |  6972 | `{` |
|        - |  6973 | `	sxi32 rc;` |
|   106535 |  6974 | `	int nMembers = 0;` |
|   106535 |  6975 | `	int bParen = 0;` |
|   106535 |  6976 | `	*pnMembers = 0;` |
|   106535 |  6977 | `	*pbParen = 0;` |
|   106535 |  6978 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        9 |  6979 | `		bParen = 1;` |
|        9 |  6980 | `		pGen->pIn++; /* skip '(' */` |
|        3 |  6981 | `	}` |
|    53265 |  6982 | `	for(;;){` |
|   106561 |  6983 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 |  6984 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6985 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 |  6986 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6987 | `		}` |
|   106561 |  6988 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   106561 |  6989 | `		if( rc != SXRET_OK ){` |
|        3 |  6990 | `			return rc;` |
|        - |  6991 | `		}` |
|   106559 |  6992 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   106559 |  6993 | `		(*pnAtoms)++;` |
|   106559 |  6994 | `		nMembers++;` |
|        - |  6995 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   106559 |  6996 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       39 |  6997 | `			SyToken *pNext = &pGen->pIn[1];` |
|       34 |  6998 | `			if( pNext < pGen->pEnd` |
|       39 |  6999 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       31 |  7000 | `				pGen->pIn++; /* skip '&' */` |
|       31 |  7001 | `				continue;` |
|        - |  7002 | `			}` |
|        4 |  7003 | `		}` |
|   106533 |  7004 | `		break;` |
|      ! 0 |  7005 | `	}` |
|   106533 |  7006 | `	if( bParen ){` |
|        9 |  7007 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  7008 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7009 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 |  7010 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7011 | `		}` |
|        9 |  7012 | `		pGen->pIn++; /* skip ')' */` |
|        9 |  7013 | `		if( nMembers < 2 ){` |
|      ! 0 |  7014 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7015 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 |  7016 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7017 | `		}` |
|        3 |  7018 | `	}` |
|   106533 |  7019 | `	*pnMembers = nMembers;` |
|   106533 |  7020 | `	*pbParen = bParen;` |
|   106533 |  7021 | `	return SXRET_OK;` |
|    53270 |  7022 | `}` |
|        - |  7023 |  |
|        - |  7024 | `/*` |
|        - |  7025 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - |  7026 | ` *` |
|        - |  7027 | ` * Outputs:` |
|        - |  7028 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - |  7029 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - |  7030 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - |  7031 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - |  7032 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - |  7033 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - |  7034 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - |  7035 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - |  7036 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - |  7037 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - |  7038 | ` *` |
|        - |  7039 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - |  7040 | ` * SXERR_ABORT on fatal compile errors.` |
|        - |  7041 | ` */` |
|   106394 |  7042 | `static sxi32 GenStateParseUnionTypeDecl(` |
|        - |  7043 | `	ph7_gen_state *pGen,` |
|        - |  7044 | `	sxu32 *pnType,` |
|        - |  7045 | `	SyString *pClass,` |
|        - |  7046 | `	SySet *pAlts,` |
|        - |  7047 | `	sxi32 *piTypeFlags,` |
|        - |  7048 | `	SyString *pTypeText,` |
|        - |  7049 | `	int iNullableFlag,` |
|        - |  7050 | `	int iUnionFlag,` |
|        - |  7051 | `	int bAllowVoid,` |
|        - |  7052 | `	sxu32 nLine` |
|        5 |  7053 | `){` |
|        - |  7054 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   106399 |  7055 | `	int nAtoms = 0;` |
|   106399 |  7056 | `	int bShortNullable = 0;` |
|   106399 |  7057 | `	int bExplicitNull = 0;` |
|        - |  7058 | `	sxi32 rc;` |
|   106399 |  7059 | `	*pnType = 0;` |
|   106399 |  7060 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   106399 |  7061 | `	*piTypeFlags = 0;` |
|   106399 |  7062 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - |  7063 |  |
|   106399 |  7064 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7065 | `		return SXRET_OK;` |
|        - |  7066 | `	}` |
|        - |  7067 | ``	/* Optional `?` shorthand prefix */`` |
|   106394 |  7068 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      101 |  7069 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      100 |  7070 | `		bShortNullable = 1;` |
|      100 |  7071 | `		pGen->pIn++;` |
|      100 |  7072 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7073 | `			return SXERR_SYNTAX;` |
|        - |  7074 | `		}` |
|       48 |  7075 | `	}` |
|        - |  7076 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - |  7077 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - |  7078 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - |  7079 | `	{` |
|        - |  7080 | `		int nMembers, bParen;` |
|   106399 |  7081 | `		sxu32 iGroup = 0;` |
|   106399 |  7082 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   106399 |  7083 | `		if( rc != SXRET_OK ){` |
|        4 |  7084 | `			return rc;` |
|        - |  7085 | `		}` |
|        - |  7086 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - |  7087 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - |  7088 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - |  7089 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - |  7090 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|   159797 |  7091 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   106606 |  7092 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      143 |  7093 | `			if( bShortNullable ){` |
|        - |  7094 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - |  7095 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - |  7096 | `				 * already reported" so callers skip their own error emission. */` |
|        3 |  7097 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7098 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 |  7099 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - |  7100 | `			}` |
|      141 |  7101 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 |  7102 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - |  7103 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7104 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7105 | `			}` |
|      141 |  7106 | ``			pGen->pIn++; /* skip `\|` */`` |
|      141 |  7107 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|      141 |  7108 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7109 | `				return rc;` |
|        - |  7110 | `			}` |
|        5 |  7111 | `		}` |
|   106395 |  7112 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 |  7113 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7114 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7115 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7116 | `		}` |
|        - |  7117 | `	}` |
|        - |  7118 | `	/* Validation pass.` |
|        - |  7119 | `	 *` |
|        - |  7120 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - |  7121 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - |  7122 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - |  7123 | `	 */` |
|        - |  7124 | `	{` |
|        - |  7125 | `		int i, j;` |
|   106395 |  7126 | `		int bHasNonNull = 0;` |
|   106395 |  7127 | `		int bAnyIntersection = 0;` |
|        - |  7128 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - |  7129 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - |  7130 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|  3510875 |  7131 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   212947 |  7132 | `		for( i = 0; i < nAtoms; i++ ){` |
|   106557 |  7133 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|    53281 |  7134 | `		}` |
|   212891 |  7135 | `		for( i = 0; i < nAtoms; i++ ){` |
|   106527 |  7136 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|    53253 |  7137 | `		}` |
|        - |  7138 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - |  7139 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   106395 |  7140 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 |  7141 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7142 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 |  7143 | `			return SXERR_SYNTAX;` |
|        - |  7144 | `		}` |
|   212933 |  7145 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - |  7146 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - |  7147 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - |  7148 | ``			 * `true`/`false` in an intersection). */`` |
|   106555 |  7149 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       55 |  7150 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       55 |  7151 | `				if( bClassLike ){` |
|       53 |  7152 | `					SyString *pC = &aAtoms[i].sClass;` |
|       48 |  7153 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       48 |  7154 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       48 |  7155 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       53 |  7156 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 |  7157 | `						bClassLike = 0;` |
|      ! 0 |  7158 | `					}` |
|       24 |  7159 | `				}` |
|       55 |  7160 | `				if( !bClassLike ){` |
|        - |  7161 | `					const char *zName; sxu32 nName;` |
|        3 |  7162 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7163 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7164 | `					}else{` |
|        3 |  7165 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - |  7166 | `					}` |
|        4 |  7167 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7168 | `						"Type %.*s cannot be part of an intersection type",` |
|        1 |  7169 | `						(int)nName, zName);` |
|        3 |  7170 | `					return SXERR_SYNTAX;` |
|        - |  7171 | `				}` |
|       24 |  7172 | `			}` |
|   106553 |  7173 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      177 |  7174 | `				if( nAtoms > 1 ){` |
|        3 |  7175 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7176 | `						"Void can only be used as a standalone type");` |
|        3 |  7177 | `					return SXERR_SYNTAX;` |
|        - |  7178 | `				}` |
|      175 |  7179 | `				if( !bAllowVoid ){` |
|      ! 0 |  7180 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7181 | `						"void cannot be used here");` |
|      ! 0 |  7182 | `					return SXERR_SYNTAX;` |
|        - |  7183 | `				}` |
|      175 |  7184 | `				if( bShortNullable ){` |
|      ! 0 |  7185 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7186 | `						"Void type cannot be nullable");` |
|      ! 0 |  7187 | `					return SXERR_SYNTAX;` |
|        - |  7188 | `				}` |
|       85 |  7189 | `			}` |
|   106551 |  7190 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - |  7191 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - |  7192 | `				 * type (never = the function does not return). Mirrors the void` |
|        - |  7193 | `				 * validation above; accepted here and enforced at compile time` |
|        - |  7194 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       26 |  7195 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - |  7196 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - |  7197 | `					 * same as any other non-standalone use. */` |
|        5 |  7198 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7199 | `						"never can only be used as a standalone type");` |
|        5 |  7200 | `					return SXERR_SYNTAX;` |
|        - |  7201 | `				}` |
|       21 |  7202 | `				if( !bAllowVoid ){` |
|        - |  7203 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 |  7204 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7205 | `						"never cannot be used as a parameter type");` |
|        3 |  7206 | `					return SXERR_SYNTAX;` |
|        - |  7207 | `				}` |
|        8 |  7208 | `			}` |
|   106545 |  7209 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       34 |  7210 | `				bExplicitNull = 1;` |
|       19 |  7211 | `			}else{` |
|   106515 |  7212 | `				bHasNonNull = 1;` |
|        - |  7213 | `			}` |
|        - |  7214 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - |  7215 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - |  7216 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - |  7217 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - |  7218 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   106745 |  7219 | `			for( j = 0; j < i; j++ ){` |
|      207 |  7220 | `				int bDup = 0;` |
|      207 |  7221 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|      395 |  7222 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|      202 |  7223 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|      207 |  7224 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|      195 |  7225 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|       51 |  7226 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       44 |  7227 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       44 |  7228 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       17 |  7229 | `								aAtoms[j].sClass.zString,` |
|       34 |  7230 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 |  7231 | `							bDup = 1;` |
|      ! 0 |  7232 | `						}` |
|       27 |  7233 | `					}else{` |
|        3 |  7234 | `						bDup = 1;` |
|        - |  7235 | `					}` |
|       23 |  7236 | `				}` |
|      195 |  7237 | `				if( bDup ){` |
|        - |  7238 | `					const char *zName;` |
|        - |  7239 | `					sxu32 nName;` |
|        3 |  7240 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7241 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 |  7242 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7243 | `					}else{` |
|        3 |  7244 | `						zName = aAtoms[i].zCanon;` |
|        3 |  7245 | `						nName = aAtoms[i].nCanon;` |
|        - |  7246 | `					}` |
|        4 |  7247 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 |  7248 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 |  7249 | `					return SXERR_SYNTAX;` |
|        - |  7250 | `				}` |
|       99 |  7251 | `			}` |
|    53274 |  7252 | `		}` |
|   106383 |  7253 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 |  7254 | `			if( bShortNullable ){` |
|        - |  7255 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 |  7256 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7257 | `					"Null can not be used as a standalone type");` |
|      ! 0 |  7258 | `				return SXERR_SYNTAX;` |
|        - |  7259 | `			}` |
|        - |  7260 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - |  7261 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - |  7262 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - |  7263 | `			 * atom, so set it here. */` |
|        7 |  7264 | `			*pnType = MEMOBJ_NULL;` |
|        3 |  7265 | `		}` |
|        - |  7266 | `	}` |
|        - |  7267 | `	/* Compute nullability flag */` |
|   106383 |  7268 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      128 |  7269 | `		*piTypeFlags \|= iNullableFlag;` |
|       62 |  7270 | `	}` |
|        - |  7271 | `	/* Build canonical type text */` |
|   106383 |  7272 | `	if( pTypeText ){` |
|        - |  7273 | `		SyBlob sBlob;` |
|   106383 |  7274 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   159525 |  7275 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|    53189 |  7276 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   106383 |  7277 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   159293 |  7278 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   106192 |  7279 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   106197 |  7280 | `			if( zDup ){` |
|   106197 |  7281 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|    53096 |  7282 | `			}` |
|    53096 |  7283 | `		}` |
|   106383 |  7284 | `		SyBlobRelease(&sBlob);` |
|    53189 |  7285 | `	}` |
|        - |  7286 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - |  7287 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - |  7288 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - |  7289 | `	{` |
|   106383 |  7290 | `		int nNonNull = 0;` |
|   106383 |  7291 | `		int iNonNullIdx = -1;` |
|        - |  7292 | `		int i;` |
|   212913 |  7293 | `		for( i = 0; i < nAtoms; i++ ){` |
|   106535 |  7294 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   106505 |  7295 | `				nNonNull++;` |
|   106505 |  7296 | `				iNonNullIdx = i;` |
|    53250 |  7297 | `			}` |
|    53270 |  7298 | `		}` |
|   106383 |  7299 | `		if( nNonNull <= 1 ){` |
|        - |  7300 | `			/* Fast path: store as single type. */` |
|   106277 |  7301 | `			if( iNonNullIdx >= 0 ){` |
|   106271 |  7302 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   106271 |  7303 | `				if( pA->nType == SXU32_HIGH ){` |
|    23672 |  7304 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     7889 |  7305 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    15783 |  7306 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    15783 |  7307 | `					*pnType = SXU32_HIGH;` |
|    15783 |  7308 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    98382 |  7309 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      175 |  7310 | `					*pnType = MEMOBJ_VOID;` |
|    90408 |  7311 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       18 |  7312 | `					*pnType = MEMOBJ_NEVER;` |
|       10 |  7313 | `				}else{` |
|    90307 |  7314 | `					*pnType = pA->nType;` |
|        - |  7315 | `				}` |
|    53133 |  7316 | `			}` |
|    53141 |  7317 | `		}else{` |
|        - |  7318 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      111 |  7319 | `			*piTypeFlags \|= iUnionFlag;` |
|      355 |  7320 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - |  7321 | `				ph7_type_alt sAlt;` |
|      249 |  7322 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      239 |  7323 | `				SyZero(&sAlt, sizeof(sAlt));` |
|      239 |  7324 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|      239 |  7325 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      146 |  7326 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       47 |  7327 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       99 |  7328 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|       99 |  7329 | `					sAlt.nType = SXU32_HIGH;` |
|       99 |  7330 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|       52 |  7331 | `				}else{` |
|      145 |  7332 | `					sAlt.nType = aAtoms[i].nType;` |
|      145 |  7333 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - |  7334 | `				}` |
|      239 |  7335 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      122 |  7336 | `			}` |
|        - |  7337 | `		}` |
|        - |  7338 | `	}` |
|   106383 |  7339 | `	return SXRET_OK;` |
|    53202 |  7340 | `}` |
|        - |  7341 |  |
|        - |  7342 | `/*` |
|        - |  7343 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - |  7344 | `` * pGen->pIn should point to the token after `)`.`` |
|        - |  7345 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - |  7346 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - |  7347 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - |  7348 | `` *          and union types `: T\|U`.`` |
|        - |  7349 | ` */` |
|  1518504 |  7350 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 |  7351 | `{` |
|  1518509 |  7352 | `	sxi32 iFlags = 0;` |
|        - |  7353 | `	sxi32 rc;` |
|        - |  7354 | `	sxu32 nLine;` |
|  1518509 |  7355 | `	pFunc->nReturnType = 0;` |
|  1518509 |  7356 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  1518509 |  7357 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - |  7358 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - |  7359 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - |  7360 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - |  7361 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - |  7362 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|  1518509 |  7363 | `	SySetReset(&pFunc->aReturnUnion);` |
|  1518509 |  7364 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|  1518509 |  7365 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  1517853 |  7366 | `		return SXRET_OK;` |
|        - |  7367 | `	}` |
|      661 |  7368 | `	pGen->pIn++; /* Skip ':' */` |
|      661 |  7369 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7370 | `		return SXRET_OK;` |
|        - |  7371 | `	}` |
|      661 |  7372 | `	nLine = pGen->pIn->nLine;` |
|      661 |  7373 | `	rc = GenStateParseUnionTypeDecl(` |
|      328 |  7374 | `		pGen,` |
|      328 |  7375 | `		&pFunc->nReturnType,` |
|      328 |  7376 | `		&pFunc->sReturnClass,` |
|      328 |  7377 | `		&pFunc->aReturnUnion,` |
|        - |  7378 | `		&iFlags,` |
|      328 |  7379 | `		&pFunc->sReturnTypeName,` |
|        - |  7380 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - |  7381 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - |  7382 | `		/* iUnionFlag */ 0,` |
|        - |  7383 | `		/* bAllowVoid */ 1,` |
|      328 |  7384 | `		nLine);` |
|      661 |  7385 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7386 | `		return SXERR_ABORT;` |
|        - |  7387 | `	}` |
|      661 |  7388 | `	if( rc == SXERR_CORRUPT ){` |
|        - |  7389 | `		/* Error already reported */` |
|      ! 0 |  7390 | `		return SXERR_SYNTAX;` |
|        - |  7391 | `	}` |
|      661 |  7392 | `	if( rc == SXERR_SYNTAX ){` |
|        8 |  7393 | `		if( pGen->pIn < pGen->pEnd ){` |
|       11 |  7394 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7395 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|        6 |  7396 | `				&pGen->pIn->sData);` |
|        5 |  7397 | `		}else{` |
|      ! 0 |  7398 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - |  7399 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - |  7400 | `		}` |
|        8 |  7401 | `		return SXERR_SYNTAX;` |
|        - |  7402 | `	}` |
|      655 |  7403 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|      655 |  7404 | `	return SXRET_OK;` |
|   759257 |  7405 | `}` |
|        - |  7406 |  |
|   118442 |  7407 | `static sxi32 GenStateCompileFunc(` |
|        - |  7408 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  7409 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - |  7410 | `	sxi32 iFlags,        /* Control flags */` |
|        - |  7411 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - |  7412 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - |  7413 | `	)` |
|        5 |  7414 | `{` |
|        - |  7415 | `	ph7_vm_func *pFunc;` |
|        - |  7416 | `	SyToken *pEnd;` |
|        - |  7417 | `	sxu32 nLine;` |
|        - |  7418 | `	char *zName;` |
|        - |  7419 | `	sxi32 rc;` |
|        - |  7420 | `	/* Extract line number */` |
|   118447 |  7421 | `	nLine = pGen->pIn->nLine;` |
|        - |  7422 | `	/* Jump the left parenthesis '(' */` |
|   118447 |  7423 | `	pGen->pIn++;` |
|        - |  7424 | `	/* Delimit the function signature */` |
|   118447 |  7425 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   118447 |  7426 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  7427 | `		/* Syntax error */` |
|        8 |  7428 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|        8 |  7429 | `		if( rc == SXERR_ABORT ){` |
|        - |  7430 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7431 | `			return SXERR_ABORT;` |
|        - |  7432 | `		}` |
|        8 |  7433 | `		pGen->pIn = pGen->pEnd;` |
|        8 |  7434 | `		return SXRET_OK;` |
|        - |  7435 | `	}` |
|        - |  7436 | `	/* Create the function state */` |
|   118441 |  7437 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   118441 |  7438 | `	if( pFunc == 0 ){` |
|      ! 0 |  7439 | `		goto OutOfMem;` |
|        - |  7440 | `	}` |
|        - |  7441 | `	/* Build the function name, prepending namespace if active */` |
|   118448 |  7442 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - |  7443 | `		SyBlob sFQN;` |
|        - |  7444 | `		sxu32 nLen;` |
|       16 |  7445 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       16 |  7446 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       16 |  7447 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       16 |  7448 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       16 |  7449 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       16 |  7450 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       16 |  7451 | `		SyBlobRelease(&sFQN);` |
|       16 |  7452 | `		if( zName == 0 ){` |
|      ! 0 |  7453 | `			goto OutOfMem;` |
|        - |  7454 | `		}` |
|       16 |  7455 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        9 |  7456 | `	}else{` |
|   118427 |  7457 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   118427 |  7458 | `		if( zName == 0 ){` |
|      ! 0 |  7459 | `			goto OutOfMem;` |
|        - |  7460 | `		}` |
|   118427 |  7461 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - |  7462 | `	}` |
|        - |  7463 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - |  7464 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   118441 |  7465 | `	pFunc->nLine = nLine;` |
|   118441 |  7466 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   118441 |  7467 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  7468 | `		return SXERR_ABORT;` |
|        - |  7469 | `	}` |
|   118441 |  7470 | `	if( pGen->pIn < pEnd ){` |
|        - |  7471 | `		/* Collect function arguments */` |
|   102077 |  7472 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   102077 |  7473 | `		if( rc == SXERR_ABORT ){` |
|        - |  7474 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  7475 | `			return SXERR_ABORT;` |
|        - |  7476 | `		}` |
|    51036 |  7477 | `	}` |
|        - |  7478 | `	/* Point past ')' and parse optional return type ': type' */` |
|   118441 |  7479 | `	pGen->pIn = &pEnd[1];` |
|        - |  7480 | `	{` |
|   118441 |  7481 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   118441 |  7482 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  7483 | `			return SXERR_ABORT;` |
|   118441 |  7484 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|        8 |  7485 | `			return SXERR_SYNTAX;` |
|        - |  7486 | `		}` |
|        - |  7487 | `	}` |
|   118435 |  7488 | `	if( bHandleClosure ){` |
|        - |  7489 | `		ph7_vm_func_closure_env sEnv;` |
|      453 |  7490 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|      448 |  7491 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      270 |  7492 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|       87 |  7493 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  7494 | `				/* Closure,record environment variable */` |
|       87 |  7495 | `				pGen->pIn++;` |
|       87 |  7496 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  7497 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 |  7498 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  7499 | `						return SXERR_ABORT;` |
|        - |  7500 | `					}` |
|      ! 0 |  7501 | `				}` |
|       87 |  7502 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - |  7503 | `				/* Compile until we hit the first closing parenthesis */` |
|      179 |  7504 | `				while( pGen->pIn < pGen->pEnd ){` |
|      179 |  7505 | `					int iFlagsLocal = 0;` |
|      179 |  7506 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|       87 |  7507 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|       87 |  7508 | `						break;` |
|        - |  7509 | `					}` |
|       97 |  7510 | `					nLineLocal = pGen->pIn->nLine;` |
|       97 |  7511 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - |  7512 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|        - |  7513 | `						 * to the variable's memory slot instead of copying its value. */` |
|       53 |  7514 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|       53 |  7515 | `						pGen->pIn++;` |
|       26 |  7516 | `					}` |
|       92 |  7517 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       97 |  7518 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  7519 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  7520 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 |  7521 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  7522 | `								return SXERR_ABORT;` |
|        - |  7523 | `							}` |
|        - |  7524 | `							/* Find the closing parenthesis */` |
|      ! 0 |  7525 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  7526 | `								pGen->pIn++;` |
|      ! 0 |  7527 | `							}` |
|      ! 0 |  7528 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 |  7529 | `								pGen->pIn++;` |
|      ! 0 |  7530 | `							}` |
|      ! 0 |  7531 | `							break;` |
|        - |  7532 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 |  7533 | `					}else{` |
|        - |  7534 | `						SyString *pNameLocal;` |
|        - |  7535 | `						char *zDup;` |
|        - |  7536 | `						/* Duplicate variable name */` |
|       97 |  7537 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       97 |  7538 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       97 |  7539 | `						if( zDup ){` |
|        - |  7540 | `							/* Zero the structure */` |
|       97 |  7541 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       97 |  7542 | `							sEnv.iFlags = iFlagsLocal;` |
|       97 |  7543 | `							sEnv.nIdx = SXU32_HIGH;` |
|       97 |  7544 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       97 |  7545 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      112 |  7546 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|       30 |  7547 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 |  7548 | `									got_this = 1;` |
|      ! 0 |  7549 | `							}` |
|        - |  7550 | `							/* Save imported variable */` |
|       97 |  7551 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       51 |  7552 | `						}else{` |
|      ! 0 |  7553 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  7554 | `							 return SXERR_ABORT;` |
|        - |  7555 | `						}` |
|        - |  7556 | `					}` |
|       97 |  7557 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      109 |  7558 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  7559 | `						/* Ignore trailing commas */` |
|       13 |  7560 | `						pGen->pIn++;` |
|        1 |  7561 | `					}` |
|        5 |  7562 | `				}` |
|        - |  7563 | `				/* php 7.1+: the return type follows the use clause —` |
|        - |  7564 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - |  7565 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - |  7566 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - |  7567 | `				 * legacy pre-use position. */` |
|       87 |  7568 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|        7 |  7569 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|        7 |  7570 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 |  7571 | `						return SXERR_ABORT;` |
|        7 |  7572 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 |  7573 | `						return SXERR_SYNTAX;` |
|        - |  7574 | `					}` |
|        3 |  7575 | `				}` |
|       41 |  7576 | `		}` |
|      453 |  7577 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|        - |  7578 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|        - |  7579 | `			 * available to the closure environment — for EVERY non-static` |
|        - |  7580 | `			 * anonymous function, use list or not (php binds $this to any` |
|        - |  7581 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|        - |  7582 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|        - |  7583 | `			 * a global-scope closure is silently dropped at install. A static` |
|        - |  7584 | `			 * closure never binds $this (php). */` |
|      445 |  7585 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      445 |  7586 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      445 |  7587 | `			sEnv.nIdx = SXU32_HIGH;` |
|      445 |  7588 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      445 |  7589 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      445 |  7590 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      220 |  7591 | `		}` |
|      453 |  7592 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - |  7593 | `			/* Mark as closure */` |
|      447 |  7594 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|      221 |  7595 | `		}` |
|      224 |  7596 | `	}` |
|        - |  7597 | `	/* Compile the body */` |
|   118435 |  7598 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   118435 |  7599 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7600 | `		return SXERR_ABORT;` |
|        - |  7601 | `	}` |
|        - |  7602 | `	/* The cursor sits just past the body's closing brace */` |
|   118435 |  7603 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   118435 |  7604 | `	if( ppFunc ){` |
|   118435 |  7605 | `		*ppFunc = pFunc;` |
|    59215 |  7606 | `	}` |
|   118435 |  7607 | `	rc = SXRET_OK;` |
|   118435 |  7608 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|        - |  7609 | `		/* Finally register the function */` |
|   117993 |  7610 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    58994 |  7611 | `	}` |
|   118435 |  7612 | `	if( rc == SXRET_OK ){` |
|   118435 |  7613 | `		return SXRET_OK;` |
|        - |  7614 | `	}` |
|        - |  7615 | `	/* Fall through if something goes wrong */` |
|      ! 0 |  7616 | `OutOfMem:` |
|        - |  7617 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  7618 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  7619 | `	 */` |
|      ! 0 |  7620 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  7621 | `	return SXERR_ABORT;` |
|    59226 |  7622 | `}` |
|        - |  7623 | `/*` |
|        - |  7624 | ` * Compile a standard PHP function.` |
|        - |  7625 | ` *  Refer to the block-comment above for more information.` |
|        - |  7626 | ` */` |
|   118002 |  7627 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 |  7628 | `{` |
|        - |  7629 | `	SyString *pName;` |
|        - |  7630 | `	sxi32 iFlags;` |
|        - |  7631 | `	sxu32 nKwLine;` |
|        - |  7632 | `	sxu32 nLine;` |
|        - |  7633 | `	sxi32 rc;` |
|        - |  7634 |  |
|   118007 |  7635 | `	nLine = pGen->pIn->nLine;` |
|   118007 |  7636 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   118007 |  7637 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   118007 |  7638 | `	iFlags = 0;` |
|   118007 |  7639 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  7640 | `		/* Return by reference,remember that */` |
|       12 |  7641 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  7642 | `		/* Jump the '&' token */` |
|       12 |  7643 | `		pGen->pIn++;` |
|        5 |  7644 | `	}` |
|   118007 |  7645 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  7646 | `		/* Invalid function name */` |
|        8 |  7647 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|        8 |  7648 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7649 | `			return SXERR_ABORT;` |
|        - |  7650 | `		}` |
|        - |  7651 | `		/* Sychronize with the next semi-colon or braces*/` |
|       22 |  7652 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       16 |  7653 | `			pGen->pIn++;` |
|        2 |  7654 | `		}` |
|        8 |  7655 | `		return SXRET_OK;` |
|        - |  7656 | `	}` |
|   118001 |  7657 | `	pName = &pGen->pIn->sData;` |
|   118001 |  7658 | `	nLine = pGen->pIn->nLine;` |
|        - |  7659 | `	/* Jump the function name */` |
|   118001 |  7660 | `	pGen->pIn++;` |
|   118001 |  7661 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  7662 | `		/* Syntax error */` |
|        3 |  7663 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|        3 |  7664 | `		if( rc == SXERR_ABORT ){` |
|        - |  7665 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7666 | `			return SXERR_ABORT;` |
|        - |  7667 | `		}` |
|        - |  7668 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 |  7669 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  7670 | `			pGen->pIn++;` |
|      ! 0 |  7671 | `		}` |
|        3 |  7672 | `		return SXRET_OK;` |
|        - |  7673 | `	}` |
|        - |  7674 | `	/* Compile function body */` |
|        - |  7675 | `	{` |
|   117999 |  7676 | `		ph7_vm_func *pFuncState = 0;` |
|   117999 |  7677 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|   117999 |  7678 | `		if( pFuncState ){` |
|        - |  7679 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   117987 |  7680 | `			pFuncState->nLine = nKwLine;` |
|    58991 |  7681 | `		}` |
|        - |  7682 | `	}` |
|   117999 |  7683 | `	return rc;` |
|    59006 |  7684 | `}` |
|        - |  7685 | `/*` |
|        - |  7686 | ` * Extract the visibility level associated with a given keyword.` |
|        - |  7687 | ` * According to the PHP language reference manual` |
|        - |  7688 | ` *  Visibility:` |
|        - |  7689 | ` *  The visibility of a property or method can be defined by prefixing` |
|        - |  7690 | ` *  the declaration with the keywords public, protected or private.` |
|        - |  7691 | ` *  Class members declared public can be accessed everywhere.` |
|        - |  7692 | ` *  Members declared protected can be accessed only within the class` |
|        - |  7693 | ` *  itself and by inherited and parent classes. Members declared as private` |
|        - |  7694 | ` *  may only be accessed by the class that defines the member.` |
|        - |  7695 | ` */` |
|  1754374 |  7696 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |  7697 | `{` |
|  1754379 |  7698 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    23477 |  7699 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  1730907 |  7700 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   182629 |  7701 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  7702 | `	}` |
|        - |  7703 | `	/* Assume public by default */` |
|  1548283 |  7704 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   877192 |  7705 | `}` |
|        - |  7706 | `/*` |
|        - |  7707 | ` * Compile a class constant.` |
|        - |  7708 | ` * According to the PHP language reference manual` |
|        - |  7709 | ` *  Class Constants` |
|        - |  7710 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - |  7711 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - |  7712 | ` *   you don't use the $ symbol to declare or use them.` |
|        - |  7713 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - |  7714 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - |  7715 | ` *   It's also possible for interfaces to have constants.` |
|        - |  7716 | ` * Symisc eXtension.` |
|        - |  7717 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - |  7718 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  7719 | ` *  Example:` |
|        - |  7720 | ` *   class Test{` |
|        - |  7721 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  7722 | ` *   };` |
|        - |  7723 | ` *   var_dump(TEST::MyConst);` |
|        - |  7724 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  7725 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  7726 | ` */` |
|        - |  7727 | `/*` |
|        - |  7728 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |  7729 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |  7730 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |  7731 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |  7732 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |  7733 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |  7734 | ` */` |
|   143884 |  7735 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |  7736 | `{` |
|        - |  7737 | `	SyToken *p0, *p1;` |
|   143889 |  7738 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7739 | `		return 0;` |
|        - |  7740 | `	}` |
|   143889 |  7741 | `	p0 = pGen->pIn;` |
|        - |  7742 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|   143889 |  7743 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|      ! 0 |  7744 | `		return 1;` |
|        - |  7745 | `	}` |
|   143889 |  7746 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  7747 | `		return 1;` |
|        - |  7748 | `	}` |
|        - |  7749 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  7750 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|        - |  7751 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|   143885 |  7752 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   143885 |  7753 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|   143885 |  7754 | `		if( p1 ){` |
|   143885 |  7755 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|       34 |  7756 | `				return 1;` |
|        - |  7757 | `			}` |
|   143855 |  7758 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|        5 |  7759 | `				return 1;` |
|        - |  7760 | `			}` |
|    71923 |  7761 | `		}` |
|    71923 |  7762 | `	}` |
|   143851 |  7763 | `	return 0;` |
|    71947 |  7764 | `}` |
|        - |  7765 | `/*` |
|        - |  7766 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|        - |  7767 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|        - |  7768 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|        - |  7769 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|        - |  7770 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|        - |  7771 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|        - |  7772 | ` * Peek only; never consumes tokens.` |
|        - |  7773 | ` */` |
|       24 |  7774 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|        4 |  7775 | `{` |
|       28 |  7776 | `	SyToken *p = pGen->pIn;` |
|       39 |  7777 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       20 |  7778 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|        3 |  7779 | `		p++; /* skip leading unary sign(s) */` |
|        1 |  7780 | `	}` |
|       28 |  7781 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|       23 |  7782 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|        - |  7783 | `	}` |
|        6 |  7784 | `	p++;` |
|        - |  7785 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|        6 |  7786 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|       16 |  7787 | `}` |
|        - |  7788 | `/*` |
|        - |  7789 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|        - |  7790 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|        - |  7791 | `` * `$o->new`), not a `new` expression.`` |
|        - |  7792 | ` */` |
|      110 |  7793 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|        4 |  7794 | `{` |
|        - |  7795 | `	sxi32 iOp;` |
|      114 |  7796 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|       11 |  7797 | `		return 0;` |
|        - |  7798 | `	}` |
|      104 |  7799 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|      104 |  7800 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|       59 |  7801 | `}` |
|        - |  7802 | `/*` |
|        - |  7803 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|        - |  7804 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|        - |  7805 | ` * interface-constant and (instance/static) property-default initializers` |
|        - |  7806 | ` * ("New expressions are not supported in this context") while still allowing it` |
|        - |  7807 | ` * in global constants, parameter defaults and static-local initializers (which` |
|        - |  7808 | ` * are compiled by different functions and left untouched). The scan is` |
|        - |  7809 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|        - |  7810 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|        - |  7811 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|        - |  7812 | ` *` |
|        - |  7813 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|        - |  7814 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|        - |  7815 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|        - |  7816 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|        - |  7817 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|        - |  7818 | ` */` |
|   230002 |  7819 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|        5 |  7820 | `{` |
|   230007 |  7821 | `	SyToken *p = pGen->pIn;` |
|   230007 |  7822 | `	int iDepth = 0;` |
|   562073 |  7823 | `	while( p < pGen->pEnd ){` |
|   562073 |  7824 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   229955 |  7825 | `			break; /* end of this initializer */` |
|        - |  7826 | `		}` |
|   332118 |  7827 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   169964 |  7828 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     7800 |  7829 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  7830 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|        - |  7831 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|        - |  7832 | `			 * expression. */` |
|        3 |  7833 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        3 |  7834 | `			p++;` |
|        3 |  7835 | `			if( bArrow ){` |
|        - |  7836 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|        - |  7837 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|        3 |  7838 | `				int iBase = iDepth;` |
|       17 |  7839 | `				while( p < pGen->pEnd ){` |
|       17 |  7840 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        5 |  7841 | `						iDepth++;` |
|       15 |  7842 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        5 |  7843 | `						if( iDepth <= iBase ){` |
|      ! 0 |  7844 | `							break; /* closes an enclosing group, not the fn's own */` |
|        - |  7845 | `						}` |
|        5 |  7846 | `						iDepth--;` |
|       11 |  7847 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        3 |  7848 | `						break;` |
|        - |  7849 | `					}` |
|       15 |  7850 | `					p++;` |
|        1 |  7851 | `				}` |
|        2 |  7852 | `			}else{` |
|        - |  7853 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|        - |  7854 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|        - |  7855 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|        - |  7856 | `				 * then skip the balanced brace block. */` |
|      ! 0 |  7857 | `				int iLocal = 0;` |
|      ! 0 |  7858 | `				while( p < pGen->pEnd ){` |
|      ! 0 |  7859 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      ! 0 |  7860 | `						break; /* body brace */` |
|        - |  7861 | `					}` |
|      ! 0 |  7862 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  7863 | `						iLocal++;` |
|      ! 0 |  7864 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  7865 | `						if( iLocal > 0 ){` |
|      ! 0 |  7866 | `							iLocal--;` |
|      ! 0 |  7867 | `						}` |
|      ! 0 |  7868 | `					}` |
|      ! 0 |  7869 | `					p++;` |
|      ! 0 |  7870 | `				}` |
|      ! 0 |  7871 | `				if( p < pGen->pEnd ){` |
|      ! 0 |  7872 | `					int iBrace = 0; /* p is on the body '{' */` |
|      ! 0 |  7873 | `					while( p < pGen->pEnd ){` |
|      ! 0 |  7874 | `						if( p->nType & PH7_TK_OCB ){` |
|      ! 0 |  7875 | `							iBrace++;` |
|      ! 0 |  7876 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      ! 0 |  7877 | `							iBrace--;` |
|      ! 0 |  7878 | `							if( iBrace == 0 ){` |
|      ! 0 |  7879 | `								p++;` |
|      ! 0 |  7880 | `								break;` |
|        - |  7881 | `							}` |
|      ! 0 |  7882 | `						}` |
|      ! 0 |  7883 | `						p++;` |
|      ! 0 |  7884 | `					}` |
|      ! 0 |  7885 | `				}` |
|        - |  7886 | `			}` |
|        3 |  7887 | `			continue;` |
|        - |  7888 | `		}` |
|   332121 |  7889 | `		if( p->nType & PH7_TK_OCB ){` |
|       45 |  7890 | `			if( iDepth == 0 ){` |
|        - |  7891 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|        - |  7892 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|        - |  7893 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|        - |  7894 | `				 * is legal — don't scan into it. */` |
|       45 |  7895 | `				break;` |
|        - |  7896 | `			}` |
|      ! 0 |  7897 | `			iDepth++;` |
|   332077 |  7898 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     7855 |  7899 | `			iDepth++;` |
|   328152 |  7900 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     7853 |  7901 | `			if( iDepth > 0 ){` |
|     7853 |  7902 | `				iDepth--;` |
|     3924 |  7903 | `			}` |
|   320303 |  7904 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    86225 |  7905 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  7906 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|        - |  7907 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|        - |  7908 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|       11 |  7909 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  7910 | `				return 1;` |
|        - |  7911 | `			}` |
|      ! 0 |  7912 | `		}` |
|   332069 |  7913 | `		p++;` |
|        5 |  7914 | `	}` |
|   229999 |  7915 | `	return 0;` |
|   115006 |  7916 | `}` |
|        - |  7917 | `/*` |
|        - |  7918 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  7919 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  7920 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  7921 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  7922 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  7923 | ` * share the same backing.` |
|        - |  7924 | ` */` |
|      350 |  7925 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  7926 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  7927 | `{` |
|      355 |  7928 | `	pAttr->nType = nType;` |
|      355 |  7929 | `	pAttr->sClass = *pClass;` |
|      355 |  7930 | `	pAttr->sTypeName = *pTypeName;` |
|      355 |  7931 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  7932 | `		sxu32 i;` |
|       73 |  7933 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       51 |  7934 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       51 |  7935 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       28 |  7936 | `		}` |
|       11 |  7937 | `	}` |
|      355 |  7938 | `}` |
|   143884 |  7939 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  7940 | `{` |
|   143889 |  7941 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  7942 | `	SySet *pInstrContainer;` |
|        - |  7943 | `	ph7_class_attr *pCons;` |
|        - |  7944 | `	SyString *pName;` |
|        - |  7945 | `	sxi32 rc;` |
|   143889 |  7946 | `	sxu32 nType = 0;` |
|        - |  7947 | `	SyString sTypeClass;` |
|        - |  7948 | `	SyString sTypeText;` |
|        - |  7949 | `	SySet aUnionAlts;` |
|   143889 |  7950 | `	sxi32 iTypeFlags = 0;` |
|   143889 |  7951 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   143889 |  7952 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   143889 |  7953 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  7954 | `	/* Extract visibility level */` |
|   143889 |  7955 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  7956 | `	/* Mark as constant */` |
|   143889 |  7957 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   143889 |  7958 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  7959 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  7960 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   143908 |  7961 | `	if( GenStateClassConstHasType(pGen) ){` |
|       61 |  7962 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|       38 |  7963 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|        - |  7964 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  7965 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  7966 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  7967 | `		 * and success paths release. */` |
|       42 |  7968 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  7969 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  7970 | `			goto Synchronize;` |
|       42 |  7971 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  7972 | `			return SXERR_ABORT;` |
|       42 |  7973 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  7974 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  7975 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|      ! 0 |  7976 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  7977 | `				return SXERR_ABORT;` |
|        - |  7978 | `			}` |
|      ! 0 |  7979 | `			goto Synchronize;` |
|        - |  7980 | `		}` |
|       42 |  7981 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       19 |  7982 | `	}` |
|    71942 |  7983 | `loop:` |
|   143891 |  7984 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  7985 | `		/* Invalid constant name */` |
|      ! 0 |  7986 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  7987 | `		if( rc == SXERR_ABORT ){` |
|        - |  7988 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7989 | `			return SXERR_ABORT;` |
|        - |  7990 | `		}` |
|      ! 0 |  7991 | `		goto Synchronize;` |
|        - |  7992 | `	}` |
|        - |  7993 | `	/* Peek constant name */` |
|   143891 |  7994 | `	pName = &pGen->pIn->sData;` |
|        - |  7995 | `	/* Make sure the constant name isn't reserved */` |
|   143891 |  7996 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  7997 | `		/* Reserved constant name */` |
|      ! 0 |  7998 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  7999 | `		if( rc == SXERR_ABORT ){` |
|        - |  8000 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8001 | `			return SXERR_ABORT;` |
|        - |  8002 | `		}` |
|      ! 0 |  8003 | `		goto Synchronize;` |
|        - |  8004 | `	}` |
|        - |  8005 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   143891 |  8006 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       61 |  8007 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       38 |  8008 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       19 |  8009 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       42 |  8010 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8011 | `			return SXERR_ABORT;` |
|       42 |  8012 | `		}else if( rc != SXRET_OK ){` |
|        3 |  8013 | `			goto Synchronize;` |
|        - |  8014 | `		}` |
|       18 |  8015 | `	}` |
|        - |  8016 | `	/* Advance the stream cursor */` |
|   143889 |  8017 | `	pGen->pIn++;` |
|   143889 |  8018 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  8019 | `		/* Invalid declaration */` |
|      ! 0 |  8020 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  8021 | `		if( rc == SXERR_ABORT ){` |
|        - |  8022 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8023 | `			return SXERR_ABORT;` |
|        - |  8024 | `		}` |
|      ! 0 |  8025 | `		goto Synchronize;` |
|        - |  8026 | `	}` |
|   143889 |  8027 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  8028 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  8029 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  8030 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  8031 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   143884 |  8032 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       39 |  8033 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  8034 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8035 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  8036 | `			&pClass->sName,pName,&sTypeText);` |
|        6 |  8037 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8038 | `			return SXERR_ABORT;` |
|        - |  8039 | `		}` |
|        6 |  8040 | `		goto Synchronize;` |
|        - |  8041 | `	}` |
|        - |  8042 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|        - |  8043 | `	 * constant initializer ("New expressions are not supported in this context").` |
|        - |  8044 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|   143885 |  8045 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|        5 |  8046 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8047 | `			"New expressions are not supported in this context");` |
|        5 |  8048 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8049 | `			return SXERR_ABORT;` |
|        - |  8050 | `		}` |
|        5 |  8051 | `		goto Synchronize;` |
|        - |  8052 | `	}` |
|        - |  8053 | `	/* Allocate a new class attribute */` |
|   143881 |  8054 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   143881 |  8055 | `	if( pCons ){` |
|   143881 |  8056 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   143881 |  8057 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8058 | `			return SXERR_ABORT;` |
|        - |  8059 | `		}` |
|    71938 |  8060 | `	}` |
|   143881 |  8061 | `	if( pCons == 0 ){` |
|      ! 0 |  8062 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8063 | `		return SXERR_ABORT;` |
|        - |  8064 | `	}` |
|   143881 |  8065 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  8066 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  8067 | `	}` |
|        - |  8068 | `	/* Swap bytecode container */` |
|   143881 |  8069 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   143881 |  8070 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  8071 | `	/* Compile constant value.` |
|        - |  8072 | `	 */` |
|   143881 |  8073 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   143881 |  8074 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  8075 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  8076 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8077 | `			return SXERR_ABORT;` |
|        - |  8078 | `		}` |
|        1 |  8079 | `	}` |
|        - |  8080 | `	/* Emit the done instruction */` |
|   143881 |  8081 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   143881 |  8082 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   143881 |  8083 | `	if( rc == SXERR_ABORT ){` |
|        - |  8084 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  8085 | `		return SXERR_ABORT;` |
|        - |  8086 | `	}` |
|        - |  8087 | `	/* All done,install the constant */` |
|   143881 |  8088 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   143881 |  8089 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8090 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8091 | `		return SXERR_ABORT;` |
|        - |  8092 | `	}` |
|   143881 |  8093 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8094 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        3 |  8095 | `		pGen->pIn++; /* Jump the comma */` |
|        3 |  8096 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  8097 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8098 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8099 | `				pTok--;` |
|      ! 0 |  8100 | `			}` |
|      ! 0 |  8101 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8102 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  8103 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8104 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8105 | `				return SXERR_ABORT;` |
|        - |  8106 | `			}` |
|      ! 0 |  8107 | `		}else{` |
|        3 |  8108 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|        3 |  8109 | `				goto loop;` |
|        - |  8110 | `			}` |
|        - |  8111 | `		}` |
|      ! 0 |  8112 | `	}` |
|   143879 |  8113 | `	SySetRelease(&aUnionAlts);` |
|   143879 |  8114 | `	return SXRET_OK;` |
|        5 |  8115 | `Synchronize:` |
|       13 |  8116 | `	SySetRelease(&aUnionAlts);` |
|        - |  8117 | `	/* Synchronize with the first semi-colon */` |
|       45 |  8118 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  8119 | `		pGen->pIn++;` |
|        3 |  8120 | `	}` |
|       13 |  8121 | `	return SXERR_CORRUPT;` |
|    71947 |  8122 | `}` |
|        - |  8123 | `/*` |
|        - |  8124 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  8125 | ` * According to the PHP language reference manual` |
|        - |  8126 | ` *  Properties` |
|        - |  8127 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  8128 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  8129 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  8130 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  8131 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  8132 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  8133 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  8134 | ` * Symisc eXtension.` |
|        - |  8135 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  8136 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  8137 | ` *  Example:` |
|        - |  8138 | ` *   class Test{` |
|        - |  8139 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  8140 | ` *   };` |
|        - |  8141 | ` *   var_dump(TEST::myVar);` |
|        - |  8142 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  8143 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  8144 | ` */` |
|        - |  8145 | `/*` |
|        - |  8146 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  8147 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  8148 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  8149 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  8150 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  8151 | ` */` |
|  1310496 |  8152 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  8153 | `{` |
|  1310501 |  8154 | `	SyToken *p = pStart;` |
|  1310501 |  8155 | `	int bFirst = 1;` |
|  1310501 |  8156 | `	if( p >= pEnd ) return 0;` |
|        - |  8157 | ``	/* Optional nullable `?` shorthand. */`` |
|  1310501 |  8158 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       35 |  8159 | `		p++;` |
|       35 |  8160 | `		if( p >= pEnd ) return 0;` |
|       16 |  8161 | `	}` |
|        - |  8162 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  8163 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  8164 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  8165 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   655248 |  8166 | `	for(;;){` |
|  1310521 |  8167 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  8168 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  8169 | `			p++;` |
|        9 |  8170 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  8171 | `			if( p >= pEnd ) return 0;` |
|        3 |  8172 | `			p++; /* skip ')' */` |
|        2 |  8173 | `		}else{` |
|        - |  8174 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  8175 | ``			 * then any `&`-joined intersection members. */`` |
|  1310519 |  8176 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  1310519 |  8177 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  8178 | `				return 0;` |
|        - |  8179 | `			}` |
|        - |  8180 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  8181 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  8182 | `			 * may still appear at the initial dispatch site). */` |
|  1310519 |  8183 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  1310471 |  8184 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  1310466 |  8185 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    23706 |  8186 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  1310189 |  8187 | `					return 0;` |
|        - |  8188 | `				}` |
|      141 |  8189 | `			}` |
|      335 |  8190 | `			p++;` |
|      337 |  8191 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8192 | `				p += 2;` |
|        1 |  8193 | `			}` |
|      498 |  8194 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|      338 |  8195 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8196 | `				p++; /* skip '&' */` |
|        3 |  8197 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  8198 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  8199 | `				p++;` |
|        3 |  8200 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  8201 | `					p += 2;` |
|      ! 0 |  8202 | `				}` |
|        1 |  8203 | `			}` |
|        - |  8204 | `		}` |
|      337 |  8205 | `		bFirst = 0;` |
|      332 |  8206 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  8207 | `			&& p->sData.zString[0] == '\|' ){` |
|       25 |  8208 | ``			p++; /* next `\|`-separated part */`` |
|       25 |  8209 | `			continue;` |
|        - |  8210 | `		}` |
|      317 |  8211 | `		break;` |
|      ! 0 |  8212 | `	}` |
|      317 |  8213 | `	if( p >= pEnd ) return 0;` |
|      317 |  8214 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   655253 |  8215 | `}` |
|        - |  8216 |  |
|        - |  8217 | `/*` |
|        - |  8218 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - |  8219 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - |  8220 | ` * if not). Recognized forms:` |
|        - |  8221 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - |  8222 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - |  8223 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - |  8224 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - |  8225 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - |  8226 | ` * on unrecoverable error.` |
|        - |  8227 | ` *` |
|        - |  8228 | ` * When a type is parsed:` |
|        - |  8229 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - |  8230 | ` *   *pClass is set to the class name (for class types)` |
|        - |  8231 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - |  8232 | ` *   *pTypeText is set to the original text span of the type` |
|        - |  8233 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - |  8234 | ` */` |
|      322 |  8235 | `static sxi32 GenStateParsePropertyType(` |
|        - |  8236 | `	ph7_gen_state *pGen,` |
|        - |  8237 | `	sxu32 *pnType,` |
|        - |  8238 | `	SyString *pClass,` |
|        - |  8239 | `	sxi32 *piTypeFlags,` |
|        - |  8240 | `	SyString *pTypeText,` |
|        - |  8241 | `	SySet *pAlts` |
|        5 |  8242 | `){` |
|      327 |  8243 | `	sxi32 iFlags = 0;` |
|        - |  8244 | `	sxi32 rc;` |
|      327 |  8245 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  8246 | `		return SXRET_OK;` |
|        - |  8247 | `	}` |
|        - |  8248 | `	/* If the first token is '$', there's no type */` |
|      327 |  8249 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  8250 | `		return SXRET_OK;` |
|        - |  8251 | `	}` |
|      327 |  8252 | `	rc = GenStateParseUnionTypeDecl(` |
|      161 |  8253 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  8254 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  8255 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  8256 | `		/* bAllowVoid */ 0,` |
|      322 |  8257 | `		pGen->pIn->nLine);` |
|      327 |  8258 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8259 | `		return rc;` |
|        - |  8260 | `	}` |
|        - |  8261 | `	/* Verify next token is '$' (start of property name) */` |
|      327 |  8262 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8263 | `		return SXERR_SYNTAX;` |
|        - |  8264 | `	}` |
|      327 |  8265 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|      327 |  8266 | `	return SXRET_OK;` |
|      166 |  8267 | `}` |
|        - |  8268 |  |
|        - |  8269 | `/*` |
|        - |  8270 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - |  8271 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - |  8272 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - |  8273 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - |  8274 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - |  8275 | ` * by the type parser itself before reaching here.` |
|        - |  8276 | ` *` |
|        - |  8277 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - |  8278 | ` * use in the error message.` |
|        - |  8279 | ` */` |
|      498 |  8280 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  8281 | `	sxu32 nType,` |
|        - |  8282 | `	const SyString *pClass,` |
|        - |  8283 | `	const char **pzName,` |
|        - |  8284 | `	sxu32 *pnName)` |
|        5 |  8285 | `{` |
|        - |  8286 | `	const char *z;` |
|        - |  8287 | `	sxu32 n;` |
|      503 |  8288 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|      449 |  8289 | `		return 0;` |
|        - |  8290 | `	}` |
|       59 |  8291 | `	z = pClass->zString;` |
|       59 |  8292 | `	n = pClass->nByte;` |
|       59 |  8293 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 |  8294 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - |  8295 | `	}` |
|        - |  8296 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - |  8297 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - |  8298 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|       52 |  8299 | `	return 0;` |
|      254 |  8300 | `}` |
|        - |  8301 |  |
|        - |  8302 | `/*` |
|        - |  8303 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - |  8304 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - |  8305 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - |  8306 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - |  8307 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - |  8308 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - |  8309 | ` *` |
|        - |  8310 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - |  8311 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - |  8312 | ` */` |
|      436 |  8313 | `static sxi32 GenStateValidateMemberType(` |
|        - |  8314 | `	ph7_gen_state *pGen,` |
|        - |  8315 | `	ph7_class *pClass,` |
|        - |  8316 | `	const SyString *pMemberName,` |
|        - |  8317 | `	sxu32 nType,` |
|        - |  8318 | `	const SyString *pTypeClass,` |
|        - |  8319 | `	const SyString *pTypeText,` |
|        - |  8320 | `	SySet *pUnionAlts,` |
|        - |  8321 | `	const char *zErrFmt,` |
|        - |  8322 | `	sxu32 nLine)` |
|        5 |  8323 | `{` |
|      441 |  8324 | `	const char *zBad = 0;` |
|      441 |  8325 | `	sxu32 nBad = 0;` |
|        - |  8326 | `	SyString sFallback;` |
|        - |  8327 | `	const SyString *pBad;` |
|        - |  8328 | `	sxi32 rc;` |
|      441 |  8329 | `	int bDisallowed = 0;` |
|      441 |  8330 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  8331 | `		bDisallowed = 1;` |
|      439 |  8332 | `	}else if( pUnionAlts ){` |
|        - |  8333 | `		sxu32 i;` |
|       95 |  8334 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       67 |  8335 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       67 |  8336 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  8337 | `				bDisallowed = 1;` |
|        3 |  8338 | `				break;` |
|        - |  8339 | `			}` |
|       35 |  8340 | `		}` |
|       15 |  8341 | `	}` |
|      441 |  8342 | `	if( !bDisallowed ){` |
|      435 |  8343 | `		return SXRET_OK;` |
|        - |  8344 | `	}` |
|        - |  8345 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - |  8346 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - |  8347 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 |  8348 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 |  8349 | `		pBad = pTypeText;` |
|        5 |  8350 | `	}else{` |
|      ! 0 |  8351 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 |  8352 | `		pBad = &sFallback;` |
|        - |  8353 | `	}` |
|       11 |  8354 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  8355 | `		zErrFmt,` |
|        3 |  8356 | `		&pClass->sName,pMemberName,pBad);` |
|        8 |  8357 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  8358 | `		return SXERR_ABORT;` |
|        - |  8359 | `	}` |
|        8 |  8360 | `	return SXERR_SYNTAX;` |
|      223 |  8361 | `}` |
|        - |  8362 | `/*` |
|        - |  8363 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  8364 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  8365 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  8366 | ` * than promoted to a lexer keyword.` |
|        - |  8367 | ` */` |
| 10236554 |  8368 | `static int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  8369 | `{` |
| 10279646 |  8370 | `	return (pTok->nType & PH7_TK_ID)` |
|  5161364 |  8371 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 10279641 |  8372 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  8373 | `}` |
|        - |  8374 | `/*` |
|        - |  8375 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|        - |  8376 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|        - |  8377 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|        - |  8378 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|        - |  8379 | ` */` |
|  3743430 |  8380 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|        5 |  8381 | `{` |
|  3743435 |  8382 | `	*pnTok = 0;` |
|  3743430 |  8383 | `	if( &pTok[3] < pEnd` |
|  3567810 |  8384 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|  3123366 |  8385 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|  1427279 |  8386 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       16 |  8387 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|       16 |  8388 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|       21 |  8389 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|       17 |  8390 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|       17 |  8391 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|       17 |  8392 | `			*pnTok = 4;` |
|       17 |  8393 | `			return nKw;` |
|        - |  8394 | `		}` |
|      ! 0 |  8395 | `	}` |
|  3743419 |  8396 | `	return 0;` |
|  1871720 |  8397 | `}` |
|        - |  8398 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|       16 |  8399 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|        1 |  8400 | `{` |
|       17 |  8401 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|       13 |  8402 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|        - |  8403 | `	}` |
|        5 |  8404 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|        3 |  8405 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|        - |  8406 | `	}` |
|        3 |  8407 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|        9 |  8408 | `}` |
|   210700 |  8409 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  8410 | `{` |
|   210705 |  8411 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8412 | `	ph7_class_attr *pAttr;` |
|        - |  8413 | `	SyString *pName;` |
|        - |  8414 | `	sxi32 rc;` |
|   210705 |  8415 | `	sxu32 nType = 0;` |
|        - |  8416 | `	SyString sTypeClass;` |
|        - |  8417 | `	SyString sTypeText;` |
|        - |  8418 | `	SySet aUnionAlts;` |
|   210705 |  8419 | `	sxi32 iTypeFlags = 0;` |
|   210705 |  8420 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   210705 |  8421 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   210705 |  8422 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  8423 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  8424 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  8425 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   210705 |  8426 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  8427 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  8428 | `	}` |
|        - |  8429 | `	/* Extract visibility level */` |
|   210705 |  8430 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  8431 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   210866 |  8432 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      327 |  8433 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|      327 |  8434 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  8435 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  8436 | `			goto Synchronize;` |
|      327 |  8437 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  8438 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8439 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  8440 | `				&pGen->pIn->sData);` |
|      ! 0 |  8441 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8442 | `				return SXERR_ABORT;` |
|        - |  8443 | `			}` |
|      ! 0 |  8444 | `			goto Synchronize;` |
|      327 |  8445 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  8446 | `			return SXERR_ABORT;` |
|        - |  8447 | `		}` |
|      161 |  8448 | `	}` |
|      ! 0 |  8449 | `loop:` |
|   210709 |  8450 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8451 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 |  8452 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8453 | `			return SXERR_ABORT;` |
|        - |  8454 | `		}` |
|      ! 0 |  8455 | `		goto Synchronize;` |
|        - |  8456 | `	}` |
|   210709 |  8457 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   210709 |  8458 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - |  8459 | `		/* Invalid attribute name */` |
|      ! 0 |  8460 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 |  8461 | `		if( rc == SXERR_ABORT ){` |
|        - |  8462 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8463 | `			return SXERR_ABORT;` |
|        - |  8464 | `		}` |
|      ! 0 |  8465 | `		goto Synchronize;` |
|        - |  8466 | `	}` |
|        - |  8467 | `	/* Peek attribute name */` |
|   210709 |  8468 | `	pName = &pGen->pIn->sData;` |
|        - |  8469 | `	/* Advance the stream cursor */` |
|   210709 |  8470 | `	pGen->pIn++;` |
|   210709 |  8471 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|        - |  8472 | `		/* Invalid declaration */` |
|        3 |  8473 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|        3 |  8474 | `		if( rc == SXERR_ABORT ){` |
|        - |  8475 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8476 | `			return SXERR_ABORT;` |
|        - |  8477 | `		}` |
|        3 |  8478 | `		goto Synchronize;` |
|        - |  8479 | `	}` |
|        - |  8480 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|        - |  8481 | `	 * the read visibility must not be narrower than the set visibility. */` |
|   210707 |  8482 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|       13 |  8483 | `		const char *zAvErr = 0;` |
|       19 |  8484 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|       10 |  8485 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|        2 |  8486 | `			: PH7_CLASS_PROT_PUBLIC;` |
|       13 |  8487 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  8488 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|       13 |  8489 | `		}else if( iProtection > iSetLevel ){` |
|      ! 0 |  8490 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|      ! 0 |  8491 | `		}` |
|       13 |  8492 | `		if( zAvErr ){` |
|      ! 0 |  8493 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|      ! 0 |  8494 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8495 | `				return SXERR_ABORT;` |
|        - |  8496 | `			}` |
|      ! 0 |  8497 | `			goto Synchronize;` |
|        - |  8498 | `		}` |
|        6 |  8499 | `	}` |
|        - |  8500 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - |  8501 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   210707 |  8502 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       43 |  8503 | `		const char *zRoErr = 0;` |
|       43 |  8504 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 |  8505 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       42 |  8506 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 |  8507 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       39 |  8508 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 |  8509 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 |  8510 | `		}` |
|       43 |  8511 | `		if( zRoErr ){` |
|       13 |  8512 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 |  8513 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8514 | `				return SXERR_ABORT;` |
|        - |  8515 | `			}` |
|       13 |  8516 | `			goto Synchronize;` |
|        - |  8517 | `		}` |
|       14 |  8518 | `	}` |
|        - |  8519 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - |  8520 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - |  8521 | `	 * by the type parser. */` |
|   210697 |  8522 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      485 |  8523 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - |  8524 | `			&sTypeText,` |
|      320 |  8525 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      160 |  8526 | `			"Property %z::$%z cannot have type %z",nLine);` |
|      325 |  8527 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8528 | `			return SXERR_ABORT;` |
|      325 |  8529 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  8530 | `			goto Synchronize;` |
|        - |  8531 | `		}` |
|      160 |  8532 | `	}` |
|        - |  8533 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   210697 |  8534 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 |  8535 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8536 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 |  8537 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8538 | `			return SXERR_ABORT;` |
|        - |  8539 | `		}` |
|        3 |  8540 | `		goto Synchronize;` |
|        - |  8541 | `	}` |
|        - |  8542 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - |  8543 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - |  8544 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - |  8545 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - |  8546 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - |  8547 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   210695 |  8548 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 |  8549 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8550 | `			"New expressions are not supported in this context");` |
|        6 |  8551 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8552 | `			return SXERR_ABORT;` |
|        - |  8553 | `		}` |
|        6 |  8554 | `		goto Synchronize;` |
|        - |  8555 | `	}` |
|        - |  8556 | `	/* Allocate a new class attribute */` |
|   210691 |  8557 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   210691 |  8558 | `	if( pAttr ){` |
|   210691 |  8559 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   210691 |  8560 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8561 | `			return SXERR_ABORT;` |
|        - |  8562 | `		}` |
|   105343 |  8563 | `	}` |
|   210691 |  8564 | `	if( pAttr == 0 ){` |
|      ! 0 |  8565 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  8566 | `		return SXERR_ABORT;` |
|        - |  8567 | `	}` |
|   210691 |  8568 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      323 |  8569 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      159 |  8570 | `	}` |
|   210691 |  8571 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - |  8572 | `		SySet *pInstrContainer;` |
|    86123 |  8573 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    86123 |  8574 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - |  8575 | `		{` |
|        - |  8576 | `			/* Delimit the default expression: it ends at the declaration's` |
|        - |  8577 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|        - |  8578 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|        - |  8579 | `			 * compiler would otherwise run into the hook tokens. */` |
|    86123 |  8580 | `			SyToken *pScan = pGen->pIn;` |
|    86123 |  8581 | `			sxi32 iNest = 0;` |
|   188121 |  8582 | `			while( pScan < pGen->pEnd ){` |
|   188121 |  8583 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     7853 |  8584 | `					iNest++;` |
|   184197 |  8585 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     7853 |  8586 | `					iNest--;` |
|   176349 |  8587 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    86123 |  8588 | `					break;` |
|        - |  8589 | `				}` |
|   102003 |  8590 | `				pScan++;` |
|        5 |  8591 | `			}` |
|    86123 |  8592 | `			pGen->pEnd = pScan;` |
|        - |  8593 | `		}` |
|        - |  8594 | `		/* Swap bytecode container */` |
|    86123 |  8595 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    86123 |  8596 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - |  8597 | `		/* Compile attribute value.` |
|        - |  8598 | `		 */` |
|    86123 |  8599 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    86123 |  8600 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  8601 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 |  8602 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8603 | `				return SXERR_ABORT;` |
|        - |  8604 | `			}` |
|      ! 0 |  8605 | `		}` |
|        - |  8606 | `		/* Emit the done instruction */` |
|    86123 |  8607 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    86123 |  8608 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    86123 |  8609 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    86123 |  8610 | `		pGen->pEnd = pSavedDefEnd;` |
|    43059 |  8611 | `	}` |
|        - |  8612 | `	/* All done,install the attribute */` |
|   210691 |  8613 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   210691 |  8614 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8615 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8616 | `		return SXERR_ABORT;` |
|        - |  8617 | `	}` |
|   210691 |  8618 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|        - |  8619 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|        - |  8620 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|       95 |  8621 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|       95 |  8622 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8623 | `			return SXERR_ABORT;` |
|        - |  8624 | `		}` |
|       95 |  8625 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  8626 | `			goto Synchronize;` |
|        - |  8627 | `		}` |
|       95 |  8628 | `		SySetRelease(&aUnionAlts);` |
|       95 |  8629 | `		return SXRET_OK;` |
|        - |  8630 | `	}` |
|   210597 |  8631 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - |  8632 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|        - |  8633 | `		 * wording differs per declaration site) */` |
|      ! 0 |  8634 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  8635 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|        - |  8636 | `				? "Interfaces may only include hooked properties"` |
|        - |  8637 | `				: "Only hooked properties may be declared abstract");` |
|      ! 0 |  8638 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8639 | `			return SXERR_ABORT;` |
|        - |  8640 | `		}` |
|      ! 0 |  8641 | `		goto Synchronize;` |
|        - |  8642 | `	}` |
|   210597 |  8643 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8644 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 |  8645 | `		pGen->pIn++; /* Jump the comma */` |
|        5 |  8646 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  8647 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8648 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8649 | `				pTok--;` |
|      ! 0 |  8650 | `			}` |
|      ! 0 |  8651 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8652 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 |  8653 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8654 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8655 | `				return SXERR_ABORT;` |
|        - |  8656 | `			}` |
|      ! 0 |  8657 | `		}else{` |
|        5 |  8658 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 |  8659 | `				goto loop;` |
|        - |  8660 | `			}` |
|        - |  8661 | `		}` |
|      ! 0 |  8662 | `	}` |
|   210593 |  8663 | `	SySetRelease(&aUnionAlts);` |
|   210593 |  8664 | `	return SXRET_OK;` |
|        9 |  8665 | `Synchronize:` |
|        - |  8666 | `	/* Synchronize with the first semi-colon */` |
|       56 |  8667 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       37 |  8668 | `		pGen->pIn++;` |
|        3 |  8669 | `	}` |
|       22 |  8670 | `	SySetRelease(&aUnionAlts);` |
|       22 |  8671 | `	return SXERR_CORRUPT;` |
|   105355 |  8672 | `}` |
|        - |  8673 | `/*` |
|        - |  8674 | ` * Compile a class method.` |
|        - |  8675 | ` *` |
|        - |  8676 | ` * Refer to the official documentation for more information` |
|        - |  8677 | ` * on the powerful extension introduced by the PH7 engine` |
|        - |  8678 | ` * to the OO subsystem such as full type hinting,method` |
|        - |  8679 | ` * overloading and many more.` |
|        - |  8680 | ` */` |
|  1399790 |  8681 | `static sxi32 GenStateCompileClassMethod(` |
|        - |  8682 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  8683 | `	sxi32 iProtection,   /* Visibility level */` |
|        - |  8684 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - |  8685 | `	int doBody,          /* TRUE to process method body */` |
|        - |  8686 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - |  8687 | `	)` |
|        5 |  8688 | `{` |
|  1399795 |  8689 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1399795 |  8690 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - |  8691 | `	ph7_class_method *pMeth;` |
|        - |  8692 | `	sxi32 iFuncFlags;` |
|        - |  8693 | `	SyString *pName;` |
|        - |  8694 | `	SyToken *pEnd;` |
|        - |  8695 | `	sxi32 rc;` |
|        - |  8696 | `	/* Extract visibility level */` |
|  1399795 |  8697 | `	iProtection = GetProtectionLevel(iProtection);` |
|  1399795 |  8698 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  1399795 |  8699 | `	iFuncFlags = 0;` |
|  1399795 |  8700 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  8701 | `		/* Invalid method name */` |
|      ! 0 |  8702 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8703 | `		if( rc == SXERR_ABORT ){` |
|        - |  8704 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8705 | `			return SXERR_ABORT;` |
|        - |  8706 | `		}` |
|      ! 0 |  8707 | `		goto Synchronize;` |
|        - |  8708 | `	}` |
|  1399795 |  8709 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  8710 | `		/* Return by reference,remember that */` |
|      ! 0 |  8711 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  8712 | `		/* Jump the '&' token */` |
|      ! 0 |  8713 | `		pGen->pIn++;` |
|      ! 0 |  8714 | `	}` |
|  1399795 |  8715 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  8716 | `		/* Invalid method name */` |
|      ! 0 |  8717 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8718 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8719 | `			return SXERR_ABORT;` |
|        - |  8720 | `		}` |
|      ! 0 |  8721 | `		goto Synchronize;` |
|        - |  8722 | `	}` |
|        - |  8723 | `	/* Peek method name */` |
|  1399795 |  8724 | `	pName = &pGen->pIn->sData;` |
|  1399795 |  8725 | `	nLine = pGen->pIn->nLine;` |
|        - |  8726 | `	/* Jump the method name */` |
|  1399795 |  8727 | `	pGen->pIn++;` |
|  1399795 |  8728 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - |  8729 | `		/* Abstract method */` |
|   101051 |  8730 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 |  8731 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8732 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 |  8733 | `				&pClass->sName,pName);` |
|      ! 0 |  8734 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8735 | `				return SXERR_ABORT;` |
|        - |  8736 | `			}` |
|      ! 0 |  8737 | `		}` |
|        - |  8738 | `		/* Assemble method signature only */` |
|   101051 |  8739 | `		doBody = FALSE;` |
|    50523 |  8740 | `	}` |
|  1399795 |  8741 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  8742 | `		/* Syntax error */` |
|      ! 0 |  8743 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 |  8744 | `		if( rc == SXERR_ABORT ){` |
|        - |  8745 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8746 | `			return SXERR_ABORT;` |
|        - |  8747 | `		}` |
|      ! 0 |  8748 | `		goto Synchronize;` |
|        - |  8749 | `	}` |
|        - |  8750 | `	/* Allocate a new class_method instance */` |
|  1399795 |  8751 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  1399795 |  8752 | `	if( pMeth == 0 ){` |
|      ! 0 |  8753 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8754 | `		return SXERR_ABORT;` |
|        - |  8755 | `	}` |
|  1399795 |  8756 | `	pMeth->sFunc.nLine = nKwLine;` |
|  1399795 |  8757 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  1399795 |  8758 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8759 | `		return SXERR_ABORT;` |
|        - |  8760 | `	}` |
|        - |  8761 | `	/* Jump the left parenthesis '(' */` |
|  1399795 |  8762 | `	pGen->pIn++;` |
|  1399795 |  8763 | `	pEnd = 0; /* cc warning */` |
|        - |  8764 | `	/* Delimit the method signature */` |
|  1399795 |  8765 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1399795 |  8766 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8767 | `		/* Syntax error */` |
|        3 |  8768 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 |  8769 | `		if( rc == SXERR_ABORT ){` |
|        - |  8770 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8771 | `			return SXERR_ABORT;` |
|        - |  8772 | `		}` |
|        3 |  8773 | `		goto Synchronize;` |
|        - |  8774 | `	}` |
|        - |  8775 | `	{` |
|  1399793 |  8776 | `		int bIsCtor = 0;` |
|  1399793 |  8777 | `		int bAbstractCtor = 0;` |
|  1399788 |  8778 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   816566 |  8779 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  1347255 |  8780 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   105081 |  8781 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 |  8782 | `				bAbstractCtor = 1;` |
|        2 |  8783 | `			}else{` |
|   105079 |  8784 | `				bIsCtor = 1;` |
|        - |  8785 | `			}` |
|    52538 |  8786 | `		}` |
|  1399793 |  8787 | `		if( pGen->pIn < pEnd ){` |
|        - |  8788 | `			/* Collect method arguments */` |
|   396839 |  8789 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   396839 |  8790 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8791 | `				return SXERR_ABORT;` |
|        - |  8792 | `			}` |
|   198417 |  8793 | `		}` |
|        - |  8794 | `	}` |
|        - |  8795 | `	/* Point past ')' and parse optional return type ': type' */` |
|  1399793 |  8796 | `	pGen->pIn = &pEnd[1];` |
|        - |  8797 | `	{` |
|  1399793 |  8798 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  1399793 |  8799 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  8800 | `			return SXERR_ABORT;` |
|  1399793 |  8801 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 |  8802 | `			goto Synchronize;` |
|        - |  8803 | `		}` |
|        - |  8804 | `	}` |
|        - |  8805 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - |  8806 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - |  8807 | `	 * since we mint real ph7_class_attr entries. */` |
|        - |  8808 | `	{` |
|  1399793 |  8809 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - |  8810 | `		sxu32 i;` |
|  1990995 |  8811 | `		for( i = 0; i < nArg; i++ ){` |
|   591217 |  8812 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - |  8813 | `			ph7_class_attr *pAttr;` |
|   591217 |  8814 | `			sxi32 iAttrFlags = 0;` |
|        - |  8815 | `			int bArgTyped;` |
|   591217 |  8816 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   591133 |  8817 | `				continue;` |
|        - |  8818 | `			}` |
|        - |  8819 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - |  8820 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - |  8821 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       59 |  8822 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       90 |  8823 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       89 |  8824 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 |  8825 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8826 | `					"Cannot declare variadic promoted property");` |
|        3 |  8827 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8828 | `					return SXERR_ABORT;` |
|        - |  8829 | `				}` |
|        3 |  8830 | `				goto Synchronize;` |
|        - |  8831 | `			}` |
|        - |  8832 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - |  8833 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - |  8834 | `			 * appear as an alternative of a union type. */` |
|       87 |  8835 | `			if( bArgTyped ){` |
|      122 |  8836 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       78 |  8837 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       78 |  8838 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       39 |  8839 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       83 |  8840 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8841 | `					return SXERR_ABORT;` |
|       83 |  8842 | `				}else if( rc != SXRET_OK ){` |
|        6 |  8843 | `					goto Synchronize;` |
|        - |  8844 | `				}` |
|       37 |  8845 | `			}` |
|        - |  8846 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       83 |  8847 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 |  8848 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8849 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 |  8850 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8851 | `					return SXERR_ABORT;` |
|        - |  8852 | `				}` |
|        3 |  8853 | `				goto Synchronize;` |
|        - |  8854 | `			}` |
|       81 |  8855 | `			if( bArgTyped ){` |
|       77 |  8856 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       36 |  8857 | `			}` |
|       81 |  8858 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 |  8859 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 |  8860 | `			}` |
|       81 |  8861 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 |  8862 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 |  8863 | `			}` |
|       81 |  8864 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - |  8865 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - |  8866 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 |  8867 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 |  8868 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8869 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 |  8870 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  8871 | `						return SXERR_ABORT;` |
|        - |  8872 | `					}` |
|        3 |  8873 | `					goto Synchronize;` |
|        - |  8874 | `				}` |
|       24 |  8875 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 |  8876 | `			}` |
|       79 |  8877 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|        - |  8878 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|        5 |  8879 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  8880 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8881 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|      ! 0 |  8882 | `						&pClass->sName,&pArg->sName);` |
|      ! 0 |  8883 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  8884 | `						return SXERR_ABORT;` |
|        - |  8885 | `					}` |
|      ! 0 |  8886 | `					goto Synchronize;` |
|        - |  8887 | `				}` |
|        5 |  8888 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|        2 |  8889 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|        2 |  8890 | `			}` |
|       79 |  8891 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       79 |  8892 | `			if( pAttr == 0 ){` |
|      ! 0 |  8893 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8894 | `				return SXERR_ABORT;` |
|        - |  8895 | `			}` |
|       79 |  8896 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       77 |  8897 | `				pAttr->nType = pArg->nType;` |
|       77 |  8898 | `				pAttr->sClass = pArg->sClass;` |
|       77 |  8899 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       77 |  8900 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  8901 | `					sxu32 k;` |
|       20 |  8902 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 |  8903 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 |  8904 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 |  8905 | `					}` |
|        3 |  8906 | `				}` |
|       36 |  8907 | `			}` |
|       79 |  8908 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       79 |  8909 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8910 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8911 | `				return SXERR_ABORT;` |
|        - |  8912 | `			}` |
|       42 |  8913 | `		}` |
|        - |  8914 | `	}` |
|  1399783 |  8915 | `	if( doBody ){` |
|        - |  8916 | `		/* Compile method body */` |
|  1298737 |  8917 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  1298737 |  8918 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8919 | `			return SXERR_ABORT;` |
|        - |  8920 | `		}` |
|        - |  8921 | `		/* The cursor sits just past the body's closing brace */` |
|  1298737 |  8922 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   649371 |  8923 | `	}else{` |
|        - |  8924 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   101051 |  8925 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   101051 |  8926 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    50523 |  8927 | `		}` |
|        - |  8928 | `		/* Only method signature is allowed */` |
|   101051 |  8929 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 |  8930 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8931 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 |  8932 | `				if( rc == SXERR_ABORT ){` |
|        - |  8933 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8934 | `					return SXERR_ABORT;` |
|        - |  8935 | `				}` |
|      ! 0 |  8936 | `				return SXERR_CORRUPT;` |
|        - |  8937 | `			}` |
|        - |  8938 | `	}` |
|        - |  8939 | `	/* All done,install the method */` |
|  1399783 |  8940 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  1399783 |  8941 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8942 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8943 | `		return SXERR_ABORT;` |
|        - |  8944 | `	}` |
|  1399783 |  8945 | `	return SXRET_OK;` |
|        6 |  8946 | `Synchronize:` |
|        - |  8947 | `	/* Synchronize with the first semi-colon */` |
|       40 |  8948 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 |  8949 | `		pGen->pIn++;` |
|        4 |  8950 | `	}` |
|       16 |  8951 | `	return SXERR_CORRUPT;` |
|   699900 |  8952 | `}` |
|        - |  8953 | `/*` |
|        - |  8954 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|        - |  8955 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|        - |  8956 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|        - |  8957 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|        - |  8958 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|        - |  8959 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|        - |  8960 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|        - |  8961 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|        - |  8962 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|        - |  8963 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|        - |  8964 | `` * implicit `$value` formal.`` |
|        - |  8965 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|        - |  8966 | ` */` |
|        - |  8967 | `/*` |
|        - |  8968 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|        - |  8969 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|        - |  8970 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|        - |  8971 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|        - |  8972 | ` * allowed, excluded from the raw object surfaces.` |
|        - |  8973 | ` */` |
|       94 |  8974 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|        1 |  8975 | `{` |
|        - |  8976 | `	SyToken *p;` |
|      345 |  8977 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|      303 |  8978 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      223 |  8979 | `			continue;` |
|        - |  8980 | `		}` |
|        - |  8981 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|       80 |  8982 | `		if( p + 3 < pEnd` |
|       80 |  8983 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       80 |  8984 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|       73 |  8985 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|       66 |  8986 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|       66 |  8987 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       66 |  8988 | `		 && p[3].sData.nByte == pName->nByte` |
|       60 |  8989 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       51 |  8990 | `			return 1;` |
|        - |  8991 | `		}` |
|        - |  8992 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|        - |  8993 | `		 * hook operates on the shared per-instance backing store, so the` |
|        - |  8994 | `		 * property is backed (php compiles a default alongside it). */` |
|       30 |  8995 | `		if( p > pStart` |
|       26 |  8996 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|       12 |  8997 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        2 |  8998 | `		 && p[1].sData.nByte == pName->nByte` |
|        3 |  8999 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        3 |  9000 | `			return 1;` |
|        - |  9001 | `		}` |
|       15 |  9002 | `	}` |
|       43 |  9003 | `	return 0;` |
|       48 |  9004 | `}` |
|        - |  9005 | `/*` |
|        - |  9006 | ` * True when p opens php 8.4's parent-hook call form` |
|        - |  9007 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|        - |  9008 | ` */` |
|      990 |  9009 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|        1 |  9010 | `{` |
|     1167 |  9011 | `	return p + 6 < pEnd` |
|      671 |  9012 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      250 |  9013 | `	 && p->sData.nByte == sizeof("parent")-1` |
|       81 |  9014 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|       11 |  9015 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|        8 |  9016 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|        8 |  9017 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 |  9018 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|        8 |  9019 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 |  9020 | `	 && p[5].sData.nByte == 3` |
|        8 |  9021 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|        6 |  9022 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|     1166 |  9023 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|        1 |  9024 | `}` |
|        - |  9025 | `/*` |
|        - |  9026 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|        - |  9027 | ` * hook body into calls of the parent class's synthesized hook method` |
|        - |  9028 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|        - |  9029 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|        - |  9030 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|        - |  9031 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|        - |  9032 | ` * or SXERR_MEM.` |
|        - |  9033 | ` */` |
|        4 |  9034 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|        - |  9035 | `	SyToken *pStart,SyToken *pEnd)` |
|        1 |  9036 | `{` |
|        5 |  9037 | `	SyToken *p = pStart;` |
|       35 |  9038 | `	while( p < pEnd ){` |
|       31 |  9039 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|        - |  9040 | `			SyToken sTok;` |
|        - |  9041 | `			char zName[384];` |
|        - |  9042 | `			sxu32 nName;` |
|        - |  9043 | `			char *zDup;` |
|        - |  9044 | ``			/* `parent` `::` */`` |
|        5 |  9045 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|        5 |  9046 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|        7 |  9047 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|        4 |  9048 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|        5 |  9049 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|        5 |  9050 | `			if( zDup == 0 ){` |
|      ! 0 |  9051 | `				return SXERR_MEM;` |
|        - |  9052 | `			}` |
|        5 |  9053 | `			sTok = p[3]; /* keep the line info of the property name */` |
|        5 |  9054 | `			sTok.nType = PH7_TK_ID;` |
|        5 |  9055 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|        5 |  9056 | `			sTok.pUserData = 0;` |
|        5 |  9057 | `			SySetPut(pCopy,(const void *)&sTok);` |
|        5 |  9058 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|        5 |  9059 | `			continue;` |
|        - |  9060 | `		}` |
|       27 |  9061 | `		SySetPut(pCopy,(const void *)p);` |
|       27 |  9062 | `		p++;` |
|        1 |  9063 | `	}` |
|        5 |  9064 | `	return SXRET_OK;` |
|        3 |  9065 | `}` |
|       94 |  9066 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  9067 | `{` |
|       95 |  9068 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9069 | `	sxi32 rc;` |
|       95 |  9070 | `	int bRefsSelf = 0;` |
|       95 |  9071 | `	pGen->pIn++; /* Jump '{' */` |
|      253 |  9072 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|        - |  9073 | `		char zHook[384];` |
|        - |  9074 | `		SyString sHookName;` |
|        - |  9075 | `		ph7_class_method *pMeth;` |
|        - |  9076 | `		int bGet;` |
|      159 |  9077 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|      159 |  9078 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       15 |  9079 | `			pGen->pIn++; /* stray ';' between hooks */` |
|       22 |  9080 | `			continue;` |
|        - |  9081 | `		}` |
|      145 |  9082 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - |  9083 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|      ! 0 |  9084 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - |  9085 | `				"By-reference property hooks are not supported for %z::$%z",` |
|      ! 0 |  9086 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 |  9087 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9088 | `				return SXERR_ABORT;` |
|        - |  9089 | `			}` |
|      ! 0 |  9090 | `			return SXERR_CORRUPT;` |
|        - |  9091 | `		}` |
|      145 |  9092 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  9093 | `			goto HookSyntax;` |
|        - |  9094 | `		}` |
|      144 |  9095 | `		if( pGen->pIn->sData.nByte == 3` |
|      145 |  9096 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|       79 |  9097 | `			bGet = 1;` |
|      106 |  9098 | `		}else if( pGen->pIn->sData.nByte == 3` |
|       67 |  9099 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|       67 |  9100 | `			bGet = 0;` |
|       34 |  9101 | `		}else{` |
|      ! 0 |  9102 | `			goto HookSyntax;` |
|        - |  9103 | `		}` |
|      145 |  9104 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|      145 |  9105 | `		sHookName.zString = zHook;` |
|      217 |  9106 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|       72 |  9107 | `			bGet ? "get" : "set",&pAttr->sName);` |
|      145 |  9108 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|        - |  9109 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|        - |  9110 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|        - |  9111 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|        - |  9112 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|        - |  9113 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|       14 |  9114 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|        8 |  9115 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  9116 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - |  9117 | `					"Non-abstract property hook must have a body");` |
|      ! 0 |  9118 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9119 | `					return SXERR_ABORT;` |
|        - |  9120 | `				}` |
|      ! 0 |  9121 | `				return SXERR_CORRUPT;` |
|        - |  9122 | `			}` |
|       15 |  9123 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - |  9124 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|       15 |  9125 | `			if( pMeth == 0 ){` |
|      ! 0 |  9126 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9127 | `				return SXERR_ABORT;` |
|        - |  9128 | `			}` |
|       15 |  9129 | `			pMeth->sFunc.nLine = nHLine;` |
|       15 |  9130 | `			if( !bGet ){` |
|        - |  9131 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|        - |  9132 | `				 * compatible with concrete set-hook implementations (which` |
|        - |  9133 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|        - |  9134 | `				 * type (php: the abstract set's parameter type IS the property` |
|        - |  9135 | `				 * type), so the override contravariance check accepts a typed` |
|        - |  9136 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|        - |  9137 | `				ph7_vm_func_arg sVArg;` |
|        7 |  9138 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        7 |  9139 | `				if( zVName == 0 ){` |
|      ! 0 |  9140 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9141 | `					return SXERR_ABORT;` |
|        - |  9142 | `				}` |
|        7 |  9143 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        7 |  9144 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        7 |  9145 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        7 |  9146 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        7 |  9147 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        7 |  9148 | `				sVArg.nType = pAttr->nType;` |
|        7 |  9149 | `				sVArg.sClass = pAttr->sClass;` |
|        7 |  9150 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|        7 |  9151 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      ! 0 |  9152 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      ! 0 |  9153 | `				}` |
|        7 |  9154 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        3 |  9155 | `			}` |
|       15 |  9156 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       15 |  9157 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9158 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9159 | `				return SXERR_ABORT;` |
|        - |  9160 | `			}` |
|       15 |  9161 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|       15 |  9162 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|        - |  9163 | `		}` |
|      130 |  9164 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|      131 |  9165 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|        - |  9166 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|      ! 0 |  9167 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - |  9168 | `				"Abstract property hook cannot have body");` |
|      ! 0 |  9169 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9170 | `				return SXERR_ABORT;` |
|        - |  9171 | `			}` |
|      ! 0 |  9172 | `			return SXERR_CORRUPT;` |
|        - |  9173 | `		}` |
|      131 |  9174 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - |  9175 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|      131 |  9176 | `		if( pMeth == 0 ){` |
|      ! 0 |  9177 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9178 | `			return SXERR_ABORT;` |
|        - |  9179 | `		}` |
|      131 |  9180 | `		pMeth->sFunc.nLine = nHLine;` |
|      131 |  9181 | `		if( !bGet ){` |
|        - |  9182 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|       61 |  9183 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       17 |  9184 | `				SyToken *pRp = 0;` |
|       17 |  9185 | `				pGen->pIn++;` |
|       17 |  9186 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|       17 |  9187 | `				if( pRp >= pGen->pEnd ){` |
|      ! 0 |  9188 | `					goto HookSyntax;` |
|        - |  9189 | `				}` |
|       17 |  9190 | `				if( pGen->pIn < pRp ){` |
|       17 |  9191 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|       17 |  9192 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9193 | `						return SXERR_ABORT;` |
|        - |  9194 | `					}` |
|        8 |  9195 | `				}` |
|       17 |  9196 | `				pGen->pIn = &pRp[1];` |
|        8 |  9197 | `			}` |
|       61 |  9198 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|        - |  9199 | `				/* Implicit $value formal */` |
|        - |  9200 | `				ph7_vm_func_arg sVArg;` |
|       45 |  9201 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       45 |  9202 | `				if( zVName == 0 ){` |
|      ! 0 |  9203 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9204 | `					return SXERR_ABORT;` |
|        - |  9205 | `				}` |
|       45 |  9206 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       45 |  9207 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       45 |  9208 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       45 |  9209 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       45 |  9210 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       45 |  9211 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|       45 |  9212 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       22 |  9213 | `			}` |
|       30 |  9214 | `		}` |
|      165 |  9215 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - |  9216 | `			/* Block body */` |
|       69 |  9217 | `			SyToken *pBodyStart = pGen->pIn;` |
|       69 |  9218 | `			SyToken *pCloser = 0;` |
|       69 |  9219 | `			int bParentCall = 0;` |
|       69 |  9220 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|       69 |  9221 | `			if( pCloser < pGen->pEnd ){` |
|        - |  9222 | `				SyToken *pScan;` |
|      753 |  9223 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|      687 |  9224 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|        3 |  9225 | `						bParentCall = 1;` |
|        3 |  9226 | `						break;` |
|        - |  9227 | `					}` |
|      343 |  9228 | `				}` |
|       34 |  9229 | `			}` |
|       69 |  9230 | `			if( bParentCall ){` |
|        - |  9231 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|        - |  9232 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|        - |  9233 | `				 * hook method), then continue past the original body. */` |
|        - |  9234 | `				SySet sBody;` |
|        3 |  9235 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|        3 |  9236 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 |  9237 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|        3 |  9238 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  9239 | `					SySetRelease(&sBody);` |
|      ! 0 |  9240 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9241 | `					return SXERR_ABORT;` |
|        - |  9242 | `				}` |
|        3 |  9243 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 |  9244 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        3 |  9245 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        3 |  9246 | `				pGen->pIn = &pCloser[1];` |
|        3 |  9247 | `				pGen->pEnd = pSavedEnd;` |
|        3 |  9248 | `				SySetRelease(&sBody);` |
|        3 |  9249 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9250 | `					return SXERR_ABORT;` |
|        - |  9251 | `				}` |
|        3 |  9252 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|        2 |  9253 | `			}else{` |
|       67 |  9254 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|       67 |  9255 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9256 | `					return SXERR_ABORT;` |
|        - |  9257 | `				}` |
|       67 |  9258 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|        - |  9259 | `			}` |
|       69 |  9260 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       17 |  9261 | `				bRefsSelf = 1;` |
|        9 |  9262 | `			}` |
|      128 |  9263 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        - |  9264 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|        - |  9265 | `			GenBlock *pBlock;` |
|        - |  9266 | `			SySet *pInstrContainer;` |
|        - |  9267 | `			SyToken *pBodyStart;` |
|        - |  9268 | `			SyToken *pExprEnd;` |
|       63 |  9269 | `			SyToken *pSavedEnd = 0;` |
|        - |  9270 | `			SySet sBody;` |
|       63 |  9271 | `			int bParentCall = 0;` |
|       63 |  9272 | `			pGen->pIn++; /* Jump '=>' */` |
|       63 |  9273 | `			pBodyStart = pGen->pIn;` |
|        - |  9274 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|        - |  9275 | `			 * would end the enclosing hook list) and rewrite any` |
|        - |  9276 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|        - |  9277 | `			 * method on a token copy. */` |
|        - |  9278 | `			{` |
|       63 |  9279 | `				sxi32 iNest = 0;` |
|       63 |  9280 | `				pExprEnd = pBodyStart;` |
|      355 |  9281 | `				while( pExprEnd < pGen->pEnd ){` |
|      355 |  9282 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        9 |  9283 | `						iNest++;` |
|      351 |  9284 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        9 |  9285 | `						if( iNest <= 0 ){` |
|      ! 0 |  9286 | `							break;` |
|        - |  9287 | `						}` |
|        9 |  9288 | `						iNest--;` |
|      343 |  9289 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|       63 |  9290 | `						break;` |
|        - |  9291 | `					}` |
|      293 |  9292 | `					pExprEnd++;` |
|        1 |  9293 | `				}` |
|        - |  9294 | `			}` |
|        - |  9295 | `			{` |
|        - |  9296 | `				SyToken *pScan;` |
|      335 |  9297 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|      275 |  9298 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|        3 |  9299 | `						bParentCall = 1;` |
|        3 |  9300 | `						break;` |
|        - |  9301 | `					}` |
|      137 |  9302 | `				}` |
|        - |  9303 | `			}` |
|       63 |  9304 | `			if( bParentCall ){` |
|        3 |  9305 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 |  9306 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|        3 |  9307 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  9308 | `					SySetRelease(&sBody);` |
|      ! 0 |  9309 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9310 | `					return SXERR_ABORT;` |
|        - |  9311 | `				}` |
|        3 |  9312 | `				pSavedEnd = pGen->pEnd;` |
|        3 |  9313 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 |  9314 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        1 |  9315 | `			}` |
|       94 |  9316 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       62 |  9317 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|       63 |  9318 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9319 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|      ! 0 |  9320 | `				return SXERR_ABORT;` |
|        - |  9321 | `			}` |
|       63 |  9322 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       63 |  9323 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|       63 |  9324 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       63 |  9325 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       63 |  9326 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       63 |  9327 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       63 |  9328 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       63 |  9329 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       63 |  9330 | `			if( bParentCall ){` |
|        3 |  9331 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|        3 |  9332 | `				pGen->pEnd = pSavedEnd;` |
|        3 |  9333 | `				SySetRelease(&sBody);` |
|        1 |  9334 | `			}` |
|       63 |  9335 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9336 | `				return SXERR_ABORT;` |
|        - |  9337 | `			}` |
|       63 |  9338 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|       63 |  9339 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       37 |  9340 | `				bRefsSelf = 1;` |
|       18 |  9341 | `			}` |
|       63 |  9342 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       63 |  9343 | `				pGen->pIn++; /* Jump ';' */` |
|       31 |  9344 | `			}` |
|       63 |  9345 | `			if( !bGet ){` |
|        - |  9346 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|        - |  9347 | `				 * the dispatcher consumes the implicit return value — which` |
|        - |  9348 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|        - |  9349 | ``				 * for `$this->NAME = expr`). */`` |
|        3 |  9350 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|        3 |  9351 | `				bRefsSelf = 1;` |
|        1 |  9352 | `			}` |
|       32 |  9353 | `		}else{` |
|      ! 0 |  9354 | `			goto HookSyntax;` |
|        - |  9355 | `		}` |
|      131 |  9356 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|      131 |  9357 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  9358 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9359 | `			return SXERR_ABORT;` |
|        - |  9360 | `		}` |
|      131 |  9361 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        1 |  9362 | `	}` |
|       95 |  9363 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|      ! 0 |  9364 | `		goto HookSyntax;` |
|        - |  9365 | `	}` |
|       95 |  9366 | `	pGen->pIn++; /* Jump '}' */` |
|       95 |  9367 | `	if( !bRefsSelf ){` |
|        - |  9368 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|        - |  9369 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|        - |  9370 | `		 * a default value (compile fatal, php's exact wording). */` |
|       41 |  9371 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|       41 |  9372 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 |  9373 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9374 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|      ! 0 |  9375 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 |  9376 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9377 | `				return SXERR_ABORT;` |
|        - |  9378 | `			}` |
|      ! 0 |  9379 | `			return SXERR_CORRUPT;` |
|        - |  9380 | `		}` |
|       20 |  9381 | `	}` |
|       95 |  9382 | `	return SXRET_OK;` |
|      ! 0 |  9383 | `HookSyntax:` |
|      ! 0 |  9384 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9385 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|      ! 0 |  9386 | `		&pClass->sName,&pAttr->sName);` |
|      ! 0 |  9387 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  9388 | `		return SXERR_ABORT;` |
|        - |  9389 | `	}` |
|      ! 0 |  9390 | `	return SXERR_CORRUPT;` |
|       48 |  9391 | `}` |
|        - |  9392 | `/*` |
|        - |  9393 | ` * Compile an object interface.` |
|        - |  9394 | ` *  According to the PHP language reference manual` |
|        - |  9395 | ` *   Object Interfaces:` |
|        - |  9396 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - |  9397 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - |  9398 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  9399 | ` *   class, but without any of the methods having their contents defined.` |
|        - |  9400 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  9401 | ` */` |
|    46712 |  9402 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 |  9403 | `{` |
|    46717 |  9404 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9405 | `	ph7_class *pClass,*pBase;` |
|        - |  9406 | `	SyToken *pEnd,*pTmp;` |
|        - |  9407 | `	SyString *pName;` |
|        - |  9408 | `	sxi32 nKwrd;` |
|        - |  9409 | `	sxi32 rc;` |
|        - |  9410 | `	/* Jump the 'interface' keyword */` |
|    46717 |  9411 | `	pGen->pIn++;` |
|        - |  9412 | `	/* Extract interface name */` |
|    46717 |  9413 | `	pName = &pGen->pIn->sData;` |
|        - |  9414 | `	/* Advance the stream cursor */` |
|    46717 |  9415 | `	pGen->pIn++;` |
|        - |  9416 | `	/* Build FQN and obtain a raw class */ {` |
|        - |  9417 | `		SyBlob sFQN;` |
|        - |  9418 | `		SyString sFQNStr;` |
|    46717 |  9419 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    46717 |  9420 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    46717 |  9421 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    46717 |  9422 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    46717 |  9423 | `		SyBlobRelease(&sFQN);` |
|        - |  9424 | `	}` |
|    46717 |  9425 | `	if( pClass == 0 ){` |
|      ! 0 |  9426 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9427 | `		return SXERR_ABORT;` |
|        - |  9428 | `	}` |
|    46717 |  9429 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    46717 |  9430 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9431 | `		return SXERR_ABORT;` |
|        - |  9432 | `	}` |
|        - |  9433 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    46717 |  9434 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - |  9435 | `	/* Assume no base class is given */` |
|    46717 |  9436 | `	pBase = 0;` |
|    46717 |  9437 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    15551 |  9438 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    15551 |  9439 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|        - |  9440 | `			SyBlob sResolved;` |
|        - |  9441 | `			SyString sBaseName;` |
|        - |  9442 | `			sxu32 nRefLine;` |
|        - |  9443 | `			/* Extract base interface */` |
|    15551 |  9444 | `			pGen->pIn++;` |
|    15551 |  9445 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    15551 |  9446 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15551 |  9447 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  9448 | `				SyBlobRelease(&sResolved);` |
|      ! 0 |  9449 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9450 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 |  9451 | `					pName);` |
|      ! 0 |  9452 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9453 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9454 | `					return SXERR_ABORT;` |
|        - |  9455 | `				}` |
|      ! 0 |  9456 | `				return SXRET_OK;` |
|        - |  9457 | `			}` |
|    23324 |  9458 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    15546 |  9459 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15551 |  9460 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  9461 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9462 | `			/* Only interfaces is allowed */` |
|    15551 |  9463 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  9464 | `				pBase = pBase->pNextName;` |
|      ! 0 |  9465 | `			}` |
|    15551 |  9466 | `			if( pBase == 0 ){` |
|      ! 0 |  9467 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9468 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 |  9469 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9470 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9471 | `					return SXERR_ABORT;` |
|        - |  9472 | `				}` |
|      ! 0 |  9473 | `			}` |
|    15551 |  9474 | `			SyBlobRelease(&sResolved);` |
|     7773 |  9475 | `		}` |
|     7773 |  9476 | `	}` |
|    46717 |  9477 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  9478 | `		/* Syntax error */` |
|      ! 0 |  9479 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 |  9480 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9481 | `		if( rc == SXERR_ABORT ){` |
|        - |  9482 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9483 | `			return SXERR_ABORT;` |
|        - |  9484 | `		}` |
|      ! 0 |  9485 | `		return SXRET_OK;` |
|        - |  9486 | `	}` |
|    46717 |  9487 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    46717 |  9488 | `	pEnd = 0; /* cc warning */` |
|        - |  9489 | `	/* Delimit the interface body */` |
|    46717 |  9490 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    46717 |  9491 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  9492 | `		/* Syntax error */` |
|      ! 0 |  9493 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 |  9494 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9495 | `		if( rc == SXERR_ABORT ){` |
|        - |  9496 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9497 | `			return SXERR_ABORT;` |
|        - |  9498 | `		}` |
|      ! 0 |  9499 | `		return SXRET_OK;` |
|        - |  9500 | `	}` |
|        - |  9501 | `	/* The delimiter token is the interface body's closing brace */` |
|    46717 |  9502 | `	pClass->nEndLine = pEnd->nLine;` |
|        - |  9503 | `	/* Swap token stream */` |
|    46717 |  9504 | `	pTmp = pGen->pEnd;` |
|    46717 |  9505 | `	pGen->pEnd = pEnd;` |
|        - |  9506 | `	/* Start the parse process` |
|        - |  9507 | `	 * Note (According to the PHP reference manual):` |
|        - |  9508 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - |  9509 | `	 *  Only 'public' visibility is allowed.` |
|        - |  9510 | `	 */` |
|    73877 |  9511 | `	for(;;){` |
|        - |  9512 | `		/* Jump leading/trailing semi-colons */` |
|   248805 |  9513 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   101047 |  9514 | `			pGen->pIn++;` |
|        5 |  9515 | `		}` |
|   147763 |  9516 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  9517 | `			/* End of interface body */` |
|    46713 |  9518 | `			break;` |
|        - |  9519 | `		}` |
|        - |  9520 | `		/* Bind a directly-preceding docblock to this member */` |
|   101055 |  9521 | `		GenStateSetPendingDoc(&(*pGen));` |
|   101055 |  9522 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  9523 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9524 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 |  9525 | `				&pGen->pIn->sData,pName);` |
|      ! 0 |  9526 | `			if( rc == SXERR_ABORT ){` |
|        - |  9527 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9528 | `				return SXERR_ABORT;` |
|        - |  9529 | `			}` |
|      ! 0 |  9530 | `			goto done;` |
|        - |  9531 | `		}` |
|        - |  9532 | `		/* Extract the current keyword */` |
|   101055 |  9533 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101055 |  9534 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - |  9535 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - |  9536 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 |  9537 | `			const char *zKind = "member";` |
|        3 |  9538 | `			SyString *pMemberName = 0;` |
|        3 |  9539 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 |  9540 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 |  9541 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 |  9542 | `					zKind = "constant";` |
|        3 |  9543 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 |  9544 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 |  9545 | `					}` |
|        1 |  9546 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  9547 | `					zKind = "method";` |
|      ! 0 |  9548 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 |  9549 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 |  9550 | `					}` |
|      ! 0 |  9551 | `				}` |
|        1 |  9552 | `			}` |
|        3 |  9553 | `			if( pMemberName ){` |
|        4 |  9554 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 |  9555 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 |  9556 | `			}else{` |
|      ! 0 |  9557 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9558 | `					"Access type for interface %s must be public",zKind);` |
|        - |  9559 | `			}` |
|        3 |  9560 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9561 | `				return SXERR_ABORT;` |
|        - |  9562 | `			}` |
|        3 |  9563 | `			goto done;` |
|        - |  9564 | `		}` |
|   101053 |  9565 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  9566 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9567 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  9568 | `			if( rc == SXERR_ABORT ){` |
|        - |  9569 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9570 | `				return SXERR_ABORT;` |
|        - |  9571 | `			}` |
|      ! 0 |  9572 | `			goto done;` |
|        - |  9573 | `		}` |
|   101053 |  9574 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - |  9575 | `			/* Advance the stream cursor */` |
|   101035 |  9576 | `			pGen->pIn++;` |
|   101030 |  9577 | `			if( pGen->pIn < pGen->pEnd` |
|   101035 |  9578 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|   101030 |  9579 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        - |  9580 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|        - |  9581 | `				 * requirement. The attribute compiler + hook parser handle it` |
|        - |  9582 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|        - |  9583 | `				 * property without hooks is ITS "Interfaces may only include` |
|        - |  9584 | `				 * hooked properties" error). */` |
|      ! 0 |  9585 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 |  9586 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 |  9587 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  9588 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9589 | `						return SXERR_ABORT;` |
|        - |  9590 | `					}` |
|      ! 0 |  9591 | `					goto done;` |
|        - |  9592 | `				}` |
|      ! 0 |  9593 | `				continue;` |
|        - |  9594 | `			}` |
|   101035 |  9595 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        - |  9596 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|        - |  9597 | `				 * '$' also opens a hooked-property requirement. */` |
|      ! 0 |  9598 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|      ! 0 |  9599 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|      ! 0 |  9600 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|      ! 0 |  9601 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 |  9602 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 |  9603 | `					if( rc != SXRET_OK ){` |
|      ! 0 |  9604 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9605 | `							return SXERR_ABORT;` |
|        - |  9606 | `						}` |
|      ! 0 |  9607 | `						goto done;` |
|        - |  9608 | `					}` |
|      ! 0 |  9609 | `					continue;` |
|        - |  9610 | `				}` |
|      ! 0 |  9611 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9612 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  9613 | `				if( rc == SXERR_ABORT ){` |
|        - |  9614 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  9615 | `					return SXERR_ABORT;` |
|        - |  9616 | `				}` |
|      ! 0 |  9617 | `				goto done;` |
|        - |  9618 | `			}` |
|   101035 |  9619 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101035 |  9620 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|        - |  9621 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|        - |  9622 | `				 * hooked-property requirement (PHP 8.4). */` |
|        4 |  9623 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|        5 |  9624 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|        7 |  9625 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|        2 |  9626 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|        5 |  9627 | `					if( rc != SXRET_OK ){` |
|      ! 0 |  9628 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9629 | `							return SXERR_ABORT;` |
|        - |  9630 | `						}` |
|      ! 0 |  9631 | `						goto done;` |
|        - |  9632 | `					}` |
|        5 |  9633 | `					continue;` |
|        - |  9634 | `				}` |
|      ! 0 |  9635 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9636 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  9637 | `				if( rc == SXERR_ABORT ){` |
|        - |  9638 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  9639 | `					return SXERR_ABORT;` |
|        - |  9640 | `				}` |
|      ! 0 |  9641 | `				goto done;` |
|        - |  9642 | `			}` |
|    50513 |  9643 | `		}` |
|   101049 |  9644 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - |  9645 | `			/* Parse constant */` |
|       16 |  9646 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       16 |  9647 | `			if( rc != SXRET_OK ){` |
|        3 |  9648 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9649 | `					return SXERR_ABORT;` |
|        - |  9650 | `				}` |
|        3 |  9651 | `				goto done;` |
|        - |  9652 | `			}` |
|        7 |  9653 | `		}else{` |
|   101035 |  9654 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   101035 |  9655 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - |  9656 | `				/* Static method,record that */` |
|    11657 |  9657 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - |  9658 | `				/* Advance the stream cursor */` |
|    11657 |  9659 | `				pGen->pIn++;` |
|    11652 |  9660 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11657 |  9661 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  9662 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9663 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  9664 | `						if( rc == SXERR_ABORT ){` |
|        - |  9665 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 |  9666 | `							return SXERR_ABORT;` |
|        - |  9667 | `						}` |
|      ! 0 |  9668 | `						goto done;` |
|        - |  9669 | `				}` |
|     5826 |  9670 | `			}` |
|        - |  9671 | `			/* Process method signature (no body for interface methods) */` |
|   101035 |  9672 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   101035 |  9673 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9674 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9675 | `					return SXERR_ABORT;` |
|        - |  9676 | `				}` |
|      ! 0 |  9677 | `				goto done;` |
|        - |  9678 | `			}` |
|        - |  9679 | `		}` |
|        5 |  9680 | `	}` |
|        - |  9681 | `	/* Install the interface */` |
|    46713 |  9682 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    46713 |  9683 | `	if( rc == SXRET_OK && pBase ){` |
|        - |  9684 | `		/* Inherit from the base interface */` |
|    15551 |  9685 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     7773 |  9686 | `	}` |
|    46713 |  9687 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9688 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9689 | `		return SXERR_ABORT;` |
|        - |  9690 | `	}` |
|    23354 |  9691 | `done:` |
|        - |  9692 | `	/* Point beyond the interface body */` |
|    46717 |  9693 | `	pGen->pIn  = &pEnd[1];` |
|    46717 |  9694 | `	pGen->pEnd = pTmp;` |
|    46717 |  9695 | `	return PH7_OK;` |
|    23361 |  9696 | `}` |
|        - |  9697 | `/*` |
|        - |  9698 | ` * Compile a user-defined class.` |
|        - |  9699 | ` * According to the PHP language reference manual` |
|        - |  9700 | ` *  class` |
|        - |  9701 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - |  9702 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - |  9703 | ` *  of the properties and methods belonging to the class.` |
|        - |  9704 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - |  9705 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - |  9706 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - |  9707 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  9708 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - |  9709 | ` *  (called "methods").` |
|        - |  9710 | ` */` |
|        - |  9711 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - |  9712 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - |  9713 | `struct TraitUseEntry {` |
|        - |  9714 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - |  9715 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - |  9716 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - |  9717 | `};` |
|        - |  9718 | `/*` |
|        - |  9719 | ` * Validate that methods implementing interface contracts have compatible` |
|        - |  9720 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - |  9721 | ` */` |
|   219196 |  9722 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9723 | `{` |
|        - |  9724 | `	ph7_class **apIface;` |
|        - |  9725 | `	sxu32 nIface,i;` |
|        - |  9726 | `	sxi32 rc;` |
|   219201 |  9727 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 |  9728 | `		return SXRET_OK;` |
|        - |  9729 | `	}` |
|   219201 |  9730 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   219201 |  9731 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   440921 |  9732 | `	for(i = 0; i < nIface; i++){` |
|   221725 |  9733 | `		ph7_class *pIface = apIface[i];` |
|        - |  9734 | `		SyHashEntry *pEntry;` |
|   221725 |  9735 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   521373 |  9736 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   299653 |  9737 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |  9738 | `			ph7_class_method *pImplMeth;` |
|   299653 |  9739 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - |  9740 | `			/* Find the implementing method in the class */` |
|   299653 |  9741 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   299653 |  9742 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       23 |  9743 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - |  9744 | `			}` |
|        - |  9745 | `			/* Check visibility: interface methods must be implemented as public */` |
|   299635 |  9746 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 |  9747 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9748 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 |  9749 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 |  9750 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9751 | `					return SXERR_ABORT;` |
|        - |  9752 | `				}` |
|        1 |  9753 | `			}` |
|        - |  9754 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - |  9755 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - |  9756 | `			 */` |
|        - |  9757 | `			{` |
|   299635 |  9758 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   299635 |  9759 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   299635 |  9760 | `				int sigError = 0;` |
|   299635 |  9761 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 |  9762 | `					sigError = 1;` |
|   299634 |  9763 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - |  9764 | `					/* Extra parameters must all have default values */` |
|        6 |  9765 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - |  9766 | `					sxu32 k;` |
|        8 |  9767 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|        6 |  9768 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 |  9769 | `							sigError = 1;` |
|        3 |  9770 | `							break;` |
|        - |  9771 | `						}` |
|        2 |  9772 | `					}` |
|        2 |  9773 | `				}` |
|   299635 |  9774 | `				if( sigError ){` |
|        - |  9775 | `					SyBlob sImplSig, sIfaceSig;` |
|        - |  9776 | `					ph7_vm_func_arg *aArgs;` |
|        - |  9777 | `					sxu32 j;` |
|        6 |  9778 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 |  9779 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - |  9780 | `					/* Build implementing method signature */` |
|        6 |  9781 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 |  9782 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 |  9783 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 |  9784 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 |  9785 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9786 | `					}` |
|        - |  9787 | `					/* Build interface method signature */` |
|        6 |  9788 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 |  9789 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 |  9790 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 |  9791 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 |  9792 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9793 | `					}` |
|        8 |  9794 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9795 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 |  9796 | `						&pClass->sName,pMName,` |
|        4 |  9797 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 |  9798 | `						&pIface->sName,pMName,` |
|        4 |  9799 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 |  9800 | `					SyBlobRelease(&sImplSig);` |
|        6 |  9801 | `					SyBlobRelease(&sIfaceSig);` |
|        6 |  9802 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9803 | `						return SXERR_ABORT;` |
|        - |  9804 | `					}` |
|        2 |  9805 | `				}` |
|        - |  9806 | `			}` |
|        5 |  9807 | `		}` |
|   110865 |  9808 | `	}` |
|   219201 |  9809 | `	return SXRET_OK;` |
|   109603 |  9810 | `}` |
|        - |  9811 | `/*` |
|        - |  9812 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|        - |  9813 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|        - |  9814 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|        - |  9815 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|        - |  9816 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|        - |  9817 | ` * means that specific hook is still missing.` |
|        - |  9818 | ` */` |
|       38 |  9819 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|        5 |  9820 | `{` |
|        - |  9821 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        - |  9822 | `	ph7_class_attr *pProp;` |
|       38 |  9823 | `	if( pMName->nByte <= nPfx` |
|       27 |  9824 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|        4 |  9825 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|       36 |  9826 | `		return 0; /* not a hook stub */` |
|        - |  9827 | `	}` |
|        7 |  9828 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|        7 |  9829 | `	return pProp != 0` |
|        6 |  9830 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|        3 |  9831 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|       24 |  9832 | `}` |
|        - |  9833 | `/*` |
|        - |  9834 | ` * Append an abstract member's display name to the message blob, translating a` |
|        - |  9835 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|        - |  9836 | ` */` |
|       16 |  9837 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|        4 |  9838 | `{` |
|        - |  9839 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|       16 |  9840 | `	if( pMName->nByte > nPfx` |
|       12 |  9841 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|      ! 0 |  9842 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|      ! 0 |  9843 | `		SyBlobAppend(pMsg,"$",1);` |
|      ! 0 |  9844 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|      ! 0 |  9845 | `		SyBlobAppend(pMsg,"::",2);` |
|      ! 0 |  9846 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|      ! 0 |  9847 | `		return;` |
|        - |  9848 | `	}` |
|       20 |  9849 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|       12 |  9850 | `}` |
|        - |  9851 | `/*` |
|        - |  9852 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - |  9853 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - |  9854 | ` */` |
|   219196 |  9855 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9856 | `{` |
|        - |  9857 | `	ph7_class_method *pMeth;` |
|        - |  9858 | `	SyHashEntry *pEntry;` |
|        - |  9859 | `	sxu32 nAbstract;` |
|        - |  9860 | `	SyBlob sMsg;` |
|        - |  9861 | `	sxi32 rc;` |
|        - |  9862 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   219201 |  9863 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     7817 |  9864 | `		return SXRET_OK;` |
|        - |  9865 | `	}` |
|        - |  9866 | `	/* Count abstract methods */` |
|   211389 |  9867 | `	nAbstract = 0;` |
|   211389 |  9868 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  3189667 |  9869 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  2872591 |  9870 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  2872591 |  9871 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       27 |  9872 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|        7 |  9873 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - |  9874 | `			}` |
|       20 |  9875 | `			nAbstract++;` |
|        8 |  9876 | `		}` |
|        5 |  9877 | `	}` |
|   211389 |  9878 | `	if( nAbstract == 0 ){` |
|   211375 |  9879 | `		return SXRET_OK;` |
|        - |  9880 | `	}` |
|        - |  9881 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 |  9882 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 |  9883 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - |  9884 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 |  9885 | `		&pClass->sName,nAbstract,` |
|        7 |  9886 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 |  9887 | `		(nAbstract > 1 ? "s" : ""));` |
|        - |  9888 | `	/* Second pass: list methods with origins */` |
|        - |  9889 | `	{` |
|       18 |  9890 | `		sxu32 nListed = 0;` |
|       18 |  9891 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 |  9892 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 |  9893 | `			ph7_class *pOrigin = 0;` |
|        - |  9894 | `			SyString *pMName;` |
|       22 |  9895 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 |  9896 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 |  9897 | `				continue;` |
|        - |  9898 | `			}` |
|       20 |  9899 | `			pMName = &pMeth->sFunc.sName;` |
|       20 |  9900 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|      ! 0 |  9901 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - |  9902 | `			}` |
|       20 |  9903 | `			if( nListed > 0 ){` |
|        3 |  9904 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 |  9905 | `			}` |
|        - |  9906 | `			/* Find the origin of this abstract method.` |
|        - |  9907 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - |  9908 | `			 * inheritance chains) take precedence for interface-declared` |
|        - |  9909 | `			 * methods. Abstract class methods only win when the class` |
|        - |  9910 | `			 * itself declared the abstract method (not inherited from` |
|        - |  9911 | `			 * an interface). Trait methods are adopted into the using` |
|        - |  9912 | `			 * class's namespace.` |
|        - |  9913 | `			 */` |
|        - |  9914 | `			{` |
|        - |  9915 | `				ph7_class **apIface;` |
|        - |  9916 | `				ph7_class **apTrait;` |
|        - |  9917 | `				ph7_class *pWalk;` |
|        - |  9918 | `				sxu32 i;` |
|        - |  9919 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - |  9920 | `				 * (one that was written in the class body, not inherited from an` |
|        - |  9921 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - |  9922 | `				 */` |
|       20 |  9923 | `				if( pClass->pBase ){` |
|       11 |  9924 | `					pWalk = pClass->pBase;` |
|       19 |  9925 | `					while( pWalk ){` |
|        - |  9926 | `						ph7_class_method *pParentMeth;` |
|       13 |  9927 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       13 |  9928 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - |  9929 | `							/* Exclude methods that came from an interface anywhere` |
|        - |  9930 | `							 * in this class's ancestor chain.` |
|        - |  9931 | `							 */` |
|       13 |  9932 | `							int fromIface = 0;` |
|       13 |  9933 | `							ph7_class *pAnc = pWalk;` |
|       17 |  9934 | `							while( pAnc ){` |
|        - |  9935 | `								ph7_class **apPI;` |
|        - |  9936 | `								sxu32 j;` |
|       15 |  9937 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       15 |  9938 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 |  9939 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 |  9940 | `										fromIface = 1;` |
|       10 |  9941 | `										break;` |
|        - |  9942 | `									}` |
|      ! 0 |  9943 | `								}` |
|       15 |  9944 | `								if( fromIface ) break;` |
|        6 |  9945 | `								pAnc = pAnc->pBase;` |
|        2 |  9946 | `							}` |
|       13 |  9947 | `							if( !fromIface ){` |
|        3 |  9948 | `								pOrigin = pWalk;` |
|        3 |  9949 | `								break;` |
|        - |  9950 | `							}` |
|        4 |  9951 | `						}` |
|       10 |  9952 | `						pWalk = pWalk->pBase;` |
|        2 |  9953 | `					}` |
|        4 |  9954 | `				}` |
|        - |  9955 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - |  9956 | `				 * each interface's own parent chain for the deepest origin.` |
|        - |  9957 | `				 */` |
|       20 |  9958 | `				if( !pOrigin ){` |
|       18 |  9959 | `					pWalk = pClass;` |
|       40 |  9960 | `					while( pWalk && !pOrigin ){` |
|       26 |  9961 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 |  9962 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 |  9963 | `							ph7_class *pIface = apIface[i];` |
|       16 |  9964 | `							ph7_class *pDeepest = 0;` |
|       28 |  9965 | `							while( pIface ){` |
|       16 |  9966 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 |  9967 | `									pDeepest = pIface;` |
|        6 |  9968 | `								}` |
|       16 |  9969 | `								pIface = pIface->pBase;` |
|        4 |  9970 | `							}` |
|       16 |  9971 | `							if( pDeepest ){` |
|       16 |  9972 | `								pOrigin = pDeepest;` |
|       16 |  9973 | `								break;` |
|        - |  9974 | `							}` |
|      ! 0 |  9975 | `						}` |
|       26 |  9976 | `						pWalk = pWalk->pBase;` |
|        4 |  9977 | `					}` |
|        7 |  9978 | `				}` |
|        - |  9979 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 |  9980 | `				if( !pOrigin ){` |
|        3 |  9981 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 |  9982 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 |  9983 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 |  9984 | `							pOrigin = pClass;` |
|        3 |  9985 | `							break;` |
|        - |  9986 | `						}` |
|      ! 0 |  9987 | `					}` |
|        1 |  9988 | `				}` |
|        - |  9989 | `			}` |
|       20 |  9990 | `			if( pOrigin ){` |
|       20 |  9991 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|       12 |  9992 | `			}else{` |
|        - |  9993 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 |  9994 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|        - |  9995 | `			}` |
|       20 |  9996 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|       20 |  9997 | `			nListed++;` |
|        4 |  9998 | `		}` |
|        - |  9999 | `	}` |
|       18 | 10000 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 | 10001 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 | 10002 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 | 10003 | `	SyBlobRelease(&sMsg);` |
|       18 | 10004 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 10005 | `		return SXERR_ABORT;` |
|        - | 10006 | `	}` |
|       18 | 10007 | `	return SXRET_OK;` |
|   109603 | 10008 | `}` |
|        - | 10009 | `/*` |
|        - | 10010 | ` * Parse a class/interface name reference from the current token stream.` |
|        - | 10011 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - | 10012 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - | 10013 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - | 10014 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - | 10015 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - | 10016 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - | 10017 | ` */` |
|   192218 | 10018 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 | 10019 | `{` |
|   192223 | 10020 | `	int isAbsolute = 0;` |
|   192223 | 10021 | `	SyToken *pStart = pGen->pIn;` |
|        - | 10022 | `	SyBlob sName;` |
|   192223 | 10023 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4473 | 10024 | `		isAbsolute = 1;` |
|     4473 | 10025 | `		pGen->pIn++;` |
|     2234 | 10026 | `	}` |
|   192223 | 10027 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        8 | 10028 | `		pGen->pIn = pStart;` |
|        8 | 10029 | `		return SXERR_INVALID;` |
|        - | 10030 | `	}` |
|   192217 | 10031 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   192217 | 10032 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   192217 | 10033 | `	pGen->pIn++;` |
|   288339 | 10034 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    96132 | 10035 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       16 | 10036 | `		SyBlobAppend(&sName,"\\",1);` |
|       16 | 10037 | `		pGen->pIn++;` |
|       16 | 10038 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       16 | 10039 | `		pGen->pIn++;` |
|        2 | 10040 | `	}` |
|   192217 | 10041 | `	if( isAbsolute ){` |
|     4471 | 10042 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2238 | 10043 | `	}else{` |
|        - | 10044 | `		SyString sRaw;` |
|   187751 | 10045 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   187751 | 10046 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - | 10047 | `	}` |
|   192217 | 10048 | `	SyBlobRelease(&sName);` |
|   192217 | 10049 | `	return SXRET_OK;` |
|    96114 | 10050 | `}` |
|        - | 10051 | `/*` |
|        - | 10052 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - | 10053 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - | 10054 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - | 10055 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - | 10056 | ` * either direction cannot run unbounded.` |
|        - | 10057 | ` */` |
|        - | 10058 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    46808 | 10059 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 | 10060 | `{` |
|        - | 10061 | `	ph7_class **apParent;` |
|        - | 10062 | `	sxu32 n;` |
|   120847 | 10063 | `	while( pInterface ){` |
|    81817 | 10064 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 | 10065 | `			return FALSE;` |
|        - | 10066 | `		}` |
|   101256 | 10067 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    38878 | 10068 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7783 | 10069 | `			return TRUE;` |
|        - | 10070 | `		}` |
|    74039 | 10071 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    74039 | 10072 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|      ! 0 | 10073 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 | 10074 | `				return TRUE;` |
|        - | 10075 | `			}` |
|      ! 0 | 10076 | `		}` |
|    74039 | 10077 | `		pInterface = pInterface->pBase;` |
|    74039 | 10078 | `		iDepth++;` |
|        5 | 10079 | `	}` |
|    39035 | 10080 | `	return FALSE;` |
|    23409 | 10081 | `}` |
|    46808 | 10082 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 | 10083 | `{` |
|    46813 | 10084 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 | 10085 | `}` |
|        - | 10086 | `/*` |
|        - | 10087 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - | 10088 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - | 10089 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - | 10090 | ` */` |
|     7778 | 10091 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 | 10092 | `{` |
|     7787 | 10093 | `	while( pBase ){` |
|       10 | 10094 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 | 10095 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 | 10096 | `			return TRUE;` |
|        - | 10097 | `		}` |
|       10 | 10098 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 | 10099 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 | 10100 | `			return TRUE;` |
|        - | 10101 | `		}` |
|        5 | 10102 | `		pBase = pBase->pBase;` |
|        1 | 10103 | `	}` |
|     7779 | 10104 | `	return FALSE;` |
|     3894 | 10105 | `}` |
|        - | 10106 | `/*` |
|        - | 10107 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - | 10108 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - | 10109 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - | 10110 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - | 10111 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - | 10112 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - | 10113 | ` * pClass->aEnumCases for cases().` |
|        - | 10114 | ` */` |
|     7810 | 10115 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 10116 | `{` |
|     7815 | 10117 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10118 | `	SySet *pInstrContainer;` |
|        - | 10119 | `	ph7_class_attr *pCase;` |
|        - | 10120 | `	SyString *pName;` |
|        - | 10121 | `	sxi32 rc;` |
|     7815 | 10122 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|     7815 | 10123 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 10124 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 10125 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|      ! 0 | 10126 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10127 | `			return SXERR_ABORT;` |
|        - | 10128 | `		}` |
|      ! 0 | 10129 | `		goto Synchronize;` |
|        - | 10130 | `	}` |
|     7815 | 10131 | `	pName = &pGen->pIn->sData;` |
|        - | 10132 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|     7815 | 10133 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 | 10134 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10135 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 | 10136 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10137 | `			return SXERR_ABORT;` |
|        - | 10138 | `		}` |
|      ! 0 | 10139 | `		goto Synchronize;` |
|        - | 10140 | `	}` |
|     7815 | 10141 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 10142 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|     7815 | 10143 | `	if( pCase == 0 ){` |
|      ! 0 | 10144 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10145 | `		return SXERR_ABORT;` |
|        - | 10146 | `	}` |
|     7815 | 10147 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|     7815 | 10148 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10149 | `		return SXERR_ABORT;` |
|        - | 10150 | `	}` |
|     7815 | 10151 | `	pGen->pIn++; /* Jump the case name */` |
|     7815 | 10152 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|     7801 | 10153 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 | 10154 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 10155 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|        6 | 10156 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10157 | `				return SXERR_ABORT;` |
|        - | 10158 | `			}` |
|        6 | 10159 | `			goto Synchronize;` |
|        - | 10160 | `		}` |
|     7797 | 10161 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - | 10162 | `		/* Compile the backing value expression into the case's own container` |
|        - | 10163 | `		 * (same technique as class constants). */` |
|     7797 | 10164 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7797 | 10165 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|     7797 | 10166 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7797 | 10167 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 10168 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 10169 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|      ! 0 | 10170 | `		}` |
|     7797 | 10171 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7797 | 10172 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7797 | 10173 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10174 | `			return SXERR_ABORT;` |
|        - | 10175 | `		}` |
|     3901 | 10176 | `	}else{` |
|       17 | 10177 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 | 10178 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 10179 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|      ! 0 | 10180 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10181 | `				return SXERR_ABORT;` |
|        - | 10182 | `			}` |
|      ! 0 | 10183 | `			goto Synchronize;` |
|        - | 10184 | `		}` |
|        - | 10185 | `	}` |
|     7811 | 10186 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|     7811 | 10187 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10188 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10189 | `		return SXERR_ABORT;` |
|        - | 10190 | `	}` |
|     7811 | 10191 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|     7811 | 10192 | `	return SXRET_OK;` |
|        2 | 10193 | `Synchronize:` |
|        - | 10194 | `	/* Synchronize with the first semi-colon */` |
|       14 | 10195 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       10 | 10196 | `		pGen->pIn++;` |
|        2 | 10197 | `	}` |
|        6 | 10198 | `	return SXERR_CORRUPT;` |
|     3910 | 10199 | `}` |
|        - | 10200 | `/*` |
|        - | 10201 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|        - | 10202 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|        - | 10203 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|        - | 10204 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|        - | 10205 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|        - | 10206 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|        - | 10207 | ` * pointers into it (see the constructor-promotion precedent above).` |
|        - | 10208 | ` */` |
|     3908 | 10209 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 10210 | `{` |
|        - | 10211 | `	SyToken *pSaveIn,*pSaveEnd;` |
|        - | 10212 | `	const char *zBack;` |
|        - | 10213 | `	SySet sToken;` |
|        - | 10214 | `	char *zSrc;` |
|        - | 10215 | `	sxu32 nSrc,nMax;` |
|     3913 | 10216 | `	sxi32 rc = SXRET_OK;` |
|     3913 | 10217 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|     3908 | 10218 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|     3913 | 10219 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|     3913 | 10220 | `	if( zSrc == 0 ){` |
|      ! 0 | 10221 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10222 | `		return SXERR_ABORT;` |
|        - | 10223 | `	}` |
|     3913 | 10224 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|     3913 | 10225 | `	if( pClass->nEnumBacking != 0 ){` |
|     5849 | 10226 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        - | 10227 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|        - | 10228 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|        - | 10229 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|     1948 | 10230 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|     1953 | 10231 | `	}else{` |
|       21 | 10232 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        6 | 10233 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|        - | 10234 | `	}` |
|     3913 | 10235 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     3913 | 10236 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|     3913 | 10237 | `	pSaveIn = pGen->pIn;` |
|     3913 | 10238 | `	pSaveEnd = pGen->pEnd;` |
|     3913 | 10239 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     3913 | 10240 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|    15613 | 10241 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|    11705 | 10242 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|        5 | 10243 | `	}` |
|     3913 | 10244 | `	pGen->pIn = pSaveIn;` |
|     3913 | 10245 | `	pGen->pEnd = pSaveEnd;` |
|     3913 | 10246 | `	SySetRelease(&sToken);` |
|     3913 | 10247 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|     1959 | 10248 | `}` |
|        - | 10249 | `/*` |
|        - | 10250 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - | 10251 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - | 10252 | ` */` |
|        - | 10253 | `static const char *azEnumBannedMagic[] = {` |
|        - | 10254 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - | 10255 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - | 10256 | `};` |
|        - | 10257 | `/*` |
|        - | 10258 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - | 10259 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - | 10260 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - | 10261 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - | 10262 | ` * and before the class is installed.` |
|        - | 10263 | ` */` |
|     3908 | 10264 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        5 | 10265 | `{` |
|        - | 10266 | `	SyHashEntry *pEntry;` |
|        - | 10267 | `	sxi32 rc;` |
|        - | 10268 | `	sxu32 n;` |
|        - | 10269 | `	/* php: "Enum %s cannot include properties" */` |
|     3913 | 10270 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    11723 | 10271 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     7817 | 10272 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     7817 | 10273 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 | 10274 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 | 10275 | `				"Enum %z cannot include properties",&pClass->sName);` |
|        3 | 10276 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10277 | `				return SXERR_ABORT;` |
|        - | 10278 | `			}` |
|        3 | 10279 | `			break;` |
|        - | 10280 | `		}` |
|        5 | 10281 | `	}` |
|        - | 10282 | `	/* php: "Enum %s cannot include magic method %s" */` |
|    54717 | 10283 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|    76206 | 10284 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|    50809 | 10285 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 | 10286 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 10287 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|      ! 0 | 10288 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10289 | `				return SXERR_ABORT;` |
|        - | 10290 | `			}` |
|      ! 0 | 10291 | `		}` |
|    25407 | 10292 | `	}` |
|        - | 10293 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - | 10294 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - | 10295 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - | 10296 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - | 10297 | `	{` |
|        - | 10298 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - | 10299 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - | 10300 | `		ph7_class_attr *pAttr;` |
|     3913 | 10301 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 10302 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3913 | 10303 | `		if( pAttr == 0 ){` |
|      ! 0 | 10304 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10305 | `			return SXERR_ABORT;` |
|        - | 10306 | `		}` |
|     3913 | 10307 | `		pAttr->nType = MEMOBJ_STRING;` |
|     3913 | 10308 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|     3913 | 10309 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|     3913 | 10310 | `		if( pClass->nEnumBacking != 0 ){` |
|     3901 | 10311 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 10312 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3901 | 10313 | `			if( pAttr == 0 ){` |
|      ! 0 | 10314 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10315 | `				return SXERR_ABORT;` |
|        - | 10316 | `			}` |
|     3901 | 10317 | `			pAttr->nType = pClass->nEnumBacking;` |
|     3901 | 10318 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 | 10319 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|        4 | 10320 | `			}else{` |
|     3895 | 10321 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - | 10322 | `			}` |
|     3901 | 10323 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|     1948 | 10324 | `		}` |
|        - | 10325 | `	}` |
|     3913 | 10326 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|     1959 | 10327 | `}` |
|        - | 10328 | `/*` |
|        - | 10329 | ` * Compile a class declaration, named or anonymous.` |
|        - | 10330 | ` *` |
|        - | 10331 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - | 10332 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - | 10333 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - | 10334 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - | 10335 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - | 10336 | ` * implements, body, install) is shared by both paths.` |
|        - | 10337 | ` */` |
|   219240 | 10338 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - | 10339 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 | 10340 | `{` |
|   219245 | 10341 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10342 | `	ph7_class *pClass,*pBase;` |
|        - | 10343 | `	SyToken *pEnd,*pTmp;` |
|        - | 10344 | `	sxi32 iProtection;` |
|        - | 10345 | `	SySet aInterfaces;` |
|        - | 10346 | `	SySet aUseEntries;` |
|        - | 10347 | `	sxi32 iAttrflags;` |
|        - | 10348 | `	SyString *pName;` |
|        - | 10349 | `	sxi32 nKwrd;` |
|        - | 10350 | `	sxi32 rc;` |
|        - | 10351 | `	/* Jump the 'class' keyword */` |
|   219245 | 10352 | `	pGen->pIn++;` |
|   219245 | 10353 | `	if( pAnonName ){` |
|        - | 10354 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - | 10355 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - | 10356 | `		 * then use the synthesized name. */` |
|       32 | 10357 | `		*ppArgStart = *ppArgEnd = 0;` |
|       32 | 10358 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 | 10359 | `			pGen->pIn++; /* Jump '(' */` |
|        7 | 10360 | `			*ppArgStart = pGen->pIn;` |
|       10 | 10361 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 | 10362 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 | 10363 | `			pGen->pIn = *ppArgEnd;` |
|        7 | 10364 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 | 10365 | `		}` |
|       32 | 10366 | `		pName = pAnonName;` |
|       32 | 10367 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       18 | 10368 | `	}else{` |
|   219217 | 10369 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - | 10370 | `			/* Syntax error */` |
|      ! 0 | 10371 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 | 10372 | `			if( rc == SXERR_ABORT ){` |
|        - | 10373 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 10374 | `				return SXERR_ABORT;` |
|        - | 10375 | `			}` |
|        - | 10376 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 | 10377 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 | 10378 | `				pGen->pIn++;` |
|      ! 0 | 10379 | `			}` |
|      ! 0 | 10380 | `			return SXRET_OK;` |
|        - | 10381 | `		}` |
|        - | 10382 | `		/* Extract class name */` |
|   219217 | 10383 | `		pName = &pGen->pIn->sData;` |
|        - | 10384 | `		/* Advance the stream cursor */` |
|   219217 | 10385 | `		pGen->pIn++;` |
|        - | 10386 | `		/* Build FQN and obtain a raw class */ {` |
|        - | 10387 | `			SyBlob sFQN;` |
|        - | 10388 | `			SyString sFQNStr;` |
|   219217 | 10389 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   219217 | 10390 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   219217 | 10391 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   219217 | 10392 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   219217 | 10393 | `			SyBlobRelease(&sFQN);` |
|        - | 10394 | `		}` |
|        - | 10395 | `	}` |
|   219245 | 10396 | `	if( pClass == 0 ){` |
|      ! 0 | 10397 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10398 | `		return SXERR_ABORT;` |
|        - | 10399 | `	}` |
|   219240 | 10400 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|     3917 | 10401 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - | 10402 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|     3903 | 10403 | `		pGen->pIn++; /* Jump ':' */` |
|     3898 | 10404 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3903 | 10405 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        7 | 10406 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        7 | 10407 | `			pGen->pIn++;` |
|     3896 | 10408 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3897 | 10409 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|     3895 | 10410 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|     3895 | 10411 | `			pGen->pIn++;` |
|     1950 | 10412 | `		}else{` |
|        3 | 10413 | `			SyToken *pTok = pGen->pIn;` |
|        3 | 10414 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 | 10415 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 | 10416 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 | 10417 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10418 | `				return SXERR_ABORT;` |
|        - | 10419 | `			}` |
|        3 | 10420 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 | 10421 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 | 10422 | `			}` |
|        - | 10423 | `		}` |
|     1949 | 10424 | `	}` |
|   219245 | 10425 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   219245 | 10426 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10427 | `		return SXERR_ABORT;` |
|        - | 10428 | `	}` |
|        - | 10429 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   219245 | 10430 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   219245 | 10431 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - | 10432 | `	/* Assume a standalone class */` |
|   219245 | 10433 | `	pBase = 0;` |
|   219245 | 10434 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   171313 | 10435 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   171313 | 10436 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - | 10437 | `			SyBlob sResolved;` |
|        - | 10438 | `			SyString sBaseName;` |
|        - | 10439 | `			sxu32 nRefLine;` |
|   124529 | 10440 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - | 10441 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 | 10442 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10443 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|      ! 0 | 10444 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10445 | `					return SXERR_ABORT;` |
|        - | 10446 | `				}` |
|      ! 0 | 10447 | `			}` |
|   124529 | 10448 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   124529 | 10449 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   124529 | 10450 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   124529 | 10451 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 | 10452 | `				SyBlobRelease(&sResolved);` |
|        4 | 10453 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 10454 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 | 10455 | `					pName);` |
|        3 | 10456 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 | 10457 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10458 | `					return SXERR_ABORT;` |
|        - | 10459 | `				}` |
|        3 | 10460 | `				return SXRET_OK;` |
|        - | 10461 | `			}` |
|   186788 | 10462 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   124522 | 10463 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   124527 | 10464 | `			SyStringInitFromBuf(&sBaseName,` |
|        - | 10465 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 10466 | `			/* Interfaces are not allowed */` |
|   124527 | 10467 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 | 10468 | `				pBase = pBase->pNextName;` |
|      ! 0 | 10469 | `			}` |
|   124527 | 10470 | `			if( pBase == 0 ){` |
|      ! 0 | 10471 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 10472 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 | 10473 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10474 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 10475 | `					return SXERR_ABORT;` |
|        - | 10476 | `				}` |
|      ! 0 | 10477 | `			}else{` |
|   124527 | 10478 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 | 10479 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 10480 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|        3 | 10481 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10482 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 10483 | `						return SXERR_ABORT;` |
|        - | 10484 | `					}` |
|        3 | 10485 | `					pBase = 0; /* Never inherit from an enum */` |
|   124526 | 10486 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 | 10487 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 10488 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 | 10489 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10490 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 10491 | `						return SXERR_ABORT;` |
|        - | 10492 | `					}` |
|      ! 0 | 10493 | `				}` |
|        - | 10494 | `			}` |
|   124527 | 10495 | `			SyBlobRelease(&sResolved);` |
|   124527 | 10496 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 10497 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 | 10498 | `			}` |
|    62261 | 10499 | `		}` |
|   171311 | 10500 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - | 10501 | `			ph7_class *pInterface;` |
|        - | 10502 | `			/* Interface implementation */` |
|    46801 | 10503 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    23410 | 10504 | `			for(;;){` |
|        - | 10505 | `				SyBlob sResolved;` |
|        - | 10506 | `				SyString sIntName;` |
|        - | 10507 | `				sxu32 nRefLine;` |
|    46813 | 10508 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    46813 | 10509 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    46813 | 10510 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 10511 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 10512 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 10513 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 | 10514 | `						pName);` |
|      ! 0 | 10515 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10516 | `						return SXERR_ABORT;` |
|        - | 10517 | `					}` |
|      ! 0 | 10518 | `					break;` |
|        - | 10519 | `				}` |
|    93621 | 10520 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    46808 | 10521 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    46813 | 10522 | `				SyStringInitFromBuf(&sIntName,` |
|        - | 10523 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 10524 | `				/* Only interfaces are allowed */` |
|    46813 | 10525 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10526 | `					pInterface = pInterface->pNextName;` |
|      ! 0 | 10527 | `				}` |
|    46813 | 10528 | `				if( pInterface == 0 ){` |
|      ! 0 | 10529 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 10530 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 | 10531 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10532 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 10533 | `						return SXERR_ABORT;` |
|        - | 10534 | `					}` |
|      ! 0 | 10535 | `				}else{` |
|        - | 10536 | `					/* Reject user classes that try to implement Throwable` |
|        - | 10537 | `					 * directly (or via an interface that extends Throwable)` |
|        - | 10538 | `					 * unless they already extend Exception or Error.` |
|        - | 10539 | `					 * Exception and Error themselves are compiled from the` |
|        - | 10540 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - | 10541 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    46813 | 10542 | `					SyString *pFqn = &pClass->sName;` |
|    46813 | 10543 | `					int bIsExceptionOrError =` |
|    27292 | 10544 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    72158 | 10545 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    44873 | 10546 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3898 | 10547 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    50697 | 10548 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11670 | 10549 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3887 | 10550 | `						!bIsExceptionOrError ){` |
|       12 | 10551 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10552 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 | 10553 | `							&pClass->sName);` |
|        9 | 10554 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10555 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 10556 | `							return SXERR_ABORT;` |
|        - | 10557 | `						}` |
|        - | 10558 | `						/* Skip registration so the follow-up abstract-method` |
|        - | 10559 | `						 * check does not produce a duplicate fatal. */` |
|        6 | 10560 | `					}else{` |
|    46807 | 10561 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - | 10562 | `					}` |
|        - | 10563 | `				}` |
|    46813 | 10564 | `				SyBlobRelease(&sResolved);` |
|    46813 | 10565 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    23403 | 10566 | `					break;` |
|        - | 10567 | `				}` |
|       16 | 10568 | `				pGen->pIn++;/* Jump the comma */` |
|        4 | 10569 | `			}` |
|    23398 | 10570 | `		}` |
|    85653 | 10571 | `	}` |
|   219243 | 10572 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 10573 | `		/* Syntax error */` |
|      ! 0 | 10574 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 | 10575 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10576 | `		if( rc == SXERR_ABORT ){` |
|        - | 10577 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 10578 | `			return SXERR_ABORT;` |
|        - | 10579 | `		}` |
|      ! 0 | 10580 | `		return SXRET_OK;` |
|        - | 10581 | `	}` |
|   219243 | 10582 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   219243 | 10583 | `	pEnd = 0; /* cc warning */` |
|        - | 10584 | `	/* Delimit the class body */` |
|   219243 | 10585 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   219243 | 10586 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 10587 | `		/* Syntax error */` |
|      ! 0 | 10588 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 | 10589 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10590 | `		if( rc == SXERR_ABORT ){` |
|        - | 10591 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 10592 | `			return SXERR_ABORT;` |
|        - | 10593 | `		}` |
|      ! 0 | 10594 | `		return SXRET_OK;` |
|        - | 10595 | `	}` |
|        - | 10596 | `	/* The delimiter token is the class body's closing brace */` |
|   219243 | 10597 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 10598 | `	/* Swap token stream */` |
|   219243 | 10599 | `	pTmp = pGen->pEnd;` |
|   219243 | 10600 | `	pGen->pEnd = pEnd;` |
|        - | 10601 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   219243 | 10602 | `	pClass->iFlags \|= iFlags;` |
|        - | 10603 | `	/* Start the parse process */` |
|   825036 | 10604 | `	for(;;){` |
|        - | 10605 | `		/* Jump leading/trailing semi-colons */` |
|  2230909 | 10606 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   362305 | 10607 | `			pGen->pIn++;` |
|        5 | 10608 | `		}` |
|  1868609 | 10609 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 10610 | `			/* End of class body */` |
|   219201 | 10611 | `			break;` |
|        - | 10612 | `		}` |
|        - | 10613 | `		/* Bind a directly-preceding docblock to this member */` |
|  1649413 | 10614 | `		GenStateSetPendingDoc(&(*pGen));` |
|  1649408 | 10615 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   824709 | 10616 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 | 10617 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10618 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 10619 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 10620 | `			if( rc == SXERR_ABORT ){` |
|        - | 10621 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 10622 | `				return SXERR_ABORT;` |
|        - | 10623 | `			}` |
|      ! 0 | 10624 | `			goto done;` |
|        - | 10625 | `		}` |
|        - | 10626 | `		/* Assume public visibility */` |
|  1649413 | 10627 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  1649413 | 10628 | `		iAttrflags = 0;` |
|        - | 10629 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - | 10630 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - | 10631 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - | 10632 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  1649413 | 10633 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 10634 | `			int bMod = 0;` |
|      ! 0 | 10635 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 10636 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - | 10637 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - | 10638 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - | 10639 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - | 10640 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 | 10641 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 | 10642 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 | 10643 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 | 10644 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 | 10645 | `			}` |
|      ! 0 | 10646 | `			if( !bMod ){` |
|      ! 0 | 10647 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10648 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 10649 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10650 | `						return SXERR_ABORT;` |
|        - | 10651 | `					}` |
|      ! 0 | 10652 | `					goto done;` |
|        - | 10653 | `				}` |
|      ! 0 | 10654 | `				continue;` |
|        - | 10655 | `			}` |
|      ! 0 | 10656 | `		}` |
|  1649413 | 10657 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10658 | `			/* Extract the current keyword */` |
|  1649413 | 10659 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1649413 | 10660 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - | 10661 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|     7815 | 10662 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|     7815 | 10663 | `				if( rc != SXRET_OK ){` |
|        6 | 10664 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10665 | `						return SXERR_ABORT;` |
|        - | 10666 | `					}` |
|        6 | 10667 | `					goto done;` |
|        - | 10668 | `				}` |
|     7811 | 10669 | `				continue;` |
|        - | 10670 | `			}` |
|  1641603 | 10671 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 10672 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - | 10673 | `				TraitUseEntry sUse;` |
|       63 | 10674 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       63 | 10675 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|       63 | 10676 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|       37 | 10677 | `				for(;;){` |
|        - | 10678 | `					ph7_class *pTrait;` |
|        - | 10679 | `					SyString *pTraitName;` |
|       71 | 10680 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10681 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10682 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 | 10683 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10684 | `							return SXERR_ABORT;` |
|        - | 10685 | `						}` |
|      ! 0 | 10686 | `						break;` |
|        - | 10687 | `					}` |
|       71 | 10688 | `					pTraitName = &pGen->pIn->sData;` |
|        - | 10689 | `					/* Resolve trait name through namespace/imports */ {` |
|        - | 10690 | `						SyBlob sResolved;` |
|       71 | 10691 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       71 | 10692 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      137 | 10693 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|       66 | 10694 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       71 | 10695 | `						SyBlobRelease(&sResolved);` |
|        - | 10696 | `					}` |
|        - | 10697 | `					/* Only traits are allowed */` |
|       71 | 10698 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 10699 | `						pTrait = pTrait->pNextName;` |
|      ! 0 | 10700 | `					}` |
|       71 | 10701 | `					if( pTrait == 0 ){` |
|      ! 0 | 10702 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10703 | `							"'%z' is not a trait",pTraitName);` |
|      ! 0 | 10704 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10705 | `							return SXERR_ABORT;` |
|        - | 10706 | `						}` |
|      ! 0 | 10707 | `					}else{` |
|       71 | 10708 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - | 10709 | `					}` |
|       71 | 10710 | `					pGen->pIn++; /* Advance past trait name */` |
|       71 | 10711 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       34 | 10712 | `						break;` |
|        - | 10713 | `					}` |
|       10 | 10714 | `					pGen->pIn++; /* Jump the comma */` |
|        2 | 10715 | `				}` |
|        - | 10716 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|       63 | 10717 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 10718 | `					SyToken *pBlock;` |
|       13 | 10719 | `					pGen->pIn++; /* Jump '{' */` |
|       13 | 10720 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       13 | 10721 | `					sUse.pResolvStart = pGen->pIn;` |
|       13 | 10722 | `					sUse.pResolvEnd = pBlock;` |
|       13 | 10723 | `					if( pBlock < pGen->pEnd ){` |
|       13 | 10724 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        8 | 10725 | `					}else{` |
|      ! 0 | 10726 | `						pGen->pIn = pGen->pEnd;` |
|        - | 10727 | `					}` |
|        5 | 10728 | `				}` |
|       63 | 10729 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 10730 | `				/* The semicolon will be consumed by the outer loop */` |
|       63 | 10731 | `				continue;` |
|        - | 10732 | `			}` |
|  1641545 | 10733 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 10734 | `				int nSetTok;` |
|  1497355 | 10735 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  1497355 | 10736 | `				if( nSetVis ){` |
|        - | 10737 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|        - | 10738 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|        3 | 10739 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 10740 | `					pGen->pIn += nSetTok;` |
|        2 | 10741 | `				}else{` |
|  1497353 | 10742 | `					iProtection = nKwrd;` |
|  1497353 | 10743 | `					pGen->pIn++; /* Jump the visibility token */` |
|        - | 10744 | `					/* Optional asymmetric set-visibility after the read` |
|        - | 10745 | ``					 * visibility: `public private(set) int $x`. */`` |
|  1497353 | 10746 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  1497353 | 10747 | `					if( nSetVis ){` |
|        9 | 10748 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        9 | 10749 | `						pGen->pIn += nSetTok;` |
|        4 | 10750 | `					}` |
|        - | 10751 | `				}` |
|        - | 10752 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|        - | 10753 | ``				 * `public private(set) readonly int $x`. */`` |
|  1497355 | 10754 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       24 | 10755 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       24 | 10756 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       10 | 10757 | `				}` |
|  1497350 | 10758 | `				if( pGen->pIn >= pGen->pEnd` |
|  1497355 | 10759 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10760 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10761 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 10762 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10763 | `					if( rc == SXERR_ABORT ){` |
|        - | 10764 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 | 10765 | `						return SXERR_ABORT;` |
|        - | 10766 | `					}` |
|      ! 0 | 10767 | `					goto done;` |
|        - | 10768 | `				}` |
|  1497355 | 10769 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10770 | `					/* Attribute declaration (untyped) */` |
|   210339 | 10771 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   210339 | 10772 | `					if( rc != SXRET_OK ){` |
|       11 | 10773 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10774 | `							return SXERR_ABORT;` |
|        - | 10775 | `						}` |
|       11 | 10776 | `						goto done;` |
|        - | 10777 | `					}` |
|   210475 | 10778 | `					continue;` |
|        - | 10779 | `				}` |
|  1287021 | 10780 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10781 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|      299 | 10782 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      299 | 10783 | `					if( rc != SXRET_OK ){` |
|        8 | 10784 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10785 | `							return SXERR_ABORT;` |
|        - | 10786 | `						}` |
|        8 | 10787 | `						goto done;` |
|        - | 10788 | `					}` |
|      293 | 10789 | `					continue;` |
|        - | 10790 | `				}` |
|        - | 10791 | `				/* Extract the keyword */` |
|  1286727 | 10792 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   643361 | 10793 | `			}` |
|  1430917 | 10794 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 10795 | `				/* Process constant declaration */` |
|   143863 | 10796 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   143863 | 10797 | `				if( rc != SXRET_OK ){` |
|       11 | 10798 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10799 | `						return SXERR_ABORT;` |
|        - | 10800 | `					}` |
|       11 | 10801 | `					goto done;` |
|        - | 10802 | `				}` |
|    71930 | 10803 | `			}else{` |
|  1287059 | 10804 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 10805 | `					/* Static method or attribute,record that */` |
|    23445 | 10806 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    23445 | 10807 | `					pGen->pIn++; /* Jump the static keyword */` |
|    23445 | 10808 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10809 | `						int nSetTok;` |
|    23417 | 10810 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    23417 | 10811 | `						if( nSetVis ){` |
|        - | 10812 | ``							/* `static private(set) int $x` — read side stays public */`` |
|        3 | 10813 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 10814 | `							pGen->pIn += nSetTok;` |
|        2 | 10815 | `						}else{` |
|        - | 10816 | `							/* Extract the keyword */` |
|    23415 | 10817 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    23415 | 10818 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 10819 | `								iProtection = nKwrd;` |
|      ! 0 | 10820 | `								pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 | 10821 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|      ! 0 | 10822 | `								if( nSetVis ){` |
|      ! 0 | 10823 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|      ! 0 | 10824 | `									pGen->pIn += nSetTok;` |
|      ! 0 | 10825 | `								}` |
|      ! 0 | 10826 | `							}` |
|        - | 10827 | `						}` |
|    11706 | 10828 | `					}` |
|        - | 10829 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - | 10830 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - | 10831 | `					 * than a generic "expecting method" parse error. */` |
|    23445 | 10832 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 10833 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 10834 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 | 10835 | `					}` |
|    23440 | 10836 | `					if( pGen->pIn >= pGen->pEnd` |
|    23445 | 10837 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10838 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10839 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 | 10840 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10841 | `						if( rc == SXERR_ABORT ){` |
|        - | 10842 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10843 | `							return SXERR_ABORT;` |
|        - | 10844 | `						}` |
|      ! 0 | 10845 | `						goto done;` |
|        - | 10846 | `					}` |
|    23445 | 10847 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10848 | `						/* Attribute declaration */` |
|       29 | 10849 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       29 | 10850 | `						if( rc != SXRET_OK ){` |
|        3 | 10851 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10852 | `								return SXERR_ABORT;` |
|        - | 10853 | `							}` |
|        3 | 10854 | `							goto done;` |
|        - | 10855 | `						}` |
|       26 | 10856 | `						continue;` |
|        - | 10857 | `					}` |
|    23419 | 10858 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10859 | `						/* Typed static attribute declaration */` |
|       17 | 10860 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       17 | 10861 | `						if( rc != SXRET_OK ){` |
|        3 | 10862 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10863 | `								return SXERR_ABORT;` |
|        - | 10864 | `							}` |
|        3 | 10865 | `							goto done;` |
|        - | 10866 | `						}` |
|       15 | 10867 | `						continue;` |
|        - | 10868 | `					}` |
|        - | 10869 | `					/* Extract the keyword */` |
|    23405 | 10870 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1275319 | 10871 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - | 10872 | `					/* Abstract method,record that */` |
|       21 | 10873 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - | 10874 | `					/* Mark the whole class as abstract */` |
|       21 | 10875 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|        - | 10876 | `					/* Advance the stream cursor */` |
|       21 | 10877 | `					pGen->pIn++;` |
|       21 | 10878 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       21 | 10879 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 | 10880 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       19 | 10881 | `							iProtection = nKwrd;` |
|       19 | 10882 | `							pGen->pIn++; /* Jump the visibility token */` |
|        8 | 10883 | `						}` |
|        9 | 10884 | `					}` |
|       21 | 10885 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 10886 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10887 | `							/* Static method */` |
|      ! 0 | 10888 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10889 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10890 | `					}` |
|       21 | 10891 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       18 | 10892 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|        - | 10893 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|        - | 10894 | `							 * HOOKED property declaration. Route anything that is not a` |
|        - | 10895 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|        - | 10896 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|        - | 10897 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|        6 | 10898 | `							if( pGen->pIn < pGen->pEnd` |
|        7 | 10899 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|        3 | 10900 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        7 | 10901 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        7 | 10902 | `								if( rc != SXRET_OK ){` |
|      ! 0 | 10903 | `									if( rc == SXERR_ABORT ){` |
|      ! 0 | 10904 | `										return SXERR_ABORT;` |
|        - | 10905 | `									}` |
|      ! 0 | 10906 | `									goto done;` |
|        - | 10907 | `								}` |
|        7 | 10908 | `								continue;` |
|        - | 10909 | `							}` |
|      ! 0 | 10910 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10911 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 | 10912 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10913 | `							if( rc == SXERR_ABORT ){` |
|        - | 10914 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10915 | `								return SXERR_ABORT;` |
|        - | 10916 | `							}` |
|      ! 0 | 10917 | `							goto done;` |
|        - | 10918 | `					}` |
|       15 | 10919 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  1263607 | 10920 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - | 10921 | `					/* final method ,record that */` |
|       21 | 10922 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       21 | 10923 | `					pGen->pIn++; /* Jump the final keyword */` |
|       21 | 10924 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10925 | `						/* Extract the keyword */` |
|       21 | 10926 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 | 10927 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       11 | 10928 | `							iProtection = nKwrd;` |
|       11 | 10929 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 | 10930 | `						}` |
|        9 | 10931 | `					}` |
|       21 | 10932 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 10933 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - | 10934 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - | 10935 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - | 10936 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 | 10937 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 | 10938 | `							if( rc != SXRET_OK ){` |
|      ! 0 | 10939 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 | 10940 | `									return SXERR_ABORT;` |
|        - | 10941 | `								}` |
|      ! 0 | 10942 | `								goto done;` |
|        - | 10943 | `							}` |
|       14 | 10944 | `							continue;` |
|        - | 10945 | `					}` |
|        9 | 10946 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 | 10947 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10948 | `							/* Static method */` |
|      ! 0 | 10949 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10950 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10951 | `					}` |
|        9 | 10952 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 10953 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10954 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10955 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 | 10956 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10957 | `							if( rc == SXERR_ABORT ){` |
|        - | 10958 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10959 | `								return SXERR_ABORT;` |
|        - | 10960 | `							}` |
|      ! 0 | 10961 | `							goto done;` |
|        - | 10962 | `					}` |
|        9 | 10963 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 10964 | `				}` |
|  1287001 | 10965 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 10966 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10967 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 | 10968 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10969 | `						if( rc == SXERR_ABORT ){` |
|        - | 10970 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10971 | `							return SXERR_ABORT;` |
|        - | 10972 | `						}` |
|      ! 0 | 10973 | `						goto done;` |
|        - | 10974 | `				}` |
|  1287001 | 10975 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 | 10976 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 10977 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 | 10978 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10979 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 10980 | `						if( rc == SXERR_ABORT ){` |
|        - | 10981 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10982 | `							return SXERR_ABORT;` |
|        - | 10983 | `						}` |
|      ! 0 | 10984 | `						goto done;` |
|        - | 10985 | `					}` |
|        - | 10986 | `					/* Attribute declaration */` |
|        7 | 10987 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 | 10988 | `				}else{` |
|        - | 10989 | `					/* Process method declaration */` |
|  1286995 | 10990 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 10991 | `				}` |
|  1287001 | 10992 | `				if( rc != SXRET_OK ){` |
|       16 | 10993 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10994 | `						return SXERR_ABORT;` |
|        - | 10995 | `					}` |
|       16 | 10996 | `					goto done;` |
|        - | 10997 | `				}` |
|        - | 10998 | `			}` |
|   715422 | 10999 | `		}else{` |
|        - | 11000 | `			/* Attribute declaration */` |
|      ! 0 | 11001 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11002 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11003 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11004 | `					return SXERR_ABORT;` |
|        - | 11005 | `				}` |
|      ! 0 | 11006 | `				goto done;` |
|        - | 11007 | `			}` |
|        - | 11008 | `		}` |
|        5 | 11009 | `	}` |
|        - | 11010 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 11011 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 11012 | `	 */` |
|        - | 11013 | `	{` |
|        - | 11014 | `		TraitUseEntry *apUse;` |
|        - | 11015 | `		sxu32 nU;` |
|   219201 | 11016 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   219259 | 11017 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|       63 | 11018 | `			TraitUseEntry *pUse = &apUse[nU];` |
|       63 | 11019 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|       63 | 11020 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|       63 | 11021 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 11022 | `			sxu32 nT;` |
|       63 | 11023 | `			if( !hasResolution ){` |
|        - | 11024 | `				/* No conflict resolution block: use standard trait application */` |
|      107 | 11025 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       59 | 11026 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|       59 | 11027 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 11028 | `						break;` |
|        - | 11029 | `					}` |
|       32 | 11030 | `				}` |
|       29 | 11031 | `			}else{` |
|        - | 11032 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 11033 | `				 * then use the block to resolve method conflicts.` |
|        - | 11034 | `				 */` |
|        - | 11035 | `				SyToken *pR;` |
|       25 | 11036 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       15 | 11037 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 11038 | `					ph7_class_attr *pAR;` |
|        - | 11039 | `					SyHashEntry *pER;` |
|        - | 11040 | `					SyString *pNR;` |
|       15 | 11041 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       21 | 11042 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 | 11043 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 11044 | `						pNR = &pAR->sName;` |
|      ! 0 | 11045 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 11046 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 11047 | `						}` |
|      ! 0 | 11048 | `					}` |
|       15 | 11049 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        9 | 11050 | `				}` |
|        - | 11051 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       13 | 11052 | `				pR = pUse->pResolvStart;` |
|       27 | 11053 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 11054 | `					SyString sTrait,sMethod;` |
|        - | 11055 | `					ph7_class *pSrcTrait;` |
|        - | 11056 | `					ph7_class_method *pMeth;` |
|        - | 11057 | `					sxi32 nRKwrd;` |
|       41 | 11058 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 11059 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 11060 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 11061 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 11062 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 11063 | `					sMethod = pR->sData;` |
|       17 | 11064 | `					pR++;` |
|       17 | 11065 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 11066 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 11067 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 11068 | `							sTrait = sMethod;` |
|        7 | 11069 | `							pR++;` |
|        7 | 11070 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 11071 | `							sMethod = pR->sData;` |
|        7 | 11072 | `							pR++;` |
|        3 | 11073 | `						}` |
|        3 | 11074 | `					}` |
|       17 | 11075 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 11076 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 11077 | `						continue;` |
|        - | 11078 | `					}` |
|       17 | 11079 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 11080 | `					pR++;` |
|       17 | 11081 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 11082 | `						pSrcTrait = 0;` |
|        7 | 11083 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 11084 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 11085 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 11086 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 11087 | `								pSrcTrait = apTrait[nT];` |
|        5 | 11088 | `								break;` |
|        - | 11089 | `							}` |
|        2 | 11090 | `						}` |
|        5 | 11091 | `						if( pSrcTrait ){` |
|        5 | 11092 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 11093 | `							if( pMeth ){` |
|        5 | 11094 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 11095 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 11096 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 11097 | `								}` |
|        2 | 11098 | `							}` |
|        2 | 11099 | `						}` |
|        2 | 11100 | `					}` |
|       35 | 11101 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 11102 | `				}` |
|        - | 11103 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       25 | 11104 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 11105 | `					ph7_class_method *pMR;` |
|        - | 11106 | `					SyHashEntry *pER;` |
|        - | 11107 | `					SyString *pNR;` |
|       15 | 11108 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       41 | 11109 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       23 | 11110 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       23 | 11111 | `						pNR = &pMR->sFunc.sName;` |
|       23 | 11112 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       14 | 11113 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 11114 | `						}` |
|        3 | 11115 | `					}` |
|        9 | 11116 | `				}` |
|        - | 11117 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       13 | 11118 | `				pR = pUse->pResolvStart;` |
|       27 | 11119 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 11120 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 11121 | `					ph7_class *pSrcTrait;` |
|        - | 11122 | `					ph7_class_method *pMeth;` |
|       27 | 11123 | `					int hasQual = 0;` |
|        - | 11124 | `					sxi32 nRKwrd;` |
|       41 | 11125 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 11126 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 11127 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 11128 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 11129 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       17 | 11130 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 11131 | `					sMethod = pR->sData;` |
|       17 | 11132 | `					pR++;` |
|       17 | 11133 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 11134 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 11135 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 11136 | `							sTrait = sMethod;` |
|        7 | 11137 | `							hasQual = 1;` |
|        7 | 11138 | `							pR++;` |
|        7 | 11139 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 11140 | `							sMethod = pR->sData;` |
|        7 | 11141 | `							pR++;` |
|        3 | 11142 | `						}` |
|        3 | 11143 | `					}` |
|       17 | 11144 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 11145 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 11146 | `						continue;` |
|        - | 11147 | `					}` |
|       17 | 11148 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 11149 | `					pR++;` |
|       17 | 11150 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       13 | 11151 | `						sxi32 iNewVis = -1;` |
|       13 | 11152 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 11153 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 11154 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 11155 | `								iNewVis = nAK;` |
|        7 | 11156 | `								pR++;` |
|        3 | 11157 | `							}` |
|        3 | 11158 | `						}` |
|       13 | 11159 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       11 | 11160 | `							sAlias = pR->sData;` |
|       11 | 11161 | `							pR++;` |
|        4 | 11162 | `						}` |
|       13 | 11163 | `						pMeth = 0;` |
|       13 | 11164 | `						if( hasQual ){` |
|        3 | 11165 | `							pSrcTrait = 0;` |
|        5 | 11166 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 11167 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 11168 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 11169 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 11170 | `									pSrcTrait = apTrait[nT];` |
|        3 | 11171 | `									break;` |
|        - | 11172 | `								}` |
|        2 | 11173 | `							}` |
|        3 | 11174 | `							if( pSrcTrait ){` |
|        3 | 11175 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 11176 | `							}` |
|        2 | 11177 | `						}else{` |
|       10 | 11178 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 11179 | `						}` |
|       13 | 11180 | `						if( pMeth ){` |
|       13 | 11181 | `							if( sAlias.nByte > 0 ){` |
|        - | 11182 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 11183 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 11184 | `								 */` |
|        - | 11185 | `								ph7_class_method *pAlias;` |
|        - | 11186 | `								char *zAliasDup;` |
|       11 | 11187 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       11 | 11188 | `								if( pAlias ){` |
|       11 | 11189 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       11 | 11190 | `									if( iNewVis >= 0 ){` |
|        5 | 11191 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 11192 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 11193 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 11194 | `									}` |
|       11 | 11195 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       11 | 11196 | `									if( zAliasDup ){` |
|       11 | 11197 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 11198 | `									}` |
|        7 | 11199 | `								}` |
|        7 | 11200 | `							}else if( iNewVis >= 0 ){` |
|        - | 11201 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 11202 | `								ph7_class_method *pCopy;` |
|        3 | 11203 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 11204 | `								if( pCopy ){` |
|        3 | 11205 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 11206 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 11207 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 11208 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 11209 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 11210 | `									/* Replace the method in the class hash */` |
|        3 | 11211 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 11212 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 11213 | `								}` |
|        1 | 11214 | `							}` |
|        5 | 11215 | `						}` |
|        5 | 11216 | `						SXUNUSED(hasQual);` |
|        5 | 11217 | `					}` |
|       21 | 11218 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 11219 | `				}` |
|        - | 11220 | `			}` |
|       63 | 11221 | `			SySetRelease(&pUse->aTraits);` |
|       34 | 11222 | `		}` |
|        - | 11223 | `	}` |
|   219201 | 11224 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 11225 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 11226 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|     3913 | 11227 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|     3913 | 11228 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11229 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 11230 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 11231 | `			return SXERR_ABORT;` |
|        - | 11232 | `		}` |
|     1954 | 11233 | `	}` |
|        - | 11234 | `	/* Install the class */` |
|   219201 | 11235 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   219201 | 11236 | `	if( rc == SXRET_OK ){` |
|        - | 11237 | `		ph7_class **apInterface;` |
|        - | 11238 | `		sxu32 n;` |
|   219201 | 11239 | `		if( pBase ){` |
|        - | 11240 | `			/* Inherit from base class and mark as a subclass */` |
|   124525 | 11241 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    62260 | 11242 | `		}` |
|   219201 | 11243 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   266003 | 11244 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 11245 | `			/* Implements one or more interface */` |
|    46807 | 11246 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    46807 | 11247 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11248 | `				break;` |
|        - | 11249 | `			}` |
|    23406 | 11250 | `		}` |
|        - | 11251 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 11252 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|   219201 | 11253 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     3913 | 11254 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|     3913 | 11255 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 11256 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 11257 | `			}` |
|     3913 | 11258 | `			if( pIntf ){` |
|     3913 | 11259 | `				PH7_ClassImplement(pClass,pIntf);` |
|     1954 | 11260 | `			}` |
|     3913 | 11261 | `			if( pClass->nEnumBacking != 0 ){` |
|     3901 | 11262 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|     3901 | 11263 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 11264 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 11265 | `				}` |
|     3901 | 11266 | `				if( pIntf ){` |
|     3901 | 11267 | `					PH7_ClassImplement(pClass,pIntf);` |
|     1948 | 11268 | `				}` |
|     1948 | 11269 | `			}` |
|     1954 | 11270 | `		}` |
|        - | 11271 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 11272 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   219196 | 11273 | `		if( rc == SXRET_OK` |
|   219196 | 11274 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   219201 | 11275 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   171005 | 11276 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 11277 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   171005 | 11278 | `			if( pStringable ){` |
|   171005 | 11279 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   171005 | 11280 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 11281 | `				sxu32 i;` |
|   171005 | 11282 | `				int bAlready = 0;` |
|   209849 | 11283 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    42735 | 11284 | `					if( apImpl[i] == pStringable ){` |
|     3891 | 11285 | `						bAlready = 1;` |
|     3891 | 11286 | `						break;` |
|        - | 11287 | `					}` |
|    19427 | 11288 | `				}` |
|   171005 | 11289 | `				if( !bAlready ){` |
|   167119 | 11290 | `					PH7_ClassImplement(pClass,pStringable);` |
|    83557 | 11291 | `				}` |
|    85500 | 11292 | `			}` |
|    85500 | 11293 | `		}` |
|        - | 11294 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   219201 | 11295 | `		if( rc == SXRET_OK ){` |
|   219201 | 11296 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   219201 | 11297 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 11298 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 11299 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 11300 | `				return SXERR_ABORT;` |
|        - | 11301 | `			}` |
|   109598 | 11302 | `		}` |
|        - | 11303 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   219201 | 11304 | `		if( rc == SXRET_OK ){` |
|   219201 | 11305 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   219201 | 11306 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 11307 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 11308 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 11309 | `				return SXERR_ABORT;` |
|        - | 11310 | `			}` |
|   109598 | 11311 | `		}` |
|   109598 | 11312 | `	}` |
|   219201 | 11313 | `	SySetRelease(&aUseEntries);` |
|   219201 | 11314 | `	SySetRelease(&aInterfaces);` |
|   219201 | 11315 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11316 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11317 | `		return SXERR_ABORT;` |
|        - | 11318 | `	}` |
|   109598 | 11319 | `done:` |
|        - | 11320 | `	/* Point beyond the class body */` |
|   219243 | 11321 | `	pGen->pIn = &pEnd[1];` |
|   219243 | 11322 | `	pGen->pEnd = pTmp;` |
|   219243 | 11323 | `	return PH7_OK;` |
|   109625 | 11324 | `}` |
|        - | 11325 | `/* Compile a named class declaration (the common case). */` |
|   219212 | 11326 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 11327 | `{` |
|   219217 | 11328 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 11329 | `}` |
|        - | 11330 | `/*` |
|        - | 11331 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 11332 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 11333 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 11334 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 11335 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 11336 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 11337 | ` */` |
|       28 | 11338 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 11339 | `{` |
|        - | 11340 | `	char zName[128];         /* Synthesized class name */` |
|        - | 11341 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 11342 | `	SyString sName;` |
|        - | 11343 | `	SyToken *pArgStart,*pArgEnd;` |
|       32 | 11344 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|        - | 11345 | `	                              * is keyed to this 'class' token */` |
|        - | 11346 | `	ph7_value *pObj;` |
|       32 | 11347 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11348 | `	sxu32 nIdx,nLen;` |
|        - | 11349 | `	sxi32 nArg,rc;` |
|       14 | 11350 | `	SXUNUSED(iCompileFlag);` |
|        - | 11351 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       32 | 11352 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       32 | 11353 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 11354 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 11355 | `	}` |
|       32 | 11356 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 11357 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 11358 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 11359 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       32 | 11360 | `	pArgStart = pArgEnd = 0;` |
|       32 | 11361 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       32 | 11362 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11363 | `		return rc;` |
|        - | 11364 | `	}` |
|        - | 11365 | `	{` |
|        - | 11366 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|       32 | 11367 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|       28 | 11368 | `		if( pAnonClass` |
|       32 | 11369 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 11370 | `			return SXERR_ABORT;` |
|        - | 11371 | `		}` |
|        - | 11372 | `	}` |
|        - | 11373 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 11374 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       32 | 11375 | `	nArg = 0;` |
|       32 | 11376 | `	if( pArgStart < pArgEnd ){` |
|        7 | 11377 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 11378 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 11379 | `		SyToken *pArgNext;` |
|        7 | 11380 | `		pGen->pIn = pArgStart;` |
|        7 | 11381 | `		pGen->pEnd = pArgEnd;` |
|       13 | 11382 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 11383 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 11384 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 11385 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11386 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 11387 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 11388 | `					return SXERR_ABORT;` |
|        - | 11389 | `				}` |
|        7 | 11390 | `				nArg++;` |
|        3 | 11391 | `			}` |
|        7 | 11392 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 11393 | `		}` |
|        7 | 11394 | `		pGen->pIn = pSavedIn;` |
|        7 | 11395 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 11396 | `	}` |
|        - | 11397 | `	/* Load the synthesized class name */` |
|       32 | 11398 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       32 | 11399 | `	if( pObj == 0 ){` |
|      ! 0 | 11400 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11401 | `		return SXERR_ABORT;` |
|        - | 11402 | `	}` |
|       32 | 11403 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       32 | 11404 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 11405 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       32 | 11406 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       32 | 11407 | `	return SXRET_OK;` |
|       18 | 11408 | `}` |
|        - | 11409 | `/*` |
|        - | 11410 | ` * Compile a user-defined abstract class.` |
|        - | 11411 | ` *  According to the PHP language reference manual` |
|        - | 11412 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 11413 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 11414 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 11415 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 11416 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 11417 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 11418 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 11419 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 11420 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 11421 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 11422 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 11423 | ` *   could differ.` |
|        - | 11424 | ` */` |
|        - | 11425 | `/*` |
|        - | 11426 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 11427 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 11428 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 11429 | ` */` |
|  6403290 | 11430 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 11431 | `{` |
|  6403295 | 11432 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  3967005 | 11433 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  3967005 | 11434 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  3920371 | 11435 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  1952372 | 11436 | `	}` |
|  6341039 | 11437 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  6340979 | 11438 | `	return FALSE;` |
|  3201650 | 11439 | `}` |
|        - | 11440 | `/*` |
|        - | 11441 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 11442 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 11443 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 11444 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 11445 | ` */` |
|  6340974 | 11446 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 11447 | `{` |
|  6340979 | 11448 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  6340979 | 11449 | `	sxi32 iFlags = 0,iFlag;` |
|  6403295 | 11450 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    62321 | 11451 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 11452 | `			pDup = pIn;` |
|        2 | 11453 | `		}` |
|    62321 | 11454 | `		iFlags \|= iFlag;` |
|    62321 | 11455 | `		pIn++;` |
|        5 | 11456 | `	}` |
|  6340979 | 11457 | `	*ppIn = pIn;` |
|  6340979 | 11458 | `	if( ppDup ){ *ppDup = pDup; }` |
|  6340979 | 11459 | `	return iFlags;` |
|        5 | 11460 | `}` |
|        - | 11461 | `/*` |
|        - | 11462 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 11463 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 11464 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 11465 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 11466 | `` * `readonly`) to their existing handlers.`` |
|        - | 11467 | ` */` |
|  6313710 | 11468 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 11469 | `{` |
|  6313715 | 11470 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  3191894 | 11471 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  6331228 | 11472 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 11473 | `}` |
|        - | 11474 | `/*` |
|        - | 11475 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 11476 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 11477 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 11478 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 11479 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 11480 | ` */` |
|    27264 | 11481 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 11482 | `{` |
|        - | 11483 | `	SyToken *pDup;` |
|    27269 | 11484 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 11485 | `	sxi32 rc;` |
|    27269 | 11486 | `	if( pDup ){` |
|        4 | 11487 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 11488 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 11489 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11490 | `			return SXERR_ABORT;` |
|        - | 11491 | `		}` |
|        1 | 11492 | `	}` |
|    27264 | 11493 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    13637 | 11494 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 11495 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11496 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 11497 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11498 | `			return SXERR_ABORT;` |
|        - | 11499 | `		}` |
|        1 | 11500 | `	}` |
|    27269 | 11501 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    13637 | 11502 | `}` |
|        - | 11503 | `/*` |
|        - | 11504 | ` * Compile a user-defined trait.` |
|        - | 11505 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 11506 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 11507 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 11508 | ` */` |
|       72 | 11509 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 11510 | `{` |
|       77 | 11511 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11512 | `	ph7_class *pClass;` |
|        - | 11513 | `	SyToken *pEnd,*pTmp;` |
|        - | 11514 | `	sxi32 iProtection;` |
|        - | 11515 | `	sxi32 iAttrflags;` |
|        - | 11516 | `	SyString *pName;` |
|        - | 11517 | `	sxi32 nKwrd;` |
|        - | 11518 | `	sxi32 rc;` |
|        - | 11519 | `	/* Jump the 'trait' keyword */` |
|       77 | 11520 | `	pGen->pIn++;` |
|       77 | 11521 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 11522 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 11523 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11524 | `			return SXERR_ABORT;` |
|        - | 11525 | `		}` |
|      ! 0 | 11526 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 11527 | `			pGen->pIn++;` |
|      ! 0 | 11528 | `		}` |
|      ! 0 | 11529 | `		return SXRET_OK;` |
|        - | 11530 | `	}` |
|        - | 11531 | `	/* Extract trait name */` |
|       77 | 11532 | `	pName = &pGen->pIn->sData;` |
|       77 | 11533 | `	pGen->pIn++;` |
|        - | 11534 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 11535 | `		SyBlob sFQN;` |
|        - | 11536 | `		SyString sFQNStr;` |
|       77 | 11537 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       77 | 11538 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       77 | 11539 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       77 | 11540 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|       77 | 11541 | `		SyBlobRelease(&sFQN);` |
|        - | 11542 | `	}` |
|       77 | 11543 | `	if( pClass == 0 ){` |
|      ! 0 | 11544 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11545 | `		return SXERR_ABORT;` |
|        - | 11546 | `	}` |
|       77 | 11547 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|       77 | 11548 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 11549 | `		return SXERR_ABORT;` |
|        - | 11550 | `	}` |
|        - | 11551 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|       77 | 11552 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 11553 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 11554 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 11555 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11556 | `			return SXERR_ABORT;` |
|        - | 11557 | `		}` |
|      ! 0 | 11558 | `		return SXRET_OK;` |
|        - | 11559 | `	}` |
|       77 | 11560 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|       77 | 11561 | `	pEnd = 0;` |
|       77 | 11562 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|       77 | 11563 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 11564 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 11565 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 11566 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11567 | `			return SXERR_ABORT;` |
|        - | 11568 | `		}` |
|      ! 0 | 11569 | `		return SXRET_OK;` |
|        - | 11570 | `	}` |
|        - | 11571 | `	/* The delimiter token is the trait body's closing brace */` |
|       77 | 11572 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 11573 | `	/* Swap token stream */` |
|       77 | 11574 | `	pTmp = pGen->pEnd;` |
|       77 | 11575 | `	pGen->pEnd = pEnd;` |
|        - | 11576 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|       77 | 11577 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 11578 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|       71 | 11579 | `	for(;;){` |
|      191 | 11580 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       28 | 11581 | `			pGen->pIn++;` |
|        4 | 11582 | `		}` |
|      167 | 11583 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       77 | 11584 | `			break;` |
|        - | 11585 | `		}` |
|        - | 11586 | `		/* Bind a directly-preceding docblock to this member */` |
|       95 | 11587 | `		GenStateSetPendingDoc(&(*pGen));` |
|       95 | 11588 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 11589 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11590 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 11591 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 11592 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11593 | `				return SXERR_ABORT;` |
|        - | 11594 | `			}` |
|      ! 0 | 11595 | `			goto done;` |
|        - | 11596 | `		}` |
|       95 | 11597 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|       95 | 11598 | `		iAttrflags = 0;` |
|       95 | 11599 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       95 | 11600 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       95 | 11601 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 11602 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 11603 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 11604 | `				for(;;){` |
|        - | 11605 | `					ph7_class *pUsedTrait;` |
|        - | 11606 | `					SyString *pUsedName;` |
|        5 | 11607 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 11608 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 11609 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 11610 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11611 | `							return SXERR_ABORT;` |
|        - | 11612 | `						}` |
|      ! 0 | 11613 | `						break;` |
|        - | 11614 | `					}` |
|        5 | 11615 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 11616 | `					{` |
|        - | 11617 | `						SyBlob sResolved;` |
|        5 | 11618 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 11619 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 11620 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 11621 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 11622 | `						SyBlobRelease(&sResolved);` |
|        - | 11623 | `					}` |
|        5 | 11624 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 11625 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 11626 | `					}` |
|        5 | 11627 | `					if( pUsedTrait == 0 ){` |
|        4 | 11628 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 11629 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 11630 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11631 | `							return SXERR_ABORT;` |
|        - | 11632 | `						}` |
|        2 | 11633 | `					}else{` |
|        3 | 11634 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 11635 | `					}` |
|        5 | 11636 | `					pGen->pIn++;` |
|        5 | 11637 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 11638 | `						break;` |
|        - | 11639 | `					}` |
|      ! 0 | 11640 | `					pGen->pIn++;` |
|      ! 0 | 11641 | `				}` |
|        5 | 11642 | `				continue;` |
|        - | 11643 | `			}` |
|       91 | 11644 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       77 | 11645 | `				iProtection = nKwrd;` |
|       77 | 11646 | `				pGen->pIn++;` |
|       72 | 11647 | `				if( pGen->pIn >= pGen->pEnd` |
|       77 | 11648 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 11649 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11650 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 11651 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 11652 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11653 | `						return SXERR_ABORT;` |
|        - | 11654 | `					}` |
|      ! 0 | 11655 | `					goto done;` |
|        - | 11656 | `				}` |
|       77 | 11657 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       12 | 11658 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       12 | 11659 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 11660 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11661 | `							return SXERR_ABORT;` |
|        - | 11662 | `						}` |
|      ! 0 | 11663 | `						goto done;` |
|        - | 11664 | `					}` |
|       12 | 11665 | `					continue;` |
|        - | 11666 | `				}` |
|       67 | 11667 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        5 | 11668 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        5 | 11669 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 11670 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11671 | `							return SXERR_ABORT;` |
|        - | 11672 | `						}` |
|      ! 0 | 11673 | `						goto done;` |
|        - | 11674 | `					}` |
|        5 | 11675 | `					continue;` |
|        - | 11676 | `				}` |
|       63 | 11677 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       29 | 11678 | `			}` |
|       77 | 11679 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 11680 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11681 | `					"Traits cannot have constants");` |
|      ! 0 | 11682 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11683 | `					return SXERR_ABORT;` |
|        - | 11684 | `				}` |
|      ! 0 | 11685 | `				goto done;` |
|      ! 0 | 11686 | `			}else{` |
|       77 | 11687 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        8 | 11688 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|        8 | 11689 | `					pGen->pIn++;` |
|        8 | 11690 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 11691 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 11692 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 11693 | `							iProtection = nKwrd;` |
|      ! 0 | 11694 | `							pGen->pIn++;` |
|      ! 0 | 11695 | `						}` |
|        2 | 11696 | `					}` |
|        6 | 11697 | `					if( pGen->pIn >= pGen->pEnd` |
|        8 | 11698 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 11699 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11700 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 11701 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 11702 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11703 | `							return SXERR_ABORT;` |
|        - | 11704 | `						}` |
|      ! 0 | 11705 | `						goto done;` |
|        - | 11706 | `					}` |
|        8 | 11707 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 11708 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 11709 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 11710 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 11711 | `								return SXERR_ABORT;` |
|        - | 11712 | `							}` |
|      ! 0 | 11713 | `							goto done;` |
|        - | 11714 | `						}` |
|        3 | 11715 | `						continue;` |
|        - | 11716 | `					}` |
|        6 | 11717 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 11718 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11719 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 11720 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 11721 | `								return SXERR_ABORT;` |
|        - | 11722 | `							}` |
|      ! 0 | 11723 | `							goto done;` |
|        - | 11724 | `						}` |
|      ! 0 | 11725 | `						continue;` |
|        - | 11726 | `					}` |
|        6 | 11727 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       73 | 11728 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        6 | 11729 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        6 | 11730 | `					pGen->pIn++;` |
|        6 | 11731 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 11732 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 11733 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        6 | 11734 | `							iProtection = nKwrd;` |
|        6 | 11735 | `							pGen->pIn++;` |
|        2 | 11736 | `						}` |
|        2 | 11737 | `					}` |
|        6 | 11738 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        4 | 11739 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 11740 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11741 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 11742 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 11743 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11744 | `							return SXERR_ABORT;` |
|        - | 11745 | `						}` |
|      ! 0 | 11746 | `						goto done;` |
|        - | 11747 | `					}` |
|        6 | 11748 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        2 | 11749 | `				}` |
|       75 | 11750 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 11751 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11752 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 11753 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 11754 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11755 | `						return SXERR_ABORT;` |
|        - | 11756 | `					}` |
|      ! 0 | 11757 | `					goto done;` |
|        - | 11758 | `				}` |
|       75 | 11759 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 11760 | `					pGen->pIn++;` |
|      ! 0 | 11761 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 11762 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11763 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 11764 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11765 | `							return SXERR_ABORT;` |
|        - | 11766 | `						}` |
|      ! 0 | 11767 | `						goto done;` |
|        - | 11768 | `					}` |
|      ! 0 | 11769 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11770 | `				}else{` |
|       75 | 11771 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 11772 | `				}` |
|       75 | 11773 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 11774 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11775 | `						return SXERR_ABORT;` |
|        - | 11776 | `					}` |
|      ! 0 | 11777 | `					goto done;` |
|        - | 11778 | `				}` |
|        - | 11779 | `			}` |
|       40 | 11780 | `		}else{` |
|      ! 0 | 11781 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11782 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11783 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11784 | `					return SXERR_ABORT;` |
|        - | 11785 | `				}` |
|      ! 0 | 11786 | `				goto done;` |
|        - | 11787 | `			}` |
|        - | 11788 | `		}` |
|        5 | 11789 | `	}` |
|        - | 11790 | `	/* Install the trait */` |
|       77 | 11791 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|       77 | 11792 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11793 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11794 | `		return SXERR_ABORT;` |
|        - | 11795 | `	}` |
|       36 | 11796 | `done:` |
|        - | 11797 | `	/* Point beyond the trait body */` |
|       77 | 11798 | `	pGen->pIn = &pEnd[1];` |
|       77 | 11799 | `	pGen->pEnd = pTmp;` |
|       77 | 11800 | `	return PH7_OK;` |
|       41 | 11801 | `}` |
|        - | 11802 | `/*` |
|        - | 11803 | ` * Compile a user-defined class.` |
|        - | 11804 | ` *  According to the PHP language reference manual` |
|        - | 11805 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 11806 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 11807 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 11808 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 11809 | ` *   and functions (called "methods").` |
|        - | 11810 | ` */` |
|   188036 | 11811 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 11812 | `{` |
|        - | 11813 | `	sxi32 rc;` |
|   188041 | 11814 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   188041 | 11815 | `	return rc;` |
|        5 | 11816 | `}` |
|        - | 11817 | `/*` |
|        - | 11818 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 11819 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 11820 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 11821 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 11822 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 11823 | ` */` |
|  6278678 | 11824 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 11825 | `{` |
|  6313900 | 11826 | `	return (pIn->nType & PH7_TK_ID)` |
|  3174556 | 11827 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    41166 | 11828 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  6313895 | 11829 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 11830 | `}` |
|        - | 11831 | `/*` |
|        - | 11832 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 11833 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 11834 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 11835 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 11836 | ` */` |
|     3912 | 11837 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 11838 | `{` |
|     3917 | 11839 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 11840 | `}` |
|        - | 11841 | `/*` |
|        - | 11842 | ` * Exception handling.` |
|        - | 11843 | ` *  According to the PHP language reference manual` |
|        - | 11844 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 11845 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 11846 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 11847 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 11848 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 11849 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 11850 | ` *    (or re-thrown) within a catch block.` |
|        - | 11851 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 11852 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 11853 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 11854 | ` *    been defined with set_exception_handler().` |
|        - | 11855 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 11856 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 11857 | ` */` |
|        - | 11858 | `/*` |
|        - | 11859 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 11860 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 11861 | ` * indicates failure.` |
|        - | 11862 | ` */` |
|   315016 | 11863 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 11864 | `{` |
|   315021 | 11865 | `	sxi32 rc = SXRET_OK;` |
|   315021 | 11866 | `	if( pRoot->pOp ){` |
|   315009 | 11867 | `		switch( pRoot->pOp->iOp ){` |
|   157502 | 11868 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|        - | 11869 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|        - | 11870 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|        - | 11871 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|        - | 11872 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|        - | 11873 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   315009 | 11874 | `			break;` |
|      ! 0 | 11875 | `		default:` |
|        - | 11876 | `			/* Runtime will still reject non-Throwable values; the set above` |
|        - | 11877 | `			 * covers the common shapes and gives a friendlier compile error` |
|        - | 11878 | ``			 * for obvious mistakes like `throw 5`. */`` |
|      ! 0 | 11879 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11880 | `				"throw: Expecting an exception class instance");` |
|      ! 0 | 11881 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 | 11882 | `				rc = SXERR_INVALID;` |
|      ! 0 | 11883 | `			}` |
|      ! 0 | 11884 | `			break;` |
|        - | 11885 | `		}` |
|   157519 | 11886 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 11887 | `		/* Unexpected expression */` |
|      ! 0 | 11888 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11889 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11890 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 11891 | `			rc = SXERR_INVALID;` |
|      ! 0 | 11892 | `		}` |
|      ! 0 | 11893 | `	}` |
|   315021 | 11894 | `	return rc;` |
|        5 | 11895 | `}` |
|        - | 11896 | `/*` |
|        - | 11897 | ` * Compile a 'throw' statement.` |
|        - | 11898 | ` * throw: This is how you trigger an exception.` |
|        - | 11899 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 11900 | ` */` |
|   314980 | 11901 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 11902 | `{` |
|   314985 | 11903 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11904 | `	GenBlock *pBlock;` |
|        - | 11905 | `	sxu32 nIdx;` |
|        - | 11906 | `	sxi32 rc;` |
|   314985 | 11907 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 11908 | `	/* Compile the expression */` |
|   314985 | 11909 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   314985 | 11910 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11911 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 11912 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11913 | `			return SXERR_ABORT;` |
|        - | 11914 | `		}` |
|      ! 0 | 11915 | `		return SXRET_OK;` |
|        - | 11916 | `	}` |
|   314985 | 11917 | `	pBlock = pGen->pCurrent;` |
|        - | 11918 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  1228121 | 11919 | `	while(pBlock->pParent){` |
|  1228117 | 11920 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   314981 | 11921 | `			break;` |
|        - | 11922 | `		}` |
|        - | 11923 | `		/* Point to the parent block */` |
|   913141 | 11924 | `		pBlock = pBlock->pParent;` |
|        5 | 11925 | `	}` |
|        - | 11926 | `	/* Emit the throw instruction */` |
|   314985 | 11927 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 11928 | `	/* Emit the jump */` |
|   314985 | 11929 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   314985 | 11930 | `	return SXRET_OK;` |
|   157495 | 11931 | `}` |
|        - | 11932 | `/*` |
|        - | 11933 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 11934 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 11935 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 11936 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 11937 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 11938 | ` */` |
|       36 | 11939 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 11940 | `{` |
|       38 | 11941 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11942 | `	GenBlock *pBlock;` |
|        - | 11943 | `	sxu32 nIdx;` |
|        - | 11944 | `	sxi32 rc;` |
|       18 | 11945 | `	(void)iCompileFlag;` |
|       38 | 11946 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 11947 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 11948 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11949 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11950 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11951 | `			return SXERR_ABORT;` |
|        - | 11952 | `		}` |
|      ! 0 | 11953 | `		return SXRET_OK;` |
|        - | 11954 | `	}` |
|       38 | 11955 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 11956 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11957 | `		return SXERR_ABORT;` |
|        - | 11958 | `	}` |
|       38 | 11959 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11960 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11961 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11962 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11963 | `			return SXERR_ABORT;` |
|        - | 11964 | `		}` |
|      ! 0 | 11965 | `		return SXRET_OK;` |
|        - | 11966 | `	}` |
|        - | 11967 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 11968 | `	pBlock = pGen->pCurrent;` |
|       60 | 11969 | `	while( pBlock->pParent ){` |
|       49 | 11970 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 11971 | `			break;` |
|        - | 11972 | `		}` |
|       23 | 11973 | `		pBlock = pBlock->pParent;` |
|        1 | 11974 | `	}` |
|       38 | 11975 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 11976 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 11977 | `	return SXRET_OK;` |
|       20 | 11978 | `}` |
|        - | 11979 | `/*` |
|        - | 11980 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 11981 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 11982 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 11983 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 11984 | ` * compile error propagated from the parser.` |
|        - | 11985 | ` */` |
|       54 | 11986 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 11987 | `{` |
|        - | 11988 | `	SyString sClassName;` |
|        - | 11989 | `	SyToken *pToken;` |
|        - | 11990 | `	SyString *pName;` |
|        - | 11991 | `	char *zDup;` |
|        - | 11992 | `	sxi32 rc;` |
|       59 | 11993 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       59 | 11994 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       59 | 11995 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       59 | 11996 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       59 | 11997 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 11998 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11999 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12000 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12001 | `		return SXERR_INVALID;` |
|        - | 12002 | `	}` |
|       59 | 12003 | `	pGen->pIn++; /* '(' */` |
|       27 | 12004 | `	for(;;){` |
|        - | 12005 | `		SyBlob sResolved;` |
|       59 | 12006 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       59 | 12007 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 12008 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 12009 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 12010 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12011 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12012 | `			return SXERR_INVALID;` |
|        - | 12013 | `		}` |
|       86 | 12014 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       54 | 12015 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       59 | 12016 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       59 | 12017 | `		SyBlobRelease(&sResolved);` |
|       59 | 12018 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 12019 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       59 | 12020 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       54 | 12021 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 12022 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 12023 | `			pGen->pIn++; continue;` |
|        - | 12024 | `		}` |
|       59 | 12025 | `		break;` |
|      ! 0 | 12026 | `	}` |
|       54 | 12027 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 12028 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 12029 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 12030 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12031 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12032 | `		return SXERR_INVALID;` |
|        - | 12033 | `	}` |
|       59 | 12034 | `	pGen->pIn++; /* '$' */` |
|       59 | 12035 | `	pName = &pGen->pIn->sData;` |
|       59 | 12036 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 12037 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 12038 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 12039 | `	pGen->pIn++;` |
|       59 | 12040 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 12041 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 12042 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12043 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12044 | `		return SXERR_INVALID;` |
|        - | 12045 | `	}` |
|       59 | 12046 | `	pGen->pIn++; /* ')' */` |
|       59 | 12047 | `	return SXRET_OK;` |
|       32 | 12048 | `}` |
|        - | 12049 | `/*` |
|        - | 12050 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 12051 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 12052 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 12053 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 12054 | ` * VmThrowException):` |
|        - | 12055 | ` *` |
|        - | 12056 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 12057 | ` *    <try body>` |
|        - | 12058 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 12059 | ` *    JMP  -> finally\|end` |
|        - | 12060 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 12061 | ` *    <catch body>` |
|        - | 12062 | ` *    JMP  -> finally\|end` |
|        - | 12063 | ` *    ... more catches ...` |
|        - | 12064 | ` *  Lfin: <finally body>` |
|        - | 12065 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 12066 | ` *  Lend:` |
|        - | 12067 | ` */` |
|       98 | 12068 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 12069 | `{` |
|      103 | 12070 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 12071 | `	GenBlock *pTry;` |
|        - | 12072 | `	VmInstr *pInstr;` |
|      103 | 12073 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 12074 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 12075 | `	sxi32 rc;` |
|      103 | 12076 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 12077 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      103 | 12078 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      103 | 12079 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      103 | 12080 | `	pTry->pUserData = pException;` |
|      103 | 12081 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      103 | 12082 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      103 | 12083 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      103 | 12084 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      103 | 12085 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      103 | 12086 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 12087 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      103 | 12088 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      103 | 12089 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      103 | 12090 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      103 | 12091 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 12092 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      103 | 12093 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 12094 | `	/* Catch clauses (inline) */` |
|      103 | 12095 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       98 | 12096 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       59 | 12097 | `		sxu32 k = 0;` |
|       81 | 12098 | `		for(;;){` |
|        - | 12099 | `			ph7_exception_block sCatch;` |
|        - | 12100 | `			GenBlock *pCatchBlk;` |
|      113 | 12101 | `			sxu32 idxJmp = 0;` |
|      108 | 12102 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      104 | 12103 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       32 | 12104 | `				break;` |
|        - | 12105 | `			}` |
|       59 | 12106 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       59 | 12107 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 12108 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       59 | 12109 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       59 | 12110 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       59 | 12111 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       59 | 12112 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 12113 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 12114 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 12115 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       59 | 12116 | `			pCatchBlk->pUserData = pException;` |
|       59 | 12117 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       59 | 12118 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 12119 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       59 | 12120 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 12121 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 12122 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       59 | 12123 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       59 | 12124 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       59 | 12125 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       59 | 12126 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       59 | 12127 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       59 | 12128 | `			k++;` |
|        5 | 12129 | `		}` |
|       27 | 12130 | `	}` |
|        - | 12131 | `	/* Finally (inline) */` |
|      103 | 12132 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 12133 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 12134 | `		GenBlock *pFinBlk;` |
|       52 | 12135 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 12136 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 12137 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 12138 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 12139 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 12140 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 12141 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 12142 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 12143 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 12144 | `		pException->iHasFinally = 1;` |
|       24 | 12145 | `	}` |
|      103 | 12146 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      103 | 12147 | `	pException->iInlined = 1;` |
|        - | 12148 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 12149 | `	{` |
|      103 | 12150 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 12151 | `		sxu32 *aJ; sxu32 n;` |
|      103 | 12152 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      103 | 12153 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      103 | 12154 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      157 | 12155 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       59 | 12156 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       59 | 12157 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       32 | 12158 | `		}` |
|        - | 12159 | `	}` |
|      103 | 12160 | `	SySetRelease(&aCatchJmp);` |
|      103 | 12161 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 12162 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 12163 | `	}` |
|      103 | 12164 | `	return SXRET_OK;` |
|       54 | 12165 | `}` |
|        - | 12166 | `/*` |
|        - | 12167 | ` * Compile a 'catch' block.` |
|        - | 12168 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 12169 | ` * an object containing the exception information.` |
|        - | 12170 | ` */` |
|     5258 | 12171 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 12172 | `{` |
|     5263 | 12173 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 12174 | `	ph7_exception_block sCatch;` |
|        - | 12175 | `	SySet *pInstrContainer;` |
|        - | 12176 | `	SyString sClassName;` |
|        - | 12177 | `	GenBlock *pCatch;` |
|        - | 12178 | `	SyToken *pToken;` |
|        - | 12179 | `	SyString *pName;` |
|        - | 12180 | `	char *zDup;` |
|        - | 12181 | `	sxi32 rc;` |
|     5263 | 12182 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 12183 | `	/* Zero the structure */` |
|     5263 | 12184 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 12185 | `	/* Initialize fields */` |
|     5263 | 12186 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     5263 | 12187 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     5263 | 12188 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 12189 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 12190 | `			pToken = pGen->pIn;` |
|      ! 0 | 12191 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 12192 | `				pToken--;` |
|      ! 0 | 12193 | `			}` |
|      ! 0 | 12194 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 12195 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12196 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12197 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12198 | `				return SXERR_ABORT;` |
|        - | 12199 | `			}` |
|      ! 0 | 12200 | `			return SXERR_INVALID;` |
|        - | 12201 | `	}` |
|        - | 12202 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     5263 | 12203 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     2643 | 12204 | `	for(;;){` |
|        - | 12205 | `		SyBlob sResolved;` |
|     5291 | 12206 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     5291 | 12207 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 12208 | `			SyBlobRelease(&sResolved);` |
|        6 | 12209 | `			pToken = pGen->pIn;` |
|        6 | 12210 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 12211 | `				pToken--;` |
|      ! 0 | 12212 | `			}` |
|        8 | 12213 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 12214 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 12215 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 12216 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12217 | `				return SXERR_ABORT;` |
|        - | 12218 | `			}` |
|        6 | 12219 | `			return SXERR_INVALID;` |
|        - | 12220 | `		}` |
|        - | 12221 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 12222 | `		 * transient SyBlob allocation. */` |
|     7928 | 12223 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     5282 | 12224 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     5287 | 12225 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     5287 | 12226 | `		SyBlobRelease(&sResolved);` |
|     5287 | 12227 | `		if( zDup == 0 ){` |
|      ! 0 | 12228 | `			goto Mem;` |
|        - | 12229 | `		}` |
|     5287 | 12230 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     5287 | 12231 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12232 | `			goto Mem;` |
|        - | 12233 | `		}` |
|        - | 12234 | `		/* Check for '\|' (multi-catch separator) */` |
|     5282 | 12235 | `		if( pGen->pIn < pGen->pEnd &&` |
|     5282 | 12236 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       33 | 12237 | `			pGen->pIn->sData.nByte == 1 &&` |
|       28 | 12238 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       30 | 12239 | `			pGen->pIn++; /* Consume the '\|' */` |
|       30 | 12240 | `			continue;` |
|        - | 12241 | `		}` |
|     5259 | 12242 | `		break;` |
|      ! 0 | 12243 | `	}` |
|     5254 | 12244 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     5259 | 12245 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 12246 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 12247 | `			pToken = pGen->pIn;` |
|      ! 0 | 12248 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 12249 | `				pToken--;` |
|      ! 0 | 12250 | `			}` |
|      ! 0 | 12251 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 12252 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12253 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12254 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12255 | `				return SXERR_ABORT;` |
|        - | 12256 | `			}` |
|      ! 0 | 12257 | `			return SXERR_INVALID;` |
|        - | 12258 | `	}` |
|     5259 | 12259 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 12260 | `	/* Duplicate instance name */` |
|     5259 | 12261 | `	pName = &pGen->pIn->sData;` |
|     5259 | 12262 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     5259 | 12263 | `	if( zDup == 0 ){` |
|      ! 0 | 12264 | `		goto Mem;` |
|        - | 12265 | `	}` |
|     5259 | 12266 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     5259 | 12267 | `	pGen->pIn++;` |
|     5259 | 12268 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 12269 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 12270 | `		pToken = pGen->pIn;` |
|      ! 0 | 12271 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 12272 | `			pToken--;` |
|      ! 0 | 12273 | `		}` |
|      ! 0 | 12274 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 12275 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12276 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12277 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12278 | `			return SXERR_ABORT;` |
|        - | 12279 | `		}` |
|      ! 0 | 12280 | `		return SXERR_INVALID;` |
|        - | 12281 | `	}` |
|        - | 12282 | `	/* Compile the block */` |
|     5259 | 12283 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 12284 | `	/* Create the catch block */` |
|     5259 | 12285 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     5259 | 12286 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12287 | `		return SXERR_ABORT;` |
|        - | 12288 | `	}` |
|        - | 12289 | `	/* Swap bytecode container */` |
|     5259 | 12290 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     5259 | 12291 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 12292 | `	/* Compile the block */` |
|     5259 | 12293 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 12294 | `	/* Fix forward jumps now the destination is resolved  */` |
|     5259 | 12295 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 12296 | `	/* Emit the DONE instruction */` |
|     5259 | 12297 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 12298 | `	/* Leave the block */` |
|     5259 | 12299 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 12300 | `	/* Restore the default container */` |
|     5259 | 12301 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 12302 | `	/* Install the catch block */` |
|     5259 | 12303 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     5259 | 12304 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12305 | `		goto Mem;` |
|        - | 12306 | `	}` |
|     5259 | 12307 | `	return SXRET_OK;` |
|      ! 0 | 12308 | `Mem:` |
|      ! 0 | 12309 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 12310 | `	return SXERR_ABORT;` |
|     2634 | 12311 | `}` |
|        - | 12312 | `/*` |
|        - | 12313 | ` * Compile a 'try' block.` |
|        - | 12314 | ` * A function using an exception should be in a "try" block.` |
|        - | 12315 | ` * If the exception does not trigger, the code will continue` |
|        - | 12316 | ` * as normal. However if the exception triggers, an exception` |
|        - | 12317 | ` * is "thrown".` |
|        - | 12318 | ` */` |
|     5414 | 12319 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 12320 | `{` |
|        - | 12321 | `	ph7_exception *pException;` |
|     5419 | 12322 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 12323 | `	GenBlock *pTry;` |
|        - | 12324 | `	sxu32 nJmpIdx;` |
|        - | 12325 | `	sxi32 rc;` |
|        - | 12326 | `	/* Create the exception container */` |
|     5419 | 12327 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     5419 | 12328 | `	if( pException == 0 ){` |
|      ! 0 | 12329 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 12330 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 12331 | `		return SXERR_ABORT;` |
|        - | 12332 | `	}` |
|        - | 12333 | `	/* Zero the structure */` |
|     5419 | 12334 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 12335 | `	/* Initialize fields */` |
|     5419 | 12336 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     5419 | 12337 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     5419 | 12338 | `	pException->iHasFinally = 0;` |
|     5419 | 12339 | `	pException->iFinallyDone = 0;` |
|     5419 | 12340 | `	pException->pVm = pGen->pVm;` |
|        - | 12341 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 12342 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 12343 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 12344 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 12345 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 12346 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     5419 | 12347 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      103 | 12348 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 12349 | `	}` |
|        - | 12350 | `	/* Create the try block */` |
|     5321 | 12351 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     5321 | 12352 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12353 | `		return SXERR_ABORT;` |
|        - | 12354 | `	}` |
|        - | 12355 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     5321 | 12356 | `	pTry->pUserData = pException;` |
|        - | 12357 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     5321 | 12358 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 12359 | `	/* Fix the jump later when the destination is resolved */` |
|     5321 | 12360 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     5321 | 12361 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 12362 | `	/* Compile the block */` |
|     5321 | 12363 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     5321 | 12364 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 12365 | `		return SXERR_ABORT;` |
|        - | 12366 | `	}` |
|        - | 12367 | `	/* Fix forward jumps now the destination is resolved */` |
|     5321 | 12368 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 12369 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     5321 | 12370 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 12371 | `	/* Leave the block */` |
|     5321 | 12372 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 12373 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     5321 | 12374 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     5314 | 12375 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 12376 | `		/* Compile one or more catch blocks */` |
|     5254 | 12377 | `		for(;;){` |
|    10508 | 12378 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     7938 | 12379 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     2630 | 12380 | `					break;` |
|        - | 12381 | `			}` |
|     5263 | 12382 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     5263 | 12383 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12384 | `				return SXERR_ABORT;` |
|        - | 12385 | `			}` |
|        5 | 12386 | `		}` |
|     2625 | 12387 | `	}` |
|        - | 12388 | `	/* Compile optional finally block */` |
|     5321 | 12389 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      688 | 12390 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 12391 | `		SySet *pInstrContainer;` |
|        - | 12392 | `		GenBlock *pFinBlock;` |
|      129 | 12393 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 12394 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 12395 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 12396 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12397 | `			return SXERR_ABORT;` |
|        - | 12398 | `		}` |
|        - | 12399 | `		/* Swap bytecode container */` |
|      129 | 12400 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 12401 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 12402 | `		/* Compile the finally body */` |
|      129 | 12403 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 12404 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12405 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 12406 | `			return SXERR_ABORT;` |
|        - | 12407 | `		}` |
|        - | 12408 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 12409 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 12410 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 12411 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 12412 | `		/* Leave the block */` |
|      129 | 12413 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 12414 | `		/* Restore the default container */` |
|      129 | 12415 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 12416 | `		pException->iHasFinally = 1;` |
|       62 | 12417 | `	}` |
|        - | 12418 | `	/* Must have at least one catch or finally */` |
|     5321 | 12419 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        8 | 12420 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 12421 | `			"Cannot use try without catch or finally");` |
|        8 | 12422 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12423 | `			return SXERR_ABORT;` |
|        - | 12424 | `		}` |
|        3 | 12425 | `	}` |
|     5321 | 12426 | `	return SXRET_OK;` |
|     2712 | 12427 | `}` |
|        - | 12428 | `/*` |
|        - | 12429 | ` * Compile a switch block.` |
|        - | 12430 | ` *  (See block-comment below for more information)` |
|        - | 12431 | ` */` |
|      112 | 12432 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 12433 | `{` |
|      117 | 12434 | `	sxi32 rc = SXRET_OK;` |
|      117 | 12435 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 12436 | `		/* Unexpected token */` |
|      ! 0 | 12437 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 12438 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12439 | `			return SXERR_ABORT;` |
|        - | 12440 | `		}` |
|      ! 0 | 12441 | `		pGen->pIn++;` |
|      ! 0 | 12442 | `	}` |
|      117 | 12443 | `	pGen->pIn++;` |
|        - | 12444 | `	/* First instruction to execute in this block. */` |
|      117 | 12445 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 12446 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 12447 | `	 * or the '}' token */` |
|      206 | 12448 | `	for(;;){` |
|      417 | 12449 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 12450 | `			/* No more input to process */` |
|      ! 0 | 12451 | `			break;` |
|        - | 12452 | `		}` |
|      417 | 12453 | `		rc = SXRET_OK;` |
|      417 | 12454 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       85 | 12455 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|       31 | 12456 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 12457 | `					/* Unexpected token */` |
|      ! 0 | 12458 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 12459 | `						&pGen->pIn->sData);` |
|      ! 0 | 12460 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 12461 | `						return SXERR_ABORT;` |
|        - | 12462 | `					}` |
|        - | 12463 | `					/* FALL THROUGH */` |
|      ! 0 | 12464 | `				}` |
|       31 | 12465 | `				rc = SXERR_EOF;` |
|       31 | 12466 | `				break;` |
|        - | 12467 | `			}` |
|       32 | 12468 | `		}else{` |
|        - | 12469 | `			sxi32 nKwrd;` |
|        - | 12470 | `			/* Extract the keyword */` |
|      337 | 12471 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      337 | 12472 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|       47 | 12473 | `				break;` |
|        - | 12474 | `			}` |
|      253 | 12475 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 12476 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 12477 | `					/* Unexpected token */` |
|      ! 0 | 12478 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 12479 | `						&pGen->pIn->sData);` |
|      ! 0 | 12480 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 12481 | `						return SXERR_ABORT;` |
|        - | 12482 | `					}` |
|        - | 12483 | `					/* FALL THROUGH */` |
|      ! 0 | 12484 | `				}` |
|        - | 12485 | `				/* Block compiled */` |
|        3 | 12486 | `				break;` |
|        - | 12487 | `			}` |
|        - | 12488 | `		}` |
|        - | 12489 | `		/* Compile block */` |
|      305 | 12490 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      305 | 12491 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12492 | `			return SXERR_ABORT;` |
|        - | 12493 | `		}` |
|        5 | 12494 | `	}` |
|      117 | 12495 | `	return rc;` |
|       61 | 12496 | `}` |
|        - | 12497 | `/*` |
|        - | 12498 | ` * Compile a case eXpression.` |
|        - | 12499 | ` *  (See block-comment below for more information)` |
|        - | 12500 | ` */` |
|       92 | 12501 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 12502 | `{` |
|        - | 12503 | `	SySet *pInstrContainer;` |
|        - | 12504 | `	SyToken *pEnd,*pTmp;` |
|       97 | 12505 | `	sxi32 iNest = 0;` |
|        - | 12506 | `	sxi32 rc;` |
|        - | 12507 | `	/* Delimit the expression */` |
|       97 | 12508 | `	pEnd = pGen->pIn;` |
|      197 | 12509 | `	while( pEnd < pGen->pEnd ){` |
|      197 | 12510 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 12511 | `			/* Increment nesting level */` |
|        3 | 12512 | `			iNest++;` |
|      196 | 12513 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 12514 | `			/* Decrement nesting level */` |
|        3 | 12515 | `			iNest--;` |
|      194 | 12516 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|       97 | 12517 | `			break;` |
|        - | 12518 | `		}` |
|      105 | 12519 | `		pEnd++;` |
|        5 | 12520 | `	}` |
|       97 | 12521 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 12522 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 12523 | `		if( rc == SXERR_ABORT ){` |
|        - | 12524 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 12525 | `			return SXERR_ABORT;` |
|        - | 12526 | `		}` |
|      ! 0 | 12527 | `	}` |
|        - | 12528 | `	/* Swap token stream */` |
|       97 | 12529 | `	pTmp = pGen->pEnd;` |
|       97 | 12530 | `	pGen->pEnd = pEnd;` |
|       97 | 12531 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       97 | 12532 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|       97 | 12533 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 12534 | `	/* Emit the done instruction */` |
|       97 | 12535 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       97 | 12536 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 12537 | `	/* Update token stream */` |
|       97 | 12538 | `	pGen->pIn  = pEnd;` |
|       97 | 12539 | `	pGen->pEnd = pTmp;` |
|       97 | 12540 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 12541 | `		return SXERR_ABORT;` |
|        - | 12542 | `	}` |
|       97 | 12543 | `	return SXRET_OK;` |
|       51 | 12544 | `}` |
|        - | 12545 | `/*` |
|        - | 12546 | ` * Compile the smart switch statement.` |
|        - | 12547 | ` * According to the PHP language reference manual` |
|        - | 12548 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 12549 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 12550 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 12551 | ` *  This is exactly what the switch statement is for.` |
|        - | 12552 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 12553 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 12554 | ` *  of the outer loop, use continue 2.` |
|        - | 12555 | ` *  Note that switch/case does loose comparision.` |
|        - | 12556 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 12557 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 12558 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 12559 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 12560 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 12561 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 12562 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 12563 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 12564 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 12565 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 12566 | ` *  list for the next case.` |
|        - | 12567 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 12568 | ` *  or floating-point numbers and strings.` |
|        - | 12569 | ` */` |
|       28 | 12570 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 12571 | `{` |
|        - | 12572 | `	GenBlock *pSwitchBlock;` |
|        - | 12573 | `	SyToken *pTmp,*pEnd;` |
|        - | 12574 | `	ph7_switch *pSwitch;` |
|        - | 12575 | `	sxu32 nToken;` |
|        - | 12576 | `	sxu32 nLine;` |
|        - | 12577 | `	sxi32 rc;` |
|       33 | 12578 | `	nLine = pGen->pIn->nLine;` |
|        - | 12579 | `	/* Jump the 'switch' keyword */` |
|       33 | 12580 | `	pGen->pIn++;` |
|       33 | 12581 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 12582 | `		/* Syntax error */` |
|      ! 0 | 12583 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 12584 | `		if( rc == SXERR_ABORT ){` |
|        - | 12585 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 12586 | `			return SXERR_ABORT;` |
|        - | 12587 | `		}` |
|      ! 0 | 12588 | `		goto Synchronize;` |
|        - | 12589 | `	}` |
|        - | 12590 | `	/* Jump the left parenthesis '(' */` |
|       33 | 12591 | `	pGen->pIn++;` |
|       33 | 12592 | `	pEnd = 0; /* cc warning */` |
|        - | 12593 | `	/* Create the loop block */` |
|       47 | 12594 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|       14 | 12595 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|       33 | 12596 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12597 | `		return SXERR_ABORT;` |
|        - | 12598 | `	}` |
|        - | 12599 | `	/* Delimit the condition */` |
|       33 | 12600 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       33 | 12601 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 12602 | `		/* Empty expression */` |
|      ! 0 | 12603 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 12604 | `		if( rc == SXERR_ABORT ){` |
|        - | 12605 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 12606 | `			return SXERR_ABORT;` |
|        - | 12607 | `		}` |
|      ! 0 | 12608 | `	}` |
|        - | 12609 | `	/* Swap token streams */` |
|       33 | 12610 | `	pTmp = pGen->pEnd;` |
|       33 | 12611 | `	pGen->pEnd = pEnd;` |
|        - | 12612 | `	/* Compile the expression */` |
|       33 | 12613 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       33 | 12614 | `	if( rc == SXERR_ABORT ){` |
|        - | 12615 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 12616 | `		return SXERR_ABORT;` |
|        - | 12617 | `	}` |
|        - | 12618 | `	/* Update token stream */` |
|       33 | 12619 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 12620 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 12621 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 12622 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12623 | `			return SXERR_ABORT;` |
|        - | 12624 | `		}` |
|      ! 0 | 12625 | `		pGen->pIn++;` |
|      ! 0 | 12626 | `	}` |
|       33 | 12627 | `	pGen->pIn  = &pEnd[1];` |
|       33 | 12628 | `	pGen->pEnd = pTmp;` |
|       33 | 12629 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       28 | 12630 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 12631 | `			pTmp = pGen->pIn;` |
|      ! 0 | 12632 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 12633 | `				pTmp--;` |
|      ! 0 | 12634 | `			}` |
|        - | 12635 | `			/* Unexpected token */` |
|      ! 0 | 12636 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 12637 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12638 | `				return SXERR_ABORT;` |
|        - | 12639 | `			}` |
|      ! 0 | 12640 | `			goto Synchronize;` |
|        - | 12641 | `	}` |
|        - | 12642 | `	/* Set the delimiter token */` |
|       33 | 12643 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 12644 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 12645 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 12646 | `	}else{` |
|       31 | 12647 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 12648 | `	}` |
|       33 | 12649 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 12650 | `	/* Create the switch blocks container */` |
|       33 | 12651 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|       33 | 12652 | `	if( pSwitch == 0 ){` |
|        - | 12653 | `		/* Abort compilation */` |
|      ! 0 | 12654 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 12655 | `		return SXERR_ABORT;` |
|        - | 12656 | `	}` |
|        - | 12657 | `	/* Zero the structure */` |
|       33 | 12658 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 12659 | `	/* Initialize fields */` |
|       33 | 12660 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 12661 | `	/* Emit the switch instruction */` |
|       33 | 12662 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 12663 | `	/* Compile case blocks */` |
|      100 | 12664 | `	for(;;){` |
|        - | 12665 | `		sxu32 nKwrd;` |
|      119 | 12666 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 12667 | `			/* No more input to process */` |
|      ! 0 | 12668 | `			break;` |
|        - | 12669 | `		}` |
|      119 | 12670 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 12671 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 12672 | `				/* Unexpected token */` |
|      ! 0 | 12673 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12674 | `					&pGen->pIn->sData);` |
|      ! 0 | 12675 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 12676 | `					return SXERR_ABORT;` |
|        - | 12677 | `				}` |
|        - | 12678 | `				/* FALL THROUGH */` |
|      ! 0 | 12679 | `			}` |
|        - | 12680 | `			/* Block compiled */` |
|      ! 0 | 12681 | `			break;` |
|        - | 12682 | `		}` |
|        - | 12683 | `		/* Extract the keyword */` |
|      119 | 12684 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      119 | 12685 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 12686 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 12687 | `				/* Unexpected token */` |
|      ! 0 | 12688 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12689 | `					&pGen->pIn->sData);` |
|      ! 0 | 12690 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 12691 | `					return SXERR_ABORT;` |
|        - | 12692 | `				}` |
|        - | 12693 | `				/* FALL THROUGH */` |
|      ! 0 | 12694 | `			}` |
|        - | 12695 | `			/* Block compiled */` |
|        3 | 12696 | `			break;` |
|        - | 12697 | `		}` |
|      117 | 12698 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 12699 | `			/*` |
|        - | 12700 | `			 * Accroding to the PHP language reference manual` |
|        - | 12701 | `			 *  A special case is the default case. This case matches anything` |
|        - | 12702 | `			 *  that wasn't matched by the other cases.` |
|        - | 12703 | `			 */` |
|       25 | 12704 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 12705 | `				/* Default case already compiled */` |
|      ! 0 | 12706 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 12707 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 12708 | `					return SXERR_ABORT;` |
|        - | 12709 | `				}` |
|      ! 0 | 12710 | `			}` |
|       25 | 12711 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 12712 | `			/* Compile the default block */` |
|       25 | 12713 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|       25 | 12714 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 12715 | `				return SXERR_ABORT;` |
|       25 | 12716 | `			}else if( rc == SXERR_EOF ){` |
|       23 | 12717 | `				break;` |
|        1 | 12718 | `			}` |
|       98 | 12719 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 12720 | `			ph7_case_expr sCase;` |
|        - | 12721 | `			/* Standard case block */` |
|       97 | 12722 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 12723 | `			/* initialize the structure */` |
|       97 | 12724 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 12725 | `			/* Compile the case expression */` |
|       97 | 12726 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|       97 | 12727 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12728 | `				return SXERR_ABORT;` |
|        - | 12729 | `			}` |
|        - | 12730 | `			/* Compile the case block */` |
|       97 | 12731 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 12732 | `			/* Insert in the switch container */` |
|       97 | 12733 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|       97 | 12734 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 12735 | `				return SXERR_ABORT;` |
|       97 | 12736 | `			}else if( rc == SXERR_EOF ){` |
|        9 | 12737 | `				break;` |
|        - | 12738 | `			}` |
|       47 | 12739 | `		}else{` |
|        - | 12740 | `			/* Unexpected token */` |
|      ! 0 | 12741 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12742 | `				&pGen->pIn->sData);` |
|      ! 0 | 12743 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12744 | `				return SXERR_ABORT;` |
|        - | 12745 | `			}` |
|      ! 0 | 12746 | `			break;` |
|        - | 12747 | `		}` |
|        5 | 12748 | `	}` |
|        - | 12749 | `	/* Fix all jumps now the destination is resolved */` |
|       33 | 12750 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|       33 | 12751 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 12752 | `	/* Release the loop block */` |
|       33 | 12753 | `	GenStateLeaveBlock(pGen,0);` |
|       33 | 12754 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 12755 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|       33 | 12756 | `		pGen->pIn++;` |
|       14 | 12757 | `	}` |
|        - | 12758 | `	/* Statement successfully compiled */` |
|       33 | 12759 | `	return SXRET_OK;` |
|      ! 0 | 12760 | `Synchronize:` |
|        - | 12761 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 12762 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 12763 | `		pGen->pIn++;` |
|      ! 0 | 12764 | `	}` |
|      ! 0 | 12765 | `	return SXRET_OK;` |
|       19 | 12766 | `}` |
|        - | 12767 | `/*` |
|        - | 12768 | ` * Chain operators participate in a postfix member-access chain.` |
|        - | 12769 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|        - | 12770 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|        - | 12771 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|        - | 12772 | ` */` |
|        - | 12773 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|        - | 12774 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|        - | 12775 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|        - | 12776 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|        - | 12777 |  |
|        - | 12778 | `/*` |
|        - | 12779 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|        - | 12780 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|        - | 12781 | ` * patched entries from the pending set.` |
|        - | 12782 | ` */` |
| 23373846 | 12783 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|        5 | 12784 | `{` |
| 23373851 | 12785 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|        - | 12786 | `	sxu32 nTarget;` |
|        - | 12787 | `	sxu32 *aIdx;` |
|        - | 12788 | `	sxu32 i;` |
| 23373851 | 12789 | `	if( nCur <= nBaseline ){` |
| 23373755 | 12790 | `		return;` |
|        - | 12791 | `	}` |
|      100 | 12792 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      100 | 12793 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|      204 | 12794 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|      108 | 12795 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|      108 | 12796 | `		if( pInstr ){` |
|      108 | 12797 | `			pInstr->iP2 = (sxi32)nTarget;` |
|       52 | 12798 | `		}` |
|       56 | 12799 | `	}` |
|      100 | 12800 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 11686928 | 12801 | `}` |
|        - | 12802 |  |
|        - | 12803 | `/*` |
|        - | 12804 | ` * By-reference out-parameters of builtin functions.` |
|        - | 12805 | ` *` |
|        - | 12806 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|        - | 12807 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|        - | 12808 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|        - | 12809 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|        - | 12810 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|        - | 12811 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|        - | 12812 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|        - | 12813 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|        - | 12814 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|        - | 12815 | ` * creates it" behaviour).` |
|        - | 12816 | ` *` |
|        - | 12817 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|        - | 12818 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|        - | 12819 | ` */` |
|  3282008 | 12820 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|        5 | 12821 | `{` |
|        - | 12822 | `	static const struct {` |
|        - | 12823 | `		const char *zName;` |
|        - | 12824 | `		sxu32 nByte;` |
|        - | 12825 | `		sxu32 mask;` |
|        - | 12826 | `	} aByRef[] = {` |
|        - | 12827 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12828 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12829 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12830 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12831 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|        - | 12832 | `	};` |
|        - | 12833 | `	sxu32 i;` |
|  3282013 | 12834 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   866167 | 12835 | `		return 0;` |
|        - | 12836 | `	}` |
| 14494687 | 12837 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 12078958 | 12838 | `		if( pName->nByte == aByRef[i].nByte` |
|  6184904 | 12839 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      127 | 12840 | `			return aByRef[i].mask;` |
|        - | 12841 | `		}` |
|  6039423 | 12842 | `	}` |
|  2415729 | 12843 | `	return 0;` |
|  1641009 | 12844 | `}` |
|        - | 12845 | `/*` |
|        - | 12846 | ` * Recover the bare global-builtin name from a call's callee node.` |
|        - | 12847 | ` *` |
|        - | 12848 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|        - | 12849 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|        - | 12850 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|        - | 12851 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|        - | 12852 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|        - | 12853 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|        - | 12854 | ` */` |
|  3282008 | 12855 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|        5 | 12856 | `{` |
|        - | 12857 | `	SyToken *p, *pEnd;` |
|  3282013 | 12858 | `	pOut->zString = 0;` |
|  3282013 | 12859 | `	pOut->nByte = 0;` |
|  3282013 | 12860 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|      ! 0 | 12861 | `		return;` |
|        - | 12862 | `	}` |
|  3282013 | 12863 | `	p = pLeft->pStart;` |
|  3282013 | 12864 | `	pEnd = pLeft->pEnd;` |
|        - | 12865 | `	/* Optional single leading namespace separator (absolute path). */` |
|  3282013 | 12866 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|     3917 | 12867 | `		p++;` |
|     1956 | 12868 | `	}` |
|  3282013 | 12869 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   866131 | 12870 | `		return;` |
|        - | 12871 | `	}` |
|        - | 12872 | `	/* Must be a single component: nothing follows the name token. */` |
|  2415887 | 12873 | `	if( p + 1 != pEnd ){` |
|       40 | 12874 | `		return;` |
|        - | 12875 | `	}` |
|  2415851 | 12876 | `	*pOut = p->sData;` |
|  1641009 | 12877 | `}` |
|        - | 12878 | `/*` |
|        - | 12879 | ` * Generate bytecode for a given expression tree.` |
|        - | 12880 | ` * If something goes wrong while generating bytecode` |
|        - | 12881 | ` * for the expression tree (A very unlikely scenario)` |
|        - | 12882 | ` * this function takes care of generating the appropriate` |
|        - | 12883 | ` * error message.` |
|        - | 12884 | ` */` |
| 32454548 | 12885 | `static sxi32 GenStateEmitExprCode(` |
|        - | 12886 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 12887 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|        - | 12888 | `	sxi32 iFlags /* Control flags */` |
|        - | 12889 | `	)` |
|        5 | 12890 | `{` |
|        - | 12891 | `	VmInstr *pInstr;` |
|        - | 12892 | `	sxu32 nJmpIdx;` |
| 32454553 | 12893 | `	sxi32 iP1 = 0;` |
| 32454553 | 12894 | `	sxu32 iP2 = 0;` |
| 32454553 | 12895 | `	void *p3  = 0;` |
|        - | 12896 | `	sxi32 iVmOp;` |
|        - | 12897 | `	sxi32 rc;` |
| 32454553 | 12898 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 32454553 | 12899 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 32454553 | 12900 | `	sxu32 nRhsNsBase = 0;` |
| 32454553 | 12901 | `	if( pNode->xCode ){` |
|        - | 12902 | `		SyToken *pTmpIn,*pTmpEnd;` |
|        - | 12903 | `		/* Compile node */` |
| 19466783 | 12904 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 19466783 | 12905 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 19466783 | 12906 | `		RE_SWAP_DELIMITER(pGen);` |
| 19466783 | 12907 | `		return rc;` |
|        - | 12908 | `	}` |
| 12987775 | 12909 | `	if( pNode->pOp == 0 ){` |
|      ! 0 | 12910 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12911 | `			"Invalid expression node,PH7 is aborting compilation");` |
|      ! 0 | 12912 | `		return SXERR_ABORT;` |
|        - | 12913 | `	}` |
| 12987775 | 12914 | `	iVmOp = pNode->pOp->iVmOp;` |
| 12987775 | 12915 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|        - | 12916 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|        - | 12917 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|        - | 12918 | `		 * and later errors are still reported. */` |
|        3 | 12919 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12920 | `			"The (unset) cast is no longer supported");` |
|        3 | 12921 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12922 | `			return SXERR_ABORT;` |
|        - | 12923 | `		}` |
|        1 | 12924 | `	}` |
| 12987775 | 12925 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|       91 | 12926 | `		sxu32 nJmp = 0;` |
|        - | 12927 | `		sxu32 nNcNsBase;` |
|        - | 12928 | `		VmInstr *pInstrFix;` |
|        - | 12929 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|        - | 12930 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|        - | 12931 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|        - | 12932 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|        - | 12933 | `		 * stack slot carries a writable nIdx. */` |
|       91 | 12934 | `		if( pNode->pRight ){` |
|       91 | 12935 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       91 | 12936 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       91 | 12937 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12938 | `				return rc;` |
|        - | 12939 | `			}` |
|       91 | 12940 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        - | 12941 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|        - | 12942 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|        - | 12943 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|        - | 12944 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|        - | 12945 | `			 * the store, so the parent array does not need to be copied at` |
|        - | 12946 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|        - | 12947 | `			 * cascade for the actual write path stays correct. */` |
|       91 | 12948 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|       91 | 12949 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       33 | 12950 | `				pInstrFix->iP2 = 3;` |
|       15 | 12951 | `			}` |
|       44 | 12952 | `		}` |
|        - | 12953 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|       91 | 12954 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|        - | 12955 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|       91 | 12956 | `		if( pNode->pLeft ){` |
|       91 | 12957 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       91 | 12958 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|       91 | 12959 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12960 | `				return rc;` |
|        - | 12961 | `			}` |
|       91 | 12962 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       44 | 12963 | `		}` |
|        - | 12964 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|       91 | 12965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|        - | 12966 | `		/* Patch the short-circuit jump to land after the store. */` |
|       91 | 12967 | `		if( nJmp > 0 ){` |
|       91 | 12968 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|       91 | 12969 | `			if( pInstrFix ){` |
|       91 | 12970 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|       44 | 12971 | `			}` |
|       44 | 12972 | `		}` |
|       91 | 12973 | `		return SXRET_OK;` |
|        - | 12974 | `	}` |
| 12987687 | 12975 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|        - | 12976 | `		sxu32 nJz,nJmp;` |
|        - | 12977 | `		sxu32 nTernaryNsBase;` |
|        - | 12978 | `		/* Ternary operator require special handling */` |
|        - | 12979 | `		/* Phase#1: Compile the condition */` |
|   228477 | 12980 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   228477 | 12981 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|   228477 | 12982 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12983 | `			return rc;` |
|        - | 12984 | `		}` |
|        - | 12985 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|        - | 12986 | `		 * compiling the condition must short-circuit to the end of the` |
|        - | 12987 | `		 * condition expression, not leak past the ternary. */` |
|   228477 | 12988 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   228477 | 12989 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|   228477 | 12990 | `		if( pNode->pLeft ){` |
|        - | 12991 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|        - | 12992 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|   228409 | 12993 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 12994 | `			/* Phase#3: Compile the 'then' expression  */` |
|   228409 | 12995 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   228409 | 12996 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|   228409 | 12997 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12998 | `				return rc;` |
|        - | 12999 | `			}` |
|   228409 | 13000 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   114207 | 13001 | `		}else{` |
|        - | 13002 | `			/* Elvis operator: (expr) ?: (else)` |
|        - | 13003 | `			 * Duplicate condition so original value is the 'then' result.` |
|        - | 13004 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       70 | 13005 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       70 | 13006 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 13007 | `		}` |
|        - | 13008 | `		/* Phase#4: Emit the unconditional jump */` |
|   228477 | 13009 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|        - | 13010 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|   228477 | 13011 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|   228477 | 13012 | `		if( pInstr ){` |
|   228477 | 13013 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   114236 | 13014 | `		}` |
|   228477 | 13015 | `		if( !pNode->pLeft ){` |
|        - | 13016 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       70 | 13017 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       34 | 13018 | `		}` |
|        - | 13019 | `		/* Phase#6: Compile the 'else' expression */` |
|   228477 | 13020 | `		if( pNode->pRight ){` |
|   228477 | 13021 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   228477 | 13022 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|   228477 | 13023 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 13024 | `				return rc;` |
|        - | 13025 | `			}` |
|   228477 | 13026 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   114236 | 13027 | `		}` |
|   228477 | 13028 | `		if( nJmp > 0 ){` |
|        - | 13029 | `			/* Phase#7: Fix the unconditional jump */` |
|   228477 | 13030 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|   228477 | 13031 | `			if( pInstr ){` |
|   228477 | 13032 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   114236 | 13033 | `			}` |
|   114236 | 13034 | `		}` |
|        - | 13035 | `		/* All done */` |
|   228477 | 13036 | `		return SXRET_OK;` |
|        - | 13037 | `	}` |
| 12759215 | 13038 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|        - | 13039 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|        - | 13040 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|        - | 13041 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|        - | 13042 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|        - | 13043 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|        - | 13044 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|        - | 13045 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|        - | 13046 | `		sxu32 nPipeNsBase;` |
|       27 | 13047 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|       27 | 13048 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|      ! 0 | 13049 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 13050 | `				"'\|>': Missing operand");` |
|      ! 0 | 13051 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 13052 | `		}` |
|        - | 13053 | `		/* Argument: the LHS value. */` |
|       27 | 13054 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 13055 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|       27 | 13056 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 13057 | `			return rc;` |
|        - | 13058 | `		}` |
|       27 | 13059 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 13060 | `		/* Callable: the RHS. */` |
|       27 | 13061 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 13062 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|       27 | 13063 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 13064 | `			return rc;` |
|        - | 13065 | `		}` |
|       27 | 13066 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 13067 | `		/* Invoke the callable with the single piped argument. */` |
|       27 | 13068 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       27 | 13069 | `		return SXRET_OK;` |
|        - | 13070 | `	}` |
| 12759189 | 13071 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|        - | 13072 | `	/* Generate code for the left tree */` |
| 12759189 | 13073 | `	if( pNode->pLeft ){` |
| 12747535 | 13074 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 12747535 | 13075 | `		if( iVmOp == PH7_OP_CALL ){` |
|        - | 13076 | `			ph7_expr_node **apNode;` |
|  3286213 | 13077 | `			int hasSpread = 0;` |
|  3286213 | 13078 | `			int hasNamed = 0;` |
|  3286213 | 13079 | `			int bAnySpread = 0;` |
|  3286213 | 13080 | `			sxu32 byRefMask = 0;` |
|        - | 13081 | `			sxi32 nArgs;` |
|        - | 13082 | `			sxi32 n;` |
|        - | 13083 | `			/* Recurse and generate bytecodes for function arguments */` |
|  3286213 | 13084 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3286213 | 13085 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|        - | 13086 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|        - | 13087 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|        - | 13088 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  3286213 | 13089 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|       81 | 13090 | `				bFcc = 1;` |
|       81 | 13091 | `				nArgs = 0;` |
|       40 | 13092 | `			}` |
|        - | 13093 | `			/* Validate argument order like php: no positional argument after a` |
|        - | 13094 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|        - | 13095 | `			{` |
|  3286213 | 13096 | `				int seenNamed = 0;` |
|  3286213 | 13097 | `				int seenSpread = 0;` |
|  6527787 | 13098 | `				for( n = 0; n < nArgs; ++n ){` |
|  3241581 | 13099 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|     4073 | 13100 | `						bAnySpread = 1;` |
|     4073 | 13101 | `						seenSpread = 1;` |
|     4073 | 13102 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      ! 0 | 13103 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 13104 | `								"syntax error, unexpected token \"...\"");` |
|      ! 0 | 13105 | `							return SXERR_SYNTAX;` |
|        5 | 13106 | `						}` |
|  3239547 | 13107 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      289 | 13108 | `						seenNamed = 1;` |
|      289 | 13109 | `						hasNamed = 1;` |
|  3237371 | 13110 | `					}else if( seenNamed ){` |
|        3 | 13111 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 13112 | `							"Cannot use positional argument after named argument");` |
|        3 | 13113 | `						return SXERR_SYNTAX;` |
|  3237227 | 13114 | `					}else if( seenSpread ){` |
|      ! 0 | 13115 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 13116 | `							"Cannot use positional argument after argument unpacking");` |
|      ! 0 | 13117 | `						return SXERR_SYNTAX;` |
|        - | 13118 | `					}` |
|  1620792 | 13119 | `				}` |
|        - | 13120 | `			}` |
|        - | 13121 | `			/* Read-only load */` |
|  3286211 | 13122 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|        - | 13123 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|        - | 13124 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|        - | 13125 | `			 * objects dispatch to the right method (offsetExists for both;` |
|        - | 13126 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  3286211 | 13127 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  3286211 | 13128 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  3286206 | 13129 | `				if( pCallName->nByte == 5` |
|  1821080 | 13130 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   194563 | 13131 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  3188932 | 13132 | `				}else if( pCallName->nByte == 5` |
|  1626522 | 13133 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      103 | 13134 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       49 | 13135 | `				}` |
|        - | 13136 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|        - | 13137 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|        - | 13138 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|        - | 13139 | `				 * write back through. Skipped when spread/named args are present:` |
|        - | 13140 | `				 * the compile-time positional index no longer maps to the` |
|        - | 13141 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  3286211 | 13142 | `				if( !bAnySpread && !hasNamed ){` |
|        - | 13143 | `					SyString sBuiltin;` |
|  3282013 | 13144 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  3282013 | 13145 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  1641004 | 13146 | `				}` |
|  1643103 | 13147 | `			}` |
|  6527783 | 13148 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  3241577 | 13149 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  3241577 | 13150 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 13151 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|        - | 13152 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|        - | 13153 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|        - | 13154 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|        - | 13155 | `				 * builtin to write back through. A plain $var target is unaffected` |
|        - | 13156 | `				 * (iP1=0 either way). */` |
|  3241577 | 13157 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|       61 | 13158 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|       61 | 13159 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|       28 | 13160 | `				}` |
|  3241577 | 13161 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  3241577 | 13162 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 13163 | `					return rc;` |
|        - | 13164 | `				}` |
|        - | 13165 | `				/* Each argument is an independent nullsafe scope. */` |
|  3241577 | 13166 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  3241577 | 13167 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|        - | 13168 | `					/* Emit spread opcode to unpack this array argument */` |
|     4073 | 13169 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|     4073 | 13170 | `					hasSpread = 1;` |
|     2034 | 13171 | `				}` |
|  1620791 | 13172 | `			}` |
|        - | 13173 | `			/* Total number of given arguments */` |
|  3286211 | 13174 | `			iP1 = nArgs;` |
|  3286211 | 13175 | `			iP2 = hasSpread;` |
|        - | 13176 | `			/* Build VmCallArgMap if named arguments are present.` |
|        - | 13177 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  3286211 | 13178 | `			if( hasNamed ){` |
|      178 | 13179 | `				sxu32 nStrBytes = 0;` |
|        - | 13180 | `				char *zBuf;` |
|      534 | 13181 | `				for( n = 0; n < nArgs; ++n ){` |
|      360 | 13182 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 13183 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      141 | 13184 | `					}` |
|      182 | 13185 | `				}` |
|        - | 13186 | `				{` |
|      178 | 13187 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      178 | 13188 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      174 | 13189 | `					&pGen->pVm->sAllocator, mapSize);` |
|      178 | 13190 | `				if( pMap ){` |
|      178 | 13191 | `					SyZero(pMap, mapSize);` |
|      178 | 13192 | `					pMap->bHasNamed = 1;` |
|      178 | 13193 | `					pMap->nTotal = (sxu32)nArgs;` |
|      178 | 13194 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      178 | 13195 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|      534 | 13196 | `					for( n = 0; n < nArgs; ++n ){` |
|      360 | 13197 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 13198 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      286 | 13199 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      286 | 13200 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      286 | 13201 | `							zBuf += nb;` |
|      141 | 13202 | `						}` |
|        - | 13203 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      182 | 13204 | `					}` |
|      178 | 13205 | `					p3 = (void *)pMap;` |
|       87 | 13206 | `				}` |
|        - | 13207 | `				}` |
|       87 | 13208 | `			}` |
|        - | 13209 | `			/* Remove stale flags now */` |
|  3286211 | 13210 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  1643103 | 13211 | `		}` |
|        - | 13212 | `		{` |
|        - | 13213 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|        - | 13214 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|        - | 13215 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|        - | 13216 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|        - | 13217 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|        - | 13218 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|        - | 13219 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|        - | 13220 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 12747533 | 13221 | `			sxi32 iLeftFlags = iFlags;` |
| 12747528 | 13222 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 10620398 | 13223 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  4246660 | 13224 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  3769081 | 13225 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   971097 | 13226 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   485546 | 13227 | `			}` |
|        - | 13228 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|        - | 13229 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|        - | 13230 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|        - | 13231 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|        - | 13232 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|        - | 13233 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|        - | 13234 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 12747528 | 13235 | `			if( pNode->pOp` |
| 18112637 | 13236 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 11738920 | 13237 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 10730260 | 13238 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  2049097 | 13239 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|  1024546 | 13240 | `			}` |
|        - | 13241 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|        - | 13242 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|        - | 13243 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|        - | 13244 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|        - | 13245 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|        - | 13246 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
| 12747528 | 13247 | `			if( pNode->pOp` |
| 12747533 | 13248 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    70395 | 13249 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|    35195 | 13250 | `			}` |
| 12747533 | 13251 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|        - | 13252 | `		}` |
| 12747533 | 13253 | `		if( rc != SXRET_OK ){` |
|       34 | 13254 | `			return rc;` |
|        - | 13255 | `		}` |
| 12747503 | 13256 | `		if( !bIsChainOp ){` |
|        - | 13257 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|        - | 13258 | `			 * target the end of that LHS chain, which is right here. */` |
|  5727519 | 13259 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  2863757 | 13260 | `		}` |
| 12747503 | 13261 | `		if( iVmOp == PH7_OP_CALL ){` |
|  3286211 | 13262 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  3286211 | 13263 | `			if( pInstr ){` |
|  3286211 | 13264 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  2416127 | 13265 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|        - | 13266 | `					sxu32 nQual;` |
|  2416127 | 13267 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 13268 | `					/* Prevent constant expansion but preserve the absolute flag` |
|        - | 13269 | `					 * so the later NEW handler (if any) can see it. */` |
|  2416127 | 13270 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|        - | 13271 | `					/* Namespace-qualify the function name for CALL, unless the` |
|        - | 13272 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|        - | 13273 | `					 * imports — class imports must NOT affect function` |
|        - | 13274 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|        - | 13275 | `					 * before NEW; we store the original literal index in the` |
|        - | 13276 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|        - | 13277 | `					 * the unqualified name and re-qualify with class imports. */` |
|  2416127 | 13278 | `					if( bAbsolute ){` |
|     3917 | 13279 | `						pInstr->iP2 = (sxi32)nOrig;` |
|     1961 | 13280 | `					}else{` |
|  2412215 | 13281 | `						int fromImport = 0;` |
|  2412215 | 13282 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  2412215 | 13283 | `						pInstr->iP2 = (sxi32)nQual;` |
|  2412215 | 13284 | `						if( nQual != nOrig ){` |
|        - | 13285 | `							/* Record the original literal index in the arg map` |
|        - | 13286 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|        - | 13287 | `							 * flag) so the NEW handler can recover the` |
|        - | 13288 | `							 * unqualified name and re-qualify with CLASS` |
|        - | 13289 | `							 * imports. */` |
|       77 | 13290 | `							if( p3 == 0 ){` |
|       77 | 13291 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       72 | 13292 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       77 | 13293 | `								if( pMap ){` |
|       77 | 13294 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       77 | 13295 | `									p3 = (void *)pMap;` |
|       36 | 13296 | `								}` |
|       36 | 13297 | `							}` |
|       77 | 13298 | `							if( p3 ){` |
|       77 | 13299 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       77 | 13300 | `								if( !fromImport ){` |
|        - | 13301 | `									/* Mark as namespace-qualified */` |
|       67 | 13302 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       31 | 13303 | `								}` |
|       36 | 13304 | `							}` |
|       36 | 13305 | `						}` |
|        5 | 13306 | `					}` |
|  2078150 | 13307 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|        - | 13308 | `					/* Method call,flag that */` |
|   865599 | 13309 | `					pInstr->iP2 = 1;` |
|   432797 | 13310 | `				}` |
|  1643108 | 13311 | `			}` |
| 11104400 | 13312 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|        - | 13313 | `			ph7_expr_node **apNode;` |
|        - | 13314 | `			sxi32 n;` |
|  1684691 | 13315 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 13316 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|        - | 13317 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 13318 | `			/* Recurse and generate bytecodes for array index */` |
|  1684691 | 13319 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3241073 | 13320 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  1556387 | 13321 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1556387 | 13322 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|  1556387 | 13323 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 13324 | `					return rc;` |
|        - | 13325 | `				}` |
|        - | 13326 | `				/* Each subscript index is an independent nullsafe scope. */` |
|  1556387 | 13327 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   778196 | 13328 | `			}` |
|  1684691 | 13329 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|  1556387 | 13330 | `				iP1 = 1; /* Node have an index associated with it */` |
|   778191 | 13331 | `			}` |
|  1684691 | 13332 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|        - | 13333 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|   233287 | 13334 | `				iP2 = 4;` |
|  1568050 | 13335 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 13336 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|        - | 13337 | `				 * so the trailing unset() builtin can drop the slot. */` |
|       72 | 13338 | `				iP2 = 5;` |
|  1451375 | 13339 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        - | 13340 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|        - | 13341 | `				 * short-circuit on missing keys without invoking offsetGet` |
|        - | 13342 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|       29 | 13343 | `				iP2 = 6;` |
|  1451329 | 13344 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|        - | 13345 | `				/* Create an empty entry when the desired index is not found */` |
|   198687 | 13346 | `				iP2 = 1;` |
|    99346 | 13347 | `			}` |
|  8618954 | 13348 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        - | 13349 | `			/* POP the left node */` |
|       32 | 13350 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       15 | 13351 | `		}` |
|  6373749 | 13352 | `	}` |
| 12759157 | 13353 | `	rc = SXRET_OK;` |
| 12759157 | 13354 | `	nJmpIdx = 0;` |
|        - | 13355 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|        - | 13356 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|        - | 13357 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 12759157 | 13358 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    43431 | 13359 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    43431 | 13360 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    43431 | 13361 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    43431 | 13362 | `			int isSpecial = 0;` |
|    43431 | 13363 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    20095 | 13364 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    20095 | 13365 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    20090 | 13366 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    31690 | 13367 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    15847 | 13368 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    11793 | 13369 | `					isSpecial = 1;` |
|     5894 | 13370 | `				}` |
|    15879 | 13371 | `			}` |
|    55099 | 13372 | `			pInstr->iP1 = 0;` |
|    55099 | 13373 | `			if( !isSpecial ){` |
|    19975 | 13374 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     9985 | 13375 | `			}` |
|        - | 13376 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|        - | 13377 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    31763 | 13378 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    19975 | 13379 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    19975 | 13380 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       60 | 13381 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|       62 | 13382 | `					return SXRET_OK;` |
|        - | 13383 | `				}` |
|     9956 | 13384 | `			}` |
|    15850 | 13385 | `		}` |
|    39165 | 13386 | `	}` |
|        - | 13387 | `	/* Generate code for the right tree */` |
| 12747445 | 13388 | `	if( pNode->pRight ){` |
|  6928865 | 13389 | `		if( iVmOp == PH7_OP_LAND ){` |
|        - | 13390 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   159775 | 13391 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6848980 | 13392 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|        - | 13393 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    97283 | 13394 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6720456 | 13395 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|        - | 13396 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      145 | 13397 | `			iVmOp = 0; /* No binary operator to emit */` |
|      145 | 13398 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  6671799 | 13399 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        - | 13400 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|        - | 13401 | `			 * the entire containing postfix chain to null. The jump target is` |
|        - | 13402 | `			 * patched later by the innermost non-chain ancestor (or by` |
|        - | 13403 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|        - | 13404 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|        - | 13405 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|      108 | 13406 | `			sxu32 nNsJmp = 0;` |
|      108 | 13407 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|      108 | 13408 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  6671625 | 13409 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|        - | 13410 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|        - | 13411 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|        - | 13412 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  2360973 | 13413 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  1180484 | 13414 | `		}` |
|  6928865 | 13415 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  6928865 | 13416 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  6928865 | 13417 | `		if( !bIsChainOp ){` |
|        - | 13418 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|        - | 13419 | `			 * operator instruction is emitted. */` |
|  4879831 | 13420 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  2439913 | 13421 | `		}` |
|  6928865 | 13422 | `		if( iVmOp == PH7_OP_STORE ){` |
|  2073237 | 13423 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  2073202 | 13424 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|        - | 13425 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|        - | 13426 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|        - | 13427 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|        - | 13428 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|        - | 13429 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|        - | 13430 | `				 */` |
|       91 | 13431 | `				iVmOp = 0;` |
|  2073194 | 13432 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  2073151 | 13433 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 13434 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   249267 | 13435 | `					iP2 = 1;` |
|   124636 | 13436 | `				}else{` |
|  1823889 | 13437 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 13438 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   198595 | 13439 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   198595 | 13440 | `						iP1 = pInstr->iP1;` |
|    99300 | 13441 | `					}else{` |
|  1625299 | 13442 | `						p3 = pInstr->p3;` |
|        - | 13443 | `					}` |
|        - | 13444 | `					/* POP the last dynamic load instruction */` |
|  1823889 | 13445 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 13446 | `				}` |
|  1036578 | 13447 | `			}` |
|  5892249 | 13448 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|       64 | 13449 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|       64 | 13450 | `			if( pInstr ){` |
|       64 | 13451 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 13452 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|        - | 13453 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|        - | 13454 | `					 */` |
|       19 | 13455 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|       19 | 13456 | `					iP1 = pInstr->iP1;` |
|       19 | 13457 | `					iP2 = pInstr->iP2;` |
|       19 | 13458 | `					p3  = pInstr->p3;` |
|       10 | 13459 | `				}else{` |
|       46 | 13460 | `					p3 = pInstr->p3;` |
|        - | 13461 | `				}` |
|       30 | 13462 | `			}` |
|       30 | 13463 | `		}` |
|  3464430 | 13464 | `	}` |
| 12747440 | 13465 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   246065 | 13466 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|        - | 13467 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|        - | 13468 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       32 | 13469 | `		iVmOp = 0;` |
|       14 | 13470 | `	}` |
| 12747445 | 13471 | `	if( iVmOp > 0 ){` |
| 12747161 | 13472 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    70395 | 13473 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|        - | 13474 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    11687 | 13475 | `				iP1 = 1;` |
|     5846 | 13476 | `			}` |
| 12711966 | 13477 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|        - | 13478 | `			/* Namespace-qualify the class name for NEW */ {` |
|   491821 | 13479 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   491821 | 13480 | `				VmInstr *pCallInstr = 0;` |
|   491821 | 13481 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   491573 | 13482 | `					pCallInstr = pPeek;` |
|   491573 | 13483 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   245784 | 13484 | `				}` |
|   491821 | 13485 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   491817 | 13486 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 13487 | `					sxu32 nLitForClass;` |
|   491817 | 13488 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|        - | 13489 | `					/* If the CALL handler qualified the name with FUNCTION` |
|        - | 13490 | `					 * imports, recover the original literal (recorded in the` |
|        - | 13491 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|        - | 13492 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|        - | 13493 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|        - | 13494 | `					 * with class imports. */` |
|   491817 | 13495 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       37 | 13496 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|       21 | 13497 | `					}else{` |
|   491785 | 13498 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|        - | 13499 | `					}` |
|   491817 | 13500 | `					pPeek->iP1 = 0;` |
|   491817 | 13501 | `					if( !bAbsolute ){` |
|   487909 | 13502 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   243957 | 13503 | `					}else{` |
|     3913 | 13504 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|        - | 13505 | `					}` |
|   245906 | 13506 | `				}` |
|        - | 13507 | `			}` |
|   491821 | 13508 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   491821 | 13509 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|        - | 13510 | `				VmInstr *pPrev;` |
|   491573 | 13511 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   491573 | 13512 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|        - | 13513 | `					/* Pop the call instruction, preserve named-arg map and` |
|        - | 13514 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|        - | 13515 | `					 * accumulator exactly like OP_CALL would have). */` |
|   491573 | 13516 | `					iP1 = pInstr->iP1;` |
|   491573 | 13517 | `					iP2 = pInstr->iP2;` |
|   491573 | 13518 | `					if( pInstr->p3 ){` |
|       47 | 13519 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|       21 | 13520 | `					}` |
|   491573 | 13521 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   245784 | 13522 | `				}` |
|   245789 | 13523 | `			}` |
| 12430863 | 13524 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|        - | 13525 | `			/* instanceof: right operand is a class name, not a constant.` |
|        - | 13526 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|    39069 | 13527 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    39069 | 13528 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    39069 | 13529 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    39069 | 13530 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|    39069 | 13531 | `				int isSpecialIs = 0;` |
|    39069 | 13532 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    39069 | 13533 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    39069 | 13534 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    39064 | 13535 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    39067 | 13536 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    19532 | 13537 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       12 | 13538 | `						isSpecialIs = 1;` |
|        5 | 13539 | `					}` |
|    19532 | 13540 | `				}` |
|    39069 | 13541 | `				pInstr->iP1 = 0;` |
|    39069 | 13542 | `				if( !isSpecialIs && !bAbsolute ){` |
|    39049 | 13543 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    19522 | 13544 | `				}` |
|    19537 | 13545 | `			}` |
| 12165423 | 13546 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|        - | 13547 | `			/* Prevent constant expansion for member/property names.` |
|        - | 13548 | `			 * The right child (member name) was just compiled — its LOADC` |
|        - | 13549 | `			 * should not trigger constant lookup. */` |
|  2049039 | 13550 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  2049039 | 13551 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  2048967 | 13552 | `				pInstr->iP1 = 0;` |
|  1024481 | 13553 | `			}` |
|  2049039 | 13554 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|        - | 13555 | `				/* Static member access,remember that */` |
|    31719 | 13556 | `				iP1 = 1;` |
|    31719 | 13557 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    31719 | 13558 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       62 | 13559 | `					p3 = pInstr->p3;` |
|       62 | 13560 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       29 | 13561 | `				}` |
|    15857 | 13562 | `			}` |
|        - | 13563 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|        - | 13564 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|        - | 13565 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|        - | 13566 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  2049039 | 13567 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  2049039 | 13568 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       40 | 13569 | `					iP2 = PH7_MEMBER_UNSET;` |
|  2049020 | 13570 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       97 | 13571 | `					iP2 = PH7_MEMBER_ISSET;` |
|  2048955 | 13572 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       17 | 13573 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  2048901 | 13574 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|        - | 13575 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   249445 | 13576 | `					iP2 = PH7_MEMBER_WRITE;` |
|   124720 | 13577 | `				}` |
|  1024517 | 13578 | `			}` |
|  1024517 | 13579 | `		}` |
|        - | 13580 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|        - | 13581 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|        - | 13582 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|        - | 13583 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|        - | 13584 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 12747161 | 13585 | `		if( bFcc ){` |
|       81 | 13586 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|       81 | 13587 | `			iP2 = 0;` |
|       81 | 13588 | `			p3 = 0;` |
|       81 | 13589 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       81 | 13590 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 13591 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|        - | 13592 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|        - | 13593 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|        - | 13594 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|       37 | 13595 | `				void *pMemberName = pInstr->p3;` |
|       37 | 13596 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|       37 | 13597 | `				if( pMemberName ){` |
|        3 | 13598 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|        1 | 13599 | `				}` |
|       37 | 13600 | `				iP1 = 2;` |
|       19 | 13601 | `			}else{` |
|       45 | 13602 | `				iP1 = 1;` |
|        - | 13603 | `			}` |
|       40 | 13604 | `		}` |
|        - | 13605 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|        - | 13606 | `		 * This is the primary emit path for user-visible calls. */` |
| 12747161 | 13607 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  3777947 | 13608 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  1888971 | 13609 | `		}` |
|        - | 13610 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 12747161 | 13611 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  6373578 | 13612 | `	}` |
| 12747445 | 13613 | `	if( nJmpIdx > 0 ){` |
|        - | 13614 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   257193 | 13615 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   257193 | 13616 | `		if( pInstr ){` |
|   257193 | 13617 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   128594 | 13618 | `		}` |
|   128594 | 13619 | `	}` |
| 12747445 | 13620 | `	return rc;` |
| 16221452 | 13621 | `}` |
|        - | 13622 | `/*` |
|        - | 13623 | ` * Compile a PHP expression.` |
|        - | 13624 | ` * According to the PHP language reference manual:` |
|        - | 13625 | ` *  Expressions are the most important building stones of PHP.` |
|        - | 13626 | ` *  In PHP, almost anything you write is an expression.` |
|        - | 13627 | ` *  The simplest yet most accurate way to define an expression` |
|        - | 13628 | ` *  is "anything that has a value".` |
|        - | 13629 | ` * If something goes wrong while compiling the expression,this` |
|        - | 13630 | ` * function takes care of generating the appropriate error` |
|        - | 13631 | ` * message.` |
|        - | 13632 | ` */` |
|  7283168 | 13633 | `static sxi32 PH7_CompileExpr(` |
|        - | 13634 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 13635 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 13636 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|        - | 13637 | `	)` |
|        5 | 13638 | `{` |
|        - | 13639 | `	ph7_expr_node *pRoot;` |
|        - | 13640 | `	SySet sExprNode;` |
|        - | 13641 | `	SyToken *pEnd;` |
|        - | 13642 | `	sxi32 nExpr;` |
|        - | 13643 | `	sxi32 iNest;` |
|        - | 13644 | `	sxi32 rc;` |
|        - | 13645 | `	sxu32 nNullsafeBase;` |
|        - | 13646 | `	/* Initialize worker variables */` |
|  7283173 | 13647 | `	nExpr = 0;` |
|  7283173 | 13648 | `	pRoot = 0;` |
|        - | 13649 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|        - | 13650 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  7283173 | 13651 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  7283173 | 13652 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  7283173 | 13653 | `	SySetAlloc(&sExprNode,0x10);` |
|  7283173 | 13654 | `	rc = SXRET_OK;` |
|        - | 13655 | `	/* Delimit the expression */` |
|  7283173 | 13656 | `	pEnd = pGen->pIn;` |
|  7283173 | 13657 | `	iNest = 0;` |
| 57215561 | 13658 | `	while( pEnd < pGen->pEnd ){` |
| 54550903 | 13659 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 13660 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      701 | 13661 | `			iNest++;` |
| 54550555 | 13662 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      709 | 13663 | `			iNest--;` |
| 54549855 | 13664 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  4619101 | 13665 | `			if( iNest <= 0 ){` |
|  4618515 | 13666 | `				break;` |
|        - | 13667 | `			}` |
|      293 | 13668 | `		}` |
| 49932393 | 13669 | `		pEnd++;` |
|        5 | 13670 | `	}` |
|  7283173 | 13671 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   245689 | 13672 | `		SyToken *pEnd2 = pGen->pIn;` |
|   245689 | 13673 | `		iNest = 0;` |
|        - | 13674 | `		/* Stop at the first comma */` |
|   569779 | 13675 | `		while( pEnd2 < pEnd ){` |
|   324097 | 13676 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     7875 | 13677 | `				iNest++;` |
|   320162 | 13678 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     7875 | 13679 | `				iNest--;` |
|   312292 | 13680 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       63 | 13681 | `				if( iNest <= 0 ){` |
|        3 | 13682 | `					break;` |
|        - | 13683 | `				}` |
|       28 | 13684 | `			}` |
|   324095 | 13685 | `			pEnd2++;` |
|        5 | 13686 | `		}` |
|   245689 | 13687 | `		if( pEnd2 <pEnd ){` |
|        3 | 13688 | `			pEnd = pEnd2;` |
|        1 | 13689 | `		}` |
|   122842 | 13690 | `	}` |
|  7283173 | 13691 | `	if( pEnd > pGen->pIn ){` |
|  7283163 | 13692 | `		SyToken *pTmp = pGen->pEnd;` |
|        - | 13693 | `		/* Swap delimiter */` |
|  7283163 | 13694 | `		pGen->pEnd = pEnd;` |
|        - | 13695 | `		/* Try to get an expression tree */` |
|  7283163 | 13696 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  7283163 | 13697 | `		if( rc == SXRET_OK && pRoot ){` |
|  7282981 | 13698 | `			rc = SXRET_OK;` |
|  7282981 | 13699 | `			if( xTreeValidator ){` |
|        - | 13700 | `				/* Call the upper layer validator callback */` |
|   563743 | 13701 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   281869 | 13702 | `			}` |
|  7282981 | 13703 | `			if( rc != SXERR_ABORT ){` |
|        - | 13704 | `				/* Generate code for the given tree */` |
|  7282981 | 13705 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|        - | 13706 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|        - | 13707 | `				 * expression so they short-circuit to its end. */` |
|  7282981 | 13708 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  3641488 | 13709 | `			}` |
|  7282981 | 13710 | `			nExpr = 1;` |
|  3641488 | 13711 | `		}` |
|        - | 13712 | `		/* Release the whole tree */` |
|  7283163 | 13713 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|        - | 13714 | `		/* Synchronize token stream */` |
|  7283163 | 13715 | `		pGen->pEnd = pTmp;` |
|  7283163 | 13716 | `		pGen->pIn  = pEnd;` |
|  7283163 | 13717 | `		if( rc == SXERR_ABORT ){` |
|       13 | 13718 | `			SySetRelease(&sExprNode);` |
|       13 | 13719 | `			return SXERR_ABORT;` |
|        - | 13720 | `		}` |
|  3641574 | 13721 | `	}` |
|  7283163 | 13722 | `	SySetRelease(&sExprNode);` |
|  7283163 | 13723 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  3641589 | 13724 | `}` |
|        - | 13725 | `/*` |
|        - | 13726 | ` * Return a pointer to the node construct handler associated` |
|        - | 13727 | ` * with a given node type [i.e: string,integer,float,...].` |
|        - | 13728 | ` */` |
|  4434802 | 13729 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|        5 | 13730 | `{` |
|  4434807 | 13731 | `	if( nNodeType & PH7_TK_NUM ){` |
|        - | 13732 | `		/* Numeric literal: Either real or integer */` |
|  1297095 | 13733 | `		return PH7_CompileNumLiteral;` |
|  3137717 | 13734 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|        - | 13735 | `		/* Double quoted string */` |
|    37327 | 13736 | `		return PH7_CompileString;` |
|  3100395 | 13737 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|        - | 13738 | `		/* Single quoted string */` |
|  3100275 | 13739 | `		return PH7_CompileSimpleString;` |
|      125 | 13740 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|        - | 13741 | `		/* Heredoc */` |
|       71 | 13742 | `		return PH7_CompileHereDoc;` |
|       58 | 13743 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|        - | 13744 | `		/* Nowdoc */` |
|       51 | 13745 | `		return PH7_CompileNowDoc;` |
|        9 | 13746 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|        - | 13747 | `		/* Backtick quoted string */` |
|        6 | 13748 | `		return PH7_CompileBacktic;` |
|        - | 13749 | `	}` |
|        3 | 13750 | `	return 0;` |
|  2217406 | 13751 | `}` |
|        - | 13752 | `/*` |
|        - | 13753 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|        - | 13754 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|        - | 13755 | ` * in write context" parse error.` |
|        - | 13756 | ` */` |
|     6856 | 13757 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|        5 | 13758 | `{` |
|        - | 13759 | `	sxi32 rc;` |
|     6861 | 13760 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     6859 | 13761 | `		return SXRET_OK;` |
|        - | 13762 | `	}` |
|        5 | 13763 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|        2 | 13764 | `		pNode ? pNode->pStart->nLine : 1,` |
|        - | 13765 | `		"Can't use nullsafe operator in write context");` |
|        3 | 13766 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     3433 | 13767 | `}` |
|        - | 13768 | `/*` |
|        - | 13769 | ` * Compile an unset() statement.` |
|        - | 13770 | ` * unset($var, $arr[$key], ...);` |
|        - | 13771 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|        - | 13772 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|        - | 13773 | ` * parent array before extracting the element to unset.` |
|        - | 13774 | ` */` |
|     2934 | 13775 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|        5 | 13776 | `{` |
|     2939 | 13777 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     2939 | 13778 | `	sxu32 nIdx = 0;` |
|        - | 13779 | `	SyString sName;` |
|        - | 13780 | `	sxi32 rc;` |
|        - | 13781 | `	/* Jump the 'unset' keyword */` |
|     2939 | 13782 | `	pGen->pIn++;` |
|        - | 13783 | `	/* Save delimiter */` |
|     2939 | 13784 | `	pTmp = pGen->pEnd;` |
|        - | 13785 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     2939 | 13786 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     2939 | 13787 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 13788 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|        - | 13789 | `		SyToken *pClose;` |
|     2939 | 13790 | `		pGen->pIn++;   /* Skip '(' */` |
|     2939 | 13791 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     2939 | 13792 | `		pEnd = pClose; /* Stop at ')' */` |
|     1467 | 13793 | `	}` |
|     2939 | 13794 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|        - | 13795 | `	/* Resolve the 'unset' builtin name once */` |
|     2939 | 13796 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      379 | 13797 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      379 | 13798 | `		if( pObj == 0 ){` |
|      ! 0 | 13799 | `			return SXERR_ABORT;` |
|        - | 13800 | `		}` |
|      379 | 13801 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      379 | 13802 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      187 | 13803 | `	}` |
|        - | 13804 | `	/* Compile each comma-separated argument */` |
|     9797 | 13805 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     6863 | 13806 | `		if( pGen->pIn < pNext ){` |
|     6863 | 13807 | `			pGen->pEnd = pNext;` |
|     6863 | 13808 | `			rc = PH7_CompileExpr(&(*pGen),` |
|        - | 13809 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|        - | 13810 | `				GenStateUnsetValidator);` |
|     6863 | 13811 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13812 | `				return SXERR_ABORT;` |
|        - | 13813 | `			}` |
|     6863 | 13814 | `			if( rc != SXERR_EMPTY ){` |
|        - | 13815 | `				/* Emit call for this single argument */` |
|     6861 | 13816 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     6861 | 13817 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     6861 | 13818 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     3428 | 13819 | `			}` |
|     3429 | 13820 | `		}` |
|        - | 13821 | `		/* Jump trailing commas */` |
|    10789 | 13822 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|     3931 | 13823 | `			pNext++;` |
|        5 | 13824 | `		}` |
|     6863 | 13825 | `		pGen->pIn = pNext;` |
|        5 | 13826 | `	}` |
|        - | 13827 | `	/* Skip past the closing ')' if present */` |
|     2939 | 13828 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     2939 | 13829 | `		pGen->pIn++;` |
|     1467 | 13830 | `	}` |
|        - | 13831 | `	/* Restore token stream */` |
|     2939 | 13832 | `	pGen->pEnd = pTmp;` |
|     2939 | 13833 | `	return SXRET_OK;` |
|     1472 | 13834 | `}` |
|        - | 13835 | `/*` |
|        - | 13836 | ` * PHP Language construct table.` |
|        - | 13837 | ` */` |
|        - | 13838 | `static const LangConstruct aLangConstruct[] = {` |
|        - | 13839 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|        - | 13840 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|        - | 13841 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|        - | 13842 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|        - | 13843 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|        - | 13844 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|        - | 13845 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|        - | 13846 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|        - | 13847 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|        - | 13848 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|        - | 13849 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|        - | 13850 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|        - | 13851 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|        - | 13852 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|        - | 13853 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|        - | 13854 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|        - | 13855 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|        - | 13856 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|        - | 13857 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|        - | 13858 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|        - | 13859 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|        - | 13860 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|        - | 13861 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|        - | 13862 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|        - | 13863 | `};` |
|        - | 13864 | `/*` |
|        - | 13865 | ` * Return a pointer to the statement handler routine associated` |
|        - | 13866 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|        - | 13867 | ` */` |
|  3842394 | 13868 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|        - | 13869 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|        - | 13870 | `	SyToken *pLookahed  /* Look-ahead token */` |
|        - | 13871 | `	)` |
|        5 | 13872 | `{` |
|  3842399 | 13873 | `	sxu32 n = 0;` |
| 15601316 | 13874 | `	for(;;){` |
| 31202637 | 13875 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   246961 | 13876 | `			break;` |
|        - | 13877 | `		}` |
| 30955681 | 13878 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  3595443 | 13879 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|      ! 0 | 13880 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|      ! 0 | 13881 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|        - | 13882 | `					/* 'static' (class context),return null */` |
|      ! 0 | 13883 | `					return 0;` |
|        - | 13884 | `				}` |
|      ! 0 | 13885 | `			}` |
|  3595438 | 13886 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       14 | 13887 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       14 | 13888 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|        - | 13889 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|        3 | 13890 | `				return 0;` |
|        - | 13891 | `			}` |
|        - | 13892 | `			/* Return a pointer to the handler.` |
|        - | 13893 | `			*/` |
|  3595441 | 13894 | `			return aLangConstruct[n].xConstruct;` |
|        - | 13895 | `		}` |
| 27360243 | 13896 | `		n++;` |
|        5 | 13897 | `	}` |
|   246961 | 13898 | `	if( pLookahed ){` |
|   246961 | 13899 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    46717 | 13900 | `			return PH7_CompileClassInterface;` |
|   200249 | 13901 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   188041 | 13902 | `			return PH7_CompileClass;` |
|    12213 | 13903 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|       77 | 13904 | `			return PH7_CompileTrait;` |
|        - | 13905 | `		}` |
|        - | 13906 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|        - | 13907 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|        - | 13908 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|        - | 13909 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     6068 | 13910 | `	}` |
|        - | 13911 | `	/* Not a language construct */` |
|    12141 | 13912 | `	return 0;` |
|  1921202 | 13913 | `}` |
|        - | 13914 | `/*` |
|        - | 13915 | ` * Check if the given keyword is in fact a PHP language construct.` |
|        - | 13916 | ` * Return TRUE on success. FALSE otheriwse.` |
|        - | 13917 | ` */` |
|    12138 | 13918 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|        5 | 13919 | `{` |
|        - | 13920 | `	int rc;` |
|    12143 | 13921 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    12143 | 13922 | `	if( rc == FALSE ){` |
|    12024 | 13923 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      366 | 13924 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|        - | 13925 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|        - | 13926 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|        - | 13927 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|        - | 13928 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|        - | 13929 | `			*/` |
|        - | 13930 | `			){` |
|    12021 | 13931 | `				rc = TRUE;` |
|     6008 | 13932 | `		}` |
|     6012 | 13933 | `	}` |
|    12143 | 13934 | `	return rc;` |
|        5 | 13935 | `}` |
|        - | 13936 | `/*` |
|        - | 13937 | ` * Compile a PHP chunk.` |
|        - | 13938 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13939 | ` * takes care of generating the appropriate error message.` |
|        - | 13940 | ` */` |
|        - | 13941 | `/*` |
|        - | 13942 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|        - | 13943 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|        - | 13944 | ` * the chunk token set it becomes the pending docblock. An existing` |
|        - | 13945 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|        - | 13946 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|        - | 13947 | ` * intervening non-declaration statements.` |
|        - | 13948 | ` */` |
|  8060334 | 13949 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|        5 | 13950 | `{` |
|  8060339 | 13951 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  8060339 | 13952 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  8060339 | 13953 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13954 | `	sxu32 nIdx, n;` |
|  8060334 | 13955 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|  1537013 | 13956 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|        - | 13957 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|        - | 13958 | `		 * indexes do not map to the sidecar */` |
|  6523333 | 13959 | `		return;` |
|        - | 13960 | `	}` |
|  1537011 | 13961 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|        - | 13962 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|        - | 13963 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|  1537011 | 13964 | `	SySetReset(&pGen->aPendingAttrs);` |
|  4612517 | 13965 | `	for( n = 0 ; n < nT ; n++ ){` |
|  3075511 | 13966 | `		if( aT[n].nTokIdx != nIdx ){` |
|  3067579 | 13967 | `			continue;` |
|        - | 13968 | `		}` |
|     7937 | 13969 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       29 | 13970 | `			pGen->sPendingDoc = aT[n].sText;` |
|     7925 | 13971 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     7913 | 13972 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|     3954 | 13973 | `		}` |
|     3971 | 13974 | `	}` |
|  4030172 | 13975 | `}` |
|        - | 13976 | `/*` |
|        - | 13977 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|        - | 13978 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|        - | 13979 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|        - | 13980 | ` */` |
|  2146622 | 13981 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|        5 | 13982 | `{` |
|        - | 13983 | `	char *zDup;` |
|  2146627 | 13984 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|  2146607 | 13985 | `		return;` |
|        - | 13986 | `	}` |
|       35 | 13987 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       10 | 13988 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       25 | 13989 | `	if( zDup ){` |
|       25 | 13990 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       10 | 13991 | `	}` |
|       25 | 13992 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|  1073316 | 13993 | `}` |
|        - | 13994 | `/*` |
|        - | 13995 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|        - | 13996 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|        - | 13997 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|        - | 13998 | ` * names may point into the token text, which must outlive the raw script` |
|        - | 13999 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|        - | 14000 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|        - | 14001 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|        - | 14002 | ` */` |
|     7920 | 14003 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|        5 | 14004 | `{` |
|        - | 14005 | `	SySet *pToken;` |
|        - | 14006 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|        - | 14007 | `	char *zSpan;` |
|     7925 | 14008 | `	sxi32 rc = SXRET_OK;` |
|     7925 | 14009 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|      ! 0 | 14010 | `		return SXRET_OK;` |
|        - | 14011 | `	}` |
|    11885 | 14012 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3960 | 14013 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|     7925 | 14014 | `	if( zSpan == 0 ){` |
|      ! 0 | 14015 | `		return SXRET_OK;` |
|        - | 14016 | `	}` |
|        - | 14017 | `	/* The token set must outlive compilation too: interned operands may` |
|        - | 14018 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|        - | 14019 | `	 * the number of attribute declarations in the program. */` |
|     7925 | 14020 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|     7925 | 14021 | `	if( pToken == 0 ){` |
|      ! 0 | 14022 | `		return SXRET_OK;` |
|        - | 14023 | `	}` |
|     7925 | 14024 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     7925 | 14025 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|     7925 | 14026 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|     7925 | 14027 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|     7925 | 14028 | `	pSavedIn = pGen->pIn;` |
|     7925 | 14029 | `	pSavedEnd = pGen->pEnd;` |
|     7929 | 14030 | `	while( pIn < pEnd ){` |
|        - | 14031 | `		ph7_attribute sAttr;` |
|        - | 14032 | `		SyBlob sFQN;` |
|     7929 | 14033 | `		int bAbsolute = 0;` |
|     7929 | 14034 | `		SyZero(&sAttr,sizeof(sAttr));` |
|     7929 | 14035 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|     7929 | 14036 | `		sAttr.nLine = pIn->nLine;` |
|     7929 | 14037 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       75 | 14038 | `			bAbsolute = 1;` |
|       75 | 14039 | `			pIn++;` |
|       35 | 14040 | `		}` |
|     7929 | 14041 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7929 | 14042 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     7929 | 14043 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|     7929 | 14044 | `			pIn++;` |
|     7929 | 14045 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      ! 0 | 14046 | `				SyBlobAppend(&sFQN,"\\",1);` |
|      ! 0 | 14047 | `				pIn++;` |
|      ! 0 | 14048 | `				continue;` |
|        - | 14049 | `			}` |
|     7929 | 14050 | `			break;` |
|      ! 0 | 14051 | `		}` |
|     7929 | 14052 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|        - | 14053 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|        - | 14054 | `			 * this feature; never turn it into a new fatal) */` |
|      ! 0 | 14055 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 14056 | `			break;` |
|        - | 14057 | `		}` |
|        - | 14058 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|        - | 14059 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|        - | 14060 | `		{` |
|     7929 | 14061 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|     7929 | 14062 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|     7929 | 14063 | `			char *zDup = 0;` |
|     7929 | 14064 | `			if( !bAbsolute ){` |
|     7859 | 14065 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|     7859 | 14066 | `				if( pImp ){` |
|      ! 0 | 14067 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|      ! 0 | 14068 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|      ! 0 | 14069 | `					if( zDup ){` |
|      ! 0 | 14070 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|      ! 0 | 14071 | `					}` |
|     7859 | 14072 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 14073 | `					SyBlob sTmp;` |
|      ! 0 | 14074 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|      ! 0 | 14075 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      ! 0 | 14076 | `					SyBlobAppend(&sTmp,"\\",1);` |
|      ! 0 | 14077 | `					SyBlobAppend(&sTmp,zName,nName);` |
|      ! 0 | 14078 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      ! 0 | 14079 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|      ! 0 | 14080 | `					if( zDup ){` |
|      ! 0 | 14081 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|      ! 0 | 14082 | `					}` |
|      ! 0 | 14083 | `					SyBlobRelease(&sTmp);` |
|      ! 0 | 14084 | `				}` |
|     3927 | 14085 | `			}` |
|     7929 | 14086 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|     7929 | 14087 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|     7929 | 14088 | `				if( zDup ){` |
|     7929 | 14089 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|     3962 | 14090 | `				}` |
|     3962 | 14091 | `			}` |
|        - | 14092 | `		}` |
|     7929 | 14093 | `		SyBlobRelease(&sFQN);` |
|     7929 | 14094 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 14095 | `			SyToken *pArgsEnd;` |
|     7827 | 14096 | `			pIn++;` |
|     7827 | 14097 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|    15663 | 14098 | `			while( pIn < pArgsEnd ){` |
|     7841 | 14099 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|     7841 | 14100 | `				sxi32 iDepth = 0;` |
|        - | 14101 | `				ph7_attr_arg sArgRec;` |
|    77925 | 14102 | `				while( pArgStop < pArgsEnd ){` |
|    70105 | 14103 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       11 | 14104 | `						iDepth++;` |
|    70100 | 14105 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       11 | 14106 | `						iDepth--;` |
|    70090 | 14107 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|       17 | 14108 | `						break;` |
|        - | 14109 | `					}` |
|    70089 | 14110 | `					pArgStop++;` |
|        5 | 14111 | `				}` |
|     7841 | 14112 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|     7841 | 14113 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     7836 | 14114 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|     7820 | 14115 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       28 | 14116 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        9 | 14117 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|       19 | 14118 | `					if( zN ){` |
|       19 | 14119 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|        9 | 14120 | `					}` |
|       19 | 14121 | `					pArgStart += 2;` |
|        9 | 14122 | `				}` |
|     7841 | 14123 | `				if( pArgStart < pArgStop ){` |
|        - | 14124 | `					SySet *pInstrContainer;` |
|     7841 | 14125 | `					pGen->pIn = pArgStart;` |
|     7841 | 14126 | `					pGen->pEnd = pArgStop;` |
|     7841 | 14127 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7841 | 14128 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|     7841 | 14129 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7841 | 14130 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7841 | 14131 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7841 | 14132 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 14133 | `						pGen->pIn = pSavedIn;` |
|      ! 0 | 14134 | `						pGen->pEnd = pSavedEnd;` |
|      ! 0 | 14135 | `						return SXERR_ABORT;` |
|        - | 14136 | `					}` |
|     7841 | 14137 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|     3918 | 14138 | `				}` |
|     7841 | 14139 | `				pIn = pArgStop;` |
|     7841 | 14140 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 | 14141 | `					pIn++;` |
|        8 | 14142 | `				}` |
|        5 | 14143 | `			}` |
|     7827 | 14144 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|     3911 | 14145 | `		}` |
|     7929 | 14146 | `		SySetPut(pOut,(const void *)&sAttr);` |
|     7929 | 14147 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        5 | 14148 | `			pIn++;` |
|        5 | 14149 | `			continue;` |
|        - | 14150 | `		}` |
|     7925 | 14151 | `		break;` |
|      ! 0 | 14152 | `	}` |
|     7925 | 14153 | `	pGen->pIn = pSavedIn;` |
|     7925 | 14154 | `	pGen->pEnd = pSavedEnd;` |
|     7925 | 14155 | `	return SXRET_OK;` |
|     3965 | 14156 | `}` |
|        - | 14157 | `/*` |
|        - | 14158 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|        - | 14159 | ` * every recorded group into pOut and clear the pending list.` |
|        - | 14160 | ` */` |
|  2146626 | 14161 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|        5 | 14162 | `{` |
|  2146631 | 14163 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        - | 14164 | `	sxu32 n;` |
|        - | 14165 | `	sxi32 rc;` |
|  2154539 | 14166 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|     7913 | 14167 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|     7913 | 14168 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 14169 | `			return SXERR_ABORT;` |
|        - | 14170 | `		}` |
|     3959 | 14171 | `	}` |
|  2146631 | 14172 | `	SySetReset(&pGen->aPendingAttrs);` |
|  2146631 | 14173 | `	return SXRET_OK;` |
|  1073318 | 14174 | `}` |
|        - | 14175 | `/*` |
|        - | 14176 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|        - | 14177 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|        - | 14178 | ` * the main token stream, so the sidecar indexes map directly.` |
|        - | 14179 | ` */` |
|   725986 | 14180 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|        5 | 14181 | `{` |
|   725991 | 14182 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   725991 | 14183 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   725991 | 14184 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 14185 | `	sxu32 nIdx, n;` |
|        - | 14186 | `	sxi32 rc;` |
|   725986 | 14187 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   194535 | 14188 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   531461 | 14189 | `		return SXRET_OK;` |
|        - | 14190 | `	}` |
|   194535 | 14191 | `	nIdx = (sxu32)(pTok - pBase);` |
|   583593 | 14192 | `	for( n = 0 ; n < nT ; n++ ){` |
|   389063 | 14193 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       13 | 14194 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|       13 | 14195 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 14196 | `				return SXERR_ABORT;` |
|        - | 14197 | `			}` |
|        6 | 14198 | `		}` |
|   194534 | 14199 | `	}` |
|   194535 | 14200 | `	return SXRET_OK;` |
|   362998 | 14201 | `}` |
|  5942804 | 14202 | `static sxi32 GenStateCompileChunk(` |
|        - | 14203 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 14204 | `	sxi32 iFlags         /* Compile flags */` |
|        - | 14205 | `	)` |
|        5 | 14206 | `{` |
|        - | 14207 | `	ProcLangConstruct xCons;` |
|        - | 14208 | `	sxi32 rc;` |
|  5942809 | 14209 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  3391744 | 14210 | `	for(;;){` |
|  6363151 | 14211 | `		int bStmtIsDeclare = 0;` |
|  6363151 | 14212 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 14213 | `			/* No more input to process */` |
|    53365 | 14214 | `			break;` |
|        - | 14215 | `		}` |
|        - | 14216 | `		/* Bind a directly-preceding docblock to this statement */` |
|  6309791 | 14217 | `		GenStateSetPendingDoc(&(*pGen));` |
|  6309791 | 14218 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - | 14219 | `			/* php: a statement-position attribute group must be followed by a` |
|        - | 14220 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|        - | 14221 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|        - | 14222 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|        - | 14223 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|     7831 | 14224 | `			int bAttrTarget = 0;` |
|     7826 | 14225 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|     3947 | 14226 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|     7773 | 14227 | `				bAttrTarget = 1;` |
|     3943 | 14228 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       59 | 14229 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       58 | 14230 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|       15 | 14231 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|        4 | 14232 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|        4 | 14233 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|        1 | 14234 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|       59 | 14235 | `					bAttrTarget = 1;` |
|       29 | 14236 | `				}` |
|       29 | 14237 | `			}` |
|     7831 | 14238 | `			if( !bAttrTarget ){` |
|      ! 0 | 14239 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 14240 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|      ! 0 | 14241 | `					&pGen->pIn->sData);` |
|      ! 0 | 14242 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 14243 | `					break;` |
|        - | 14244 | `				}` |
|      ! 0 | 14245 | `				SySetReset(&pGen->aPendingAttrs);` |
|      ! 0 | 14246 | `			}` |
|     3913 | 14247 | `		}` |
|        - | 14248 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|        - | 14249 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  6309791 | 14250 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3869637 | 14251 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  3869637 | 14252 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       47 | 14253 | `				bStmtIsDeclare = 1;` |
|       21 | 14254 | `			}` |
|  1934816 | 14255 | `		}` |
|  6309791 | 14256 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|        - | 14257 | `			/* Any non-declare top-level statement locks the strict_types` |
|        - | 14258 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   420315 | 14259 | `			pGen->bStrictTypesLocked = 1;` |
|   210155 | 14260 | `		}` |
|  6309791 | 14261 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 14262 | `			/* Compile block */` |
|     3907 | 14263 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|     3907 | 14264 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 14265 | `				break;` |
|        - | 14266 | `			}` |
|     1956 | 14267 | `		}else{` |
|  6305889 | 14268 | `			xCons = 0;` |
|  6305889 | 14269 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|        - | 14270 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|        - | 14271 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|        - | 14272 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    27269 | 14273 | `				xCons = PH7_CompileClassModifiers;` |
|  6292257 | 14274 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        - | 14275 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|        - | 14276 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|     3917 | 14277 | `				xCons = PH7_CompileEnum;` |
|  6276669 | 14278 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3842399 | 14279 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - | 14280 | `				/* Try to extract a language construct handler */` |
|  3842399 | 14281 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  3842399 | 14282 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|       13 | 14283 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 14284 | `						"Syntax error: Unexpected keyword '%z'",` |
|        8 | 14285 | `						&pGen->pIn->sData);` |
|        9 | 14286 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 14287 | `						break;` |
|        - | 14288 | `					}` |
|        - | 14289 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|        - | 14290 | `					 * this erroneous statement.` |
|        - | 14291 | `					 */` |
|        9 | 14292 | `					xCons = PH7_ErrorRecover;` |
|        4 | 14293 | `				}` |
|  4353516 | 14294 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    66527 | 14295 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|        - | 14296 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|      117 | 14297 | `				xCons = PH7_CompileLabel;` |
|       56 | 14298 | `			}` |
|  6305889 | 14299 | `			if( xCons == 0 ){` |
|        - | 14300 | `				/* Assume an expression an try to compile it */` |
|  2444337 | 14301 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  2444337 | 14302 | `				if(  rc != SXERR_EMPTY ){` |
|        - | 14303 | `					/* Pop l-value */` |
|  2444187 | 14304 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  1222091 | 14305 | `				}` |
|  1222171 | 14306 | `			}else{` |
|        - | 14307 | `				/* Go compile the sucker */` |
|  3861557 | 14308 | `				rc = xCons(&(*pGen));` |
|        - | 14309 | `			}` |
|  6305889 | 14310 | `			if( rc == SXERR_ABORT ){` |
|        - | 14311 | `				/* Request to abort compilation */` |
|       13 | 14312 | `				break;` |
|        - | 14313 | `			}` |
|        - | 14314 | `		}` |
|        - | 14315 | `		/* Ignore trailing semi-colons ';' */` |
| 10793261 | 14316 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  4483485 | 14317 | `			pGen->pIn++;` |
|        5 | 14318 | `		}` |
|  6309781 | 14319 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|        - | 14320 | `			/* Compile a single statement and return */` |
|  5889439 | 14321 | `			break;` |
|        - | 14322 | `		}` |
|        - | 14323 | `		/* LOOP ONE */` |
|        - | 14324 | `		/* LOOP TWO */` |
|        - | 14325 | `		/* LOOP THREE */` |
|        - | 14326 | `		/* LOOP FOUR */` |
|        5 | 14327 | `	}` |
|        - | 14328 | `	/* Return compilation status */` |
|  5942809 | 14329 | `	return rc;` |
|        5 | 14330 | `}` |
|        - | 14331 | `/*` |
|        - | 14332 | ` * Compile a Raw PHP chunk.` |
|        - | 14333 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 14334 | ` * takes care of generating the appropriate error message.` |
|        - | 14335 | ` */` |
|    53372 | 14336 | `static sxi32 PH7_CompilePHP(` |
|        - | 14337 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 14338 | `	SySet *pTokenSet,     /* Token set */` |
|        - | 14339 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|        - | 14340 | `	)` |
|        5 | 14341 | `{` |
|    53377 | 14342 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|        - | 14343 | `	sxi32 rc;` |
|        - | 14344 | `	/* Reset the token set (and its trivia sidecar) */` |
|    53377 | 14345 | `	SySetReset(&(*pTokenSet));` |
|    53377 | 14346 | `	SySetReset(&pGen->aTrivia);` |
|        - | 14347 | `	/* Mark as the default token set */` |
|    53377 | 14348 | `	pGen->pTokenSet = &(*pTokenSet);` |
|        - | 14349 | `	/* Advance the stream cursor */` |
|    53377 | 14350 | `	pGen->pRawIn++;` |
|        - | 14351 | `	/* Tokenize the PHP chunk first */` |
|    53377 | 14352 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|        - | 14353 | `	/* Point to the head and tail of the token stream. */` |
|    53377 | 14354 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    53377 | 14355 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    53377 | 14356 | `	if( is_expr ){` |
|      ! 0 | 14357 | `		rc = SXERR_EMPTY;` |
|      ! 0 | 14358 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 14359 | `			/* A simple expression,compile it */` |
|      ! 0 | 14360 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 | 14361 | `		}` |
|        - | 14362 | `		/* Emit the DONE instruction */` |
|      ! 0 | 14363 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      ! 0 | 14364 | `		return SXRET_OK;` |
|        - | 14365 | `	}` |
|    53377 | 14366 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - | 14367 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - | 14368 | `		/*` |
|        - | 14369 | `		 * Shortcut syntax for the 'echo' language construct.` |
|        - | 14370 | `		 * According to the PHP reference manual:` |
|        - | 14371 | `		 *  echo() also has a shortcut syntax, where you can` |
|        - | 14372 | `		 *  immediately follow` |
|        - | 14373 | `		 *  the opening tag with an equals sign as follows:` |
|        - | 14374 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|        - | 14375 | `		 * Symisc extension:` |
|        - | 14376 | `		 *   This short syntax works with all PHP opening` |
|        - | 14377 | `		 *   tags unlike the default PHP engine that handle` |
|        - | 14378 | `		 *   only short tag.` |
|        - | 14379 | `		 */` |
|        - | 14380 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|        3 | 14381 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        3 | 14382 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        3 | 14383 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        3 | 14384 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|        3 | 14385 | `		if( rc != SXERR_EMPTY ){` |
|        3 | 14386 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 14387 | `		}` |
|        3 | 14388 | `		return SXRET_OK;` |
|        - | 14389 | `	}` |
|        - | 14390 | `	/* Compile the PHP chunk */` |
|    53375 | 14391 | `	rc = GenStateCompileChunk(pGen,0);` |
|        - | 14392 | `	/* Fix exceptions jumps */` |
|    53375 | 14393 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - | 14394 | `	/* Fix gotos now, the jump destination is resolved */` |
|    53375 | 14395 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|        3 | 14396 | `		rc = SXERR_ABORT;` |
|        1 | 14397 | `	}` |
|        - | 14398 | `	/* Reset container */` |
|    53375 | 14399 | `	SySetReset(&pGen->aGoto);` |
|    53375 | 14400 | `	SySetReset(&pGen->aLabel);` |
|    53375 | 14401 | `	SySetReset(&pGen->aNullsafeJmp);` |
|        - | 14402 | `	/* Compilation result */` |
|    53375 | 14403 | `	return rc;` |
|    26691 | 14404 | `}` |
|        - | 14405 | `/*` |
|        - | 14406 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|        - | 14407 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|        - | 14408 | ` * This is the only compile interface exported from this file.` |
|        - | 14409 | ` */` |
|    56432 | 14410 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|        - | 14411 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|        - | 14412 | `	SyString *pScript,  /* Script to compile */` |
|        - | 14413 | `	sxi32 iFlags        /* Compile flags */` |
|        - | 14414 | `	)` |
|        5 | 14415 | `{` |
|        - | 14416 | `	SySet aPhpToken,aRawToken;` |
|        - | 14417 | `	ph7_gen_state *pCodeGen;` |
|        - | 14418 | `	ph7_value *pRawObj;` |
|        - | 14419 | `	sxu32 nObjIdx;` |
|        - | 14420 | `	sxi32 nRawObj;` |
|        - | 14421 | `	int is_expr;` |
|        - | 14422 | `	sxi8 bSavedStrict;` |
|        - | 14423 | `	sxi8 bSavedStrictLocked;` |
|        - | 14424 | `	sxi32 rc;` |
|    56437 | 14425 | `	if( pScript->nByte < 1 ){` |
|        - | 14426 | `		/* Nothing to compile */` |
|      ! 0 | 14427 | `		return PH7_OK;` |
|        - | 14428 | `	}` |
|        - | 14429 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|        - | 14430 | `	 * file's flags so include/require restore them on return. */` |
|    56437 | 14431 | `	pCodeGen = &pVm->sCodeGen;` |
|    56437 | 14432 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    56437 | 14433 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    56437 | 14434 | `	pCodeGen->bStrictTypes = 0;` |
|    56437 | 14435 | `	pCodeGen->bStrictTypesLocked = 0;` |
|        - | 14436 | `	/* Initialize the tokens containers */` |
|    56437 | 14437 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    56437 | 14438 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    56437 | 14439 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    56437 | 14440 | `	is_expr = 0;` |
|    56437 | 14441 | `	if( iFlags & PH7_PHP_ONLY ){` |
|        - | 14442 | `		SyToken sTmp;` |
|        - | 14443 | `		/* PHP only: -*/` |
|    42827 | 14444 | `		sTmp.nLine = 1;` |
|    42827 | 14445 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    42827 | 14446 | `		sTmp.pUserData = 0;` |
|    42827 | 14447 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    42827 | 14448 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    42827 | 14449 | `		if( iFlags & PH7_PHP_EXPR ){` |
|        - | 14450 | `			/* A simple PHP expression */` |
|      ! 0 | 14451 | `			is_expr = 1;` |
|      ! 0 | 14452 | `		}` |
|    21416 | 14453 | `	}else{` |
|        - | 14454 | `		/* Tokenize raw text */` |
|    13615 | 14455 | `		SySetAlloc(&aRawToken,32);` |
|    13615 | 14456 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|        - | 14457 | `	}` |
|        - | 14458 | `	/* Process high-level tokens */` |
|    56437 | 14459 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    56437 | 14460 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    56437 | 14461 | `	rc = PH7_OK;` |
|    56437 | 14462 | `	if( is_expr ){` |
|        - | 14463 | `		/* Compile the expression */` |
|      ! 0 | 14464 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|      ! 0 | 14465 | `		goto cleanup;` |
|        - | 14466 | `	}` |
|    56437 | 14467 | `	nObjIdx = 0;` |
|        - | 14468 | `	/* Each compilation unit starts in the global namespace.` |
|        - | 14469 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|        - | 14470 | `	 * preventing namespace bleeding across include()d files. */` |
|    56437 | 14471 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        - | 14472 | `	/* Start the compilation process */` |
|    35027 | 14473 | `	for(;;){` |
|   123419 | 14474 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    56425 | 14475 | `			break; /* No more tokens to process */` |
|        - | 14476 | `		}` |
|    66999 | 14477 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|        - | 14478 | `			/* Compile the PHP chunk */` |
|    53377 | 14479 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    53377 | 14480 | `			if( rc == SXERR_ABORT ){` |
|       15 | 14481 | `				break;` |
|        - | 14482 | `			}` |
|    53365 | 14483 | `			continue;` |
|        - | 14484 | `		}` |
|        - | 14485 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    13627 | 14486 | `		nRawObj = 0;` |
|    27291 | 14487 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|        - | 14488 | `			/* Consume the raw chunk without any processing */` |
|    13669 | 14489 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    13669 | 14490 | `			if( pRawObj == 0 ){` |
|      ! 0 | 14491 | `				rc = SXERR_MEM;` |
|      ! 0 | 14492 | `				break;` |
|        - | 14493 | `			}` |
|        - | 14494 | `			/* Mark as constant and emit the load constant instruction */` |
|    13669 | 14495 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    13669 | 14496 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    13669 | 14497 | `			++nRawObj;` |
|    13669 | 14498 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|        5 | 14499 | `		}` |
|    13627 | 14500 | `		if( nRawObj > 0 ){` |
|        - | 14501 | `			/* Emit the consume instruction */` |
|    13627 | 14502 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     6811 | 14503 | `		}` |
|    28221 | 14504 | `	}` |
|    28216 | 14505 | `cleanup:` |
|    56437 | 14506 | `	SySetRelease(&aRawToken);` |
|    56437 | 14507 | `	SySetRelease(&aPhpToken);` |
|        - | 14508 | `	/* Restore outer file's strict_types scope */` |
|    56437 | 14509 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    56437 | 14510 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    56437 | 14511 | `	return rc;` |
|    28221 | 14512 | `}` |
|        - | 14513 | `/*` |
|        - | 14514 | ` * Utility routines.Initialize the code generator.` |
|        - | 14515 | ` */` |
|     3884 | 14516 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|        - | 14517 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 14518 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 14519 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 14520 | `	)` |
|        5 | 14521 | `{` |
|     3889 | 14522 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 14523 | `	/* Zero the structure */` |
|     3889 | 14524 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|        - | 14525 | `	/* Initial state */` |
|     3889 | 14526 | `	pGen->pVm  = &(*pVm);` |
|     3889 | 14527 | `	pGen->xErr = xErr;` |
|     3889 | 14528 | `	pGen->pErrData = pErrData;` |
|     3889 | 14529 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|     3889 | 14530 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|     3889 | 14531 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|     3889 | 14532 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3889 | 14533 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3889 | 14534 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|     3889 | 14535 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|        - | 14536 | `	/* Error log buffer */` |
|     3889 | 14537 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        - | 14538 | `	/* General purpose working buffer */` |
|     3889 | 14539 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        - | 14540 | `	/* Namespace state */` |
|     3889 | 14541 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     3889 | 14542 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     3889 | 14543 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     3889 | 14544 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 14545 | `	/* Create the global scope */` |
|     3889 | 14546 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|        - | 14547 | `	/* Point to the global scope */` |
|     3889 | 14548 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     3889 | 14549 | `	return SXRET_OK;` |
|        5 | 14550 | `}` |
|        - | 14551 | `/*` |
|        - | 14552 | ` * Utility routines. Reset the code generator to it's initial state.` |
|        - | 14553 | ` */` |
|    59936 | 14554 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|        - | 14555 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 14556 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 14557 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 14558 | `	)` |
|        5 | 14559 | `{` |
|    59941 | 14560 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 14561 | `	GenBlock *pBlock,*pParent;` |
|        - | 14562 | `	/* Reset state */` |
|    59941 | 14563 | `	SySetReset(&pGen->aLabel);` |
|    59941 | 14564 | `	SySetReset(&pGen->aGoto);` |
|    59941 | 14565 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    59941 | 14566 | `	SySetReset(&pGen->aTrivia);` |
|    59941 | 14567 | `	SySetReset(&pGen->aPendingAttrs);` |
|    59941 | 14568 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    59941 | 14569 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    59941 | 14570 | `	SyBlobRelease(&pGen->sWorker);` |
|    59941 | 14571 | `	SyBlobRelease(&pGen->sNamespace);` |
|    59941 | 14572 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    59941 | 14573 | `	SyHashRelease(&pGen->hUseImports);` |
|    59941 | 14574 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    59941 | 14575 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    59941 | 14576 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    59941 | 14577 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    59941 | 14578 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 14579 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|        - | 14580 | `	 * They intern variable names and literal strings that are referenced by` |
|        - | 14581 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|        - | 14582 | `	 * Releasing them would either leak the interned strings or require freeing` |
|        - | 14583 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|        - | 14584 | `	 * number of unique names, which is acceptable. */` |
|        - | 14585 | `	/* Point to the global scope */` |
|    59941 | 14586 | `	pBlock = pGen->pCurrent;` |
|    59941 | 14587 | `	while( pBlock->pParent != 0 ){` |
|      ! 0 | 14588 | `		pParent = pBlock->pParent;` |
|      ! 0 | 14589 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 14590 | `		pBlock = pParent;` |
|      ! 0 | 14591 | `	}` |
|    59941 | 14592 | `	pGen->xErr = xErr;` |
|    59941 | 14593 | `	pGen->pErrData = pErrData;` |
|    59941 | 14594 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    59941 | 14595 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    59941 | 14596 | `	pGen->pIn = pGen->pEnd = 0;` |
|    59941 | 14597 | `	pGen->nErr = 0;` |
|    59941 | 14598 | `	return SXRET_OK;` |
|        5 | 14599 | `}` |
|        - | 14600 | `/*` |
|        - | 14601 | ` * Generate a compile-time error message.` |
|        - | 14602 | ` * If the error count limit is reached (usually 15 error message)` |
|        - | 14603 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|        - | 14604 | ` * abort compilation immediately.` |
|        - | 14605 | ` */` |
|      652 | 14606 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|        5 | 14607 | `{` |
|      657 | 14608 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|      657 | 14609 | `	const char *zErr = "Error";` |
|        - | 14610 | `	SyString *pFile;` |
|        - | 14611 | `	va_list ap;` |
|        - | 14612 | `	sxi32 rc;` |
|        - | 14613 | `	/* Reset the working buffer */` |
|      657 | 14614 | `	SyBlobReset(pWorker);` |
|        - | 14615 | `	/* Peek the processed file path if available */` |
|      657 | 14616 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      657 | 14617 | `	if( nErrType == E_ERROR ){` |
|        - | 14618 | `		/* Increment the error counter */` |
|      543 | 14619 | `		pGen->nErr++;` |
|      543 | 14620 | `		if( pGen->nErr > 15 ){` |
|        - | 14621 | `			/* Error count limit reached */` |
|        6 | 14622 | `			if( pGen->xErr ){` |
|        6 | 14623 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|        6 | 14624 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|        6 | 14625 | `				if( pFile ){` |
|        6 | 14626 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|        2 | 14627 | `				}` |
|        6 | 14628 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|        6 | 14629 | `				if( SyBlobLength(pWorker) > 0 ){` |
|        6 | 14630 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|        2 | 14631 | `				}` |
|        2 | 14632 | `			}` |
|        - | 14633 | `			/* Abort immediately */` |
|        6 | 14634 | `			return SXERR_ABORT;` |
|        - | 14635 | `		}` |
|      267 | 14636 | `	}` |
|      653 | 14637 | `	if( pGen->xErr == 0 ){` |
|        - | 14638 | `		/* No available error consumer,return immediately */` |
|        3 | 14639 | `		return SXRET_OK;` |
|        - | 14640 | `	}` |
|      650 | 14641 | `	switch(nErrType){` |
|      536 | 14642 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|       32 | 14643 | `	case E_WARNING: zErr = "Warning";     break;` |
|       82 | 14644 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       12 | 14645 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|      ! 0 | 14646 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|      ! 0 | 14647 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|      ! 0 | 14648 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|      ! 0 | 14649 | `	default:` |
|      ! 0 | 14650 | `		break;` |
|        - | 14651 | `	}` |
|      650 | 14652 | `	rc = SXRET_OK;` |
|        - | 14653 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|      650 | 14654 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|      650 | 14655 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|      650 | 14656 | `	va_start(ap,zFormat);` |
|      650 | 14657 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      650 | 14658 | `	va_end(ap);` |
|      650 | 14659 | `	if( pFile ){` |
|      650 | 14660 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|      323 | 14661 | `	}` |
|        - | 14662 | `	/* Append a new line */` |
|      650 | 14663 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|      650 | 14664 | `	if( SyBlobLength(pWorker) > 0 ){` |
|        - | 14665 | `		/* Consume the generated error message */` |
|      650 | 14666 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|      323 | 14667 | `	}` |
|      650 | 14668 | `	return rc;` |
|      331 | 14669 | `}` |
|        - | 14670 |  |
