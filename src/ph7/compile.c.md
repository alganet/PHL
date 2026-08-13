# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6586/8142 lines (80.89%)

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
|  5837980 |   161 | `static void GenStateInitBlock(` |
|        - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|        - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|        - |   166 | `	void *pUserData      /* Upper layer private data */` |
|        - |   167 | `	)` |
|        5 |   168 | `{` |
|        - |   169 | `	/* Initialize block fields */` |
|  5837985 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  5837985 |   171 | `	pBlock->pUserData   = pUserData;` |
|  5837985 |   172 | `	pBlock->pGen        = pGen;` |
|  5837985 |   173 | `	pBlock->iFlags      = iType;` |
|  5837985 |   174 | `	pBlock->pParent     = 0;` |
|  5837985 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5837985 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5837985 |   177 | `}` |
|        - |   178 | `/*` |
|        - |   179 | ` * Allocate a new block instance.` |
|        - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|        - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|        - |   182 | ` * processing on failure.` |
|        - |   183 | ` */` |
|  5834096 |   184 | `static sxi32 GenStateEnterBlock(` |
|        - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|        - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|        - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|        - |   190 | `	)` |
|        5 |   191 | `{` |
|        - |   192 | `	GenBlock *pBlock;` |
|        - |   193 | `	/* Allocate a new block instance */` |
|  5834101 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  5834101 |   195 | `	if( pBlock == 0 ){` |
|        - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   198 | `		 */` |
|      ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|        - |   200 | `		/* Abort processing immediately */` |
|      ! 0 |   201 | `		return SXERR_ABORT;` |
|        - |   202 | `	}` |
|        - |   203 | `	/* Zero the structure */` |
|  5834101 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  5834101 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|        - |   206 | `	/* Link to the parent block */` |
|  5834101 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|        - |   208 | `	/* Mark as the current block */` |
|  5834101 |   209 | `	pGen->pCurrent = pBlock;` |
|  5834101 |   210 | `	if( ppBlock ){` |
|        - |   211 | `		/* Write a pointer to the new instance */` |
|  2826383 |   212 | `		*ppBlock = pBlock;` |
|  1413189 |   213 | `	}` |
|  5834101 |   214 | `	return SXRET_OK;` |
|  2917053 |   215 | `}` |
|        - |   216 | `/*` |
|        - |   217 | ` * Release block fields without freeing the whole instance.` |
|        - |   218 | ` */` |
|  5834088 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|        5 |   220 | `{` |
|  5834093 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  5834093 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  5834093 |   223 | `}` |
|        - |   224 | `/*` |
|        - |   225 | ` * Release a block.` |
|        - |   226 | ` */` |
|  5834088 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|        5 |   228 | `{` |
|  5834093 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  5834093 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|        - |   231 | `	/* Free the instance */` |
|  5834093 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  5834093 |   233 | `}` |
|        - |   234 | `/*` |
|        - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|        - |   236 | ` */` |
|  5834088 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|        5 |   238 | `{` |
|  5834093 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  5834093 |   240 | `	if( pBlock == 0 ){` |
|        - |   241 | `		/* No more block to pop */` |
|      ! 0 |   242 | `		return SXERR_EMPTY;` |
|        - |   243 | `	}` |
|        - |   244 | `	/* Point to the upper block */` |
|  5834093 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  5834093 |   246 | `	if( ppBlock ){` |
|        - |   247 | `		/* Write a pointer to the popped block */` |
|      ! 0 |   248 | `		*ppBlock = pBlock;` |
|      ! 0 |   249 | `	}else{` |
|        - |   250 | `		/* Safely release the block */` |
|  5834093 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|        - |   252 | `	}` |
|  5834093 |   253 | `	return SXRET_OK;` |
|  2917049 |   254 | `}` |
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
|  2208902 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|        5 |   266 | `{` |
|        - |   267 | `	JumpFixup sJumpFix;` |
|        - |   268 | `	sxi32 rc;` |
|        - |   269 | `	/* Init the JumpFixup structure */` |
|  2208907 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  2208907 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|        - |   272 | `	/* Insert in the jump fixup table */` |
|  2208907 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  2208907 |   274 | `	return rc;` |
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
|  4152516 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|        5 |   288 | `{` |
|        - |   289 | `	JumpFixup *aFix;` |
|        - |   290 | `	VmInstr *pInstr;` |
|        - |   291 | `	sxu32 nFixed;` |
|        - |   292 | `	sxu32 n;` |
|        - |   293 | `	/* Point to the jump fixup table */` |
|  4152521 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|        - |   295 | `	/* Fix the desired jumps */` |
|  8083339 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  3930823 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|        - |   298 | `			/* Already fixed */` |
|  1410349 |   299 | `			continue;` |
|        - |   300 | `		}` |
|  2520479 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|        - |   302 | `			/* Not of our interest */` |
|   311579 |   303 | `			continue;` |
|        - |   304 | `		}` |
|        - |   305 | `		/* Point to the instruction to fix */` |
|  2208905 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  2208905 |   307 | `		if( pInstr ){` |
|  2208905 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  2208905 |   309 | `			nFixed++;` |
|        - |   310 | `			/* Mark as fixed */` |
|  2208905 |   311 | `			aFix[n].nJumpType = -1;` |
|  1104450 |   312 | `		}` |
|  1104455 |   313 | `	}` |
|        - |   314 | `	/* Total number of fixed jumps */` |
|  4152521 |   315 | `	return nFixed;` |
|        5 |   316 | `}` |
|        - |   317 | `/*` |
|        - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|        - |   319 | ` * The goto statement can be used to jump to another section` |
|        - |   320 | ` * in the program.` |
|        - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|        - |   322 | ` * statement for more information.` |
|        - |   323 | ` */` |
|  1458812 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|        5 |   325 | `{` |
|        - |   326 | `	JumpFixup *pJump,*aJumps;` |
|        - |   327 | `	Label *pLabel,*aLabel;` |
|        - |   328 | `	VmInstr *pInstr;` |
|        - |   329 | `	sxi32 rc;` |
|        - |   330 | `	sxu32 n;` |
|        - |   331 | `	/* Point to the goto table */` |
|  1458817 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|        - |   333 | `	/* Fix */` |
|  1458963 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  1458815 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  1458947 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|        - |   362 | `			/* Emit a warning */` |
|       40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|       24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|       12 |   365 | `		}` |
|       71 |   366 | `	}` |
|  1458815 |   367 | `	return SXRET_OK;` |
|   729411 |   368 | `}` |
|        - |   369 | `/*` |
|        - |   370 | ` * Check if a given token value is installed in the literal table.` |
|        - |   371 | ` */` |
|  7303156 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|        5 |   373 | `{` |
|        - |   374 | `	SyHashEntry *pEntry;` |
|  7303161 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  7303161 |   376 | `	if( pEntry == 0 ){` |
|  1926677 |   377 | `		return SXERR_NOTFOUND;` |
|        - |   378 | `	}` |
|  5376489 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  5376489 |   380 | `	return SXRET_OK;` |
|  3651583 |   381 | `}` |
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
|  1926672 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|        5 |   393 | `{` |
|  1926677 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  1926677 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   963336 |   396 | `	}` |
|  1926677 |   397 | `	return SXRET_OK;` |
|        5 |   398 | `}` |
|        - |   399 | `/*` |
|        - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|        - |   401 | ` * in the constant table.` |
|        - |   402 | ` */` |
|  1288004 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|        5 |   404 | `{` |
|        - |   405 | `	ph7_value *pObj;` |
|  1288009 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|        - |   407 | `	/* Reserve a new constant */` |
|  1288009 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1288009 |   409 | `	if( pObj == 0 ){` |
|      ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   411 | `		return 0;` |
|        - |   412 | `	}` |
|  1288009 |   413 | `	*pIdx = nIdx;` |
|        - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|        - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|        - |   416 | `	 */` |
|  1288009 |   417 | `	return pObj;` |
|   644007 |   418 | `}` |
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
|  3683412 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|        5 |   434 | `{` |
|        - |   435 | `	VmCallArgMap *pMap;` |
|  3683417 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|       39 |   437 | `	if( p3 == 0 ){` |
|       35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       35 |   439 | `		if( pMap == 0 ) return 0;` |
|       35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       35 |   441 | `		p3 = (void *)pMap;` |
|       16 |   442 | `	}` |
|       39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|       39 |   444 | `	return p3;` |
|  1841711 |   445 | `}` |
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
|  1288992 |   509 | `static int GenStateFindBadNumericSeparator(` |
|        - |   510 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   511 | `{` |
|  1288997 |   512 | `	const char *z = pRaw->zString;` |
|  1288997 |   513 | `	sxu32 n = pRaw->nByte;` |
|  1288997 |   514 | `	int base = 10;` |
|        - |   515 | `	sxu32 i, start;` |
|  1288997 |   516 | `	if( n < 2 ) return 0;` |
|   400331 |   517 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|       80 |   518 | `		base = 16;` |
|   400292 |   519 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      284 |   520 | `		base = 2;` |
|      141 |   521 | `	}` |
|  1295173 |   522 | `	for( i = 0; i < n; ++i ){` |
|   894861 |   523 | `		if( z[i] != '_' ) continue;` |
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
|   400317 |   540 | `	return 0;` |
|   644501 |   541 | `}` |
|        - |   542 | `/*` |
|        - |   543 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   544 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   545 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   546 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   547 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   548 | ` * so callers can bail from the current construct).` |
|        - |   549 | ` */` |
|  1288992 |   550 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   551 | `{` |
|  1288997 |   552 | `	const char *zBad = 0;` |
|  1288997 |   553 | `	sxu32 nBad = 0;` |
|        - |   554 | `	SyString sBad;` |
|        - |   555 | `	sxi32 rc;` |
|  1288997 |   556 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  1288983 |   557 | `		return SXRET_OK;` |
|        - |   558 | `	}` |
|       18 |   559 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       18 |   560 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   561 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       18 |   562 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   563 | `		return SXERR_ABORT;` |
|        - |   564 | `	}` |
|       18 |   565 | `	return SXERR_SYNTAX;` |
|   644501 |   566 | `}` |
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
|  1288978 |   583 | `static sxi32 GenStateStripNumericSeparators(` |
|        - |   584 | `	SyMemBackend *pAlloc,` |
|        - |   585 | `	const SyString *pToken,` |
|        - |   586 | `	char *zScratch, sxu32 nScratch,` |
|        - |   587 | `	SyString *pOut, char **pzAlloc)` |
|        5 |   588 | `{` |
|        - |   589 | `	sxu32 i, j;` |
|  1288983 |   590 | `	int hasUnderscore = 0;` |
|        - |   591 | `	char *zBuf;` |
|  1288983 |   592 | `	*pzAlloc = 0;` |
|  3070425 |   593 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  1781699 |   594 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   890726 |   595 | `	}` |
|  1288983 |   596 | `	if( !hasUnderscore ){` |
|  1288731 |   597 | `		SyStringDupPtr(pOut, pToken);` |
|  1288731 |   598 | `		return SXRET_OK;` |
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
|   644494 |   615 | `}` |
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
|  1288038 |   651 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |   652 | `{` |
|  1288043 |   653 | `	const char *z = pNum->zString;` |
|  1288043 |   654 | `	const char *zEnd = z + pNum->nByte;` |
|        - |   655 | `	const char *p, *q;` |
|        - |   656 | `	int n;` |
|  1288043 |   657 | `	*pbDecimal = FALSE;` |
|  1288043 |   658 | `	if( z >= zEnd ){` |
|      ! 0 |   659 | `		return FALSE;` |
|        - |   660 | `	}` |
|  1288043 |   661 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
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
|  1287967 |   676 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|  1287687 |   691 | `	}else if( z[0] == '0' ){` |
|        - |   692 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |   693 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |   694 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   355571 |   695 | `		p = z;` |
|   711139 |   696 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   355799 |   697 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   355571 |   698 | `		if( n <= 21 ){` |
|   355569 |   699 | `			return FALSE;` |
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
|   932121 |   712 | `	p = z;` |
|   932121 |   713 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  2351533 |   714 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   932121 |   715 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       25 |   716 | `		*pbDecimal = TRUE;` |
|       25 |   717 | `		return TRUE;` |
|        - |   718 | `	}` |
|   932097 |   719 | `	return FALSE;` |
|   644024 |   720 | `}` |
|  1288964 |   721 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   722 | `{` |
|  1288969 |   723 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  1288969 |   724 | `	sxu32 nIdx = 0;` |
|        - |   725 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  1288969 |   726 | `	char *zAlloc = 0;` |
|        - |   727 | `	SyString sNum;` |
|        - |   728 | `	sxi32 rc;` |
|   644482 |   729 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  1288969 |   730 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  1288969 |   731 | `	if( rc != SXRET_OK ){` |
|       14 |   732 | `		return rc;` |
|        - |   733 | `	}` |
|  1933436 |   734 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   644477 |   735 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  1288959 |   736 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   737 | `		return SXERR_ABORT;` |
|        - |   738 | `	}` |
|  1288959 |   739 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |   740 | `		ph7_value *pObj;` |
|        - |   741 | `		sxi64 iValue;` |
|  1288043 |   742 | `		ph7_real rOverflow = 0;` |
|  1288043 |   743 | `		int bDecimalOverflow = 0;` |
|  1288043 |   744 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|  1288009 |   761 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  1288009 |   762 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  1288009 |   763 | `			if( pObj == 0 ){` |
|      ! 0 |   764 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   765 | `				return SXERR_ABORT;` |
|        - |   766 | `			}` |
|  1288009 |   767 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |   768 | `		}` |
|   644024 |   769 | `	}else{` |
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
|  1288959 |   782 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |   783 | `	/* Emit the load constant instruction */` |
|  1288959 |   784 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |   785 | `	/* Node successfully compiled */` |
|  1288959 |   786 | `	return SXRET_OK;` |
|   644487 |   787 | `}` |
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
|  2972098 |   799 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   800 | `{` |
|  2972103 |   801 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |   802 | `	const char *zIn,*zCur,*zEnd;` |
|        - |   803 | `	ph7_value *pObj;` |
|        - |   804 | `	sxu32 nIdx;` |
|  2972103 |   805 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |   806 | `	/* Delimit the string */` |
|  2972103 |   807 | `	zIn  = pStr->zString;` |
|  2972103 |   808 | `	zEnd = &zIn[pStr->nByte];` |
|  2972103 |   809 | `	if( zIn >= zEnd ){` |
|        - |   810 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |   811 | `		 * rather than reserving a new object each time. */` |
|   136133 |   812 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   136133 |   813 | `		return SXRET_OK;` |
|        - |   814 | `	}` |
|  2835975 |   815 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |   816 | `		/* Already processed,emit the load constant instruction` |
|        - |   817 | `		 * and return.` |
|        - |   818 | `		 */` |
|  1821867 |   819 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  1821867 |   820 | `		return SXRET_OK;` |
|        - |   821 | `	}` |
|        - |   822 | `	/* Reserve a new constant */` |
|  1014113 |   823 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1014113 |   824 | `	if( pObj == 0 ){` |
|      ! 0 |   825 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   826 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |   827 | `		return SXERR_ABORT;` |
|        - |   828 | `	}` |
|  1014113 |   829 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |   830 | `	/* Compile the node */` |
|  1014167 |   831 | `	for(;;){` |
|  2028339 |   832 | `		if( zIn >= zEnd ){` |
|        - |   833 | `			/* End of input */` |
|  1014113 |   834 | `			break;` |
|        - |   835 | `		}` |
|  1014231 |   836 | `		zCur = zIn;` |
| 19816307 |   837 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 18802081 |   838 | `			zIn++;` |
|        5 |   839 | `		}` |
|  1014231 |   840 | `		if( zIn > zCur ){` |
|        - |   841 | `			/* Append raw contents*/` |
|   983133 |   842 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   491564 |   843 | `		}` |
|  1014231 |   844 | `		zIn++;` |
|  1014231 |   845 | `		if( zIn < zEnd ){` |
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
|  1014231 |   860 | `		zIn++;` |
|        5 |   861 | `	}` |
|        - |   862 | `	/* Emit the load constant instruction */` |
|  1014113 |   863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  1014113 |   864 | `	if( pStr->nByte < 1024 ){` |
|        - |   865 | `		/* Install in the literal table */` |
|  1014113 |   866 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   507054 |   867 | `	}` |
|        - |   868 | `	/* Node successfully compiled */` |
|  1014113 |   869 | `	return SXRET_OK;` |
|  1486054 |   870 | `}` |
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
|    38398 |  1070 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  1071 | `{` |
|        - |  1072 | `	ph7_value *pConstObj;` |
|    38403 |  1073 | `	sxu32 nIdx = 0;` |
|        - |  1074 | `	/* Reserve a new constant */` |
|    38403 |  1075 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    38403 |  1076 | `	if( pConstObj == 0 ){` |
|      ! 0 |  1077 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  1078 | `		return 0;` |
|        - |  1079 | `	}` |
|    38403 |  1080 | `	(*pCount)++;` |
|    38403 |  1081 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  1082 | `	/* Emit the load constant instruction */` |
|    38403 |  1083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    38403 |  1084 | `	return pConstObj;` |
|    19204 |  1085 | `}` |
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
|    36882 |  1148 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  1149 | `{` |
|    36887 |  1150 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  1151 | `	const char *zIn,*zCur,*zEnd;` |
|    36887 |  1152 | `	ph7_value *pObj = 0;` |
|        - |  1153 | `	sxi32 iCons;` |
|        - |  1154 | `	sxi32 rc;` |
|        - |  1155 | `	/* Delimit the string */` |
|    36887 |  1156 | `	zIn  = pStr->zString;` |
|    36887 |  1157 | `	zEnd = &zIn[pStr->nByte];` |
|    36887 |  1158 | `	if( zIn >= zEnd ){` |
|        - |  1159 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  1160 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  1161 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  1162 | `		 */` |
|      375 |  1163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      375 |  1164 | `		return SXRET_OK;` |
|        - |  1165 | `	}` |
|    36517 |  1166 | `	zCur = 0;` |
|        - |  1167 | `	/* Compile the node */` |
|    36517 |  1168 | `	iCons = 0;` |
|    19490 |  1169 | `	for(;;){` |
|    62801 |  1170 | `		zCur = zIn;` |
|   214873 |  1171 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   154545 |  1172 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       72 |  1173 | `				break;` |
|   154411 |  1174 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2338 |  1175 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1170 |  1176 | `					break;` |
|        - |  1177 | `			}` |
|   152077 |  1178 | `			zIn++;` |
|        5 |  1179 | `		}` |
|    62801 |  1180 | `		if( zIn > zCur ){` |
|    20373 |  1181 | `			if( pObj == 0 ){` |
|    19843 |  1182 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    19843 |  1183 | `				if( pObj == 0 ){` |
|      ! 0 |  1184 | `					return SXERR_ABORT;` |
|        - |  1185 | `				}` |
|     9919 |  1186 | `			}` |
|    20373 |  1187 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    10184 |  1188 | `		}` |
|    62801 |  1189 | `		if( zIn >= zEnd ){` |
|    36515 |  1190 | `			break;` |
|        - |  1191 | `		}` |
|    26291 |  1192 | `		if( zIn[0] == '\\' ){` |
|    23823 |  1193 | `			const char *zPtr = 0;` |
|        - |  1194 | `			sxu32 n;` |
|    23823 |  1195 | `			zIn++;` |
|    23823 |  1196 | `			if( pObj == 0 ){` |
|    18565 |  1197 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    18565 |  1198 | `				if( pObj == 0 ){` |
|      ! 0 |  1199 | `					return SXERR_ABORT;` |
|        - |  1200 | `				}` |
|     9280 |  1201 | `			}` |
|    23823 |  1202 | `			if( zIn >= zEnd ){` |
|        - |  1203 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  1204 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  1205 | `				break;` |
|        - |  1206 | `			}` |
|    23821 |  1207 | `			n = sizeof(char); /* size of conversion */` |
|    23821 |  1208 | `			switch( zIn[0] ){` |
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
|    11356 |  1225 | `			case 'n':` |
|        - |  1226 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    22717 |  1227 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    22717 |  1228 | `				break;` |
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
|    23821 |  1351 | `			zIn += n;` |
|    23821 |  1352 | `			continue;` |
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
|    36517 |  1470 | `	if( iCons > 1 ){` |
|        - |  1471 | `		/* Concatenate all compiled constants */` |
|     1807 |  1472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|      901 |  1473 | `	}` |
|        - |  1474 | `	/* Node successfully compiled */` |
|    36517 |  1475 | `	return SXRET_OK;` |
|    18446 |  1476 | `}` |
|        - |  1477 | `/*` |
|        - |  1478 | ` * Compile a double quoted string.` |
|        - |  1479 | ` *  See the block-comment above for more information.` |
|        - |  1480 | ` */` |
|    36820 |  1481 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1482 | `{` |
|        - |  1483 | `	sxi32 rc;` |
|    36825 |  1484 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    18410 |  1485 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  1486 | `	/* Compilation result */` |
|    36825 |  1487 | `	return rc;` |
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
|   514334 |  1531 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   514339 |  1542 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - |  1543 | `	/* Compile the expression*/` |
|   514339 |  1544 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - |  1545 | `	/* Restore token stream */` |
|   514339 |  1546 | `	RE_SWAP_DELIMITER(pGen);` |
|   514339 |  1547 | `	return rc;` |
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
|   551922 |  1588 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 |  1589 | `{` |
|   551927 |  1590 | `	SyToken *pCur = pStart;` |
|   551927 |  1591 | `	sxi32 iNest = 0;` |
|  1677929 |  1592 | `	while( pCur < pEnd ){` |
|  1330131 |  1593 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   204125 |  1594 | `			return pCur;` |
|        - |  1595 | `		}` |
|        - |  1596 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - |  1597 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - |  1598 | `		 * not an entry separator. Skip past the signature.` |
|        - |  1599 | `		 */` |
|  1126011 |  1600 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|  1126005 |  1661 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    50943 |  1662 | `			iNest++;` |
|  1100536 |  1663 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - |  1664 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - |  1665 | `			 * parser will shortly detect any syntax error. */` |
|    50943 |  1666 | `			iNest--;` |
|    25469 |  1667 | `		}` |
|  1126005 |  1668 | `		pCur++;` |
|        5 |  1669 | `	}` |
|   347803 |  1670 | `	return pEnd;` |
|   275966 |  1671 | `}` |
|        - |  1672 | `/*` |
|        - |  1673 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - |  1674 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - |  1675 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - |  1676 | ` */` |
|   287096 |  1677 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 |  1678 | `{` |
|        - |  1679 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - |  1680 | `	SyToken *pKey,*pCur;` |
|   287101 |  1681 | `	sxi32 iEmitRef = 0;` |
|   287101 |  1682 | `	sxi32 iSpread = 0;` |
|   287101 |  1683 | `	sxi32 nPair = 0;` |
|        - |  1684 | `	sxi32 rc;` |
|   287101 |  1685 | `	xValidator = 0;` |
|   331684 |  1686 | `	for(;;){` |
|        - |  1687 | `		/* Jump leading commas */` |
|   943425 |  1688 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   280057 |  1689 | `			pGen->pIn++;` |
|        5 |  1690 | `		}` |
|   663373 |  1691 | `		pCur = pGen->pIn;` |
|   663373 |  1692 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - |  1693 | `			/* No more entry to process */` |
|   287085 |  1694 | `			break;` |
|        - |  1695 | `		}` |
|   376293 |  1696 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 |  1697 | `			continue;` |
|        - |  1698 | `		}` |
|        - |  1699 | `		/* Compile the key if available */` |
|   376293 |  1700 | `		pKey = pCur;` |
|   376293 |  1701 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   376293 |  1702 | `		rc = SXERR_EMPTY;` |
|   376293 |  1703 | `		if( pCur < pGen->pIn ){` |
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
|   307393 |  1719 | `		}else if( pKey == pCur ){` |
|        - |  1720 | `			/* Key is omitted,emit a warning */` |
|      ! 0 |  1721 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|      ! 0 |  1722 | `			pCur++; /* Jump the '=>' operator */` |
|      ! 0 |  1723 | `		}else{` |
|        - |  1724 | `			/* Reset back the cursor and point to the entry value */` |
|   238503 |  1725 | `			pCur = pKey;` |
|        - |  1726 | `		}` |
|   376283 |  1727 | `		if( rc == SXERR_EMPTY ){` |
|        - |  1728 | `			/* No available key,load NULL */` |
|   238505 |  1729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|   119250 |  1730 | `		}` |
|   376283 |  1731 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   376281 |  1750 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   376281 |  1751 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   376277 |  1764 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   376277 |  1765 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  1766 | `			return SXERR_ABORT;` |
|        - |  1767 | `		}` |
|   376277 |  1768 | `		if( iSpread ){` |
|        - |  1769 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|       69 |  1770 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   376244 |  1771 | `		}else if( iEmitRef ){` |
|        - |  1772 | `			/* Emit the load reference instruction */` |
|       41 |  1773 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       18 |  1774 | `		}` |
|   376277 |  1775 | `		xValidator = 0;` |
|   376277 |  1776 | `		iEmitRef = 0;` |
|   376277 |  1777 | `		iSpread = 0;` |
|   376277 |  1778 | `		nPair++;` |
|        5 |  1779 | `	}` |
|        - |  1780 | `	/* Emit the load map instruction */` |
|   287085 |  1781 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - |  1782 | `	/* Node successfully compiled */` |
|   287085 |  1783 | `	return SXRET_OK;` |
|   143553 |  1784 | `}` |
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
|     1716 |  1899 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1900 | `{` |
|        - |  1901 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     1721 |  1902 | `	pGen->pIn++;` |
|     1721 |  1903 | `	pGen->pEnd--;` |
|      858 |  1904 | `	SXUNUSED(iCompileFlag);` |
|     1721 |  1905 | `	return GenStateCompileArrayBody(pGen);` |
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
|      442 |  2243 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2244 | `{` |
|      447 |  2245 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |  2246 | `	char zName[512];         /* Unique lambda name */` |
|        - |  2247 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |  2248 | `							  * one thread is allowed to compile the script.` |
|        - |  2249 | `						      */` |
|        - |  2250 | `	SyString sName;` |
|        - |  2251 | `	sxu32 nKwLine;` |
|      447 |  2252 | `	sxi32 iFlags = 0;` |
|        - |  2253 | `	sxu32 nLen;` |
|        - |  2254 | `	sxi32 rc;` |
|      221 |  2255 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2256 |  |
|      447 |  2257 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|      442 |  2258 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      447 |  2259 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |  2260 | `		/* Static closure: no $this auto-capture, bind refused */` |
|        9 |  2261 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        9 |  2262 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|        4 |  2263 | `	}` |
|      447 |  2264 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      447 |  2265 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |  2266 | `		pGen->pIn++;` |
|      ! 0 |  2267 | `	}` |
|        - |  2268 | `	/* Generate a unique name */` |
|      447 |  2269 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |  2270 | `	/* Make sure the generated name is unique */` |
|      447 |  2271 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2272 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2273 | `	}` |
|      447 |  2274 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |  2275 | `	/* Compile the lambda body */` |
|      447 |  2276 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|      447 |  2277 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2278 | `		return SXERR_ABORT;` |
|        - |  2279 | `	}` |
|      447 |  2280 | `	if( pAnnonFunc ){` |
|      447 |  2281 | `		pAnnonFunc->nLine = nKwLine;` |
|      221 |  2282 | `	}` |
|        - |  2283 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |  2284 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |  2285 | `	 * the handler wraps either in a Closure instance. */` |
|      447 |  2286 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |  2287 | `	/* Node successfully compiled */` |
|      447 |  2288 | `	return SXRET_OK;` |
|      226 |  2289 | `}` |
|        - |  2290 | `/*` |
|        - |  2291 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |  2292 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |  2293 | ` * enclosing arrow level, or has already been captured.` |
|        - |  2294 | ` */` |
|      196 |  2295 | `static sxi32 GenStateArrowAddCapture(` |
|        - |  2296 | `	ph7_gen_state *pGen,` |
|        - |  2297 | `	ph7_vm_func *pFunc,` |
|        - |  2298 | `	const char *zName,` |
|        - |  2299 | `	sxu32 nByte,` |
|        - |  2300 | `	SyString *aShadow,` |
|        - |  2301 | `	sxu32 nShadow)` |
|        3 |  2302 | `{` |
|        - |  2303 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2304 | `	ph7_vm_func_closure_env *aEnv;` |
|        - |  2305 | `	sxu32 n, nEnv;` |
|        - |  2306 | `	char *zDup;` |
|      199 |  2307 | `	if( nByte == 0 ){` |
|      ! 0 |  2308 | `		return SXRET_OK;` |
|        - |  2309 | `	}` |
|      196 |  2310 | `	if( nByte == sizeof("this")-1` |
|      107 |  2311 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        3 |  2312 | `		return SXRET_OK;` |
|        - |  2313 | `	}` |
|      247 |  2314 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      182 |  2315 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      176 |  2316 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      135 |  2317 | `			return SXRET_OK;` |
|        - |  2318 | `		}` |
|       27 |  2319 | `	}` |
|       63 |  2320 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       63 |  2321 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|       91 |  2322 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|       30 |  2323 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|       29 |  2324 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|        3 |  2325 | `			return SXRET_OK;` |
|        - |  2326 | `		}` |
|       15 |  2327 | `	}` |
|       61 |  2328 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|       61 |  2329 | `	if( zDup == 0 ){` |
|      ! 0 |  2330 | `		return SXERR_ABORT;` |
|        - |  2331 | `	}` |
|       61 |  2332 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       61 |  2333 | `	sEnv.iFlags = 0;` |
|       61 |  2334 | `	sEnv.nIdx = SXU32_HIGH;` |
|       61 |  2335 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       61 |  2336 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|       61 |  2337 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       61 |  2338 | `	return SXRET_OK;` |
|      101 |  2339 | `}` |
|        - |  2340 | `/*` |
|        - |  2341 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|        - |  2342 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|        - |  2343 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|        - |  2344 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|        - |  2345 | ` */` |
|       56 |  2346 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|        - |  2347 | `	ph7_gen_state *pGen,` |
|        - |  2348 | `	ph7_vm_func *pFunc,` |
|        - |  2349 | `	const char *zIn,` |
|        - |  2350 | `	const char *zEnd,` |
|        - |  2351 | `	SyString *aShadow,` |
|        - |  2352 | `	sxu32 nShadow)` |
|        2 |  2353 | `{` |
|        - |  2354 | `	sxi32 rc;` |
|      370 |  2355 | `	while( zIn < zEnd ){` |
|      314 |  2356 | `		if( zIn[0] == '\\' ){` |
|        5 |  2357 | `			zIn++;` |
|        5 |  2358 | `			if( zIn < zEnd ){` |
|        5 |  2359 | `				zIn++;` |
|        2 |  2360 | `			}` |
|        5 |  2361 | `			continue;` |
|        - |  2362 | `		}` |
|      308 |  2363 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|       26 |  2364 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|       24 |  2365 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|        - |  2366 | `			const char *zName;` |
|       26 |  2367 | `			zIn++; /* skip '$' */` |
|       26 |  2368 | `			zName = zIn;` |
|       82 |  2369 | `			while( zIn < zEnd ){` |
|       76 |  2370 | `				unsigned char c = (unsigned char)zIn[0];` |
|       76 |  2371 | `				if( c >= 0xc0 ){` |
|      ! 0 |  2372 | `					zIn++;` |
|      ! 0 |  2373 | `					while( zIn < zEnd` |
|      ! 0 |  2374 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  2375 | `						zIn++;` |
|      ! 0 |  2376 | `					}` |
|      ! 0 |  2377 | `					continue;` |
|        - |  2378 | `				}` |
|       76 |  2379 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       20 |  2380 | `					break;` |
|        - |  2381 | `				}` |
|       58 |  2382 | `				zIn++;` |
|        2 |  2383 | `			}` |
|       26 |  2384 | `			if( zIn > zName ){` |
|       38 |  2385 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|       24 |  2386 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|       26 |  2387 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2388 | `					return SXERR_ABORT;` |
|        - |  2389 | `				}` |
|       12 |  2390 | `			}` |
|       26 |  2391 | `			continue;` |
|        - |  2392 | `		}` |
|      286 |  2393 | `		zIn++;` |
|        2 |  2394 | `	}` |
|       58 |  2395 | `	return SXRET_OK;` |
|       30 |  2396 | `}` |
|        - |  2397 | `/*` |
|        - |  2398 | ` * Scan the body token range of an arrow function for free-variable` |
|        - |  2399 | ` * references and record them in pFunc's closure environment. Handles:` |
|        - |  2400 | ` *   - plain $<id> pairs` |
|        - |  2401 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|        - |  2402 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|        - |  2403 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|        - |  2404 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|        - |  2405 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|        - |  2406 | ` *     are never mistakenly captured.` |
|        - |  2407 | ` */` |
|      294 |  2408 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  2409 | `	ph7_gen_state *pGen,` |
|        - |  2410 | `	ph7_vm_func *pFunc,` |
|        - |  2411 | `	SyToken *pStart,` |
|        - |  2412 | `	SyToken *pEnd,` |
|        - |  2413 | `	SyString *aShadow,` |
|        - |  2414 | `	sxu32 nShadow)` |
|        4 |  2415 | `{` |
|      298 |  2416 | `	SyToken *pScan = pStart;` |
|        - |  2417 | `	sxi32 rc;` |
|     1704 |  2418 | `	while( pScan < pEnd ){` |
|     1410 |  2419 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       86 |  2420 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       28 |  2421 | `				pScan->sData.zString,` |
|       56 |  2422 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       28 |  2423 | `				aShadow,nShadow);` |
|       58 |  2424 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2425 | `				return SXERR_ABORT;` |
|        - |  2426 | `			}` |
|       58 |  2427 | `			pScan++;` |
|       58 |  2428 | `			continue;` |
|        - |  2429 | `		}` |
|     1354 |  2430 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|       30 |  2431 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|       30 |  2432 | `			SyToken *pFnKw = pScan;` |
|       28 |  2433 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|      ! 0 |  2434 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|        2 |  2435 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  2436 | `				pFnKw = &pScan[1];` |
|      ! 0 |  2437 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  2438 | `			}` |
|       30 |  2439 | `			if( nKw == PH7_TKWRD_FN ){` |
|        - |  2440 | `				SyToken *pInnerSigStart;` |
|        - |  2441 | `				SyToken *pInnerSigEnd;` |
|        - |  2442 | `				SyToken *pInnerBodyEnd;` |
|        - |  2443 | `				SyString *aInnerShadow;` |
|        - |  2444 | `				sxu32 nInnerShadow;` |
|        - |  2445 | `				sxu32 nInnerParamMax;` |
|        - |  2446 | `				SyToken *p;` |
|        - |  2447 | `				int iNestInner;` |
|       19 |  2448 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|       19 |  2449 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2450 | `					pScan++;` |
|      ! 0 |  2451 | `				}` |
|       19 |  2452 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2453 | `					pScan++;` |
|      ! 0 |  2454 | `					continue;` |
|        - |  2455 | `				}` |
|       19 |  2456 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|       19 |  2457 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|        - |  2458 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|       19 |  2459 | `				if( pInnerSigEnd >= pEnd ){` |
|      ! 0 |  2460 | `					pScan = pEnd;` |
|      ! 0 |  2461 | `					continue;` |
|        - |  2462 | `				}` |
|        - |  2463 | `				/* Build an augmented shadow list: inherited + inner params */` |
|       19 |  2464 | `				nInnerParamMax = 0;` |
|       57 |  2465 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2466 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|       13 |  2467 | `						nInnerParamMax++;` |
|        6 |  2468 | `					}` |
|       20 |  2469 | `				}` |
|       19 |  2470 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       18 |  2471 | `					&pGen->pVm->sAllocator,` |
|       18 |  2472 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|       19 |  2473 | `				if( aInnerShadow == 0 ){` |
|      ! 0 |  2474 | `					return SXERR_ABORT;` |
|        - |  2475 | `				}` |
|       19 |  2476 | `				nInnerShadow = 0;` |
|       25 |  2477 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|        7 |  2478 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|        4 |  2479 | `				}` |
|       57 |  2480 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2481 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       27 |  2482 | `						continue;` |
|        - |  2483 | `					}` |
|       13 |  2484 | `					if( &p[1] >= pInnerSigEnd ){` |
|      ! 0 |  2485 | `						break;` |
|        - |  2486 | `					}` |
|       13 |  2487 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2488 | `						continue;` |
|        - |  2489 | `					}` |
|       13 |  2490 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|        7 |  2491 | `				}` |
|       19 |  2492 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|       19 |  2493 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|      ! 0 |  2494 | `					pScan++;` |
|      ! 0 |  2495 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|      ! 0 |  2496 | `						&& pScan->sData.nByte == 1` |
|      ! 0 |  2497 | `						&& pScan->sData.zString[0] == '?' ){` |
|      ! 0 |  2498 | `						pScan++;` |
|      ! 0 |  2499 | `					}` |
|      ! 0 |  2500 | `					if( pScan < pEnd` |
|      ! 0 |  2501 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  2502 | `						pScan++;` |
|      ! 0 |  2503 | `					}` |
|      ! 0 |  2504 | `				}` |
|       19 |  2505 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|       19 |  2506 | `					pScan++; /* past '=>' */` |
|        9 |  2507 | `				}` |
|       19 |  2508 | `				pInnerBodyEnd = pScan;` |
|       19 |  2509 | `				iNestInner = 0;` |
|      131 |  2510 | `				while( pInnerBodyEnd < pEnd ){` |
|      113 |  2511 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|        - |  2512 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|        - |  2513 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      ! 0 |  2514 | `						break;` |
|        - |  2515 | `					}` |
|      113 |  2516 | `					if( pInnerBodyEnd->nType &` |
|        - |  2517 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        3 |  2518 | `						iNestInner++;` |
|      112 |  2519 | `					}else if( pInnerBodyEnd->nType &` |
|        - |  2520 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        3 |  2521 | `						iNestInner--;` |
|        1 |  2522 | `					}` |
|      113 |  2523 | `					pInnerBodyEnd++;` |
|        1 |  2524 | `				}` |
|        - |  2525 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|        - |  2526 | `				 * the outer's body: a default value is evaluated at call time` |
|        - |  2527 | `				 * in the outer frame, so any free variable it references is` |
|        - |  2528 | `				 * an outer capture. We must NOT scan the parameter-name` |
|        - |  2529 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|        - |  2530 | `				 * or those names leak into the outer's closure environment.` |
|        - |  2531 | `				 *` |
|        - |  2532 | `				 * Walk the signature argument-by-argument, splitting on` |
|        - |  2533 | `				 * top-level commas, and for each argument scan only the token` |
|        - |  2534 | `				 * range after the '=' sign. */` |
|        - |  2535 | `				{` |
|       19 |  2536 | `					SyToken *pArgStart = pInnerSigStart;` |
|       31 |  2537 | `					while( pArgStart < pInnerSigEnd ){` |
|       13 |  2538 | `						SyToken *pArgEnd = pArgStart;` |
|       13 |  2539 | `						SyToken *pEq = 0;` |
|       13 |  2540 | `						int iNestArg = 0;` |
|       49 |  2541 | `						while( pArgEnd < pInnerSigEnd ){` |
|       38 |  2542 | `							if( iNestArg == 0` |
|       39 |  2543 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|        3 |  2544 | `								break;` |
|        - |  2545 | `							}` |
|       37 |  2546 | `							if( pArgEnd->nType &` |
|        - |  2547 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  2548 | `								iNestArg++;` |
|       37 |  2549 | `							}else if( pArgEnd->nType &` |
|        - |  2550 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  2551 | `								iNestArg--;` |
|      ! 0 |  2552 | `							}` |
|       36 |  2553 | `							if( pEq == 0 && iNestArg == 0` |
|       31 |  2554 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|        7 |  2555 | `								pEq = pArgEnd;` |
|        3 |  2556 | `							}` |
|       37 |  2557 | `							pArgEnd++;` |
|        1 |  2558 | `						}` |
|       13 |  2559 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|       10 |  2560 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        3 |  2561 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|        7 |  2562 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  2563 | `								return SXERR_ABORT;` |
|        - |  2564 | `							}` |
|        3 |  2565 | `						}` |
|       13 |  2566 | `						pArgStart = pArgEnd;` |
|       12 |  2567 | `						if( pArgStart < pInnerSigEnd` |
|        8 |  2568 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|        3 |  2569 | `							pArgStart++;` |
|        1 |  2570 | `						}` |
|        1 |  2571 | `					}` |
|        - |  2572 | `				}` |
|       28 |  2573 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        9 |  2574 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|       19 |  2575 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2576 | `					return SXERR_ABORT;` |
|        - |  2577 | `				}` |
|       19 |  2578 | `				pScan = pInnerBodyEnd;` |
|       19 |  2579 | `				continue;` |
|        - |  2580 | `			}` |
|        5 |  2581 | `		}` |
|     1336 |  2582 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     1164 |  2583 | `			pScan++;` |
|     1164 |  2584 | `			continue;` |
|        - |  2585 | `		}` |
|        - |  2586 | `		{` |
|        - |  2587 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      175 |  2588 | `			SyToken *pDollar = pScan;` |
|      258 |  2589 | `			while( &pDollar[1] < pEnd` |
|      175 |  2590 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  2591 | `				pDollar++;` |
|      ! 0 |  2592 | `			}` |
|      175 |  2593 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  2594 | `				break;` |
|        - |  2595 | `			}` |
|      175 |  2596 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2597 | `				pScan = pDollar + 1;` |
|      ! 0 |  2598 | `				continue;` |
|        - |  2599 | `			}` |
|      261 |  2600 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      172 |  2601 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|       86 |  2602 | `				aShadow,nShadow);` |
|      175 |  2603 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2604 | `				return SXERR_ABORT;` |
|        - |  2605 | `			}` |
|      175 |  2606 | `			pScan = pDollar + 2;` |
|        - |  2607 | `		}` |
|        3 |  2608 | `	}` |
|      298 |  2609 | `	return SXRET_OK;` |
|      151 |  2610 | `}` |
|        - |  2611 | `/*` |
|        - |  2612 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  2613 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  2614 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  2615 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  2616 | ` * $this is also made available.` |
|        - |  2617 | ` */` |
|      276 |  2618 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2619 | `{` |
|        - |  2620 | `	ph7_vm_func *pFunc;` |
|        - |  2621 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2622 | `	GenBlock *pBlock;` |
|        - |  2623 | `	SySet *pInstrContainer;` |
|        - |  2624 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|        - |  2625 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|        - |  2626 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|        - |  2627 | `	SyToken *pSavedEnd;` |
|        - |  2628 | `	ph7_vm_func_arg *aArgs;` |
|        - |  2629 | `	char zName[512];` |
|        - |  2630 | `	static int iCnt = 1;` |
|        - |  2631 | `	char *zDup;` |
|        - |  2632 | `	sxu32 nLen;` |
|        - |  2633 | `	sxu32 nLine;` |
|      281 |  2634 | `	sxi32 iFlags = 0;` |
|      281 |  2635 | `	int bStatic = 0;` |
|        - |  2636 | `	sxi32 rc;` |
|        - |  2637 | `	sxu32 n;` |
|      138 |  2638 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2639 |  |
|      281 |  2640 | `	nLine = pGen->pIn->nLine;` |
|        - |  2641 | `	/* Optional 'static' prefix */` |
|      276 |  2642 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      281 |  2643 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        7 |  2644 | `		bStatic = 1;` |
|        7 |  2645 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        7 |  2646 | `		pGen->pIn++;` |
|        3 |  2647 | `	}` |
|        - |  2648 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      276 |  2649 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      281 |  2650 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  2651 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2652 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  2653 | `		return SXERR_SYNTAX;` |
|        - |  2654 | `	}` |
|      281 |  2655 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  2656 | `	/* Optional '&' — return by reference */` |
|      281 |  2657 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2658 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  2659 | `		pGen->pIn++;` |
|      ! 0 |  2660 | `	}` |
|        - |  2661 | `	/* Expect '(' */` |
|      281 |  2662 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  2663 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2664 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2665 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|        2 |  2666 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2667 | `		}else{` |
|      ! 0 |  2668 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2669 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|        - |  2670 | `		}` |
|        3 |  2671 | `		return SXERR_SYNTAX;` |
|        - |  2672 | `	}` |
|      279 |  2673 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  2674 | `	/* Delimit the parameter list */` |
|      279 |  2675 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      279 |  2676 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  2677 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2678 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  2679 | `		return SXERR_SYNTAX;` |
|        - |  2680 | `	}` |
|        - |  2681 | `	/* Allocate the function state */` |
|      277 |  2682 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      277 |  2683 | `	if( pFunc == 0 ){` |
|      ! 0 |  2684 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2685 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2686 | `		return SXERR_ABORT;` |
|        - |  2687 | `	}` |
|        - |  2688 | `	/* Generate a unique lambda name */` |
|      277 |  2689 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      277 |  2690 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2691 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2692 | `	}` |
|      277 |  2693 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      277 |  2694 | `	if( zDup == 0 ){` |
|      ! 0 |  2695 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2696 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2697 | `		return SXERR_ABORT;` |
|        - |  2698 | `	}` |
|      277 |  2699 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  2700 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|      277 |  2701 | `	pFunc->nLine = nLine;` |
|        - |  2702 | `	/* Collect function arguments */` |
|      277 |  2703 | `	if( pGen->pIn < pSigEnd ){` |
|      110 |  2704 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      110 |  2705 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2706 | `			return SXERR_ABORT;` |
|        - |  2707 | `		}` |
|       53 |  2708 | `	}` |
|        - |  2709 | `	/* Point past ')' and parse optional return type */` |
|      277 |  2710 | `	pGen->pIn = &pSigEnd[1];` |
|      277 |  2711 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      277 |  2712 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2713 | `		return SXERR_ABORT;` |
|      277 |  2714 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  2715 | `		return SXERR_SYNTAX;` |
|        - |  2716 | `	}` |
|        - |  2717 | `	/* Expect '=>' */` |
|      277 |  2718 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  2719 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2720 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2721 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|        2 |  2722 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2723 | `		}else{` |
|      ! 0 |  2724 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2725 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|        - |  2726 | `		}` |
|        3 |  2727 | `		return SXERR_SYNTAX;` |
|        - |  2728 | `	}` |
|      274 |  2729 | `	pGen->pIn++; /* Jump '=>' */` |
|      274 |  2730 | `	pBodyStart = pGen->pIn;` |
|      274 |  2731 | `	pBodyEnd = pGen->pEnd;` |
|        - |  2732 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  2733 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  2734 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  2735 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      274 |  2736 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  2737 | `	{` |
|      274 |  2738 | `		SyString *aShadow = 0;` |
|      274 |  2739 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      274 |  2740 | `		if( nShadow > 0 ){` |
|      107 |  2741 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      104 |  2742 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      107 |  2743 | `			if( aShadow == 0 ){` |
|      ! 0 |  2744 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2745 | `					"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2746 | `				return SXERR_ABORT;` |
|        - |  2747 | `			}` |
|      239 |  2748 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      135 |  2749 | `				aShadow[n] = aArgs[n].sName;` |
|       69 |  2750 | `			}` |
|       52 |  2751 | `		}` |
|      409 |  2752 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      135 |  2753 | `			aShadow,nShadow);` |
|      274 |  2754 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2755 | `			return SXERR_ABORT;` |
|        - |  2756 | `		}` |
|        - |  2757 | `	}` |
|        - |  2758 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  2759 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  2760 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  2761 | `	 * $this. */` |
|      274 |  2762 | `	if( !bStatic ){` |
|        - |  2763 | `		char *zThisDup;` |
|      268 |  2764 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      268 |  2765 | `		if( zThisDup == 0 ){` |
|      ! 0 |  2766 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2767 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2768 | `			return SXERR_ABORT;` |
|        - |  2769 | `		}` |
|      268 |  2770 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      268 |  2771 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      268 |  2772 | `		sEnv.nIdx = SXU32_HIGH;` |
|      268 |  2773 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      268 |  2774 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      268 |  2775 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      132 |  2776 | `	}` |
|        - |  2777 | `	/* Arrow functions are always closures */` |
|      274 |  2778 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|        - |  2779 | `	/* Compile the body expression as an implicit return */` |
|      409 |  2780 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      135 |  2781 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      274 |  2782 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2783 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2784 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  2785 | `		return SXERR_ABORT;` |
|        - |  2786 | `	}` |
|      274 |  2787 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      274 |  2788 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      274 |  2789 | `	pSavedEnd = pGen->pEnd;` |
|      274 |  2790 | `	pGen->pIn = pBodyStart;` |
|      274 |  2791 | `	pGen->pEnd = pBodyEnd;` |
|      274 |  2792 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      274 |  2793 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2794 | `		return SXERR_ABORT;` |
|        - |  2795 | `	}` |
|        - |  2796 | `	/* The cursor stopped just past the body expression */` |
|      274 |  2797 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  2798 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  2799 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  2800 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  2801 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      274 |  2802 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      274 |  2803 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      274 |  2804 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      274 |  2805 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      274 |  2806 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  2807 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      274 |  2808 | `	pGen->pIn = pBodyEnd;` |
|      274 |  2809 | `	pGen->pEnd = pSavedEnd;` |
|        - |  2810 | `	/* Emit the load-closure instruction */` |
|      274 |  2811 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      274 |  2812 | `	return SXRET_OK;` |
|      143 |  2813 | `}` |
|        - |  2814 | `/*` |
|        - |  2815 | ` * Compile a single arm's expression range into a freshly-allocated` |
|        - |  2816 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|        - |  2817 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|        - |  2818 | ` * expression's value.` |
|        - |  2819 | ` */` |
|      354 |  2820 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|        - |  2821 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|        3 |  2822 | `{` |
|        - |  2823 | `	SySet *pInstrContainer;` |
|        - |  2824 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  2825 | `	GenBlock *pArmBlock;` |
|        - |  2826 | `	sxi32 rc;` |
|      357 |  2827 | `	pTmpIn  = pGen->pIn;` |
|      357 |  2828 | `	pTmpEnd = pGen->pEnd;` |
|      357 |  2829 | `	pGen->pIn  = pStart;` |
|      357 |  2830 | `	pGen->pEnd = pStop;` |
|      357 |  2831 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      357 |  2832 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|        - |  2833 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|        - |  2834 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|        - |  2835 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|        - |  2836 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|        - |  2837 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|      534 |  2838 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      177 |  2839 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|      357 |  2840 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2841 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  2842 | `		pGen->pIn  = pTmpIn;` |
|      ! 0 |  2843 | `		pGen->pEnd = pTmpEnd;` |
|      ! 0 |  2844 | `		return SXERR_ABORT;` |
|        - |  2845 | `	}` |
|      357 |  2846 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      357 |  2847 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      357 |  2848 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      357 |  2849 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      357 |  2850 | `	GenStateLeaveBlock(&(*pGen),0);` |
|      357 |  2851 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      357 |  2852 | `	pGen->pIn  = pTmpIn;` |
|      357 |  2853 | `	pGen->pEnd = pTmpEnd;` |
|      357 |  2854 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2855 | `		return SXERR_ABORT;` |
|        - |  2856 | `	}` |
|      357 |  2857 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 |  2858 | `		return SXERR_EMPTY;` |
|        - |  2859 | `	}` |
|      357 |  2860 | `	return SXRET_OK;` |
|      180 |  2861 | `}` |
|        - |  2862 | `/*` |
|        - |  2863 | ` * Compile a PHP 8.0 match expression:` |
|        - |  2864 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|        - |  2865 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|        - |  2866 | ` * Strict comparison (===) is used between the subject and each condition.` |
|        - |  2867 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|        - |  2868 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|        - |  2869 | ` */` |
|        - |  2870 | `/*` |
|        - |  2871 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|        - |  2872 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|        - |  2873 | ` * caller can bail out of the current expression.` |
|        - |  2874 | ` */` |
|        2 |  2875 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|        1 |  2876 | `{` |
|        - |  2877 | `	va_list ap;` |
|        - |  2878 | `	sxi32 rc;` |
|        - |  2879 | `	SyBlob sMsg;` |
|        3 |  2880 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        3 |  2881 | `	va_start(ap,zFmt);` |
|        3 |  2882 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|        3 |  2883 | `	va_end(ap);` |
|        3 |  2884 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|        3 |  2885 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|        3 |  2886 | `	SyBlobRelease(&sMsg);` |
|        3 |  2887 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2888 | `		return SXERR_ABORT;` |
|        - |  2889 | `	}` |
|        3 |  2890 | `	return SXERR_SYNTAX;` |
|        2 |  2891 | `}` |
|        - |  2892 | `/*` |
|        - |  2893 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|        - |  2894 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|        - |  2895 | ` * Returns the stop token pointer (or pEnd if none found).` |
|        - |  2896 | ` */` |
|      356 |  2897 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|        4 |  2898 | `{` |
|      360 |  2899 | `	SyToken *pCur = pStart;` |
|      360 |  2900 | `	int iNest = 0;` |
|      838 |  2901 | `	while( pCur < pEnd ){` |
|      802 |  2902 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       13 |  2903 | `			iNest++;` |
|      796 |  2904 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       13 |  2905 | `			iNest--;` |
|      784 |  2906 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|      323 |  2907 | `			return pCur;` |
|        - |  2908 | `		}` |
|      482 |  2909 | `		pCur++;` |
|        4 |  2910 | `	}` |
|       39 |  2911 | `	return pEnd;` |
|      182 |  2912 | `}` |
|       72 |  2913 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2914 | `{` |
|        - |  2915 | `	ph7_match *pMatch;` |
|        - |  2916 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|       77 |  2917 | `	int bHasDefault = 0;` |
|        - |  2918 | `	sxu32 nLine;` |
|        - |  2919 | `	sxi32 rc;` |
|       36 |  2920 | `	SXUNUSED(iCompileFlag);` |
|       77 |  2921 | `	nLine = pGen->pIn->nLine;` |
|       77 |  2922 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|        - |  2923 | `	/* Expect '(' */` |
|       77 |  2924 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2925 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2926 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|      ! 0 |  2927 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|        - |  2928 | `	}` |
|       77 |  2929 | `	pGen->pIn++; /* Jump '(' */` |
|       77 |  2930 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|       77 |  2931 | `	if( pSubjEnd >= pGen->pEnd ){` |
|      ! 0 |  2932 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2933 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        - |  2934 | `	}` |
|       77 |  2935 | `	if( pGen->pIn >= pSubjEnd ){` |
|      ! 0 |  2936 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2937 | `			"syntax error, unexpected \")\", expecting match subject");` |
|        - |  2938 | `	}` |
|        - |  2939 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|       77 |  2940 | `	pSavedEnd = pGen->pEnd;` |
|       77 |  2941 | `	pGen->pEnd = pSubjEnd;` |
|       77 |  2942 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       77 |  2943 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2944 | `		return SXERR_ABORT;` |
|        - |  2945 | `	}` |
|       77 |  2946 | `	pGen->pEnd = pSavedEnd;` |
|       77 |  2947 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|        - |  2948 | `	/* Expect '{' */` |
|       77 |  2949 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 |  2950 | `		return GenStateMatchError(pGen,` |
|      ! 0 |  2951 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  2952 | `			"syntax error, expecting \"{\" after match subject");` |
|        - |  2953 | `	}` |
|       77 |  2954 | `	pGen->pIn++; /* Jump '{' */` |
|       77 |  2955 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|       77 |  2956 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  2957 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2958 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|        - |  2959 | `	}` |
|        - |  2960 | `	/* Allocate ph7_match container */` |
|       77 |  2961 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|       77 |  2962 | `	if( pMatch == 0 ){` |
|      ! 0 |  2963 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2964 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2965 | `		return SXERR_ABORT;` |
|        - |  2966 | `	}` |
|       77 |  2967 | `	SyZero(pMatch,sizeof(ph7_match));` |
|       77 |  2968 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|        - |  2969 | `	/* Iterate arms */` |
|      259 |  2970 | `	while( pGen->pIn < pBodyEnd ){` |
|        - |  2971 | `		ph7_match_arm sArm;` |
|        - |  2972 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|      190 |  2973 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|      190 |  2974 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|      190 |  2975 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|      190 |  2976 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  2977 | `		/* 'default' arm? */` |
|      186 |  2978 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      107 |  2979 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|       22 |  2980 | `			if( bHasDefault ){` |
|        3 |  2981 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|        - |  2982 | `					"Match expressions may only contain one default arm");` |
|        4 |  2983 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  2984 | `			}` |
|       20 |  2985 | `			sArm.bDefault = 1;` |
|       20 |  2986 | `			bHasDefault = 1;` |
|       20 |  2987 | `			pGen->pIn++;` |
|       20 |  2988 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|      ! 0 |  2989 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  2990 | `					"syntax error, expecting \"=>\" after 'default'");` |
|        - |  2991 | `			}` |
|       20 |  2992 | `			pGen->pIn++; /* Jump '=>' */` |
|       11 |  2993 | `		}else{` |
|        - |  2994 | `			/* Condition list: cond (',' cond)* '=>' */` |
|      170 |  2995 | `			pCondStart = pGen->pIn;` |
|      170 |  2996 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|        - |  2997 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|      178 |  2998 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|        - |  2999 | `				SySet sCondBc;` |
|        9 |  3000 | `				if( pCondStart >= pArrow ){` |
|      ! 0 |  3001 | `					return GenStateMatchError(pGen,nArmLine,` |
|        - |  3002 | `						"syntax error, empty match condition expression");` |
|        - |  3003 | `				}` |
|        9 |  3004 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        9 |  3005 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|        9 |  3006 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3007 | `					return SXERR_ABORT;` |
|        - |  3008 | `				}` |
|        9 |  3009 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        9 |  3010 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|        9 |  3011 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|        - |  3012 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|        1 |  3013 | `			}` |
|      170 |  3014 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  3015 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3016 | `					"syntax error, expecting \"=>\" in match arm");` |
|        - |  3017 | `			}` |
|      167 |  3018 | `			if( pCondStart >= pArrow ){` |
|      ! 0 |  3019 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3020 | `					"syntax error, empty match condition expression");` |
|        - |  3021 | `			}` |
|        - |  3022 | `			{` |
|        - |  3023 | `				SySet sCondBc;` |
|      167 |  3024 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      167 |  3025 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      167 |  3026 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3027 | `					return SXERR_ABORT;` |
|        - |  3028 | `				}` |
|      167 |  3029 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        - |  3030 | `			}` |
|      167 |  3031 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|        - |  3032 | `		}` |
|        - |  3033 | `		/* Compile result expression: up to top-level ',' or body end */` |
|      185 |  3034 | `		pResStart = pGen->pIn;` |
|      185 |  3035 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|      185 |  3036 | `		if( pResStart >= pResEnd ){` |
|      ! 0 |  3037 | `			return GenStateMatchError(pGen,nArmLine,` |
|        - |  3038 | `				"syntax error, expected expression after \"=>\"");` |
|        - |  3039 | `		}` |
|      185 |  3040 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|      185 |  3041 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3042 | `			return SXERR_ABORT;` |
|        - |  3043 | `		}` |
|      185 |  3044 | `		pGen->pIn = pResEnd;` |
|      185 |  3045 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      151 |  3046 | `			pGen->pIn++; /* Skip trailing ',' */` |
|       74 |  3047 | `		}` |
|      185 |  3048 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|        3 |  3049 | `	}` |
|       71 |  3050 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|       71 |  3051 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|       71 |  3052 | `	return SXRET_OK;` |
|       41 |  3053 | `}` |
|        - |  3054 | `/*` |
|        - |  3055 | ` * Compile a backtick quoted string.` |
|        - |  3056 | ` */` |
|        4 |  3057 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 |  3058 | `{` |
|        - |  3059 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|        - |  3060 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|        - |  3061 | `	 */` |
|        8 |  3062 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|        - |  3063 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|        2 |  3064 | `		ph7_lib_version()` |
|        - |  3065 | `		);` |
|        - |  3066 | `	/* Load NULL */` |
|        6 |  3067 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3068 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  3069 | `	/* Node successfully compiled */` |
|        6 |  3070 | `	return SXRET_OK;` |
|        2 |  3071 | `}` |
|        - |  3072 | `/*` |
|        - |  3073 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|        - |  3074 | ` * construct.` |
|        - |  3075 | ` */` |
|       82 |  3076 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3077 | `{` |
|        - |  3078 | `	SyString *pName;` |
|        - |  3079 | `	sxu32 nKeyID;` |
|        - |  3080 | `	sxi32 rc;` |
|        - |  3081 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|       87 |  3082 | `	pName = &pGen->pIn->sData;` |
|       87 |  3083 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       87 |  3084 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|       87 |  3085 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|        9 |  3086 | `		SyToken *pTmp,*pNext = 0;` |
|        - |  3087 | `		/* Compile arguments one after one */` |
|        9 |  3088 | `		pTmp = pGen->pEnd;` |
|        - |  3089 | `		/* Symisc eXtension to the PHP programming language:` |
|        - |  3090 | `		 * 'echo' can be used in the context of a function which` |
|        - |  3091 | `		 *  mean that the following expression is valid:` |
|        - |  3092 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|        - |  3093 | `		 */` |
|        9 |  3094 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|       17 |  3095 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        9 |  3096 | `			if( pGen->pIn < pNext ){` |
|        9 |  3097 | `				pGen->pEnd = pNext;` |
|        9 |  3098 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|        9 |  3099 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3100 | `					return SXERR_ABORT;` |
|        - |  3101 | `				}` |
|        9 |  3102 | `				if( rc != SXERR_EMPTY ){` |
|        - |  3103 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|        - |  3104 | `					 * without the overhead of a function call.` |
|        - |  3105 | `					 * This is a very powerful optimization that improve` |
|        - |  3106 | `					 * performance greatly.` |
|        - |  3107 | `					 */` |
|        9 |  3108 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|        4 |  3109 | `				}` |
|        4 |  3110 | `			}` |
|        - |  3111 | `			/* Jump trailing commas */` |
|        9 |  3112 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      ! 0 |  3113 | `				pNext++;` |
|      ! 0 |  3114 | `			}` |
|        9 |  3115 | `			pGen->pIn = pNext;` |
|        1 |  3116 | `		}` |
|        - |  3117 | `		/* Restore token stream */` |
|        9 |  3118 | `		pGen->pEnd = pTmp;` |
|        5 |  3119 | `	}else{` |
|       79 |  3120 | `		sxi32 nArg = 0;` |
|       79 |  3121 | `		sxu32 nIdx = 0;` |
|       79 |  3122 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       79 |  3123 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3124 | `			return SXERR_ABORT;` |
|       79 |  3125 | `		}else if(rc != SXERR_EMPTY ){` |
|       79 |  3126 | `			nArg = 1;` |
|       37 |  3127 | `		}` |
|       79 |  3128 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|        - |  3129 | `			ph7_value *pObj;` |
|        - |  3130 | `			/* Emit the call instruction */` |
|       31 |  3131 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       31 |  3132 | `			if( pObj == 0 ){` |
|      ! 0 |  3133 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3134 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3135 | `				return SXERR_ABORT;` |
|        - |  3136 | `			}` |
|       31 |  3137 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|        - |  3138 | `			/* Install in the literal table */` |
|       31 |  3139 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       13 |  3140 | `		}` |
|        - |  3141 | `		/* Emit the call instruction */` |
|       79 |  3142 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       79 |  3143 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        - |  3144 | `	}` |
|        - |  3145 | `	/* Node successfully compiled */` |
|       87 |  3146 | `	return SXRET_OK;` |
|       46 |  3147 | `}` |
|        - |  3148 | `/*` |
|        - |  3149 | ` * Compile a node holding a variable declaration.` |
|        - |  3150 | ` * According to the PHP language reference` |
|        - |  3151 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|        - |  3152 | ` *  The variable name is case-sensitive.` |
|        - |  3153 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|        - |  3154 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3155 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|        - |  3156 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|        - |  3157 | ` *  Note: $this is a special variable that can't be assigned.` |
|        - |  3158 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|        - |  3159 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|        - |  3160 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|        - |  3161 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|        - |  3162 | ` *  the chapter on Expressions.` |
|        - |  3163 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|        - |  3164 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|        - |  3165 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|        - |  3166 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|        - |  3167 | ` *  is being assigned (the source variable).` |
|        - |  3168 | ` */` |
|  8787764 |  3169 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3170 | `{` |
|  8787769 |  3171 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3172 | `	sxi32 iVv;` |
|        - |  3173 | `	sxi32 iP1;` |
|        - |  3174 | `	void *p3;` |
|        - |  3175 | `	sxi32 rc;` |
|  8787769 |  3176 | `	iVv = -1; /* Variable variable counter */` |
| 17575545 |  3177 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  8787781 |  3178 | `		pGen->pIn++;` |
|  8787781 |  3179 | `		iVv++;` |
|        5 |  3180 | `	}` |
|  8787769 |  3181 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - |  3182 | `		/* Invalid variable name */` |
|      ! 0 |  3183 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|      ! 0 |  3184 | `		if( rc == SXERR_ABORT ){` |
|        - |  3185 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3186 | `			return SXERR_ABORT;` |
|        - |  3187 | `		}` |
|      ! 0 |  3188 | `		return SXRET_OK;` |
|        - |  3189 | `	}` |
|  8787769 |  3190 | `	p3  = 0;` |
|  8787769 |  3191 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|        - |  3192 | `		/* Dynamic variable creation */` |
|       21 |  3193 | `		pGen->pIn++;  /* Jump the open curly */` |
|       21 |  3194 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|       21 |  3195 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3196 | `			/* Empty expression */` |
|        3 |  3197 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|        3 |  3198 | `			return SXRET_OK;` |
|        - |  3199 | `		}` |
|        - |  3200 | `		/* Compile the expression holding the variable name */` |
|       18 |  3201 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       18 |  3202 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3203 | `			return SXERR_ABORT;` |
|       18 |  3204 | `		}else if( rc == SXERR_EMPTY ){` |
|        3 |  3205 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|        3 |  3206 | `			return SXRET_OK;` |
|        - |  3207 | `		}` |
|        8 |  3208 | `	}else{` |
|        - |  3209 | `		SyHashEntry *pEntry;` |
|        - |  3210 | `		SyString *pName;` |
|  8787751 |  3211 | `		char *zName = 0;` |
|        - |  3212 | `		/* Extract variable name */` |
|  8787751 |  3213 | `		pName = &pGen->pIn->sData;` |
|        - |  3214 | `		/* Advance the stream cursor */` |
|  8787751 |  3215 | `		pGen->pIn++;` |
|  8787751 |  3216 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  8787751 |  3217 | `		if( pEntry == 0 ){` |
|        - |  3218 | `			/* Duplicate name */` |
|   562823 |  3219 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   562823 |  3220 | `			if( zName == 0 ){` |
|      ! 0 |  3221 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3222 | `				return SXERR_ABORT;` |
|        - |  3223 | `			}` |
|        - |  3224 | `			/* Install in the hashtable */` |
|   562823 |  3225 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   281414 |  3226 | `		}else{` |
|        - |  3227 | `			/* Name already available */` |
|  8224933 |  3228 | `			zName = (char *)pEntry->pUserData;` |
|        - |  3229 | `		}` |
|  8787751 |  3230 | `		p3 = (void *)zName;` |
|        - |  3231 | `	}` |
|  8787765 |  3232 | `	iP1 = 0;` |
|  8787765 |  3233 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  2648189 |  3234 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - |  3235 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  2648171 |  3236 | `			iP1 = 1;` |
|  1324083 |  3237 | `		}` |
|  1324092 |  3238 | `	}` |
|        - |  3239 | `	/* Emit the load instruction */` |
|  8787765 |  3240 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  8787777 |  3241 | `	while( iVv > 0 ){` |
|       13 |  3242 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|       13 |  3243 | `		iVv--;` |
|        1 |  3244 | `	}` |
|        - |  3245 | `	/* Node successfully compiled */` |
|  8787765 |  3246 | `	return SXRET_OK;` |
|  4393887 |  3247 | `}` |
|        - |  3248 | `/*` |
|        - |  3249 | ` * Load a literal.` |
|        - |  3250 | ` */` |
|  5573480 |  3251 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 |  3252 | `{` |
|  5573485 |  3253 | `	SyToken *pToken = pGen->pIn;` |
|        - |  3254 | `	ph7_value *pObj;` |
|        - |  3255 | `	SyString *pStr;` |
|        - |  3256 | `	sxu32 nIdx;` |
|        - |  3257 | `	/* Extract token value */` |
|  5573485 |  3258 | `	pStr = &pToken->sData;` |
|        - |  3259 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  5573485 |  3260 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  1347431 |  3261 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - |  3262 | `			/* NULL constant are always indexed at 0 */` |
|   552427 |  3263 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   552427 |  3264 | `			return SXRET_OK;` |
|   795009 |  3265 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - |  3266 | `			/* TRUE constant are always indexed at 1 */` |
|   148575 |  3267 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   148575 |  3268 | `			return SXRET_OK;` |
|        5 |  3269 | `		}` |
|  5023060 |  3270 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   947568 |  3271 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - |  3272 | `			/* FALSE constant are always indexed at 2 */` |
|   400695 |  3273 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   400695 |  3274 | `			return SXRET_OK;` |
|  4107759 |  3275 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   564780 |  3276 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - |  3277 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    11663 |  3278 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    11663 |  3279 | `			if( pObj == 0 ){` |
|      ! 0 |  3280 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3281 | `				return SXERR_ABORT;` |
|        - |  3282 | `			}` |
|    11663 |  3283 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - |  3284 | `			/* Emit the load constant instruction */` |
|    11663 |  3285 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    11663 |  3286 | `			return SXRET_OK;` |
|  3843135 |  3287 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    58848 |  3288 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|        - |  3289 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|        8 |  3290 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        8 |  3291 | `			if( pObj == 0 ){` |
|      ! 0 |  3292 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3293 | `				return SXERR_ABORT;` |
|        - |  3294 | `			}` |
|        8 |  3295 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - |  3296 | `				SyString sNs;` |
|        8 |  3297 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  3298 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|        5 |  3299 | `			}else{` |
|      ! 0 |  3300 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  3301 | `			}` |
|        8 |  3302 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        8 |  3303 | `			return SXRET_OK;` |
|  3835587 |  3304 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   151991 |  3305 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  3921905 |  3306 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   216424 |  3307 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|       11 |  3308 | `			GenBlock *pBlock = pGen->pCurrent;` |
|        - |  3309 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       21 |  3310 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|        - |  3311 | `				/* Point to the upper block */` |
|       11 |  3312 | `				pBlock = pBlock->pParent;` |
|        1 |  3313 | `			}` |
|       11 |  3314 | `			if( pBlock == 0 ){` |
|        - |  3315 | `				/* Called in the global scope,load NULL */` |
|        5 |  3316 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        3 |  3317 | `			}else{` |
|        - |  3318 | `				/* Extract the target function/method */` |
|        7 |  3319 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        7 |  3320 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        - |  3321 | `					/* Not a class method,Load null */` |
|        3 |  3322 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3323 | `				}else{` |
|        5 |  3324 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        5 |  3325 | `					if( pObj == 0 ){` |
|      ! 0 |  3326 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3327 | `						return SXERR_ABORT;` |
|        - |  3328 | `					}` |
|        5 |  3329 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|        - |  3330 | `					/* Emit the load constant instruction */` |
|        5 |  3331 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  3332 | `				}` |
|        - |  3333 | `			}` |
|       11 |  3334 | `			return SXRET_OK;` |
|        - |  3335 | `	}` |
|        - |  3336 | `	/* Query literal table */` |
|  4460129 |  3337 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|        - |  3338 | `		ph7_value *pLitObj;` |
|        - |  3339 | `		/* Unknown literal,install it in the literal table */` |
|   908191 |  3340 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   908191 |  3341 | `		if( pLitObj == 0 ){` |
|      ! 0 |  3342 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3343 | `			return SXERR_ABORT;` |
|        - |  3344 | `		}` |
|   908191 |  3345 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   908191 |  3346 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   454093 |  3347 | `	}` |
|        - |  3348 | `	/* Emit the load constant instruction */` |
|  4460129 |  3349 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  4460129 |  3350 | `	return SXRET_OK;` |
|  2786745 |  3351 | `}` |
|        - |  3352 | `/*` |
|        - |  3353 | ` * Resolve a namespace path or simply load a literal.` |
|        - |  3354 | ` * If the token stream contains namespace separators (backslashes),` |
|        - |  3355 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - |  3356 | ` * Otherwise, load the simple literal directly.` |
|        - |  3357 | ` */` |
|  5577412 |  3358 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 |  3359 | `{` |
|        - |  3360 | `	sxi32 rc;` |
|  5577417 |  3361 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  3362 | `		return SXRET_OK;` |
|        - |  3363 | `	}` |
|        - |  3364 | `	/* Check if this is a multi-token namespace path */` |
|  5577417 |  3365 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - |  3366 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     3937 |  3367 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     3937 |  3368 | `		int isAbsolute = 0;` |
|     3937 |  3369 | `		SyBlobReset(pWorker);` |
|        - |  3370 | `		/* Check for leading backslash (absolute path) */` |
|     3937 |  3371 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     3935 |  3372 | `			isAbsolute = 1;` |
|     3935 |  3373 | `			pGen->pIn++; /* Skip leading backslash */` |
|     1965 |  3374 | `		}` |
|        - |  3375 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|     3937 |  3376 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        3 |  3377 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 |  3378 | `			SyBlobAppend(pWorker,"\\",1);` |
|        1 |  3379 | `		}` |
|        - |  3380 | `		/* Collect all path components */` |
|     4045 |  3381 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4045 |  3382 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       58 |  3383 | `				SyBlobAppend(pWorker,"\\",1);` |
|       31 |  3384 | `			}else{` |
|     3991 |  3385 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  3386 | `			}` |
|     4045 |  3387 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     3937 |  3388 | `				pGen->pIn++;` |
|     3937 |  3389 | `				break;` |
|        - |  3390 | `			}` |
|      112 |  3391 | `			pGen->pIn++;` |
|        4 |  3392 | `		}` |
|     3937 |  3393 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - |  3394 | `			ph7_value *pObj;` |
|        - |  3395 | `			SyString sPath;` |
|        - |  3396 | `			sxu32 nIdx;` |
|     3937 |  3397 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - |  3398 | `			/* Install in the literal table */` |
|     3937 |  3399 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     3909 |  3400 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3909 |  3401 | `				if( pObj == 0 ){` |
|      ! 0 |  3402 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3403 | `					return SXERR_ABORT;` |
|        - |  3404 | `				}` |
|     3909 |  3405 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     3909 |  3406 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1952 |  3407 | `			}` |
|        - |  3408 | `			/* Emit the load constant instruction.` |
|        - |  3409 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - |  3410 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     5903 |  3411 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     1966 |  3412 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     1966 |  3413 | `				nIdx,0,0);` |
|     3937 |  3414 | `			return SXRET_OK;` |
|        - |  3415 | `		}` |
|      ! 0 |  3416 | `	}` |
|        - |  3417 | `	/* Single-token literal: load directly */` |
|  5573485 |  3418 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  5573485 |  3419 | `	return rc;` |
|  2788711 |  3420 | `}` |
|        - |  3421 | `/*` |
|        - |  3422 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|        - |  3423 | ` */` |
|        - |  3424 | `/*` |
|        - |  3425 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|        - |  3426 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|        - |  3427 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|        - |  3428 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|        - |  3429 | ` */` |
|      ! 0 |  3430 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|      ! 0 |  3431 | `{` |
|      ! 0 |  3432 | `	SXUNUSED(iCompileFlag);` |
|      ! 0 |  3433 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - |  3434 | `		"Cannot use the first-class callable syntax '...' here");` |
|      ! 0 |  3435 | `	return SXERR_SYNTAX;` |
|      ! 0 |  3436 | `}` |
|  5577412 |  3437 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3438 | `{` |
|        - |  3439 | `	sxi32 rc;` |
|  5577417 |  3440 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  5577417 |  3441 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3442 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3443 | `		return rc;` |
|        - |  3444 | `	}` |
|        - |  3445 | `	/* Node successfully compiled */` |
|  5577417 |  3446 | `	return SXRET_OK;` |
|  2788711 |  3447 | `}` |
|        - |  3448 | `/*` |
|        - |  3449 | ` * Recover from a compile-time error. In other words synchronize` |
|        - |  3450 | ` * the token stream cursor with the first semi-colon seen.` |
|        - |  3451 | ` */` |
|        8 |  3452 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|        1 |  3453 | `{` |
|        - |  3454 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       17 |  3455 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|        9 |  3456 | `		pGen->pIn++;` |
|        1 |  3457 | `	}` |
|        9 |  3458 | `	return SXRET_OK;` |
|        1 |  3459 | `}` |
|        - |  3460 | `/*` |
|        - |  3461 | ` * Check if the given identifier name is reserved or not.` |
|        - |  3462 | ` * Return TRUE if reserved.FALSE otherwise.` |
|        - |  3463 | ` */` |
|   143922 |  3464 | `static int GenStateIsReservedConstant(SyString *pName)` |
|        5 |  3465 | `{` |
|   143927 |  3466 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|       48 |  3467 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|        3 |  3468 | `			return TRUE;` |
|       46 |  3469 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|        6 |  3470 | `			return TRUE;` |
|        3 |  3471 | `		}` |
|   143902 |  3472 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       18 |  3473 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|        3 |  3474 | `			return TRUE;` |
|        - |  3475 | `		}` |
|        7 |  3476 | `	}` |
|        - |  3477 | `	/* Not a reserved constant */` |
|   143919 |  3478 | `	return FALSE;` |
|    71966 |  3479 | `}` |
|        - |  3480 | `/*` |
|        - |  3481 | ` * Compile the 'const' statement.` |
|        - |  3482 | ` * According to the PHP language reference` |
|        - |  3483 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|        - |  3484 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|        - |  3485 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|        - |  3486 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|        - |  3487 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3488 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|        - |  3489 | ` *  Syntax` |
|        - |  3490 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|        - |  3491 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|        - |  3492 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|        - |  3493 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|        - |  3494 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|        - |  3495 | ` *  to get a list of all defined constants.` |
|        - |  3496 | ` *` |
|        - |  3497 | ` * Symisc eXtension.` |
|        - |  3498 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|        - |  3499 | ` *  would allow only simple scalar value.` |
|        - |  3500 | ` *  Example` |
|        - |  3501 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  3502 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  3503 | ` */` |
|       44 |  3504 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |  3505 | `{` |
|        - |  3506 | `	SySet *pConsCode,*pInstrContainer;` |
|       49 |  3507 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3508 | `	SyString *pName;` |
|        - |  3509 | `	sxi32 rc;` |
|       49 |  3510 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       49 |  3511 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  3512 | `		/* Invalid constant name */` |
|        8 |  3513 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|        8 |  3514 | `		if( rc == SXERR_ABORT ){` |
|        - |  3515 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3516 | `			return SXERR_ABORT;` |
|        - |  3517 | `		}` |
|        8 |  3518 | `		goto Synchronize;` |
|        - |  3519 | `	}` |
|        - |  3520 | `	/* Peek constant name */` |
|       43 |  3521 | `	pName = &pGen->pIn->sData;` |
|        - |  3522 | `	/* Make sure the constant name isn't reserved */` |
|       43 |  3523 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  3524 | `		/* Reserved constant */` |
|       10 |  3525 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       10 |  3526 | `		if( rc == SXERR_ABORT ){` |
|        - |  3527 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3528 | `			return SXERR_ABORT;` |
|        - |  3529 | `		}` |
|       10 |  3530 | `		goto Synchronize;` |
|        - |  3531 | `	}` |
|       34 |  3532 | `	pGen->pIn++;` |
|       34 |  3533 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  3534 | `		/* Invalid statement*/` |
|        6 |  3535 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|        6 |  3536 | `		if( rc == SXERR_ABORT ){` |
|        - |  3537 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3538 | `			return SXERR_ABORT;` |
|        - |  3539 | `		}` |
|        6 |  3540 | `		goto Synchronize;` |
|        - |  3541 | `	}` |
|       28 |  3542 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |  3543 | `	/* Allocate a new constant value container */` |
|       28 |  3544 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       28 |  3545 | `	if( pConsCode == 0 ){` |
|      ! 0 |  3546 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3547 | `		return SXERR_ABORT;` |
|        - |  3548 | `	}` |
|       28 |  3549 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  3550 | `	/* Swap bytecode container */` |
|       28 |  3551 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       28 |  3552 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |  3553 | `	/* Compile constant value */` |
|       28 |  3554 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  3555 | `	/* Emit the done instruction */` |
|       28 |  3556 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       28 |  3557 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       28 |  3558 | `	if( rc == SXERR_ABORT ){` |
|        - |  3559 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  3560 | `		return SXERR_ABORT;` |
|        - |  3561 | `	}` |
|       28 |  3562 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |  3563 | `	/* Register the constant with namespace-qualified name */` |
|        - |  3564 | `	{` |
|        - |  3565 | `		SyBlob sFQN;` |
|        - |  3566 | `		SyString sFQNStr;` |
|       28 |  3567 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       28 |  3568 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       28 |  3569 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       41 |  3570 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|       26 |  3571 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|       28 |  3572 | `		SyBlobRelease(&sFQN);` |
|        - |  3573 | `	}` |
|       28 |  3574 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3575 | `		SySetRelease(pConsCode);` |
|      ! 0 |  3576 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  3577 | `	}` |
|       28 |  3578 | `	return SXRET_OK;` |
|        9 |  3579 | `Synchronize:` |
|        - |  3580 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       60 |  3581 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       42 |  3582 | `		pGen->pIn++;` |
|        4 |  3583 | `	}` |
|       22 |  3584 | `	return SXRET_OK;` |
|       27 |  3585 | `}` |
|        - |  3586 | `/*` |
|        - |  3587 | ` * Compile the 'continue' statement.` |
|        - |  3588 | ` * According to the PHP language reference` |
|        - |  3589 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  3590 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  3591 | ` *  iteration.` |
|        - |  3592 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  3593 | ` *  the purposes of continue.` |
|        - |  3594 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  3595 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  3596 | ` *  Note:` |
|        - |  3597 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  3598 | ` */` |
|        - |  3599 | `/*` |
|        - |  3600 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|        - |  3601 | ` * block and the target loop block. This ensures finally blocks run when` |
|        - |  3602 | ` * break/continue crosses a try boundary.` |
|        - |  3603 | ` *` |
|        - |  3604 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|        - |  3605 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|        - |  3606 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|        - |  3607 | ` */` |
|    58412 |  3608 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  3609 | `{` |
|    58417 |  3610 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    58417 |  3611 | `	int nInlineTry = 0;` |
|   272279 |  3612 | `	while( pBlock && pBlock != pTarget ){` |
|   213867 |  3613 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        6 |  3614 | `			if( pBlock->pUserData ){` |
|        - |  3615 | `				/* A try block with an exception context. In a generator its catch/finally` |
|        - |  3616 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|        - |  3617 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|        - |  3618 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|        6 |  3619 | `				if( pGen->bInGenerator ){` |
|        3 |  3620 | `					nInlineTry++;` |
|        2 |  3621 | `				}else{` |
|        3 |  3622 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|        - |  3623 | `				}` |
|        4 |  3624 | `			}else{` |
|        - |  3625 | `				/* A catch/finally block compiled into a separate bytecode container` |
|        - |  3626 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|      ! 0 |  3627 | `				break;` |
|        - |  3628 | `			}` |
|        2 |  3629 | `		}` |
|   213867 |  3630 | `		pBlock = pBlock->pParent;` |
|        5 |  3631 | `	}` |
|    58417 |  3632 | `	return nInlineTry;` |
|        5 |  3633 | `}` |
|    27238 |  3634 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  3635 | `{` |
|        - |  3636 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3637 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3638 | `	sxu32 nLineLocal;` |
|        - |  3639 | `	sxi32 rc;` |
|    27243 |  3640 | `	nLineLocal = pGen->pIn->nLine;` |
|    27243 |  3641 | `	iLevel = 0;` |
|        - |  3642 | `	/* Jump the 'continue' keyword */` |
|    27243 |  3643 | `	pGen->pIn++;` |
|    27243 |  3644 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3645 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3646 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3647 | `		 */` |
|        - |  3648 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  3649 | `		char *zAlloc = 0;` |
|        - |  3650 | `		SyString sNum;` |
|       17 |  3651 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  3652 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3653 | `			return SXERR_ABORT;` |
|        - |  3654 | `		}` |
|       17 |  3655 | `		if( rc == SXRET_OK ){` |
|       20 |  3656 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3657 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  3658 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3659 | `				return SXERR_ABORT;` |
|        - |  3660 | `			}` |
|       14 |  3661 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  3662 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3663 | `		}` |
|       17 |  3664 | `		if( iLevel < 2 ){` |
|        3 |  3665 | `			iLevel = 0;` |
|        1 |  3666 | `		}` |
|       17 |  3667 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3668 | `	}` |
|        - |  3669 | `	/* Point to the target loop */` |
|    27243 |  3670 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    27243 |  3671 | `	if( pLoop == 0 ){` |
|        - |  3672 | `		/* Illegal continue */` |
|       12 |  3673 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|       12 |  3674 | `		if( rc == SXERR_ABORT ){` |
|        - |  3675 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3676 | `			return SXERR_ABORT;` |
|        - |  3677 | `		}` |
|        7 |  3678 | `	}else{` |
|    27233 |  3679 | `		sxu32 nInstrIdx = 0;` |
|        - |  3680 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    27233 |  3681 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3682 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  3683 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    27233 |  3684 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    27233 |  3685 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  3686 | `			/* According to the PHP language reference manual` |
|        - |  3687 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|        - |  3688 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|        - |  3689 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|        - |  3690 | `			 */` |
|        5 |  3691 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|        5 |  3692 | `			if( rc == SXRET_OK ){` |
|        5 |  3693 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  3694 | `			}` |
|        3 |  3695 | `		}else{` |
|        - |  3696 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    27229 |  3697 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    27229 |  3698 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  3699 | `				JumpFixup sJumpFix;` |
|        - |  3700 | `				/* Post-continue */` |
|       14 |  3701 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       14 |  3702 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       14 |  3703 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|        6 |  3704 | `			}` |
|        - |  3705 | `		}` |
|        - |  3706 | `	}` |
|    27243 |  3707 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3708 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3709 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  3710 | `	}` |
|        - |  3711 | `	/* Statement successfully compiled */` |
|    27243 |  3712 | `	return SXRET_OK;` |
|    13624 |  3713 | `}` |
|        - |  3714 | `/*` |
|        - |  3715 | ` * Compile the 'break' statement.` |
|        - |  3716 | ` * According to the PHP language reference` |
|        - |  3717 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  3718 | ` *  structure.` |
|        - |  3719 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  3720 | ` *  enclosing structures are to be broken out of.` |
|        - |  3721 | ` */` |
|    31200 |  3722 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  3723 | `{` |
|        - |  3724 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3725 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3726 | `	sxi32 rc;` |
|    31205 |  3727 | `	iLevel = 0;` |
|        - |  3728 | `	/* Jump the 'break' keyword */` |
|    31205 |  3729 | `	pGen->pIn++;` |
|    31205 |  3730 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3731 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3732 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3733 | `		 */` |
|        - |  3734 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       18 |  3735 | `		char *zAlloc = 0;` |
|        - |  3736 | `		SyString sNum;` |
|       18 |  3737 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       18 |  3738 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3739 | `			return SXERR_ABORT;` |
|        - |  3740 | `		}` |
|       18 |  3741 | `		if( rc == SXRET_OK ){` |
|       21 |  3742 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3743 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       15 |  3744 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3745 | `				return SXERR_ABORT;` |
|        - |  3746 | `			}` |
|       15 |  3747 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       15 |  3748 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3749 | `		}` |
|       18 |  3750 | `		if( iLevel < 2 ){` |
|        3 |  3751 | `			iLevel = 0;` |
|        1 |  3752 | `		}` |
|       18 |  3753 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3754 | `	}` |
|        - |  3755 | `	/* Extract the target loop */` |
|    31205 |  3756 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    31205 |  3757 | `	if( pLoop == 0 ){` |
|        - |  3758 | `		/* Illegal break */` |
|       19 |  3759 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|       19 |  3760 | `		if( rc == SXERR_ABORT ){` |
|        - |  3761 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3762 | `			return SXERR_ABORT;` |
|        - |  3763 | `		}` |
|       11 |  3764 | `	}else{` |
|        - |  3765 | `		sxu32 nInstrIdx;` |
|        - |  3766 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    31189 |  3767 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3768 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    31189 |  3769 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    31189 |  3770 | `		if( rc == SXRET_OK ){` |
|        - |  3771 | `			/* Fix the jump later when the jump destination is resolved */` |
|    31189 |  3772 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    15592 |  3773 | `		}` |
|        - |  3774 | `	}` |
|    31205 |  3775 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3776 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3777 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  3778 | `	}` |
|        - |  3779 | `	/* Statement successfully compiled */` |
|    31205 |  3780 | `	return SXRET_OK;` |
|    15605 |  3781 | `}` |
|        - |  3782 | `/*` |
|        - |  3783 | ` * Compile or record a label.` |
|        - |  3784 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  3785 | ` * Example` |
|        - |  3786 | ` *  goto LABEL;` |
|        - |  3787 | ` *   echo 'Foo';` |
|        - |  3788 | ` *  LABEL:` |
|        - |  3789 | ` *   echo 'Bar';` |
|        - |  3790 | ` */` |
|      112 |  3791 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  3792 | `{` |
|        - |  3793 | `	GenBlock *pBlock;` |
|        - |  3794 | `	Label sLabel;` |
|        - |  3795 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|      117 |  3796 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|      117 |  3797 | `	if( pBlock ){` |
|        - |  3798 | `		sxi32 rc;` |
|        8 |  3799 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        4 |  3800 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|        6 |  3801 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3802 | `			return SXERR_ABORT;` |
|        - |  3803 | `		}` |
|        4 |  3804 | `	}else{` |
|      113 |  3805 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3806 | `		char *zDup;` |
|        - |  3807 | `		/* Initialize label fields */` |
|      113 |  3808 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  3809 | `		/* Duplicate label name */` |
|      113 |  3810 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      113 |  3811 | `		if( zDup == 0 ){` |
|      ! 0 |  3812 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3813 | `			return SXERR_ABORT;` |
|        - |  3814 | `		}` |
|      113 |  3815 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      113 |  3816 | `		sLabel.bRef  = FALSE;` |
|      113 |  3817 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      113 |  3818 | `		pBlock = pGen->pCurrent;` |
|      221 |  3819 | `		while( pBlock ){` |
|      133 |  3820 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       24 |  3821 | `				break;` |
|        - |  3822 | `			}` |
|        - |  3823 | `			/* Point to the upper block */` |
|      113 |  3824 | `			pBlock = pBlock->pParent;` |
|        5 |  3825 | `		}` |
|      113 |  3826 | `		if( pBlock ){` |
|       24 |  3827 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       14 |  3828 | `		}else{` |
|       93 |  3829 | `			sLabel.pFunc = 0;` |
|        - |  3830 | `		}` |
|        - |  3831 | `		/* Insert in label set */` |
|      113 |  3832 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  3833 | `	}` |
|      117 |  3834 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      117 |  3835 | `	return SXRET_OK;` |
|       61 |  3836 | `}` |
|        - |  3837 | `/*` |
|        - |  3838 | ` * Compile the so hated 'goto' statement.` |
|        - |  3839 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  3840 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  3841 | ` * a compiler it has to do this.` |
|        - |  3842 | ` * According to the PHP language reference manual` |
|        - |  3843 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  3844 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  3845 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  3846 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  3847 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  3848 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  3849 | ` *   of a multi-level break` |
|        - |  3850 | ` */` |
|      152 |  3851 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  3852 | `{` |
|        - |  3853 | `	JumpFixup sJump;` |
|        - |  3854 | `	sxi32 rc;` |
|      157 |  3855 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      157 |  3856 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3857 | `		/* Missing label */` |
|      ! 0 |  3858 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  3859 | `		if( rc == SXERR_ABORT ){` |
|        - |  3860 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3861 | `			return SXERR_ABORT;` |
|        - |  3862 | `		}` |
|      ! 0 |  3863 | `		return SXRET_OK;` |
|        - |  3864 | `	}` |
|      157 |  3865 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        6 |  3866 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|        6 |  3867 | `		if( rc == SXERR_ABORT ){` |
|        - |  3868 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3869 | `			return SXERR_ABORT;` |
|        - |  3870 | `		}` |
|        4 |  3871 | `	}else{` |
|      153 |  3872 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3873 | `		GenBlock *pBlock;` |
|        - |  3874 | `		char *zDup;` |
|        - |  3875 | `		/* Prepare the jump destination */` |
|      153 |  3876 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      153 |  3877 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  3878 | `		/* Duplicate label name */` |
|      153 |  3879 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      153 |  3880 | `		if( zDup == 0 ){` |
|      ! 0 |  3881 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3882 | `			return SXERR_ABORT;` |
|        - |  3883 | `		}` |
|      153 |  3884 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|      153 |  3885 | `		pBlock = pGen->pCurrent;` |
|      315 |  3886 | `		while( pBlock ){` |
|      199 |  3887 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       37 |  3888 | `				break;` |
|        - |  3889 | `			}` |
|        - |  3890 | `			/* Point to the upper block */` |
|      167 |  3891 | `			pBlock = pBlock->pParent;` |
|        5 |  3892 | `		}` |
|      153 |  3893 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        9 |  3894 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|        9 |  3895 | `			if( rc == SXERR_ABORT ){` |
|        - |  3896 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  3897 | `				return SXERR_ABORT;` |
|        - |  3898 | `			}` |
|        3 |  3899 | `		}` |
|      153 |  3900 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       30 |  3901 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       17 |  3902 | `		}else{` |
|      127 |  3903 | `			sJump.pFunc = 0;` |
|        - |  3904 | `		}` |
|        - |  3905 | `		/* Emit the unconditional jump */` |
|      153 |  3906 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      153 |  3907 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|       74 |  3908 | `		}` |
|        - |  3909 | `	}` |
|      157 |  3910 | `	pGen->pIn++; /* Jump the label name */` |
|      157 |  3911 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  3912 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  3913 | `	}` |
|        - |  3914 | `	/* Statement successfully compiled */` |
|      157 |  3915 | `	return SXRET_OK;` |
|       81 |  3916 | `}` |
|        - |  3917 | `/*` |
|        - |  3918 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  3919 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  3920 | ` * failure.` |
|        - |  3921 | ` */` |
|       20 |  3922 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  3923 | `{` |
|        - |  3924 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  3925 | `	sxu32 nRawObj;` |
|       10 |  3926 | `	sxu32 nObjIdx;` |
|        - |  3927 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  3928 | `	 * a PHP block.` |
|        - |  3929 | `	 */` |
|       10 |  3930 | `Consume:` |
|       22 |  3931 | `	nRawObj = nObjIdx = 0;` |
|       22 |  3932 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  3933 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  3934 | `		if( pRawObj == 0 ){` |
|      ! 0 |  3935 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3936 | `			return SXERR_ABORT;` |
|        - |  3937 | `		}` |
|        - |  3938 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  3939 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  3940 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  3941 | `		++nRawObj;` |
|      ! 0 |  3942 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  3943 | `	}` |
|       22 |  3944 | `	if( nRawObj > 0 ){` |
|        - |  3945 | `		/* Emit the consume instruction */` |
|      ! 0 |  3946 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  3947 | `	}` |
|       22 |  3948 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  3949 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  3950 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  3951 | `		SySetReset(pTokenSet);` |
|      ! 0 |  3952 | `		SySetReset(&pGen->aTrivia);` |
|        - |  3953 | `		/* Tokenize input */` |
|      ! 0 |  3954 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  3955 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  3956 | `		/* Point to the fresh token stream */` |
|      ! 0 |  3957 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  3958 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  3959 | `		/* Advance the stream cursor */` |
|      ! 0 |  3960 | `		pGen->pRawIn++;` |
|        - |  3961 | `		/* TICKET 1433-011 */` |
|      ! 0 |  3962 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  3963 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  3964 | `			sxi32 rc;` |
|        - |  3965 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  3966 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  3967 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  3968 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|      ! 0 |  3969 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  3970 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3971 | `				return SXERR_ABORT;` |
|      ! 0 |  3972 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  3973 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  3974 | `			}` |
|      ! 0 |  3975 | `			goto Consume;` |
|        - |  3976 | `		}` |
|      ! 0 |  3977 | `	}else{` |
|        - |  3978 | `		/* No more chunks to process */` |
|       22 |  3979 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  3980 | `		return SXERR_EOF;` |
|        - |  3981 | `	}` |
|      ! 0 |  3982 | `	return SXRET_OK;` |
|       12 |  3983 | `}` |
|        - |  3984 | `/*` |
|        - |  3985 | ` * Compile a PHP block.` |
|        - |  3986 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  3987 | ` * optionally delimited by braces {}.` |
|        - |  3988 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  3989 | ` * and this function takes care of generating the appropriate error` |
|        - |  3990 | ` * message.` |
|        - |  3991 | ` */` |
|  3009176 |  3992 | `static sxi32 PH7_CompileBlock(` |
|        - |  3993 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  3994 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  3995 | `	)` |
|        5 |  3996 | `{` |
|        - |  3997 | `	sxi32 rc;` |
|        - |  3998 | `	sxu32 nLine;` |
|  3009181 |  3999 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  3007723 |  4000 | `		nLine = pGen->pIn->nLine;` |
|  3007723 |  4001 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  3007723 |  4002 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4003 | `			return SXERR_ABORT;` |
|        - |  4004 | `		}` |
|  3007723 |  4005 | `		pGen->pIn++;` |
|        - |  4006 | `		/* Compile until we hit the closing braces '}' */` |
|  4401103 |  4007 | `		for(;;){` |
|  8802211 |  4008 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  4009 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  4010 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4011 | `			 	   return SXERR_ABORT;` |
|        - |  4012 | `				}` |
|       22 |  4013 | `				if( rc == SXERR_EOF ){` |
|        - |  4014 | `					/* No more token to process. Missing closing braces */` |
|       22 |  4015 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|       22 |  4016 | `					break;` |
|        - |  4017 | `				}` |
|      ! 0 |  4018 | `			}` |
|  8802191 |  4019 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  4020 | `				/* Closing braces found,break immediately*/` |
|  3007703 |  4021 | `				pGen->pIn++;` |
|  3007703 |  4022 | `				break;` |
|        - |  4023 | `			}` |
|        - |  4024 | `			/* Compile a single statement */` |
|  5794493 |  4025 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  5794493 |  4026 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4027 | `				return SXERR_ABORT;` |
|        - |  4028 | `			}` |
|        5 |  4029 | `		}` |
|  3007723 |  4030 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  1505322 |  4031 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      ! 0 |  4032 | `		pGen->pIn++;` |
|      ! 0 |  4033 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      ! 0 |  4034 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4035 | `			return SXERR_ABORT;` |
|        - |  4036 | `		}` |
|        - |  4037 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      ! 0 |  4038 | `		for(;;){` |
|      ! 0 |  4039 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  4040 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  4041 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4042 | `			 	   return SXERR_ABORT;` |
|        - |  4043 | `				}` |
|      ! 0 |  4044 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  4045 | `					/* No more token to process */` |
|      ! 0 |  4046 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  4047 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  4048 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  4049 | `					}` |
|      ! 0 |  4050 | `					break;` |
|        - |  4051 | `				}` |
|      ! 0 |  4052 | `			}` |
|      ! 0 |  4053 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  4054 | `				sxi32 nKwrd;` |
|        - |  4055 | `				/* Keyword found */` |
|      ! 0 |  4056 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  4057 | `				if( nKwrd == nKeywordEnd \|\|` |
|      ! 0 |  4058 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  4059 | `						/* Delimiter keyword found,break */` |
|      ! 0 |  4060 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      ! 0 |  4061 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      ! 0 |  4062 | `						}` |
|      ! 0 |  4063 | `						break;` |
|        - |  4064 | `				}` |
|      ! 0 |  4065 | `			}` |
|        - |  4066 | `			/* Compile a single statement */` |
|      ! 0 |  4067 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      ! 0 |  4068 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4069 | `				return SXERR_ABORT;` |
|        - |  4070 | `			}` |
|      ! 0 |  4071 | `		}` |
|      ! 0 |  4072 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  4073 | `	}else{` |
|        - |  4074 | `		/* Compile a single statement */` |
|     1463 |  4075 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     1463 |  4076 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4077 | `			return SXERR_ABORT;` |
|        - |  4078 | `		}` |
|        - |  4079 | `	}` |
|        - |  4080 | `	/* Jump trailing semi-colons ';' */` |
|  3009181 |  4081 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4082 | `		pGen->pIn++;` |
|      ! 0 |  4083 | `	}` |
|  3009181 |  4084 | `	return SXRET_OK;` |
|  1504593 |  4085 | `}` |
|        - |  4086 | `/*` |
|        - |  4087 | ` * Compile the gentle 'while' statement.` |
|        - |  4088 | ` * According to the PHP language reference` |
|        - |  4089 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  4090 | ` *  The basic form of a while statement is:` |
|        - |  4091 | ` *  while (expr)` |
|        - |  4092 | ` *   statement` |
|        - |  4093 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  4094 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  4095 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  4096 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  4097 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  4098 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  4099 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  4100 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  4101 | ` *  while (expr):` |
|        - |  4102 | ` *    statement` |
|        - |  4103 | ` *   endwhile;` |
|        - |  4104 | ` */` |
|    15672 |  4105 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  4106 | `{` |
|    15677 |  4107 | `	GenBlock *pWhileBlock = 0;` |
|    15677 |  4108 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  4109 | `	sxu32 nFalseJump;` |
|        - |  4110 | `	sxu32 nLine;` |
|        - |  4111 | `	sxi32 rc;` |
|    15677 |  4112 | `	nLine = pGen->pIn->nLine;` |
|        - |  4113 | `	/* Jump the 'while' keyword */` |
|    15677 |  4114 | `	pGen->pIn++;` |
|    15677 |  4115 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4116 | `		/* Syntax error */` |
|      ! 0 |  4117 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4118 | `		if( rc == SXERR_ABORT ){` |
|        - |  4119 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4120 | `			return SXERR_ABORT;` |
|        - |  4121 | `		}` |
|      ! 0 |  4122 | `		goto Synchronize;` |
|        - |  4123 | `	}` |
|        - |  4124 | `	/* Jump the left parenthesis '(' */` |
|    15677 |  4125 | `	pGen->pIn++;` |
|        - |  4126 | `	/* Create the loop block */` |
|    15677 |  4127 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    15677 |  4128 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4129 | `		return SXERR_ABORT;` |
|        - |  4130 | `	}` |
|        - |  4131 | `	/* Delimit the condition */` |
|    15677 |  4132 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    15677 |  4133 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4134 | `		/* Empty expression */` |
|        3 |  4135 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|        3 |  4136 | `		if( rc == SXERR_ABORT ){` |
|        - |  4137 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4138 | `			return SXERR_ABORT;` |
|        - |  4139 | `		}` |
|        1 |  4140 | `	}` |
|        - |  4141 | `	/* Swap token streams */` |
|    15677 |  4142 | `	pTmp = pGen->pEnd;` |
|    15677 |  4143 | `	pGen->pEnd = pEnd;` |
|        - |  4144 | `	/* Compile the expression */` |
|    15677 |  4145 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    15677 |  4146 | `	if( rc == SXERR_ABORT ){` |
|        - |  4147 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4148 | `		return SXERR_ABORT;` |
|        - |  4149 | `	}` |
|        - |  4150 | `	/* Update token stream */` |
|    15677 |  4151 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4152 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4153 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4154 | `			return SXERR_ABORT;` |
|        - |  4155 | `		}` |
|      ! 0 |  4156 | `		pGen->pIn++;` |
|      ! 0 |  4157 | `	}` |
|        - |  4158 | `	/* Synchronize pointers */` |
|    15677 |  4159 | `	pGen->pIn  = &pEnd[1];` |
|    15677 |  4160 | `	pGen->pEnd = pTmp;` |
|        - |  4161 | `	/* Emit the false jump */` |
|    15677 |  4162 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4163 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    15677 |  4164 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  4165 | `	/* Compile the loop body */` |
|    15677 |  4166 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    15677 |  4167 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4168 | `		return SXERR_ABORT;` |
|        - |  4169 | `	}` |
|        - |  4170 | `	/* Emit the unconditional jump to the start of the loop */` |
|    15677 |  4171 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  4172 | `	/* Fix all jumps now the destination is resolved */` |
|    15677 |  4173 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4174 | `	/* Release the loop block */` |
|    15677 |  4175 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4176 | `	/* Statement successfully compiled */` |
|    15677 |  4177 | `	return SXRET_OK;` |
|      ! 0 |  4178 | `Synchronize:` |
|        - |  4179 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4180 | `	 * compiling this erroneous block.` |
|        - |  4181 | `	 */` |
|      ! 0 |  4182 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4183 | `		pGen->pIn++;` |
|      ! 0 |  4184 | `	}` |
|      ! 0 |  4185 | `	return SXRET_OK;` |
|     7841 |  4186 | `}` |
|        - |  4187 | `/*` |
|        - |  4188 | ` * Compile the ugly do..while() statement.` |
|        - |  4189 | ` * According to the PHP language reference` |
|        - |  4190 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  4191 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  4192 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  4193 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  4194 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  4195 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  4196 | ` *  would end immediately).` |
|        - |  4197 | ` *  There is just one syntax for do-while loops:` |
|        - |  4198 | ` *  <?php` |
|        - |  4199 | ` *  $i = 0;` |
|        - |  4200 | ` *  do {` |
|        - |  4201 | ` *   echo $i;` |
|        - |  4202 | ` *  } while ($i > 0);` |
|        - |  4203 | ` * ?>` |
|        - |  4204 | ` */` |
|        2 |  4205 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  4206 | `{` |
|        3 |  4207 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  4208 | `	GenBlock *pDoBlock = 0;` |
|        - |  4209 | `	sxu32 nLine;` |
|        - |  4210 | `	sxi32 rc;` |
|        3 |  4211 | `	nLine = pGen->pIn->nLine;` |
|        - |  4212 | `	/* Jump the 'do' keyword */` |
|        3 |  4213 | `	pGen->pIn++;` |
|        - |  4214 | `	/* Create the loop block */` |
|        3 |  4215 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  4216 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4217 | `		return SXERR_ABORT;` |
|        - |  4218 | `	}` |
|        - |  4219 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  4220 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  4221 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  4222 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4223 | `		return SXERR_ABORT;` |
|        - |  4224 | `	}` |
|        3 |  4225 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4226 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  4227 | `	}` |
|        3 |  4228 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  4229 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  4230 | `			/* Missing 'while' statement */` |
|        3 |  4231 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|        3 |  4232 | `			if( rc == SXERR_ABORT ){` |
|        - |  4233 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4234 | `				return SXERR_ABORT;` |
|        - |  4235 | `			}` |
|        3 |  4236 | `			goto Synchronize;` |
|        - |  4237 | `	}` |
|        - |  4238 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  4239 | `	pGen->pIn++;` |
|      ! 0 |  4240 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4241 | `		/* Syntax error */` |
|      ! 0 |  4242 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4243 | `		if( rc == SXERR_ABORT ){` |
|        - |  4244 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4245 | `			return SXERR_ABORT;` |
|        - |  4246 | `		}` |
|      ! 0 |  4247 | `		goto Synchronize;` |
|        - |  4248 | `	}` |
|        - |  4249 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  4250 | `	pGen->pIn++;` |
|        - |  4251 | `	/* Delimit the condition */` |
|      ! 0 |  4252 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  4253 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4254 | `		/* Empty expression */` |
|      ! 0 |  4255 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|      ! 0 |  4256 | `		if( rc == SXERR_ABORT ){` |
|        - |  4257 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4258 | `			return SXERR_ABORT;` |
|        - |  4259 | `		}` |
|      ! 0 |  4260 | `		goto Synchronize;` |
|        - |  4261 | `	}` |
|        - |  4262 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  4263 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  4264 | `		JumpFixup *aPost;` |
|        - |  4265 | `		VmInstr *pInstr;` |
|        - |  4266 | `		sxu32 nJumpDest;` |
|        - |  4267 | `		sxu32 n;` |
|      ! 0 |  4268 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  4269 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  4270 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  4271 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  4272 | `			if( pInstr ){` |
|        - |  4273 | `				/* Fix */` |
|      ! 0 |  4274 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  4275 | `			}` |
|      ! 0 |  4276 | `		}` |
|      ! 0 |  4277 | `	}` |
|        - |  4278 | `	/* Swap token streams */` |
|      ! 0 |  4279 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  4280 | `	pGen->pEnd = pEnd;` |
|        - |  4281 | `	/* Compile the expression */` |
|      ! 0 |  4282 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  4283 | `	if( rc == SXERR_ABORT ){` |
|        - |  4284 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4285 | `		return SXERR_ABORT;` |
|        - |  4286 | `	}` |
|        - |  4287 | `	/* Update token stream */` |
|      ! 0 |  4288 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4289 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4290 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4291 | `			return SXERR_ABORT;` |
|        - |  4292 | `		}` |
|      ! 0 |  4293 | `		pGen->pIn++;` |
|      ! 0 |  4294 | `	}` |
|      ! 0 |  4295 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  4296 | `	pGen->pEnd = pTmp;` |
|        - |  4297 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  4298 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  4299 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  4300 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4301 | `	/* Release the loop block */` |
|      ! 0 |  4302 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4303 | `	/* Statement successfully compiled */` |
|      ! 0 |  4304 | `	return SXRET_OK;` |
|        1 |  4305 | `Synchronize:` |
|        - |  4306 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4307 | `	 * compiling this erroneous block.` |
|        - |  4308 | `	 */` |
|        3 |  4309 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4310 | `		pGen->pIn++;` |
|      ! 0 |  4311 | `	}` |
|        3 |  4312 | `	return SXRET_OK;` |
|        2 |  4313 | `}` |
|        - |  4314 | `/*` |
|        - |  4315 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  4316 | ` * According to the PHP language reference` |
|        - |  4317 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  4318 | ` *  The syntax of a for loop is:` |
|        - |  4319 | ` *  for (expr1; expr2; expr3)` |
|        - |  4320 | ` *   statement` |
|        - |  4321 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  4322 | ` *  the beginning of the loop.` |
|        - |  4323 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  4324 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  4325 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  4326 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  4327 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  4328 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  4329 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  4330 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  4331 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  4332 | ` *  of using the for truth expression.` |
|        - |  4333 | ` */` |
|    38980 |  4334 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  4335 | `{` |
|    38985 |  4336 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    38985 |  4337 | `	GenBlock *pForBlock = 0;` |
|        - |  4338 | `	sxu32 nFalseJump;` |
|        - |  4339 | `	sxu32 nLine;` |
|        - |  4340 | `	sxi32 rc;` |
|    38985 |  4341 | `	nLine = pGen->pIn->nLine;` |
|        - |  4342 | `	/* Jump the 'for' keyword */` |
|    38985 |  4343 | `	pGen->pIn++;` |
|    38985 |  4344 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4345 | `		/* Syntax error */` |
|      ! 0 |  4346 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  4347 | `		if( rc == SXERR_ABORT ){` |
|        - |  4348 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4349 | `			return SXERR_ABORT;` |
|        - |  4350 | `		}` |
|      ! 0 |  4351 | `		return SXRET_OK;` |
|        - |  4352 | `	}` |
|        - |  4353 | `	/* Jump the left parenthesis '(' */` |
|    38985 |  4354 | `	pGen->pIn++;` |
|        - |  4355 | `	/* Delimit the init-expr;condition;post-expr */` |
|    38985 |  4356 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    38985 |  4357 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4358 | `		/* Empty expression */` |
|      ! 0 |  4359 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  4360 | `		if( rc == SXERR_ABORT ){` |
|        - |  4361 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4362 | `			return SXERR_ABORT;` |
|        - |  4363 | `		}` |
|        - |  4364 | `		/* Synchronize */` |
|      ! 0 |  4365 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4366 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4367 | `			pGen->pIn++;` |
|      ! 0 |  4368 | `		}` |
|      ! 0 |  4369 | `		return SXRET_OK;` |
|        - |  4370 | `	}` |
|        - |  4371 | `	/* Swap token streams */` |
|    38985 |  4372 | `	pTmp = pGen->pEnd;` |
|    38985 |  4373 | `	pGen->pEnd = pEnd;` |
|        - |  4374 | `	/* Compile initialization expressions if available */` |
|    38985 |  4375 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4376 | `	/* Pop operand lvalues */` |
|    38985 |  4377 | `	if( rc == SXERR_ABORT ){` |
|        - |  4378 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4379 | `		return SXERR_ABORT;` |
|    38985 |  4380 | `	}else if( rc != SXERR_EMPTY ){` |
|    38983 |  4381 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19489 |  4382 | `	}` |
|    38985 |  4383 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4384 | `		/* Syntax error */` |
|      ! 0 |  4385 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4386 | `			"for: Expected ';' after initialization expressions");` |
|      ! 0 |  4387 | `		if( rc == SXERR_ABORT ){` |
|        - |  4388 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4389 | `			return SXERR_ABORT;` |
|        - |  4390 | `		}` |
|      ! 0 |  4391 | `		return SXRET_OK;` |
|        - |  4392 | `	}` |
|        - |  4393 | `	/* Jump the trailing ';' */` |
|    38985 |  4394 | `	pGen->pIn++;` |
|        - |  4395 | `	/* Create the loop block */` |
|    38985 |  4396 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    38985 |  4397 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4398 | `		return SXERR_ABORT;` |
|        - |  4399 | `	}` |
|        - |  4400 | `	/* Deffer continue jumps */` |
|    38985 |  4401 | `	pForBlock->bPostContinue = TRUE;` |
|        - |  4402 | `	/* Compile the condition */` |
|    38985 |  4403 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38985 |  4404 | `	if( rc == SXERR_ABORT ){` |
|        - |  4405 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4406 | `		return SXERR_ABORT;` |
|    38985 |  4407 | `	}else if( rc != SXERR_EMPTY ){` |
|        - |  4408 | `		/* Emit the false jump */` |
|    38983 |  4409 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4410 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    38983 |  4411 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    19489 |  4412 | `	}` |
|    38985 |  4413 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4414 | `		/* Syntax error */` |
|        6 |  4415 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4416 | `			"for: Expected ';' after conditionals expressions");` |
|        6 |  4417 | `		if( rc == SXERR_ABORT ){` |
|        - |  4418 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4419 | `			return SXERR_ABORT;` |
|        - |  4420 | `		}` |
|        6 |  4421 | `		return SXRET_OK;` |
|        - |  4422 | `	}` |
|        - |  4423 | `	/* Jump the trailing ';' */` |
|    38981 |  4424 | `	pGen->pIn++;` |
|        - |  4425 | `	/* Save the post condition stream */` |
|    38981 |  4426 | `	pPostStart = pGen->pIn;` |
|        - |  4427 | `	/* Compile the loop body */` |
|    38981 |  4428 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    38981 |  4429 | `	pGen->pEnd = pTmp;` |
|    38981 |  4430 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    38981 |  4431 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4432 | `		return SXERR_ABORT;` |
|        - |  4433 | `	}` |
|        - |  4434 | `	/* Fix post-continue jumps */` |
|    38981 |  4435 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - |  4436 | `		JumpFixup *aPost;` |
|        - |  4437 | `		VmInstr *pInstr;` |
|        - |  4438 | `		sxu32 nJumpDest;` |
|        - |  4439 | `		sxu32 n;` |
|       14 |  4440 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       14 |  4441 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       26 |  4442 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       14 |  4443 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       14 |  4444 | `			if( pInstr ){` |
|        - |  4445 | `				/* Fix jump */` |
|       14 |  4446 | `				pInstr->iP2 = nJumpDest;` |
|        6 |  4447 | `			}` |
|        8 |  4448 | `		}` |
|        6 |  4449 | `	}` |
|        - |  4450 | `	/* compile the post-expressions if available */` |
|    38981 |  4451 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4452 | `		pPostStart++;` |
|      ! 0 |  4453 | `	}` |
|    38981 |  4454 | `	if( pPostStart < pEnd ){` |
|        - |  4455 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    38981 |  4456 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    38981 |  4457 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38981 |  4458 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - |  4459 | `			/* Syntax error */` |
|      ! 0 |  4460 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 |  4461 | `			if( rc == SXERR_ABORT ){` |
|        - |  4462 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4463 | `				return SXERR_ABORT;` |
|        - |  4464 | `			}` |
|      ! 0 |  4465 | `			return SXRET_OK;` |
|        - |  4466 | `		}` |
|    38981 |  4467 | `		RE_SWAP_DELIMITER(pGen);` |
|    38981 |  4468 | `		if( rc == SXERR_ABORT ){` |
|        - |  4469 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4470 | `			return SXERR_ABORT;` |
|    38981 |  4471 | `		}else if( rc != SXERR_EMPTY){` |
|        - |  4472 | `			/* Pop operand lvalue */` |
|    38981 |  4473 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19488 |  4474 | `		}` |
|    19488 |  4475 | `	}` |
|        - |  4476 | `	/* Emit the unconditional jump to the start of the loop */` |
|    38981 |  4477 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - |  4478 | `	/* Fix all jumps now the destination is resolved */` |
|    38981 |  4479 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4480 | `	/* Release the loop block */` |
|    38981 |  4481 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4482 | `	/* Statement successfully compiled */` |
|    38981 |  4483 | `	return SXRET_OK;` |
|    19495 |  4484 | `}` |
|        - |  4485 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - |  4486 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - |  4487 | ` * are allowed.` |
|        - |  4488 | ` */` |
|   241616 |  4489 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  4490 | `{` |
|   241621 |  4491 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   241621 |  4492 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  4493 | `		/* Unexpected expression */` |
|      ! 0 |  4494 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  4495 | `			"foreach: Expecting a variable name");` |
|      ! 0 |  4496 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  4497 | `			rc = SXERR_INVALID;` |
|      ! 0 |  4498 | `		}` |
|      ! 0 |  4499 | `	}` |
|   241621 |  4500 | `	return rc;` |
|        5 |  4501 | `}` |
|        - |  4502 | `/*` |
|        - |  4503 | ` * Compile the 'foreach' statement.` |
|        - |  4504 | ` * According to the PHP language reference` |
|        - |  4505 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - |  4506 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - |  4507 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - |  4508 | ` *  is a minor but useful extension of the first:` |
|        - |  4509 | ` *  foreach (array_expression as $value)` |
|        - |  4510 | ` *    statement` |
|        - |  4511 | ` *  foreach (array_expression as $key => $value)` |
|        - |  4512 | ` *   statement` |
|        - |  4513 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - |  4514 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - |  4515 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - |  4516 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - |  4517 | ` *  to the variable $key on each loop.` |
|        - |  4518 | ` *  Note:` |
|        - |  4519 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - |  4520 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - |  4521 | ` *  Note:` |
|        - |  4522 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - |  4523 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - |  4524 | ` *  or after the foreach without resetting it.` |
|        - |  4525 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - |  4526 | ` *  of copying the value.` |
|        - |  4527 | ` */` |
|   175378 |  4528 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 |  4529 | `{` |
|   175383 |  4530 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   175383 |  4531 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   175383 |  4532 | `	GenBlock *pForeachBlock = 0;` |
|        - |  4533 | `	ph7_foreach_info *pInfo;` |
|        - |  4534 | `	sxu32 nFalseJump;` |
|        - |  4535 | `	VmInstr *pInstr;` |
|        - |  4536 | `	sxu32 nLine;` |
|        - |  4537 | `	sxi32 rc;` |
|   175383 |  4538 | `	nLine = pGen->pIn->nLine;` |
|        - |  4539 | `	/* Jump the 'foreach' keyword */` |
|   175383 |  4540 | `	pGen->pIn++;` |
|   175383 |  4541 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4542 | `		/* Syntax error */` |
|      ! 0 |  4543 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 |  4544 | `		if( rc == SXERR_ABORT ){` |
|        - |  4545 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4546 | `			return SXERR_ABORT;` |
|        - |  4547 | `		}` |
|      ! 0 |  4548 | `		goto Synchronize;` |
|        - |  4549 | `	}` |
|        - |  4550 | `	/* Jump the left parenthesis '(' */` |
|   175383 |  4551 | `	pGen->pIn++;` |
|        - |  4552 | `	/* Create the loop block */` |
|   175383 |  4553 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   175383 |  4554 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4555 | `		return SXERR_ABORT;` |
|        - |  4556 | `	}` |
|        - |  4557 | `	/* Delimit the expression */` |
|   175383 |  4558 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   175383 |  4559 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4560 | `		/* Empty expression */` |
|      ! 0 |  4561 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 |  4562 | `		if( rc == SXERR_ABORT ){` |
|        - |  4563 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4564 | `			return SXERR_ABORT;` |
|        - |  4565 | `		}` |
|        - |  4566 | `		/* Synchronize */` |
|      ! 0 |  4567 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4568 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4569 | `			pGen->pIn++;` |
|      ! 0 |  4570 | `		}` |
|      ! 0 |  4571 | `		return SXRET_OK;` |
|        - |  4572 | `	}` |
|        - |  4573 | `	/* Compile the array expression */` |
|   175383 |  4574 | `	pCur = pGen->pIn;` |
|  1024999 |  4575 | `	while( pCur < pEnd ){` |
|  1024999 |  4576 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   179281 |  4577 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   179281 |  4578 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - |  4579 | `				/* Break with the first 'as' found */` |
|   175383 |  4580 | `				break;` |
|        - |  4581 | `			}` |
|     1949 |  4582 | `		}` |
|        - |  4583 | `		/* Advance the stream cursor */` |
|   849621 |  4584 | `		pCur++;` |
|        5 |  4585 | `	}` |
|   175383 |  4586 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 |  4587 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  4588 | `			"foreach: Missing array/object expression");` |
|      ! 0 |  4589 | `		if( rc == SXERR_ABORT ){` |
|        - |  4590 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4591 | `			return SXERR_ABORT;` |
|        - |  4592 | `		}` |
|      ! 0 |  4593 | `		goto Synchronize;` |
|        - |  4594 | `	}` |
|        - |  4595 | `	/* Swap token streams */` |
|   175383 |  4596 | `	pTmp = pGen->pEnd;` |
|   175383 |  4597 | `	pGen->pEnd = pCur;` |
|   175383 |  4598 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   175383 |  4599 | `	if( rc == SXERR_ABORT ){` |
|        - |  4600 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4601 | `		return SXERR_ABORT;` |
|        - |  4602 | `	}` |
|        - |  4603 | `	/* Update token stream */` |
|   175383 |  4604 | `	while(pGen->pIn < pCur ){` |
|      ! 0 |  4605 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4606 | `		if( rc == SXERR_ABORT ){` |
|        - |  4607 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4608 | `			return SXERR_ABORT;` |
|        - |  4609 | `		}` |
|      ! 0 |  4610 | `		pGen->pIn++;` |
|      ! 0 |  4611 | `	}` |
|   175383 |  4612 | `	pCur++; /* Jump the 'as' keyword */` |
|   175383 |  4613 | `	pGen->pIn = pCur;` |
|   175383 |  4614 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4615 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 |  4616 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4617 | `			return SXERR_ABORT;` |
|        - |  4618 | `		}` |
|      ! 0 |  4619 | `	}` |
|        - |  4620 | `	/* Create the foreach context */` |
|   175383 |  4621 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   175383 |  4622 | `	if( pInfo == 0 ){` |
|      ! 0 |  4623 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  4624 | `		return SXERR_ABORT;` |
|        - |  4625 | `	}` |
|        - |  4626 | `	/* Zero the structure */` |
|   175383 |  4627 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - |  4628 | `	/* Initialize structure fields */` |
|   175383 |  4629 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - |  4630 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - |  4631 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - |  4632 | `	 * '=>'. */` |
|   175383 |  4633 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   175383 |  4634 | `	if( pCur < pEnd ){` |
|        - |  4635 | `		/* Compile the expression holding the key name */` |
|    66263 |  4636 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 |  4637 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 |  4638 | `			if( rc == SXERR_ABORT ){` |
|        - |  4639 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4640 | `				return SXERR_ABORT;` |
|        - |  4641 | `			}` |
|      ! 0 |  4642 | `		}else{` |
|    66263 |  4643 | `			pGen->pEnd = pCur;` |
|    66263 |  4644 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    66263 |  4645 | `			if( rc == SXERR_ABORT ){` |
|        - |  4646 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4647 | `				return SXERR_ABORT;` |
|        - |  4648 | `			}` |
|    66263 |  4649 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    66263 |  4650 | `			if( pInstr->p3 ){` |
|        - |  4651 | `				/* Record key name */` |
|    66263 |  4652 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    33129 |  4653 | `			}` |
|    66263 |  4654 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - |  4655 | `		}` |
|    66263 |  4656 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    33129 |  4657 | `	}` |
|   175383 |  4658 | `	pGen->pEnd = pEnd;` |
|   175383 |  4659 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4660 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 |  4661 | `		if( rc == SXERR_ABORT ){` |
|        - |  4662 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4663 | `			return SXERR_ABORT;` |
|        - |  4664 | `		}` |
|      ! 0 |  4665 | `		goto Synchronize;` |
|        - |  4666 | `	}` |
|   175383 |  4667 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       31 |  4668 | `		pGen->pIn++;` |
|        - |  4669 | `		/* Pass by reference  */` |
|       31 |  4670 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       14 |  4671 | `	}` |
|        - |  4672 | `	/* Check if the value target is list() */` |
|   175383 |  4673 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 |  4674 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - |  4675 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - |  4676 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - |  4677 | `		 */` |
|        - |  4678 | `		static int iForeachListCnt = 0;` |
|        - |  4679 | `		char zTmp[128];` |
|        - |  4680 | `		sxu32 nLen;` |
|        - |  4681 | `		char *zDup;` |
|       10 |  4682 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 |  4683 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 |  4684 | `		if( zDup == 0 ){` |
|      ! 0 |  4685 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4686 | `			return SXERR_ABORT;` |
|        - |  4687 | `		}` |
|       10 |  4688 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4689 | `		/* Save list() token boundaries */` |
|       10 |  4690 | `		pListStart = pGen->pIn;` |
|        - |  4691 | `		/* Advance past list(...) — validate parentheses */` |
|       10 |  4692 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 |  4693 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  4694 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  4695 | `				"foreach: Expected '(' after 'list'");` |
|        3 |  4696 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4697 | `				return SXERR_ABORT;` |
|        - |  4698 | `			}` |
|        3 |  4699 | `			goto Synchronize;` |
|        - |  4700 | `		}` |
|        7 |  4701 | `		pGen->pIn++; /* Jump '(' */` |
|        7 |  4702 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 |  4703 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4704 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4705 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 |  4706 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4707 | `				return SXERR_ABORT;` |
|        - |  4708 | `			}` |
|      ! 0 |  4709 | `			goto Synchronize;` |
|        - |  4710 | `		}` |
|        7 |  4711 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 |  4712 | `		pListEnd = pGen->pIn;` |
|        7 |  4713 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   175378 |  4714 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - |  4715 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - |  4716 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - |  4717 | `		 */` |
|        - |  4718 | `		static int iForeachShortListCnt = 0;` |
|        - |  4719 | `		char zTmp[128];` |
|        - |  4720 | `		sxu32 nLen;` |
|        - |  4721 | `		char *zDup;` |
|       13 |  4722 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       13 |  4723 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       13 |  4724 | `		if( zDup == 0 ){` |
|      ! 0 |  4725 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4726 | `			return SXERR_ABORT;` |
|        - |  4727 | `		}` |
|       13 |  4728 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4729 | `		/* Save [...] token boundaries */` |
|       13 |  4730 | `		pListStart = pGen->pIn;` |
|        - |  4731 | `		/* Advance past [...] */` |
|       13 |  4732 | `		pGen->pIn++; /* Jump '[' */` |
|       13 |  4733 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       13 |  4734 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4735 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4736 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 |  4737 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4738 | `				return SXERR_ABORT;` |
|        - |  4739 | `			}` |
|      ! 0 |  4740 | `			goto Synchronize;` |
|        - |  4741 | `		}` |
|       13 |  4742 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       13 |  4743 | `		pListEnd = pGen->pIn;` |
|       13 |  4744 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        7 |  4745 | `	}else{` |
|        - |  4746 | `		/* Compile the expression holding the value name */` |
|   175363 |  4747 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   175363 |  4748 | `		if( rc == SXERR_ABORT ){` |
|        - |  4749 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4750 | `			return SXERR_ABORT;` |
|        - |  4751 | `		}` |
|   175363 |  4752 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   175363 |  4753 | `		if( pInstr->p3 ){` |
|        - |  4754 | `			/* Record value name */` |
|   175363 |  4755 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    87679 |  4756 | `		}` |
|        - |  4757 | `	}` |
|        - |  4758 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   175381 |  4759 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - |  4760 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175381 |  4761 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - |  4762 | `	/* Record the first instruction to execute */` |
|   175381 |  4763 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4764 | `	/* Emit the FOREACH_STEP instruction */` |
|   175381 |  4765 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - |  4766 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175381 |  4767 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - |  4768 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   175381 |  4769 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - |  4770 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - |  4771 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - |  4772 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - |  4773 | `		 */` |
|       19 |  4774 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - |  4775 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - |  4776 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - |  4777 | `		 * picks up the delimiter and the variable names inside.` |
|        - |  4778 | `		 */` |
|       19 |  4779 | `		pSavedIn = pGen->pIn;` |
|       19 |  4780 | `		pSavedEnd = pGen->pEnd;` |
|       19 |  4781 | `		pGen->pIn = pListStart;` |
|       19 |  4782 | `		pGen->pEnd = pListEnd;` |
|       19 |  4783 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       13 |  4784 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 |  4785 | `		}else{` |
|        7 |  4786 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - |  4787 | `		}` |
|       19 |  4788 | `		pGen->pIn = pSavedIn;` |
|       19 |  4789 | `		pGen->pEnd = pSavedEnd;` |
|       19 |  4790 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4791 | `			return SXERR_ABORT;` |
|        - |  4792 | `		}` |
|        - |  4793 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       19 |  4794 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        9 |  4795 | `	}` |
|        - |  4796 | `	/* Compile the loop body */` |
|   175381 |  4797 | `	pGen->pIn = &pEnd[1];` |
|   175381 |  4798 | `	pGen->pEnd = pTmp;` |
|   175381 |  4799 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   175381 |  4800 | `	if( rc == SXERR_ABORT ){` |
|        - |  4801 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4802 | `		return SXERR_ABORT;` |
|        - |  4803 | `	}` |
|        - |  4804 | `	/* Emit the unconditional jump to the start of the loop */` |
|   175381 |  4805 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - |  4806 | `	/* Fix all jumps now the destination is resolved */` |
|   175381 |  4807 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4808 | `	/* Release the loop block */` |
|   175381 |  4809 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4810 | `	/* Statement successfully compiled */` |
|   175381 |  4811 | `	return SXRET_OK;` |
|        1 |  4812 | `Synchronize:` |
|        - |  4813 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4814 | `	 * compiling this erroneous block.` |
|        - |  4815 | `	 */` |
|        3 |  4816 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4817 | `		pGen->pIn++;` |
|      ! 0 |  4818 | `	}` |
|        3 |  4819 | `	return SXRET_OK;` |
|    87694 |  4820 | `}` |
|        - |  4821 | `/*` |
|        - |  4822 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - |  4823 | ` * According to the PHP language reference` |
|        - |  4824 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - |  4825 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - |  4826 | ` *  that is similar to that of C:` |
|        - |  4827 | ` *  if (expr)` |
|        - |  4828 | ` *   statement` |
|        - |  4829 | ` *  else construct:` |
|        - |  4830 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - |  4831 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - |  4832 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - |  4833 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - |  4834 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - |  4835 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - |  4836 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - |  4837 | ` *  elseif` |
|        - |  4838 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - |  4839 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - |  4840 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - |  4841 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - |  4842 | ` *   than b, a equal to b or a is smaller than b:` |
|        - |  4843 | ` *   <?php` |
|        - |  4844 | ` *    if ($a > $b) {` |
|        - |  4845 | ` *     echo "a is bigger than b";` |
|        - |  4846 | ` *    } elseif ($a == $b) {` |
|        - |  4847 | ` *     echo "a is equal to b";` |
|        - |  4848 | ` *    } else {` |
|        - |  4849 | ` *     echo "a is smaller than b";` |
|        - |  4850 | ` *    }` |
|        - |  4851 | ` *    ?>` |
|        - |  4852 | ` */` |
|  1179458 |  4853 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 |  4854 | `{` |
|  1179463 |  4855 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  1179463 |  4856 | `	GenBlock *pCondBlock = 0;` |
|        - |  4857 | `	sxu32 nJumpIdx;` |
|        - |  4858 | `	sxu32 nKeyID;` |
|        - |  4859 | `	sxi32 rc;` |
|        - |  4860 | `	/* Jump the 'if' keyword */` |
|  1179463 |  4861 | `	pGen->pIn++;` |
|  1179463 |  4862 | `	pToken = pGen->pIn;` |
|        - |  4863 | `	/* Create the conditional block */` |
|  1179463 |  4864 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  1179463 |  4865 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4866 | `		return SXERR_ABORT;` |
|        - |  4867 | `	}` |
|        - |  4868 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   636396 |  4869 | `	for(;;){` |
|  1272797 |  4870 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4871 | `			/* Syntax error */` |
|      ! 0 |  4872 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4873 | `				pToken--;` |
|      ! 0 |  4874 | `			}` |
|      ! 0 |  4875 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 |  4876 | `			if( rc == SXERR_ABORT ){` |
|        - |  4877 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4878 | `				return SXERR_ABORT;` |
|        - |  4879 | `			}` |
|      ! 0 |  4880 | `			goto Synchronize;` |
|        - |  4881 | `		}` |
|        - |  4882 | `		/* Jump the left parenthesis '(' */` |
|  1272797 |  4883 | `		pToken++;` |
|        - |  4884 | `		/* Delimit the condition */` |
|  1272797 |  4885 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1272797 |  4886 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - |  4887 | `			/* Syntax error */` |
|      ! 0 |  4888 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4889 | `				pToken--;` |
|      ! 0 |  4890 | `			}` |
|      ! 0 |  4891 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 |  4892 | `			if( rc == SXERR_ABORT ){` |
|        - |  4893 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4894 | `				return SXERR_ABORT;` |
|        - |  4895 | `			}` |
|      ! 0 |  4896 | `			goto Synchronize;` |
|        - |  4897 | `		}` |
|        - |  4898 | `		/* Swap token streams */` |
|  1272797 |  4899 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - |  4900 | `		/* Compile the condition */` |
|  1272797 |  4901 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4902 | `		/* Update token stream */` |
|  1272797 |  4903 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 |  4904 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4905 | `			pGen->pIn++;` |
|      ! 0 |  4906 | `		}` |
|  1272797 |  4907 | `		pGen->pIn  = &pEnd[1];` |
|  1272797 |  4908 | `		pGen->pEnd = pTmp;` |
|  1272797 |  4909 | `		if( rc == SXERR_ABORT ){` |
|        - |  4910 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4911 | `			return SXERR_ABORT;` |
|        - |  4912 | `		}` |
|        - |  4913 | `		/* Emit the false jump */` |
|  1272797 |  4914 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - |  4915 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  1272797 |  4916 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - |  4917 | `		/* Compile the body */` |
|  1272797 |  4918 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  1272797 |  4919 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4920 | `			return SXERR_ABORT;` |
|        - |  4921 | `		}` |
|  1272797 |  4922 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   239764 |  4923 | `			break;` |
|        - |  4924 | `		}` |
|        - |  4925 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   793279 |  4926 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   793279 |  4927 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   614025 |  4928 | `			break;` |
|        - |  4929 | `		}` |
|        - |  4930 | `		/* Emit the unconditional jump */` |
|   179259 |  4931 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - |  4932 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   179259 |  4933 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   179259 |  4934 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   171373 |  4935 | `			pToken = &pGen->pIn[1];` |
|   171373 |  4936 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85486 |  4937 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    42965 |  4938 | `					break;` |
|        - |  4939 | `			}` |
|    85453 |  4940 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42724 |  4941 | `		}` |
|    93339 |  4942 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - |  4943 | `		/* Synchronize cursors */` |
|    93339 |  4944 | `		pToken = pGen->pIn;` |
|        - |  4945 | `		/* Fix the false jump */` |
|    93339 |  4946 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 |  4947 | `	} /* For(;;) */` |
|        - |  4948 | `	/* Fix the false jump */` |
|  1179463 |  4949 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  1179463 |  4950 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   699940 |  4951 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - |  4952 | `			/* Compile the else block */` |
|    85925 |  4953 | `			pGen->pIn++;` |
|    85925 |  4954 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    85925 |  4955 | `			if( rc == SXERR_ABORT ){` |
|        - |  4956 |  |
|      ! 0 |  4957 | `				return SXERR_ABORT;` |
|        - |  4958 | `			}` |
|    42960 |  4959 | `	}` |
|  1179463 |  4960 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4961 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  1179463 |  4962 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - |  4963 | `	/* Release the conditional block */` |
|  1179463 |  4964 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4965 | `	/* Statement successfully compiled */` |
|  1179463 |  4966 | `	return SXRET_OK;` |
|      ! 0 |  4967 | `Synchronize:` |
|        - |  4968 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - |  4969 | `	 */` |
|      ! 0 |  4970 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4971 | `		pGen->pIn++;` |
|      ! 0 |  4972 | `	}` |
|      ! 0 |  4973 | `	return SXRET_OK;` |
|   589734 |  4974 | `}` |
|        - |  4975 | `/*` |
|        - |  4976 | ` * Compile the global construct.` |
|        - |  4977 | ` * According to the PHP language reference` |
|        - |  4978 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - |  4979 | ` *  to be used in that function.` |
|        - |  4980 | ` *  Example #1 Using global` |
|        - |  4981 | ` *  <?php` |
|        - |  4982 | ` *   $a = 1;` |
|        - |  4983 | ` *   $b = 2;` |
|        - |  4984 | ` *   function Sum()` |
|        - |  4985 | ` *   {` |
|        - |  4986 | ` *    global $a, $b;` |
|        - |  4987 | ` *    $b = $a + $b;` |
|        - |  4988 | ` *   }` |
|        - |  4989 | ` *   Sum();` |
|        - |  4990 | ` *   echo $b;` |
|        - |  4991 | ` *  ?>` |
|        - |  4992 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - |  4993 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - |  4994 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - |  4995 | ` */` |
|       36 |  4996 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 |  4997 | `{` |
|       41 |  4998 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  4999 | `	sxi32 nExpr;` |
|        - |  5000 | `	sxi32 rc;` |
|        - |  5001 | `	/* Jump the 'global' keyword */` |
|       41 |  5002 | `	pGen->pIn++;` |
|       41 |  5003 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - |  5004 | `		/* Nothing to process */` |
|      ! 0 |  5005 | `		return SXRET_OK;` |
|        - |  5006 | `	}` |
|       41 |  5007 | `	pTmp = pGen->pEnd;` |
|       41 |  5008 | `	nExpr = 0;` |
|       87 |  5009 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 |  5010 | `		if( pGen->pIn < pNext ){` |
|       51 |  5011 | `			pGen->pEnd = pNext;` |
|       51 |  5012 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5013 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 |  5014 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  5015 | `					return SXERR_ABORT;` |
|        - |  5016 | `				}` |
|      ! 0 |  5017 | `			}else{` |
|       51 |  5018 | `				pGen->pIn++;` |
|       51 |  5019 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5020 | `					/* Emit a warning */` |
|      ! 0 |  5021 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 |  5022 | `				}else{` |
|       51 |  5023 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 |  5024 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  5025 | `						return SXERR_ABORT;` |
|       51 |  5026 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 |  5027 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 |  5028 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - |  5029 | `							/* Variable name, not a constant */` |
|       51 |  5030 | `							pLast->iP1 = 0;` |
|       23 |  5031 | `						}` |
|       51 |  5032 | `						nExpr++;` |
|       23 |  5033 | `					}` |
|        - |  5034 | `				}` |
|        - |  5035 | `			}` |
|       23 |  5036 | `		}` |
|        - |  5037 | `		/* Next expression in the stream */` |
|       51 |  5038 | `		pGen->pIn = pNext;` |
|        - |  5039 | `		/* Jump trailing commas */` |
|       61 |  5040 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 |  5041 | `			pGen->pIn++;` |
|        5 |  5042 | `		}` |
|        5 |  5043 | `	}` |
|        - |  5044 | `	/* Restore token stream */` |
|       41 |  5045 | `	pGen->pEnd = pTmp;` |
|       41 |  5046 | `	if( nExpr > 0 ){` |
|        - |  5047 | `		/* Emit the uplink instruction */` |
|       41 |  5048 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 |  5049 | `	}` |
|       41 |  5050 | `	return SXRET_OK;` |
|       23 |  5051 | `}` |
|        - |  5052 | `/*` |
|        - |  5053 | ` * Compile the return statement.` |
|        - |  5054 | ` * According to the PHP language reference` |
|        - |  5055 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - |  5056 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - |  5057 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - |  5058 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - |  5059 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - |  5060 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - |  5061 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - |  5062 | ` *  from within the main script file, then script execution end.` |
|        - |  5063 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - |  5064 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - |  5065 | ` *  should do so as PHP has less work to do in this case.` |
|        - |  5066 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - |  5067 | ` */` |
|  1621592 |  5068 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 |  5069 | `{` |
|  1621597 |  5070 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - |  5071 | `	sxi32 rc;` |
|  1621597 |  5072 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1621597 |  5073 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - |  5074 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - |  5075 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - |  5076 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - |  5077 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - |  5078 | `	 * normally below so token processing stays consistent. */` |
|  4222755 |  5079 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  2601163 |  5080 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 |  5081 | `	}` |
|  1621592 |  5082 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  1621565 |  5083 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 |  5084 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  5085 | `			"A never-returning function must not return");` |
|        3 |  5086 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5087 | `			return SXERR_ABORT;` |
|        - |  5088 | `		}` |
|        1 |  5089 | `	}` |
|        - |  5090 | `	/* Jump the 'return' keyword */` |
|  1621597 |  5091 | `	pGen->pIn++;` |
|  1621597 |  5092 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5093 | `		/* Compile the expression */` |
|  1606031 |  5094 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  1606031 |  5095 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5096 | `			return SXERR_ABORT;` |
|  1606031 |  5097 | `		}else if(rc != SXERR_EMPTY ){` |
|  1606031 |  5098 | `			nRet = 1;` |
|   803013 |  5099 | `		}` |
|   803013 |  5100 | `	}` |
|        - |  5101 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - |  5102 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - |  5103 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - |  5104 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  1621597 |  5105 | `	if( pGen->bInGenerator ){` |
|       32 |  5106 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|       32 |  5107 | `		return SXRET_OK;` |
|        - |  5108 | `	}` |
|        - |  5109 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - |  5110 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - |  5111 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - |  5112 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - |  5113 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  1621569 |  5114 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  1621569 |  5115 | `	return SXRET_OK;` |
|   810801 |  5116 | `}` |
|        - |  5117 | `/*` |
|        - |  5118 | ` * Compile a yield expression.` |
|        - |  5119 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - |  5120 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - |  5121 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - |  5122 | ` */` |
|      384 |  5123 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 |  5124 | `{` |
|        - |  5125 | `	SyToken *pTmp, *pSplit;` |
|      389 |  5126 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      389 |  5127 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - |  5128 | `	sxi32 rc;` |
|      192 |  5129 | `	(void)iCompileFlag;` |
|        - |  5130 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      389 |  5131 | `	pGen->pIn++;` |
|        - |  5132 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - |  5133 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - |  5134 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - |  5135 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - |  5136 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|      384 |  5137 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      227 |  5138 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 |  5139 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 |  5140 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 |  5141 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 |  5142 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5143 | `			return SXERR_ABORT;` |
|        - |  5144 | `		}` |
|       67 |  5145 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  5146 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 |  5147 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - |  5148 | `				"Missing expression after 'yield from'");` |
|      ! 0 |  5149 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5150 | `				return SXERR_ABORT;` |
|        - |  5151 | `			}` |
|      ! 0 |  5152 | `		}` |
|       67 |  5153 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 |  5154 | `		return SXRET_OK;` |
|        - |  5155 | `	}` |
|      327 |  5156 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5157 | `		/* Bare yield — no value */` |
|        3 |  5158 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 |  5159 | `		return SXRET_OK;` |
|        - |  5160 | `	}` |
|        - |  5161 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      325 |  5162 | `	pSplit = 0;` |
|        - |  5163 | `	{` |
|      325 |  5164 | `		SyToken *pCur = pGen->pIn;` |
|      325 |  5165 | `		sxi32 nNest = 0;` |
|      781 |  5166 | `		while( pCur < pGen->pEnd ){` |
|      475 |  5167 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 |  5168 | `				nNest++;` |
|      467 |  5169 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 |  5170 | `				nNest--;` |
|      451 |  5171 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       16 |  5172 | `				pSplit = pCur;` |
|       16 |  5173 | `				break;` |
|        - |  5174 | `			}` |
|      461 |  5175 | `			pCur++;` |
|        5 |  5176 | `		}` |
|        - |  5177 | `	}` |
|      325 |  5178 | `	pTmp = pGen->pEnd;` |
|      325 |  5179 | `	if( pSplit ){` |
|        - |  5180 | `		/* yield $key => $value */` |
|       16 |  5181 | `		pGen->pEnd = pSplit;` |
|       16 |  5182 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5183 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5184 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       16 |  5185 | `		pGen->pEnd = pTmp;` |
|       16 |  5186 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5187 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5188 | `		iP1 = 1;` |
|       16 |  5189 | `		iP2 = 1;` |
|        9 |  5190 | `	}else{` |
|        - |  5191 | `		/* yield $value */` |
|      311 |  5192 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      311 |  5193 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      311 |  5194 | `		if( rc != SXERR_EMPTY ){` |
|      311 |  5195 | `			iP1 = 1;` |
|      153 |  5196 | `		}` |
|        - |  5197 | `	}` |
|      325 |  5198 | `	pGen->pEnd = pTmp;` |
|      325 |  5199 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      325 |  5200 | `	return SXRET_OK;` |
|      197 |  5201 | `}` |
|        - |  5202 | `/*` |
|        - |  5203 | ` * Compile the die/exit language construct.` |
|        - |  5204 | ` * The role of these constructs is to terminate execution of the script.` |
|        - |  5205 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - |  5206 | ` */` |
|      122 |  5207 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 |  5208 | `{` |
|      127 |  5209 | `	sxi32 nExpr = 0;` |
|        - |  5210 | `	sxi32 rc;` |
|        - |  5211 | `	/* Jump the die/exit keyword */` |
|      127 |  5212 | `	pGen->pIn++;` |
|      127 |  5213 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5214 | `		/* Compile the expression */` |
|      127 |  5215 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      127 |  5216 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5217 | `			return SXERR_ABORT;` |
|      127 |  5218 | `		}else if(rc != SXERR_EMPTY ){` |
|      127 |  5219 | `			nExpr = 1;` |
|       61 |  5220 | `		}` |
|       61 |  5221 | `	}` |
|        - |  5222 | `	/* Emit the HALT instruction */` |
|      127 |  5223 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      127 |  5224 | `	return SXRET_OK;` |
|       66 |  5225 | `}` |
|        - |  5226 | `/*` |
|        - |  5227 | ` * Compile the 'echo' language construct.` |
|        - |  5228 | ` */` |
|    17042 |  5229 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 |  5230 | `{` |
|    17047 |  5231 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5232 | `	sxi32 rc;` |
|        - |  5233 | `	/* Jump the 'echo' keyword */` |
|    17047 |  5234 | `	pGen->pIn++;` |
|        - |  5235 | `	/* Compile arguments one after one */` |
|    17047 |  5236 | `	pTmp = pGen->pEnd;` |
|    41583 |  5237 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    24541 |  5238 | `		if( pGen->pIn < pNext ){` |
|    24541 |  5239 | `			pGen->pEnd = pNext;` |
|    24541 |  5240 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    24541 |  5241 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5242 | `				return SXERR_ABORT;` |
|    24541 |  5243 | `			}else if( rc != SXERR_EMPTY ){` |
|        - |  5244 | `				/* Emit the consume instruction */` |
|    24517 |  5245 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    12256 |  5246 | `			}` |
|    12268 |  5247 | `		}` |
|        - |  5248 | `		/* Jump trailing commas */` |
|    32035 |  5249 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     7499 |  5250 | `			pNext++;` |
|        5 |  5251 | `		}` |
|    24541 |  5252 | `		pGen->pIn = pNext;` |
|        5 |  5253 | `	}` |
|        - |  5254 | `	/* Restore token stream */` |
|    17047 |  5255 | `	pGen->pEnd = pTmp;` |
|    17047 |  5256 | `	return SXRET_OK;` |
|     8526 |  5257 | `}` |
|        - |  5258 | `/*` |
|        - |  5259 | ` * Compile the static statement.` |
|        - |  5260 | ` * According to the PHP language reference` |
|        - |  5261 | ` *  Another important feature of variable scoping is the static variable.` |
|        - |  5262 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - |  5263 | ` *  when program execution leaves this scope.` |
|        - |  5264 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - |  5265 | ` * Symisc eXtension.` |
|        - |  5266 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - |  5267 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  5268 | ` *  Example` |
|        - |  5269 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  5270 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  5271 | ` */` |
|       12 |  5272 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        3 |  5273 | `{` |
|        - |  5274 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - |  5275 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - |  5276 | `	GenBlock *pBlock;` |
|        - |  5277 | `	SyString *pName;` |
|        - |  5278 | `	char *zDup;` |
|        - |  5279 | `	sxu32 nLine;` |
|        - |  5280 | `	sxi32 rc;` |
|        - |  5281 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - |  5282 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - |  5283 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|       12 |  5284 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|       10 |  5285 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 |  5286 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 |  5287 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 |  5288 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5289 | `			return SXERR_ABORT;` |
|        3 |  5290 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 |  5291 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 |  5292 | `		}` |
|        3 |  5293 | `		return SXRET_OK;` |
|        - |  5294 | `	}` |
|        - |  5295 | `	/* Jump the static keyword */` |
|       13 |  5296 | `	nLine = pGen->pIn->nLine;` |
|       13 |  5297 | `	pGen->pIn++;` |
|        - |  5298 | `	/* Extract the enclosing function if any */` |
|       13 |  5299 | `	pBlock = pGen->pCurrent;` |
|       23 |  5300 | `	while( pBlock ){` |
|       23 |  5301 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       13 |  5302 | `			break;` |
|        - |  5303 | `		}` |
|        - |  5304 | `		/* Point to the upper block */` |
|       13 |  5305 | `		pBlock = pBlock->pParent;` |
|        3 |  5306 | `	}` |
|       13 |  5307 | `	if( pBlock == 0 ){` |
|        - |  5308 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 |  5309 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5310 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|      ! 0 |  5311 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5312 | `				return SXERR_ABORT;` |
|        - |  5313 | `			}` |
|      ! 0 |  5314 | `			goto Synchronize;` |
|        - |  5315 | `		}` |
|        - |  5316 | `		/* Compile the expression holding the variable */` |
|      ! 0 |  5317 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  5318 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5319 | `			return SXERR_ABORT;` |
|      ! 0 |  5320 | `		}else if( rc != SXERR_EMPTY ){` |
|        - |  5321 | `			/* Emit the POP instruction */` |
|      ! 0 |  5322 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  5323 | `		}` |
|      ! 0 |  5324 | `		return SXRET_OK;` |
|        - |  5325 | `	}` |
|       13 |  5326 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - |  5327 | `	/* Make sure we are dealing with a valid statement */` |
|       13 |  5328 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        8 |  5329 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        3 |  5330 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|        3 |  5331 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5332 | `				return SXERR_ABORT;` |
|        - |  5333 | `			}` |
|        3 |  5334 | `			goto Synchronize;` |
|        - |  5335 | `	}` |
|       10 |  5336 | `	pGen->pIn++;` |
|        - |  5337 | `	/* Extract variable name */` |
|       10 |  5338 | `	pName = &pGen->pIn->sData;` |
|       10 |  5339 | `	pGen->pIn++; /* Jump the var name */` |
|       10 |  5340 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 |  5341 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  5342 | `		goto Synchronize;` |
|        - |  5343 | `	}` |
|        - |  5344 | `	/* Initialize the structure describing the static variable */` |
|       10 |  5345 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       10 |  5346 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - |  5347 | `	/* Duplicate variable name */` |
|       10 |  5348 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       10 |  5349 | `	if( zDup == 0 ){` |
|      ! 0 |  5350 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  5351 | `		return SXERR_ABORT;` |
|        - |  5352 | `	}` |
|       10 |  5353 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - |  5354 | `	/* Check if we have an expression to compile */` |
|       10 |  5355 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - |  5356 | `		SySet *pInstrContainer;` |
|        - |  5357 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - |  5358 | `		 * Static variable can take any complex expression including function` |
|        - |  5359 | `		 * call as their initialization value.` |
|        - |  5360 | `		 * Example:` |
|        - |  5361 | `		 *		static $var = foo(1,4+5,bar());` |
|        - |  5362 | `		 */` |
|       10 |  5363 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - |  5364 | `		/* Swap bytecode container */` |
|       10 |  5365 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       10 |  5366 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - |  5367 | `		/* Compile the expression */` |
|       10 |  5368 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  5369 | `		/* Emit the done instruction */` |
|       10 |  5370 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - |  5371 | `		/* Restore default bytecode container */` |
|       10 |  5372 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        4 |  5373 | `	}` |
|        - |  5374 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       10 |  5375 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       10 |  5376 | `	return SXRET_OK;` |
|        1 |  5377 | `Synchronize:` |
|        - |  5378 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - |  5379 | `	 * statement.` |
|        - |  5380 | `	 */` |
|        5 |  5381 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 |  5382 | `		pGen->pIn++;` |
|        1 |  5383 | `	}` |
|        3 |  5384 | `	return SXRET_OK;` |
|        9 |  5385 | `}` |
|        - |  5386 | `/*` |
|        - |  5387 | ` * Compile the var statement.` |
|        - |  5388 | ` * Symisc Extension:` |
|        - |  5389 | ` *      var statement can be used outside of a class definition.` |
|        - |  5390 | ` */` |
|        4 |  5391 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 |  5392 | `{` |
|        - |  5393 | `	sxu32 nLine;` |
|        - |  5394 | `	sxi32 rc;` |
|        5 |  5395 | `	nLine = pGen->pIn->nLine;` |
|        - |  5396 | `	/* Jump the 'var' keyword */` |
|        5 |  5397 | `	pGen->pIn++;` |
|        5 |  5398 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  5399 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|        - |  5400 | `		/* Synchronize with the first semi-colon */` |
|      ! 0 |  5401 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|      ! 0 |  5402 | `			pGen->pIn++;` |
|      ! 0 |  5403 | `		}` |
|      ! 0 |  5404 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5405 | `			return SXERR_ABORT;` |
|        - |  5406 | `		}` |
|      ! 0 |  5407 | `	}else{` |
|        - |  5408 | `		/* Compile the expression */` |
|        5 |  5409 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        5 |  5410 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5411 | `			return SXERR_ABORT;` |
|        5 |  5412 | `		}else if( rc != SXERR_EMPTY ){` |
|        5 |  5413 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 |  5414 | `		}` |
|        - |  5415 | `	}` |
|        5 |  5416 | `	return SXRET_OK;` |
|        3 |  5417 | `}` |
|        - |  5418 | `/*` |
|        - |  5419 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - |  5420 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - |  5421 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - |  5422 | ` */` |
|        - |  5423 | `/*` |
|        - |  5424 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - |  5425 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - |  5426 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - |  5427 | ` * qualified name and updates the instruction's operand index.` |
|        - |  5428 | ` *` |
|        - |  5429 | ` * Resolution order:` |
|        - |  5430 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - |  5431 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - |  5432 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - |  5433 | ` *` |
|        - |  5434 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - |  5435 | ` * came from an import (step 1) and 0 otherwise.` |
|        - |  5436 | ` * Returns the (possibly new) literal index.` |
|        - |  5437 | ` */` |
|  2877156 |  5438 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 |  5439 | `{` |
|        - |  5440 | `	ph7_value *pLit;` |
|        - |  5441 | `	const char *zLit;` |
|        - |  5442 | `	SyString sQualified;` |
|        - |  5443 | `	sxu32 nLit;` |
|        - |  5444 | `	sxu32 k;` |
|        - |  5445 | `	sxu32 nNewIdx;` |
|        - |  5446 | `	int hasNsSep;` |
|        - |  5447 | `	SyHashEntry *pImport;` |
|        - |  5448 | `	ph7_value *pNew;` |
|  2877161 |  5449 | `	if( pFromImport ){` |
|  2345929 |  5450 | `		*pFromImport = 0;` |
|  1172962 |  5451 | `	}` |
|  2877161 |  5452 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  2877161 |  5453 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 |  5454 | `		return nOrigIdx;` |
|        - |  5455 | `	}` |
|  2877161 |  5456 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  2877161 |  5457 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - |  5458 | `	/* Skip if already qualified (contains backslash) */` |
|  2877161 |  5459 | `	hasNsSep = 0;` |
| 37068827 |  5460 | `	for( k = 0; k < nLit; k++ ){` |
| 34191679 |  5461 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 17095838 |  5462 | `	}` |
|  2877161 |  5463 | `	if( hasNsSep ){` |
|       10 |  5464 | `		return nOrigIdx;` |
|        - |  5465 | `	}` |
|        - |  5466 | `	/* Check use imports first (works even outside namespaces) */` |
|  2877153 |  5467 | `	SyBlobReset(&pGen->sWorker);` |
|  2877153 |  5468 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  2877153 |  5469 | `	if( pImport ){` |
|       41 |  5470 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 |  5471 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 |  5472 | `		if( pFromImport ){` |
|       18 |  5473 | `			*pFromImport = 1;` |
|        8 |  5474 | `		}` |
|       23 |  5475 | `	}else{` |
|  2877117 |  5476 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  2877027 |  5477 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - |  5478 | `		}` |
|        - |  5479 | `		/* Prepend current namespace */` |
|       95 |  5480 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       95 |  5481 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       95 |  5482 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - |  5483 | `	}` |
|        - |  5484 | `	/* Look up or create a new literal for the qualified name */` |
|      131 |  5485 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      131 |  5486 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       57 |  5487 | `		return nNewIdx; /* Already interned */` |
|        - |  5488 | `	}` |
|       79 |  5489 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|       79 |  5490 | `	if( pNew == 0 ){` |
|      ! 0 |  5491 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - |  5492 | `	}` |
|       79 |  5493 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|       79 |  5494 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|       79 |  5495 | `	return nNewIdx;` |
|  1438583 |  5496 | `}` |
|        - |  5497 | `/*` |
|        - |  5498 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - |  5499 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - |  5500 | ` */` |
|   187740 |  5501 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5502 | `{` |
|        - |  5503 | `	SyHashEntry *pImport;` |
|        - |  5504 | `	/* Check use imports first */` |
|   187745 |  5505 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   187745 |  5506 | `	if( pImport ){` |
|       19 |  5507 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       19 |  5508 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       19 |  5509 | `		return;` |
|        - |  5510 | `	}` |
|        - |  5511 | `	/* Prepend current namespace if active */` |
|   187729 |  5512 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        8 |  5513 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  5514 | `		SyBlobAppend(pOut,"\\",1);` |
|        3 |  5515 | `	}` |
|   187729 |  5516 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    93875 |  5517 | `}` |
|        - |  5518 | `/*` |
|        - |  5519 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - |  5520 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - |  5521 | ` * The caller must release pOut when done.` |
|        - |  5522 | ` */` |
|   262012 |  5523 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5524 | `{` |
|   262017 |  5525 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3947 |  5526 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3947 |  5527 | `		SyBlobAppend(pOut,"\\",1);` |
|     1971 |  5528 | `	}` |
|   262017 |  5529 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   262017 |  5530 | `}` |
|        - |  5531 | `/*` |
|        - |  5532 | ` * Compile a namespace statement` |
|        - |  5533 | ` * According to the PHP language reference manual` |
|        - |  5534 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - |  5535 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - |  5536 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - |  5537 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - |  5538 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - |  5539 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - |  5540 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - |  5541 | ` *  programming world.` |
|        - |  5542 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - |  5543 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - |  5544 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - |  5545 | ` *  classes/functions/constants.` |
|        - |  5546 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - |  5547 | ` *  readability of source code.` |
|        - |  5548 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - |  5549 | ` *  Here is an example of namespace syntax in PHP:` |
|        - |  5550 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - |  5551 | ` *       class MyClass {}` |
|        - |  5552 | ` *       function myfunction() {}` |
|        - |  5553 | ` *       const MYCONST = 1;` |
|        - |  5554 | ` *       $a = new MyClass;` |
|        - |  5555 | ` *       $c = new \my\name\MyClass;` |
|        - |  5556 | ` *       $a = strlen('hi');` |
|        - |  5557 | ` *       $d = namespace\MYCONST;` |
|        - |  5558 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - |  5559 | ` *       echo constant($d);` |
|        - |  5560 | ` * NOTE` |
|        - |  5561 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5562 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5563 | ` */` |
|        - |  5564 | `/*` |
|        - |  5565 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - |  5566 | ` */` |
|       14 |  5567 | `static const char * TokenTypeName(sxu32 nType)` |
|        3 |  5568 | `{` |
|       17 |  5569 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 |  5570 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 |  5571 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 |  5572 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 |  5573 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 |  5574 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 |  5575 | `	return "token";` |
|       10 |  5576 | `}` |
|     3990 |  5577 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 |  5578 | `{` |
|        - |  5579 | `	sxu32 nLine;` |
|        - |  5580 | `	sxi32 rc;` |
|     3995 |  5581 | `	nLine = pGen->pIn->nLine;` |
|     3995 |  5582 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - |  5583 | `	/* Reset namespace and clear previous use imports */` |
|     3995 |  5584 | `	SyBlobReset(&pGen->sNamespace);` |
|     3995 |  5585 | `	SyHashRelease(&pGen->hUseImports);` |
|     3995 |  5586 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5587 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     3995 |  5588 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5589 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     3995 |  5590 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5591 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5592 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 |  5593 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5594 | `		return SXRET_OK;` |
|        - |  5595 | `	}` |
|     3995 |  5596 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - |  5597 | `		/* namespace; — switch to global namespace */` |
|      ! 0 |  5598 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5599 | `		return SXRET_OK;` |
|        - |  5600 | `	}` |
|     3995 |  5601 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - |  5602 | `		/* namespace { } — global namespace block */` |
|      ! 0 |  5603 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5604 | `		return SXRET_OK;` |
|        - |  5605 | `	}` |
|        - |  5606 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8027 |  5607 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4037 |  5608 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - |  5609 | `			/* Append backslash separator */` |
|       26 |  5610 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       26 |  5611 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       11 |  5612 | `			}` |
|       15 |  5613 | `		}else{` |
|        - |  5614 | `			/* Append identifier */` |
|     4015 |  5615 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  5616 | `		}` |
|     4037 |  5617 | `		pGen->pIn++;` |
|        5 |  5618 | `	}` |
|        - |  5619 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - |  5620 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - |  5621 | `	{` |
|     3995 |  5622 | `		char *zNsDup = 0;` |
|     3995 |  5623 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     5987 |  5624 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3988 |  5625 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     1994 |  5626 | `		}` |
|     3995 |  5627 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - |  5628 | `	}` |
|     3995 |  5629 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 |  5630 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  5631 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 |  5632 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 |  5633 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5634 | `			return SXERR_ABORT;` |
|        - |  5635 | `		}` |
|        2 |  5636 | `	}` |
|     3995 |  5637 | `	return SXRET_OK;` |
|     2000 |  5638 | `}` |
|        - |  5639 | `/*` |
|        - |  5640 | ` * Compile the 'use' statement` |
|        - |  5641 | ` * According to the PHP language reference manual` |
|        - |  5642 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - |  5643 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - |  5644 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - |  5645 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - |  5646 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - |  5647 | ` *  a function or constant is not supported.` |
|        - |  5648 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - |  5649 | ` * NOTE` |
|        - |  5650 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5651 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5652 | ` */` |
|       72 |  5653 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 |  5654 | `{` |
|        - |  5655 | `	sxu32 nLine;` |
|        - |  5656 | `	sxi32 rc;` |
|        - |  5657 | `	SyBlob sPath;` |
|        - |  5658 | `	SyString sAlias;` |
|        - |  5659 | `	SyToken *pLast;` |
|        - |  5660 | `	char *zDup;` |
|        - |  5661 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - |  5662 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - |  5663 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       77 |  5664 | `	nLine = pGen->pIn->nLine;` |
|       77 |  5665 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - |  5666 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       77 |  5667 | `	iUseType = 0;` |
|       77 |  5668 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 |  5669 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 |  5670 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 |  5671 | `			iUseType = 1;` |
|       16 |  5672 | `			pGen->pIn++;` |
|       23 |  5673 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 |  5674 | `			iUseType = 2;` |
|       16 |  5675 | `			pGen->pIn++;` |
|        7 |  5676 | `		}` |
|       14 |  5677 | `	}` |
|        - |  5678 | `	/* Select target hash tables based on import type */` |
|       77 |  5679 | `	switch( iUseType ){` |
|        7 |  5680 | `		case 1:` |
|       16 |  5681 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 |  5682 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 |  5683 | `			break;` |
|        7 |  5684 | `		case 2:` |
|       16 |  5685 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 |  5686 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 |  5687 | `			break;` |
|       22 |  5688 | `		default:` |
|       49 |  5689 | `			pGenHash = &pGen->hUseImports;` |
|       49 |  5690 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       44 |  5691 | `			break;` |
|        - |  5692 | `	}` |
|       77 |  5693 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - |  5694 | `	/* Process one or more use declarations separated by commas */` |
|       37 |  5695 | `	for(;;){` |
|       79 |  5696 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  5697 | `			break;` |
|        - |  5698 | `		}` |
|       79 |  5699 | `		SyBlobReset(&sPath);` |
|       79 |  5700 | `		pLast = 0;` |
|        - |  5701 | `		/* Collect the full namespace path */` |
|      269 |  5702 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      195 |  5703 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      135 |  5704 | `				pLast = pGen->pIn;` |
|      135 |  5705 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       65 |  5706 | `					SyBlobAppend(&sPath,"\\",1);` |
|       30 |  5707 | `				}` |
|      135 |  5708 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       65 |  5709 | `			}` |
|      195 |  5710 | `			pGen->pIn++;` |
|        5 |  5711 | `		}` |
|       79 |  5712 | `		if( pLast == 0 ){` |
|        - |  5713 | `			/* Empty path */` |
|        6 |  5714 | `			break;` |
|        - |  5715 | `		}` |
|        - |  5716 | `		/* Default alias is the last component of the path */` |
|       75 |  5717 | `		sAlias = pLast->sData;` |
|        - |  5718 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       70 |  5719 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       50 |  5720 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       24 |  5721 | `			pGen->pIn++; /* Jump 'as' */` |
|       24 |  5722 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       24 |  5723 | `				sAlias = pGen->pIn->sData;` |
|       24 |  5724 | `				pGen->pIn++;` |
|       10 |  5725 | `			}` |
|       10 |  5726 | `		}` |
|        - |  5727 | `		/* Check for duplicate import alias (per-type) */` |
|       75 |  5728 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 |  5729 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  5730 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 |  5731 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 |  5732 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5733 | `				SyBlobRelease(&sPath);` |
|      ! 0 |  5734 | `				return SXERR_ABORT;` |
|        - |  5735 | `			}` |
|        2 |  5736 | `		}` |
|        - |  5737 | `		/* Register the import: alias -> FQN.` |
|        - |  5738 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - |  5739 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - |  5740 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      110 |  5741 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       70 |  5742 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       75 |  5743 | `		if( zDup ){` |
|       75 |  5744 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       75 |  5745 | `			if( pVmHash ){` |
|        - |  5746 | `				/* Class imports: populate VM table directly (class resolution` |
|        - |  5747 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       47 |  5748 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       47 |  5749 | `				if( zAliasDup ){` |
|       47 |  5750 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       21 |  5751 | `				}` |
|       21 |  5752 | `			}` |
|       75 |  5753 | `			if( iUseType == 2 ){` |
|        - |  5754 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - |  5755 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 |  5756 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 |  5757 | `				if( zAliasDup ){` |
|        - |  5758 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - |  5759 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - |  5760 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 |  5761 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 |  5762 | `					if( azPair ){` |
|       16 |  5763 | `						azPair[0] = zAliasDup;` |
|       16 |  5764 | `						azPair[1] = zDup;` |
|       16 |  5765 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 |  5766 | `					}` |
|        7 |  5767 | `				}` |
|        7 |  5768 | `			}` |
|       35 |  5769 | `		}` |
|        - |  5770 | `		/* Check for comma (multiple use declarations) */` |
|       75 |  5771 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 |  5772 | `			pGen->pIn++;` |
|        2 |  5773 | `		}else{` |
|       39 |  5774 | `			break;` |
|        - |  5775 | `		}` |
|        1 |  5776 | `	}` |
|       77 |  5777 | `	SyBlobRelease(&sPath);` |
|       77 |  5778 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 |  5779 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 |  5780 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 |  5781 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5782 | `			return SXERR_ABORT;` |
|        - |  5783 | `		}` |
|        1 |  5784 | `	}` |
|       77 |  5785 | `	return SXRET_OK;` |
|       41 |  5786 | `}` |
|        - |  5787 | `/*` |
|        - |  5788 | ` * Compile the stupid 'declare' language construct.` |
|        - |  5789 | ` *` |
|        - |  5790 | ` * According to the PHP language reference manual.` |
|        - |  5791 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - |  5792 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - |  5793 | ` *  declare (directive)` |
|        - |  5794 | ` *   statement` |
|        - |  5795 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - |  5796 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - |  5797 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - |  5798 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - |  5799 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - |  5800 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - |  5801 | ` * <?php` |
|        - |  5802 | ` * // these are the same:` |
|        - |  5803 | ` * // you can use this:` |
|        - |  5804 | ` * declare(ticks=1) {` |
|        - |  5805 | ` *   // entire script here` |
|        - |  5806 | ` * }` |
|        - |  5807 | ` * // or you can use this:` |
|        - |  5808 | ` * declare(ticks=1);` |
|        - |  5809 | ` * // entire script here` |
|        - |  5810 | ` * ?>` |
|        - |  5811 | ` *` |
|        - |  5812 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - |  5813 | ` */` |
|        - |  5814 | `/*` |
|        - |  5815 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - |  5816 | ` */` |
|       72 |  5817 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 |  5818 | `{` |
|      109 |  5819 | `	return SyStringLength(pName) == nWant` |
|       72 |  5820 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 |  5821 | `}` |
|        - |  5822 |  |
|       42 |  5823 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 |  5824 | `{` |
|       47 |  5825 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       47 |  5826 | `	SyToken *pBodyEnd = 0;` |
|        - |  5827 | `	SyToken *pBodyStart;` |
|        - |  5828 | `	SyToken *pCursor;` |
|        - |  5829 | `	int bHasStrictTypes;` |
|        - |  5830 | `	int bBlockForm;` |
|        - |  5831 | `	int bPlacementOk;` |
|        - |  5832 | `	sxi32 rc;` |
|       47 |  5833 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       47 |  5834 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 |  5835 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|        6 |  5836 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5837 | `			return SXERR_ABORT;` |
|        - |  5838 | `		}` |
|        6 |  5839 | `		goto Synchro;` |
|        - |  5840 | `	}` |
|       43 |  5841 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       43 |  5842 | `	pBodyStart = pGen->pIn;` |
|        - |  5843 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       43 |  5844 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       43 |  5845 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  5846 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|      ! 0 |  5847 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5848 | `			return SXERR_ABORT;` |
|        - |  5849 | `		}` |
|      ! 0 |  5850 | `		return SXRET_OK;` |
|        - |  5851 | `	}` |
|        - |  5852 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - |  5853 | `	 * now delimits the comma-separated directive list. */` |
|       43 |  5854 | `	pGen->pIn = &pBodyEnd[1];` |
|       43 |  5855 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 |  5856 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 |  5857 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5858 | `			return SXERR_ABORT;` |
|        - |  5859 | `		}` |
|      ! 0 |  5860 | `	}` |
|       43 |  5861 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       43 |  5862 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       43 |  5863 | `	bHasStrictTypes = 0;` |
|        - |  5864 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - |  5865 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - |  5866 | `	 * directive appears anywhere in the list, before validating values. */` |
|       43 |  5867 | `	pCursor = pBodyStart;` |
|       55 |  5868 | `	while( pCursor < pBodyEnd ){` |
|       51 |  5869 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       43 |  5870 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       39 |  5871 | `				bHasStrictTypes = 1;` |
|       39 |  5872 | `				break;` |
|        - |  5873 | `			}` |
|        2 |  5874 | `		}` |
|       14 |  5875 | `		pCursor++;` |
|        2 |  5876 | `	}` |
|       43 |  5877 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 |  5878 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5879 | `			"strict_types declaration must not use block mode");` |
|        3 |  5880 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5881 | `		return SXRET_OK;` |
|        - |  5882 | `	}` |
|       41 |  5883 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 |  5884 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5885 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 |  5886 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 |  5887 | `		return SXRET_OK;` |
|        - |  5888 | `	}` |
|        - |  5889 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       37 |  5890 | `	pCursor = pBodyStart;` |
|       69 |  5891 | `	while( pCursor < pBodyEnd ){` |
|        - |  5892 | `		SyToken *pNameTok;` |
|        - |  5893 | `		SyToken *pEqTok;` |
|        - |  5894 | `		SyToken *pValTok;` |
|        - |  5895 | `		SyString *pDirName;` |
|        - |  5896 | `		int bIsStrict;` |
|        - |  5897 | `		int iStrictValue;` |
|       39 |  5898 | `		pNameTok = pCursor;` |
|       39 |  5899 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  5900 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5901 | `				"declare: Expecting a directive name");` |
|      ! 0 |  5902 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5903 | `			return SXRET_OK;` |
|        - |  5904 | `		}` |
|       39 |  5905 | `		pEqTok = pNameTok + 1;` |
|       39 |  5906 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 |  5907 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5908 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 |  5909 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5910 | `			return SXRET_OK;` |
|        - |  5911 | `		}` |
|       39 |  5912 | `		pValTok = pEqTok + 1;` |
|       39 |  5913 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 |  5914 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5915 | `				"declare: Expecting value after '='");` |
|      ! 0 |  5916 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5917 | `			return SXRET_OK;` |
|        - |  5918 | `		}` |
|       39 |  5919 | `		pDirName = &pNameTok->sData;` |
|       39 |  5920 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       39 |  5921 | `		if( bIsStrict ){` |
|        - |  5922 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - |  5923 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       35 |  5924 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 |  5925 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5926 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 |  5927 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5928 | `				return SXRET_OK;` |
|        - |  5929 | `			}` |
|       35 |  5930 | `			iStrictValue = -1;` |
|       35 |  5931 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       35 |  5932 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       35 |  5933 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       35 |  5934 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       33 |  5935 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       15 |  5936 | `			}` |
|       35 |  5937 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 |  5938 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5939 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 |  5940 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5941 | `				return SXRET_OK;` |
|        - |  5942 | `			}` |
|       32 |  5943 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       18 |  5944 | `		}else{` |
|        - |  5945 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|        - |  5946 | `			 * preserve the legacy notice so callers relying on the old` |
|        - |  5947 | `			 * behavior don't regress. */` |
|        8 |  5948 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|        - |  5949 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|        2 |  5950 | `				ph7_lib_version()` |
|        - |  5951 | `				);` |
|        - |  5952 | `		}` |
|       36 |  5953 | `		pCursor = pValTok + 1;` |
|        - |  5954 | `		/* Consume separating comma (or end). */` |
|       36 |  5955 | `		if( pCursor < pBodyEnd ){` |
|        3 |  5956 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  5957 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5958 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 |  5959 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5960 | `				return SXRET_OK;` |
|        - |  5961 | `			}` |
|        3 |  5962 | `			pCursor++;` |
|        1 |  5963 | `		}` |
|        4 |  5964 | `	}` |
|        - |  5965 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - |  5966 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - |  5967 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       34 |  5968 | `	return SXRET_OK;` |
|        2 |  5969 | `Synchro:` |
|        - |  5970 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 |  5971 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 |  5972 | `		pGen->pIn++;` |
|        2 |  5973 | `	}` |
|        6 |  5974 | `	return SXRET_OK;` |
|       26 |  5975 | `}` |
|        - |  5976 | `/*` |
|        - |  5977 | ` * Process default argument values. That is,a function may define C++-style default value` |
|        - |  5978 | ` * as follows:` |
|        - |  5979 | ` * function makecoffee($type = "cappuccino")` |
|        - |  5980 | ` * {` |
|        - |  5981 | ` *   return "Making a cup of $type.\n";` |
|        - |  5982 | ` * }` |
|        - |  5983 | ` * Symisc eXtension.` |
|        - |  5984 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|        - |  5985 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|        - |  5986 | ` *      Example: Work only with PH7,generate error under zend` |
|        - |  5987 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|        - |  5988 | ` *      {` |
|        - |  5989 | ` *       var_dump($a);` |
|        - |  5990 | ` *      }` |
|        - |  5991 | ` *     //call test without args` |
|        - |  5992 | ` *      test();` |
|        - |  5993 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|        - |  5994 | ` *      Example:` |
|        - |  5995 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|        - |  5996 | ` * 3 -) Function overloading!!` |
|        - |  5997 | ` *      Example:` |
|        - |  5998 | ` *      function foo($a) {` |
|        - |  5999 | ` *   	  return $a.PHP_EOL;` |
|        - |  6000 | ` *	    }` |
|        - |  6001 | ` *	    function foo($a, $b) {` |
|        - |  6002 | ` *   	  return $a + $b;` |
|        - |  6003 | ` *	    }` |
|        - |  6004 | ` *	    echo foo(5); // Prints "5"` |
|        - |  6005 | ` *	    echo foo(5, 2); // Prints "7"` |
|        - |  6006 | ` *      // Same arg` |
|        - |  6007 | ` *	   function foo(string $a)` |
|        - |  6008 | ` *	   {` |
|        - |  6009 | ` *	     echo "a is a string\n";` |
|        - |  6010 | ` *	     var_dump($a);` |
|        - |  6011 | ` *	   }` |
|        - |  6012 | ` *	  function foo(int $a)` |
|        - |  6013 | ` *	  {` |
|        - |  6014 | ` *	    echo "a is integer\n";` |
|        - |  6015 | ` *	    var_dump($a);` |
|        - |  6016 | ` *	  }` |
|        - |  6017 | ` *	  function foo(array $a)` |
|        - |  6018 | ` *	  {` |
|        - |  6019 | ` * 	    echo "a is an array\n";` |
|        - |  6020 | ` * 	    var_dump($a);` |
|        - |  6021 | ` *	  }` |
|        - |  6022 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|        - |  6023 | ` *	  foo(52); // a is integer [second foo]` |
|        - |  6024 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|        - |  6025 | ` * Please refer to the official documentation for more information on the powerful extension` |
|        - |  6026 | ` * introduced by the PH7 engine.` |
|        - |  6027 | ` */` |
|   240958 |  6028 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |  6029 | `{` |
|        - |  6030 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6031 | `	SySet *pInstrContainer;` |
|        - |  6032 | `	sxi32 rc;` |
|        - |  6033 | `	/* Swap token stream */` |
|   240963 |  6034 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   240963 |  6035 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   240963 |  6036 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |  6037 | `	/* Compile the expression holding the argument value */` |
|   240963 |  6038 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  6039 | `	/* Emit the done instruction */` |
|   240963 |  6040 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   240963 |  6041 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   240963 |  6042 | `	RE_SWAP_DELIMITER(pGen);` |
|   240963 |  6043 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  6044 | `		return SXERR_ABORT;` |
|        - |  6045 | `	}` |
|   240963 |  6046 | `	return SXRET_OK;` |
|   120484 |  6047 | `}` |
|        - |  6048 | `/*` |
|        - |  6049 | ` * Collect function arguments one after one.` |
|        - |  6050 | ` * According to the PHP language reference manual.` |
|        - |  6051 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|        - |  6052 | ` * list of expressions.` |
|        - |  6053 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|        - |  6054 | ` * and default argument values. Variable-length argument lists are also supported,` |
|        - |  6055 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|        - |  6056 | ` * for more information.` |
|        - |  6057 | ` * Example #1 Passing arrays to functions` |
|        - |  6058 | ` * <?php` |
|        - |  6059 | ` * function takes_array($input)` |
|        - |  6060 | ` * {` |
|        - |  6061 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|        - |  6062 | ` * }` |
|        - |  6063 | ` * ?>` |
|        - |  6064 | ` * Making arguments be passed by reference` |
|        - |  6065 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|        - |  6066 | ` * within the function is changed, it does not get changed outside of the function).` |
|        - |  6067 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|        - |  6068 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|        - |  6069 | ` * to the argument name in the function definition:` |
|        - |  6070 | ` * Example #2 Passing function parameters by reference` |
|        - |  6071 | ` * <?php` |
|        - |  6072 | ` * function add_some_extra(&$string)` |
|        - |  6073 | ` * {` |
|        - |  6074 | ` *   $string .= 'and something extra.';` |
|        - |  6075 | ` * }` |
|        - |  6076 | ` * $str = 'This is a string, ';` |
|        - |  6077 | ` * add_some_extra($str);` |
|        - |  6078 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|        - |  6079 | ` * ?>` |
|        - |  6080 | ` *` |
|        - |  6081 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|        - |  6082 | ` * complex agrument values.Please refer to the official documentation for more information` |
|        - |  6083 | ` * on these extension.` |
|        - |  6084 | ` */` |
|   491212 |  6085 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  6086 | `{` |
|        - |  6087 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  6088 | `	SyToken *pIn;  /* Token stream */` |
|        - |  6089 | `	SyBlob sSig;         /* Function signature */` |
|        - |  6090 | `	char *zDup;          /* Copy of argument name */` |
|        - |  6091 | `	sxi32 rc;` |
|        - |  6092 |  |
|   491217 |  6093 | `	pIn = pGen->pIn;` |
|   491217 |  6094 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  6095 | `	/* Process arguments one after one */` |
|   604302 |  6096 | `	for(;;){` |
|  1208609 |  6097 | `		if( pIn >= pEnd ){` |
|        - |  6098 | `			/* No more arguments to process */` |
|   491201 |  6099 | `			break;` |
|        - |  6100 | `		}` |
|   717413 |  6101 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   717413 |  6102 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   717413 |  6103 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   717413 |  6104 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   717413 |  6105 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  6106 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  6107 | `		 * first token inside the main token stream */` |
|   717413 |  6108 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  6109 | `			return SXERR_ABORT;` |
|        - |  6110 | `		}` |
|        - |  6111 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|        - |  6112 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|        - |  6113 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|        - |  6114 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|        - |  6115 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|        - |  6116 | `		{` |
|   717413 |  6117 | `			int bReadonly = 0, bVisSeen = 0;` |
|   717413 |  6118 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   717413 |  6119 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        3 |  6120 | `				bReadonly = 1;` |
|        3 |  6121 | `				pIn++;` |
|        1 |  6122 | `			}` |
|   717413 |  6123 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    81939 |  6124 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    81939 |  6125 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|       83 |  6126 | `					bVisSeen = 1;` |
|       83 |  6127 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      111 |  6128 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|       36 |  6129 | `						: PH7_CLASS_PROT_PUBLIC;` |
|       83 |  6130 | `					pIn++;` |
|       83 |  6131 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       18 |  6132 | `						bReadonly = 1;` |
|       18 |  6133 | `						pIn++;` |
|        7 |  6134 | `					}` |
|       39 |  6135 | `				}` |
|    40967 |  6136 | `			}` |
|   717413 |  6137 | `			if( bVisSeen \|\| bReadonly ){` |
|       85 |  6138 | `				if( !bCtorCtx ){` |
|        6 |  6139 | `					if( bAbstractCtx ){` |
|        3 |  6140 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6141 | `							"Cannot declare promoted property in an abstract constructor");` |
|        2 |  6142 | `					}else{` |
|        3 |  6143 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6144 | `							"Cannot declare promoted property outside a constructor");` |
|        - |  6145 | `					}` |
|        6 |  6146 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  6147 | `						return SXERR_ABORT;` |
|        - |  6148 | `					}` |
|        6 |  6149 | `					return SXERR_SYNTAX;` |
|        - |  6150 | `				}` |
|       81 |  6151 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|       81 |  6152 | `				sArg.iPromoteVis = iVis;` |
|       81 |  6153 | `				if( bReadonly ){` |
|       20 |  6154 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|        8 |  6155 | `				}` |
|       38 |  6156 | `			}` |
|        - |  6157 | `		}` |
|        - |  6158 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   717404 |  6159 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   419204 |  6160 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   119051 |  6161 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    97591 |  6162 | `			sxu32 nLineLocal = pIn->nLine;` |
|    97591 |  6163 | `			sxi32 iTFlags = 0;` |
|    97591 |  6164 | `			pGen->pIn = pIn;` |
|    97591 |  6165 | `			rc = GenStateParseUnionTypeDecl(` |
|    48793 |  6166 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|    48793 |  6167 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  6168 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  6169 | `				/* bAllowVoid */ 0,` |
|    48793 |  6170 | `						nLineLocal);` |
|    97591 |  6171 | `			pIn = pGen->pIn;` |
|    97591 |  6172 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  6173 | `				return SXERR_ABORT;` |
|    97591 |  6174 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  6175 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  6176 | `				return SXERR_SYNTAX;` |
|    97589 |  6177 | `			}else if( rc == SXERR_SYNTAX ){` |
|       12 |  6178 | `				if( pIn < pEnd ){` |
|       16 |  6179 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  6180 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        4 |  6181 | `						&pIn->sData);` |
|        8 |  6182 | `				}else{` |
|      ! 0 |  6183 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  6184 | `						"syntax error, unexpected end of file");` |
|        - |  6185 | `				}` |
|       12 |  6186 | `				return SXERR_SYNTAX;` |
|        - |  6187 | `			}` |
|    97581 |  6188 | `			sArg.iFlags \|= iTFlags;` |
|    48788 |  6189 | `		}` |
|   717399 |  6190 | `		if( pIn >= pEnd ){` |
|      ! 0 |  6191 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  6192 | `			return rc;` |
|        - |  6193 | `		}` |
|   717399 |  6194 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  6195 | `			/* Pass by reference,record that */` |
|     3929 |  6196 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     3929 |  6197 | `			pIn++;` |
|     1962 |  6198 | `		}` |
|   717399 |  6199 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  6200 | `			/* Variadic parameter: ...$args */` |
|    19529 |  6201 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    19529 |  6202 | `			pIn++;` |
|     9762 |  6203 | `		}` |
|   717399 |  6204 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  6205 | `			/* Invalid argument */` |
|      ! 0 |  6206 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|      ! 0 |  6207 | `			return rc;` |
|        - |  6208 | `		}` |
|   717399 |  6209 | `		pIn++; /* Jump the dollar sign */` |
|        - |  6210 | `		/* Copy argument name */` |
|   717399 |  6211 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   717399 |  6212 | `		if( zDup == 0 ){` |
|      ! 0 |  6213 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  6214 | `			return SXERR_ABORT;` |
|        - |  6215 | `		}` |
|   717399 |  6216 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   717399 |  6217 | `		pIn++;` |
|   717399 |  6218 | `		if( pIn < pEnd ){` |
|   373903 |  6219 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  6220 | `				SyToken *pDefend;` |
|   240965 |  6221 | `				sxi32 iNest = 0;` |
|   240965 |  6222 | `				pIn++; /* Jump the equal sign */` |
|   240965 |  6223 | `				pDefend = pIn;` |
|        - |  6224 | `				/* Process the default value associated with this argument */` |
|   513023 |  6225 | `				while( pDefend < pEnd ){` |
|   365327 |  6226 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    93269 |  6227 | `						break;` |
|        - |  6228 | `					}` |
|   272063 |  6229 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  6230 | `						/* Increment nesting level */` |
|    15549 |  6231 | `						iNest++;` |
|   264291 |  6232 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  6233 | `						/* Decrement nesting level */` |
|    15549 |  6234 | `						iNest--;` |
|     7772 |  6235 | `					}` |
|   272063 |  6236 | `					pDefend++;` |
|        5 |  6237 | `				}` |
|   240965 |  6238 | `				if( pIn >= pDefend ){` |
|        3 |  6239 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|        3 |  6240 | `					return rc;` |
|        - |  6241 | `				}` |
|        - |  6242 | `				/* Process default value */` |
|   240963 |  6243 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   240963 |  6244 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  6245 | `					return rc;` |
|        - |  6246 | `				}` |
|        - |  6247 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  6248 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  6249 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  6250 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  6251 | `				 * arg-type check lets null through. */` |
|   240958 |  6252 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|   145744 |  6253 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|   145741 |  6254 | `					&& &pIn[1] == pDefend` |
|    46639 |  6255 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|    34974 |  6256 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|    21373 |  6257 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|    15547 |  6258 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|     7771 |  6259 | `				}` |
|        - |  6260 | `				/* Point beyond the default value */` |
|   240963 |  6261 | `				pIn = pDefend;` |
|   120479 |  6262 | `			}` |
|   373901 |  6263 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  6264 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  6265 | `				return rc;` |
|        - |  6266 | `			}` |
|   373901 |  6267 | `			pIn++; /* Jump the trailing comma */` |
|   186948 |  6268 | `		}` |
|        - |  6269 | `		/* Append argument signature */` |
|   717397 |  6270 | `		if( sArg.nType > 0 ){` |
|    97519 |  6271 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  6272 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    15621 |  6273 | `				int marker = 'o';` |
|    15621 |  6274 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    15621 |  6275 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     7813 |  6276 | `			}else{` |
|        - |  6277 | `				int c;` |
|    81903 |  6278 | `				c = 'n'; /* cc warning */` |
|        - |  6279 | `				/* Type leading character */` |
|    81903 |  6280 | `				switch(sArg.nType){` |
|     5832 |  6281 | `				case MEMOBJ_HASHMAP:` |
|        - |  6282 | `					/* Hashmap aka 'array' */` |
|    11669 |  6283 | `					c = 'h';` |
|    11669 |  6284 | `					break;` |
|     9821 |  6285 | `				case MEMOBJ_INT:` |
|        - |  6286 | `					/* Integer */` |
|    19647 |  6287 | `					c = 'i';` |
|    19647 |  6288 | `					break;` |
|        2 |  6289 | `				case MEMOBJ_BOOL:` |
|        - |  6290 | `					/* Bool */` |
|        5 |  6291 | `					c = 'b';` |
|        5 |  6292 | `					break;` |
|        5 |  6293 | `				case MEMOBJ_REAL:` |
|        - |  6294 | `					/* Float */` |
|       12 |  6295 | `					c = 'f';` |
|       12 |  6296 | `					break;` |
|    25281 |  6297 | `				case MEMOBJ_STRING:` |
|        - |  6298 | `					/* String */` |
|    50567 |  6299 | `					c = 's';` |
|    50567 |  6300 | `					break;` |
|        7 |  6301 | `				case MEMOBJ_OBJ:` |
|        - |  6302 | `					/* Object */` |
|       16 |  6303 | `					c = 'o';` |
|       14 |  6304 | `					break;` |
|        1 |  6305 | `				default:` |
|        2 |  6306 | `					break;` |
|        - |  6307 | `				}` |
|    81903 |  6308 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  6309 | `			}` |
|    48762 |  6310 | `		}else{` |
|        - |  6311 | `			/* No type is associated with this parameter which mean` |
|        - |  6312 | `			 * that this function is not condidate for overloading.` |
|        - |  6313 | `			 */` |
|   619883 |  6314 | `			SyBlobRelease(&sSig);` |
|        - |  6315 | `		}` |
|        - |  6316 | `		/* Save in the argument set */` |
|   717397 |  6317 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  6318 | `	}` |
|   491201 |  6319 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  6320 | `		/* Save function signature */` |
|    66377 |  6321 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|    33186 |  6322 | `	}` |
|   491201 |  6323 | `	return SXRET_OK;` |
|   245611 |  6324 | `}` |
|        - |  6325 | `/*` |
|        - |  6326 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  6327 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  6328 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  6329 | ` */` |
|    34998 |  6330 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  6331 | `{` |
|    35003 |  6332 | `	sxi32 iParen = 0;` |
|    35003 |  6333 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  6334 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  6335 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  6336 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|   155593 |  6337 | `	while( pIn < pEnd ){` |
|   155593 |  6338 | `		sxu32 t = pIn->nType;` |
|   155593 |  6339 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|   151655 |  6340 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|   104993 |  6341 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|    85531 |  6342 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|   120595 |  6343 | `		pIn++;` |
|        5 |  6344 | `	}` |
|    19467 |  6345 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  6346 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  6347 | `	{` |
|    19467 |  6348 | `		sxi32 d = 0;` |
|   773341 |  6349 | `		while( pIn < pEnd ){` |
|   773341 |  6350 | `			sxu32 t = pIn->nType;` |
|   773341 |  6351 | `			if( t & PH7_TK_OCB ){ d++; }` |
|   742223 |  6352 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|   753879 |  6353 | `			pIn++;` |
|        5 |  6354 | `		}` |
|        - |  6355 | `	}` |
|    19467 |  6356 | `	return pIn;` |
|    17504 |  6357 | `}` |
|        - |  6358 | `/*` |
|        - |  6359 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  6360 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  6361 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  6362 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  6363 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  6364 | ` * detached-mini-program path untouched.` |
|        - |  6365 | ` */` |
|        - |  6366 | `/*` |
|        - |  6367 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  6368 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  6369 | ` * mixed, object.` |
|        - |  6370 | ` */` |
|       28 |  6371 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        3 |  6372 | `{` |
|        - |  6373 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  6374 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  6375 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  6376 | `	};` |
|        - |  6377 | `	sxu32 i;` |
|       31 |  6378 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  6379 | `		zName++;` |
|      ! 0 |  6380 | `		nName--;` |
|      ! 0 |  6381 | `	}` |
|       63 |  6382 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|       59 |  6383 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|       27 |  6384 | `			return 1;` |
|        - |  6385 | `		}` |
|       17 |  6386 | `	}` |
|        5 |  6387 | `	return 0;` |
|       17 |  6388 | `}` |
|        - |  6389 | `/*` |
|        - |  6390 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  6391 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  6392 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  6393 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  6394 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  6395 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  6396 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  6397 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  6398 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  6399 | ` * both rather than fatal on valid code (divergence recorded in PLAN.md).` |
|        - |  6400 | ` */` |
|       26 |  6401 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        4 |  6402 | `{` |
|       30 |  6403 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  6404 | ``		return 1; /* bare `object` */`` |
|        - |  6405 | `	}` |
|       30 |  6406 | `	if( nType != SXU32_HIGH ){` |
|        3 |  6407 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  6408 | `	}` |
|       27 |  6409 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|       23 |  6410 | `		return 1;` |
|        - |  6411 | `	}` |
|        - |  6412 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  6413 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  6414 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  6415 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  6416 | `	{` |
|        - |  6417 | `		SyBlob sFQN;` |
|        - |  6418 | `		int bOk;` |
|        5 |  6419 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        5 |  6420 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|        5 |  6421 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|        5 |  6422 | `		SyBlobRelease(&sFQN);` |
|        5 |  6423 | `		return bOk;` |
|        - |  6424 | `	}` |
|       17 |  6425 | `}` |
|        - |  6426 | `/*` |
|        - |  6427 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  6428 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  6429 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  6430 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  6431 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  6432 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  6433 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  6434 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  6435 | ` */` |
|      264 |  6436 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  6437 | `{` |
|      269 |  6438 | `	int bOk = 0;` |
|        - |  6439 | `	sxu32 nLine;` |
|        - |  6440 | `	sxi32 rc;` |
|      269 |  6441 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      243 |  6442 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  6443 | `	}` |
|       30 |  6444 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  6445 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  6446 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  6447 | `		sxu32 i,j;` |
|      ! 0 |  6448 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  6449 | `			int bGroupOk;` |
|      ! 0 |  6450 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  6451 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  6452 | `			}` |
|      ! 0 |  6453 | `			bGroupOk = 1;` |
|      ! 0 |  6454 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  6455 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  6456 | `					bGroupOk = 0;` |
|      ! 0 |  6457 | `					break;` |
|        - |  6458 | `				}` |
|      ! 0 |  6459 | `			}` |
|      ! 0 |  6460 | `			bOk = bGroupOk;` |
|      ! 0 |  6461 | `		}` |
|      ! 0 |  6462 | `	}else{` |
|       30 |  6463 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  6464 | `	}` |
|       30 |  6465 | `	if( bOk ){` |
|       27 |  6466 | `		return SXRET_OK;` |
|        - |  6467 | `	}` |
|        - |  6468 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  6469 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  6470 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  6471 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|        - |  6472 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  6473 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  6474 | `	{` |
|        3 |  6475 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  6476 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  6477 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  6478 | `		}` |
|        3 |  6479 | `		if( sGiven.nByte < 1 ){` |
|        - |  6480 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|        - |  6481 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|        - |  6482 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|      ! 0 |  6483 | `			const char *zScalar =` |
|      ! 0 |  6484 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|      ! 0 |  6485 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|      ! 0 |  6486 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|      ! 0 |  6487 | `		}` |
|        3 |  6488 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  6489 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  6490 | `	}` |
|        3 |  6491 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      137 |  6492 | `}` |
|  1405458 |  6493 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  6494 | `{` |
|  1405463 |  6495 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  1405463 |  6496 | `	SyToken *pEnd = pGen->pEnd;` |
|  1405463 |  6497 | `	sxi32 iDepth = 0;` |
|  1405463 |  6498 | `	int bStarted = 0;` |
| 63110097 |  6499 | `	while( pIn < pEnd ){` |
| 63110097 |  6500 | `		sxu32 t = pIn->nType;` |
| 63110097 |  6501 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 60142315 |  6502 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 57174911 |  6503 | `		if( t & PH7_TK_KEYWORD ){` |
|  4638717 |  6504 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  4638717 |  6505 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  4638453 |  6506 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  6507 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  2301725 |  6508 | `		}` |
| 57139649 |  6509 | `		pIn++;` |
|        5 |  6510 | `	}` |
|  1405199 |  6511 | `	return FALSE;` |
|   702734 |  6512 | `}` |
|        - |  6513 | `/*` |
|        - |  6514 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  6515 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  6516 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  6517 | ` */` |
|  1405458 |  6518 | `static sxi32 GenStateCompileFuncBody(` |
|        - |  6519 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  6520 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  6521 | `	)` |
|        5 |  6522 | `{` |
|        - |  6523 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  6524 | `	GenBlock *pBlock;` |
|        - |  6525 | `	sxu32 nGotoOfft;` |
|        - |  6526 | `	sxi32 rc;` |
|        - |  6527 | `	/* Attach the new function */` |
|  1405463 |  6528 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  1405463 |  6529 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6530 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  6531 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6532 | `		return SXERR_ABORT;` |
|        - |  6533 | `	}` |
|  1405463 |  6534 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  6535 | `	/* Swap bytecode containers */` |
|  1405463 |  6536 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  1405463 |  6537 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  6538 | `	/* Emit constructor property promotion prologue:` |
|        - |  6539 | `	 *   $this->NAME = $NAME;` |
|        - |  6540 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  6541 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  6542 | `	{` |
|  1405463 |  6543 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  6544 | `		sxu32 i;` |
|  2091627 |  6545 | `		for( i = 0; i < nArg; i++ ){` |
|   686169 |  6546 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  6547 | `			char *zSrc;` |
|        - |  6548 | `			sxu32 nSrc,nName;` |
|        - |  6549 | `			SySet sToken;` |
|        - |  6550 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6551 | `			sxi32 rcPromote;` |
|   686169 |  6552 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   686103 |  6553 | `				continue;` |
|        - |  6554 | `			}` |
|        - |  6555 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  6556 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  6557 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  6558 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  6559 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|       71 |  6560 | `			nName = SyStringLength(&pArg->sName);` |
|       71 |  6561 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|       71 |  6562 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|       71 |  6563 | `			if( zSrc == 0 ){` |
|      ! 0 |  6564 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6565 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6566 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  6567 | `				return SXERR_ABORT;` |
|        - |  6568 | `			}` |
|        - |  6569 | `			{` |
|       71 |  6570 | `				char *z = zSrc;` |
|       71 |  6571 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|       71 |  6572 | `				z += sizeof("$this->")-1;` |
|       71 |  6573 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       71 |  6574 | `				z += nName;` |
|       71 |  6575 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|       71 |  6576 | `				z += sizeof(" = $")-1;` |
|       71 |  6577 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       71 |  6578 | `				z += nName;` |
|       71 |  6579 | `				*z = 0;` |
|        - |  6580 | `			}` |
|       71 |  6581 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       71 |  6582 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|       71 |  6583 | `			pTmpIn = pGen->pIn;` |
|       71 |  6584 | `			pTmpEnd = pGen->pEnd;` |
|       71 |  6585 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       71 |  6586 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       71 |  6587 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|       71 |  6588 | `			pGen->pIn = pTmpIn;` |
|       71 |  6589 | `			pGen->pEnd = pTmpEnd;` |
|       71 |  6590 | `			SySetRelease(&sToken);` |
|       71 |  6591 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  6592 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6593 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6594 | `				return SXERR_ABORT;` |
|        - |  6595 | `			}` |
|        - |  6596 | `			/* Discard the assignment result — this is a statement expression. */` |
|       71 |  6597 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       38 |  6598 | `		}` |
|        - |  6599 | `	}` |
|        - |  6600 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  6601 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  6602 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  6603 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  6604 | `	{` |
|  1405463 |  6605 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  1405463 |  6606 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|        - |  6607 | `		/* Compile the body */` |
|  1405463 |  6608 | `		PH7_CompileBlock(&(*pGen),0);` |
|  1405463 |  6609 | `		pGen->bInGenerator = bSavedGen;` |
|        - |  6610 | `	}` |
|        - |  6611 | `	/* Fix exception jumps now the destination is resolved */` |
|  1405463 |  6612 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  6613 | `	/* Emit the final return if not yet done */` |
|  1405463 |  6614 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  6615 | `	/* Fix gotos jumps now the destination is resolved */` |
|  1405463 |  6616 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  6617 | `		rc = SXERR_ABORT;` |
|      ! 0 |  6618 | `	}` |
|  1405463 |  6619 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  6620 | `	/* Restore the default container */` |
|  1405463 |  6621 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  6622 | `	/* Leave function block */` |
|  1405463 |  6623 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  1405463 |  6624 | `	if( rc == SXERR_ABORT ){` |
|        - |  6625 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6626 | `		return SXERR_ABORT;` |
|        - |  6627 | `	}` |
|        - |  6628 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  6629 | `	{` |
|  1405463 |  6630 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  6631 | `		sxu32 i;` |
| 38344309 |  6632 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 36939115 |  6633 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      269 |  6634 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      269 |  6635 | `				break;` |
|        - |  6636 | `			}` |
| 18469428 |  6637 | `		}` |
|        - |  6638 | `	}` |
|  1405463 |  6639 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6640 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|      269 |  6641 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  6642 | `			return SXERR_ABORT;` |
|        - |  6643 | `		}` |
|      132 |  6644 | `	}` |
|        - |  6645 | `	/* All done, function body compiled */` |
|  1405463 |  6646 | `	return SXRET_OK;` |
|   702734 |  6647 | `}` |
|        - |  6648 | `/*` |
|        - |  6649 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  6650 | ` * According to the PHP language reference manual.` |
|        - |  6651 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  6652 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  6653 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  6654 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  6655 | ` *  Functions need not be defined before they are referenced.` |
|        - |  6656 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  6657 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  6658 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  6659 | ` *  calls with over 32-64 recursion levels.` |
|        - |  6660 | ` *` |
|        - |  6661 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  6662 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  6663 | ` * on these extension.` |
|        - |  6664 | ` */` |
|        - |  6665 | `/*` |
|        - |  6666 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  6667 | ` */` |
|      570 |  6668 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  6669 | `{` |
|        - |  6670 | `	sxu32 i;` |
|     1611 |  6671 | `	for( i = 0; i < n; i++ ){` |
|     1381 |  6672 | `		int a = zA[i], b = zB[i];` |
|     1381 |  6673 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     1381 |  6674 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     1381 |  6675 | `		if( a != b ) return a - b;` |
|      523 |  6676 | `	}` |
|      235 |  6677 | `	return 0;` |
|      290 |  6678 | `}` |
|        - |  6679 | `/*` |
|        - |  6680 | ` * Internal type-atom kinds used during union type parsing.` |
|        - |  6681 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - |  6682 | ` * (which are positive bit values stored in sxu32).` |
|        - |  6683 | ` */` |
|        - |  6684 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - |  6685 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - |  6686 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - |  6687 |  |
|        - |  6688 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - |  6689 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - |  6690 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - |  6691 |  |
|        - |  6692 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - |  6693 | `struct PhlTypeAtom {` |
|        - |  6694 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - |  6695 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - |  6696 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - |  6697 | `	sxu32 nCanon;` |
|        - |  6698 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - |  6699 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - |  6700 | `};` |
|        - |  6701 |  |
|        - |  6702 | `/*` |
|        - |  6703 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - |  6704 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - |  6705 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - |  6706 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - |  6707 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - |  6708 | ` * already be consumed by the caller.` |
|        - |  6709 | ` */` |
|    98636 |  6710 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 |  6711 | `{` |
|    98641 |  6712 | `	SyToken *pIn = pGen->pIn;` |
|    98641 |  6713 | `	SyZero(pOut, sizeof(*pOut));` |
|    98641 |  6714 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    98641 |  6715 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6716 | `		return SXERR_SYNTAX;` |
|        - |  6717 | `	}` |
|        - |  6718 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    98641 |  6719 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        8 |  6720 | `		pIn++;` |
|        8 |  6721 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6722 | `			return SXERR_SYNTAX;` |
|        - |  6723 | `		}` |
|        3 |  6724 | `	}` |
|    98641 |  6725 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  6726 | `		return SXERR_SYNTAX;` |
|        - |  6727 | `	}` |
|    98641 |  6728 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    82565 |  6729 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    82565 |  6730 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|    11693 |  6731 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    76721 |  6732 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       81 |  6733 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    70839 |  6734 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    19947 |  6735 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    60830 |  6736 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|    50777 |  6737 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|    25473 |  6738 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       41 |  6739 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|       68 |  6740 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       27 |  6741 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|       37 |  6742 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       14 |  6743 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       23 |  6744 | `			pOut->nType = SXU32_HIGH;` |
|       23 |  6745 | `			pOut->sClass = pIn->sData;` |
|       13 |  6746 | `		}else{` |
|        3 |  6747 | `			return SXERR_SYNTAX;` |
|        - |  6748 | `		}` |
|    82563 |  6749 | `		pIn++;` |
|    41284 |  6750 | `	}else{` |
|        - |  6751 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - |  6752 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    16081 |  6753 | `		SyString *pT = &pIn->sData;` |
|    16081 |  6754 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       34 |  6755 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       34 |  6756 | `			pIn++;` |
|    16066 |  6757 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      177 |  6758 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      177 |  6759 | `			pIn++;` |
|    15965 |  6760 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       26 |  6761 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       26 |  6762 | `			pIn++;` |
|       15 |  6763 | `		}else{` |
|        - |  6764 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    15857 |  6765 | `			SyToken *pFirst = pIn;` |
|    15857 |  6766 | `			SyToken *pLast = pIn;` |
|    15857 |  6767 | `			pOut->nType = SXU32_HIGH;` |
|    15857 |  6768 | `			pOut->sClass = pIn->sData;` |
|    15857 |  6769 | `			pIn++;` |
|    23781 |  6770 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    15860 |  6771 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|        3 |  6772 | `				pLast = &pIn[1];` |
|        3 |  6773 | `				pIn += 2;` |
|        1 |  6774 | `			}` |
|    15857 |  6775 | `			if( pLast != pFirst ){` |
|        3 |  6776 | `				const char *zFirst = pFirst->sData.zString;` |
|        3 |  6777 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|        3 |  6778 | `				pOut->sClass.zString = zFirst;` |
|        3 |  6779 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|        1 |  6780 | `			}` |
|        - |  6781 | `		}` |
|        - |  6782 | `	}` |
|    98639 |  6783 | `	pGen->pIn = pIn;` |
|    98639 |  6784 | `	return SXRET_OK;` |
|    49323 |  6785 | `}` |
|        - |  6786 |  |
|        - |  6787 | `/*` |
|        - |  6788 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - |  6789 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - |  6790 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|        - |  6791 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - |  6792 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - |  6793 | ` */` |
|    98458 |  6794 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 |  6795 | `{` |
|        - |  6796 | `	int i;` |
|    98463 |  6797 | `	int nNonNull = 0;` |
|    98463 |  6798 | `	int bAnyIntersection = 0;` |
|        - |  6799 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    98463 |  6800 | `	sxu32 nMaxGroup = 0;` |
|  3249119 |  6801 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   197073 |  6802 | `	for( i = 0; i < nAtoms; i++ ){` |
|    98615 |  6803 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    98585 |  6804 | `			nNonNull++;` |
|    98585 |  6805 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    98585 |  6806 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    98585 |  6807 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|    49290 |  6808 | `			}` |
|    49290 |  6809 | `		}` |
|    49310 |  6810 | `	}` |
|   197021 |  6811 | `	for( i = 0; i < nAtoms; i++ ){` |
|    98587 |  6812 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       29 |  6813 | `			bAnyIntersection = 1;` |
|       29 |  6814 | `			break;` |
|        - |  6815 | `		}` |
|    49284 |  6816 | `	}` |
|    98463 |  6817 | `	if( bAnyIntersection ){` |
|        - |  6818 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - |  6819 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - |  6820 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       29 |  6821 | `		sxu32 g, nGroups = 0;` |
|       29 |  6822 | `		int bFirstGroup = 1;` |
|       59 |  6823 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|       59 |  6824 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       35 |  6825 | `			int bFirstMember = 1;` |
|        - |  6826 | `			int bWrap;` |
|       35 |  6827 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - |  6828 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - |  6829 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - |  6830 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - |  6831 | `			 * parens, matching PHP's canonical text. */` |
|       47 |  6832 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       35 |  6833 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       35 |  6834 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      107 |  6835 | `			for( i = 0; i < nAtoms; i++ ){` |
|       77 |  6836 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|       59 |  6837 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|       59 |  6838 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       55 |  6839 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       30 |  6840 | `				}else{` |
|        6 |  6841 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6842 | `				}` |
|       59 |  6843 | `				bFirstMember = 0;` |
|       32 |  6844 | `			}` |
|       35 |  6845 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       35 |  6846 | `			bFirstGroup = 0;` |
|       20 |  6847 | `		}` |
|       29 |  6848 | `		if( bNullable ){` |
|      ! 0 |  6849 | `			SyBlobAppend(pBlob, "\|", 1);` |
|      ! 0 |  6850 | `			SyBlobAppend(pBlob, "null", 4);` |
|      ! 0 |  6851 | `		}` |
|       78 |  6852 | `		return;` |
|        - |  6853 | `	}` |
|    98439 |  6854 | `	if( nNonNull == 1 && bNullable ){` |
|        - |  6855 | `		/* Shorthand: ?T */` |
|      102 |  6856 | `		for( i = 0; i < nAtoms; i++ ){` |
|      102 |  6857 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      102 |  6858 | `			SyBlobAppend(pBlob, "?", 1);` |
|      102 |  6859 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|       24 |  6860 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       14 |  6861 | `			}else{` |
|       82 |  6862 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6863 | `			}` |
|      102 |  6864 | `			return;` |
|      ! 0 |  6865 | `		}` |
|      ! 0 |  6866 | `	}` |
|        - |  6867 | `	{` |
|    98341 |  6868 | `		int bFirst = 1;` |
|        - |  6869 | `		/* 1) Classes in declaration order */` |
|   196785 |  6870 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98449 |  6871 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    15807 |  6872 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    15807 |  6873 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    15807 |  6874 | `				bFirst = 0;` |
|     7901 |  6875 | `			}` |
|    49227 |  6876 | `		}` |
|        - |  6877 | `		/* 2) Built-ins in canonical order */` |
|        - |  6878 | `		{` |
|        - |  6879 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|        - |  6880 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|        - |  6881 | `			int k;` |
|   688357 |  6882 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  1098133 |  6883 | `				for( i = 0; i < nAtoms; i++ ){` |
|   590557 |  6884 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|    82445 |  6885 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    82445 |  6886 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|    82445 |  6887 | `						bFirst = 0;` |
|    82445 |  6888 | `						break;` |
|        - |  6889 | `					}` |
|   254061 |  6890 | `				}` |
|   295013 |  6891 | `			}` |
|        - |  6892 | `		}` |
|        - |  6893 | `		/* 3) null suffix */` |
|    98341 |  6894 | `		if( bNullable ){` |
|       19 |  6895 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       19 |  6896 | `			SyBlobAppend(pBlob, "null", 4);` |
|        8 |  6897 | `		}` |
|        - |  6898 | `	}` |
|    49234 |  6899 | `}` |
|        - |  6900 |  |
|        - |  6901 | `/*` |
|        - |  6902 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - |  6903 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - |  6904 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - |  6905 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - |  6906 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - |  6907 | ` * whether it was parenthesized.` |
|        - |  6908 | ` *` |
|        - |  6909 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - |  6910 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - |  6911 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - |  6912 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - |  6913 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - |  6914 | ` */` |
|    98610 |  6915 | `static sxi32 GenStateParsePart(` |
|        - |  6916 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - |  6917 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 |  6918 | `{` |
|        - |  6919 | `	sxi32 rc;` |
|    98615 |  6920 | `	int nMembers = 0;` |
|    98615 |  6921 | `	int bParen = 0;` |
|    98615 |  6922 | `	*pnMembers = 0;` |
|    98615 |  6923 | `	*pbParen = 0;` |
|    98615 |  6924 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        9 |  6925 | `		bParen = 1;` |
|        9 |  6926 | `		pGen->pIn++; /* skip '(' */` |
|        3 |  6927 | `	}` |
|    49305 |  6928 | `	for(;;){` |
|    98641 |  6929 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 |  6930 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6931 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 |  6932 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6933 | `		}` |
|    98641 |  6934 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    98641 |  6935 | `		if( rc != SXRET_OK ){` |
|        3 |  6936 | `			return rc;` |
|        - |  6937 | `		}` |
|    98639 |  6938 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    98639 |  6939 | `		(*pnAtoms)++;` |
|    98639 |  6940 | `		nMembers++;` |
|        - |  6941 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    98639 |  6942 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       39 |  6943 | `			SyToken *pNext = &pGen->pIn[1];` |
|       34 |  6944 | `			if( pNext < pGen->pEnd` |
|       39 |  6945 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       31 |  6946 | `				pGen->pIn++; /* skip '&' */` |
|       31 |  6947 | `				continue;` |
|        - |  6948 | `			}` |
|        4 |  6949 | `		}` |
|    98613 |  6950 | `		break;` |
|      ! 0 |  6951 | `	}` |
|    98613 |  6952 | `	if( bParen ){` |
|        9 |  6953 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  6954 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6955 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 |  6956 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6957 | `		}` |
|        9 |  6958 | `		pGen->pIn++; /* skip ')' */` |
|        9 |  6959 | `		if( nMembers < 2 ){` |
|      ! 0 |  6960 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6961 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 |  6962 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6963 | `		}` |
|        3 |  6964 | `	}` |
|    98613 |  6965 | `	*pnMembers = nMembers;` |
|    98613 |  6966 | `	*pbParen = bParen;` |
|    98613 |  6967 | `	return SXRET_OK;` |
|    49310 |  6968 | `}` |
|        - |  6969 |  |
|        - |  6970 | `/*` |
|        - |  6971 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - |  6972 | ` *` |
|        - |  6973 | ` * Outputs:` |
|        - |  6974 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - |  6975 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - |  6976 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - |  6977 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - |  6978 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - |  6979 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - |  6980 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - |  6981 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - |  6982 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - |  6983 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - |  6984 | ` *` |
|        - |  6985 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - |  6986 | ` * SXERR_ABORT on fatal compile errors.` |
|        - |  6987 | ` */` |
|    98474 |  6988 | `static sxi32 GenStateParseUnionTypeDecl(` |
|        - |  6989 | `	ph7_gen_state *pGen,` |
|        - |  6990 | `	sxu32 *pnType,` |
|        - |  6991 | `	SyString *pClass,` |
|        - |  6992 | `	SySet *pAlts,` |
|        - |  6993 | `	sxi32 *piTypeFlags,` |
|        - |  6994 | `	SyString *pTypeText,` |
|        - |  6995 | `	int iNullableFlag,` |
|        - |  6996 | `	int iUnionFlag,` |
|        - |  6997 | `	int bAllowVoid,` |
|        - |  6998 | `	sxu32 nLine` |
|        5 |  6999 | `){` |
|        - |  7000 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    98479 |  7001 | `	int nAtoms = 0;` |
|    98479 |  7002 | `	int bShortNullable = 0;` |
|    98479 |  7003 | `	int bExplicitNull = 0;` |
|        - |  7004 | `	sxi32 rc;` |
|    98479 |  7005 | `	*pnType = 0;` |
|    98479 |  7006 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    98479 |  7007 | `	*piTypeFlags = 0;` |
|    98479 |  7008 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - |  7009 |  |
|    98479 |  7010 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7011 | `		return SXRET_OK;` |
|        - |  7012 | `	}` |
|        - |  7013 | ``	/* Optional `?` shorthand prefix */`` |
|    98474 |  7014 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       91 |  7015 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       90 |  7016 | `		bShortNullable = 1;` |
|       90 |  7017 | `		pGen->pIn++;` |
|       90 |  7018 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7019 | `			return SXERR_SYNTAX;` |
|        - |  7020 | `		}` |
|       43 |  7021 | `	}` |
|        - |  7022 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - |  7023 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - |  7024 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - |  7025 | `	{` |
|        - |  7026 | `		int nMembers, bParen;` |
|    98479 |  7027 | `		sxu32 iGroup = 0;` |
|    98479 |  7028 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    98479 |  7029 | `		if( rc != SXRET_OK ){` |
|        4 |  7030 | `			return rc;` |
|        - |  7031 | `		}` |
|        - |  7032 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - |  7033 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - |  7034 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - |  7035 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - |  7036 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|   147917 |  7037 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    98686 |  7038 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      143 |  7039 | `			if( bShortNullable ){` |
|        - |  7040 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - |  7041 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - |  7042 | `				 * already reported" so callers skip their own error emission. */` |
|        3 |  7043 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7044 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 |  7045 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - |  7046 | `			}` |
|      141 |  7047 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 |  7048 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - |  7049 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7050 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7051 | `			}` |
|      141 |  7052 | ``			pGen->pIn++; /* skip `\|` */`` |
|      141 |  7053 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|      141 |  7054 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7055 | `				return rc;` |
|        - |  7056 | `			}` |
|        5 |  7057 | `		}` |
|    98475 |  7058 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 |  7059 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7060 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7061 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7062 | `		}` |
|        - |  7063 | `	}` |
|        - |  7064 | `	/* Validation pass.` |
|        - |  7065 | `	 *` |
|        - |  7066 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - |  7067 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - |  7068 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - |  7069 | `	 */` |
|        - |  7070 | `	{` |
|        - |  7071 | `		int i, j;` |
|    98475 |  7072 | `		int bHasNonNull = 0;` |
|    98475 |  7073 | `		int bAnyIntersection = 0;` |
|        - |  7074 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - |  7075 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - |  7076 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|  3249515 |  7077 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   197107 |  7078 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98637 |  7079 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|    49321 |  7080 | `		}` |
|   197051 |  7081 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98607 |  7082 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|    49293 |  7083 | `		}` |
|        - |  7084 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - |  7085 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    98475 |  7086 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 |  7087 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7088 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 |  7089 | `			return SXERR_SYNTAX;` |
|        - |  7090 | `		}` |
|   197093 |  7091 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - |  7092 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - |  7093 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - |  7094 | ``			 * `true`/`false` in an intersection). */`` |
|    98635 |  7095 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       55 |  7096 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       55 |  7097 | `				if( bClassLike ){` |
|       53 |  7098 | `					SyString *pC = &aAtoms[i].sClass;` |
|       48 |  7099 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       48 |  7100 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       48 |  7101 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       53 |  7102 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 |  7103 | `						bClassLike = 0;` |
|      ! 0 |  7104 | `					}` |
|       24 |  7105 | `				}` |
|       55 |  7106 | `				if( !bClassLike ){` |
|        - |  7107 | `					const char *zName; sxu32 nName;` |
|        3 |  7108 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7109 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7110 | `					}else{` |
|        3 |  7111 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - |  7112 | `					}` |
|        4 |  7113 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7114 | `						"Type %.*s cannot be part of an intersection type",` |
|        1 |  7115 | `						(int)nName, zName);` |
|        3 |  7116 | `					return SXERR_SYNTAX;` |
|        - |  7117 | `				}` |
|       24 |  7118 | `			}` |
|    98633 |  7119 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      177 |  7120 | `				if( nAtoms > 1 ){` |
|        3 |  7121 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7122 | `						"Void can only be used as a standalone type");` |
|        3 |  7123 | `					return SXERR_SYNTAX;` |
|        - |  7124 | `				}` |
|      175 |  7125 | `				if( !bAllowVoid ){` |
|      ! 0 |  7126 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7127 | `						"void cannot be used here");` |
|      ! 0 |  7128 | `					return SXERR_SYNTAX;` |
|        - |  7129 | `				}` |
|      175 |  7130 | `				if( bShortNullable ){` |
|      ! 0 |  7131 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7132 | `						"Void type cannot be nullable");` |
|      ! 0 |  7133 | `					return SXERR_SYNTAX;` |
|        - |  7134 | `				}` |
|       85 |  7135 | `			}` |
|    98631 |  7136 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - |  7137 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - |  7138 | `				 * type (never = the function does not return). Mirrors the void` |
|        - |  7139 | `				 * validation above; accepted here and enforced at compile time` |
|        - |  7140 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       26 |  7141 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - |  7142 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - |  7143 | `					 * same as any other non-standalone use. */` |
|        5 |  7144 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7145 | `						"never can only be used as a standalone type");` |
|        5 |  7146 | `					return SXERR_SYNTAX;` |
|        - |  7147 | `				}` |
|       21 |  7148 | `				if( !bAllowVoid ){` |
|        - |  7149 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 |  7150 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7151 | `						"never cannot be used as a parameter type");` |
|        3 |  7152 | `					return SXERR_SYNTAX;` |
|        - |  7153 | `				}` |
|        8 |  7154 | `			}` |
|    98625 |  7155 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       34 |  7156 | `				bExplicitNull = 1;` |
|       19 |  7157 | `			}else{` |
|    98595 |  7158 | `				bHasNonNull = 1;` |
|        - |  7159 | `			}` |
|        - |  7160 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - |  7161 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - |  7162 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - |  7163 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - |  7164 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    98825 |  7165 | `			for( j = 0; j < i; j++ ){` |
|      207 |  7166 | `				int bDup = 0;` |
|      207 |  7167 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|      395 |  7168 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|      202 |  7169 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|      207 |  7170 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|      195 |  7171 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|       51 |  7172 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       44 |  7173 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       44 |  7174 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       17 |  7175 | `								aAtoms[j].sClass.zString,` |
|       34 |  7176 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 |  7177 | `							bDup = 1;` |
|      ! 0 |  7178 | `						}` |
|       27 |  7179 | `					}else{` |
|        3 |  7180 | `						bDup = 1;` |
|        - |  7181 | `					}` |
|       23 |  7182 | `				}` |
|      195 |  7183 | `				if( bDup ){` |
|        - |  7184 | `					const char *zName;` |
|        - |  7185 | `					sxu32 nName;` |
|        3 |  7186 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7187 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 |  7188 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7189 | `					}else{` |
|        3 |  7190 | `						zName = aAtoms[i].zCanon;` |
|        3 |  7191 | `						nName = aAtoms[i].nCanon;` |
|        - |  7192 | `					}` |
|        4 |  7193 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 |  7194 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 |  7195 | `					return SXERR_SYNTAX;` |
|        - |  7196 | `				}` |
|       99 |  7197 | `			}` |
|    49314 |  7198 | `		}` |
|    98463 |  7199 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 |  7200 | `			if( bShortNullable ){` |
|        - |  7201 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 |  7202 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7203 | `					"Null can not be used as a standalone type");` |
|      ! 0 |  7204 | `				return SXERR_SYNTAX;` |
|        - |  7205 | `			}` |
|        - |  7206 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - |  7207 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - |  7208 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - |  7209 | `			 * atom, so set it here. */` |
|        7 |  7210 | `			*pnType = MEMOBJ_NULL;` |
|        3 |  7211 | `		}` |
|        - |  7212 | `	}` |
|        - |  7213 | `	/* Compute nullability flag */` |
|    98463 |  7214 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      118 |  7215 | `		*piTypeFlags \|= iNullableFlag;` |
|       57 |  7216 | `	}` |
|        - |  7217 | `	/* Build canonical type text */` |
|    98463 |  7218 | `	if( pTypeText ){` |
|        - |  7219 | `		SyBlob sBlob;` |
|    98463 |  7220 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   147650 |  7221 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|    49229 |  7222 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    98463 |  7223 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   147413 |  7224 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    98272 |  7225 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    98277 |  7226 | `			if( zDup ){` |
|    98277 |  7227 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|    49136 |  7228 | `			}` |
|    49136 |  7229 | `		}` |
|    98463 |  7230 | `		SyBlobRelease(&sBlob);` |
|    49229 |  7231 | `	}` |
|        - |  7232 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - |  7233 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - |  7234 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - |  7235 | `	{` |
|    98463 |  7236 | `		int nNonNull = 0;` |
|    98463 |  7237 | `		int iNonNullIdx = -1;` |
|        - |  7238 | `		int i;` |
|   197073 |  7239 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98615 |  7240 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    98585 |  7241 | `				nNonNull++;` |
|    98585 |  7242 | `				iNonNullIdx = i;` |
|    49290 |  7243 | `			}` |
|    49310 |  7244 | `		}` |
|    98463 |  7245 | `		if( nNonNull <= 1 ){` |
|        - |  7246 | `			/* Fast path: store as single type. */` |
|    98357 |  7247 | `			if( iNonNullIdx >= 0 ){` |
|    98351 |  7248 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    98351 |  7249 | `				if( pA->nType == SXU32_HIGH ){` |
|    23672 |  7250 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     7889 |  7251 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    15783 |  7252 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    15783 |  7253 | `					*pnType = SXU32_HIGH;` |
|    15783 |  7254 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    90462 |  7255 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      175 |  7256 | `					*pnType = MEMOBJ_VOID;` |
|    82488 |  7257 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       18 |  7258 | `					*pnType = MEMOBJ_NEVER;` |
|       10 |  7259 | `				}else{` |
|    82387 |  7260 | `					*pnType = pA->nType;` |
|        - |  7261 | `				}` |
|    49173 |  7262 | `			}` |
|    49181 |  7263 | `		}else{` |
|        - |  7264 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      111 |  7265 | `			*piTypeFlags \|= iUnionFlag;` |
|      355 |  7266 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - |  7267 | `				ph7_type_alt sAlt;` |
|      249 |  7268 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      239 |  7269 | `				SyZero(&sAlt, sizeof(sAlt));` |
|      239 |  7270 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|      239 |  7271 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      146 |  7272 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       47 |  7273 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       99 |  7274 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|       99 |  7275 | `					sAlt.nType = SXU32_HIGH;` |
|       99 |  7276 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|       52 |  7277 | `				}else{` |
|      145 |  7278 | `					sAlt.nType = aAtoms[i].nType;` |
|      145 |  7279 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - |  7280 | `				}` |
|      239 |  7281 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      122 |  7282 | `			}` |
|        - |  7283 | `		}` |
|        - |  7284 | `	}` |
|    98463 |  7285 | `	return SXRET_OK;` |
|    49242 |  7286 | `}` |
|        - |  7287 |  |
|        - |  7288 | `/*` |
|        - |  7289 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - |  7290 | `` * pGen->pIn should point to the token after `)`.`` |
|        - |  7291 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - |  7292 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - |  7293 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - |  7294 | `` *          and union types `: T\|U`.`` |
|        - |  7295 | ` */` |
|  1506798 |  7296 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 |  7297 | `{` |
|  1506803 |  7298 | `	sxi32 iFlags = 0;` |
|        - |  7299 | `	sxi32 rc;` |
|        - |  7300 | `	sxu32 nLine;` |
|  1506803 |  7301 | `	pFunc->nReturnType = 0;` |
|  1506803 |  7302 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  1506803 |  7303 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - |  7304 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - |  7305 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - |  7306 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - |  7307 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - |  7308 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|  1506803 |  7309 | `	SySetReset(&pFunc->aReturnUnion);` |
|  1506803 |  7310 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|  1506803 |  7311 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  1506151 |  7312 | `		return SXRET_OK;` |
|        - |  7313 | `	}` |
|      657 |  7314 | `	pGen->pIn++; /* Skip ':' */` |
|      657 |  7315 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7316 | `		return SXRET_OK;` |
|        - |  7317 | `	}` |
|      657 |  7318 | `	nLine = pGen->pIn->nLine;` |
|      657 |  7319 | `	rc = GenStateParseUnionTypeDecl(` |
|      326 |  7320 | `		pGen,` |
|      326 |  7321 | `		&pFunc->nReturnType,` |
|      326 |  7322 | `		&pFunc->sReturnClass,` |
|      326 |  7323 | `		&pFunc->aReturnUnion,` |
|        - |  7324 | `		&iFlags,` |
|      326 |  7325 | `		&pFunc->sReturnTypeName,` |
|        - |  7326 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - |  7327 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - |  7328 | `		/* iUnionFlag */ 0,` |
|        - |  7329 | `		/* bAllowVoid */ 1,` |
|      326 |  7330 | `		nLine);` |
|      657 |  7331 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7332 | `		return SXERR_ABORT;` |
|        - |  7333 | `	}` |
|      657 |  7334 | `	if( rc == SXERR_CORRUPT ){` |
|        - |  7335 | `		/* Error already reported */` |
|      ! 0 |  7336 | `		return SXERR_SYNTAX;` |
|        - |  7337 | `	}` |
|      657 |  7338 | `	if( rc == SXERR_SYNTAX ){` |
|        8 |  7339 | `		if( pGen->pIn < pGen->pEnd ){` |
|       11 |  7340 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7341 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|        6 |  7342 | `				&pGen->pIn->sData);` |
|        5 |  7343 | `		}else{` |
|      ! 0 |  7344 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - |  7345 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - |  7346 | `		}` |
|        8 |  7347 | `		return SXERR_SYNTAX;` |
|        - |  7348 | `	}` |
|      651 |  7349 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|      651 |  7350 | `	return SXRET_OK;` |
|   753404 |  7351 | `}` |
|        - |  7352 |  |
|   118430 |  7353 | `static sxi32 GenStateCompileFunc(` |
|        - |  7354 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  7355 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - |  7356 | `	sxi32 iFlags,        /* Control flags */` |
|        - |  7357 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - |  7358 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - |  7359 | `	)` |
|        5 |  7360 | `{` |
|        - |  7361 | `	ph7_vm_func *pFunc;` |
|        - |  7362 | `	SyToken *pEnd;` |
|        - |  7363 | `	sxu32 nLine;` |
|        - |  7364 | `	char *zName;` |
|        - |  7365 | `	sxi32 rc;` |
|        - |  7366 | `	/* Extract line number */` |
|   118435 |  7367 | `	nLine = pGen->pIn->nLine;` |
|        - |  7368 | `	/* Jump the left parenthesis '(' */` |
|   118435 |  7369 | `	pGen->pIn++;` |
|        - |  7370 | `	/* Delimit the function signature */` |
|   118435 |  7371 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   118435 |  7372 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  7373 | `		/* Syntax error */` |
|        8 |  7374 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|        8 |  7375 | `		if( rc == SXERR_ABORT ){` |
|        - |  7376 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7377 | `			return SXERR_ABORT;` |
|        - |  7378 | `		}` |
|        8 |  7379 | `		pGen->pIn = pGen->pEnd;` |
|        8 |  7380 | `		return SXRET_OK;` |
|        - |  7381 | `	}` |
|        - |  7382 | `	/* Create the function state */` |
|   118429 |  7383 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   118429 |  7384 | `	if( pFunc == 0 ){` |
|      ! 0 |  7385 | `		goto OutOfMem;` |
|        - |  7386 | `	}` |
|        - |  7387 | `	/* Build the function name, prepending namespace if active */` |
|   118436 |  7388 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - |  7389 | `		SyBlob sFQN;` |
|        - |  7390 | `		sxu32 nLen;` |
|       16 |  7391 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       16 |  7392 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       16 |  7393 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       16 |  7394 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       16 |  7395 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       16 |  7396 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       16 |  7397 | `		SyBlobRelease(&sFQN);` |
|       16 |  7398 | `		if( zName == 0 ){` |
|      ! 0 |  7399 | `			goto OutOfMem;` |
|        - |  7400 | `		}` |
|       16 |  7401 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        9 |  7402 | `	}else{` |
|   118415 |  7403 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   118415 |  7404 | `		if( zName == 0 ){` |
|      ! 0 |  7405 | `			goto OutOfMem;` |
|        - |  7406 | `		}` |
|   118415 |  7407 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - |  7408 | `	}` |
|        - |  7409 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - |  7410 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   118429 |  7411 | `	pFunc->nLine = nLine;` |
|   118429 |  7412 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   118429 |  7413 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  7414 | `		return SXERR_ABORT;` |
|        - |  7415 | `	}` |
|   118429 |  7416 | `	if( pGen->pIn < pEnd ){` |
|        - |  7417 | `		/* Collect function arguments */` |
|   102073 |  7418 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   102073 |  7419 | `		if( rc == SXERR_ABORT ){` |
|        - |  7420 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  7421 | `			return SXERR_ABORT;` |
|        - |  7422 | `		}` |
|    51034 |  7423 | `	}` |
|        - |  7424 | `	/* Point past ')' and parse optional return type ': type' */` |
|   118429 |  7425 | `	pGen->pIn = &pEnd[1];` |
|        - |  7426 | `	{` |
|   118429 |  7427 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   118429 |  7428 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  7429 | `			return SXERR_ABORT;` |
|   118429 |  7430 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|        8 |  7431 | `			return SXERR_SYNTAX;` |
|        - |  7432 | `		}` |
|        - |  7433 | `	}` |
|   118423 |  7434 | `	if( bHandleClosure ){` |
|        - |  7435 | `		ph7_vm_func_closure_env sEnv;` |
|      447 |  7436 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|      442 |  7437 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      267 |  7438 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|       87 |  7439 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  7440 | `				/* Closure,record environment variable */` |
|       87 |  7441 | `				pGen->pIn++;` |
|       87 |  7442 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  7443 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 |  7444 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  7445 | `						return SXERR_ABORT;` |
|        - |  7446 | `					}` |
|      ! 0 |  7447 | `				}` |
|       87 |  7448 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - |  7449 | `				/* Compile until we hit the first closing parenthesis */` |
|      179 |  7450 | `				while( pGen->pIn < pGen->pEnd ){` |
|      179 |  7451 | `					int iFlagsLocal = 0;` |
|      179 |  7452 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|       87 |  7453 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|       87 |  7454 | `						break;` |
|        - |  7455 | `					}` |
|       97 |  7456 | `					nLineLocal = pGen->pIn->nLine;` |
|       97 |  7457 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - |  7458 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|        - |  7459 | `						 * to the variable's memory slot instead of copying its value. */` |
|       53 |  7460 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|       53 |  7461 | `						pGen->pIn++;` |
|       26 |  7462 | `					}` |
|       92 |  7463 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       97 |  7464 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  7465 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  7466 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 |  7467 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  7468 | `								return SXERR_ABORT;` |
|        - |  7469 | `							}` |
|        - |  7470 | `							/* Find the closing parenthesis */` |
|      ! 0 |  7471 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  7472 | `								pGen->pIn++;` |
|      ! 0 |  7473 | `							}` |
|      ! 0 |  7474 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 |  7475 | `								pGen->pIn++;` |
|      ! 0 |  7476 | `							}` |
|      ! 0 |  7477 | `							break;` |
|        - |  7478 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 |  7479 | `					}else{` |
|        - |  7480 | `						SyString *pNameLocal;` |
|        - |  7481 | `						char *zDup;` |
|        - |  7482 | `						/* Duplicate variable name */` |
|       97 |  7483 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       97 |  7484 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       97 |  7485 | `						if( zDup ){` |
|        - |  7486 | `							/* Zero the structure */` |
|       97 |  7487 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       97 |  7488 | `							sEnv.iFlags = iFlagsLocal;` |
|       97 |  7489 | `							sEnv.nIdx = SXU32_HIGH;` |
|       97 |  7490 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       97 |  7491 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      112 |  7492 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|       30 |  7493 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 |  7494 | `									got_this = 1;` |
|      ! 0 |  7495 | `							}` |
|        - |  7496 | `							/* Save imported variable */` |
|       97 |  7497 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       51 |  7498 | `						}else{` |
|      ! 0 |  7499 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  7500 | `							 return SXERR_ABORT;` |
|        - |  7501 | `						}` |
|        - |  7502 | `					}` |
|       97 |  7503 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      109 |  7504 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  7505 | `						/* Ignore trailing commas */` |
|       13 |  7506 | `						pGen->pIn++;` |
|        1 |  7507 | `					}` |
|        5 |  7508 | `				}` |
|        - |  7509 | `				/* php 7.1+: the return type follows the use clause —` |
|        - |  7510 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - |  7511 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - |  7512 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - |  7513 | `				 * legacy pre-use position. */` |
|       87 |  7514 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|        7 |  7515 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|        7 |  7516 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 |  7517 | `						return SXERR_ABORT;` |
|        7 |  7518 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 |  7519 | `						return SXERR_SYNTAX;` |
|        - |  7520 | `					}` |
|        3 |  7521 | `				}` |
|       41 |  7522 | `		}` |
|      447 |  7523 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|        - |  7524 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|        - |  7525 | `			 * available to the closure environment — for EVERY non-static` |
|        - |  7526 | `			 * anonymous function, use list or not (php binds $this to any` |
|        - |  7527 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|        - |  7528 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|        - |  7529 | `			 * a global-scope closure is silently dropped at install. A static` |
|        - |  7530 | `			 * closure never binds $this (php). */` |
|      439 |  7531 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      439 |  7532 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      439 |  7533 | `			sEnv.nIdx = SXU32_HIGH;` |
|      439 |  7534 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      439 |  7535 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      439 |  7536 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      217 |  7537 | `		}` |
|      447 |  7538 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - |  7539 | `			/* Mark as closure */` |
|      441 |  7540 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|      218 |  7541 | `		}` |
|      221 |  7542 | `	}` |
|        - |  7543 | `	/* Compile the body */` |
|   118423 |  7544 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   118423 |  7545 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7546 | `		return SXERR_ABORT;` |
|        - |  7547 | `	}` |
|        - |  7548 | `	/* The cursor sits just past the body's closing brace */` |
|   118423 |  7549 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   118423 |  7550 | `	if( ppFunc ){` |
|   118423 |  7551 | `		*ppFunc = pFunc;` |
|    59209 |  7552 | `	}` |
|   118423 |  7553 | `	rc = SXRET_OK;` |
|   118423 |  7554 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|        - |  7555 | `		/* Finally register the function */` |
|   117987 |  7556 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    58991 |  7557 | `	}` |
|   118423 |  7558 | `	if( rc == SXRET_OK ){` |
|   118423 |  7559 | `		return SXRET_OK;` |
|        - |  7560 | `	}` |
|        - |  7561 | `	/* Fall through if something goes wrong */` |
|      ! 0 |  7562 | `OutOfMem:` |
|        - |  7563 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  7564 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  7565 | `	 */` |
|      ! 0 |  7566 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  7567 | `	return SXERR_ABORT;` |
|    59220 |  7568 | `}` |
|        - |  7569 | `/*` |
|        - |  7570 | ` * Compile a standard PHP function.` |
|        - |  7571 | ` *  Refer to the block-comment above for more information.` |
|        - |  7572 | ` */` |
|   117996 |  7573 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 |  7574 | `{` |
|        - |  7575 | `	SyString *pName;` |
|        - |  7576 | `	sxi32 iFlags;` |
|        - |  7577 | `	sxu32 nKwLine;` |
|        - |  7578 | `	sxu32 nLine;` |
|        - |  7579 | `	sxi32 rc;` |
|        - |  7580 |  |
|   118001 |  7581 | `	nLine = pGen->pIn->nLine;` |
|   118001 |  7582 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   118001 |  7583 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   118001 |  7584 | `	iFlags = 0;` |
|   118001 |  7585 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  7586 | `		/* Return by reference,remember that */` |
|       12 |  7587 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  7588 | `		/* Jump the '&' token */` |
|       12 |  7589 | `		pGen->pIn++;` |
|        5 |  7590 | `	}` |
|   118001 |  7591 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  7592 | `		/* Invalid function name */` |
|        8 |  7593 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|        8 |  7594 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7595 | `			return SXERR_ABORT;` |
|        - |  7596 | `		}` |
|        - |  7597 | `		/* Sychronize with the next semi-colon or braces*/` |
|       22 |  7598 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       16 |  7599 | `			pGen->pIn++;` |
|        2 |  7600 | `		}` |
|        8 |  7601 | `		return SXRET_OK;` |
|        - |  7602 | `	}` |
|   117995 |  7603 | `	pName = &pGen->pIn->sData;` |
|   117995 |  7604 | `	nLine = pGen->pIn->nLine;` |
|        - |  7605 | `	/* Jump the function name */` |
|   117995 |  7606 | `	pGen->pIn++;` |
|   117995 |  7607 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  7608 | `		/* Syntax error */` |
|        3 |  7609 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|        3 |  7610 | `		if( rc == SXERR_ABORT ){` |
|        - |  7611 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7612 | `			return SXERR_ABORT;` |
|        - |  7613 | `		}` |
|        - |  7614 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 |  7615 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  7616 | `			pGen->pIn++;` |
|      ! 0 |  7617 | `		}` |
|        3 |  7618 | `		return SXRET_OK;` |
|        - |  7619 | `	}` |
|        - |  7620 | `	/* Compile function body */` |
|        - |  7621 | `	{` |
|   117993 |  7622 | `		ph7_vm_func *pFuncState = 0;` |
|   117993 |  7623 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|   117993 |  7624 | `		if( pFuncState ){` |
|        - |  7625 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   117981 |  7626 | `			pFuncState->nLine = nKwLine;` |
|    58988 |  7627 | `		}` |
|        - |  7628 | `	}` |
|   117993 |  7629 | `	return rc;` |
|    59003 |  7630 | `}` |
|        - |  7631 | `/*` |
|        - |  7632 | ` * Extract the visibility level associated with a given keyword.` |
|        - |  7633 | ` * According to the PHP language reference manual` |
|        - |  7634 | ` *  Visibility:` |
|        - |  7635 | ` *  The visibility of a property or method can be defined by prefixing` |
|        - |  7636 | ` *  the declaration with the keywords public, protected or private.` |
|        - |  7637 | ` *  Class members declared public can be accessed everywhere.` |
|        - |  7638 | ` *  Members declared protected can be accessed only within the class` |
|        - |  7639 | ` *  itself and by inherited and parent classes. Members declared as private` |
|        - |  7640 | ` *  may only be accessed by the class that defines the member.` |
|        - |  7641 | ` */` |
|  1742544 |  7642 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |  7643 | `{` |
|  1742549 |  7644 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    23467 |  7645 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  1719087 |  7646 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   182629 |  7647 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  7648 | `	}` |
|        - |  7649 | `	/* Assume public by default */` |
|  1536463 |  7650 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   871277 |  7651 | `}` |
|        - |  7652 | `/*` |
|        - |  7653 | ` * Compile a class constant.` |
|        - |  7654 | ` * According to the PHP language reference manual` |
|        - |  7655 | ` *  Class Constants` |
|        - |  7656 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - |  7657 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - |  7658 | ` *   you don't use the $ symbol to declare or use them.` |
|        - |  7659 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - |  7660 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - |  7661 | ` *   It's also possible for interfaces to have constants.` |
|        - |  7662 | ` * Symisc eXtension.` |
|        - |  7663 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - |  7664 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  7665 | ` *  Example:` |
|        - |  7666 | ` *   class Test{` |
|        - |  7667 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  7668 | ` *   };` |
|        - |  7669 | ` *   var_dump(TEST::MyConst);` |
|        - |  7670 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  7671 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  7672 | ` */` |
|        - |  7673 | `/*` |
|        - |  7674 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |  7675 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |  7676 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |  7677 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |  7678 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |  7679 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |  7680 | ` */` |
|   143882 |  7681 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |  7682 | `{` |
|        - |  7683 | `	SyToken *p0, *p1;` |
|   143887 |  7684 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7685 | `		return 0;` |
|        - |  7686 | `	}` |
|   143887 |  7687 | `	p0 = pGen->pIn;` |
|        - |  7688 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|   143887 |  7689 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|      ! 0 |  7690 | `		return 1;` |
|        - |  7691 | `	}` |
|   143887 |  7692 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  7693 | `		return 1;` |
|        - |  7694 | `	}` |
|        - |  7695 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  7696 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|        - |  7697 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|   143883 |  7698 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   143883 |  7699 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|   143883 |  7700 | `		if( p1 ){` |
|   143883 |  7701 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|       34 |  7702 | `				return 1;` |
|        - |  7703 | `			}` |
|   143853 |  7704 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|        5 |  7705 | `				return 1;` |
|        - |  7706 | `			}` |
|    71922 |  7707 | `		}` |
|    71922 |  7708 | `	}` |
|   143849 |  7709 | `	return 0;` |
|    71946 |  7710 | `}` |
|        - |  7711 | `/*` |
|        - |  7712 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|        - |  7713 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|        - |  7714 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|        - |  7715 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|        - |  7716 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|        - |  7717 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|        - |  7718 | ` * Peek only; never consumes tokens.` |
|        - |  7719 | ` */` |
|       24 |  7720 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|        4 |  7721 | `{` |
|       28 |  7722 | `	SyToken *p = pGen->pIn;` |
|       39 |  7723 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       20 |  7724 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|        3 |  7725 | `		p++; /* skip leading unary sign(s) */` |
|        1 |  7726 | `	}` |
|       28 |  7727 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|       23 |  7728 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|        - |  7729 | `	}` |
|        6 |  7730 | `	p++;` |
|        - |  7731 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|        6 |  7732 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|       16 |  7733 | `}` |
|        - |  7734 | `/*` |
|        - |  7735 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|        - |  7736 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|        - |  7737 | `` * `$o->new`), not a `new` expression.`` |
|        - |  7738 | ` */` |
|        6 |  7739 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|        3 |  7740 | `{` |
|        - |  7741 | `	sxi32 iOp;` |
|        9 |  7742 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|      ! 0 |  7743 | `		return 0;` |
|        - |  7744 | `	}` |
|        9 |  7745 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|        9 |  7746 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        6 |  7747 | `}` |
|        - |  7748 | `/*` |
|        - |  7749 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|        - |  7750 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|        - |  7751 | ` * interface-constant and (instance/static) property-default initializers` |
|        - |  7752 | ` * ("New expressions are not supported in this context") while still allowing it` |
|        - |  7753 | ` * in global constants, parameter defaults and static-local initializers (which` |
|        - |  7754 | ` * are compiled by different functions and left untouched). The scan is` |
|        - |  7755 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|        - |  7756 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|        - |  7757 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|        - |  7758 | ` *` |
|        - |  7759 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|        - |  7760 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|        - |  7761 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|        - |  7762 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|        - |  7763 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|        - |  7764 | ` */` |
|   229916 |  7765 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|        5 |  7766 | `{` |
|   229921 |  7767 | `	SyToken *p = pGen->pIn;` |
|   229921 |  7768 | `	int iDepth = 0;` |
|   561805 |  7769 | `	while( p < pGen->pEnd ){` |
|   561805 |  7770 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   229913 |  7771 | `			break; /* end of this initializer */` |
|        - |  7772 | `		}` |
|   331892 |  7773 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   169851 |  7774 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     7800 |  7775 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  7776 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|        - |  7777 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|        - |  7778 | `			 * expression. */` |
|        3 |  7779 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        3 |  7780 | `			p++;` |
|        3 |  7781 | `			if( bArrow ){` |
|        - |  7782 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|        - |  7783 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|        3 |  7784 | `				int iBase = iDepth;` |
|       17 |  7785 | `				while( p < pGen->pEnd ){` |
|       17 |  7786 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        5 |  7787 | `						iDepth++;` |
|       15 |  7788 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        5 |  7789 | `						if( iDepth <= iBase ){` |
|      ! 0 |  7790 | `							break; /* closes an enclosing group, not the fn's own */` |
|        - |  7791 | `						}` |
|        5 |  7792 | `						iDepth--;` |
|       11 |  7793 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        3 |  7794 | `						break;` |
|        - |  7795 | `					}` |
|       15 |  7796 | `					p++;` |
|        1 |  7797 | `				}` |
|        2 |  7798 | `			}else{` |
|        - |  7799 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|        - |  7800 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|        - |  7801 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|        - |  7802 | `				 * then skip the balanced brace block. */` |
|      ! 0 |  7803 | `				int iLocal = 0;` |
|      ! 0 |  7804 | `				while( p < pGen->pEnd ){` |
|      ! 0 |  7805 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      ! 0 |  7806 | `						break; /* body brace */` |
|        - |  7807 | `					}` |
|      ! 0 |  7808 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  7809 | `						iLocal++;` |
|      ! 0 |  7810 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  7811 | `						if( iLocal > 0 ){` |
|      ! 0 |  7812 | `							iLocal--;` |
|      ! 0 |  7813 | `						}` |
|      ! 0 |  7814 | `					}` |
|      ! 0 |  7815 | `					p++;` |
|      ! 0 |  7816 | `				}` |
|      ! 0 |  7817 | `				if( p < pGen->pEnd ){` |
|      ! 0 |  7818 | `					int iBrace = 0; /* p is on the body '{' */` |
|      ! 0 |  7819 | `					while( p < pGen->pEnd ){` |
|      ! 0 |  7820 | `						if( p->nType & PH7_TK_OCB ){` |
|      ! 0 |  7821 | `							iBrace++;` |
|      ! 0 |  7822 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      ! 0 |  7823 | `							iBrace--;` |
|      ! 0 |  7824 | `							if( iBrace == 0 ){` |
|      ! 0 |  7825 | `								p++;` |
|      ! 0 |  7826 | `								break;` |
|        - |  7827 | `							}` |
|      ! 0 |  7828 | `						}` |
|      ! 0 |  7829 | `						p++;` |
|      ! 0 |  7830 | `					}` |
|      ! 0 |  7831 | `				}` |
|        - |  7832 | `			}` |
|        3 |  7833 | `			continue;` |
|        - |  7834 | `		}` |
|   331895 |  7835 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     7845 |  7836 | `			iDepth++;` |
|   327975 |  7837 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     7843 |  7838 | `			if( iDepth > 0 ){` |
|     7843 |  7839 | `				iDepth--;` |
|     3919 |  7840 | `			}` |
|   320136 |  7841 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    86141 |  7842 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  7843 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|        - |  7844 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|        - |  7845 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|       11 |  7846 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  7847 | `				return 1;` |
|        - |  7848 | `			}` |
|      ! 0 |  7849 | `		}` |
|   331887 |  7850 | `		p++;` |
|        5 |  7851 | `	}` |
|   229913 |  7852 | `	return 0;` |
|   114963 |  7853 | `}` |
|        - |  7854 | `/*` |
|        - |  7855 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  7856 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  7857 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  7858 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  7859 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  7860 | ` * share the same backing.` |
|        - |  7861 | ` */` |
|      226 |  7862 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  7863 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  7864 | `{` |
|      231 |  7865 | `	pAttr->nType = nType;` |
|      231 |  7866 | `	pAttr->sClass = *pClass;` |
|      231 |  7867 | `	pAttr->sTypeName = *pTypeName;` |
|      231 |  7868 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  7869 | `		sxu32 i;` |
|       73 |  7870 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       51 |  7871 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       51 |  7872 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       28 |  7873 | `		}` |
|       11 |  7874 | `	}` |
|      231 |  7875 | `}` |
|   143882 |  7876 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  7877 | `{` |
|   143887 |  7878 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  7879 | `	SySet *pInstrContainer;` |
|        - |  7880 | `	ph7_class_attr *pCons;` |
|        - |  7881 | `	SyString *pName;` |
|        - |  7882 | `	sxi32 rc;` |
|   143887 |  7883 | `	sxu32 nType = 0;` |
|        - |  7884 | `	SyString sTypeClass;` |
|        - |  7885 | `	SyString sTypeText;` |
|        - |  7886 | `	SySet aUnionAlts;` |
|   143887 |  7887 | `	sxi32 iTypeFlags = 0;` |
|   143887 |  7888 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   143887 |  7889 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   143887 |  7890 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  7891 | `	/* Extract visibility level */` |
|   143887 |  7892 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  7893 | `	/* Mark as constant */` |
|   143887 |  7894 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   143887 |  7895 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  7896 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  7897 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   143906 |  7898 | `	if( GenStateClassConstHasType(pGen) ){` |
|       61 |  7899 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|       38 |  7900 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|        - |  7901 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  7902 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  7903 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  7904 | `		 * and success paths release. */` |
|       42 |  7905 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  7906 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  7907 | `			goto Synchronize;` |
|       42 |  7908 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  7909 | `			return SXERR_ABORT;` |
|       42 |  7910 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  7911 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  7912 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|      ! 0 |  7913 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  7914 | `				return SXERR_ABORT;` |
|        - |  7915 | `			}` |
|      ! 0 |  7916 | `			goto Synchronize;` |
|        - |  7917 | `		}` |
|       42 |  7918 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       19 |  7919 | `	}` |
|    71941 |  7920 | `loop:` |
|   143889 |  7921 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  7922 | `		/* Invalid constant name */` |
|      ! 0 |  7923 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  7924 | `		if( rc == SXERR_ABORT ){` |
|        - |  7925 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7926 | `			return SXERR_ABORT;` |
|        - |  7927 | `		}` |
|      ! 0 |  7928 | `		goto Synchronize;` |
|        - |  7929 | `	}` |
|        - |  7930 | `	/* Peek constant name */` |
|   143889 |  7931 | `	pName = &pGen->pIn->sData;` |
|        - |  7932 | `	/* Make sure the constant name isn't reserved */` |
|   143889 |  7933 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  7934 | `		/* Reserved constant name */` |
|      ! 0 |  7935 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  7936 | `		if( rc == SXERR_ABORT ){` |
|        - |  7937 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7938 | `			return SXERR_ABORT;` |
|        - |  7939 | `		}` |
|      ! 0 |  7940 | `		goto Synchronize;` |
|        - |  7941 | `	}` |
|        - |  7942 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   143889 |  7943 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       61 |  7944 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       38 |  7945 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       19 |  7946 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       42 |  7947 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7948 | `			return SXERR_ABORT;` |
|       42 |  7949 | `		}else if( rc != SXRET_OK ){` |
|        3 |  7950 | `			goto Synchronize;` |
|        - |  7951 | `		}` |
|       18 |  7952 | `	}` |
|        - |  7953 | `	/* Advance the stream cursor */` |
|   143887 |  7954 | `	pGen->pIn++;` |
|   143887 |  7955 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  7956 | `		/* Invalid declaration */` |
|      ! 0 |  7957 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  7958 | `		if( rc == SXERR_ABORT ){` |
|        - |  7959 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7960 | `			return SXERR_ABORT;` |
|        - |  7961 | `		}` |
|      ! 0 |  7962 | `		goto Synchronize;` |
|        - |  7963 | `	}` |
|   143887 |  7964 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  7965 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  7966 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  7967 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  7968 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   143882 |  7969 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       39 |  7970 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  7971 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  7972 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  7973 | `			&pClass->sName,pName,&sTypeText);` |
|        6 |  7974 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7975 | `			return SXERR_ABORT;` |
|        - |  7976 | `		}` |
|        6 |  7977 | `		goto Synchronize;` |
|        - |  7978 | `	}` |
|        - |  7979 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|        - |  7980 | `	 * constant initializer ("New expressions are not supported in this context").` |
|        - |  7981 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|   143883 |  7982 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|        5 |  7983 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  7984 | `			"New expressions are not supported in this context");` |
|        5 |  7985 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7986 | `			return SXERR_ABORT;` |
|        - |  7987 | `		}` |
|        5 |  7988 | `		goto Synchronize;` |
|        - |  7989 | `	}` |
|        - |  7990 | `	/* Allocate a new class attribute */` |
|   143879 |  7991 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   143879 |  7992 | `	if( pCons ){` |
|   143879 |  7993 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   143879 |  7994 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  7995 | `			return SXERR_ABORT;` |
|        - |  7996 | `		}` |
|    71937 |  7997 | `	}` |
|   143879 |  7998 | `	if( pCons == 0 ){` |
|      ! 0 |  7999 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8000 | `		return SXERR_ABORT;` |
|        - |  8001 | `	}` |
|   143879 |  8002 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  8003 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  8004 | `	}` |
|        - |  8005 | `	/* Swap bytecode container */` |
|   143879 |  8006 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   143879 |  8007 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  8008 | `	/* Compile constant value.` |
|        - |  8009 | `	 */` |
|   143879 |  8010 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   143879 |  8011 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  8012 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  8013 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8014 | `			return SXERR_ABORT;` |
|        - |  8015 | `		}` |
|        1 |  8016 | `	}` |
|        - |  8017 | `	/* Emit the done instruction */` |
|   143879 |  8018 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   143879 |  8019 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   143879 |  8020 | `	if( rc == SXERR_ABORT ){` |
|        - |  8021 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  8022 | `		return SXERR_ABORT;` |
|        - |  8023 | `	}` |
|        - |  8024 | `	/* All done,install the constant */` |
|   143879 |  8025 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   143879 |  8026 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8027 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8028 | `		return SXERR_ABORT;` |
|        - |  8029 | `	}` |
|   143879 |  8030 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8031 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        3 |  8032 | `		pGen->pIn++; /* Jump the comma */` |
|        3 |  8033 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  8034 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8035 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8036 | `				pTok--;` |
|      ! 0 |  8037 | `			}` |
|      ! 0 |  8038 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8039 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  8040 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8041 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8042 | `				return SXERR_ABORT;` |
|        - |  8043 | `			}` |
|      ! 0 |  8044 | `		}else{` |
|        3 |  8045 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|        3 |  8046 | `				goto loop;` |
|        - |  8047 | `			}` |
|        - |  8048 | `		}` |
|      ! 0 |  8049 | `	}` |
|   143877 |  8050 | `	SySetRelease(&aUnionAlts);` |
|   143877 |  8051 | `	return SXRET_OK;` |
|        5 |  8052 | `Synchronize:` |
|       13 |  8053 | `	SySetRelease(&aUnionAlts);` |
|        - |  8054 | `	/* Synchronize with the first semi-colon */` |
|       45 |  8055 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  8056 | `		pGen->pIn++;` |
|        3 |  8057 | `	}` |
|       13 |  8058 | `	return SXERR_CORRUPT;` |
|    71946 |  8059 | `}` |
|        - |  8060 | `/*` |
|        - |  8061 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  8062 | ` * According to the PHP language reference manual` |
|        - |  8063 | ` *  Properties` |
|        - |  8064 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  8065 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  8066 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  8067 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  8068 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  8069 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  8070 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  8071 | ` * Symisc eXtension.` |
|        - |  8072 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  8073 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  8074 | ` *  Example:` |
|        - |  8075 | ` *   class Test{` |
|        - |  8076 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  8077 | ` *   };` |
|        - |  8078 | ` *   var_dump(TEST::myVar);` |
|        - |  8079 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  8080 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  8081 | ` */` |
|        - |  8082 | `/*` |
|        - |  8083 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  8084 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  8085 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  8086 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  8087 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  8088 | ` */` |
|  1310364 |  8089 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  8090 | `{` |
|  1310369 |  8091 | `	SyToken *p = pStart;` |
|  1310369 |  8092 | `	int bFirst = 1;` |
|  1310369 |  8093 | `	if( p >= pEnd ) return 0;` |
|        - |  8094 | ``	/* Optional nullable `?` shorthand. */`` |
|  1310369 |  8095 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       25 |  8096 | `		p++;` |
|       25 |  8097 | `		if( p >= pEnd ) return 0;` |
|       11 |  8098 | `	}` |
|        - |  8099 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  8100 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  8101 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  8102 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   655182 |  8103 | `	for(;;){` |
|  1310389 |  8104 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  8105 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  8106 | `			p++;` |
|        9 |  8107 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  8108 | `			if( p >= pEnd ) return 0;` |
|        3 |  8109 | `			p++; /* skip ')' */` |
|        2 |  8110 | `		}else{` |
|        - |  8111 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  8112 | ``			 * then any `&`-joined intersection members. */`` |
|  1310387 |  8113 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  1310387 |  8114 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  8115 | `				return 0;` |
|        - |  8116 | `			}` |
|        - |  8117 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  8118 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  8119 | `			 * may still appear at the initial dispatch site). */` |
|  1310387 |  8120 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  1310339 |  8121 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  1310334 |  8122 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    23590 |  8123 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  1310171 |  8124 | `					return 0;` |
|        - |  8125 | `				}` |
|       84 |  8126 | `			}` |
|      221 |  8127 | `			p++;` |
|      223 |  8128 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8129 | `				p += 2;` |
|        1 |  8130 | `			}` |
|      327 |  8131 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|      224 |  8132 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8133 | `				p++; /* skip '&' */` |
|        3 |  8134 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  8135 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  8136 | `				p++;` |
|        3 |  8137 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  8138 | `					p += 2;` |
|      ! 0 |  8139 | `				}` |
|        1 |  8140 | `			}` |
|        - |  8141 | `		}` |
|      223 |  8142 | `		bFirst = 0;` |
|      218 |  8143 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  8144 | `			&& p->sData.zString[0] == '\|' ){` |
|       25 |  8145 | ``			p++; /* next `\|`-separated part */`` |
|       25 |  8146 | `			continue;` |
|        - |  8147 | `		}` |
|      203 |  8148 | `		break;` |
|      ! 0 |  8149 | `	}` |
|      203 |  8150 | `	if( p >= pEnd ) return 0;` |
|      203 |  8151 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   655187 |  8152 | `}` |
|        - |  8153 |  |
|        - |  8154 | `/*` |
|        - |  8155 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - |  8156 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - |  8157 | ` * if not). Recognized forms:` |
|        - |  8158 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - |  8159 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - |  8160 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - |  8161 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - |  8162 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - |  8163 | ` * on unrecoverable error.` |
|        - |  8164 | ` *` |
|        - |  8165 | ` * When a type is parsed:` |
|        - |  8166 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - |  8167 | ` *   *pClass is set to the class name (for class types)` |
|        - |  8168 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - |  8169 | ` *   *pTypeText is set to the original text span of the type` |
|        - |  8170 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - |  8171 | ` */` |
|      198 |  8172 | `static sxi32 GenStateParsePropertyType(` |
|        - |  8173 | `	ph7_gen_state *pGen,` |
|        - |  8174 | `	sxu32 *pnType,` |
|        - |  8175 | `	SyString *pClass,` |
|        - |  8176 | `	sxi32 *piTypeFlags,` |
|        - |  8177 | `	SyString *pTypeText,` |
|        - |  8178 | `	SySet *pAlts` |
|        5 |  8179 | `){` |
|      203 |  8180 | `	sxi32 iFlags = 0;` |
|        - |  8181 | `	sxi32 rc;` |
|      203 |  8182 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  8183 | `		return SXRET_OK;` |
|        - |  8184 | `	}` |
|        - |  8185 | `	/* If the first token is '$', there's no type */` |
|      203 |  8186 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  8187 | `		return SXRET_OK;` |
|        - |  8188 | `	}` |
|      203 |  8189 | `	rc = GenStateParseUnionTypeDecl(` |
|       99 |  8190 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  8191 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  8192 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  8193 | `		/* bAllowVoid */ 0,` |
|      198 |  8194 | `		pGen->pIn->nLine);` |
|      203 |  8195 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8196 | `		return rc;` |
|        - |  8197 | `	}` |
|        - |  8198 | `	/* Verify next token is '$' (start of property name) */` |
|      203 |  8199 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8200 | `		return SXERR_SYNTAX;` |
|        - |  8201 | `	}` |
|      203 |  8202 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|      203 |  8203 | `	return SXRET_OK;` |
|      104 |  8204 | `}` |
|        - |  8205 |  |
|        - |  8206 | `/*` |
|        - |  8207 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - |  8208 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - |  8209 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - |  8210 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - |  8211 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - |  8212 | ` * by the type parser itself before reaching here.` |
|        - |  8213 | ` *` |
|        - |  8214 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - |  8215 | ` * use in the error message.` |
|        - |  8216 | ` */` |
|      366 |  8217 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  8218 | `	sxu32 nType,` |
|        - |  8219 | `	const SyString *pClass,` |
|        - |  8220 | `	const char **pzName,` |
|        - |  8221 | `	sxu32 *pnName)` |
|        5 |  8222 | `{` |
|        - |  8223 | `	const char *z;` |
|        - |  8224 | `	sxu32 n;` |
|      371 |  8225 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|      317 |  8226 | `		return 0;` |
|        - |  8227 | `	}` |
|       59 |  8228 | `	z = pClass->zString;` |
|       59 |  8229 | `	n = pClass->nByte;` |
|       59 |  8230 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 |  8231 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - |  8232 | `	}` |
|        - |  8233 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - |  8234 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - |  8235 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|       52 |  8236 | `	return 0;` |
|      188 |  8237 | `}` |
|        - |  8238 |  |
|        - |  8239 | `/*` |
|        - |  8240 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - |  8241 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - |  8242 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - |  8243 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - |  8244 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - |  8245 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - |  8246 | ` *` |
|        - |  8247 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - |  8248 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - |  8249 | ` */` |
|      304 |  8250 | `static sxi32 GenStateValidateMemberType(` |
|        - |  8251 | `	ph7_gen_state *pGen,` |
|        - |  8252 | `	ph7_class *pClass,` |
|        - |  8253 | `	const SyString *pMemberName,` |
|        - |  8254 | `	sxu32 nType,` |
|        - |  8255 | `	const SyString *pTypeClass,` |
|        - |  8256 | `	const SyString *pTypeText,` |
|        - |  8257 | `	SySet *pUnionAlts,` |
|        - |  8258 | `	const char *zErrFmt,` |
|        - |  8259 | `	sxu32 nLine)` |
|        5 |  8260 | `{` |
|      309 |  8261 | `	const char *zBad = 0;` |
|      309 |  8262 | `	sxu32 nBad = 0;` |
|        - |  8263 | `	SyString sFallback;` |
|        - |  8264 | `	const SyString *pBad;` |
|        - |  8265 | `	sxi32 rc;` |
|      309 |  8266 | `	int bDisallowed = 0;` |
|      309 |  8267 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  8268 | `		bDisallowed = 1;` |
|      307 |  8269 | `	}else if( pUnionAlts ){` |
|        - |  8270 | `		sxu32 i;` |
|       95 |  8271 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       67 |  8272 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       67 |  8273 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  8274 | `				bDisallowed = 1;` |
|        3 |  8275 | `				break;` |
|        - |  8276 | `			}` |
|       35 |  8277 | `		}` |
|       15 |  8278 | `	}` |
|      309 |  8279 | `	if( !bDisallowed ){` |
|      303 |  8280 | `		return SXRET_OK;` |
|        - |  8281 | `	}` |
|        - |  8282 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - |  8283 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - |  8284 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 |  8285 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 |  8286 | `		pBad = pTypeText;` |
|        5 |  8287 | `	}else{` |
|      ! 0 |  8288 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 |  8289 | `		pBad = &sFallback;` |
|        - |  8290 | `	}` |
|       11 |  8291 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  8292 | `		zErrFmt,` |
|        3 |  8293 | `		&pClass->sName,pMemberName,pBad);` |
|        8 |  8294 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  8295 | `		return SXERR_ABORT;` |
|        - |  8296 | `	}` |
|        8 |  8297 | `	return SXERR_SYNTAX;` |
|      157 |  8298 | `}` |
|        - |  8299 | `/*` |
|        - |  8300 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  8301 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  8302 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  8303 | ` * than promoted to a lexer keyword.` |
|        - |  8304 | ` */` |
| 10114838 |  8305 | `static int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  8306 | `{` |
| 10155977 |  8307 | `	return (pTok->nType & PH7_TK_ID)` |
|  5098553 |  8308 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 10155972 |  8309 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  8310 | `}` |
|   210564 |  8311 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  8312 | `{` |
|   210569 |  8313 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8314 | `	ph7_class_attr *pAttr;` |
|        - |  8315 | `	SyString *pName;` |
|        - |  8316 | `	sxi32 rc;` |
|   210569 |  8317 | `	sxu32 nType = 0;` |
|        - |  8318 | `	SyString sTypeClass;` |
|        - |  8319 | `	SyString sTypeText;` |
|        - |  8320 | `	SySet aUnionAlts;` |
|   210569 |  8321 | `	sxi32 iTypeFlags = 0;` |
|   210569 |  8322 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   210569 |  8323 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   210569 |  8324 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  8325 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  8326 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  8327 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   210569 |  8328 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  8329 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  8330 | `	}` |
|        - |  8331 | `	/* Extract visibility level */` |
|   210569 |  8332 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  8333 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   210668 |  8334 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      203 |  8335 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|      203 |  8336 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  8337 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  8338 | `			goto Synchronize;` |
|      203 |  8339 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  8340 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8341 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  8342 | `				&pGen->pIn->sData);` |
|      ! 0 |  8343 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8344 | `				return SXERR_ABORT;` |
|        - |  8345 | `			}` |
|      ! 0 |  8346 | `			goto Synchronize;` |
|      203 |  8347 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  8348 | `			return SXERR_ABORT;` |
|        - |  8349 | `		}` |
|       99 |  8350 | `	}` |
|      ! 0 |  8351 | `loop:` |
|   210573 |  8352 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8353 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 |  8354 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8355 | `			return SXERR_ABORT;` |
|        - |  8356 | `		}` |
|      ! 0 |  8357 | `		goto Synchronize;` |
|        - |  8358 | `	}` |
|   210573 |  8359 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   210573 |  8360 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - |  8361 | `		/* Invalid attribute name */` |
|      ! 0 |  8362 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 |  8363 | `		if( rc == SXERR_ABORT ){` |
|        - |  8364 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8365 | `			return SXERR_ABORT;` |
|        - |  8366 | `		}` |
|      ! 0 |  8367 | `		goto Synchronize;` |
|        - |  8368 | `	}` |
|        - |  8369 | `	/* Peek attribute name */` |
|   210573 |  8370 | `	pName = &pGen->pIn->sData;` |
|        - |  8371 | `	/* Advance the stream cursor */` |
|   210573 |  8372 | `	pGen->pIn++;` |
|   210573 |  8373 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|        - |  8374 | `		/* Invalid declaration */` |
|        3 |  8375 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|        3 |  8376 | `		if( rc == SXERR_ABORT ){` |
|        - |  8377 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8378 | `			return SXERR_ABORT;` |
|        - |  8379 | `		}` |
|        3 |  8380 | `		goto Synchronize;` |
|        - |  8381 | `	}` |
|        - |  8382 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - |  8383 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   210571 |  8384 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       41 |  8385 | `		const char *zRoErr = 0;` |
|       41 |  8386 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 |  8387 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       40 |  8388 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 |  8389 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       37 |  8390 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 |  8391 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 |  8392 | `		}` |
|       41 |  8393 | `		if( zRoErr ){` |
|       13 |  8394 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 |  8395 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8396 | `				return SXERR_ABORT;` |
|        - |  8397 | `			}` |
|       13 |  8398 | `			goto Synchronize;` |
|        - |  8399 | `		}` |
|       13 |  8400 | `	}` |
|        - |  8401 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - |  8402 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - |  8403 | `	 * by the type parser. */` |
|   210561 |  8404 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      299 |  8405 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - |  8406 | `			&sTypeText,` |
|      196 |  8407 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       98 |  8408 | `			"Property %z::$%z cannot have type %z",nLine);` |
|      201 |  8409 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8410 | `			return SXERR_ABORT;` |
|      201 |  8411 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  8412 | `			goto Synchronize;` |
|        - |  8413 | `		}` |
|       98 |  8414 | `	}` |
|        - |  8415 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   210561 |  8416 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 |  8417 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8418 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 |  8419 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8420 | `			return SXERR_ABORT;` |
|        - |  8421 | `		}` |
|        3 |  8422 | `		goto Synchronize;` |
|        - |  8423 | `	}` |
|        - |  8424 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - |  8425 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - |  8426 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - |  8427 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - |  8428 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - |  8429 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   210559 |  8430 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 |  8431 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8432 | `			"New expressions are not supported in this context");` |
|        6 |  8433 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8434 | `			return SXERR_ABORT;` |
|        - |  8435 | `		}` |
|        6 |  8436 | `		goto Synchronize;` |
|        - |  8437 | `	}` |
|        - |  8438 | `	/* Allocate a new class attribute */` |
|   210555 |  8439 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   210555 |  8440 | `	if( pAttr ){` |
|   210555 |  8441 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   210555 |  8442 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8443 | `			return SXERR_ABORT;` |
|        - |  8444 | `		}` |
|   105275 |  8445 | `	}` |
|   210555 |  8446 | `	if( pAttr == 0 ){` |
|      ! 0 |  8447 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  8448 | `		return SXERR_ABORT;` |
|        - |  8449 | `	}` |
|   210555 |  8450 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      199 |  8451 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       97 |  8452 | `	}` |
|   210555 |  8453 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - |  8454 | `		SySet *pInstrContainer;` |
|    86039 |  8455 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - |  8456 | `		/* Swap bytecode container */` |
|    86039 |  8457 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    86039 |  8458 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - |  8459 | `		/* Compile attribute value.` |
|        - |  8460 | `		 */` |
|    86039 |  8461 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    86039 |  8462 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  8463 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 |  8464 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8465 | `				return SXERR_ABORT;` |
|        - |  8466 | `			}` |
|      ! 0 |  8467 | `		}` |
|        - |  8468 | `		/* Emit the done instruction */` |
|    86039 |  8469 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    86039 |  8470 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    43017 |  8471 | `	}` |
|        - |  8472 | `	/* All done,install the attribute */` |
|   210555 |  8473 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   210555 |  8474 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8475 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8476 | `		return SXERR_ABORT;` |
|        - |  8477 | `	}` |
|   210555 |  8478 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8479 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 |  8480 | `		pGen->pIn++; /* Jump the comma */` |
|        5 |  8481 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  8482 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8483 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8484 | `				pTok--;` |
|      ! 0 |  8485 | `			}` |
|      ! 0 |  8486 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8487 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 |  8488 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8489 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8490 | `				return SXERR_ABORT;` |
|        - |  8491 | `			}` |
|      ! 0 |  8492 | `		}else{` |
|        5 |  8493 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 |  8494 | `				goto loop;` |
|        - |  8495 | `			}` |
|        - |  8496 | `		}` |
|      ! 0 |  8497 | `	}` |
|   210551 |  8498 | `	SySetRelease(&aUnionAlts);` |
|   210551 |  8499 | `	return SXRET_OK;` |
|        9 |  8500 | `Synchronize:` |
|        - |  8501 | `	/* Synchronize with the first semi-colon */` |
|       56 |  8502 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       37 |  8503 | `		pGen->pIn++;` |
|        3 |  8504 | `	}` |
|       22 |  8505 | `	SySetRelease(&aUnionAlts);` |
|       22 |  8506 | `	return SXERR_CORRUPT;` |
|   105287 |  8507 | `}` |
|        - |  8508 | `/*` |
|        - |  8509 | ` * Compile a class method.` |
|        - |  8510 | ` *` |
|        - |  8511 | ` * Refer to the official documentation for more information` |
|        - |  8512 | ` * on the powerful extension introduced by the PH7 engine` |
|        - |  8513 | ` * to the OO subsystem such as full type hinting,method` |
|        - |  8514 | ` * overloading and many more.` |
|        - |  8515 | ` */` |
|  1388098 |  8516 | `static sxi32 GenStateCompileClassMethod(` |
|        - |  8517 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  8518 | `	sxi32 iProtection,   /* Visibility level */` |
|        - |  8519 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - |  8520 | `	int doBody,          /* TRUE to process method body */` |
|        - |  8521 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - |  8522 | `	)` |
|        5 |  8523 | `{` |
|  1388103 |  8524 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1388103 |  8525 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - |  8526 | `	ph7_class_method *pMeth;` |
|        - |  8527 | `	sxi32 iFuncFlags;` |
|        - |  8528 | `	SyString *pName;` |
|        - |  8529 | `	SyToken *pEnd;` |
|        - |  8530 | `	sxi32 rc;` |
|        - |  8531 | `	/* Extract visibility level */` |
|  1388103 |  8532 | `	iProtection = GetProtectionLevel(iProtection);` |
|  1388103 |  8533 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  1388103 |  8534 | `	iFuncFlags = 0;` |
|  1388103 |  8535 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  8536 | `		/* Invalid method name */` |
|      ! 0 |  8537 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8538 | `		if( rc == SXERR_ABORT ){` |
|        - |  8539 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8540 | `			return SXERR_ABORT;` |
|        - |  8541 | `		}` |
|      ! 0 |  8542 | `		goto Synchronize;` |
|        - |  8543 | `	}` |
|  1388103 |  8544 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  8545 | `		/* Return by reference,remember that */` |
|      ! 0 |  8546 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  8547 | `		/* Jump the '&' token */` |
|      ! 0 |  8548 | `		pGen->pIn++;` |
|      ! 0 |  8549 | `	}` |
|  1388103 |  8550 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  8551 | `		/* Invalid method name */` |
|      ! 0 |  8552 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8553 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8554 | `			return SXERR_ABORT;` |
|        - |  8555 | `		}` |
|      ! 0 |  8556 | `		goto Synchronize;` |
|        - |  8557 | `	}` |
|        - |  8558 | `	/* Peek method name */` |
|  1388103 |  8559 | `	pName = &pGen->pIn->sData;` |
|  1388103 |  8560 | `	nLine = pGen->pIn->nLine;` |
|        - |  8561 | `	/* Jump the method name */` |
|  1388103 |  8562 | `	pGen->pIn++;` |
|  1388103 |  8563 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - |  8564 | `		/* Abstract method */` |
|   101051 |  8565 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 |  8566 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8567 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 |  8568 | `				&pClass->sName,pName);` |
|      ! 0 |  8569 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8570 | `				return SXERR_ABORT;` |
|        - |  8571 | `			}` |
|      ! 0 |  8572 | `		}` |
|        - |  8573 | `		/* Assemble method signature only */` |
|   101051 |  8574 | `		doBody = FALSE;` |
|    50523 |  8575 | `	}` |
|  1388103 |  8576 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  8577 | `		/* Syntax error */` |
|      ! 0 |  8578 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 |  8579 | `		if( rc == SXERR_ABORT ){` |
|        - |  8580 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8581 | `			return SXERR_ABORT;` |
|        - |  8582 | `		}` |
|      ! 0 |  8583 | `		goto Synchronize;` |
|        - |  8584 | `	}` |
|        - |  8585 | `	/* Allocate a new class_method instance */` |
|  1388103 |  8586 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  1388103 |  8587 | `	if( pMeth == 0 ){` |
|      ! 0 |  8588 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8589 | `		return SXERR_ABORT;` |
|        - |  8590 | `	}` |
|  1388103 |  8591 | `	pMeth->sFunc.nLine = nKwLine;` |
|  1388103 |  8592 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  1388103 |  8593 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8594 | `		return SXERR_ABORT;` |
|        - |  8595 | `	}` |
|        - |  8596 | `	/* Jump the left parenthesis '(' */` |
|  1388103 |  8597 | `	pGen->pIn++;` |
|  1388103 |  8598 | `	pEnd = 0; /* cc warning */` |
|        - |  8599 | `	/* Delimit the method signature */` |
|  1388103 |  8600 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1388103 |  8601 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8602 | `		/* Syntax error */` |
|        3 |  8603 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 |  8604 | `		if( rc == SXERR_ABORT ){` |
|        - |  8605 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8606 | `			return SXERR_ABORT;` |
|        - |  8607 | `		}` |
|        3 |  8608 | `		goto Synchronize;` |
|        - |  8609 | `	}` |
|        - |  8610 | `	{` |
|  1388101 |  8611 | `		int bIsCtor = 0;` |
|  1388101 |  8612 | `		int bAbstractCtor = 0;` |
|  1388096 |  8613 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   810716 |  8614 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  1335567 |  8615 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   105073 |  8616 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 |  8617 | `				bAbstractCtor = 1;` |
|        2 |  8618 | `			}else{` |
|   105071 |  8619 | `				bIsCtor = 1;` |
|        - |  8620 | `			}` |
|    52534 |  8621 | `		}` |
|  1388101 |  8622 | `		if( pGen->pIn < pEnd ){` |
|        - |  8623 | `			/* Collect method arguments */` |
|   389043 |  8624 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   389043 |  8625 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8626 | `				return SXERR_ABORT;` |
|        - |  8627 | `			}` |
|   194519 |  8628 | `		}` |
|        - |  8629 | `	}` |
|        - |  8630 | `	/* Point past ')' and parse optional return type ': type' */` |
|  1388101 |  8631 | `	pGen->pIn = &pEnd[1];` |
|        - |  8632 | `	{` |
|  1388101 |  8633 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  1388101 |  8634 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  8635 | `			return SXERR_ABORT;` |
|  1388101 |  8636 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 |  8637 | `			goto Synchronize;` |
|        - |  8638 | `		}` |
|        - |  8639 | `	}` |
|        - |  8640 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - |  8641 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - |  8642 | `	 * since we mint real ph7_class_attr entries. */` |
|        - |  8643 | `	{` |
|  1388101 |  8644 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - |  8645 | `		sxu32 i;` |
|  1971499 |  8646 | `		for( i = 0; i < nArg; i++ ){` |
|   583413 |  8647 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - |  8648 | `			ph7_class_attr *pAttr;` |
|   583413 |  8649 | `			sxi32 iAttrFlags = 0;` |
|        - |  8650 | `			int bArgTyped;` |
|   583413 |  8651 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   583337 |  8652 | `				continue;` |
|        - |  8653 | `			}` |
|        - |  8654 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - |  8655 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - |  8656 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       55 |  8657 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       82 |  8658 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       81 |  8659 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 |  8660 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8661 | `					"Cannot declare variadic promoted property");` |
|        3 |  8662 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8663 | `					return SXERR_ABORT;` |
|        - |  8664 | `				}` |
|        3 |  8665 | `				goto Synchronize;` |
|        - |  8666 | `			}` |
|        - |  8667 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - |  8668 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - |  8669 | `			 * appear as an alternative of a union type. */` |
|       79 |  8670 | `			if( bArgTyped ){` |
|      110 |  8671 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       70 |  8672 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       70 |  8673 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       35 |  8674 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       75 |  8675 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8676 | `					return SXERR_ABORT;` |
|       75 |  8677 | `				}else if( rc != SXRET_OK ){` |
|        6 |  8678 | `					goto Synchronize;` |
|        - |  8679 | `				}` |
|       33 |  8680 | `			}` |
|        - |  8681 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       75 |  8682 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 |  8683 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8684 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 |  8685 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8686 | `					return SXERR_ABORT;` |
|        - |  8687 | `				}` |
|        3 |  8688 | `				goto Synchronize;` |
|        - |  8689 | `			}` |
|       73 |  8690 | `			if( bArgTyped ){` |
|       69 |  8691 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       32 |  8692 | `			}` |
|       73 |  8693 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 |  8694 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 |  8695 | `			}` |
|       73 |  8696 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 |  8697 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 |  8698 | `			}` |
|       73 |  8699 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - |  8700 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - |  8701 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 |  8702 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 |  8703 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8704 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 |  8705 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  8706 | `						return SXERR_ABORT;` |
|        - |  8707 | `					}` |
|        3 |  8708 | `					goto Synchronize;` |
|        - |  8709 | `				}` |
|       24 |  8710 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 |  8711 | `			}` |
|       71 |  8712 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       71 |  8713 | `			if( pAttr == 0 ){` |
|      ! 0 |  8714 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8715 | `				return SXERR_ABORT;` |
|        - |  8716 | `			}` |
|       71 |  8717 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       69 |  8718 | `				pAttr->nType = pArg->nType;` |
|       69 |  8719 | `				pAttr->sClass = pArg->sClass;` |
|       69 |  8720 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       69 |  8721 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  8722 | `					sxu32 k;` |
|       20 |  8723 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 |  8724 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 |  8725 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 |  8726 | `					}` |
|        3 |  8727 | `				}` |
|       32 |  8728 | `			}` |
|       71 |  8729 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       71 |  8730 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8731 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8732 | `				return SXERR_ABORT;` |
|        - |  8733 | `			}` |
|       38 |  8734 | `		}` |
|        - |  8735 | `	}` |
|  1388091 |  8736 | `	if( doBody ){` |
|        - |  8737 | `		/* Compile method body */` |
|  1287045 |  8738 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  1287045 |  8739 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8740 | `			return SXERR_ABORT;` |
|        - |  8741 | `		}` |
|        - |  8742 | `		/* The cursor sits just past the body's closing brace */` |
|  1287045 |  8743 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   643525 |  8744 | `	}else{` |
|        - |  8745 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   101051 |  8746 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   101051 |  8747 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    50523 |  8748 | `		}` |
|        - |  8749 | `		/* Only method signature is allowed */` |
|   101051 |  8750 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 |  8751 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8752 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 |  8753 | `				if( rc == SXERR_ABORT ){` |
|        - |  8754 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8755 | `					return SXERR_ABORT;` |
|        - |  8756 | `				}` |
|      ! 0 |  8757 | `				return SXERR_CORRUPT;` |
|        - |  8758 | `			}` |
|        - |  8759 | `	}` |
|        - |  8760 | `	/* All done,install the method */` |
|  1388091 |  8761 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  1388091 |  8762 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8763 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8764 | `		return SXERR_ABORT;` |
|        - |  8765 | `	}` |
|  1388091 |  8766 | `	return SXRET_OK;` |
|        6 |  8767 | `Synchronize:` |
|        - |  8768 | `	/* Synchronize with the first semi-colon */` |
|       40 |  8769 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 |  8770 | `		pGen->pIn++;` |
|        4 |  8771 | `	}` |
|       16 |  8772 | `	return SXERR_CORRUPT;` |
|   694054 |  8773 | `}` |
|        - |  8774 | `/*` |
|        - |  8775 | ` * Compile an object interface.` |
|        - |  8776 | ` *  According to the PHP language reference manual` |
|        - |  8777 | ` *   Object Interfaces:` |
|        - |  8778 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - |  8779 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - |  8780 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  8781 | ` *   class, but without any of the methods having their contents defined.` |
|        - |  8782 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  8783 | ` */` |
|    46708 |  8784 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 |  8785 | `{` |
|    46713 |  8786 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8787 | `	ph7_class *pClass,*pBase;` |
|        - |  8788 | `	SyToken *pEnd,*pTmp;` |
|        - |  8789 | `	SyString *pName;` |
|        - |  8790 | `	sxi32 nKwrd;` |
|        - |  8791 | `	sxi32 rc;` |
|        - |  8792 | `	/* Jump the 'interface' keyword */` |
|    46713 |  8793 | `	pGen->pIn++;` |
|        - |  8794 | `	/* Extract interface name */` |
|    46713 |  8795 | `	pName = &pGen->pIn->sData;` |
|        - |  8796 | `	/* Advance the stream cursor */` |
|    46713 |  8797 | `	pGen->pIn++;` |
|        - |  8798 | `	/* Build FQN and obtain a raw class */ {` |
|        - |  8799 | `		SyBlob sFQN;` |
|        - |  8800 | `		SyString sFQNStr;` |
|    46713 |  8801 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    46713 |  8802 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    46713 |  8803 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    46713 |  8804 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    46713 |  8805 | `		SyBlobRelease(&sFQN);` |
|        - |  8806 | `	}` |
|    46713 |  8807 | `	if( pClass == 0 ){` |
|      ! 0 |  8808 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8809 | `		return SXERR_ABORT;` |
|        - |  8810 | `	}` |
|    46713 |  8811 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    46713 |  8812 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8813 | `		return SXERR_ABORT;` |
|        - |  8814 | `	}` |
|        - |  8815 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    46713 |  8816 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - |  8817 | `	/* Assume no base class is given */` |
|    46713 |  8818 | `	pBase = 0;` |
|    46713 |  8819 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    15551 |  8820 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    15551 |  8821 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|        - |  8822 | `			SyBlob sResolved;` |
|        - |  8823 | `			SyString sBaseName;` |
|        - |  8824 | `			sxu32 nRefLine;` |
|        - |  8825 | `			/* Extract base interface */` |
|    15551 |  8826 | `			pGen->pIn++;` |
|    15551 |  8827 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    15551 |  8828 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15551 |  8829 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  8830 | `				SyBlobRelease(&sResolved);` |
|      ! 0 |  8831 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8832 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 |  8833 | `					pName);` |
|      ! 0 |  8834 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8835 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8836 | `					return SXERR_ABORT;` |
|        - |  8837 | `				}` |
|      ! 0 |  8838 | `				return SXRET_OK;` |
|        - |  8839 | `			}` |
|    23324 |  8840 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    15546 |  8841 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15551 |  8842 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  8843 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  8844 | `			/* Only interfaces is allowed */` |
|    15551 |  8845 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  8846 | `				pBase = pBase->pNextName;` |
|      ! 0 |  8847 | `			}` |
|    15551 |  8848 | `			if( pBase == 0 ){` |
|      ! 0 |  8849 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  8850 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 |  8851 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8852 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  8853 | `					return SXERR_ABORT;` |
|        - |  8854 | `				}` |
|      ! 0 |  8855 | `			}` |
|    15551 |  8856 | `			SyBlobRelease(&sResolved);` |
|     7773 |  8857 | `		}` |
|     7773 |  8858 | `	}` |
|    46713 |  8859 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  8860 | `		/* Syntax error */` |
|      ! 0 |  8861 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 |  8862 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8863 | `		if( rc == SXERR_ABORT ){` |
|        - |  8864 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8865 | `			return SXERR_ABORT;` |
|        - |  8866 | `		}` |
|      ! 0 |  8867 | `		return SXRET_OK;` |
|        - |  8868 | `	}` |
|    46713 |  8869 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    46713 |  8870 | `	pEnd = 0; /* cc warning */` |
|        - |  8871 | `	/* Delimit the interface body */` |
|    46713 |  8872 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    46713 |  8873 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8874 | `		/* Syntax error */` |
|      ! 0 |  8875 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 |  8876 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8877 | `		if( rc == SXERR_ABORT ){` |
|        - |  8878 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8879 | `			return SXERR_ABORT;` |
|        - |  8880 | `		}` |
|      ! 0 |  8881 | `		return SXRET_OK;` |
|        - |  8882 | `	}` |
|        - |  8883 | `	/* The delimiter token is the interface body's closing brace */` |
|    46713 |  8884 | `	pClass->nEndLine = pEnd->nLine;` |
|        - |  8885 | `	/* Swap token stream */` |
|    46713 |  8886 | `	pTmp = pGen->pEnd;` |
|    46713 |  8887 | `	pGen->pEnd = pEnd;` |
|        - |  8888 | `	/* Start the parse process` |
|        - |  8889 | `	 * Note (According to the PHP reference manual):` |
|        - |  8890 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - |  8891 | `	 *  Only 'public' visibility is allowed.` |
|        - |  8892 | `	 */` |
|    73875 |  8893 | `	for(;;){` |
|        - |  8894 | `		/* Jump leading/trailing semi-colons */` |
|   248797 |  8895 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   101047 |  8896 | `			pGen->pIn++;` |
|        5 |  8897 | `		}` |
|   147755 |  8898 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  8899 | `			/* End of interface body */` |
|    46709 |  8900 | `			break;` |
|        - |  8901 | `		}` |
|        - |  8902 | `		/* Bind a directly-preceding docblock to this member */` |
|   101051 |  8903 | `		GenStateSetPendingDoc(&(*pGen));` |
|   101051 |  8904 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  8905 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8906 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 |  8907 | `				&pGen->pIn->sData,pName);` |
|      ! 0 |  8908 | `			if( rc == SXERR_ABORT ){` |
|        - |  8909 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  8910 | `				return SXERR_ABORT;` |
|        - |  8911 | `			}` |
|      ! 0 |  8912 | `			goto done;` |
|        - |  8913 | `		}` |
|        - |  8914 | `		/* Extract the current keyword */` |
|   101051 |  8915 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101051 |  8916 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - |  8917 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - |  8918 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 |  8919 | `			const char *zKind = "member";` |
|        3 |  8920 | `			SyString *pMemberName = 0;` |
|        3 |  8921 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 |  8922 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 |  8923 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 |  8924 | `					zKind = "constant";` |
|        3 |  8925 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 |  8926 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 |  8927 | `					}` |
|        1 |  8928 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  8929 | `					zKind = "method";` |
|      ! 0 |  8930 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 |  8931 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 |  8932 | `					}` |
|      ! 0 |  8933 | `				}` |
|        1 |  8934 | `			}` |
|        3 |  8935 | `			if( pMemberName ){` |
|        4 |  8936 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 |  8937 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 |  8938 | `			}else{` |
|      ! 0 |  8939 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8940 | `					"Access type for interface %s must be public",zKind);` |
|        - |  8941 | `			}` |
|        3 |  8942 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8943 | `				return SXERR_ABORT;` |
|        - |  8944 | `			}` |
|        3 |  8945 | `			goto done;` |
|        - |  8946 | `		}` |
|   101049 |  8947 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  8948 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8949 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  8950 | `			if( rc == SXERR_ABORT ){` |
|        - |  8951 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  8952 | `				return SXERR_ABORT;` |
|        - |  8953 | `			}` |
|      ! 0 |  8954 | `			goto done;` |
|        - |  8955 | `		}` |
|   101049 |  8956 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - |  8957 | `			/* Advance the stream cursor */` |
|   101031 |  8958 | `			pGen->pIn++;` |
|   101031 |  8959 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  8960 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8961 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  8962 | `				if( rc == SXERR_ABORT ){` |
|        - |  8963 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8964 | `					return SXERR_ABORT;` |
|        - |  8965 | `				}` |
|      ! 0 |  8966 | `				goto done;` |
|        - |  8967 | `			}` |
|   101031 |  8968 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101031 |  8969 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  8970 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8971 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  8972 | `				if( rc == SXERR_ABORT ){` |
|        - |  8973 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8974 | `					return SXERR_ABORT;` |
|        - |  8975 | `				}` |
|      ! 0 |  8976 | `				goto done;` |
|        - |  8977 | `			}` |
|    50513 |  8978 | `		}` |
|   101049 |  8979 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - |  8980 | `			/* Parse constant */` |
|       16 |  8981 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       16 |  8982 | `			if( rc != SXRET_OK ){` |
|        3 |  8983 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8984 | `					return SXERR_ABORT;` |
|        - |  8985 | `				}` |
|        3 |  8986 | `				goto done;` |
|        - |  8987 | `			}` |
|        7 |  8988 | `		}else{` |
|   101035 |  8989 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   101035 |  8990 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - |  8991 | `				/* Static method,record that */` |
|    11657 |  8992 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - |  8993 | `				/* Advance the stream cursor */` |
|    11657 |  8994 | `				pGen->pIn++;` |
|    11652 |  8995 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11657 |  8996 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  8997 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8998 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  8999 | `						if( rc == SXERR_ABORT ){` |
|        - |  9000 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 |  9001 | `							return SXERR_ABORT;` |
|        - |  9002 | `						}` |
|      ! 0 |  9003 | `						goto done;` |
|        - |  9004 | `				}` |
|     5826 |  9005 | `			}` |
|        - |  9006 | `			/* Process method signature (no body for interface methods) */` |
|   101035 |  9007 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   101035 |  9008 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9009 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9010 | `					return SXERR_ABORT;` |
|        - |  9011 | `				}` |
|      ! 0 |  9012 | `				goto done;` |
|        - |  9013 | `			}` |
|        - |  9014 | `		}` |
|        5 |  9015 | `	}` |
|        - |  9016 | `	/* Install the interface */` |
|    46709 |  9017 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    46709 |  9018 | `	if( rc == SXRET_OK && pBase ){` |
|        - |  9019 | `		/* Inherit from the base interface */` |
|    15551 |  9020 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     7773 |  9021 | `	}` |
|    46709 |  9022 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9023 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9024 | `		return SXERR_ABORT;` |
|        - |  9025 | `	}` |
|    23352 |  9026 | `done:` |
|        - |  9027 | `	/* Point beyond the interface body */` |
|    46713 |  9028 | `	pGen->pIn  = &pEnd[1];` |
|    46713 |  9029 | `	pGen->pEnd = pTmp;` |
|    46713 |  9030 | `	return PH7_OK;` |
|    23359 |  9031 | `}` |
|        - |  9032 | `/*` |
|        - |  9033 | ` * Compile a user-defined class.` |
|        - |  9034 | ` * According to the PHP language reference manual` |
|        - |  9035 | ` *  class` |
|        - |  9036 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - |  9037 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - |  9038 | ` *  of the properties and methods belonging to the class.` |
|        - |  9039 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - |  9040 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - |  9041 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - |  9042 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  9043 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - |  9044 | ` *  (called "methods").` |
|        - |  9045 | ` */` |
|        - |  9046 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - |  9047 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - |  9048 | `struct TraitUseEntry {` |
|        - |  9049 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - |  9050 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - |  9051 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - |  9052 | `};` |
|        - |  9053 | `/*` |
|        - |  9054 | ` * Validate that methods implementing interface contracts have compatible` |
|        - |  9055 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - |  9056 | ` */` |
|   215188 |  9057 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9058 | `{` |
|        - |  9059 | `	ph7_class **apIface;` |
|        - |  9060 | `	sxu32 nIface,i;` |
|        - |  9061 | `	sxi32 rc;` |
|   215193 |  9062 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 |  9063 | `		return SXRET_OK;` |
|        - |  9064 | `	}` |
|   215193 |  9065 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   215193 |  9066 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   429139 |  9067 | `	for(i = 0; i < nIface; i++){` |
|   213951 |  9068 | `		ph7_class *pIface = apIface[i];` |
|        - |  9069 | `		SyHashEntry *pEntry;` |
|   213951 |  9070 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   498055 |  9071 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   284109 |  9072 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |  9073 | `			ph7_class_method *pImplMeth;` |
|   284109 |  9074 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - |  9075 | `			/* Find the implementing method in the class */` |
|   284109 |  9076 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   284109 |  9077 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       18 |  9078 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - |  9079 | `			}` |
|        - |  9080 | `			/* Check visibility: interface methods must be implemented as public */` |
|   284095 |  9081 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 |  9082 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9083 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 |  9084 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 |  9085 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9086 | `					return SXERR_ABORT;` |
|        - |  9087 | `				}` |
|        1 |  9088 | `			}` |
|        - |  9089 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - |  9090 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - |  9091 | `			 */` |
|        - |  9092 | `			{` |
|   284095 |  9093 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   284095 |  9094 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   284095 |  9095 | `				int sigError = 0;` |
|   284095 |  9096 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 |  9097 | `					sigError = 1;` |
|   284094 |  9098 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - |  9099 | `					/* Extra parameters must all have default values */` |
|        6 |  9100 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - |  9101 | `					sxu32 k;` |
|        8 |  9102 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|        6 |  9103 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 |  9104 | `							sigError = 1;` |
|        3 |  9105 | `							break;` |
|        - |  9106 | `						}` |
|        2 |  9107 | `					}` |
|        2 |  9108 | `				}` |
|   284095 |  9109 | `				if( sigError ){` |
|        - |  9110 | `					SyBlob sImplSig, sIfaceSig;` |
|        - |  9111 | `					ph7_vm_func_arg *aArgs;` |
|        - |  9112 | `					sxu32 j;` |
|        6 |  9113 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 |  9114 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - |  9115 | `					/* Build implementing method signature */` |
|        6 |  9116 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 |  9117 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 |  9118 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 |  9119 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 |  9120 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9121 | `					}` |
|        - |  9122 | `					/* Build interface method signature */` |
|        6 |  9123 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 |  9124 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 |  9125 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 |  9126 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 |  9127 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9128 | `					}` |
|        8 |  9129 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9130 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 |  9131 | `						&pClass->sName,pMName,` |
|        4 |  9132 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 |  9133 | `						&pIface->sName,pMName,` |
|        4 |  9134 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 |  9135 | `					SyBlobRelease(&sImplSig);` |
|        6 |  9136 | `					SyBlobRelease(&sIfaceSig);` |
|        6 |  9137 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9138 | `						return SXERR_ABORT;` |
|        - |  9139 | `					}` |
|        2 |  9140 | `				}` |
|        - |  9141 | `			}` |
|        5 |  9142 | `		}` |
|   106978 |  9143 | `	}` |
|   215193 |  9144 | `	return SXRET_OK;` |
|   107599 |  9145 | `}` |
|        - |  9146 | `/*` |
|        - |  9147 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - |  9148 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - |  9149 | ` */` |
|   215188 |  9150 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9151 | `{` |
|        - |  9152 | `	ph7_class_method *pMeth;` |
|        - |  9153 | `	SyHashEntry *pEntry;` |
|        - |  9154 | `	sxu32 nAbstract;` |
|        - |  9155 | `	SyBlob sMsg;` |
|        - |  9156 | `	sxi32 rc;` |
|        - |  9157 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   215193 |  9158 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     7811 |  9159 | `		return SXRET_OK;` |
|        - |  9160 | `	}` |
|        - |  9161 | `	/* Count abstract methods */` |
|   207387 |  9162 | `	nAbstract = 0;` |
|   207387 |  9163 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  3068133 |  9164 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  2860751 |  9165 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  2860751 |  9166 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       20 |  9167 | `			nAbstract++;` |
|        8 |  9168 | `		}` |
|        5 |  9169 | `	}` |
|   207387 |  9170 | `	if( nAbstract == 0 ){` |
|   207373 |  9171 | `		return SXRET_OK;` |
|        - |  9172 | `	}` |
|        - |  9173 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 |  9174 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 |  9175 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - |  9176 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 |  9177 | `		&pClass->sName,nAbstract,` |
|        7 |  9178 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 |  9179 | `		(nAbstract > 1 ? "s" : ""));` |
|        - |  9180 | `	/* Second pass: list methods with origins */` |
|        - |  9181 | `	{` |
|       18 |  9182 | `		sxu32 nListed = 0;` |
|       18 |  9183 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 |  9184 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 |  9185 | `			ph7_class *pOrigin = 0;` |
|        - |  9186 | `			SyString *pMName;` |
|       22 |  9187 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 |  9188 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 |  9189 | `				continue;` |
|        - |  9190 | `			}` |
|       20 |  9191 | `			pMName = &pMeth->sFunc.sName;` |
|       20 |  9192 | `			if( nListed > 0 ){` |
|        3 |  9193 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 |  9194 | `			}` |
|        - |  9195 | `			/* Find the origin of this abstract method.` |
|        - |  9196 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - |  9197 | `			 * inheritance chains) take precedence for interface-declared` |
|        - |  9198 | `			 * methods. Abstract class methods only win when the class` |
|        - |  9199 | `			 * itself declared the abstract method (not inherited from` |
|        - |  9200 | `			 * an interface). Trait methods are adopted into the using` |
|        - |  9201 | `			 * class's namespace.` |
|        - |  9202 | `			 */` |
|        - |  9203 | `			{` |
|        - |  9204 | `				ph7_class **apIface;` |
|        - |  9205 | `				ph7_class **apTrait;` |
|        - |  9206 | `				ph7_class *pWalk;` |
|        - |  9207 | `				sxu32 i;` |
|        - |  9208 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - |  9209 | `				 * (one that was written in the class body, not inherited from an` |
|        - |  9210 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - |  9211 | `				 */` |
|       20 |  9212 | `				if( pClass->pBase ){` |
|       11 |  9213 | `					pWalk = pClass->pBase;` |
|       19 |  9214 | `					while( pWalk ){` |
|        - |  9215 | `						ph7_class_method *pParentMeth;` |
|       13 |  9216 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       13 |  9217 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - |  9218 | `							/* Exclude methods that came from an interface anywhere` |
|        - |  9219 | `							 * in this class's ancestor chain.` |
|        - |  9220 | `							 */` |
|       13 |  9221 | `							int fromIface = 0;` |
|       13 |  9222 | `							ph7_class *pAnc = pWalk;` |
|       17 |  9223 | `							while( pAnc ){` |
|        - |  9224 | `								ph7_class **apPI;` |
|        - |  9225 | `								sxu32 j;` |
|       15 |  9226 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       15 |  9227 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 |  9228 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 |  9229 | `										fromIface = 1;` |
|       10 |  9230 | `										break;` |
|        - |  9231 | `									}` |
|      ! 0 |  9232 | `								}` |
|       15 |  9233 | `								if( fromIface ) break;` |
|        6 |  9234 | `								pAnc = pAnc->pBase;` |
|        2 |  9235 | `							}` |
|       13 |  9236 | `							if( !fromIface ){` |
|        3 |  9237 | `								pOrigin = pWalk;` |
|        3 |  9238 | `								break;` |
|        - |  9239 | `							}` |
|        4 |  9240 | `						}` |
|       10 |  9241 | `						pWalk = pWalk->pBase;` |
|        2 |  9242 | `					}` |
|        4 |  9243 | `				}` |
|        - |  9244 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - |  9245 | `				 * each interface's own parent chain for the deepest origin.` |
|        - |  9246 | `				 */` |
|       20 |  9247 | `				if( !pOrigin ){` |
|       18 |  9248 | `					pWalk = pClass;` |
|       40 |  9249 | `					while( pWalk && !pOrigin ){` |
|       26 |  9250 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 |  9251 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 |  9252 | `							ph7_class *pIface = apIface[i];` |
|       16 |  9253 | `							ph7_class *pDeepest = 0;` |
|       28 |  9254 | `							while( pIface ){` |
|       16 |  9255 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 |  9256 | `									pDeepest = pIface;` |
|        6 |  9257 | `								}` |
|       16 |  9258 | `								pIface = pIface->pBase;` |
|        4 |  9259 | `							}` |
|       16 |  9260 | `							if( pDeepest ){` |
|       16 |  9261 | `								pOrigin = pDeepest;` |
|       16 |  9262 | `								break;` |
|        - |  9263 | `							}` |
|      ! 0 |  9264 | `						}` |
|       26 |  9265 | `						pWalk = pWalk->pBase;` |
|        4 |  9266 | `					}` |
|        7 |  9267 | `				}` |
|        - |  9268 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 |  9269 | `				if( !pOrigin ){` |
|        3 |  9270 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 |  9271 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 |  9272 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 |  9273 | `							pOrigin = pClass;` |
|        3 |  9274 | `							break;` |
|        - |  9275 | `						}` |
|      ! 0 |  9276 | `					}` |
|        1 |  9277 | `				}` |
|        - |  9278 | `			}` |
|       20 |  9279 | `			if( pOrigin ){` |
|       20 |  9280 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       12 |  9281 | `			}else{` |
|        - |  9282 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 |  9283 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|        - |  9284 | `			}` |
|       20 |  9285 | `			nListed++;` |
|        4 |  9286 | `		}` |
|        - |  9287 | `	}` |
|       18 |  9288 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 |  9289 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 |  9290 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 |  9291 | `	SyBlobRelease(&sMsg);` |
|       18 |  9292 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  9293 | `		return SXERR_ABORT;` |
|        - |  9294 | `	}` |
|       18 |  9295 | `	return SXRET_OK;` |
|   107599 |  9296 | `}` |
|        - |  9297 | `/*` |
|        - |  9298 | ` * Parse a class/interface name reference from the current token stream.` |
|        - |  9299 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - |  9300 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - |  9301 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - |  9302 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - |  9303 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - |  9304 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - |  9305 | ` */` |
|   192138 |  9306 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 |  9307 | `{` |
|   192143 |  9308 | `	int isAbsolute = 0;` |
|   192143 |  9309 | `	SyToken *pStart = pGen->pIn;` |
|        - |  9310 | `	SyBlob sName;` |
|   192143 |  9311 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4473 |  9312 | `		isAbsolute = 1;` |
|     4473 |  9313 | `		pGen->pIn++;` |
|     2234 |  9314 | `	}` |
|   192143 |  9315 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        8 |  9316 | `		pGen->pIn = pStart;` |
|        8 |  9317 | `		return SXERR_INVALID;` |
|        - |  9318 | `	}` |
|   192137 |  9319 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   192137 |  9320 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   192137 |  9321 | `	pGen->pIn++;` |
|   288219 |  9322 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    96092 |  9323 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       16 |  9324 | `		SyBlobAppend(&sName,"\\",1);` |
|       16 |  9325 | `		pGen->pIn++;` |
|       16 |  9326 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       16 |  9327 | `		pGen->pIn++;` |
|        2 |  9328 | `	}` |
|   192137 |  9329 | `	if( isAbsolute ){` |
|     4471 |  9330 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2238 |  9331 | `	}else{` |
|        - |  9332 | `		SyString sRaw;` |
|   187671 |  9333 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   187671 |  9334 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - |  9335 | `	}` |
|   192137 |  9336 | `	SyBlobRelease(&sName);` |
|   192137 |  9337 | `	return SXRET_OK;` |
|    96074 |  9338 | `}` |
|        - |  9339 | `/*` |
|        - |  9340 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - |  9341 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - |  9342 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - |  9343 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - |  9344 | ` * either direction cannot run unbounded.` |
|        - |  9345 | ` */` |
|        - |  9346 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    46804 |  9347 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 |  9348 | `{` |
|        - |  9349 | `	ph7_class **apParent;` |
|        - |  9350 | `	sxu32 n;` |
|   120839 |  9351 | `	while( pInterface ){` |
|    81813 |  9352 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 |  9353 | `			return FALSE;` |
|        - |  9354 | `		}` |
|   101252 |  9355 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    38878 |  9356 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7783 |  9357 | `			return TRUE;` |
|        - |  9358 | `		}` |
|    74035 |  9359 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    74035 |  9360 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|      ! 0 |  9361 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 |  9362 | `				return TRUE;` |
|        - |  9363 | `			}` |
|      ! 0 |  9364 | `		}` |
|    74035 |  9365 | `		pInterface = pInterface->pBase;` |
|    74035 |  9366 | `		iDepth++;` |
|        5 |  9367 | `	}` |
|    39031 |  9368 | `	return FALSE;` |
|    23407 |  9369 | `}` |
|    46804 |  9370 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 |  9371 | `{` |
|    46809 |  9372 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 |  9373 | `}` |
|        - |  9374 | `/*` |
|        - |  9375 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - |  9376 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - |  9377 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - |  9378 | ` */` |
|     7778 |  9379 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 |  9380 | `{` |
|     7787 |  9381 | `	while( pBase ){` |
|       10 |  9382 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 |  9383 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 |  9384 | `			return TRUE;` |
|        - |  9385 | `		}` |
|       10 |  9386 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 |  9387 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 |  9388 | `			return TRUE;` |
|        - |  9389 | `		}` |
|        5 |  9390 | `		pBase = pBase->pBase;` |
|        1 |  9391 | `	}` |
|     7779 |  9392 | `	return FALSE;` |
|     3894 |  9393 | `}` |
|        - |  9394 | `/*` |
|        - |  9395 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - |  9396 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - |  9397 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - |  9398 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - |  9399 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - |  9400 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - |  9401 | ` * pClass->aEnumCases for cases().` |
|        - |  9402 | ` */` |
|       42 |  9403 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9404 | `{` |
|       47 |  9405 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9406 | `	SySet *pInstrContainer;` |
|        - |  9407 | `	ph7_class_attr *pCase;` |
|        - |  9408 | `	SyString *pName;` |
|        - |  9409 | `	sxi32 rc;` |
|       47 |  9410 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|       47 |  9411 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  9412 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9413 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|      ! 0 |  9414 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9415 | `			return SXERR_ABORT;` |
|        - |  9416 | `		}` |
|      ! 0 |  9417 | `		goto Synchronize;` |
|        - |  9418 | `	}` |
|       47 |  9419 | `	pName = &pGen->pIn->sData;` |
|        - |  9420 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|       47 |  9421 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 |  9422 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9423 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 |  9424 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9425 | `			return SXERR_ABORT;` |
|        - |  9426 | `		}` |
|      ! 0 |  9427 | `		goto Synchronize;` |
|        - |  9428 | `	}` |
|       47 |  9429 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9430 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|       47 |  9431 | `	if( pCase == 0 ){` |
|      ! 0 |  9432 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9433 | `		return SXERR_ABORT;` |
|        - |  9434 | `	}` |
|       47 |  9435 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|       47 |  9436 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9437 | `		return SXERR_ABORT;` |
|        - |  9438 | `	}` |
|       47 |  9439 | `	pGen->pIn++; /* Jump the case name */` |
|       47 |  9440 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|       31 |  9441 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 |  9442 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 |  9443 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|        6 |  9444 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9445 | `				return SXERR_ABORT;` |
|        - |  9446 | `			}` |
|        6 |  9447 | `			goto Synchronize;` |
|        - |  9448 | `		}` |
|       25 |  9449 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - |  9450 | `		/* Compile the backing value expression into the case's own container` |
|        - |  9451 | `		 * (same technique as class constants). */` |
|       25 |  9452 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       25 |  9453 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|       25 |  9454 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       25 |  9455 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  9456 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9457 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|      ! 0 |  9458 | `		}` |
|       25 |  9459 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|       25 |  9460 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       25 |  9461 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9462 | `			return SXERR_ABORT;` |
|        - |  9463 | `		}` |
|       13 |  9464 | `	}else{` |
|       17 |  9465 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 |  9466 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9467 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|      ! 0 |  9468 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9469 | `				return SXERR_ABORT;` |
|        - |  9470 | `			}` |
|      ! 0 |  9471 | `			goto Synchronize;` |
|        - |  9472 | `		}` |
|        - |  9473 | `	}` |
|       41 |  9474 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|       41 |  9475 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9476 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9477 | `		return SXERR_ABORT;` |
|        - |  9478 | `	}` |
|       41 |  9479 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|       41 |  9480 | `	return SXRET_OK;` |
|        2 |  9481 | `Synchronize:` |
|        - |  9482 | `	/* Synchronize with the first semi-colon */` |
|       14 |  9483 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       10 |  9484 | `		pGen->pIn++;` |
|        2 |  9485 | `	}` |
|        6 |  9486 | `	return SXERR_CORRUPT;` |
|       26 |  9487 | `}` |
|        - |  9488 | `/*` |
|        - |  9489 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|        - |  9490 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|        - |  9491 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|        - |  9492 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|        - |  9493 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|        - |  9494 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|        - |  9495 | ` * pointers into it (see the constructor-promotion precedent above).` |
|        - |  9496 | ` */` |
|       24 |  9497 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        3 |  9498 | `{` |
|        - |  9499 | `	SyToken *pSaveIn,*pSaveEnd;` |
|        - |  9500 | `	const char *zBack;` |
|        - |  9501 | `	SySet sToken;` |
|        - |  9502 | `	char *zSrc;` |
|        - |  9503 | `	sxu32 nSrc,nMax;` |
|       27 |  9504 | `	sxi32 rc = SXRET_OK;` |
|       27 |  9505 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|       24 |  9506 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|       27 |  9507 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|       27 |  9508 | `	if( zSrc == 0 ){` |
|      ! 0 |  9509 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9510 | `		return SXERR_ABORT;` |
|        - |  9511 | `	}` |
|       27 |  9512 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|       27 |  9513 | `	if( pClass->nEnumBacking != 0 ){` |
|       19 |  9514 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        - |  9515 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|        - |  9516 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|        - |  9517 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|        6 |  9518 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|        7 |  9519 | `	}else{` |
|       21 |  9520 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        6 |  9521 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|        - |  9522 | `	}` |
|       27 |  9523 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       27 |  9524 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|       27 |  9525 | `	pSaveIn = pGen->pIn;` |
|       27 |  9526 | `	pSaveEnd = pGen->pEnd;` |
|       27 |  9527 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       27 |  9528 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       75 |  9529 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|       51 |  9530 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|        3 |  9531 | `	}` |
|       27 |  9532 | `	pGen->pIn = pSaveIn;` |
|       27 |  9533 | `	pGen->pEnd = pSaveEnd;` |
|       27 |  9534 | `	SySetRelease(&sToken);` |
|       27 |  9535 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|       15 |  9536 | `}` |
|        - |  9537 | `/*` |
|        - |  9538 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - |  9539 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - |  9540 | ` */` |
|        - |  9541 | `static const char *azEnumBannedMagic[] = {` |
|        - |  9542 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - |  9543 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - |  9544 | `};` |
|        - |  9545 | `/*` |
|        - |  9546 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - |  9547 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - |  9548 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - |  9549 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - |  9550 | ` * and before the class is installed.` |
|        - |  9551 | ` */` |
|       24 |  9552 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        3 |  9553 | `{` |
|        - |  9554 | `	SyHashEntry *pEntry;` |
|        - |  9555 | `	sxi32 rc;` |
|        - |  9556 | `	sxu32 n;` |
|        - |  9557 | `	/* php: "Enum %s cannot include properties" */` |
|       27 |  9558 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|       69 |  9559 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|       47 |  9560 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       47 |  9561 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 |  9562 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 |  9563 | `				"Enum %z cannot include properties",&pClass->sName);` |
|        3 |  9564 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9565 | `				return SXERR_ABORT;` |
|        - |  9566 | `			}` |
|        3 |  9567 | `			break;` |
|        - |  9568 | `		}` |
|        2 |  9569 | `	}` |
|        - |  9570 | `	/* php: "Enum %s cannot include magic method %s" */` |
|      339 |  9571 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|      468 |  9572 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|      315 |  9573 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 |  9574 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9575 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|      ! 0 |  9576 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9577 | `				return SXERR_ABORT;` |
|        - |  9578 | `			}` |
|      ! 0 |  9579 | `		}` |
|      159 |  9580 | `	}` |
|        - |  9581 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - |  9582 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - |  9583 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - |  9584 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - |  9585 | `	{` |
|        - |  9586 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - |  9587 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - |  9588 | `		ph7_class_attr *pAttr;` |
|       27 |  9589 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9590 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|       27 |  9591 | `		if( pAttr == 0 ){` |
|      ! 0 |  9592 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9593 | `			return SXERR_ABORT;` |
|        - |  9594 | `		}` |
|       27 |  9595 | `		pAttr->nType = MEMOBJ_STRING;` |
|       27 |  9596 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|       27 |  9597 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|       27 |  9598 | `		if( pClass->nEnumBacking != 0 ){` |
|       13 |  9599 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9600 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|       13 |  9601 | `			if( pAttr == 0 ){` |
|      ! 0 |  9602 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9603 | `				return SXERR_ABORT;` |
|        - |  9604 | `			}` |
|       13 |  9605 | `			pAttr->nType = pClass->nEnumBacking;` |
|       13 |  9606 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 |  9607 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|        4 |  9608 | `			}else{` |
|        7 |  9609 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - |  9610 | `			}` |
|       13 |  9611 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|        6 |  9612 | `		}` |
|        - |  9613 | `	}` |
|       27 |  9614 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|       15 |  9615 | `}` |
|        - |  9616 | `/*` |
|        - |  9617 | ` * Compile a class declaration, named or anonymous.` |
|        - |  9618 | ` *` |
|        - |  9619 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - |  9620 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - |  9621 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - |  9622 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - |  9623 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - |  9624 | ` * implements, body, install) is shared by both paths.` |
|        - |  9625 | ` */` |
|   215232 |  9626 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - |  9627 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 |  9628 | `{` |
|   215237 |  9629 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9630 | `	ph7_class *pClass,*pBase;` |
|        - |  9631 | `	SyToken *pEnd,*pTmp;` |
|        - |  9632 | `	sxi32 iProtection;` |
|        - |  9633 | `	SySet aInterfaces;` |
|        - |  9634 | `	SySet aUseEntries;` |
|        - |  9635 | `	sxi32 iAttrflags;` |
|        - |  9636 | `	SyString *pName;` |
|        - |  9637 | `	sxi32 nKwrd;` |
|        - |  9638 | `	sxi32 rc;` |
|        - |  9639 | `	/* Jump the 'class' keyword */` |
|   215237 |  9640 | `	pGen->pIn++;` |
|   215237 |  9641 | `	if( pAnonName ){` |
|        - |  9642 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - |  9643 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - |  9644 | `		 * then use the synthesized name. */` |
|       30 |  9645 | `		*ppArgStart = *ppArgEnd = 0;` |
|       30 |  9646 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 |  9647 | `			pGen->pIn++; /* Jump '(' */` |
|        7 |  9648 | `			*ppArgStart = pGen->pIn;` |
|       10 |  9649 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 |  9650 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 |  9651 | `			pGen->pIn = *ppArgEnd;` |
|        7 |  9652 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 |  9653 | `		}` |
|       30 |  9654 | `		pName = pAnonName;` |
|       30 |  9655 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       17 |  9656 | `	}else{` |
|   215211 |  9657 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  9658 | `			/* Syntax error */` |
|      ! 0 |  9659 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 |  9660 | `			if( rc == SXERR_ABORT ){` |
|        - |  9661 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9662 | `				return SXERR_ABORT;` |
|        - |  9663 | `			}` |
|        - |  9664 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 |  9665 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 |  9666 | `				pGen->pIn++;` |
|      ! 0 |  9667 | `			}` |
|      ! 0 |  9668 | `			return SXRET_OK;` |
|        - |  9669 | `		}` |
|        - |  9670 | `		/* Extract class name */` |
|   215211 |  9671 | `		pName = &pGen->pIn->sData;` |
|        - |  9672 | `		/* Advance the stream cursor */` |
|   215211 |  9673 | `		pGen->pIn++;` |
|        - |  9674 | `		/* Build FQN and obtain a raw class */ {` |
|        - |  9675 | `			SyBlob sFQN;` |
|        - |  9676 | `			SyString sFQNStr;` |
|   215211 |  9677 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   215211 |  9678 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   215211 |  9679 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   215211 |  9680 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   215211 |  9681 | `			SyBlobRelease(&sFQN);` |
|        - |  9682 | `		}` |
|        - |  9683 | `	}` |
|   215237 |  9684 | `	if( pClass == 0 ){` |
|      ! 0 |  9685 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9686 | `		return SXERR_ABORT;` |
|        - |  9687 | `	}` |
|   215232 |  9688 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|       33 |  9689 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - |  9690 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|       16 |  9691 | `		pGen->pIn++; /* Jump ':' */` |
|       14 |  9692 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       16 |  9693 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        7 |  9694 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        7 |  9695 | `			pGen->pIn++;` |
|       12 |  9696 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       10 |  9697 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|        7 |  9698 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|        7 |  9699 | `			pGen->pIn++;` |
|        4 |  9700 | `		}else{` |
|        3 |  9701 | `			SyToken *pTok = pGen->pIn;` |
|        3 |  9702 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 |  9703 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 |  9704 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 |  9705 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9706 | `				return SXERR_ABORT;` |
|        - |  9707 | `			}` |
|        3 |  9708 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 |  9709 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 |  9710 | `			}` |
|        - |  9711 | `		}` |
|        7 |  9712 | `	}` |
|   215237 |  9713 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   215237 |  9714 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9715 | `		return SXERR_ABORT;` |
|        - |  9716 | `	}` |
|        - |  9717 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   215237 |  9718 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   215237 |  9719 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - |  9720 | `	/* Assume a standalone class */` |
|   215237 |  9721 | `	pBase = 0;` |
|   215237 |  9722 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   171291 |  9723 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   171291 |  9724 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - |  9725 | `			SyBlob sResolved;` |
|        - |  9726 | `			SyString sBaseName;` |
|        - |  9727 | `			sxu32 nRefLine;` |
|   124511 |  9728 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - |  9729 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 |  9730 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9731 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|      ! 0 |  9732 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9733 | `					return SXERR_ABORT;` |
|        - |  9734 | `				}` |
|      ! 0 |  9735 | `			}` |
|   124511 |  9736 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   124511 |  9737 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   124511 |  9738 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   124511 |  9739 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 |  9740 | `				SyBlobRelease(&sResolved);` |
|        4 |  9741 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9742 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 |  9743 | `					pName);` |
|        3 |  9744 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 |  9745 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9746 | `					return SXERR_ABORT;` |
|        - |  9747 | `				}` |
|        3 |  9748 | `				return SXRET_OK;` |
|        - |  9749 | `			}` |
|   186761 |  9750 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   124504 |  9751 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   124509 |  9752 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  9753 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9754 | `			/* Interfaces are not allowed */` |
|   124509 |  9755 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 |  9756 | `				pBase = pBase->pNextName;` |
|      ! 0 |  9757 | `			}` |
|   124509 |  9758 | `			if( pBase == 0 ){` |
|      ! 0 |  9759 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9760 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 |  9761 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9762 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9763 | `					return SXERR_ABORT;` |
|        - |  9764 | `				}` |
|      ! 0 |  9765 | `			}else{` |
|   124509 |  9766 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 |  9767 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  9768 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|        3 |  9769 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9770 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9771 | `						return SXERR_ABORT;` |
|        - |  9772 | `					}` |
|        3 |  9773 | `					pBase = 0; /* Never inherit from an enum */` |
|   124508 |  9774 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 |  9775 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9776 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 |  9777 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9778 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9779 | `						return SXERR_ABORT;` |
|        - |  9780 | `					}` |
|      ! 0 |  9781 | `				}` |
|        - |  9782 | `			}` |
|   124509 |  9783 | `			SyBlobRelease(&sResolved);` |
|   124509 |  9784 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 |  9785 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 |  9786 | `			}` |
|    62252 |  9787 | `		}` |
|   171289 |  9788 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - |  9789 | `			ph7_class *pInterface;` |
|        - |  9790 | `			/* Interface implementation */` |
|    46797 |  9791 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    23408 |  9792 | `			for(;;){` |
|        - |  9793 | `				SyBlob sResolved;` |
|        - |  9794 | `				SyString sIntName;` |
|        - |  9795 | `				sxu32 nRefLine;` |
|    46809 |  9796 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    46809 |  9797 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    46809 |  9798 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  9799 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9800 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9801 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 |  9802 | `						pName);` |
|      ! 0 |  9803 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9804 | `						return SXERR_ABORT;` |
|        - |  9805 | `					}` |
|      ! 0 |  9806 | `					break;` |
|        - |  9807 | `				}` |
|    93613 |  9808 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    46804 |  9809 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    46809 |  9810 | `				SyStringInitFromBuf(&sIntName,` |
|        - |  9811 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9812 | `				/* Only interfaces are allowed */` |
|    46809 |  9813 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  9814 | `					pInterface = pInterface->pNextName;` |
|      ! 0 |  9815 | `				}` |
|    46809 |  9816 | `				if( pInterface == 0 ){` |
|      ! 0 |  9817 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9818 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 |  9819 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9820 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9821 | `						return SXERR_ABORT;` |
|        - |  9822 | `					}` |
|      ! 0 |  9823 | `				}else{` |
|        - |  9824 | `					/* Reject user classes that try to implement Throwable` |
|        - |  9825 | `					 * directly (or via an interface that extends Throwable)` |
|        - |  9826 | `					 * unless they already extend Exception or Error.` |
|        - |  9827 | `					 * Exception and Error themselves are compiled from the` |
|        - |  9828 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - |  9829 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    46809 |  9830 | `					SyString *pFqn = &pClass->sName;` |
|    46809 |  9831 | `					int bIsExceptionOrError =` |
|    27290 |  9832 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    72152 |  9833 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    44869 |  9834 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3898 |  9835 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    50693 |  9836 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11670 |  9837 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3887 |  9838 | `						!bIsExceptionOrError ){` |
|       12 |  9839 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9840 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 |  9841 | `							&pClass->sName);` |
|        9 |  9842 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9843 | `							SyBlobRelease(&sResolved);` |
|      ! 0 |  9844 | `							return SXERR_ABORT;` |
|        - |  9845 | `						}` |
|        - |  9846 | `						/* Skip registration so the follow-up abstract-method` |
|        - |  9847 | `						 * check does not produce a duplicate fatal. */` |
|        6 |  9848 | `					}else{` |
|    46803 |  9849 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - |  9850 | `					}` |
|        - |  9851 | `				}` |
|    46809 |  9852 | `				SyBlobRelease(&sResolved);` |
|    46809 |  9853 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    23401 |  9854 | `					break;` |
|        - |  9855 | `				}` |
|       16 |  9856 | `				pGen->pIn++;/* Jump the comma */` |
|        4 |  9857 | `			}` |
|    23396 |  9858 | `		}` |
|    85642 |  9859 | `	}` |
|   215235 |  9860 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  9861 | `		/* Syntax error */` |
|      ! 0 |  9862 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 |  9863 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9864 | `		if( rc == SXERR_ABORT ){` |
|        - |  9865 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9866 | `			return SXERR_ABORT;` |
|        - |  9867 | `		}` |
|      ! 0 |  9868 | `		return SXRET_OK;` |
|        - |  9869 | `	}` |
|   215235 |  9870 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   215235 |  9871 | `	pEnd = 0; /* cc warning */` |
|        - |  9872 | `	/* Delimit the class body */` |
|   215235 |  9873 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   215235 |  9874 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  9875 | `		/* Syntax error */` |
|      ! 0 |  9876 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 |  9877 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9878 | `		if( rc == SXERR_ABORT ){` |
|        - |  9879 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9880 | `			return SXERR_ABORT;` |
|        - |  9881 | `		}` |
|      ! 0 |  9882 | `		return SXRET_OK;` |
|        - |  9883 | `	}` |
|        - |  9884 | `	/* The delimiter token is the class body's closing brace */` |
|   215235 |  9885 | `	pClass->nEndLine = pEnd->nLine;` |
|        - |  9886 | `	/* Swap token stream */` |
|   215235 |  9887 | `	pTmp = pGen->pEnd;` |
|   215235 |  9888 | `	pGen->pEnd = pEnd;` |
|        - |  9889 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   215235 |  9890 | `	pClass->iFlags \|= iFlags;` |
|        - |  9891 | `	/* Start the parse process */` |
|   823011 |  9892 | `	for(;;){` |
|        - |  9893 | `		/* Jump leading/trailing semi-colons */` |
|  2211147 |  9894 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   354493 |  9895 | `			pGen->pIn++;` |
|        5 |  9896 | `		}` |
|  1856659 |  9897 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  9898 | `			/* End of class body */` |
|   215193 |  9899 | `			break;` |
|        - |  9900 | `		}` |
|        - |  9901 | `		/* Bind a directly-preceding docblock to this member */` |
|  1641471 |  9902 | `		GenStateSetPendingDoc(&(*pGen));` |
|  1641466 |  9903 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   820738 |  9904 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 |  9905 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9906 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 |  9907 | `				&pGen->pIn->sData,pName);` |
|      ! 0 |  9908 | `			if( rc == SXERR_ABORT ){` |
|        - |  9909 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9910 | `				return SXERR_ABORT;` |
|        - |  9911 | `			}` |
|      ! 0 |  9912 | `			goto done;` |
|        - |  9913 | `		}` |
|        - |  9914 | `		/* Assume public visibility */` |
|  1641471 |  9915 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  1641471 |  9916 | `		iAttrflags = 0;` |
|        - |  9917 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - |  9918 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - |  9919 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - |  9920 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  1641471 |  9921 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 |  9922 | `			int bMod = 0;` |
|      ! 0 |  9923 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 |  9924 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - |  9925 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - |  9926 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - |  9927 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - |  9928 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 |  9929 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 |  9930 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  9931 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 |  9932 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 |  9933 | `			}` |
|      ! 0 |  9934 | `			if( !bMod ){` |
|      ! 0 |  9935 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 |  9936 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  9937 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9938 | `						return SXERR_ABORT;` |
|        - |  9939 | `					}` |
|      ! 0 |  9940 | `					goto done;` |
|        - |  9941 | `				}` |
|      ! 0 |  9942 | `				continue;` |
|        - |  9943 | `			}` |
|      ! 0 |  9944 | `		}` |
|  1641471 |  9945 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - |  9946 | `			/* Extract the current keyword */` |
|  1641471 |  9947 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1641471 |  9948 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - |  9949 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|       47 |  9950 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|       47 |  9951 | `				if( rc != SXRET_OK ){` |
|        6 |  9952 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9953 | `						return SXERR_ABORT;` |
|        - |  9954 | `					}` |
|        6 |  9955 | `					goto done;` |
|        - |  9956 | `				}` |
|       41 |  9957 | `				continue;` |
|        - |  9958 | `			}` |
|  1641429 |  9959 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - |  9960 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - |  9961 | `				TraitUseEntry sUse;` |
|       63 |  9962 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       63 |  9963 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|       63 |  9964 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|       37 |  9965 | `				for(;;){` |
|        - |  9966 | `					ph7_class *pTrait;` |
|        - |  9967 | `					SyString *pTraitName;` |
|       71 |  9968 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  9969 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9970 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 |  9971 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9972 | `							return SXERR_ABORT;` |
|        - |  9973 | `						}` |
|      ! 0 |  9974 | `						break;` |
|        - |  9975 | `					}` |
|       71 |  9976 | `					pTraitName = &pGen->pIn->sData;` |
|        - |  9977 | `					/* Resolve trait name through namespace/imports */ {` |
|        - |  9978 | `						SyBlob sResolved;` |
|       71 |  9979 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       71 |  9980 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      137 |  9981 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|       66 |  9982 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       71 |  9983 | `						SyBlobRelease(&sResolved);` |
|        - |  9984 | `					}` |
|        - |  9985 | `					/* Only traits are allowed */` |
|       71 |  9986 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 |  9987 | `						pTrait = pTrait->pNextName;` |
|      ! 0 |  9988 | `					}` |
|       71 |  9989 | `					if( pTrait == 0 ){` |
|      ! 0 |  9990 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9991 | `							"'%z' is not a trait",pTraitName);` |
|      ! 0 |  9992 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9993 | `							return SXERR_ABORT;` |
|        - |  9994 | `						}` |
|      ! 0 |  9995 | `					}else{` |
|       71 |  9996 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - |  9997 | `					}` |
|       71 |  9998 | `					pGen->pIn++; /* Advance past trait name */` |
|       71 |  9999 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       34 | 10000 | `						break;` |
|        - | 10001 | `					}` |
|       10 | 10002 | `					pGen->pIn++; /* Jump the comma */` |
|        2 | 10003 | `				}` |
|        - | 10004 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|       63 | 10005 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 10006 | `					SyToken *pBlock;` |
|       13 | 10007 | `					pGen->pIn++; /* Jump '{' */` |
|       13 | 10008 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       13 | 10009 | `					sUse.pResolvStart = pGen->pIn;` |
|       13 | 10010 | `					sUse.pResolvEnd = pBlock;` |
|       13 | 10011 | `					if( pBlock < pGen->pEnd ){` |
|       13 | 10012 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        8 | 10013 | `					}else{` |
|      ! 0 | 10014 | `						pGen->pIn = pGen->pEnd;` |
|        - | 10015 | `					}` |
|        5 | 10016 | `				}` |
|       63 | 10017 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 10018 | `				/* The semicolon will be consumed by the outer loop */` |
|       63 | 10019 | `				continue;` |
|        - | 10020 | `			}` |
|  1641371 | 10021 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  1497219 | 10022 | `				iProtection = nKwrd;` |
|  1497219 | 10023 | `				pGen->pIn++; /* Jump the visibility token */` |
|        - | 10024 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  1497219 | 10025 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       22 | 10026 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       22 | 10027 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        9 | 10028 | `				}` |
|  1497214 | 10029 | `				if( pGen->pIn >= pGen->pEnd` |
|  1497219 | 10030 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10031 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10032 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 10033 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10034 | `					if( rc == SXERR_ABORT ){` |
|        - | 10035 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 | 10036 | `						return SXERR_ABORT;` |
|        - | 10037 | `					}` |
|      ! 0 | 10038 | `					goto done;` |
|        - | 10039 | `				}` |
|  1497219 | 10040 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10041 | `					/* Attribute declaration (untyped) */` |
|   210327 | 10042 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   210327 | 10043 | `					if( rc != SXRET_OK ){` |
|       11 | 10044 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10045 | `							return SXERR_ABORT;` |
|        - | 10046 | `						}` |
|       11 | 10047 | `						goto done;` |
|        - | 10048 | `					}` |
|   210319 | 10049 | `					continue;` |
|        - | 10050 | `				}` |
|  1286897 | 10051 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10052 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|      187 | 10053 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      187 | 10054 | `					if( rc != SXRET_OK ){` |
|        8 | 10055 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10056 | `							return SXERR_ABORT;` |
|        - | 10057 | `						}` |
|        8 | 10058 | `						goto done;` |
|        - | 10059 | `					}` |
|      181 | 10060 | `					continue;` |
|        - | 10061 | `				}` |
|        - | 10062 | `				/* Extract the keyword */` |
|  1286715 | 10063 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   643355 | 10064 | `			}` |
|  1430867 | 10065 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 10066 | `				/* Process constant declaration */` |
|   143861 | 10067 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   143861 | 10068 | `				if( rc != SXRET_OK ){` |
|       11 | 10069 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10070 | `						return SXERR_ABORT;` |
|        - | 10071 | `					}` |
|       11 | 10072 | `					goto done;` |
|        - | 10073 | `				}` |
|    71929 | 10074 | `			}else{` |
|  1287011 | 10075 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 10076 | `					/* Static method or attribute,record that */` |
|    23437 | 10077 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    23437 | 10078 | `					pGen->pIn++; /* Jump the static keyword */` |
|    23437 | 10079 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10080 | `						/* Extract the keyword */` |
|    23409 | 10081 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    23409 | 10082 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 10083 | `							iProtection = nKwrd;` |
|      ! 0 | 10084 | `							pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 | 10085 | `						}` |
|    11702 | 10086 | `					}` |
|        - | 10087 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - | 10088 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - | 10089 | `					 * than a generic "expecting method" parse error. */` |
|    23437 | 10090 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 10091 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 10092 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 | 10093 | `					}` |
|    23432 | 10094 | `					if( pGen->pIn >= pGen->pEnd` |
|    23437 | 10095 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10096 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10097 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 | 10098 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10099 | `						if( rc == SXERR_ABORT ){` |
|        - | 10100 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10101 | `							return SXERR_ABORT;` |
|        - | 10102 | `						}` |
|      ! 0 | 10103 | `						goto done;` |
|        - | 10104 | `					}` |
|    23437 | 10105 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10106 | `						/* Attribute declaration */` |
|       29 | 10107 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       29 | 10108 | `						if( rc != SXRET_OK ){` |
|        3 | 10109 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10110 | `								return SXERR_ABORT;` |
|        - | 10111 | `							}` |
|        3 | 10112 | `							goto done;` |
|        - | 10113 | `						}` |
|       26 | 10114 | `						continue;` |
|        - | 10115 | `					}` |
|    23411 | 10116 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10117 | `						/* Typed static attribute declaration */` |
|       15 | 10118 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       15 | 10119 | `						if( rc != SXRET_OK ){` |
|        3 | 10120 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10121 | `								return SXERR_ABORT;` |
|        - | 10122 | `							}` |
|        3 | 10123 | `							goto done;` |
|        - | 10124 | `						}` |
|       13 | 10125 | `						continue;` |
|        - | 10126 | `					}` |
|        - | 10127 | `					/* Extract the keyword */` |
|    23399 | 10128 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1275276 | 10129 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - | 10130 | `					/* Abstract method,record that */` |
|       15 | 10131 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - | 10132 | `					/* Mark the whole class as abstract */` |
|       15 | 10133 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|        - | 10134 | `					/* Advance the stream cursor */` |
|       15 | 10135 | `					pGen->pIn++;` |
|       15 | 10136 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       15 | 10137 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       15 | 10138 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       13 | 10139 | `							iProtection = nKwrd;` |
|       13 | 10140 | `							pGen->pIn++; /* Jump the visibility token */` |
|        5 | 10141 | `						}` |
|        6 | 10142 | `					}` |
|       15 | 10143 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       12 | 10144 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10145 | `							/* Static method */` |
|      ! 0 | 10146 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10147 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10148 | `					}` |
|       15 | 10149 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       12 | 10150 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10151 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10152 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 | 10153 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10154 | `							if( rc == SXERR_ABORT ){` |
|        - | 10155 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10156 | `								return SXERR_ABORT;` |
|        - | 10157 | `							}` |
|      ! 0 | 10158 | `							goto done;` |
|        - | 10159 | `					}` |
|       15 | 10160 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  1263573 | 10161 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - | 10162 | `					/* final method ,record that */` |
|       21 | 10163 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       21 | 10164 | `					pGen->pIn++; /* Jump the final keyword */` |
|       21 | 10165 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10166 | `						/* Extract the keyword */` |
|       21 | 10167 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 | 10168 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       11 | 10169 | `							iProtection = nKwrd;` |
|       11 | 10170 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 | 10171 | `						}` |
|        9 | 10172 | `					}` |
|       21 | 10173 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 10174 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - | 10175 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - | 10176 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - | 10177 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 | 10178 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 | 10179 | `							if( rc != SXRET_OK ){` |
|      ! 0 | 10180 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 | 10181 | `									return SXERR_ABORT;` |
|        - | 10182 | `								}` |
|      ! 0 | 10183 | `								goto done;` |
|        - | 10184 | `							}` |
|       14 | 10185 | `							continue;` |
|        - | 10186 | `					}` |
|        9 | 10187 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 | 10188 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10189 | `							/* Static method */` |
|      ! 0 | 10190 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10191 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10192 | `					}` |
|        9 | 10193 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 10194 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10195 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10196 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 | 10197 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10198 | `							if( rc == SXERR_ABORT ){` |
|        - | 10199 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10200 | `								return SXERR_ABORT;` |
|        - | 10201 | `							}` |
|      ! 0 | 10202 | `							goto done;` |
|        - | 10203 | `					}` |
|        9 | 10204 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 10205 | `				}` |
|  1286961 | 10206 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 10207 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10208 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 | 10209 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10210 | `						if( rc == SXERR_ABORT ){` |
|        - | 10211 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10212 | `							return SXERR_ABORT;` |
|        - | 10213 | `						}` |
|      ! 0 | 10214 | `						goto done;` |
|        - | 10215 | `				}` |
|  1286961 | 10216 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 | 10217 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 10218 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 | 10219 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10220 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 10221 | `						if( rc == SXERR_ABORT ){` |
|        - | 10222 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10223 | `							return SXERR_ABORT;` |
|        - | 10224 | `						}` |
|      ! 0 | 10225 | `						goto done;` |
|        - | 10226 | `					}` |
|        - | 10227 | `					/* Attribute declaration */` |
|        7 | 10228 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 | 10229 | `				}else{` |
|        - | 10230 | `					/* Process method declaration */` |
|  1286955 | 10231 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 10232 | `				}` |
|  1286961 | 10233 | `				if( rc != SXRET_OK ){` |
|       16 | 10234 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10235 | `						return SXERR_ABORT;` |
|        - | 10236 | `					}` |
|       16 | 10237 | `					goto done;` |
|        - | 10238 | `				}` |
|        - | 10239 | `			}` |
|   715401 | 10240 | `		}else{` |
|        - | 10241 | `			/* Attribute declaration */` |
|      ! 0 | 10242 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10243 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10244 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10245 | `					return SXERR_ABORT;` |
|        - | 10246 | `				}` |
|      ! 0 | 10247 | `				goto done;` |
|        - | 10248 | `			}` |
|        - | 10249 | `		}` |
|        5 | 10250 | `	}` |
|        - | 10251 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 10252 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 10253 | `	 */` |
|        - | 10254 | `	{` |
|        - | 10255 | `		TraitUseEntry *apUse;` |
|        - | 10256 | `		sxu32 nU;` |
|   215193 | 10257 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   215251 | 10258 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|       63 | 10259 | `			TraitUseEntry *pUse = &apUse[nU];` |
|       63 | 10260 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|       63 | 10261 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|       63 | 10262 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 10263 | `			sxu32 nT;` |
|       63 | 10264 | `			if( !hasResolution ){` |
|        - | 10265 | `				/* No conflict resolution block: use standard trait application */` |
|      107 | 10266 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       59 | 10267 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|       59 | 10268 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10269 | `						break;` |
|        - | 10270 | `					}` |
|       32 | 10271 | `				}` |
|       29 | 10272 | `			}else{` |
|        - | 10273 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 10274 | `				 * then use the block to resolve method conflicts.` |
|        - | 10275 | `				 */` |
|        - | 10276 | `				SyToken *pR;` |
|       25 | 10277 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       15 | 10278 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 10279 | `					ph7_class_attr *pAR;` |
|        - | 10280 | `					SyHashEntry *pER;` |
|        - | 10281 | `					SyString *pNR;` |
|       15 | 10282 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       21 | 10283 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 | 10284 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 10285 | `						pNR = &pAR->sName;` |
|      ! 0 | 10286 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 10287 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 10288 | `						}` |
|      ! 0 | 10289 | `					}` |
|       15 | 10290 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        9 | 10291 | `				}` |
|        - | 10292 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       13 | 10293 | `				pR = pUse->pResolvStart;` |
|       27 | 10294 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 10295 | `					SyString sTrait,sMethod;` |
|        - | 10296 | `					ph7_class *pSrcTrait;` |
|        - | 10297 | `					ph7_class_method *pMeth;` |
|        - | 10298 | `					sxi32 nRKwrd;` |
|       41 | 10299 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 10300 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 10301 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 10302 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 10303 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 10304 | `					sMethod = pR->sData;` |
|       17 | 10305 | `					pR++;` |
|       17 | 10306 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 10307 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 10308 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 10309 | `							sTrait = sMethod;` |
|        7 | 10310 | `							pR++;` |
|        7 | 10311 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 10312 | `							sMethod = pR->sData;` |
|        7 | 10313 | `							pR++;` |
|        3 | 10314 | `						}` |
|        3 | 10315 | `					}` |
|       17 | 10316 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 10317 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 10318 | `						continue;` |
|        - | 10319 | `					}` |
|       17 | 10320 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 10321 | `					pR++;` |
|       17 | 10322 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 10323 | `						pSrcTrait = 0;` |
|        7 | 10324 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 10325 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 10326 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 10327 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 10328 | `								pSrcTrait = apTrait[nT];` |
|        5 | 10329 | `								break;` |
|        - | 10330 | `							}` |
|        2 | 10331 | `						}` |
|        5 | 10332 | `						if( pSrcTrait ){` |
|        5 | 10333 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 10334 | `							if( pMeth ){` |
|        5 | 10335 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 10336 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 10337 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 10338 | `								}` |
|        2 | 10339 | `							}` |
|        2 | 10340 | `						}` |
|        2 | 10341 | `					}` |
|       35 | 10342 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 10343 | `				}` |
|        - | 10344 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       25 | 10345 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 10346 | `					ph7_class_method *pMR;` |
|        - | 10347 | `					SyHashEntry *pER;` |
|        - | 10348 | `					SyString *pNR;` |
|       15 | 10349 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       41 | 10350 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       23 | 10351 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       23 | 10352 | `						pNR = &pMR->sFunc.sName;` |
|       23 | 10353 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       14 | 10354 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 10355 | `						}` |
|        3 | 10356 | `					}` |
|        9 | 10357 | `				}` |
|        - | 10358 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       13 | 10359 | `				pR = pUse->pResolvStart;` |
|       27 | 10360 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 10361 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 10362 | `					ph7_class *pSrcTrait;` |
|        - | 10363 | `					ph7_class_method *pMeth;` |
|       27 | 10364 | `					int hasQual = 0;` |
|        - | 10365 | `					sxi32 nRKwrd;` |
|       41 | 10366 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 10367 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 10368 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 10369 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 10370 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       17 | 10371 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 10372 | `					sMethod = pR->sData;` |
|       17 | 10373 | `					pR++;` |
|       17 | 10374 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 10375 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 10376 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 10377 | `							sTrait = sMethod;` |
|        7 | 10378 | `							hasQual = 1;` |
|        7 | 10379 | `							pR++;` |
|        7 | 10380 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 10381 | `							sMethod = pR->sData;` |
|        7 | 10382 | `							pR++;` |
|        3 | 10383 | `						}` |
|        3 | 10384 | `					}` |
|       17 | 10385 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 10386 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 10387 | `						continue;` |
|        - | 10388 | `					}` |
|       17 | 10389 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 10390 | `					pR++;` |
|       17 | 10391 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       13 | 10392 | `						sxi32 iNewVis = -1;` |
|       13 | 10393 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 10394 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 10395 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 10396 | `								iNewVis = nAK;` |
|        7 | 10397 | `								pR++;` |
|        3 | 10398 | `							}` |
|        3 | 10399 | `						}` |
|       13 | 10400 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       11 | 10401 | `							sAlias = pR->sData;` |
|       11 | 10402 | `							pR++;` |
|        4 | 10403 | `						}` |
|       13 | 10404 | `						pMeth = 0;` |
|       13 | 10405 | `						if( hasQual ){` |
|        3 | 10406 | `							pSrcTrait = 0;` |
|        5 | 10407 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 10408 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 10409 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 10410 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 10411 | `									pSrcTrait = apTrait[nT];` |
|        3 | 10412 | `									break;` |
|        - | 10413 | `								}` |
|        2 | 10414 | `							}` |
|        3 | 10415 | `							if( pSrcTrait ){` |
|        3 | 10416 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 10417 | `							}` |
|        2 | 10418 | `						}else{` |
|       10 | 10419 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 10420 | `						}` |
|       13 | 10421 | `						if( pMeth ){` |
|       13 | 10422 | `							if( sAlias.nByte > 0 ){` |
|        - | 10423 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 10424 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 10425 | `								 */` |
|        - | 10426 | `								ph7_class_method *pAlias;` |
|        - | 10427 | `								char *zAliasDup;` |
|       11 | 10428 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       11 | 10429 | `								if( pAlias ){` |
|       11 | 10430 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       11 | 10431 | `									if( iNewVis >= 0 ){` |
|        5 | 10432 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 10433 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 10434 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 10435 | `									}` |
|       11 | 10436 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       11 | 10437 | `									if( zAliasDup ){` |
|       11 | 10438 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 10439 | `									}` |
|        7 | 10440 | `								}` |
|        7 | 10441 | `							}else if( iNewVis >= 0 ){` |
|        - | 10442 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 10443 | `								ph7_class_method *pCopy;` |
|        3 | 10444 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 10445 | `								if( pCopy ){` |
|        3 | 10446 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 10447 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 10448 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 10449 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 10450 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 10451 | `									/* Replace the method in the class hash */` |
|        3 | 10452 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 10453 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 10454 | `								}` |
|        1 | 10455 | `							}` |
|        5 | 10456 | `						}` |
|        5 | 10457 | `						SXUNUSED(hasQual);` |
|        5 | 10458 | `					}` |
|       21 | 10459 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 10460 | `				}` |
|        - | 10461 | `			}` |
|       63 | 10462 | `			SySetRelease(&pUse->aTraits);` |
|       34 | 10463 | `		}` |
|        - | 10464 | `	}` |
|   215193 | 10465 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 10466 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 10467 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|       27 | 10468 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|       27 | 10469 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10470 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 10471 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 10472 | `			return SXERR_ABORT;` |
|        - | 10473 | `		}` |
|       12 | 10474 | `	}` |
|        - | 10475 | `	/* Install the class */` |
|   215193 | 10476 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   215193 | 10477 | `	if( rc == SXRET_OK ){` |
|        - | 10478 | `		ph7_class **apInterface;` |
|        - | 10479 | `		sxu32 n;` |
|   215193 | 10480 | `		if( pBase ){` |
|        - | 10481 | `			/* Inherit from base class and mark as a subclass */` |
|   124507 | 10482 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    62251 | 10483 | `		}` |
|   215193 | 10484 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   261991 | 10485 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 10486 | `			/* Implements one or more interface */` |
|    46803 | 10487 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    46803 | 10488 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10489 | `				break;` |
|        - | 10490 | `			}` |
|    23404 | 10491 | `		}` |
|        - | 10492 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 10493 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|   215193 | 10494 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|       27 | 10495 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|       27 | 10496 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10497 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 10498 | `			}` |
|       27 | 10499 | `			if( pIntf ){` |
|       27 | 10500 | `				PH7_ClassImplement(pClass,pIntf);` |
|       12 | 10501 | `			}` |
|       27 | 10502 | `			if( pClass->nEnumBacking != 0 ){` |
|       13 | 10503 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|       13 | 10504 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10505 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 10506 | `				}` |
|       13 | 10507 | `				if( pIntf ){` |
|       13 | 10508 | `					PH7_ClassImplement(pClass,pIntf);` |
|        6 | 10509 | `				}` |
|        6 | 10510 | `			}` |
|       12 | 10511 | `		}` |
|        - | 10512 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 10513 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   215188 | 10514 | `		if( rc == SXRET_OK` |
|   215188 | 10515 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   215193 | 10516 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   171003 | 10517 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 10518 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   171003 | 10519 | `			if( pStringable ){` |
|   171003 | 10520 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   171003 | 10521 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 10522 | `				sxu32 i;` |
|   171003 | 10523 | `				int bAlready = 0;` |
|   209847 | 10524 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    42735 | 10525 | `					if( apImpl[i] == pStringable ){` |
|     3891 | 10526 | `						bAlready = 1;` |
|     3891 | 10527 | `						break;` |
|        - | 10528 | `					}` |
|    19427 | 10529 | `				}` |
|   171003 | 10530 | `				if( !bAlready ){` |
|   167117 | 10531 | `					PH7_ClassImplement(pClass,pStringable);` |
|    83556 | 10532 | `				}` |
|    85499 | 10533 | `			}` |
|    85499 | 10534 | `		}` |
|        - | 10535 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   215193 | 10536 | `		if( rc == SXRET_OK ){` |
|   215193 | 10537 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   215193 | 10538 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 10539 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 10540 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 10541 | `				return SXERR_ABORT;` |
|        - | 10542 | `			}` |
|   107594 | 10543 | `		}` |
|        - | 10544 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   215193 | 10545 | `		if( rc == SXRET_OK ){` |
|   215193 | 10546 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   215193 | 10547 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 10548 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 10549 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 10550 | `				return SXERR_ABORT;` |
|        - | 10551 | `			}` |
|   107594 | 10552 | `		}` |
|   107594 | 10553 | `	}` |
|   215193 | 10554 | `	SySetRelease(&aUseEntries);` |
|   215193 | 10555 | `	SySetRelease(&aInterfaces);` |
|   215193 | 10556 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10557 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10558 | `		return SXERR_ABORT;` |
|        - | 10559 | `	}` |
|   107594 | 10560 | `done:` |
|        - | 10561 | `	/* Point beyond the class body */` |
|   215235 | 10562 | `	pGen->pIn = &pEnd[1];` |
|   215235 | 10563 | `	pGen->pEnd = pTmp;` |
|   215235 | 10564 | `	return PH7_OK;` |
|   107621 | 10565 | `}` |
|        - | 10566 | `/* Compile a named class declaration (the common case). */` |
|   215206 | 10567 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 10568 | `{` |
|   215211 | 10569 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 10570 | `}` |
|        - | 10571 | `/*` |
|        - | 10572 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 10573 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 10574 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 10575 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 10576 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 10577 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 10578 | ` */` |
|       26 | 10579 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 10580 | `{` |
|        - | 10581 | `	char zName[128];         /* Synthesized class name */` |
|        - | 10582 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 10583 | `	SyString sName;` |
|        - | 10584 | `	SyToken *pArgStart,*pArgEnd;` |
|        - | 10585 | `	ph7_value *pObj;` |
|       30 | 10586 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10587 | `	sxu32 nIdx,nLen;` |
|        - | 10588 | `	sxi32 nArg,rc;` |
|       13 | 10589 | `	SXUNUSED(iCompileFlag);` |
|        - | 10590 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       30 | 10591 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       30 | 10592 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 10593 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 10594 | `	}` |
|       30 | 10595 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 10596 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 10597 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 10598 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       30 | 10599 | `	pArgStart = pArgEnd = 0;` |
|       30 | 10600 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       30 | 10601 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10602 | `		return rc;` |
|        - | 10603 | `	}` |
|        - | 10604 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 10605 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       30 | 10606 | `	nArg = 0;` |
|       30 | 10607 | `	if( pArgStart < pArgEnd ){` |
|        7 | 10608 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 10609 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 10610 | `		SyToken *pArgNext;` |
|        7 | 10611 | `		pGen->pIn = pArgStart;` |
|        7 | 10612 | `		pGen->pEnd = pArgEnd;` |
|       13 | 10613 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 10614 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 10615 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 10616 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10617 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 10618 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 10619 | `					return SXERR_ABORT;` |
|        - | 10620 | `				}` |
|        7 | 10621 | `				nArg++;` |
|        3 | 10622 | `			}` |
|        7 | 10623 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 10624 | `		}` |
|        7 | 10625 | `		pGen->pIn = pSavedIn;` |
|        7 | 10626 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 10627 | `	}` |
|        - | 10628 | `	/* Load the synthesized class name */` |
|       30 | 10629 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       30 | 10630 | `	if( pObj == 0 ){` |
|      ! 0 | 10631 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 10632 | `		return SXERR_ABORT;` |
|        - | 10633 | `	}` |
|       30 | 10634 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       30 | 10635 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 10636 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       30 | 10637 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       30 | 10638 | `	return SXRET_OK;` |
|       17 | 10639 | `}` |
|        - | 10640 | `/*` |
|        - | 10641 | ` * Compile a user-defined abstract class.` |
|        - | 10642 | ` *  According to the PHP language reference manual` |
|        - | 10643 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 10644 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 10645 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 10646 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 10647 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 10648 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 10649 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 10650 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 10651 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 10652 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 10653 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 10654 | ` *   could differ.` |
|        - | 10655 | ` */` |
|        - | 10656 | `/*` |
|        - | 10657 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 10658 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 10659 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 10660 | ` */` |
|  6289716 | 10661 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 10662 | `{` |
|  6289721 | 10663 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  3908145 | 10664 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  3908145 | 10665 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  3869279 | 10666 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  1926832 | 10667 | `	}` |
|  6235245 | 10668 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  6235185 | 10669 | `	return FALSE;` |
|  3144863 | 10670 | `}` |
|        - | 10671 | `/*` |
|        - | 10672 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 10673 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 10674 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 10675 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 10676 | ` */` |
|  6235180 | 10677 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 10678 | `{` |
|  6235185 | 10679 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  6235185 | 10680 | `	sxi32 iFlags = 0,iFlag;` |
|  6289721 | 10681 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    54541 | 10682 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 10683 | `			pDup = pIn;` |
|        2 | 10684 | `		}` |
|    54541 | 10685 | `		iFlags \|= iFlag;` |
|    54541 | 10686 | `		pIn++;` |
|        5 | 10687 | `	}` |
|  6235185 | 10688 | `	*ppIn = pIn;` |
|  6235185 | 10689 | `	if( ppDup ){ *ppDup = pDup; }` |
|  6235185 | 10690 | `	return iFlags;` |
|        5 | 10691 | `}` |
|        - | 10692 | `/*` |
|        - | 10693 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 10694 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 10695 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 10696 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 10697 | `` * `readonly`) to their existing handlers.`` |
|        - | 10698 | ` */` |
|  6207922 | 10699 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 10700 | `{` |
|  6207927 | 10701 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  3131226 | 10702 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  6221553 | 10703 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 10704 | `}` |
|        - | 10705 | `/*` |
|        - | 10706 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 10707 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 10708 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 10709 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 10710 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 10711 | ` */` |
|    27258 | 10712 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 10713 | `{` |
|        - | 10714 | `	SyToken *pDup;` |
|    27263 | 10715 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 10716 | `	sxi32 rc;` |
|    27263 | 10717 | `	if( pDup ){` |
|        4 | 10718 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 10719 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 10720 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10721 | `			return SXERR_ABORT;` |
|        - | 10722 | `		}` |
|        1 | 10723 | `	}` |
|    27258 | 10724 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    13634 | 10725 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 10726 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10727 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 10728 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10729 | `			return SXERR_ABORT;` |
|        - | 10730 | `		}` |
|        1 | 10731 | `	}` |
|    27263 | 10732 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    13634 | 10733 | `}` |
|        - | 10734 | `/*` |
|        - | 10735 | ` * Compile a user-defined trait.` |
|        - | 10736 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 10737 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 10738 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 10739 | ` */` |
|       72 | 10740 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 10741 | `{` |
|       77 | 10742 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10743 | `	ph7_class *pClass;` |
|        - | 10744 | `	SyToken *pEnd,*pTmp;` |
|        - | 10745 | `	sxi32 iProtection;` |
|        - | 10746 | `	sxi32 iAttrflags;` |
|        - | 10747 | `	SyString *pName;` |
|        - | 10748 | `	sxi32 nKwrd;` |
|        - | 10749 | `	sxi32 rc;` |
|        - | 10750 | `	/* Jump the 'trait' keyword */` |
|       77 | 10751 | `	pGen->pIn++;` |
|       77 | 10752 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10753 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 10754 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10755 | `			return SXERR_ABORT;` |
|        - | 10756 | `		}` |
|      ! 0 | 10757 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 10758 | `			pGen->pIn++;` |
|      ! 0 | 10759 | `		}` |
|      ! 0 | 10760 | `		return SXRET_OK;` |
|        - | 10761 | `	}` |
|        - | 10762 | `	/* Extract trait name */` |
|       77 | 10763 | `	pName = &pGen->pIn->sData;` |
|       77 | 10764 | `	pGen->pIn++;` |
|        - | 10765 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 10766 | `		SyBlob sFQN;` |
|        - | 10767 | `		SyString sFQNStr;` |
|       77 | 10768 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       77 | 10769 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       77 | 10770 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       77 | 10771 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|       77 | 10772 | `		SyBlobRelease(&sFQN);` |
|        - | 10773 | `	}` |
|       77 | 10774 | `	if( pClass == 0 ){` |
|      ! 0 | 10775 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10776 | `		return SXERR_ABORT;` |
|        - | 10777 | `	}` |
|       77 | 10778 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|       77 | 10779 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10780 | `		return SXERR_ABORT;` |
|        - | 10781 | `	}` |
|        - | 10782 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|       77 | 10783 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 10784 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 10785 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10786 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10787 | `			return SXERR_ABORT;` |
|        - | 10788 | `		}` |
|      ! 0 | 10789 | `		return SXRET_OK;` |
|        - | 10790 | `	}` |
|       77 | 10791 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|       77 | 10792 | `	pEnd = 0;` |
|       77 | 10793 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|       77 | 10794 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 10795 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 10796 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10797 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10798 | `			return SXERR_ABORT;` |
|        - | 10799 | `		}` |
|      ! 0 | 10800 | `		return SXRET_OK;` |
|        - | 10801 | `	}` |
|        - | 10802 | `	/* The delimiter token is the trait body's closing brace */` |
|       77 | 10803 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 10804 | `	/* Swap token stream */` |
|       77 | 10805 | `	pTmp = pGen->pEnd;` |
|       77 | 10806 | `	pGen->pEnd = pEnd;` |
|        - | 10807 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|       77 | 10808 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 10809 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|       71 | 10810 | `	for(;;){` |
|      191 | 10811 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       28 | 10812 | `			pGen->pIn++;` |
|        4 | 10813 | `		}` |
|      167 | 10814 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       77 | 10815 | `			break;` |
|        - | 10816 | `		}` |
|        - | 10817 | `		/* Bind a directly-preceding docblock to this member */` |
|       95 | 10818 | `		GenStateSetPendingDoc(&(*pGen));` |
|       95 | 10819 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 10820 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10821 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 10822 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 10823 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10824 | `				return SXERR_ABORT;` |
|        - | 10825 | `			}` |
|      ! 0 | 10826 | `			goto done;` |
|        - | 10827 | `		}` |
|       95 | 10828 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|       95 | 10829 | `		iAttrflags = 0;` |
|       95 | 10830 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       95 | 10831 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       95 | 10832 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 10833 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 10834 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 10835 | `				for(;;){` |
|        - | 10836 | `					ph7_class *pUsedTrait;` |
|        - | 10837 | `					SyString *pUsedName;` |
|        5 | 10838 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10839 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10840 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 10841 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10842 | `							return SXERR_ABORT;` |
|        - | 10843 | `						}` |
|      ! 0 | 10844 | `						break;` |
|        - | 10845 | `					}` |
|        5 | 10846 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 10847 | `					{` |
|        - | 10848 | `						SyBlob sResolved;` |
|        5 | 10849 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 10850 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 10851 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 10852 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 10853 | `						SyBlobRelease(&sResolved);` |
|        - | 10854 | `					}` |
|        5 | 10855 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 10856 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 10857 | `					}` |
|        5 | 10858 | `					if( pUsedTrait == 0 ){` |
|        4 | 10859 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 10860 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 10861 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10862 | `							return SXERR_ABORT;` |
|        - | 10863 | `						}` |
|        2 | 10864 | `					}else{` |
|        3 | 10865 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 10866 | `					}` |
|        5 | 10867 | `					pGen->pIn++;` |
|        5 | 10868 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 10869 | `						break;` |
|        - | 10870 | `					}` |
|      ! 0 | 10871 | `					pGen->pIn++;` |
|      ! 0 | 10872 | `				}` |
|        5 | 10873 | `				continue;` |
|        - | 10874 | `			}` |
|       91 | 10875 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       77 | 10876 | `				iProtection = nKwrd;` |
|       77 | 10877 | `				pGen->pIn++;` |
|       72 | 10878 | `				if( pGen->pIn >= pGen->pEnd` |
|       77 | 10879 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10880 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10881 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 10882 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10883 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10884 | `						return SXERR_ABORT;` |
|        - | 10885 | `					}` |
|      ! 0 | 10886 | `					goto done;` |
|        - | 10887 | `				}` |
|       77 | 10888 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       12 | 10889 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       12 | 10890 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10891 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10892 | `							return SXERR_ABORT;` |
|        - | 10893 | `						}` |
|      ! 0 | 10894 | `						goto done;` |
|        - | 10895 | `					}` |
|       12 | 10896 | `					continue;` |
|        - | 10897 | `				}` |
|       67 | 10898 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        5 | 10899 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        5 | 10900 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10901 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10902 | `							return SXERR_ABORT;` |
|        - | 10903 | `						}` |
|      ! 0 | 10904 | `						goto done;` |
|        - | 10905 | `					}` |
|        5 | 10906 | `					continue;` |
|        - | 10907 | `				}` |
|       63 | 10908 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       29 | 10909 | `			}` |
|       77 | 10910 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 10911 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10912 | `					"Traits cannot have constants");` |
|      ! 0 | 10913 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10914 | `					return SXERR_ABORT;` |
|        - | 10915 | `				}` |
|      ! 0 | 10916 | `				goto done;` |
|      ! 0 | 10917 | `			}else{` |
|       77 | 10918 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        8 | 10919 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|        8 | 10920 | `					pGen->pIn++;` |
|        8 | 10921 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 10922 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 10923 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 10924 | `							iProtection = nKwrd;` |
|      ! 0 | 10925 | `							pGen->pIn++;` |
|      ! 0 | 10926 | `						}` |
|        2 | 10927 | `					}` |
|        6 | 10928 | `					if( pGen->pIn >= pGen->pEnd` |
|        8 | 10929 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10930 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10931 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 10932 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10933 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10934 | `							return SXERR_ABORT;` |
|        - | 10935 | `						}` |
|      ! 0 | 10936 | `						goto done;` |
|        - | 10937 | `					}` |
|        8 | 10938 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 10939 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 10940 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 10941 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10942 | `								return SXERR_ABORT;` |
|        - | 10943 | `							}` |
|      ! 0 | 10944 | `							goto done;` |
|        - | 10945 | `						}` |
|        3 | 10946 | `						continue;` |
|        - | 10947 | `					}` |
|        6 | 10948 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 10949 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10950 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 10951 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10952 | `								return SXERR_ABORT;` |
|        - | 10953 | `							}` |
|      ! 0 | 10954 | `							goto done;` |
|        - | 10955 | `						}` |
|      ! 0 | 10956 | `						continue;` |
|        - | 10957 | `					}` |
|        6 | 10958 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       73 | 10959 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        6 | 10960 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        6 | 10961 | `					pGen->pIn++;` |
|        6 | 10962 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 10963 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 10964 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        6 | 10965 | `							iProtection = nKwrd;` |
|        6 | 10966 | `							pGen->pIn++;` |
|        2 | 10967 | `						}` |
|        2 | 10968 | `					}` |
|        6 | 10969 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        4 | 10970 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10971 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10972 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 10973 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10974 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10975 | `							return SXERR_ABORT;` |
|        - | 10976 | `						}` |
|      ! 0 | 10977 | `						goto done;` |
|        - | 10978 | `					}` |
|        6 | 10979 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        2 | 10980 | `				}` |
|       75 | 10981 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 10982 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10983 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 10984 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10985 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10986 | `						return SXERR_ABORT;` |
|        - | 10987 | `					}` |
|      ! 0 | 10988 | `					goto done;` |
|        - | 10989 | `				}` |
|       75 | 10990 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 10991 | `					pGen->pIn++;` |
|      ! 0 | 10992 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 10993 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10994 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 10995 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10996 | `							return SXERR_ABORT;` |
|        - | 10997 | `						}` |
|      ! 0 | 10998 | `						goto done;` |
|        - | 10999 | `					}` |
|      ! 0 | 11000 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11001 | `				}else{` |
|       75 | 11002 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 11003 | `				}` |
|       75 | 11004 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 11005 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11006 | `						return SXERR_ABORT;` |
|        - | 11007 | `					}` |
|      ! 0 | 11008 | `					goto done;` |
|        - | 11009 | `				}` |
|        - | 11010 | `			}` |
|       40 | 11011 | `		}else{` |
|      ! 0 | 11012 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11013 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11014 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11015 | `					return SXERR_ABORT;` |
|        - | 11016 | `				}` |
|      ! 0 | 11017 | `				goto done;` |
|        - | 11018 | `			}` |
|        - | 11019 | `		}` |
|        5 | 11020 | `	}` |
|        - | 11021 | `	/* Install the trait */` |
|       77 | 11022 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|       77 | 11023 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11024 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11025 | `		return SXERR_ABORT;` |
|        - | 11026 | `	}` |
|       36 | 11027 | `done:` |
|        - | 11028 | `	/* Point beyond the trait body */` |
|       77 | 11029 | `	pGen->pIn = &pEnd[1];` |
|       77 | 11030 | `	pGen->pEnd = pTmp;` |
|       77 | 11031 | `	return PH7_OK;` |
|       41 | 11032 | `}` |
|        - | 11033 | `/*` |
|        - | 11034 | ` * Compile a user-defined class.` |
|        - | 11035 | ` *  According to the PHP language reference manual` |
|        - | 11036 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 11037 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 11038 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 11039 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 11040 | ` *   and functions (called "methods").` |
|        - | 11041 | ` */` |
|   187920 | 11042 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 11043 | `{` |
|        - | 11044 | `	sxi32 rc;` |
|   187925 | 11045 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   187925 | 11046 | `	return rc;` |
|        5 | 11047 | `}` |
|        - | 11048 | `/*` |
|        - | 11049 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 11050 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 11051 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 11052 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 11053 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 11054 | ` */` |
|  6180664 | 11055 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 11056 | `{` |
|  6213934 | 11057 | `	return (pIn->nType & PH7_TK_ID)` |
|  3123597 | 11058 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    37270 | 11059 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  6213929 | 11060 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 11061 | `}` |
|        - | 11062 | `/*` |
|        - | 11063 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 11064 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 11065 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 11066 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 11067 | ` */` |
|       28 | 11068 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 11069 | `{` |
|       33 | 11070 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 11071 | `}` |
|        - | 11072 | `/*` |
|        - | 11073 | ` * Exception handling.` |
|        - | 11074 | ` *  According to the PHP language reference manual` |
|        - | 11075 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 11076 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 11077 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 11078 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 11079 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 11080 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 11081 | ` *    (or re-thrown) within a catch block.` |
|        - | 11082 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 11083 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 11084 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 11085 | ` *    been defined with set_exception_handler().` |
|        - | 11086 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 11087 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 11088 | ` */` |
|        - | 11089 | `/*` |
|        - | 11090 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 11091 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 11092 | ` * indicates failure.` |
|        - | 11093 | ` */` |
|   315008 | 11094 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 11095 | `{` |
|   315013 | 11096 | `	sxi32 rc = SXRET_OK;` |
|   315013 | 11097 | `	if( pRoot->pOp ){` |
|   315001 | 11098 | `		switch( pRoot->pOp->iOp ){` |
|   157498 | 11099 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|        - | 11100 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|        - | 11101 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|        - | 11102 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|        - | 11103 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|        - | 11104 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   315001 | 11105 | `			break;` |
|      ! 0 | 11106 | `		default:` |
|        - | 11107 | `			/* Runtime will still reject non-Throwable values; the set above` |
|        - | 11108 | `			 * covers the common shapes and gives a friendlier compile error` |
|        - | 11109 | ``			 * for obvious mistakes like `throw 5`. */`` |
|      ! 0 | 11110 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11111 | `				"throw: Expecting an exception class instance");` |
|      ! 0 | 11112 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 | 11113 | `				rc = SXERR_INVALID;` |
|      ! 0 | 11114 | `			}` |
|      ! 0 | 11115 | `			break;` |
|        - | 11116 | `		}` |
|   157515 | 11117 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 11118 | `		/* Unexpected expression */` |
|      ! 0 | 11119 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11120 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11121 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 11122 | `			rc = SXERR_INVALID;` |
|      ! 0 | 11123 | `		}` |
|      ! 0 | 11124 | `	}` |
|   315013 | 11125 | `	return rc;` |
|        5 | 11126 | `}` |
|        - | 11127 | `/*` |
|        - | 11128 | ` * Compile a 'throw' statement.` |
|        - | 11129 | ` * throw: This is how you trigger an exception.` |
|        - | 11130 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 11131 | ` */` |
|   314972 | 11132 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 11133 | `{` |
|   314977 | 11134 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11135 | `	GenBlock *pBlock;` |
|        - | 11136 | `	sxu32 nIdx;` |
|        - | 11137 | `	sxi32 rc;` |
|   314977 | 11138 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 11139 | `	/* Compile the expression */` |
|   314977 | 11140 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   314977 | 11141 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11142 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 11143 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11144 | `			return SXERR_ABORT;` |
|        - | 11145 | `		}` |
|      ! 0 | 11146 | `		return SXRET_OK;` |
|        - | 11147 | `	}` |
|   314977 | 11148 | `	pBlock = pGen->pCurrent;` |
|        - | 11149 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  1228101 | 11150 | `	while(pBlock->pParent){` |
|  1228097 | 11151 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   314973 | 11152 | `			break;` |
|        - | 11153 | `		}` |
|        - | 11154 | `		/* Point to the parent block */` |
|   913129 | 11155 | `		pBlock = pBlock->pParent;` |
|        5 | 11156 | `	}` |
|        - | 11157 | `	/* Emit the throw instruction */` |
|   314977 | 11158 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 11159 | `	/* Emit the jump */` |
|   314977 | 11160 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   314977 | 11161 | `	return SXRET_OK;` |
|   157491 | 11162 | `}` |
|        - | 11163 | `/*` |
|        - | 11164 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 11165 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 11166 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 11167 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 11168 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 11169 | ` */` |
|       36 | 11170 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 11171 | `{` |
|       38 | 11172 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11173 | `	GenBlock *pBlock;` |
|        - | 11174 | `	sxu32 nIdx;` |
|        - | 11175 | `	sxi32 rc;` |
|       18 | 11176 | `	(void)iCompileFlag;` |
|       38 | 11177 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 11178 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 11179 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11180 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11181 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11182 | `			return SXERR_ABORT;` |
|        - | 11183 | `		}` |
|      ! 0 | 11184 | `		return SXRET_OK;` |
|        - | 11185 | `	}` |
|       38 | 11186 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 11187 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11188 | `		return SXERR_ABORT;` |
|        - | 11189 | `	}` |
|       38 | 11190 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11191 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11192 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11193 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11194 | `			return SXERR_ABORT;` |
|        - | 11195 | `		}` |
|      ! 0 | 11196 | `		return SXRET_OK;` |
|        - | 11197 | `	}` |
|        - | 11198 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 11199 | `	pBlock = pGen->pCurrent;` |
|       60 | 11200 | `	while( pBlock->pParent ){` |
|       49 | 11201 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 11202 | `			break;` |
|        - | 11203 | `		}` |
|       23 | 11204 | `		pBlock = pBlock->pParent;` |
|        1 | 11205 | `	}` |
|       38 | 11206 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 11207 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 11208 | `	return SXRET_OK;` |
|       20 | 11209 | `}` |
|        - | 11210 | `/*` |
|        - | 11211 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 11212 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 11213 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 11214 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 11215 | ` * compile error propagated from the parser.` |
|        - | 11216 | ` */` |
|       54 | 11217 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 11218 | `{` |
|        - | 11219 | `	SyString sClassName;` |
|        - | 11220 | `	SyToken *pToken;` |
|        - | 11221 | `	SyString *pName;` |
|        - | 11222 | `	char *zDup;` |
|        - | 11223 | `	sxi32 rc;` |
|       59 | 11224 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       59 | 11225 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       59 | 11226 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       59 | 11227 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       59 | 11228 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 11229 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11230 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11231 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11232 | `		return SXERR_INVALID;` |
|        - | 11233 | `	}` |
|       59 | 11234 | `	pGen->pIn++; /* '(' */` |
|       27 | 11235 | `	for(;;){` |
|        - | 11236 | `		SyBlob sResolved;` |
|       59 | 11237 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       59 | 11238 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 11239 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 11240 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11241 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11242 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11243 | `			return SXERR_INVALID;` |
|        - | 11244 | `		}` |
|       86 | 11245 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       54 | 11246 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       59 | 11247 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       59 | 11248 | `		SyBlobRelease(&sResolved);` |
|       59 | 11249 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 11250 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       59 | 11251 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       54 | 11252 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 11253 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 11254 | `			pGen->pIn++; continue;` |
|        - | 11255 | `		}` |
|       59 | 11256 | `		break;` |
|      ! 0 | 11257 | `	}` |
|       54 | 11258 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 11259 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 11260 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11261 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11262 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11263 | `		return SXERR_INVALID;` |
|        - | 11264 | `	}` |
|       59 | 11265 | `	pGen->pIn++; /* '$' */` |
|       59 | 11266 | `	pName = &pGen->pIn->sData;` |
|       59 | 11267 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 11268 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 11269 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 11270 | `	pGen->pIn++;` |
|       59 | 11271 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 11272 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11273 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11274 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11275 | `		return SXERR_INVALID;` |
|        - | 11276 | `	}` |
|       59 | 11277 | `	pGen->pIn++; /* ')' */` |
|       59 | 11278 | `	return SXRET_OK;` |
|       32 | 11279 | `}` |
|        - | 11280 | `/*` |
|        - | 11281 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 11282 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 11283 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 11284 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 11285 | ` * VmThrowException):` |
|        - | 11286 | ` *` |
|        - | 11287 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 11288 | ` *    <try body>` |
|        - | 11289 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 11290 | ` *    JMP  -> finally\|end` |
|        - | 11291 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 11292 | ` *    <catch body>` |
|        - | 11293 | ` *    JMP  -> finally\|end` |
|        - | 11294 | ` *    ... more catches ...` |
|        - | 11295 | ` *  Lfin: <finally body>` |
|        - | 11296 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 11297 | ` *  Lend:` |
|        - | 11298 | ` */` |
|       98 | 11299 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 11300 | `{` |
|      103 | 11301 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11302 | `	GenBlock *pTry;` |
|        - | 11303 | `	VmInstr *pInstr;` |
|      103 | 11304 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 11305 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 11306 | `	sxi32 rc;` |
|      103 | 11307 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 11308 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      103 | 11309 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      103 | 11310 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      103 | 11311 | `	pTry->pUserData = pException;` |
|      103 | 11312 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      103 | 11313 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      103 | 11314 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      103 | 11315 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      103 | 11316 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      103 | 11317 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 11318 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      103 | 11319 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      103 | 11320 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      103 | 11321 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      103 | 11322 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11323 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      103 | 11324 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 11325 | `	/* Catch clauses (inline) */` |
|      103 | 11326 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       98 | 11327 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       59 | 11328 | `		sxu32 k = 0;` |
|       81 | 11329 | `		for(;;){` |
|        - | 11330 | `			ph7_exception_block sCatch;` |
|        - | 11331 | `			GenBlock *pCatchBlk;` |
|      113 | 11332 | `			sxu32 idxJmp = 0;` |
|      108 | 11333 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      104 | 11334 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       32 | 11335 | `				break;` |
|        - | 11336 | `			}` |
|       59 | 11337 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       59 | 11338 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 11339 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       59 | 11340 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       59 | 11341 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       59 | 11342 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       59 | 11343 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 11344 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 11345 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 11346 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       59 | 11347 | `			pCatchBlk->pUserData = pException;` |
|       59 | 11348 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       59 | 11349 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 11350 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       59 | 11351 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11352 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 11353 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       59 | 11354 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       59 | 11355 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       59 | 11356 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       59 | 11357 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       59 | 11358 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       59 | 11359 | `			k++;` |
|        5 | 11360 | `		}` |
|       27 | 11361 | `	}` |
|        - | 11362 | `	/* Finally (inline) */` |
|      103 | 11363 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 11364 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 11365 | `		GenBlock *pFinBlk;` |
|       52 | 11366 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 11367 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 11368 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 11369 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 11370 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 11371 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 11372 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 11373 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 11374 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 11375 | `		pException->iHasFinally = 1;` |
|       24 | 11376 | `	}` |
|      103 | 11377 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      103 | 11378 | `	pException->iInlined = 1;` |
|        - | 11379 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 11380 | `	{` |
|      103 | 11381 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 11382 | `		sxu32 *aJ; sxu32 n;` |
|      103 | 11383 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      103 | 11384 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      103 | 11385 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      157 | 11386 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       59 | 11387 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       59 | 11388 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       32 | 11389 | `		}` |
|        - | 11390 | `	}` |
|      103 | 11391 | `	SySetRelease(&aCatchJmp);` |
|      103 | 11392 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 11393 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 11394 | `	}` |
|      103 | 11395 | `	return SXRET_OK;` |
|       54 | 11396 | `}` |
|        - | 11397 | `/*` |
|        - | 11398 | ` * Compile a 'catch' block.` |
|        - | 11399 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 11400 | ` * an object containing the exception information.` |
|        - | 11401 | ` */` |
|     5200 | 11402 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 11403 | `{` |
|     5205 | 11404 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11405 | `	ph7_exception_block sCatch;` |
|        - | 11406 | `	SySet *pInstrContainer;` |
|        - | 11407 | `	SyString sClassName;` |
|        - | 11408 | `	GenBlock *pCatch;` |
|        - | 11409 | `	SyToken *pToken;` |
|        - | 11410 | `	SyString *pName;` |
|        - | 11411 | `	char *zDup;` |
|        - | 11412 | `	sxi32 rc;` |
|     5205 | 11413 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 11414 | `	/* Zero the structure */` |
|     5205 | 11415 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 11416 | `	/* Initialize fields */` |
|     5205 | 11417 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     5205 | 11418 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     5205 | 11419 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 11420 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 11421 | `			pToken = pGen->pIn;` |
|      ! 0 | 11422 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11423 | `				pToken--;` |
|      ! 0 | 11424 | `			}` |
|      ! 0 | 11425 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11426 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11427 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11428 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11429 | `				return SXERR_ABORT;` |
|        - | 11430 | `			}` |
|      ! 0 | 11431 | `			return SXERR_INVALID;` |
|        - | 11432 | `	}` |
|        - | 11433 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     5205 | 11434 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     2614 | 11435 | `	for(;;){` |
|        - | 11436 | `		SyBlob sResolved;` |
|     5233 | 11437 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     5233 | 11438 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 11439 | `			SyBlobRelease(&sResolved);` |
|        6 | 11440 | `			pToken = pGen->pIn;` |
|        6 | 11441 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11442 | `				pToken--;` |
|      ! 0 | 11443 | `			}` |
|        8 | 11444 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11445 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 11446 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 11447 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11448 | `				return SXERR_ABORT;` |
|        - | 11449 | `			}` |
|        6 | 11450 | `			return SXERR_INVALID;` |
|        - | 11451 | `		}` |
|        - | 11452 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 11453 | `		 * transient SyBlob allocation. */` |
|     7841 | 11454 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     5224 | 11455 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     5229 | 11456 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     5229 | 11457 | `		SyBlobRelease(&sResolved);` |
|     5229 | 11458 | `		if( zDup == 0 ){` |
|      ! 0 | 11459 | `			goto Mem;` |
|        - | 11460 | `		}` |
|     5229 | 11461 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     5229 | 11462 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11463 | `			goto Mem;` |
|        - | 11464 | `		}` |
|        - | 11465 | `		/* Check for '\|' (multi-catch separator) */` |
|     5224 | 11466 | `		if( pGen->pIn < pGen->pEnd &&` |
|     5224 | 11467 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       33 | 11468 | `			pGen->pIn->sData.nByte == 1 &&` |
|       28 | 11469 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       30 | 11470 | `			pGen->pIn++; /* Consume the '\|' */` |
|       30 | 11471 | `			continue;` |
|        - | 11472 | `		}` |
|     5201 | 11473 | `		break;` |
|      ! 0 | 11474 | `	}` |
|     5196 | 11475 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     5201 | 11476 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 11477 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 11478 | `			pToken = pGen->pIn;` |
|      ! 0 | 11479 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11480 | `				pToken--;` |
|      ! 0 | 11481 | `			}` |
|      ! 0 | 11482 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11483 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11484 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11485 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11486 | `				return SXERR_ABORT;` |
|        - | 11487 | `			}` |
|      ! 0 | 11488 | `			return SXERR_INVALID;` |
|        - | 11489 | `	}` |
|     5201 | 11490 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 11491 | `	/* Duplicate instance name */` |
|     5201 | 11492 | `	pName = &pGen->pIn->sData;` |
|     5201 | 11493 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     5201 | 11494 | `	if( zDup == 0 ){` |
|      ! 0 | 11495 | `		goto Mem;` |
|        - | 11496 | `	}` |
|     5201 | 11497 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     5201 | 11498 | `	pGen->pIn++;` |
|     5201 | 11499 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 11500 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 11501 | `		pToken = pGen->pIn;` |
|      ! 0 | 11502 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11503 | `			pToken--;` |
|      ! 0 | 11504 | `		}` |
|      ! 0 | 11505 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11506 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11507 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11508 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11509 | `			return SXERR_ABORT;` |
|        - | 11510 | `		}` |
|      ! 0 | 11511 | `		return SXERR_INVALID;` |
|        - | 11512 | `	}` |
|        - | 11513 | `	/* Compile the block */` |
|     5201 | 11514 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 11515 | `	/* Create the catch block */` |
|     5201 | 11516 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     5201 | 11517 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11518 | `		return SXERR_ABORT;` |
|        - | 11519 | `	}` |
|        - | 11520 | `	/* Swap bytecode container */` |
|     5201 | 11521 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     5201 | 11522 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 11523 | `	/* Compile the block */` |
|     5201 | 11524 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 11525 | `	/* Fix forward jumps now the destination is resolved  */` |
|     5201 | 11526 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11527 | `	/* Emit the DONE instruction */` |
|     5201 | 11528 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 11529 | `	/* Leave the block */` |
|     5201 | 11530 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11531 | `	/* Restore the default container */` |
|     5201 | 11532 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 11533 | `	/* Install the catch block */` |
|     5201 | 11534 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     5201 | 11535 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11536 | `		goto Mem;` |
|        - | 11537 | `	}` |
|     5201 | 11538 | `	return SXRET_OK;` |
|      ! 0 | 11539 | `Mem:` |
|      ! 0 | 11540 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11541 | `	return SXERR_ABORT;` |
|     2605 | 11542 | `}` |
|        - | 11543 | `/*` |
|        - | 11544 | ` * Compile a 'try' block.` |
|        - | 11545 | ` * A function using an exception should be in a "try" block.` |
|        - | 11546 | ` * If the exception does not trigger, the code will continue` |
|        - | 11547 | ` * as normal. However if the exception triggers, an exception` |
|        - | 11548 | ` * is "thrown".` |
|        - | 11549 | ` */` |
|     5356 | 11550 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 11551 | `{` |
|        - | 11552 | `	ph7_exception *pException;` |
|     5361 | 11553 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11554 | `	GenBlock *pTry;` |
|        - | 11555 | `	sxu32 nJmpIdx;` |
|        - | 11556 | `	sxi32 rc;` |
|        - | 11557 | `	/* Create the exception container */` |
|     5361 | 11558 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     5361 | 11559 | `	if( pException == 0 ){` |
|      ! 0 | 11560 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 11561 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11562 | `		return SXERR_ABORT;` |
|        - | 11563 | `	}` |
|        - | 11564 | `	/* Zero the structure */` |
|     5361 | 11565 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 11566 | `	/* Initialize fields */` |
|     5361 | 11567 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     5361 | 11568 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     5361 | 11569 | `	pException->iHasFinally = 0;` |
|     5361 | 11570 | `	pException->iFinallyDone = 0;` |
|     5361 | 11571 | `	pException->pVm = pGen->pVm;` |
|        - | 11572 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 11573 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 11574 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 11575 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 11576 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 11577 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     5361 | 11578 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      103 | 11579 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 11580 | `	}` |
|        - | 11581 | `	/* Create the try block */` |
|     5263 | 11582 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     5263 | 11583 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11584 | `		return SXERR_ABORT;` |
|        - | 11585 | `	}` |
|        - | 11586 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     5263 | 11587 | `	pTry->pUserData = pException;` |
|        - | 11588 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     5263 | 11589 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 11590 | `	/* Fix the jump later when the destination is resolved */` |
|     5263 | 11591 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     5263 | 11592 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 11593 | `	/* Compile the block */` |
|     5263 | 11594 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     5263 | 11595 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11596 | `		return SXERR_ABORT;` |
|        - | 11597 | `	}` |
|        - | 11598 | `	/* Fix forward jumps now the destination is resolved */` |
|     5263 | 11599 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11600 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     5263 | 11601 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 11602 | `	/* Leave the block */` |
|     5263 | 11603 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11604 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     5263 | 11605 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     5256 | 11606 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 11607 | `		/* Compile one or more catch blocks */` |
|     5196 | 11608 | `		for(;;){` |
|    10392 | 11609 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     7830 | 11610 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     2601 | 11611 | `					break;` |
|        - | 11612 | `			}` |
|     5205 | 11613 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     5205 | 11614 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11615 | `				return SXERR_ABORT;` |
|        - | 11616 | `			}` |
|        5 | 11617 | `		}` |
|     2596 | 11618 | `	}` |
|        - | 11619 | `	/* Compile optional finally block */` |
|     5263 | 11620 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      644 | 11621 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 11622 | `		SySet *pInstrContainer;` |
|        - | 11623 | `		GenBlock *pFinBlock;` |
|      129 | 11624 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 11625 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 11626 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 11627 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11628 | `			return SXERR_ABORT;` |
|        - | 11629 | `		}` |
|        - | 11630 | `		/* Swap bytecode container */` |
|      129 | 11631 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 11632 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 11633 | `		/* Compile the finally body */` |
|      129 | 11634 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 11635 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11636 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 11637 | `			return SXERR_ABORT;` |
|        - | 11638 | `		}` |
|        - | 11639 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 11640 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11641 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 11642 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 11643 | `		/* Leave the block */` |
|      129 | 11644 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11645 | `		/* Restore the default container */` |
|      129 | 11646 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 11647 | `		pException->iHasFinally = 1;` |
|       62 | 11648 | `	}` |
|        - | 11649 | `	/* Must have at least one catch or finally */` |
|     5263 | 11650 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        8 | 11651 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11652 | `			"Cannot use try without catch or finally");` |
|        8 | 11653 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11654 | `			return SXERR_ABORT;` |
|        - | 11655 | `		}` |
|        3 | 11656 | `	}` |
|     5263 | 11657 | `	return SXRET_OK;` |
|     2683 | 11658 | `}` |
|        - | 11659 | `/*` |
|        - | 11660 | ` * Compile a switch block.` |
|        - | 11661 | ` *  (See block-comment below for more information)` |
|        - | 11662 | ` */` |
|      112 | 11663 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 11664 | `{` |
|      117 | 11665 | `	sxi32 rc = SXRET_OK;` |
|      117 | 11666 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 11667 | `		/* Unexpected token */` |
|      ! 0 | 11668 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 11669 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11670 | `			return SXERR_ABORT;` |
|        - | 11671 | `		}` |
|      ! 0 | 11672 | `		pGen->pIn++;` |
|      ! 0 | 11673 | `	}` |
|      117 | 11674 | `	pGen->pIn++;` |
|        - | 11675 | `	/* First instruction to execute in this block. */` |
|      117 | 11676 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 11677 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 11678 | `	 * or the '}' token */` |
|      206 | 11679 | `	for(;;){` |
|      417 | 11680 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 11681 | `			/* No more input to process */` |
|      ! 0 | 11682 | `			break;` |
|        - | 11683 | `		}` |
|      417 | 11684 | `		rc = SXRET_OK;` |
|      417 | 11685 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       85 | 11686 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|       31 | 11687 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 11688 | `					/* Unexpected token */` |
|      ! 0 | 11689 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 11690 | `						&pGen->pIn->sData);` |
|      ! 0 | 11691 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11692 | `						return SXERR_ABORT;` |
|        - | 11693 | `					}` |
|        - | 11694 | `					/* FALL THROUGH */` |
|      ! 0 | 11695 | `				}` |
|       31 | 11696 | `				rc = SXERR_EOF;` |
|       31 | 11697 | `				break;` |
|        - | 11698 | `			}` |
|       32 | 11699 | `		}else{` |
|        - | 11700 | `			sxi32 nKwrd;` |
|        - | 11701 | `			/* Extract the keyword */` |
|      337 | 11702 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      337 | 11703 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|       47 | 11704 | `				break;` |
|        - | 11705 | `			}` |
|      253 | 11706 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 11707 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 11708 | `					/* Unexpected token */` |
|      ! 0 | 11709 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 11710 | `						&pGen->pIn->sData);` |
|      ! 0 | 11711 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11712 | `						return SXERR_ABORT;` |
|        - | 11713 | `					}` |
|        - | 11714 | `					/* FALL THROUGH */` |
|      ! 0 | 11715 | `				}` |
|        - | 11716 | `				/* Block compiled */` |
|        3 | 11717 | `				break;` |
|        - | 11718 | `			}` |
|        - | 11719 | `		}` |
|        - | 11720 | `		/* Compile block */` |
|      305 | 11721 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      305 | 11722 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11723 | `			return SXERR_ABORT;` |
|        - | 11724 | `		}` |
|        5 | 11725 | `	}` |
|      117 | 11726 | `	return rc;` |
|       61 | 11727 | `}` |
|        - | 11728 | `/*` |
|        - | 11729 | ` * Compile a case eXpression.` |
|        - | 11730 | ` *  (See block-comment below for more information)` |
|        - | 11731 | ` */` |
|       92 | 11732 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 11733 | `{` |
|        - | 11734 | `	SySet *pInstrContainer;` |
|        - | 11735 | `	SyToken *pEnd,*pTmp;` |
|       97 | 11736 | `	sxi32 iNest = 0;` |
|        - | 11737 | `	sxi32 rc;` |
|        - | 11738 | `	/* Delimit the expression */` |
|       97 | 11739 | `	pEnd = pGen->pIn;` |
|      197 | 11740 | `	while( pEnd < pGen->pEnd ){` |
|      197 | 11741 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 11742 | `			/* Increment nesting level */` |
|        3 | 11743 | `			iNest++;` |
|      196 | 11744 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 11745 | `			/* Decrement nesting level */` |
|        3 | 11746 | `			iNest--;` |
|      194 | 11747 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|       97 | 11748 | `			break;` |
|        - | 11749 | `		}` |
|      105 | 11750 | `		pEnd++;` |
|        5 | 11751 | `	}` |
|       97 | 11752 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 11753 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 11754 | `		if( rc == SXERR_ABORT ){` |
|        - | 11755 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11756 | `			return SXERR_ABORT;` |
|        - | 11757 | `		}` |
|      ! 0 | 11758 | `	}` |
|        - | 11759 | `	/* Swap token stream */` |
|       97 | 11760 | `	pTmp = pGen->pEnd;` |
|       97 | 11761 | `	pGen->pEnd = pEnd;` |
|       97 | 11762 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       97 | 11763 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|       97 | 11764 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 11765 | `	/* Emit the done instruction */` |
|       97 | 11766 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       97 | 11767 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 11768 | `	/* Update token stream */` |
|       97 | 11769 | `	pGen->pIn  = pEnd;` |
|       97 | 11770 | `	pGen->pEnd = pTmp;` |
|       97 | 11771 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11772 | `		return SXERR_ABORT;` |
|        - | 11773 | `	}` |
|       97 | 11774 | `	return SXRET_OK;` |
|       51 | 11775 | `}` |
|        - | 11776 | `/*` |
|        - | 11777 | ` * Compile the smart switch statement.` |
|        - | 11778 | ` * According to the PHP language reference manual` |
|        - | 11779 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 11780 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 11781 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 11782 | ` *  This is exactly what the switch statement is for.` |
|        - | 11783 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 11784 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 11785 | ` *  of the outer loop, use continue 2.` |
|        - | 11786 | ` *  Note that switch/case does loose comparision.` |
|        - | 11787 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 11788 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 11789 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 11790 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 11791 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 11792 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 11793 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 11794 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 11795 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 11796 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 11797 | ` *  list for the next case.` |
|        - | 11798 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 11799 | ` *  or floating-point numbers and strings.` |
|        - | 11800 | ` */` |
|       28 | 11801 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 11802 | `{` |
|        - | 11803 | `	GenBlock *pSwitchBlock;` |
|        - | 11804 | `	SyToken *pTmp,*pEnd;` |
|        - | 11805 | `	ph7_switch *pSwitch;` |
|        - | 11806 | `	sxu32 nToken;` |
|        - | 11807 | `	sxu32 nLine;` |
|        - | 11808 | `	sxi32 rc;` |
|       33 | 11809 | `	nLine = pGen->pIn->nLine;` |
|        - | 11810 | `	/* Jump the 'switch' keyword */` |
|       33 | 11811 | `	pGen->pIn++;` |
|       33 | 11812 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 11813 | `		/* Syntax error */` |
|      ! 0 | 11814 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 11815 | `		if( rc == SXERR_ABORT ){` |
|        - | 11816 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11817 | `			return SXERR_ABORT;` |
|        - | 11818 | `		}` |
|      ! 0 | 11819 | `		goto Synchronize;` |
|        - | 11820 | `	}` |
|        - | 11821 | `	/* Jump the left parenthesis '(' */` |
|       33 | 11822 | `	pGen->pIn++;` |
|       33 | 11823 | `	pEnd = 0; /* cc warning */` |
|        - | 11824 | `	/* Create the loop block */` |
|       47 | 11825 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|       14 | 11826 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|       33 | 11827 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11828 | `		return SXERR_ABORT;` |
|        - | 11829 | `	}` |
|        - | 11830 | `	/* Delimit the condition */` |
|       33 | 11831 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       33 | 11832 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 11833 | `		/* Empty expression */` |
|      ! 0 | 11834 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 11835 | `		if( rc == SXERR_ABORT ){` |
|        - | 11836 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11837 | `			return SXERR_ABORT;` |
|        - | 11838 | `		}` |
|      ! 0 | 11839 | `	}` |
|        - | 11840 | `	/* Swap token streams */` |
|       33 | 11841 | `	pTmp = pGen->pEnd;` |
|       33 | 11842 | `	pGen->pEnd = pEnd;` |
|        - | 11843 | `	/* Compile the expression */` |
|       33 | 11844 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       33 | 11845 | `	if( rc == SXERR_ABORT ){` |
|        - | 11846 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 11847 | `		return SXERR_ABORT;` |
|        - | 11848 | `	}` |
|        - | 11849 | `	/* Update token stream */` |
|       33 | 11850 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 11851 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 11852 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 11853 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11854 | `			return SXERR_ABORT;` |
|        - | 11855 | `		}` |
|      ! 0 | 11856 | `		pGen->pIn++;` |
|      ! 0 | 11857 | `	}` |
|       33 | 11858 | `	pGen->pIn  = &pEnd[1];` |
|       33 | 11859 | `	pGen->pEnd = pTmp;` |
|       33 | 11860 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       28 | 11861 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 11862 | `			pTmp = pGen->pIn;` |
|      ! 0 | 11863 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 11864 | `				pTmp--;` |
|      ! 0 | 11865 | `			}` |
|        - | 11866 | `			/* Unexpected token */` |
|      ! 0 | 11867 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 11868 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11869 | `				return SXERR_ABORT;` |
|        - | 11870 | `			}` |
|      ! 0 | 11871 | `			goto Synchronize;` |
|        - | 11872 | `	}` |
|        - | 11873 | `	/* Set the delimiter token */` |
|       33 | 11874 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 11875 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 11876 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 11877 | `	}else{` |
|       31 | 11878 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 11879 | `	}` |
|       33 | 11880 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 11881 | `	/* Create the switch blocks container */` |
|       33 | 11882 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|       33 | 11883 | `	if( pSwitch == 0 ){` |
|        - | 11884 | `		/* Abort compilation */` |
|      ! 0 | 11885 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11886 | `		return SXERR_ABORT;` |
|        - | 11887 | `	}` |
|        - | 11888 | `	/* Zero the structure */` |
|       33 | 11889 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 11890 | `	/* Initialize fields */` |
|       33 | 11891 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 11892 | `	/* Emit the switch instruction */` |
|       33 | 11893 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 11894 | `	/* Compile case blocks */` |
|      100 | 11895 | `	for(;;){` |
|        - | 11896 | `		sxu32 nKwrd;` |
|      119 | 11897 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 11898 | `			/* No more input to process */` |
|      ! 0 | 11899 | `			break;` |
|        - | 11900 | `		}` |
|      119 | 11901 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 11902 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 11903 | `				/* Unexpected token */` |
|      ! 0 | 11904 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 11905 | `					&pGen->pIn->sData);` |
|      ! 0 | 11906 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11907 | `					return SXERR_ABORT;` |
|        - | 11908 | `				}` |
|        - | 11909 | `				/* FALL THROUGH */` |
|      ! 0 | 11910 | `			}` |
|        - | 11911 | `			/* Block compiled */` |
|      ! 0 | 11912 | `			break;` |
|        - | 11913 | `		}` |
|        - | 11914 | `		/* Extract the keyword */` |
|      119 | 11915 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      119 | 11916 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 11917 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 11918 | `				/* Unexpected token */` |
|      ! 0 | 11919 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 11920 | `					&pGen->pIn->sData);` |
|      ! 0 | 11921 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11922 | `					return SXERR_ABORT;` |
|        - | 11923 | `				}` |
|        - | 11924 | `				/* FALL THROUGH */` |
|      ! 0 | 11925 | `			}` |
|        - | 11926 | `			/* Block compiled */` |
|        3 | 11927 | `			break;` |
|        - | 11928 | `		}` |
|      117 | 11929 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 11930 | `			/*` |
|        - | 11931 | `			 * Accroding to the PHP language reference manual` |
|        - | 11932 | `			 *  A special case is the default case. This case matches anything` |
|        - | 11933 | `			 *  that wasn't matched by the other cases.` |
|        - | 11934 | `			 */` |
|       25 | 11935 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 11936 | `				/* Default case already compiled */` |
|      ! 0 | 11937 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 11938 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11939 | `					return SXERR_ABORT;` |
|        - | 11940 | `				}` |
|      ! 0 | 11941 | `			}` |
|       25 | 11942 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 11943 | `			/* Compile the default block */` |
|       25 | 11944 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|       25 | 11945 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 11946 | `				return SXERR_ABORT;` |
|       25 | 11947 | `			}else if( rc == SXERR_EOF ){` |
|       23 | 11948 | `				break;` |
|        1 | 11949 | `			}` |
|       98 | 11950 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 11951 | `			ph7_case_expr sCase;` |
|        - | 11952 | `			/* Standard case block */` |
|       97 | 11953 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 11954 | `			/* initialize the structure */` |
|       97 | 11955 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 11956 | `			/* Compile the case expression */` |
|       97 | 11957 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|       97 | 11958 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11959 | `				return SXERR_ABORT;` |
|        - | 11960 | `			}` |
|        - | 11961 | `			/* Compile the case block */` |
|       97 | 11962 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 11963 | `			/* Insert in the switch container */` |
|       97 | 11964 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|       97 | 11965 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 11966 | `				return SXERR_ABORT;` |
|       97 | 11967 | `			}else if( rc == SXERR_EOF ){` |
|        9 | 11968 | `				break;` |
|        - | 11969 | `			}` |
|       47 | 11970 | `		}else{` |
|        - | 11971 | `			/* Unexpected token */` |
|      ! 0 | 11972 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 11973 | `				&pGen->pIn->sData);` |
|      ! 0 | 11974 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11975 | `				return SXERR_ABORT;` |
|        - | 11976 | `			}` |
|      ! 0 | 11977 | `			break;` |
|        - | 11978 | `		}` |
|        5 | 11979 | `	}` |
|        - | 11980 | `	/* Fix all jumps now the destination is resolved */` |
|       33 | 11981 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|       33 | 11982 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11983 | `	/* Release the loop block */` |
|       33 | 11984 | `	GenStateLeaveBlock(pGen,0);` |
|       33 | 11985 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 11986 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|       33 | 11987 | `		pGen->pIn++;` |
|       14 | 11988 | `	}` |
|        - | 11989 | `	/* Statement successfully compiled */` |
|       33 | 11990 | `	return SXRET_OK;` |
|      ! 0 | 11991 | `Synchronize:` |
|        - | 11992 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 11993 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 11994 | `		pGen->pIn++;` |
|      ! 0 | 11995 | `	}` |
|      ! 0 | 11996 | `	return SXRET_OK;` |
|       19 | 11997 | `}` |
|        - | 11998 | `/*` |
|        - | 11999 | ` * Chain operators participate in a postfix member-access chain.` |
|        - | 12000 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|        - | 12001 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|        - | 12002 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|        - | 12003 | ` */` |
|        - | 12004 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|        - | 12005 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|        - | 12006 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|        - | 12007 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|        - | 12008 |  |
|        - | 12009 | `/*` |
|        - | 12010 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|        - | 12011 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|        - | 12012 | ` * patched entries from the pending set.` |
|        - | 12013 | ` */` |
| 22773244 | 12014 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|        5 | 12015 | `{` |
| 22773249 | 12016 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|        - | 12017 | `	sxu32 nTarget;` |
|        - | 12018 | `	sxu32 *aIdx;` |
|        - | 12019 | `	sxu32 i;` |
| 22773249 | 12020 | `	if( nCur <= nBaseline ){` |
| 22773153 | 12021 | `		return;` |
|        - | 12022 | `	}` |
|      100 | 12023 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      100 | 12024 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|      204 | 12025 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|      108 | 12026 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|      108 | 12027 | `		if( pInstr ){` |
|      108 | 12028 | `			pInstr->iP2 = (sxi32)nTarget;` |
|       52 | 12029 | `		}` |
|       56 | 12030 | `	}` |
|      100 | 12031 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 11386627 | 12032 | `}` |
|        - | 12033 |  |
|        - | 12034 | `/*` |
|        - | 12035 | ` * By-reference out-parameters of builtin functions.` |
|        - | 12036 | ` *` |
|        - | 12037 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|        - | 12038 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|        - | 12039 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|        - | 12040 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|        - | 12041 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|        - | 12042 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|        - | 12043 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|        - | 12044 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|        - | 12045 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|        - | 12046 | ` * creates it" behaviour).` |
|        - | 12047 | ` *` |
|        - | 12048 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|        - | 12049 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|        - | 12050 | ` */` |
|  3188406 | 12051 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|        5 | 12052 | `{` |
|        - | 12053 | `	static const struct {` |
|        - | 12054 | `		const char *zName;` |
|        - | 12055 | `		sxu32 nByte;` |
|        - | 12056 | `		sxu32 mask;` |
|        - | 12057 | `	} aByRef[] = {` |
|        - | 12058 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12059 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12060 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12061 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12062 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|        - | 12063 | `	};` |
|        - | 12064 | `	sxu32 i;` |
|  3188411 | 12065 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   838851 | 12066 | `		return 0;` |
|        - | 12067 | `	}` |
| 14096971 | 12068 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 11747528 | 12069 | `		if( pName->nByte == aByRef[i].nByte` |
|  6017231 | 12070 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      127 | 12071 | `			return aByRef[i].mask;` |
|        - | 12072 | `		}` |
|  5873708 | 12073 | `	}` |
|  2349443 | 12074 | `	return 0;` |
|  1594208 | 12075 | `}` |
|        - | 12076 | `/*` |
|        - | 12077 | ` * Recover the bare global-builtin name from a call's callee node.` |
|        - | 12078 | ` *` |
|        - | 12079 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|        - | 12080 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|        - | 12081 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|        - | 12082 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|        - | 12083 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|        - | 12084 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|        - | 12085 | ` */` |
|  3188406 | 12086 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|        5 | 12087 | `{` |
|        - | 12088 | `	SyToken *p, *pEnd;` |
|  3188411 | 12089 | `	pOut->zString = 0;` |
|  3188411 | 12090 | `	pOut->nByte = 0;` |
|  3188411 | 12091 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|      ! 0 | 12092 | `		return;` |
|        - | 12093 | `	}` |
|  3188411 | 12094 | `	p = pLeft->pStart;` |
|  3188411 | 12095 | `	pEnd = pLeft->pEnd;` |
|        - | 12096 | `	/* Optional single leading namespace separator (absolute path). */` |
|  3188411 | 12097 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|     3917 | 12098 | `		p++;` |
|     1956 | 12099 | `	}` |
|  3188411 | 12100 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   838815 | 12101 | `		return;` |
|        - | 12102 | `	}` |
|        - | 12103 | `	/* Must be a single component: nothing follows the name token. */` |
|  2349601 | 12104 | `	if( p + 1 != pEnd ){` |
|       40 | 12105 | `		return;` |
|        - | 12106 | `	}` |
|  2349565 | 12107 | `	*pOut = p->sData;` |
|  1594208 | 12108 | `}` |
|        - | 12109 | `/*` |
|        - | 12110 | ` * Generate bytecode for a given expression tree.` |
|        - | 12111 | ` * If something goes wrong while generating bytecode` |
|        - | 12112 | ` * for the expression tree (A very unlikely scenario)` |
|        - | 12113 | ` * this function takes care of generating the appropriate` |
|        - | 12114 | ` * error message.` |
|        - | 12115 | ` */` |
| 31561210 | 12116 | `static sxi32 GenStateEmitExprCode(` |
|        - | 12117 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 12118 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|        - | 12119 | `	sxi32 iFlags /* Control flags */` |
|        - | 12120 | `	)` |
|        5 | 12121 | `{` |
|        - | 12122 | `	VmInstr *pInstr;` |
|        - | 12123 | `	sxu32 nJmpIdx;` |
| 31561215 | 12124 | `	sxi32 iP1 = 0;` |
| 31561215 | 12125 | `	sxu32 iP2 = 0;` |
| 31561215 | 12126 | `	void *p3  = 0;` |
|        - | 12127 | `	sxi32 iVmOp;` |
|        - | 12128 | `	sxi32 rc;` |
| 31561215 | 12129 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 31561215 | 12130 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 31561215 | 12131 | `	sxu32 nRhsNsBase = 0;` |
| 31561215 | 12132 | `	if( pNode->xCode ){` |
|        - | 12133 | `		SyToken *pTmpIn,*pTmpEnd;` |
|        - | 12134 | `		/* Compile node */` |
| 18951703 | 12135 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 18951703 | 12136 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 18951703 | 12137 | `		RE_SWAP_DELIMITER(pGen);` |
| 18951703 | 12138 | `		return rc;` |
|        - | 12139 | `	}` |
| 12609517 | 12140 | `	if( pNode->pOp == 0 ){` |
|      ! 0 | 12141 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12142 | `			"Invalid expression node,PH7 is aborting compilation");` |
|      ! 0 | 12143 | `		return SXERR_ABORT;` |
|        - | 12144 | `	}` |
| 12609517 | 12145 | `	iVmOp = pNode->pOp->iVmOp;` |
| 12609517 | 12146 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|        - | 12147 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|        - | 12148 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|        - | 12149 | `		 * and later errors are still reported. */` |
|        3 | 12150 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12151 | `			"The (unset) cast is no longer supported");` |
|        3 | 12152 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12153 | `			return SXERR_ABORT;` |
|        - | 12154 | `		}` |
|        1 | 12155 | `	}` |
| 12609517 | 12156 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|       65 | 12157 | `		sxu32 nJmp = 0;` |
|        - | 12158 | `		sxu32 nNcNsBase;` |
|        - | 12159 | `		VmInstr *pInstrFix;` |
|        - | 12160 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|        - | 12161 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|        - | 12162 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|        - | 12163 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|        - | 12164 | `		 * stack slot carries a writable nIdx. */` |
|       65 | 12165 | `		if( pNode->pRight ){` |
|       65 | 12166 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       65 | 12167 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       65 | 12168 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12169 | `				return rc;` |
|        - | 12170 | `			}` |
|       65 | 12171 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        - | 12172 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|        - | 12173 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|        - | 12174 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|        - | 12175 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|        - | 12176 | `			 * the store, so the parent array does not need to be copied at` |
|        - | 12177 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|        - | 12178 | `			 * cascade for the actual write path stays correct. */` |
|       65 | 12179 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|       65 | 12180 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       31 | 12181 | `				pInstrFix->iP2 = 3;` |
|       14 | 12182 | `			}` |
|       31 | 12183 | `		}` |
|        - | 12184 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|       65 | 12185 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|        - | 12186 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|       65 | 12187 | `		if( pNode->pLeft ){` |
|       65 | 12188 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       65 | 12189 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|       65 | 12190 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12191 | `				return rc;` |
|        - | 12192 | `			}` |
|       65 | 12193 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       31 | 12194 | `		}` |
|        - | 12195 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|       65 | 12196 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|        - | 12197 | `		/* Patch the short-circuit jump to land after the store. */` |
|       65 | 12198 | `		if( nJmp > 0 ){` |
|       65 | 12199 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|       65 | 12200 | `			if( pInstrFix ){` |
|       65 | 12201 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|       31 | 12202 | `			}` |
|       31 | 12203 | `		}` |
|       65 | 12204 | `		return SXRET_OK;` |
|        - | 12205 | `	}` |
| 12609455 | 12206 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|        - | 12207 | `		sxu32 nJz,nJmp;` |
|        - | 12208 | `		sxu32 nTernaryNsBase;` |
|        - | 12209 | `		/* Ternary operator require special handling */` |
|        - | 12210 | `		/* Phase#1: Compile the condition */` |
|   205143 | 12211 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   205143 | 12212 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|   205143 | 12213 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12214 | `			return rc;` |
|        - | 12215 | `		}` |
|        - | 12216 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|        - | 12217 | `		 * compiling the condition must short-circuit to the end of the` |
|        - | 12218 | `		 * condition expression, not leak past the ternary. */` |
|   205143 | 12219 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   205143 | 12220 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|   205143 | 12221 | `		if( pNode->pLeft ){` |
|        - | 12222 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|        - | 12223 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|   205075 | 12224 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 12225 | `			/* Phase#3: Compile the 'then' expression  */` |
|   205075 | 12226 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   205075 | 12227 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|   205075 | 12228 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12229 | `				return rc;` |
|        - | 12230 | `			}` |
|   205075 | 12231 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   102540 | 12232 | `		}else{` |
|        - | 12233 | `			/* Elvis operator: (expr) ?: (else)` |
|        - | 12234 | `			 * Duplicate condition so original value is the 'then' result.` |
|        - | 12235 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       70 | 12236 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       70 | 12237 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 12238 | `		}` |
|        - | 12239 | `		/* Phase#4: Emit the unconditional jump */` |
|   205143 | 12240 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|        - | 12241 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|   205143 | 12242 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|   205143 | 12243 | `		if( pInstr ){` |
|   205143 | 12244 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   102569 | 12245 | `		}` |
|   205143 | 12246 | `		if( !pNode->pLeft ){` |
|        - | 12247 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       70 | 12248 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       34 | 12249 | `		}` |
|        - | 12250 | `		/* Phase#6: Compile the 'else' expression */` |
|   205143 | 12251 | `		if( pNode->pRight ){` |
|   205143 | 12252 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   205143 | 12253 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|   205143 | 12254 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12255 | `				return rc;` |
|        - | 12256 | `			}` |
|   205143 | 12257 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   102569 | 12258 | `		}` |
|   205143 | 12259 | `		if( nJmp > 0 ){` |
|        - | 12260 | `			/* Phase#7: Fix the unconditional jump */` |
|   205143 | 12261 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|   205143 | 12262 | `			if( pInstr ){` |
|   205143 | 12263 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   102569 | 12264 | `			}` |
|   102569 | 12265 | `		}` |
|        - | 12266 | `		/* All done */` |
|   205143 | 12267 | `		return SXRET_OK;` |
|        - | 12268 | `	}` |
| 12404317 | 12269 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|        - | 12270 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|        - | 12271 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|        - | 12272 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|        - | 12273 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|        - | 12274 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|        - | 12275 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|        - | 12276 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|        - | 12277 | `		sxu32 nPipeNsBase;` |
|       27 | 12278 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|       27 | 12279 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|      ! 0 | 12280 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12281 | `				"'\|>': Missing operand");` |
|      ! 0 | 12282 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 12283 | `		}` |
|        - | 12284 | `		/* Argument: the LHS value. */` |
|       27 | 12285 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 12286 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|       27 | 12287 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12288 | `			return rc;` |
|        - | 12289 | `		}` |
|       27 | 12290 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 12291 | `		/* Callable: the RHS. */` |
|       27 | 12292 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 12293 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|       27 | 12294 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12295 | `			return rc;` |
|        - | 12296 | `		}` |
|       27 | 12297 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 12298 | `		/* Invoke the callable with the single piped argument. */` |
|       27 | 12299 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       27 | 12300 | `		return SXRET_OK;` |
|        - | 12301 | `	}` |
| 12404291 | 12302 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|        - | 12303 | `	/* Generate code for the left tree */` |
| 12404291 | 12304 | `	if( pNode->pLeft ){` |
| 12392633 | 12305 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 12392633 | 12306 | `		if( iVmOp == PH7_OP_CALL ){` |
|        - | 12307 | `			ph7_expr_node **apNode;` |
|  3192611 | 12308 | `			int hasSpread = 0;` |
|  3192611 | 12309 | `			int hasNamed = 0;` |
|  3192611 | 12310 | `			int bAnySpread = 0;` |
|  3192611 | 12311 | `			sxu32 byRefMask = 0;` |
|        - | 12312 | `			sxi32 nArgs;` |
|        - | 12313 | `			sxi32 n;` |
|        - | 12314 | `			/* Recurse and generate bytecodes for function arguments */` |
|  3192611 | 12315 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3192611 | 12316 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|        - | 12317 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|        - | 12318 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|        - | 12319 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  3192611 | 12320 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|       81 | 12321 | `				bFcc = 1;` |
|       81 | 12322 | `				nArgs = 0;` |
|       40 | 12323 | `			}` |
|        - | 12324 | `			/* Validate argument order like php: no positional argument after a` |
|        - | 12325 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|        - | 12326 | `			{` |
|  3192611 | 12327 | `				int seenNamed = 0;` |
|  3192611 | 12328 | `				int seenSpread = 0;` |
|  6336895 | 12329 | `				for( n = 0; n < nArgs; ++n ){` |
|  3144291 | 12330 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|     4073 | 12331 | `						bAnySpread = 1;` |
|     4073 | 12332 | `						seenSpread = 1;` |
|     4073 | 12333 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      ! 0 | 12334 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12335 | `								"syntax error, unexpected token \"...\"");` |
|      ! 0 | 12336 | `							return SXERR_SYNTAX;` |
|        5 | 12337 | `						}` |
|  3142257 | 12338 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      289 | 12339 | `						seenNamed = 1;` |
|      289 | 12340 | `						hasNamed = 1;` |
|  3140081 | 12341 | `					}else if( seenNamed ){` |
|        3 | 12342 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12343 | `							"Cannot use positional argument after named argument");` |
|        3 | 12344 | `						return SXERR_SYNTAX;` |
|  3139937 | 12345 | `					}else if( seenSpread ){` |
|      ! 0 | 12346 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12347 | `							"Cannot use positional argument after argument unpacking");` |
|      ! 0 | 12348 | `						return SXERR_SYNTAX;` |
|        - | 12349 | `					}` |
|  1572147 | 12350 | `				}` |
|        - | 12351 | `			}` |
|        - | 12352 | `			/* Read-only load */` |
|  3192609 | 12353 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|        - | 12354 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|        - | 12355 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|        - | 12356 | `			 * objects dispatch to the right method (offsetExists for both;` |
|        - | 12357 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  3192609 | 12358 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  3192609 | 12359 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  3192604 | 12360 | `				if( pCallName->nByte == 5` |
|  1754849 | 12361 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   155717 | 12362 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  3114753 | 12363 | `				}else if( pCallName->nByte == 5` |
|  1599137 | 12364 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      101 | 12365 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       48 | 12366 | `				}` |
|        - | 12367 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|        - | 12368 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|        - | 12369 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|        - | 12370 | `				 * write back through. Skipped when spread/named args are present:` |
|        - | 12371 | `				 * the compile-time positional index no longer maps to the` |
|        - | 12372 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  3192609 | 12373 | `				if( !bAnySpread && !hasNamed ){` |
|        - | 12374 | `					SyString sBuiltin;` |
|  3188411 | 12375 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  3188411 | 12376 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  1594203 | 12377 | `				}` |
|  1596302 | 12378 | `			}` |
|  6336891 | 12379 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  3144287 | 12380 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  3144287 | 12381 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 12382 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|        - | 12383 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|        - | 12384 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|        - | 12385 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|        - | 12386 | `				 * builtin to write back through. A plain $var target is unaffected` |
|        - | 12387 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  3144287 | 12388 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|       61 | 12389 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|       61 | 12390 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|       28 | 12391 | `				}` |
|  3144287 | 12392 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  3144287 | 12393 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 12394 | `					return rc;` |
|        - | 12395 | `				}` |
|        - | 12396 | `				/* Each argument is an independent nullsafe scope. */` |
|  3144287 | 12397 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  3144287 | 12398 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|        - | 12399 | `					/* Emit spread opcode to unpack this array argument */` |
|     4073 | 12400 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|     4073 | 12401 | `					hasSpread = 1;` |
|     2034 | 12402 | `				}` |
|  1572146 | 12403 | `			}` |
|        - | 12404 | `			/* Total number of given arguments */` |
|  3192609 | 12405 | `			iP1 = nArgs;` |
|  3192609 | 12406 | `			iP2 = hasSpread;` |
|        - | 12407 | `			/* Build VmCallArgMap if named arguments are present.` |
|        - | 12408 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  3192609 | 12409 | `			if( hasNamed ){` |
|      178 | 12410 | `				sxu32 nStrBytes = 0;` |
|        - | 12411 | `				char *zBuf;` |
|      534 | 12412 | `				for( n = 0; n < nArgs; ++n ){` |
|      360 | 12413 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 12414 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      141 | 12415 | `					}` |
|      182 | 12416 | `				}` |
|        - | 12417 | `				{` |
|      178 | 12418 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      178 | 12419 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      174 | 12420 | `					&pGen->pVm->sAllocator, mapSize);` |
|      178 | 12421 | `				if( pMap ){` |
|      178 | 12422 | `					SyZero(pMap, mapSize);` |
|      178 | 12423 | `					pMap->bHasNamed = 1;` |
|      178 | 12424 | `					pMap->nTotal = (sxu32)nArgs;` |
|      178 | 12425 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      178 | 12426 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|      534 | 12427 | `					for( n = 0; n < nArgs; ++n ){` |
|      360 | 12428 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 12429 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      286 | 12430 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      286 | 12431 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      286 | 12432 | `							zBuf += nb;` |
|      141 | 12433 | `						}` |
|        - | 12434 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      182 | 12435 | `					}` |
|      178 | 12436 | `					p3 = (void *)pMap;` |
|       87 | 12437 | `				}` |
|        - | 12438 | `				}` |
|       87 | 12439 | `			}` |
|        - | 12440 | `			/* Remove stale flags now */` |
|  3192609 | 12441 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  1596302 | 12442 | `		}` |
|        - | 12443 | `		{` |
|        - | 12444 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|        - | 12445 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|        - | 12446 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|        - | 12447 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|        - | 12448 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|        - | 12449 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|        - | 12450 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|        - | 12451 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 12392631 | 12452 | `			sxi32 iLeftFlags = iFlags;` |
| 12392626 | 12453 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 10347338 | 12454 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  4151051 | 12455 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  3687155 | 12456 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   943725 | 12457 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   471860 | 12458 | `			}` |
|        - | 12459 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|        - | 12460 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|        - | 12461 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|        - | 12462 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|        - | 12463 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|        - | 12464 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|        - | 12465 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 12392626 | 12466 | `			if( pNode->pOp` |
| 17605774 | 12467 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 11409508 | 12468 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 10426338 | 12469 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  1998099 | 12470 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   999047 | 12471 | `			}` |
| 12392631 | 12472 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|        - | 12473 | `		}` |
| 12392631 | 12474 | `		if( rc != SXRET_OK ){` |
|       34 | 12475 | `			return rc;` |
|        - | 12476 | `		}` |
| 12392601 | 12477 | `		if( !bIsChainOp ){` |
|        - | 12478 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|        - | 12479 | `			 * target the end of that LHS chain, which is right here. */` |
|  5614359 | 12480 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  2807177 | 12481 | `		}` |
| 12392601 | 12482 | `		if( iVmOp == PH7_OP_CALL ){` |
|  3192609 | 12483 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  3192609 | 12484 | `			if( pInstr ){` |
|  3192609 | 12485 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  2349841 | 12486 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|        - | 12487 | `					sxu32 nQual;` |
|  2349841 | 12488 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 12489 | `					/* Prevent constant expansion but preserve the absolute flag` |
|        - | 12490 | `					 * so the later NEW handler (if any) can see it. */` |
|  2349841 | 12491 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|        - | 12492 | `					/* Namespace-qualify the function name for CALL, unless the` |
|        - | 12493 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|        - | 12494 | `					 * imports — class imports must NOT affect function` |
|        - | 12495 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|        - | 12496 | `					 * before NEW; we store the original literal index in the` |
|        - | 12497 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|        - | 12498 | `					 * the unqualified name and re-qualify with class imports. */` |
|  2349841 | 12499 | `					if( bAbsolute ){` |
|     3917 | 12500 | `						pInstr->iP2 = (sxi32)nOrig;` |
|     1961 | 12501 | `					}else{` |
|  2345929 | 12502 | `						int fromImport = 0;` |
|  2345929 | 12503 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  2345929 | 12504 | `						pInstr->iP2 = (sxi32)nQual;` |
|  2345929 | 12505 | `						if( nQual != nOrig ){` |
|        - | 12506 | `							/* Record the original literal index in the arg map` |
|        - | 12507 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|        - | 12508 | `							 * flag) so the NEW handler can recover the` |
|        - | 12509 | `							 * unqualified name and re-qualify with CLASS` |
|        - | 12510 | `							 * imports. */` |
|       77 | 12511 | `							if( p3 == 0 ){` |
|       77 | 12512 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       72 | 12513 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       77 | 12514 | `								if( pMap ){` |
|       77 | 12515 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       77 | 12516 | `									p3 = (void *)pMap;` |
|       36 | 12517 | `								}` |
|       36 | 12518 | `							}` |
|       77 | 12519 | `							if( p3 ){` |
|       77 | 12520 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       77 | 12521 | `								if( !fromImport ){` |
|        - | 12522 | `									/* Mark as namespace-qualified */` |
|       67 | 12523 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       31 | 12524 | `								}` |
|       36 | 12525 | `							}` |
|       36 | 12526 | `						}` |
|        5 | 12527 | `					}` |
|  2017691 | 12528 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|        - | 12529 | `					/* Method call,flag that */` |
|   838285 | 12530 | `					pInstr->iP2 = 1;` |
|   419140 | 12531 | `				}` |
|  1596307 | 12532 | `			}` |
| 10796299 | 12533 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|        - | 12534 | `			ph7_expr_node **apNode;` |
|        - | 12535 | `			sxi32 n;` |
|  1587549 | 12536 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 12537 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|        - | 12538 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 12539 | `			/* Recurse and generate bytecodes for array index */` |
|  1587549 | 12540 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3046791 | 12541 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  1459247 | 12542 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1459247 | 12543 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|  1459247 | 12544 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 12545 | `					return rc;` |
|        - | 12546 | `				}` |
|        - | 12547 | `				/* Each subscript index is an independent nullsafe scope. */` |
|  1459247 | 12548 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   729626 | 12549 | `			}` |
|  1587549 | 12550 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|  1459247 | 12551 | `				iP1 = 1; /* Node have an index associated with it */` |
|   729621 | 12552 | `			}` |
|  1587549 | 12553 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|        - | 12554 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|   194445 | 12555 | `				iP2 = 4;` |
|  1490329 | 12556 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 12557 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|        - | 12558 | `				 * so the trailing unset() builtin can drop the slot. */` |
|       72 | 12559 | `				iP2 = 5;` |
|  1393075 | 12560 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        - | 12561 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|        - | 12562 | `				 * short-circuit on missing keys without invoking offsetGet` |
|        - | 12563 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|       29 | 12564 | `				iP2 = 6;` |
|  1393029 | 12565 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|        - | 12566 | `				/* Create an empty entry when the desired index is not found */` |
|   190899 | 12567 | `				iP2 = 1;` |
|    95452 | 12568 | `			}` |
|  8406225 | 12569 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        - | 12570 | `			/* POP the left node */` |
|       32 | 12571 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       15 | 12572 | `		}` |
|  6196298 | 12573 | `	}` |
| 12404259 | 12574 | `	rc = SXRET_OK;` |
| 12404259 | 12575 | `	nJmpIdx = 0;` |
|        - | 12576 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|        - | 12577 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|        - | 12578 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 12404259 | 12579 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    43417 | 12580 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    43417 | 12581 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    43417 | 12582 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    43417 | 12583 | `			int isSpecial = 0;` |
|    43417 | 12584 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    20073 | 12585 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    20073 | 12586 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    20068 | 12587 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    31674 | 12588 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    15839 | 12589 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    11787 | 12590 | `					isSpecial = 1;` |
|     5891 | 12591 | `				}` |
|    15870 | 12592 | `			}` |
|    55089 | 12593 | `			pInstr->iP1 = 0;` |
|    55089 | 12594 | `			if( !isSpecial ){` |
|    19963 | 12595 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     9979 | 12596 | `			}` |
|        - | 12597 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|        - | 12598 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    31745 | 12599 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    19963 | 12600 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    19963 | 12601 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       60 | 12602 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|       62 | 12603 | `					return SXRET_OK;` |
|        - | 12604 | `				}` |
|     9950 | 12605 | `			}` |
|    15841 | 12606 | `		}` |
|    39164 | 12607 | `	}` |
|        - | 12608 | `	/* Generate code for the right tree */` |
| 12392543 | 12609 | `	if( pNode->pRight ){` |
|  6772657 | 12610 | `		if( iVmOp == PH7_OP_LAND ){` |
|        - | 12611 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   136471 | 12612 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6704424 | 12613 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|        - | 12614 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    93399 | 12615 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6589494 | 12616 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|        - | 12617 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      141 | 12618 | `			iVmOp = 0; /* No binary operator to emit */` |
|      141 | 12619 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  6542781 | 12620 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        - | 12621 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|        - | 12622 | `			 * the entire containing postfix chain to null. The jump target is` |
|        - | 12623 | `			 * patched later by the innermost non-chain ancestor (or by` |
|        - | 12624 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|        - | 12625 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|        - | 12626 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|      108 | 12627 | `			sxu32 nNsJmp = 0;` |
|      108 | 12628 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|      108 | 12629 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  6542609 | 12630 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|        - | 12631 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|        - | 12632 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|        - | 12633 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  2310207 | 12634 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  1155101 | 12635 | `		}` |
|  6772657 | 12636 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  6772657 | 12637 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  6772657 | 12638 | `		if( !bIsChainOp ){` |
|        - | 12639 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|        - | 12640 | `			 * operator instruction is emitted. */` |
|  4774621 | 12641 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  2387308 | 12642 | `		}` |
|  6772657 | 12643 | `		if( iVmOp == PH7_OP_STORE ){` |
|  2022477 | 12644 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  2022442 | 12645 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|        - | 12646 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|        - | 12647 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|        - | 12648 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|        - | 12649 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|        - | 12650 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|        - | 12651 | `				 */` |
|       91 | 12652 | `				iVmOp = 0;` |
|  2022434 | 12653 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  2022391 | 12654 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 12655 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   249143 | 12656 | `					iP2 = 1;` |
|   124574 | 12657 | `				}else{` |
|  1773253 | 12658 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 12659 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   190817 | 12660 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   190817 | 12661 | `						iP1 = pInstr->iP1;` |
|    95411 | 12662 | `					}else{` |
|  1582441 | 12663 | `						p3 = pInstr->p3;` |
|        - | 12664 | `					}` |
|        - | 12665 | `					/* POP the last dynamic load instruction */` |
|  1773253 | 12666 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 12667 | `				}` |
|  1011198 | 12668 | `			}` |
|  5761421 | 12669 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|       64 | 12670 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|       64 | 12671 | `			if( pInstr ){` |
|       64 | 12672 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 12673 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|        - | 12674 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|        - | 12675 | `					 */` |
|       19 | 12676 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|       19 | 12677 | `					iP1 = pInstr->iP1;` |
|       19 | 12678 | `					iP2 = pInstr->iP2;` |
|       19 | 12679 | `					p3  = pInstr->p3;` |
|       10 | 12680 | `				}else{` |
|       46 | 12681 | `					p3 = pInstr->p3;` |
|        - | 12682 | `				}` |
|       30 | 12683 | `			}` |
|       30 | 12684 | `		}` |
|  3386326 | 12685 | `	}` |
| 12392538 | 12686 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   242110 | 12687 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|        - | 12688 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|        - | 12689 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       30 | 12690 | `		iVmOp = 0;` |
|       13 | 12691 | `	}` |
| 12392543 | 12692 | `	if( iVmOp > 0 ){` |
| 12392265 | 12693 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    70369 | 12694 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|        - | 12695 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    11685 | 12696 | `				iP1 = 1;` |
|     5845 | 12697 | `			}` |
| 12357083 | 12698 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|        - | 12699 | `			/* Namespace-qualify the class name for NEW */ {` |
|   483915 | 12700 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   483915 | 12701 | `				VmInstr *pCallInstr = 0;` |
|   483915 | 12702 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   483667 | 12703 | `					pCallInstr = pPeek;` |
|   483667 | 12704 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   241831 | 12705 | `				}` |
|   483915 | 12706 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   483911 | 12707 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 12708 | `					sxu32 nLitForClass;` |
|   483911 | 12709 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|        - | 12710 | `					/* If the CALL handler qualified the name with FUNCTION` |
|        - | 12711 | `					 * imports, recover the original literal (recorded in the` |
|        - | 12712 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|        - | 12713 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|        - | 12714 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|        - | 12715 | `					 * with class imports. */` |
|   483911 | 12716 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       37 | 12717 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|       21 | 12718 | `					}else{` |
|   483879 | 12719 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|        - | 12720 | `					}` |
|   483911 | 12721 | `					pPeek->iP1 = 0;` |
|   483911 | 12722 | `					if( !bAbsolute ){` |
|   480003 | 12723 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   240004 | 12724 | `					}else{` |
|     3913 | 12725 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|        - | 12726 | `					}` |
|   241953 | 12727 | `				}` |
|        - | 12728 | `			}` |
|   483915 | 12729 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   483915 | 12730 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|        - | 12731 | `				VmInstr *pPrev;` |
|   483667 | 12732 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   483667 | 12733 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|        - | 12734 | `					/* Pop the call instruction, preserve named-arg map and` |
|        - | 12735 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|        - | 12736 | `					 * accumulator exactly like OP_CALL would have). */` |
|   483667 | 12737 | `					iP1 = pInstr->iP1;` |
|   483667 | 12738 | `					iP2 = pInstr->iP2;` |
|   483667 | 12739 | `					if( pInstr->p3 ){` |
|       47 | 12740 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|       21 | 12741 | `					}` |
|   483667 | 12742 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   241831 | 12743 | `				}` |
|   241836 | 12744 | `			}` |
| 12079946 | 12745 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|        - | 12746 | `			/* instanceof: right operand is a class name, not a constant.` |
|        - | 12747 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|    31301 | 12748 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    31301 | 12749 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    31301 | 12750 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    31301 | 12751 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|    31301 | 12752 | `				int isSpecialIs = 0;` |
|    31301 | 12753 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    31301 | 12754 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    31301 | 12755 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    31296 | 12756 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    31299 | 12757 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    15648 | 12758 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       12 | 12759 | `						isSpecialIs = 1;` |
|        5 | 12760 | `					}` |
|    15648 | 12761 | `				}` |
|    31301 | 12762 | `				pInstr->iP1 = 0;` |
|    31301 | 12763 | `				if( !isSpecialIs && !bAbsolute ){` |
|    31281 | 12764 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    15638 | 12765 | `				}` |
|    15653 | 12766 | `			}` |
| 11822343 | 12767 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|        - | 12768 | `			/* Prevent constant expansion for member/property names.` |
|        - | 12769 | `			 * The right child (member name) was just compiled — its LOADC` |
|        - | 12770 | `			 * should not trigger constant lookup. */` |
|  1998041 | 12771 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  1998041 | 12772 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  1997977 | 12773 | `				pInstr->iP1 = 0;` |
|   998986 | 12774 | `			}` |
|  1998041 | 12775 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|        - | 12776 | `				/* Static member access,remember that */` |
|    31701 | 12777 | `				iP1 = 1;` |
|    31701 | 12778 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    31701 | 12779 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       54 | 12780 | `					p3 = pInstr->p3;` |
|       54 | 12781 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       25 | 12782 | `				}` |
|    15848 | 12783 | `			}` |
|        - | 12784 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|        - | 12785 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|        - | 12786 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|        - | 12787 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  1998041 | 12788 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  1998041 | 12789 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       36 | 12790 | `					iP2 = PH7_MEMBER_UNSET;` |
|  1998024 | 12791 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       91 | 12792 | `					iP2 = PH7_MEMBER_ISSET;` |
|  1997964 | 12793 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       15 | 12794 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  1997914 | 12795 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|        - | 12796 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   249223 | 12797 | `					iP2 = PH7_MEMBER_WRITE;` |
|   124609 | 12798 | `				}` |
|   999018 | 12799 | `			}` |
|   999018 | 12800 | `		}` |
|        - | 12801 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|        - | 12802 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|        - | 12803 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|        - | 12804 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|        - | 12805 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 12392265 | 12806 | `		if( bFcc ){` |
|       81 | 12807 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|       81 | 12808 | `			iP2 = 0;` |
|       81 | 12809 | `			p3 = 0;` |
|       81 | 12810 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       81 | 12811 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 12812 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|        - | 12813 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|        - | 12814 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|        - | 12815 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|       37 | 12816 | `				void *pMemberName = pInstr->p3;` |
|       37 | 12817 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|       37 | 12818 | `				if( pMemberName ){` |
|        3 | 12819 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|        1 | 12820 | `				}` |
|       37 | 12821 | `				iP1 = 2;` |
|       19 | 12822 | `			}else{` |
|       45 | 12823 | `				iP1 = 1;` |
|        - | 12824 | `			}` |
|       40 | 12825 | `		}` |
|        - | 12826 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|        - | 12827 | `		 * This is the primary emit path for user-visible calls. */` |
| 12392265 | 12828 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  3676439 | 12829 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  1838217 | 12830 | `		}` |
|        - | 12831 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 12392265 | 12832 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  6196130 | 12833 | `	}` |
| 12392543 | 12834 | `	if( nJmpIdx > 0 ){` |
|        - | 12835 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   230001 | 12836 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   230001 | 12837 | `		if( pInstr ){` |
|   230001 | 12838 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   114998 | 12839 | `		}` |
|   114998 | 12840 | `	}` |
| 12392543 | 12841 | `	return rc;` |
| 15774781 | 12842 | `}` |
|        - | 12843 | `/*` |
|        - | 12844 | ` * Compile a PHP expression.` |
|        - | 12845 | ` * According to the PHP language reference manual:` |
|        - | 12846 | ` *  Expressions are the most important building stones of PHP.` |
|        - | 12847 | ` *  In PHP, almost anything you write is an expression.` |
|        - | 12848 | ` *  The simplest yet most accurate way to define an expression` |
|        - | 12849 | ` *  is "anything that has a value".` |
|        - | 12850 | ` * If something goes wrong while compiling the expression,this` |
|        - | 12851 | ` * function takes care of generating the appropriate error` |
|        - | 12852 | ` * message.` |
|        - | 12853 | ` */` |
|  7165420 | 12854 | `static sxi32 PH7_CompileExpr(` |
|        - | 12855 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 12856 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 12857 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|        - | 12858 | `	)` |
|        5 | 12859 | `{` |
|        - | 12860 | `	ph7_expr_node *pRoot;` |
|        - | 12861 | `	SySet sExprNode;` |
|        - | 12862 | `	SyToken *pEnd;` |
|        - | 12863 | `	sxi32 nExpr;` |
|        - | 12864 | `	sxi32 iNest;` |
|        - | 12865 | `	sxi32 rc;` |
|        - | 12866 | `	sxu32 nNullsafeBase;` |
|        - | 12867 | `	/* Initialize worker variables */` |
|  7165425 | 12868 | `	nExpr = 0;` |
|  7165425 | 12869 | `	pRoot = 0;` |
|        - | 12870 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|        - | 12871 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  7165425 | 12872 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  7165425 | 12873 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  7165425 | 12874 | `	SySetAlloc(&sExprNode,0x10);` |
|  7165425 | 12875 | `	rc = SXRET_OK;` |
|        - | 12876 | `	/* Delimit the expression */` |
|  7165425 | 12877 | `	pEnd = pGen->pIn;` |
|  7165425 | 12878 | `	iNest = 0;` |
| 55651471 | 12879 | `	while( pEnd < pGen->pEnd ){` |
| 53108585 | 12880 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 12881 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      693 | 12882 | `			iNest++;` |
| 53108241 | 12883 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      701 | 12884 | `			iNest--;` |
| 53107549 | 12885 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  4623115 | 12886 | `			if( iNest <= 0 ){` |
|  4622539 | 12887 | `				break;` |
|        - | 12888 | `			}` |
|      288 | 12889 | `		}` |
| 48486051 | 12890 | `		pEnd++;` |
|        5 | 12891 | `	}` |
|  7165425 | 12892 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   237763 | 12893 | `		SyToken *pEnd2 = pGen->pIn;` |
|   237763 | 12894 | `		iNest = 0;` |
|        - | 12895 | `		/* Stop at the first comma */` |
|   553691 | 12896 | `		while( pEnd2 < pEnd ){` |
|   315939 | 12897 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     7857 | 12898 | `				iNest++;` |
|   312013 | 12899 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     7857 | 12900 | `				iNest--;` |
|   304161 | 12901 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       65 | 12902 | `				if( iNest <= 0 ){` |
|        7 | 12903 | `					break;` |
|        - | 12904 | `				}` |
|       27 | 12905 | `			}` |
|   315933 | 12906 | `			pEnd2++;` |
|        5 | 12907 | `		}` |
|   237763 | 12908 | `		if( pEnd2 <pEnd ){` |
|        7 | 12909 | `			pEnd = pEnd2;` |
|        3 | 12910 | `		}` |
|   118879 | 12911 | `	}` |
|  7165425 | 12912 | `	if( pEnd > pGen->pIn ){` |
|  7165415 | 12913 | `		SyToken *pTmp = pGen->pEnd;` |
|        - | 12914 | `		/* Swap delimiter */` |
|  7165415 | 12915 | `		pGen->pEnd = pEnd;` |
|        - | 12916 | `		/* Try to get an expression tree */` |
|  7165415 | 12917 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  7165415 | 12918 | `		if( rc == SXRET_OK && pRoot ){` |
|  7165233 | 12919 | `			rc = SXRET_OK;` |
|  7165233 | 12920 | `			if( xTreeValidator ){` |
|        - | 12921 | `				/* Call the upper layer validator callback */` |
|   563719 | 12922 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   281857 | 12923 | `			}` |
|  7165233 | 12924 | `			if( rc != SXERR_ABORT ){` |
|        - | 12925 | `				/* Generate code for the given tree */` |
|  7165233 | 12926 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|        - | 12927 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|        - | 12928 | `				 * expression so they short-circuit to its end. */` |
|  7165233 | 12929 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  3582614 | 12930 | `			}` |
|  7165233 | 12931 | `			nExpr = 1;` |
|  3582614 | 12932 | `		}` |
|        - | 12933 | `		/* Release the whole tree */` |
|  7165415 | 12934 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|        - | 12935 | `		/* Synchronize token stream */` |
|  7165415 | 12936 | `		pGen->pEnd = pTmp;` |
|  7165415 | 12937 | `		pGen->pIn  = pEnd;` |
|  7165415 | 12938 | `		if( rc == SXERR_ABORT ){` |
|       13 | 12939 | `			SySetRelease(&sExprNode);` |
|       13 | 12940 | `			return SXERR_ABORT;` |
|        - | 12941 | `		}` |
|  3582700 | 12942 | `	}` |
|  7165415 | 12943 | `	SySetRelease(&sExprNode);` |
|  7165415 | 12944 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  3582715 | 12945 | `}` |
|        - | 12946 | `/*` |
|        - | 12947 | ` * Return a pointer to the node construct handler associated` |
|        - | 12948 | ` * with a given node type [i.e: string,integer,float,...].` |
|        - | 12949 | ` */` |
|  4298098 | 12950 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|        5 | 12951 | `{` |
|  4298103 | 12952 | `	if( nNodeType & PH7_TK_NUM ){` |
|        - | 12953 | `		/* Numeric literal: Either real or integer */` |
|  1289059 | 12954 | `		return PH7_CompileNumLiteral;` |
|  3009049 | 12955 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|        - | 12956 | `		/* Double quoted string */` |
|    36831 | 12957 | `		return PH7_CompileString;` |
|  2972223 | 12958 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|        - | 12959 | `		/* Single quoted string */` |
|  2972103 | 12960 | `		return PH7_CompileSimpleString;` |
|      125 | 12961 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|        - | 12962 | `		/* Heredoc */` |
|       71 | 12963 | `		return PH7_CompileHereDoc;` |
|       58 | 12964 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|        - | 12965 | `		/* Nowdoc */` |
|       51 | 12966 | `		return PH7_CompileNowDoc;` |
|        9 | 12967 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|        - | 12968 | `		/* Backtick quoted string */` |
|        6 | 12969 | `		return PH7_CompileBacktic;` |
|        - | 12970 | `	}` |
|        3 | 12971 | `	return 0;` |
|  2149054 | 12972 | `}` |
|        - | 12973 | `/*` |
|        - | 12974 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|        - | 12975 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|        - | 12976 | ` * in write context" parse error.` |
|        - | 12977 | ` */` |
|     6852 | 12978 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|        5 | 12979 | `{` |
|        - | 12980 | `	sxi32 rc;` |
|     6857 | 12981 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     6855 | 12982 | `		return SXRET_OK;` |
|        - | 12983 | `	}` |
|        5 | 12984 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|        2 | 12985 | `		pNode ? pNode->pStart->nLine : 1,` |
|        - | 12986 | `		"Can't use nullsafe operator in write context");` |
|        3 | 12987 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     3431 | 12988 | `}` |
|        - | 12989 | `/*` |
|        - | 12990 | ` * Compile an unset() statement.` |
|        - | 12991 | ` * unset($var, $arr[$key], ...);` |
|        - | 12992 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|        - | 12993 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|        - | 12994 | ` * parent array before extracting the element to unset.` |
|        - | 12995 | ` */` |
|     2930 | 12996 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|        5 | 12997 | `{` |
|     2935 | 12998 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     2935 | 12999 | `	sxu32 nIdx = 0;` |
|        - | 13000 | `	SyString sName;` |
|        - | 13001 | `	sxi32 rc;` |
|        - | 13002 | `	/* Jump the 'unset' keyword */` |
|     2935 | 13003 | `	pGen->pIn++;` |
|        - | 13004 | `	/* Save delimiter */` |
|     2935 | 13005 | `	pTmp = pGen->pEnd;` |
|        - | 13006 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     2935 | 13007 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     2935 | 13008 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 13009 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|        - | 13010 | `		SyToken *pClose;` |
|     2935 | 13011 | `		pGen->pIn++;   /* Skip '(' */` |
|     2935 | 13012 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     2935 | 13013 | `		pEnd = pClose; /* Stop at ')' */` |
|     1465 | 13014 | `	}` |
|     2935 | 13015 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|        - | 13016 | `	/* Resolve the 'unset' builtin name once */` |
|     2935 | 13017 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      379 | 13018 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      379 | 13019 | `		if( pObj == 0 ){` |
|      ! 0 | 13020 | `			return SXERR_ABORT;` |
|        - | 13021 | `		}` |
|      379 | 13022 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      379 | 13023 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      187 | 13024 | `	}` |
|        - | 13025 | `	/* Compile each comma-separated argument */` |
|     9789 | 13026 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     6859 | 13027 | `		if( pGen->pIn < pNext ){` |
|     6859 | 13028 | `			pGen->pEnd = pNext;` |
|     6859 | 13029 | `			rc = PH7_CompileExpr(&(*pGen),` |
|        - | 13030 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|        - | 13031 | `				GenStateUnsetValidator);` |
|     6859 | 13032 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13033 | `				return SXERR_ABORT;` |
|        - | 13034 | `			}` |
|     6859 | 13035 | `			if( rc != SXERR_EMPTY ){` |
|        - | 13036 | `				/* Emit call for this single argument */` |
|     6857 | 13037 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     6857 | 13038 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     6857 | 13039 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     3426 | 13040 | `			}` |
|     3427 | 13041 | `		}` |
|        - | 13042 | `		/* Jump trailing commas */` |
|    10785 | 13043 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|     3931 | 13044 | `			pNext++;` |
|        5 | 13045 | `		}` |
|     6859 | 13046 | `		pGen->pIn = pNext;` |
|        5 | 13047 | `	}` |
|        - | 13048 | `	/* Skip past the closing ')' if present */` |
|     2935 | 13049 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     2935 | 13050 | `		pGen->pIn++;` |
|     1465 | 13051 | `	}` |
|        - | 13052 | `	/* Restore token stream */` |
|     2935 | 13053 | `	pGen->pEnd = pTmp;` |
|     2935 | 13054 | `	return SXRET_OK;` |
|     1470 | 13055 | `}` |
|        - | 13056 | `/*` |
|        - | 13057 | ` * PHP Language construct table.` |
|        - | 13058 | ` */` |
|        - | 13059 | `static const LangConstruct aLangConstruct[] = {` |
|        - | 13060 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|        - | 13061 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|        - | 13062 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|        - | 13063 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|        - | 13064 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|        - | 13065 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|        - | 13066 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|        - | 13067 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|        - | 13068 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|        - | 13069 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|        - | 13070 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|        - | 13071 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|        - | 13072 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|        - | 13073 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|        - | 13074 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|        - | 13075 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|        - | 13076 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|        - | 13077 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|        - | 13078 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|        - | 13079 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|        - | 13080 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|        - | 13081 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|        - | 13082 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|        - | 13083 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|        - | 13084 | `};` |
|        - | 13085 | `/*` |
|        - | 13086 | ` * Return a pointer to the statement handler routine associated` |
|        - | 13087 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|        - | 13088 | ` */` |
|  3799152 | 13089 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|        - | 13090 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|        - | 13091 | `	SyToken *pLookahed  /* Look-ahead token */` |
|        - | 13092 | `	)` |
|        5 | 13093 | `{` |
|  3799157 | 13094 | `	sxu32 n = 0;` |
| 15474541 | 13095 | `	for(;;){` |
| 30949087 | 13096 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   246837 | 13097 | `			break;` |
|        - | 13098 | `		}` |
| 30702255 | 13099 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  3552325 | 13100 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|      ! 0 | 13101 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|      ! 0 | 13102 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|        - | 13103 | `					/* 'static' (class context),return null */` |
|      ! 0 | 13104 | `					return 0;` |
|        - | 13105 | `				}` |
|      ! 0 | 13106 | `			}` |
|  3552320 | 13107 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       14 | 13108 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       14 | 13109 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|        - | 13110 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|        3 | 13111 | `				return 0;` |
|        - | 13112 | `			}` |
|        - | 13113 | `			/* Return a pointer to the handler.` |
|        - | 13114 | `			*/` |
|  3552323 | 13115 | `			return aLangConstruct[n].xConstruct;` |
|        - | 13116 | `		}` |
| 27149935 | 13117 | `		n++;` |
|        5 | 13118 | `	}` |
|   246837 | 13119 | `	if( pLookahed ){` |
|   246837 | 13120 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    46713 | 13121 | `			return PH7_CompileClassInterface;` |
|   200129 | 13122 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   187925 | 13123 | `			return PH7_CompileClass;` |
|    12209 | 13124 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|       77 | 13125 | `			return PH7_CompileTrait;` |
|        - | 13126 | `		}` |
|        - | 13127 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|        - | 13128 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|        - | 13129 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|        - | 13130 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     6066 | 13131 | `	}` |
|        - | 13132 | `	/* Not a language construct */` |
|    12137 | 13133 | `	return 0;` |
|  1899581 | 13134 | `}` |
|        - | 13135 | `/*` |
|        - | 13136 | ` * Check if the given keyword is in fact a PHP language construct.` |
|        - | 13137 | ` * Return TRUE on success. FALSE otheriwse.` |
|        - | 13138 | ` */` |
|    12134 | 13139 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|        5 | 13140 | `{` |
|        - | 13141 | `	int rc;` |
|    12139 | 13142 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    12139 | 13143 | `	if( rc == FALSE ){` |
|    12020 | 13144 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      366 | 13145 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|        - | 13146 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|        - | 13147 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|        - | 13148 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|        - | 13149 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|        - | 13150 | `			*/` |
|        - | 13151 | `			){` |
|    12017 | 13152 | `				rc = TRUE;` |
|     6006 | 13153 | `		}` |
|     6010 | 13154 | `	}` |
|    12139 | 13155 | `	return rc;` |
|        5 | 13156 | `}` |
|        - | 13157 | `/*` |
|        - | 13158 | ` * Compile a PHP chunk.` |
|        - | 13159 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13160 | ` * takes care of generating the appropriate error message.` |
|        - | 13161 | ` */` |
|        - | 13162 | `/*` |
|        - | 13163 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|        - | 13164 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|        - | 13165 | ` * the chunk token set it becomes the pending docblock. An existing` |
|        - | 13166 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|        - | 13167 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|        - | 13168 | ` * intervening non-declaration statements.` |
|        - | 13169 | ` */` |
|  7954426 | 13170 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|        5 | 13171 | `{` |
|  7954431 | 13172 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  7954431 | 13173 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  7954431 | 13174 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13175 | `	sxu32 nIdx, n;` |
|  7954426 | 13176 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|  1536961 | 13177 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|        - | 13178 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|        - | 13179 | `		 * indexes do not map to the sidecar */` |
|  6417477 | 13180 | `		return;` |
|        - | 13181 | `	}` |
|  1536959 | 13182 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|        - | 13183 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|        - | 13184 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|  1536959 | 13185 | `	SySetReset(&pGen->aPendingAttrs);` |
|  4612101 | 13186 | `	for( n = 0 ; n < nT ; n++ ){` |
|  3075147 | 13187 | `		if( aT[n].nTokIdx != nIdx ){` |
|  3067223 | 13188 | `			continue;` |
|        - | 13189 | `		}` |
|     7929 | 13190 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       29 | 13191 | `			pGen->sPendingDoc = aT[n].sText;` |
|     7917 | 13192 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     7905 | 13193 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|     3950 | 13194 | `		}` |
|     3967 | 13195 | `	}` |
|  3977218 | 13196 | `}` |
|        - | 13197 | `/*` |
|        - | 13198 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|        - | 13199 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|        - | 13200 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|        - | 13201 | ` */` |
|  2123000 | 13202 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|        5 | 13203 | `{` |
|        - | 13204 | `	char *zDup;` |
|  2123005 | 13205 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|  2122985 | 13206 | `		return;` |
|        - | 13207 | `	}` |
|       35 | 13208 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       10 | 13209 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       25 | 13210 | `	if( zDup ){` |
|       25 | 13211 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       10 | 13212 | `	}` |
|       25 | 13213 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|  1061505 | 13214 | `}` |
|        - | 13215 | `/*` |
|        - | 13216 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|        - | 13217 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|        - | 13218 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|        - | 13219 | ` * names may point into the token text, which must outlive the raw script` |
|        - | 13220 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|        - | 13221 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|        - | 13222 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|        - | 13223 | ` */` |
|     7904 | 13224 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|        5 | 13225 | `{` |
|        - | 13226 | `	SySet *pToken;` |
|        - | 13227 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|        - | 13228 | `	char *zSpan;` |
|     7909 | 13229 | `	sxi32 rc = SXRET_OK;` |
|     7909 | 13230 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|      ! 0 | 13231 | `		return SXRET_OK;` |
|        - | 13232 | `	}` |
|    11861 | 13233 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3952 | 13234 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|     7909 | 13235 | `	if( zSpan == 0 ){` |
|      ! 0 | 13236 | `		return SXRET_OK;` |
|        - | 13237 | `	}` |
|        - | 13238 | `	/* The token set must outlive compilation too: interned operands may` |
|        - | 13239 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|        - | 13240 | `	 * the number of attribute declarations in the program. */` |
|     7909 | 13241 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|     7909 | 13242 | `	if( pToken == 0 ){` |
|      ! 0 | 13243 | `		return SXRET_OK;` |
|        - | 13244 | `	}` |
|     7909 | 13245 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     7909 | 13246 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|     7909 | 13247 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|     7909 | 13248 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|     7909 | 13249 | `	pSavedIn = pGen->pIn;` |
|     7909 | 13250 | `	pSavedEnd = pGen->pEnd;` |
|     7913 | 13251 | `	while( pIn < pEnd ){` |
|        - | 13252 | `		ph7_attribute sAttr;` |
|        - | 13253 | `		SyBlob sFQN;` |
|     7913 | 13254 | `		int bAbsolute = 0;` |
|     7913 | 13255 | `		SyZero(&sAttr,sizeof(sAttr));` |
|     7913 | 13256 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|     7913 | 13257 | `		sAttr.nLine = pIn->nLine;` |
|     7913 | 13258 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       71 | 13259 | `			bAbsolute = 1;` |
|       71 | 13260 | `			pIn++;` |
|       33 | 13261 | `		}` |
|     7913 | 13262 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7913 | 13263 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     7913 | 13264 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|     7913 | 13265 | `			pIn++;` |
|     7913 | 13266 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      ! 0 | 13267 | `				SyBlobAppend(&sFQN,"\\",1);` |
|      ! 0 | 13268 | `				pIn++;` |
|      ! 0 | 13269 | `				continue;` |
|        - | 13270 | `			}` |
|     7913 | 13271 | `			break;` |
|      ! 0 | 13272 | `		}` |
|     7913 | 13273 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|        - | 13274 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|        - | 13275 | `			 * this feature; never turn it into a new fatal) */` |
|      ! 0 | 13276 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 13277 | `			break;` |
|        - | 13278 | `		}` |
|        - | 13279 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|        - | 13280 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|        - | 13281 | `		{` |
|     7913 | 13282 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|     7913 | 13283 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|     7913 | 13284 | `			char *zDup = 0;` |
|     7913 | 13285 | `			if( !bAbsolute ){` |
|     7847 | 13286 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|     7847 | 13287 | `				if( pImp ){` |
|      ! 0 | 13288 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|      ! 0 | 13289 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|      ! 0 | 13290 | `					if( zDup ){` |
|      ! 0 | 13291 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|      ! 0 | 13292 | `					}` |
|     7847 | 13293 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 13294 | `					SyBlob sTmp;` |
|      ! 0 | 13295 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|      ! 0 | 13296 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      ! 0 | 13297 | `					SyBlobAppend(&sTmp,"\\",1);` |
|      ! 0 | 13298 | `					SyBlobAppend(&sTmp,zName,nName);` |
|      ! 0 | 13299 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      ! 0 | 13300 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|      ! 0 | 13301 | `					if( zDup ){` |
|      ! 0 | 13302 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|      ! 0 | 13303 | `					}` |
|      ! 0 | 13304 | `					SyBlobRelease(&sTmp);` |
|      ! 0 | 13305 | `				}` |
|     3921 | 13306 | `			}` |
|     7913 | 13307 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|     7913 | 13308 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|     7913 | 13309 | `				if( zDup ){` |
|     7913 | 13310 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|     3954 | 13311 | `				}` |
|     3954 | 13312 | `			}` |
|        - | 13313 | `		}` |
|     7913 | 13314 | `		SyBlobRelease(&sFQN);` |
|     7913 | 13315 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 13316 | `			SyToken *pArgsEnd;` |
|     7819 | 13317 | `			pIn++;` |
|     7819 | 13318 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|    15645 | 13319 | `			while( pIn < pArgsEnd ){` |
|     7831 | 13320 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|     7831 | 13321 | `				sxi32 iDepth = 0;` |
|        - | 13322 | `				ph7_attr_arg sArgRec;` |
|    77901 | 13323 | `				while( pArgStop < pArgsEnd ){` |
|    70089 | 13324 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       11 | 13325 | `						iDepth++;` |
|    70084 | 13326 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       11 | 13327 | `						iDepth--;` |
|    70074 | 13328 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|       15 | 13329 | `						break;` |
|        - | 13330 | `					}` |
|    70075 | 13331 | `					pArgStop++;` |
|        5 | 13332 | `				}` |
|     7831 | 13333 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|     7831 | 13334 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     7826 | 13335 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|     7814 | 13336 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       25 | 13337 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        8 | 13338 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|       17 | 13339 | `					if( zN ){` |
|       17 | 13340 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|        8 | 13341 | `					}` |
|       17 | 13342 | `					pArgStart += 2;` |
|        8 | 13343 | `				}` |
|     7831 | 13344 | `				if( pArgStart < pArgStop ){` |
|        - | 13345 | `					SySet *pInstrContainer;` |
|     7831 | 13346 | `					pGen->pIn = pArgStart;` |
|     7831 | 13347 | `					pGen->pEnd = pArgStop;` |
|     7831 | 13348 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7831 | 13349 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|     7831 | 13350 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7831 | 13351 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7831 | 13352 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7831 | 13353 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 13354 | `						pGen->pIn = pSavedIn;` |
|      ! 0 | 13355 | `						pGen->pEnd = pSavedEnd;` |
|      ! 0 | 13356 | `						return SXERR_ABORT;` |
|        - | 13357 | `					}` |
|     7831 | 13358 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|     3913 | 13359 | `				}` |
|     7831 | 13360 | `				pIn = pArgStop;` |
|     7831 | 13361 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       15 | 13362 | `					pIn++;` |
|        7 | 13363 | `				}` |
|        5 | 13364 | `			}` |
|     7819 | 13365 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|     3907 | 13366 | `		}` |
|     7913 | 13367 | `		SySetPut(pOut,(const void *)&sAttr);` |
|     7913 | 13368 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        5 | 13369 | `			pIn++;` |
|        5 | 13370 | `			continue;` |
|        - | 13371 | `		}` |
|     7909 | 13372 | `		break;` |
|      ! 0 | 13373 | `	}` |
|     7909 | 13374 | `	pGen->pIn = pSavedIn;` |
|     7909 | 13375 | `	pGen->pEnd = pSavedEnd;` |
|     7909 | 13376 | `	return SXRET_OK;` |
|     3957 | 13377 | `}` |
|        - | 13378 | `/*` |
|        - | 13379 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|        - | 13380 | ` * every recorded group into pOut and clear the pending list.` |
|        - | 13381 | ` */` |
|  2123000 | 13382 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|        5 | 13383 | `{` |
|  2123005 | 13384 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        - | 13385 | `	sxu32 n;` |
|        - | 13386 | `	sxi32 rc;` |
|  2130905 | 13387 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|     7905 | 13388 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|     7905 | 13389 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13390 | `			return SXERR_ABORT;` |
|        - | 13391 | `		}` |
|     3955 | 13392 | `	}` |
|  2123005 | 13393 | `	SySetReset(&pGen->aPendingAttrs);` |
|  2123005 | 13394 | `	return SXRET_OK;` |
|  1061505 | 13395 | `}` |
|        - | 13396 | `/*` |
|        - | 13397 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|        - | 13398 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|        - | 13399 | ` * the main token stream, so the sidecar indexes map directly.` |
|        - | 13400 | ` */` |
|   717408 | 13401 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|        5 | 13402 | `{` |
|   717413 | 13403 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   717413 | 13404 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   717413 | 13405 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13406 | `	sxu32 nIdx, n;` |
|        - | 13407 | `	sxi32 rc;` |
|   717408 | 13408 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   194505 | 13409 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   522913 | 13410 | `		return SXRET_OK;` |
|        - | 13411 | `	}` |
|   194505 | 13412 | `	nIdx = (sxu32)(pTok - pBase);` |
|   583395 | 13413 | `	for( n = 0 ; n < nT ; n++ ){` |
|   388895 | 13414 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        5 | 13415 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        5 | 13416 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13417 | `				return SXERR_ABORT;` |
|        - | 13418 | `			}` |
|        2 | 13419 | `		}` |
|   194450 | 13420 | `	}` |
|   194505 | 13421 | `	return SXRET_OK;` |
|   358709 | 13422 | `}` |
|  5849300 | 13423 | `static sxi32 GenStateCompileChunk(` |
|        - | 13424 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 13425 | `	sxi32 iFlags         /* Compile flags */` |
|        - | 13426 | `	)` |
|        5 | 13427 | `{` |
|        - | 13428 | `	ProcLangConstruct xCons;` |
|        - | 13429 | `	sxi32 rc;` |
|  5849305 | 13430 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  3340518 | 13431 | `	for(;;){` |
|  6265173 | 13432 | `		int bStmtIsDeclare = 0;` |
|  6265173 | 13433 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 13434 | `			/* No more input to process */` |
|    53349 | 13435 | `			break;` |
|        - | 13436 | `		}` |
|        - | 13437 | `		/* Bind a directly-preceding docblock to this statement */` |
|  6211829 | 13438 | `		GenStateSetPendingDoc(&(*pGen));` |
|        - | 13439 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|        - | 13440 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  6211829 | 13441 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3826389 | 13442 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  3826389 | 13443 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       47 | 13444 | `				bStmtIsDeclare = 1;` |
|       21 | 13445 | `			}` |
|  1913192 | 13446 | `		}` |
|  6211829 | 13447 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|        - | 13448 | `			/* Any non-declare top-level statement locks the strict_types` |
|        - | 13449 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   415841 | 13450 | `			pGen->bStrictTypesLocked = 1;` |
|   207918 | 13451 | `		}` |
|  6211829 | 13452 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 13453 | `			/* Compile block */` |
|     3907 | 13454 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|     3907 | 13455 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13456 | `				break;` |
|        - | 13457 | `			}` |
|     1956 | 13458 | `		}else{` |
|  6207927 | 13459 | `			xCons = 0;` |
|  6207927 | 13460 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|        - | 13461 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|        - | 13462 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|        - | 13463 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    27263 | 13464 | `				xCons = PH7_CompileClassModifiers;` |
|  6194298 | 13465 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        - | 13466 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|        - | 13467 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|       33 | 13468 | `				xCons = PH7_CompileEnum;` |
|  6180655 | 13469 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3799157 | 13470 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - | 13471 | `				/* Try to extract a language construct handler */` |
|  3799157 | 13472 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  3799157 | 13473 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|       13 | 13474 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 13475 | `						"Syntax error: Unexpected keyword '%z'",` |
|        8 | 13476 | `						&pGen->pIn->sData);` |
|        9 | 13477 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 13478 | `						break;` |
|        - | 13479 | `					}` |
|        - | 13480 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|        - | 13481 | `					 * this erroneous statement.` |
|        - | 13482 | `					 */` |
|        9 | 13483 | `					xCons = PH7_ErrorRecover;` |
|        4 | 13484 | `				}` |
|  4281065 | 13485 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    66507 | 13486 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|        - | 13487 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|      117 | 13488 | `				xCons = PH7_CompileLabel;` |
|       56 | 13489 | `			}` |
|  6207927 | 13490 | `			if( xCons == 0 ){` |
|        - | 13491 | `				/* Assume an expression an try to compile it */` |
|  2393503 | 13492 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  2393503 | 13493 | `				if(  rc != SXERR_EMPTY ){` |
|        - | 13494 | `					/* Pop l-value */` |
|  2393353 | 13495 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  1196674 | 13496 | `				}` |
|  1196754 | 13497 | `			}else{` |
|        - | 13498 | `				/* Go compile the sucker */` |
|  3814429 | 13499 | `				rc = xCons(&(*pGen));` |
|        - | 13500 | `			}` |
|  6207927 | 13501 | `			if( rc == SXERR_ABORT ){` |
|        - | 13502 | `				/* Request to abort compilation */` |
|       13 | 13503 | `				break;` |
|        - | 13504 | `			}` |
|        - | 13505 | `		}` |
|        - | 13506 | `		/* Ignore trailing semi-colons ';' */` |
| 10620839 | 13507 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  4409025 | 13508 | `			pGen->pIn++;` |
|        5 | 13509 | `		}` |
|  6211819 | 13510 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|        - | 13511 | `			/* Compile a single statement and return */` |
|  5795951 | 13512 | `			break;` |
|        - | 13513 | `		}` |
|        - | 13514 | `		/* LOOP ONE */` |
|        - | 13515 | `		/* LOOP TWO */` |
|        - | 13516 | `		/* LOOP THREE */` |
|        - | 13517 | `		/* LOOP FOUR */` |
|        5 | 13518 | `	}` |
|        - | 13519 | `	/* Return compilation status */` |
|  5849305 | 13520 | `	return rc;` |
|        5 | 13521 | `}` |
|        - | 13522 | `/*` |
|        - | 13523 | ` * Compile a Raw PHP chunk.` |
|        - | 13524 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13525 | ` * takes care of generating the appropriate error message.` |
|        - | 13526 | ` */` |
|    53356 | 13527 | `static sxi32 PH7_CompilePHP(` |
|        - | 13528 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 13529 | `	SySet *pTokenSet,     /* Token set */` |
|        - | 13530 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|        - | 13531 | `	)` |
|        5 | 13532 | `{` |
|    53361 | 13533 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|        - | 13534 | `	sxi32 rc;` |
|        - | 13535 | `	/* Reset the token set (and its trivia sidecar) */` |
|    53361 | 13536 | `	SySetReset(&(*pTokenSet));` |
|    53361 | 13537 | `	SySetReset(&pGen->aTrivia);` |
|        - | 13538 | `	/* Mark as the default token set */` |
|    53361 | 13539 | `	pGen->pTokenSet = &(*pTokenSet);` |
|        - | 13540 | `	/* Advance the stream cursor */` |
|    53361 | 13541 | `	pGen->pRawIn++;` |
|        - | 13542 | `	/* Tokenize the PHP chunk first */` |
|    53361 | 13543 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|        - | 13544 | `	/* Point to the head and tail of the token stream. */` |
|    53361 | 13545 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    53361 | 13546 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    53361 | 13547 | `	if( is_expr ){` |
|      ! 0 | 13548 | `		rc = SXERR_EMPTY;` |
|      ! 0 | 13549 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 13550 | `			/* A simple expression,compile it */` |
|      ! 0 | 13551 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 | 13552 | `		}` |
|        - | 13553 | `		/* Emit the DONE instruction */` |
|      ! 0 | 13554 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      ! 0 | 13555 | `		return SXRET_OK;` |
|        - | 13556 | `	}` |
|    53361 | 13557 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - | 13558 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - | 13559 | `		/*` |
|        - | 13560 | `		 * Shortcut syntax for the 'echo' language construct.` |
|        - | 13561 | `		 * According to the PHP reference manual:` |
|        - | 13562 | `		 *  echo() also has a shortcut syntax, where you can` |
|        - | 13563 | `		 *  immediately follow` |
|        - | 13564 | `		 *  the opening tag with an equals sign as follows:` |
|        - | 13565 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|        - | 13566 | `		 * Symisc extension:` |
|        - | 13567 | `		 *   This short syntax works with all PHP opening` |
|        - | 13568 | `		 *   tags unlike the default PHP engine that handle` |
|        - | 13569 | `		 *   only short tag.` |
|        - | 13570 | `		 */` |
|        - | 13571 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|        3 | 13572 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        3 | 13573 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        3 | 13574 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        3 | 13575 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|        3 | 13576 | `		if( rc != SXERR_EMPTY ){` |
|        3 | 13577 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 13578 | `		}` |
|        3 | 13579 | `		return SXRET_OK;` |
|        - | 13580 | `	}` |
|        - | 13581 | `	/* Compile the PHP chunk */` |
|    53359 | 13582 | `	rc = GenStateCompileChunk(pGen,0);` |
|        - | 13583 | `	/* Fix exceptions jumps */` |
|    53359 | 13584 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - | 13585 | `	/* Fix gotos now, the jump destination is resolved */` |
|    53359 | 13586 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|        3 | 13587 | `		rc = SXERR_ABORT;` |
|        1 | 13588 | `	}` |
|        - | 13589 | `	/* Reset container */` |
|    53359 | 13590 | `	SySetReset(&pGen->aGoto);` |
|    53359 | 13591 | `	SySetReset(&pGen->aLabel);` |
|    53359 | 13592 | `	SySetReset(&pGen->aNullsafeJmp);` |
|        - | 13593 | `	/* Compilation result */` |
|    53359 | 13594 | `	return rc;` |
|    26683 | 13595 | `}` |
|        - | 13596 | `/*` |
|        - | 13597 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|        - | 13598 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|        - | 13599 | ` * This is the only compile interface exported from this file.` |
|        - | 13600 | ` */` |
|    56416 | 13601 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|        - | 13602 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|        - | 13603 | `	SyString *pScript,  /* Script to compile */` |
|        - | 13604 | `	sxi32 iFlags        /* Compile flags */` |
|        - | 13605 | `	)` |
|        5 | 13606 | `{` |
|        - | 13607 | `	SySet aPhpToken,aRawToken;` |
|        - | 13608 | `	ph7_gen_state *pCodeGen;` |
|        - | 13609 | `	ph7_value *pRawObj;` |
|        - | 13610 | `	sxu32 nObjIdx;` |
|        - | 13611 | `	sxi32 nRawObj;` |
|        - | 13612 | `	int is_expr;` |
|        - | 13613 | `	sxi8 bSavedStrict;` |
|        - | 13614 | `	sxi8 bSavedStrictLocked;` |
|        - | 13615 | `	sxi32 rc;` |
|    56421 | 13616 | `	if( pScript->nByte < 1 ){` |
|        - | 13617 | `		/* Nothing to compile */` |
|      ! 0 | 13618 | `		return PH7_OK;` |
|        - | 13619 | `	}` |
|        - | 13620 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|        - | 13621 | `	 * file's flags so include/require restore them on return. */` |
|    56421 | 13622 | `	pCodeGen = &pVm->sCodeGen;` |
|    56421 | 13623 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    56421 | 13624 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    56421 | 13625 | `	pCodeGen->bStrictTypes = 0;` |
|    56421 | 13626 | `	pCodeGen->bStrictTypesLocked = 0;` |
|        - | 13627 | `	/* Initialize the tokens containers */` |
|    56421 | 13628 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    56421 | 13629 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    56421 | 13630 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    56421 | 13631 | `	is_expr = 0;` |
|    56421 | 13632 | `	if( iFlags & PH7_PHP_ONLY ){` |
|        - | 13633 | `		SyToken sTmp;` |
|        - | 13634 | `		/* PHP only: -*/` |
|    42827 | 13635 | `		sTmp.nLine = 1;` |
|    42827 | 13636 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    42827 | 13637 | `		sTmp.pUserData = 0;` |
|    42827 | 13638 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    42827 | 13639 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    42827 | 13640 | `		if( iFlags & PH7_PHP_EXPR ){` |
|        - | 13641 | `			/* A simple PHP expression */` |
|      ! 0 | 13642 | `			is_expr = 1;` |
|      ! 0 | 13643 | `		}` |
|    21416 | 13644 | `	}else{` |
|        - | 13645 | `		/* Tokenize raw text */` |
|    13599 | 13646 | `		SySetAlloc(&aRawToken,32);` |
|    13599 | 13647 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|        - | 13648 | `	}` |
|        - | 13649 | `	/* Process high-level tokens */` |
|    56421 | 13650 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    56421 | 13651 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    56421 | 13652 | `	rc = PH7_OK;` |
|    56421 | 13653 | `	if( is_expr ){` |
|        - | 13654 | `		/* Compile the expression */` |
|      ! 0 | 13655 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|      ! 0 | 13656 | `		goto cleanup;` |
|        - | 13657 | `	}` |
|    56421 | 13658 | `	nObjIdx = 0;` |
|        - | 13659 | `	/* Each compilation unit starts in the global namespace.` |
|        - | 13660 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|        - | 13661 | `	 * preventing namespace bleeding across include()d files. */` |
|    56421 | 13662 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        - | 13663 | `	/* Start the compilation process */` |
|    35011 | 13664 | `	for(;;){` |
|   123371 | 13665 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    56409 | 13666 | `			break; /* No more tokens to process */` |
|        - | 13667 | `		}` |
|    66967 | 13668 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|        - | 13669 | `			/* Compile the PHP chunk */` |
|    53361 | 13670 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    53361 | 13671 | `			if( rc == SXERR_ABORT ){` |
|       15 | 13672 | `				break;` |
|        - | 13673 | `			}` |
|    53349 | 13674 | `			continue;` |
|        - | 13675 | `		}` |
|        - | 13676 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    13611 | 13677 | `		nRawObj = 0;` |
|    27259 | 13678 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|        - | 13679 | `			/* Consume the raw chunk without any processing */` |
|    13653 | 13680 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    13653 | 13681 | `			if( pRawObj == 0 ){` |
|      ! 0 | 13682 | `				rc = SXERR_MEM;` |
|      ! 0 | 13683 | `				break;` |
|        - | 13684 | `			}` |
|        - | 13685 | `			/* Mark as constant and emit the load constant instruction */` |
|    13653 | 13686 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    13653 | 13687 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    13653 | 13688 | `			++nRawObj;` |
|    13653 | 13689 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|        5 | 13690 | `		}` |
|    13611 | 13691 | `		if( nRawObj > 0 ){` |
|        - | 13692 | `			/* Emit the consume instruction */` |
|    13611 | 13693 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     6803 | 13694 | `		}` |
|    28213 | 13695 | `	}` |
|    28208 | 13696 | `cleanup:` |
|    56421 | 13697 | `	SySetRelease(&aRawToken);` |
|    56421 | 13698 | `	SySetRelease(&aPhpToken);` |
|        - | 13699 | `	/* Restore outer file's strict_types scope */` |
|    56421 | 13700 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    56421 | 13701 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    56421 | 13702 | `	return rc;` |
|    28213 | 13703 | `}` |
|        - | 13704 | `/*` |
|        - | 13705 | ` * Utility routines.Initialize the code generator.` |
|        - | 13706 | ` */` |
|     3884 | 13707 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|        - | 13708 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 13709 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 13710 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 13711 | `	)` |
|        5 | 13712 | `{` |
|     3889 | 13713 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 13714 | `	/* Zero the structure */` |
|     3889 | 13715 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|        - | 13716 | `	/* Initial state */` |
|     3889 | 13717 | `	pGen->pVm  = &(*pVm);` |
|     3889 | 13718 | `	pGen->xErr = xErr;` |
|     3889 | 13719 | `	pGen->pErrData = pErrData;` |
|     3889 | 13720 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|     3889 | 13721 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|     3889 | 13722 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|     3889 | 13723 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3889 | 13724 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3889 | 13725 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|     3889 | 13726 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|        - | 13727 | `	/* Error log buffer */` |
|     3889 | 13728 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        - | 13729 | `	/* General purpose working buffer */` |
|     3889 | 13730 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        - | 13731 | `	/* Namespace state */` |
|     3889 | 13732 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     3889 | 13733 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     3889 | 13734 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     3889 | 13735 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 13736 | `	/* Create the global scope */` |
|     3889 | 13737 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|        - | 13738 | `	/* Point to the global scope */` |
|     3889 | 13739 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     3889 | 13740 | `	return SXRET_OK;` |
|        5 | 13741 | `}` |
|        - | 13742 | `/*` |
|        - | 13743 | ` * Utility routines. Reset the code generator to it's initial state.` |
|        - | 13744 | ` */` |
|    59920 | 13745 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|        - | 13746 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 13747 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 13748 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 13749 | `	)` |
|        5 | 13750 | `{` |
|    59925 | 13751 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 13752 | `	GenBlock *pBlock,*pParent;` |
|        - | 13753 | `	/* Reset state */` |
|    59925 | 13754 | `	SySetReset(&pGen->aLabel);` |
|    59925 | 13755 | `	SySetReset(&pGen->aGoto);` |
|    59925 | 13756 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    59925 | 13757 | `	SySetReset(&pGen->aTrivia);` |
|    59925 | 13758 | `	SySetReset(&pGen->aPendingAttrs);` |
|    59925 | 13759 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    59925 | 13760 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    59925 | 13761 | `	SyBlobRelease(&pGen->sWorker);` |
|    59925 | 13762 | `	SyBlobRelease(&pGen->sNamespace);` |
|    59925 | 13763 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    59925 | 13764 | `	SyHashRelease(&pGen->hUseImports);` |
|    59925 | 13765 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    59925 | 13766 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    59925 | 13767 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    59925 | 13768 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    59925 | 13769 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 13770 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|        - | 13771 | `	 * They intern variable names and literal strings that are referenced by` |
|        - | 13772 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|        - | 13773 | `	 * Releasing them would either leak the interned strings or require freeing` |
|        - | 13774 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|        - | 13775 | `	 * number of unique names, which is acceptable. */` |
|        - | 13776 | `	/* Point to the global scope */` |
|    59925 | 13777 | `	pBlock = pGen->pCurrent;` |
|    59925 | 13778 | `	while( pBlock->pParent != 0 ){` |
|      ! 0 | 13779 | `		pParent = pBlock->pParent;` |
|      ! 0 | 13780 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 13781 | `		pBlock = pParent;` |
|      ! 0 | 13782 | `	}` |
|    59925 | 13783 | `	pGen->xErr = xErr;` |
|    59925 | 13784 | `	pGen->pErrData = pErrData;` |
|    59925 | 13785 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    59925 | 13786 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    59925 | 13787 | `	pGen->pIn = pGen->pEnd = 0;` |
|    59925 | 13788 | `	pGen->nErr = 0;` |
|    59925 | 13789 | `	return SXRET_OK;` |
|        5 | 13790 | `}` |
|        - | 13791 | `/*` |
|        - | 13792 | ` * Generate a compile-time error message.` |
|        - | 13793 | ` * If the error count limit is reached (usually 15 error message)` |
|        - | 13794 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|        - | 13795 | ` * abort compilation immediately.` |
|        - | 13796 | ` */` |
|      652 | 13797 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|        5 | 13798 | `{` |
|      657 | 13799 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|      657 | 13800 | `	const char *zErr = "Error";` |
|        - | 13801 | `	SyString *pFile;` |
|        - | 13802 | `	va_list ap;` |
|        - | 13803 | `	sxi32 rc;` |
|        - | 13804 | `	/* Reset the working buffer */` |
|      657 | 13805 | `	SyBlobReset(pWorker);` |
|        - | 13806 | `	/* Peek the processed file path if available */` |
|      657 | 13807 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      657 | 13808 | `	if( nErrType == E_ERROR ){` |
|        - | 13809 | `		/* Increment the error counter */` |
|      543 | 13810 | `		pGen->nErr++;` |
|      543 | 13811 | `		if( pGen->nErr > 15 ){` |
|        - | 13812 | `			/* Error count limit reached */` |
|        6 | 13813 | `			if( pGen->xErr ){` |
|        6 | 13814 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|        6 | 13815 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|        6 | 13816 | `				if( pFile ){` |
|        6 | 13817 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|        2 | 13818 | `				}` |
|        6 | 13819 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|        6 | 13820 | `				if( SyBlobLength(pWorker) > 0 ){` |
|        6 | 13821 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|        2 | 13822 | `				}` |
|        2 | 13823 | `			}` |
|        - | 13824 | `			/* Abort immediately */` |
|        6 | 13825 | `			return SXERR_ABORT;` |
|        - | 13826 | `		}` |
|      267 | 13827 | `	}` |
|      653 | 13828 | `	if( pGen->xErr == 0 ){` |
|        - | 13829 | `		/* No available error consumer,return immediately */` |
|        3 | 13830 | `		return SXRET_OK;` |
|        - | 13831 | `	}` |
|      650 | 13832 | `	switch(nErrType){` |
|      536 | 13833 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|       32 | 13834 | `	case E_WARNING: zErr = "Warning";     break;` |
|       82 | 13835 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       12 | 13836 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|      ! 0 | 13837 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|      ! 0 | 13838 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|      ! 0 | 13839 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|      ! 0 | 13840 | `	default:` |
|      ! 0 | 13841 | `		break;` |
|        - | 13842 | `	}` |
|      650 | 13843 | `	rc = SXRET_OK;` |
|        - | 13844 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|      650 | 13845 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|      650 | 13846 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|      650 | 13847 | `	va_start(ap,zFormat);` |
|      650 | 13848 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      650 | 13849 | `	va_end(ap);` |
|      650 | 13850 | `	if( pFile ){` |
|      650 | 13851 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|      323 | 13852 | `	}` |
|        - | 13853 | `	/* Append a new line */` |
|      650 | 13854 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|      650 | 13855 | `	if( SyBlobLength(pWorker) > 0 ){` |
|        - | 13856 | `		/* Consume the generated error message */` |
|      650 | 13857 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|      323 | 13858 | `	}` |
|      650 | 13859 | `	return rc;` |
|      331 | 13860 | `}` |
|        - | 13861 |  |
