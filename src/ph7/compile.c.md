# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5956/7372 lines (80.79%)

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
|      96 |   122 | `			aLabel[n].bRef = TRUE;` |
|      96 |   123 | `			if( ppOut ){` |
|      96 |   124 | `				*ppOut = &aLabel[n];` |
|      46 |   125 | `			}` |
|      96 |   126 | `			return SXRET_OK;` |
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
|    3948 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   138 | `{` |
|    3953 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   11260 |   140 | `	for(;;){` |
|   22525 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    3845 |   142 | `			iCount--; /* Decrement nesting level */` |
|    3845 |   143 | `			if( iCount < 1 ){` |
|       - |   144 | `				/* Block meet with the desired criteria */` |
|    3819 |   145 | `				return pBlock;` |
|       - |   146 | `			}` |
|      13 |   147 | `		}` |
|       - |   148 | `		/* Point to the upper block */` |
|   18711 |   149 | `		pBlock = pBlock->pParent;` |
|   18711 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   151 | `			/* Forbidden */` |
|      72 |   152 | `			break;` |
|       - |   153 | `		}` |
|       5 |   154 | `	}` |
|       - |   155 | `	/* No such block */` |
|     139 |   156 | `	return 0;` |
|    1979 |   157 | `}` |
|       - |   158 | `/*` |
|       - |   159 | ` * Initialize a freshly allocated block instance.` |
|       - |   160 | ` */` |
|  866850 |   161 | `static void GenStateInitBlock(` |
|       - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   166 | `	void *pUserData      /* Upper layer private data */` |
|       - |   167 | `	)` |
|       5 |   168 | `{` |
|       - |   169 | `	/* Initialize block fields */` |
|  866855 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  866855 |   171 | `	pBlock->pUserData   = pUserData;` |
|  866855 |   172 | `	pBlock->pGen        = pGen;` |
|  866855 |   173 | `	pBlock->iFlags      = iType;` |
|  866855 |   174 | `	pBlock->pParent     = 0;` |
|  866855 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  866855 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  866855 |   177 | `}` |
|       - |   178 | `/*` |
|       - |   179 | ` * Allocate a new block instance.` |
|       - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   182 | ` * processing on failure.` |
|       - |   183 | ` */` |
|  863182 |   184 | `static sxi32 GenStateEnterBlock(` |
|       - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   190 | `	)` |
|       5 |   191 | `{` |
|       - |   192 | `	GenBlock *pBlock;` |
|       - |   193 | `	/* Allocate a new block instance */` |
|  863187 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  863187 |   195 | `	if( pBlock == 0 ){` |
|       - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   198 | `		 */` |
|     ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   200 | `		/* Abort processing immediately */` |
|     ! 0 |   201 | `		return SXERR_ABORT;` |
|       - |   202 | `	}` |
|       - |   203 | `	/* Zero the structure */` |
|  863187 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  863187 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   206 | `	/* Link to the parent block */` |
|  863187 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   208 | `	/* Mark as the current block */` |
|  863187 |   209 | `	pGen->pCurrent = pBlock;` |
|  863187 |   210 | `	if( ppBlock ){` |
|       - |   211 | `		/* Write a pointer to the new instance */` |
|  419281 |   212 | `		*ppBlock = pBlock;` |
|  209638 |   213 | `	}` |
|  863187 |   214 | `	return SXRET_OK;` |
|  431596 |   215 | `}` |
|       - |   216 | `/*` |
|       - |   217 | ` * Release block fields without freeing the whole instance.` |
|       - |   218 | ` */` |
|  863174 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   220 | `{` |
|  863179 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  863179 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  863179 |   223 | `}` |
|       - |   224 | `/*` |
|       - |   225 | ` * Release a block.` |
|       - |   226 | ` */` |
|  863174 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   228 | `{` |
|  863179 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  863179 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   231 | `	/* Free the instance */` |
|  863179 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  863179 |   233 | `}` |
|       - |   234 | `/*` |
|       - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   236 | ` */` |
|  863174 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   238 | `{` |
|  863179 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  863179 |   240 | `	if( pBlock == 0 ){` |
|       - |   241 | `		/* No more block to pop */` |
|     ! 0 |   242 | `		return SXERR_EMPTY;` |
|       - |   243 | `	}` |
|       - |   244 | `	/* Point to the upper block */` |
|  863179 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  863179 |   246 | `	if( ppBlock ){` |
|       - |   247 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   248 | `		*ppBlock = pBlock;` |
|     ! 0 |   249 | `	}else{` |
|       - |   250 | `		/* Safely release the block */` |
|  863179 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   252 | `	}` |
|  863179 |   253 | `	return SXRET_OK;` |
|  431592 |   254 | `}` |
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
|  248550 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   266 | `{` |
|       - |   267 | `	JumpFixup sJumpFix;` |
|       - |   268 | `	sxi32 rc;` |
|       - |   269 | `	/* Init the JumpFixup structure */` |
|  248555 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  248555 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   272 | `	/* Insert in the jump fixup table */` |
|  248555 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  248555 |   274 | `	return rc;` |
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
|  602218 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   288 | `{` |
|       - |   289 | `	JumpFixup *aFix;` |
|       - |   290 | `	VmInstr *pInstr;` |
|       - |   291 | `	sxu32 nFixed;` |
|       - |   292 | `	sxu32 n;` |
|       - |   293 | `	/* Point to the jump fixup table */` |
|  602223 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   295 | `	/* Fix the desired jumps */` |
| 1087933 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  485715 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   298 | `			/* Already fixed */` |
|  192011 |   299 | `			continue;` |
|       - |   300 | `		}` |
|  293709 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   302 | `			/* Not of our interest */` |
|   45161 |   303 | `			continue;` |
|       - |   304 | `		}` |
|       - |   305 | `		/* Point to the instruction to fix */` |
|  248553 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  248553 |   307 | `		if( pInstr ){` |
|  248553 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  248553 |   309 | `			nFixed++;` |
|       - |   310 | `			/* Mark as fixed */` |
|  248553 |   311 | `			aFix[n].nJumpType = -1;` |
|  124274 |   312 | `		}` |
|  124279 |   313 | `	}` |
|       - |   314 | `	/* Total number of fixed jumps */` |
|  602223 |   315 | `	return nFixed;` |
|       5 |   316 | `}` |
|       - |   317 | `/*` |
|       - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   319 | ` * The goto statement can be used to jump to another section` |
|       - |   320 | ` * in the program.` |
|       - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   322 | ` * statement for more information.` |
|       - |   323 | ` */` |
|  244372 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   325 | `{` |
|       - |   326 | `	JumpFixup *pJump,*aJumps;` |
|       - |   327 | `	Label *pLabel,*aLabel;` |
|       - |   328 | `	VmInstr *pInstr;` |
|       - |   329 | `	sxi32 rc;` |
|       - |   330 | `	sxu32 n;` |
|       - |   331 | `	/* Point to the goto table */` |
|  244377 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   333 | `	/* Fix */` |
|  244523 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|      96 |   347 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|      10 |   348 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|      10 |   349 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   350 | `				return SXERR_ABORT;` |
|       - |   351 | `			}` |
|       4 |   352 | `		}` |
|       - |   353 | `		/* Fix the jump now the destination is resolved */` |
|      96 |   354 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      96 |   355 | `		if( pInstr ){` |
|      96 |   356 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   357 | `		}` |
|      50 |   358 | `	}` |
|  244375 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  244507 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   362 | `			/* Emit a warning */` |
|      40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   365 | `		}` |
|      71 |   366 | `	}` |
|  244375 |   367 | `	return SXRET_OK;` |
|  122191 |   368 | `}` |
|       - |   369 | `/*` |
|       - |   370 | ` * Check if a given token value is installed in the literal table.` |
|       - |   371 | ` */` |
|  788722 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   373 | `{` |
|       - |   374 | `	SyHashEntry *pEntry;` |
|  788727 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  788727 |   376 | `	if( pEntry == 0 ){` |
|  355279 |   377 | `		return SXERR_NOTFOUND;` |
|       - |   378 | `	}` |
|  433453 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  433453 |   380 | `	return SXRET_OK;` |
|  394366 |   381 | `}` |
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
|  355274 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   393 | `{` |
|  355279 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  355279 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  177637 |   396 | `	}` |
|  355279 |   397 | `	return SXRET_OK;` |
|       5 |   398 | `}` |
|       - |   399 | `/*` |
|       - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   401 | ` * in the constant table.` |
|       - |   402 | ` */` |
|  128662 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   404 | `{` |
|       - |   405 | `	ph7_value *pObj;` |
|  128667 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   407 | `	/* Reserve a new constant */` |
|  128667 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  128667 |   409 | `	if( pObj == 0 ){` |
|     ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   411 | `		return 0;` |
|       - |   412 | `	}` |
|  128667 |   413 | `	*pIdx = nIdx;` |
|       - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   416 | `	 */` |
|  128667 |   417 | `	return pObj;` |
|   64336 |   418 | `}` |
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
|  492438 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   434 | `{` |
|       - |   435 | `	VmCallArgMap *pMap;` |
|  492443 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   437 | `	if( p3 == 0 ){` |
|      31 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   439 | `		if( pMap == 0 ) return 0;` |
|      31 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   441 | `		p3 = (void *)pMap;` |
|      14 |   442 | `	}` |
|      33 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   444 | `	return p3;` |
|  246224 |   445 | `}` |
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
|  129420 |   504 | `static int GenStateFindBadNumericSeparator(` |
|       - |   505 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   506 | `{` |
|  129425 |   507 | `	const char *z = pRaw->zString;` |
|  129425 |   508 | `	sxu32 n = pRaw->nByte;` |
|  129425 |   509 | `	int base = 10;` |
|       - |   510 | `	sxu32 i, start;` |
|  129425 |   511 | `	if( n < 2 ) return 0;` |
|   10771 |   512 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   513 | `		base = 16;` |
|   10736 |   514 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   515 | `		base = 2;` |
|     139 |   516 | `	}` |
|   38941 |   517 | `	for( i = 0; i < n; ++i ){` |
|   28189 |   518 | `		if( z[i] != '_' ) continue;` |
|     814 |   519 | `		if( i > 0 && i + 1 < n` |
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
|   10757 |   535 | `	return 0;` |
|   64715 |   536 | `}` |
|       - |   537 | `/*` |
|       - |   538 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   539 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   540 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   541 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   542 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   543 | ` * so callers can bail from the current construct).` |
|       - |   544 | ` */` |
|  129420 |   545 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   546 | `{` |
|  129425 |   547 | `	const char *zBad = 0;` |
|  129425 |   548 | `	sxu32 nBad = 0;` |
|       - |   549 | `	SyString sBad;` |
|       - |   550 | `	sxi32 rc;` |
|  129425 |   551 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  129411 |   552 | `		return SXRET_OK;` |
|       - |   553 | `	}` |
|      18 |   554 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   555 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   556 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   557 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   558 | `		return SXERR_ABORT;` |
|       - |   559 | `	}` |
|      18 |   560 | `	return SXERR_SYNTAX;` |
|   64715 |   561 | `}` |
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
|  129406 |   578 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   579 | `	SyMemBackend *pAlloc,` |
|       - |   580 | `	const SyString *pToken,` |
|       - |   581 | `	char *zScratch, sxu32 nScratch,` |
|       - |   582 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   583 | `{` |
|       - |   584 | `	sxu32 i, j;` |
|  129411 |   585 | `	int hasUnderscore = 0;` |
|       - |   586 | `	char *zBuf;` |
|  129411 |   587 | `	*pzAlloc = 0;` |
|  274169 |   588 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  145015 |   589 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   72384 |   590 | `	}` |
|  129411 |   591 | `	if( !hasUnderscore ){` |
|  129159 |   592 | `		SyStringDupPtr(pOut, pToken);` |
|  129159 |   593 | `		return SXRET_OK;` |
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
|   64708 |   610 | `}` |
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
|  129392 |   627 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   628 | `{` |
|  129397 |   629 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  129397 |   630 | `	sxu32 nIdx = 0;` |
|       - |   631 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  129397 |   632 | `	char *zAlloc = 0;` |
|       - |   633 | `	SyString sNum;` |
|       - |   634 | `	sxi32 rc;` |
|   64696 |   635 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  129397 |   636 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  129397 |   637 | `	if( rc != SXRET_OK ){` |
|      14 |   638 | `		return rc;` |
|       - |   639 | `	}` |
|  194078 |   640 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   64691 |   641 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  129387 |   642 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   643 | `		return SXERR_ABORT;` |
|       - |   644 | `	}` |
|  129387 |   645 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   646 | `		ph7_value *pObj;` |
|       - |   647 | `		sxi64 iValue;` |
|  128667 |   648 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  128667 |   649 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  128667 |   650 | `		if( pObj == 0 ){` |
|     ! 0 |   651 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   652 | `			return SXERR_ABORT;` |
|       - |   653 | `		}` |
|  128667 |   654 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   64336 |   655 | `	}else{` |
|       - |   656 | `		/* Real number */` |
|       - |   657 | `		ph7_value *pObj;` |
|       - |   658 | `		/* Reserve a new constant */` |
|     724 |   659 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     724 |   660 | `		if( pObj == 0 ){` |
|     ! 0 |   661 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   662 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   663 | `			return SXERR_ABORT;` |
|       - |   664 | `		}` |
|     724 |   665 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     724 |   666 | `		PH7_MemObjToReal(pObj);` |
|       - |   667 | `	}` |
|  129387 |   668 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   669 | `	/* Emit the load constant instruction */` |
|  129387 |   670 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   671 | `	/* Node successfully compiled */` |
|  129387 |   672 | `	return SXRET_OK;` |
|   64701 |   673 | `}` |
|       - |   674 | `/*` |
|       - |   675 | ` * Compile a single quoted string.` |
|       - |   676 | ` * According to the PHP language reference manual:` |
|       - |   677 | ` *` |
|       - |   678 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |   679 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |   680 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |   681 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |   682 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |   683 | ` *` |
|       - |   684 | ` */` |
|  103258 |   685 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   686 | `{` |
|  103263 |   687 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   688 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   689 | `	ph7_value *pObj;` |
|       - |   690 | `	sxu32 nIdx;` |
|  103263 |   691 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   692 | `	/* Delimit the string */` |
|  103263 |   693 | `	zIn  = pStr->zString;` |
|  103263 |   694 | `	zEnd = &zIn[pStr->nByte];` |
|  103263 |   695 | `	if( zIn >= zEnd ){` |
|       - |   696 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   697 | `		 * rather than reserving a new object each time. */` |
|    7507 |   698 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7507 |   699 | `		return SXRET_OK;` |
|       - |   700 | `	}` |
|   95761 |   701 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   702 | `		/* Already processed,emit the load constant instruction` |
|       - |   703 | `		 * and return.` |
|       - |   704 | `		 */` |
|   36973 |   705 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   36973 |   706 | `		return SXRET_OK;` |
|       - |   707 | `	}` |
|       - |   708 | `	/* Reserve a new constant */` |
|   58793 |   709 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   58793 |   710 | `	if( pObj == 0 ){` |
|     ! 0 |   711 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   712 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   713 | `		return SXERR_ABORT;` |
|       - |   714 | `	}` |
|   58793 |   715 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   716 | `	/* Compile the node */` |
|   58845 |   717 | `	for(;;){` |
|  117695 |   718 | `		if( zIn >= zEnd ){` |
|       - |   719 | `			/* End of input */` |
|   58793 |   720 | `			break;` |
|       - |   721 | `		}` |
|   58907 |   722 | `		zCur = zIn;` |
| 1006537 |   723 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  947635 |   724 | `			zIn++;` |
|       5 |   725 | `		}` |
|   58907 |   726 | `		if( zIn > zCur ){` |
|       - |   727 | `			/* Append raw contents*/` |
|   58883 |   728 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   29439 |   729 | `		}` |
|   58907 |   730 | `		zIn++;` |
|   58907 |   731 | `		if( zIn < zEnd ){` |
|     136 |   732 | `			if( zIn[0] == '\\' ){` |
|       - |   733 | `				/* A literal backslash */` |
|      23 |   734 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     125 |   735 | `			}else if( zIn[0] == '\'' ){` |
|       - |   736 | `				/* A single quote */` |
|      11 |   737 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   738 | `			}else{` |
|       - |   739 | `				/* verbatim copy */` |
|     104 |   740 | `				zIn--;` |
|     104 |   741 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     104 |   742 | `				zIn++;` |
|       - |   743 | `			}` |
|      67 |   744 | `		}` |
|       - |   745 | `		/* Advance the stream cursor */` |
|   58907 |   746 | `		zIn++;` |
|       5 |   747 | `	}` |
|       - |   748 | `	/* Emit the load constant instruction */` |
|   58793 |   749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   58793 |   750 | `	if( pStr->nByte < 1024 ){` |
|       - |   751 | `		/* Install in the literal table */` |
|   58793 |   752 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   29394 |   753 | `	}` |
|       - |   754 | `	/* Node successfully compiled */` |
|   58793 |   755 | `	return SXRET_OK;` |
|   51634 |   756 | `}` |
|       - |   757 | `/*` |
|       - |   758 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |   759 | ` *` |
|       - |   760 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |   761 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |   762 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |   763 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |   764 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |   765 | ` *` |
|       - |   766 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |   767 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |   768 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |   769 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |   770 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |   771 | ` *     whitespace.` |
|       - |   772 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |   773 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |   774 | ` */` |
|     110 |   775 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       5 |   776 | `{` |
|     115 |   777 | `	SyString *pIn = &pGen->pIn->sData;` |
|     115 |   778 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   779 | `	const char *zPrefix;` |
|       - |   780 | `	const char *z, *zEnd;` |
|       - |   781 | `	char *zBuf, *zDst;` |
|     115 |   782 | `	if( nIndent == 0 ){` |
|       - |   783 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      69 |   784 | `		*pOut = *pIn;` |
|      69 |   785 | `		return SXRET_OK;` |
|       - |   786 | `	}` |
|       - |   787 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |   788 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |   789 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |   790 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |   791 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |   792 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      48 |   793 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      48 |   794 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   795 | `		zPrefix += 2;` |
|     ! 0 |   796 | `	}else{` |
|      48 |   797 | `		zPrefix += 1;` |
|       - |   798 | `	}` |
|       - |   799 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      48 |   800 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      48 |   801 | `	if( zBuf == 0 ){` |
|     ! 0 |   802 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   803 | `		return SXERR_ABORT;` |
|       - |   804 | `	}` |
|      48 |   805 | `	zDst = zBuf;` |
|      48 |   806 | `	z = pIn->zString;` |
|      48 |   807 | `	zEnd = z + pIn->nByte;` |
|     130 |   808 | `	while( z < zEnd ){` |
|      72 |   809 | `		const char *zLine = z;` |
|       - |   810 | `		sxu32 nLine;` |
|       - |   811 | `		int bEmpty;` |
|     800 |   812 | `		while( z < zEnd && z[0] != '\n' ){` |
|     732 |   813 | `			z++;` |
|       4 |   814 | `		}` |
|      72 |   815 | `		nLine = (sxu32)(z - zLine);` |
|      72 |   816 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      72 |   817 | `		if( !bEmpty ){` |
|       - |   818 | `			sxu32 i;` |
|      68 |   819 | `			if( nLine < nIndent ){` |
|     ! 0 |   820 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   821 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   822 | `					nIndent);` |
|     ! 0 |   823 | `				return SXERR_ABORT;` |
|       - |   824 | `			}` |
|     270 |   825 | `			for( i = 0; i < nIndent; i++ ){` |
|     214 |   826 | `				if( zLine[i] != zPrefix[i] ){` |
|      11 |   827 | `					unsigned char c = (unsigned char)zLine[i];` |
|      11 |   828 | `					if( c == ' ' \|\| c == '\t' ){` |
|       6 |   829 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   830 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       4 |   831 | `					}else{` |
|       8 |   832 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   833 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   834 | `							nIndent);` |
|       - |   835 | `					}` |
|      11 |   836 | `					return SXERR_ABORT;` |
|       - |   837 | `				}` |
|     104 |   838 | `			}` |
|      57 |   839 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      57 |   840 | `			zDst += nLine - nIndent;` |
|      33 |   841 | `		}else if( nLine == 1 ){` |
|       - |   842 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |   843 | `			*zDst++ = '\r';` |
|     ! 0 |   844 | `		}` |
|      61 |   845 | `		if( z < zEnd ){` |
|      25 |   846 | `			*zDst++ = '\n';` |
|      25 |   847 | `			z++;` |
|      12 |   848 | `		}` |
|       1 |   849 | `	}` |
|      37 |   850 | `	pOut->zString = zBuf;` |
|      37 |   851 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      37 |   852 | `	return SXRET_OK;` |
|      60 |   853 | `}` |
|       - |   854 | `/*` |
|       - |   855 | ` * Compile a nowdoc string.` |
|       - |   856 | ` * According to the PHP language reference manual:` |
|       - |   857 | ` *` |
|       - |   858 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |   859 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |   860 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |   861 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |   862 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |   863 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |   864 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |   865 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |   866 | ` *  of the closing identifier.` |
|       - |   867 | ` */` |
|      46 |   868 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |   869 | `{` |
|       - |   870 | `	SyString sStripped;` |
|       - |   871 | `	SyString *pStr;` |
|       - |   872 | `	ph7_value *pObj;` |
|       - |   873 | `	sxu32 nIdx;` |
|       - |   874 | `	sxi32 rc;` |
|      50 |   875 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      50 |   876 | `	if( rc != SXRET_OK ){` |
|       6 |   877 | `		return rc;` |
|       - |   878 | `	}` |
|      44 |   879 | `	pStr = &sStripped;` |
|      44 |   880 | `	nIdx = 0; /* Prevent compiler warning */` |
|      44 |   881 | `	if( pStr->nByte <= 0 ){` |
|       - |   882 | `		/* Empty string,load NULL */` |
|       7 |   883 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |   884 | `		return SXRET_OK;` |
|       - |   885 | `	}` |
|       - |   886 | `	/* Reserve a new constant */` |
|      38 |   887 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      38 |   888 | `	if( pObj == 0 ){` |
|     ! 0 |   889 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   890 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   891 | `		return SXERR_ABORT;` |
|       - |   892 | `	}` |
|       - |   893 | `	/* No processing is done here, simply a memcpy() operation */` |
|      38 |   894 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |   895 | `	/* Emit the load constant instruction */` |
|      38 |   896 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   897 | `	/* Node successfully compiled */` |
|      38 |   898 | `	return SXRET_OK;` |
|      27 |   899 | `}` |
|       - |   900 | `/*` |
|       - |   901 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |   902 | ` * According to the PHP language reference manual` |
|       - |   903 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |   904 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |   905 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |   906 | ` *  property in a string with a minimum of effort.` |
|       - |   907 | ` *  Simple syntax` |
|       - |   908 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |   909 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |   910 | ` *   the end of the name.` |
|       - |   911 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |   912 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |   913 | ` *   as to simple variables.` |
|       - |   914 | ` *  Complex (curly) syntax` |
|       - |   915 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |   916 | ` *   of complex expressions.` |
|       - |   917 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |   918 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |   919 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |   920 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |   921 | ` */` |
|    2284 |   922 | `static sxi32 GenStateProcessStringExpression(` |
|       - |   923 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   924 | `	sxu32 nLine,         /* Line number */` |
|       - |   925 | `	const char *zIn,     /* Raw expression */` |
|       - |   926 | `	const char *zEnd     /* End of the expression */` |
|       - |   927 | `	)` |
|       5 |   928 | `{` |
|       - |   929 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |   930 | `	SySet sToken;` |
|       - |   931 | `	sxi32 rc;` |
|       - |   932 | `	/* Initialize the token set */` |
|    2289 |   933 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   934 | `	/* Preallocate some slots */` |
|    2289 |   935 | `	SySetAlloc(&sToken,0x08);` |
|       - |   936 | `	/* Tokenize the text */` |
|    2289 |   937 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   938 | `	/* Swap delimiter */` |
|    2289 |   939 | `	pTmpIn  = pGen->pIn;` |
|    2289 |   940 | `	pTmpEnd = pGen->pEnd;` |
|    2289 |   941 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2289 |   942 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   943 | `	/* Compile the expression */` |
|    2289 |   944 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   945 | `	/* Restore token stream */` |
|    2289 |   946 | `	pGen->pIn  = pTmpIn;` |
|    2289 |   947 | `	pGen->pEnd = pTmpEnd;` |
|       - |   948 | `	/* Release the token set */` |
|    2289 |   949 | `	SySetRelease(&sToken);` |
|       - |   950 | `	/* Compilation result */` |
|    2289 |   951 | `	return rc;` |
|       5 |   952 | `}` |
|       - |   953 | `/*` |
|       - |   954 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   955 | ` */` |
|   25762 |   956 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   957 | `{` |
|       - |   958 | `	ph7_value *pConstObj;` |
|   25767 |   959 | `	sxu32 nIdx = 0;` |
|       - |   960 | `	/* Reserve a new constant */` |
|   25767 |   961 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   25767 |   962 | `	if( pConstObj == 0 ){` |
|     ! 0 |   963 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   964 | `		return 0;` |
|       - |   965 | `	}` |
|   25767 |   966 | `	(*pCount)++;` |
|   25767 |   967 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   968 | `	/* Emit the load constant instruction */` |
|   25767 |   969 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   25767 |   970 | `	return pConstObj;` |
|   12886 |   971 | `}` |
|       - |   972 | `/*` |
|       - |   973 | ` * Compile a double quoted/heredoc string.` |
|       - |   974 | ` * According to the PHP language reference manual` |
|       - |   975 | ` * Heredoc` |
|       - |   976 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |   977 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |   978 | ` *  to close the quotation.` |
|       - |   979 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |   980 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |   981 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |   982 | ` *  Warning` |
|       - |   983 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |   984 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |   985 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |   986 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |   987 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |   988 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |   989 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |   990 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |   991 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |   992 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |   993 | ` * Double quoted` |
|       - |   994 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |   995 | ` *  Escaped characters Sequence 	Meaning` |
|       - |   996 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |   997 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |   998 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |   999 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |  1000 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  1001 | ` *  \\ backslash` |
|       - |  1002 | ` *  \$ dollar sign` |
|       - |  1003 | ` *  \" double-quote` |
|       - |  1004 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |  1005 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  1006 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  1007 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  1008 | ` * See string parsing for details.` |
|       - |  1009 | ` */` |
|   24268 |  1010 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1011 | `{` |
|   24273 |  1012 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1013 | `	const char *zIn,*zCur,*zEnd;` |
|   24273 |  1014 | `	ph7_value *pObj = 0;` |
|       - |  1015 | `	sxi32 iCons;` |
|       - |  1016 | `	sxi32 rc;` |
|       - |  1017 | `	/* Delimit the string */` |
|   24273 |  1018 | `	zIn  = pStr->zString;` |
|   24273 |  1019 | `	zEnd = &zIn[pStr->nByte];` |
|   24273 |  1020 | `	if( zIn >= zEnd ){` |
|       - |  1021 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1022 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1023 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1024 | `		 */` |
|     319 |  1025 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     319 |  1026 | `		return SXRET_OK;` |
|       - |  1027 | `	}` |
|   23959 |  1028 | `	zCur = 0;` |
|       - |  1029 | `	/* Compile the node */` |
|   23959 |  1030 | `	iCons = 0;` |
|   13119 |  1031 | `	for(;;){` |
|   39165 |  1032 | `		zCur = zIn;` |
|  182785 |  1033 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  145909 |  1034 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1035 | `				break;` |
|  145785 |  1036 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2164 |  1037 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1083 |  1038 | `					break;` |
|       - |  1039 | `			}` |
|  143625 |  1040 | `			zIn++;` |
|       5 |  1041 | `		}` |
|   39165 |  1042 | `		if( zIn > zCur ){` |
|   18261 |  1043 | `			if( pObj == 0 ){` |
|   17773 |  1044 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   17773 |  1045 | `				if( pObj == 0 ){` |
|     ! 0 |  1046 | `					return SXERR_ABORT;` |
|       - |  1047 | `				}` |
|    8884 |  1048 | `			}` |
|   18261 |  1049 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    9128 |  1050 | `		}` |
|   39165 |  1051 | `		if( zIn >= zEnd ){` |
|   23959 |  1052 | `			break;` |
|       - |  1053 | `		}` |
|   15211 |  1054 | `		if( zIn[0] == '\\' ){` |
|   12927 |  1055 | `			const char *zPtr = 0;` |
|       - |  1056 | `			sxu32 n;` |
|   12927 |  1057 | `			zIn++;` |
|   12927 |  1058 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1059 | `				break;` |
|       - |  1060 | `			}` |
|   12927 |  1061 | `			if( pObj == 0 ){` |
|    7999 |  1062 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    7999 |  1063 | `				if( pObj == 0 ){` |
|     ! 0 |  1064 | `					return SXERR_ABORT;` |
|       - |  1065 | `				}` |
|    3997 |  1066 | `			}` |
|   12927 |  1067 | `			n = sizeof(char); /* size of conversion */` |
|   12927 |  1068 | `			switch( zIn[0] ){` |
|       7 |  1069 | `			case '$':` |
|       - |  1070 | `				/* Dollar sign */` |
|      15 |  1071 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|      15 |  1072 | `				break;` |
|      56 |  1073 | `			case '\\':` |
|       - |  1074 | `				/* A literal backslash */` |
|     117 |  1075 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     117 |  1076 | `				break;` |
|       2 |  1077 | `			case 'a':` |
|       - |  1078 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 |  1079 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 |  1080 | `				break;` |
|       2 |  1081 | `			case 'b':` |
|       - |  1082 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 |  1083 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 |  1084 | `				break;` |
|       4 |  1085 | `			case 'f':` |
|       - |  1086 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  1087 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  1088 | `				break;` |
|    5971 |  1089 | `			case 'n':` |
|       - |  1090 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   11947 |  1091 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   11947 |  1092 | `				break;` |
|      19 |  1093 | `			case 'r':` |
|       - |  1094 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      43 |  1095 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      43 |  1096 | `				break;` |
|      24 |  1097 | `			case 't':` |
|       - |  1098 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      53 |  1099 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      53 |  1100 | `				break;` |
|       3 |  1101 | `			case 'v':` |
|       - |  1102 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  1103 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  1104 | `				break;` |
|       1 |  1105 | `			case '\'':` |
|       - |  1106 | `				/* Single quote */` |
|       3 |  1107 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 |  1108 | `				break;` |
|     108 |  1109 | `			case '"':` |
|       - |  1110 | `				/* Double quote */` |
|     221 |  1111 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     221 |  1112 | `				break;` |
|      10 |  1113 | `			case '0':` |
|       - |  1114 | `				/* NUL byte */` |
|      21 |  1115 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      21 |  1116 | `				break;` |
|     228 |  1117 | `			case 'x':` |
|     457 |  1118 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  1119 | `					int c;` |
|       - |  1120 | `					/* Hex digit */` |
|     443 |  1121 | `					c = SyHexToint(zIn[1]) << 4;` |
|     443 |  1122 | `					if( &zIn[2] < zEnd ){` |
|     443 |  1123 | `						c +=  SyHexToint(zIn[2]);` |
|     221 |  1124 | `					}` |
|       - |  1125 | `					/* Output char */` |
|     443 |  1126 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     443 |  1127 | `					n += sizeof(char) * 2;` |
|     222 |  1128 | `				}else{` |
|       - |  1129 | `					/* Output literal character  */` |
|      15 |  1130 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  1131 | `				}` |
|     457 |  1132 | `				break;` |
|      15 |  1133 | `			case 'o':` |
|      31 |  1134 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - |  1135 | `					/* Octal digit stream */` |
|       - |  1136 | `					int c;` |
|      21 |  1137 | `					c = 0;` |
|      21 |  1138 | `					zIn++;` |
|      61 |  1139 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 |  1140 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 |  1141 | `							break;` |
|       - |  1142 | `						}` |
|      41 |  1143 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 |  1144 | `					}` |
|      21 |  1145 | `					if ( c > 0 ){` |
|      15 |  1146 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 |  1147 | `					}` |
|      21 |  1148 | `					n = (sxu32)(zPtr-zIn);` |
|      11 |  1149 | `				}else{` |
|       - |  1150 | `					/* Output literal character  */` |
|      11 |  1151 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - |  1152 | `				}` |
|      31 |  1153 | `				break;` |
|      11 |  1154 | `			default:` |
|       - |  1155 | `				/* Output without a slash */` |
|      23 |  1156 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 |  1157 | `				break;` |
|       - |  1158 | `			}` |
|       - |  1159 | `			/* Advance the stream cursor */` |
|   12927 |  1160 | `			zIn += n;` |
|   12927 |  1161 | `			continue;` |
|       - |  1162 | `		}` |
|    2289 |  1163 | `		if( zIn[0] == '{' ){` |
|       - |  1164 | `			/* Curly syntax */` |
|       - |  1165 | `			const char *zExpr;` |
|     131 |  1166 | `			sxi32 iNest = 1;` |
|     131 |  1167 | `			zIn++;` |
|     131 |  1168 | `			zExpr = zIn;` |
|       - |  1169 | `			/* Synchronize with the next closing curly braces */` |
|    1359 |  1170 | `			while( zIn < zEnd ){` |
|    1359 |  1171 | `				if( zIn[0] == '{' ){` |
|       - |  1172 | `					/* Increment nesting level */` |
|       9 |  1173 | `					iNest++;` |
|    1355 |  1174 | `				}else if(zIn[0] == '}' ){` |
|       - |  1175 | `					/* Decrement nesting level */` |
|     139 |  1176 | `					iNest--;` |
|     139 |  1177 | `					if( iNest <= 0 ){` |
|     131 |  1178 | `						break;` |
|       - |  1179 | `					}` |
|       4 |  1180 | `				}` |
|    1231 |  1181 | `				zIn++;` |
|       3 |  1182 | `			}` |
|       - |  1183 | `			/* Process the expression */` |
|     131 |  1184 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     131 |  1185 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1186 | `				return SXERR_ABORT;` |
|       - |  1187 | `			}` |
|     131 |  1188 | `			if( rc != SXERR_EMPTY ){` |
|     131 |  1189 | `				++iCons;` |
|      64 |  1190 | `			}` |
|     131 |  1191 | `			if( zIn < zEnd ){` |
|       - |  1192 | `				/* Jump the trailing curly */` |
|     131 |  1193 | `				zIn++;` |
|      64 |  1194 | `			}` |
|      67 |  1195 | `		}else{` |
|       - |  1196 | `			/* Simple syntax */` |
|    2161 |  1197 | `			const char *zExpr = zIn;` |
|       - |  1198 | `			/* Assemble variable name */` |
|    1088 |  1199 | `			for(;;){` |
|       - |  1200 | `				/* Jump leading dollars */` |
|    4337 |  1201 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2161 |  1202 | `					zIn++;` |
|       5 |  1203 | `				}` |
|    1088 |  1204 | `				for(;;){` |
|   11983 |  1205 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8719 |  1206 | `						zIn++;` |
|       5 |  1207 | `					}` |
|    2181 |  1208 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1209 | `						/* UTF-8 stream */` |
|     ! 0 |  1210 | `						zIn++;` |
|     ! 0 |  1211 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1212 | `							zIn++;` |
|     ! 0 |  1213 | `						}` |
|     ! 0 |  1214 | `						continue;` |
|       - |  1215 | `					}` |
|    2181 |  1216 | `					break;` |
|     ! 0 |  1217 | `				}` |
|    2181 |  1218 | `				if( zIn >= zEnd ){` |
|     217 |  1219 | `					break;` |
|       - |  1220 | `				}` |
|    1969 |  1221 | `				if( zIn[0] == '[' ){` |
|      12 |  1222 | `					sxi32 iSquare = 1;` |
|      12 |  1223 | `					zIn++;` |
|      28 |  1224 | `					while( zIn < zEnd ){` |
|      28 |  1225 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  1226 | `							iSquare++;` |
|      28 |  1227 | `						}else if (zIn[0] == ']' ){` |
|      12 |  1228 | `							iSquare--;` |
|      12 |  1229 | `							if( iSquare <= 0 ){` |
|      12 |  1230 | `								break;` |
|       - |  1231 | `							}` |
|     ! 0 |  1232 | `						}` |
|      18 |  1233 | `						zIn++;` |
|       2 |  1234 | `					}` |
|      12 |  1235 | `					if( zIn < zEnd ){` |
|      12 |  1236 | `						zIn++;` |
|       5 |  1237 | `					}` |
|      12 |  1238 | `					break;` |
|    1959 |  1239 | `				}else if(zIn[0] == '{' ){` |
|       6 |  1240 | `					sxi32 iCurly = 1;` |
|       6 |  1241 | `					zIn++;` |
|      18 |  1242 | `					while( zIn < zEnd ){` |
|      16 |  1243 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  1244 | `							iCurly++;` |
|      16 |  1245 | `						}else if (zIn[0] == '}' ){` |
|       3 |  1246 | `							iCurly--;` |
|       3 |  1247 | `							if( iCurly <= 0 ){` |
|       3 |  1248 | `								break;` |
|       - |  1249 | `							}` |
|     ! 0 |  1250 | `						}` |
|      14 |  1251 | `						zIn++;` |
|       2 |  1252 | `					}` |
|       6 |  1253 | `					if( zIn < zEnd ){` |
|       3 |  1254 | `						zIn++;` |
|       1 |  1255 | `					}` |
|       6 |  1256 | `					break;` |
|    1955 |  1257 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1258 | `					/* Member access operator '->' */` |
|      23 |  1259 | `					zIn += 2;` |
|    1945 |  1260 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1261 | `					/* Static member access operator '::' */` |
|     ! 0 |  1262 | `					zIn += 2;` |
|     ! 0 |  1263 | `				}else{` |
|     970 |  1264 | `					break;` |
|       - |  1265 | `				}` |
|       3 |  1266 | `			}` |
|       - |  1267 | `			/* Process the expression */` |
|    2161 |  1268 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2161 |  1269 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1270 | `				return SXERR_ABORT;` |
|       - |  1271 | `			}` |
|    2161 |  1272 | `			if( rc != SXERR_EMPTY ){` |
|    2159 |  1273 | `				++iCons;` |
|    1077 |  1274 | `			}` |
|       - |  1275 | `		}` |
|       - |  1276 | `		/* Invalidate the previously used constant */` |
|    2289 |  1277 | `		pObj = 0;` |
|       5 |  1278 | `	}/*for(;;)*/` |
|   23959 |  1279 | `	if( iCons > 1 ){` |
|       - |  1280 | `		/* Concatenate all compiled constants */` |
|    1701 |  1281 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     848 |  1282 | `	}` |
|       - |  1283 | `	/* Node successfully compiled */` |
|   23959 |  1284 | `	return SXRET_OK;` |
|   12139 |  1285 | `}` |
|       - |  1286 | `/*` |
|       - |  1287 | ` * Compile a double quoted string.` |
|       - |  1288 | ` *  See the block-comment above for more information.` |
|       - |  1289 | ` */` |
|   24208 |  1290 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1291 | `{` |
|       - |  1292 | `	sxi32 rc;` |
|   24213 |  1293 | `	rc = GenStateCompileString(&(*pGen));` |
|   12104 |  1294 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1295 | `	/* Compilation result */` |
|   24213 |  1296 | `	return rc;` |
|       5 |  1297 | `}` |
|       - |  1298 | `/*` |
|       - |  1299 | ` * Compile a Heredoc string.` |
|       - |  1300 | ` *  See the block-comment above for more information.` |
|       - |  1301 | ` */` |
|      64 |  1302 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  1303 | `{` |
|       - |  1304 | `	SyString sOrig, sStripped;` |
|       - |  1305 | `	sxi32 rc;` |
|      68 |  1306 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      68 |  1307 | `	if( rc != SXRET_OK ){` |
|       6 |  1308 | `		return rc;` |
|       - |  1309 | `	}` |
|       - |  1310 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - |  1311 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - |  1312 | `	 * Restore before returning so downstream code that references pIn is` |
|       - |  1313 | `	 * unaffected, including on the error path. */` |
|      63 |  1314 | `	sOrig = pGen->pIn->sData;` |
|      63 |  1315 | `	pGen->pIn->sData = sStripped;` |
|      63 |  1316 | `	rc = GenStateCompileString(&(*pGen));` |
|      63 |  1317 | `	pGen->pIn->sData = sOrig;` |
|      30 |  1318 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      63 |  1319 | `	return rc;` |
|      36 |  1320 | `}` |
|       - |  1321 | `/*` |
|       - |  1322 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  1323 | ` *  Notes on array entries.` |
|       - |  1324 | ` *  According to the PHP language reference manual` |
|       - |  1325 | ` *  An array can be created by the array() language construct.` |
|       - |  1326 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  1327 | ` *  array(  key =>  value` |
|       - |  1328 | ` *    , ...` |
|       - |  1329 | ` *    )` |
|       - |  1330 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - |  1331 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - |  1332 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - |  1333 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - |  1334 | ` *  contain integer and string indices.` |
|       - |  1335 | ` *  A value can be any PHP type.` |
|       - |  1336 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - |  1337 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - |  1338 | ` *  is specified, that value will be overwritten.` |
|       - |  1339 | ` */` |
|   22586 |  1340 | `static sxi32 GenStateCompileArrayEntry(` |
|       - |  1341 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1342 | `	SyToken *pIn,        /* Token stream */` |
|       - |  1343 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - |  1344 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - |  1345 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - |  1346 | `	)` |
|       5 |  1347 | `{` |
|       - |  1348 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1349 | `	sxi32 rc;` |
|       - |  1350 | `	/* Swap token stream */` |
|   22591 |  1351 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1352 | `	/* Compile the expression*/` |
|   22591 |  1353 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1354 | `	/* Restore token stream */` |
|   22591 |  1355 | `	RE_SWAP_DELIMITER(pGen);` |
|   22591 |  1356 | `	return rc;` |
|       5 |  1357 | `}` |
|       - |  1358 | `/*` |
|       - |  1359 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - |  1360 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1361 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1362 | ` * error message.` |
|       - |  1363 | ` * See the routine responible of compiling the array language construct` |
|       - |  1364 | ` * for more inforation.` |
|       - |  1365 | ` */` |
|      36 |  1366 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1367 | `{` |
|      40 |  1368 | `	sxi32 rc = SXRET_OK;` |
|      40 |  1369 | `	if( pRoot->pOp ){` |
|      19 |  1370 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1371 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      16 |  1372 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1373 | `			/* Unexpected expression */` |
|      13 |  1374 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1375 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      13 |  1376 | `			if( rc != SXERR_ABORT ){` |
|      13 |  1377 | `				rc = SXERR_INVALID;` |
|       5 |  1378 | `			}` |
|       9 |  1379 | `		}` |
|      31 |  1380 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1381 | `		/* Unexpected expression */` |
|       3 |  1382 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1383 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1384 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1385 | `			rc = SXERR_INVALID;` |
|       1 |  1386 | `		}` |
|       1 |  1387 | `	}` |
|      40 |  1388 | `	return rc;` |
|       4 |  1389 | `}` |
|       - |  1390 | `/*` |
|       - |  1391 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - |  1392 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - |  1393 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - |  1394 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - |  1395 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - |  1396 | ` */` |
|   25032 |  1397 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1398 | `{` |
|   25037 |  1399 | `	SyToken *pCur = pStart;` |
|   25037 |  1400 | `	sxi32 iNest = 0;` |
|   71065 |  1401 | `	while( pCur < pEnd ){` |
|   51655 |  1402 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5623 |  1403 | `			return pCur;` |
|       - |  1404 | `		}` |
|       - |  1405 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1406 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1407 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1408 | `		 */` |
|   46037 |  1409 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      95 |  1410 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      95 |  1411 | `			SyToken *pFn = pCur;` |
|      92 |  1412 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|     ! 0 |  1413 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3 |  1414 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1415 | `				pFn = &pCur[1];` |
|     ! 0 |  1416 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1417 | `			}` |
|      95 |  1418 | `			if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1419 | `				pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1420 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1421 | `					pCur++;` |
|     ! 0 |  1422 | `				}` |
|       5 |  1423 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1424 | `					pCur++;` |
|       5 |  1425 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1426 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1427 | `					if( pCur < pEnd ){` |
|       5 |  1428 | `						pCur++;` |
|       2 |  1429 | `					}` |
|       2 |  1430 | `				}` |
|       5 |  1431 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1432 | `					pCur++;` |
|     ! 0 |  1433 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1434 | `						&& pCur->sData.nByte == 1` |
|     ! 0 |  1435 | `						&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1436 | `						pCur++;` |
|     ! 0 |  1437 | `					}` |
|     ! 0 |  1438 | `					if( pCur < pEnd` |
|     ! 0 |  1439 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1440 | `						pCur++;` |
|     ! 0 |  1441 | `					}` |
|     ! 0 |  1442 | `				}` |
|       - |  1443 | `				/* The rest of the entry is the arrow-function body — no outer` |
|       - |  1444 | `				 * key to extract. */` |
|       5 |  1445 | `				return pEnd;` |
|       - |  1446 | `			}` |
|       - |  1447 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|       - |  1448 | `			 * entry separator. Skip past the full match span. */` |
|      91 |  1449 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1450 | `				pCur++; /* past 'match' */` |
|       3 |  1451 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1452 | `					pCur++;` |
|       3 |  1453 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1454 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1455 | `					if( pCur < pEnd ){` |
|       3 |  1456 | `						pCur++;` |
|       1 |  1457 | `					}` |
|       1 |  1458 | `				}` |
|       3 |  1459 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1460 | `					pCur++;` |
|       3 |  1461 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1462 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1463 | `					if( pCur < pEnd ){` |
|       3 |  1464 | `						pCur++;` |
|       1 |  1465 | `					}` |
|       1 |  1466 | `				}` |
|       3 |  1467 | `				continue;` |
|       - |  1468 | `			}` |
|      43 |  1469 | `		}` |
|   46031 |  1470 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     402 |  1471 | `			iNest++;` |
|   45832 |  1472 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1473 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1474 | `			 * parser will shortly detect any syntax error. */` |
|     402 |  1475 | `			iNest--;` |
|     199 |  1476 | `		}` |
|   46031 |  1477 | `		pCur++;` |
|       5 |  1478 | `	}` |
|   19415 |  1479 | `	return pEnd;` |
|   12521 |  1480 | `}` |
|       - |  1481 | `/*` |
|       - |  1482 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1483 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1484 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1485 | ` */` |
|   32288 |  1486 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1487 | `{` |
|       - |  1488 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1489 | `	SyToken *pKey,*pCur;` |
|   32293 |  1490 | `	sxi32 iEmitRef = 0;` |
|   32293 |  1491 | `	sxi32 iSpread = 0;` |
|   32293 |  1492 | `	sxi32 nPair = 0;` |
|       - |  1493 | `	sxi32 rc;` |
|   32293 |  1494 | `	xValidator = 0;` |
|   26497 |  1495 | `	for(;;){` |
|       - |  1496 | `		/* Jump leading commas */` |
|   60213 |  1497 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    7219 |  1498 | `			pGen->pIn++;` |
|       5 |  1499 | `		}` |
|   52999 |  1500 | `		pCur = pGen->pIn;` |
|   52999 |  1501 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1502 | `			/* No more entry to process */` |
|   32277 |  1503 | `			break;` |
|       - |  1504 | `		}` |
|   20727 |  1505 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1506 | `			continue;` |
|       - |  1507 | `		}` |
|       - |  1508 | `		/* Compile the key if available */` |
|   20727 |  1509 | `		pKey = pCur;` |
|   20727 |  1510 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   20727 |  1511 | `		rc = SXERR_EMPTY;` |
|   20727 |  1512 | `		if( pCur < pGen->pIn ){` |
|    1675 |  1513 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1514 | `				/* Missing value */` |
|      13 |  1515 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1516 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1517 | `					return SXERR_ABORT;` |
|       - |  1518 | `				}` |
|      13 |  1519 | `				return SXRET_OK;` |
|       - |  1520 | `			}` |
|       - |  1521 | `			/* Compile the expression holding the key */` |
|    1665 |  1522 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1523 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1665 |  1524 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1525 | `				return SXERR_ABORT;` |
|       - |  1526 | `			}` |
|    1665 |  1527 | `			pCur++; /* Jump the '=>' operator */` |
|   19887 |  1528 | `		}else if( pKey == pCur ){` |
|       - |  1529 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1530 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1531 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1532 | `		}else{` |
|       - |  1533 | `			/* Reset back the cursor and point to the entry value */` |
|   19057 |  1534 | `			pCur = pKey;` |
|       - |  1535 | `		}` |
|   20717 |  1536 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1537 | `			/* No available key,load NULL */` |
|   19059 |  1538 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9527 |  1539 | `		}` |
|   20717 |  1540 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1541 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      44 |  1542 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      44 |  1543 | `			iEmitRef = 1;` |
|      44 |  1544 | `			pCur++; /* Jump the '&' token */` |
|      44 |  1545 | `			if( pCur >= pGen->pIn ){` |
|       - |  1546 | `				/* Missing value */` |
|       3 |  1547 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1548 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1549 | `					return SXERR_ABORT;` |
|       - |  1550 | `				}` |
|       3 |  1551 | `				return SXRET_OK;` |
|       - |  1552 | `			}` |
|      19 |  1553 | `		}` |
|       - |  1554 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - |  1555 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - |  1556 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - |  1557 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - |  1558 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   20715 |  1559 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   20715 |  1560 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - |  1561 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - |  1562 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - |  1563 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - |  1564 | `			 * output is engine-portable. */` |
|       6 |  1565 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - |  1566 | `				"syntax error, unexpected token \"...\"");` |
|       6 |  1567 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1568 | `				return SXERR_ABORT;` |
|       - |  1569 | `			}` |
|       6 |  1570 | `			return SXRET_OK;` |
|       - |  1571 | `		}` |
|       - |  1572 | `		/* Compile indice value */` |
|   20711 |  1573 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   20711 |  1574 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1575 | `			return SXERR_ABORT;` |
|       - |  1576 | `		}` |
|   20711 |  1577 | `		if( iSpread ){` |
|       - |  1578 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1579 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   20680 |  1580 | `		}else if( iEmitRef ){` |
|       - |  1581 | `			/* Emit the load reference instruction */` |
|      40 |  1582 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1583 | `		}` |
|   20711 |  1584 | `		xValidator = 0;` |
|   20711 |  1585 | `		iEmitRef = 0;` |
|   20711 |  1586 | `		iSpread = 0;` |
|   20711 |  1587 | `		nPair++;` |
|       5 |  1588 | `	}` |
|       - |  1589 | `	/* Emit the load map instruction */` |
|   32277 |  1590 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1591 | `	/* Node successfully compiled */` |
|   32277 |  1592 | `	return SXRET_OK;` |
|   16149 |  1593 | `}` |
|       - |  1594 | `/*` |
|       - |  1595 | ` * Compile the 'array' language construct.` |
|       - |  1596 | ` *	 According to the PHP language reference manual` |
|       - |  1597 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1598 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1599 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1600 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1601 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1602 | ` */` |
|   31152 |  1603 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1604 | `{` |
|       - |  1605 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   31157 |  1606 | `	pGen->pIn += 2;` |
|   31157 |  1607 | `	pGen->pEnd--;` |
|   15576 |  1608 | `	SXUNUSED(iCompileFlag);` |
|   31157 |  1609 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1610 | `}` |
|       - |  1611 | `/*` |
|       - |  1612 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1613 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1614 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1615 | ` */` |
|    1136 |  1616 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1617 | `{` |
|       - |  1618 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    1141 |  1619 | `	pGen->pIn++;` |
|    1141 |  1620 | `	pGen->pEnd--;` |
|     568 |  1621 | `	SXUNUSED(iCompileFlag);` |
|    1141 |  1622 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1623 | `}` |
|       - |  1624 | `/*` |
|       - |  1625 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1626 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1627 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1628 | ` * error message.` |
|       - |  1629 | ` * See the routine responible of compiling the list language construct` |
|       - |  1630 | ` * for more inforation.` |
|       - |  1631 | ` */` |
|     178 |  1632 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1633 | `{` |
|     182 |  1634 | `	sxi32 rc = SXRET_OK;` |
|     182 |  1635 | `	if( pRoot->pOp ){` |
|       4 |  1636 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       2 |  1637 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1638 | `				/* Unexpected expression */` |
|     ! 0 |  1639 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1640 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1641 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1642 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1643 | `				}` |
|       1 |  1644 | `		}` |
|     180 |  1645 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1646 | `		/* Unexpected expression */` |
|       6 |  1647 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1648 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1649 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1650 | `			rc = SXERR_INVALID;` |
|       2 |  1651 | `		}` |
|       2 |  1652 | `	}` |
|     182 |  1653 | `	return rc;` |
|       4 |  1654 | `}` |
|       - |  1655 | `/*` |
|       - |  1656 | ` * Compile the 'list' language construct.` |
|       - |  1657 | ` *  According to the PHP language reference` |
|       - |  1658 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1659 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1660 | ` *  Description` |
|       - |  1661 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1662 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1663 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1664 | ` *  Parameters` |
|       - |  1665 | ` *   $varname: A variable.` |
|       - |  1666 | ` *  Return Values` |
|       - |  1667 | ` *   The assigned array.` |
|       - |  1668 | ` */` |
|       - |  1669 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1670 | `struct NestedListEntry {` |
|       - |  1671 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1672 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1673 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1674 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1675 | `};` |
|       - |  1676 | `/*` |
|       - |  1677 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|       - |  1678 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|       - |  1679 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|       - |  1680 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|       - |  1681 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|       - |  1682 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|       - |  1683 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|       - |  1684 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|       - |  1685 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|       - |  1686 | ` */` |
|      28 |  1687 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       2 |  1688 | `{` |
|       - |  1689 | `	SyToken *pNext;` |
|       - |  1690 | `	sxi32 rc;` |
|      66 |  1691 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - |  1692 | `		SyToken *pArrow,*pTarget;` |
|       - |  1693 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      38 |  1694 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      38 |  1695 | `		pTarget = &pArrow[1];` |
|      38 |  1696 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - |  1697 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - |  1698 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 |  1699 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1700 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1701 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1702 | `		}` |
|       - |  1703 | `		/* DUP the source array (it is on the stack top) */` |
|      38 |  1704 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1705 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      38 |  1706 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      38 |  1707 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1708 | `			return SXERR_ABORT;` |
|       - |  1709 | `		}` |
|       - |  1710 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - |  1711 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|       - |  1712 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|       - |  1713 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|       - |  1714 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|       - |  1715 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|      38 |  1716 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|      38 |  1717 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      34 |  1718 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      18 |  1719 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - |  1720 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - |  1721 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - |  1722 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 |  1723 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 |  1724 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 |  1725 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 |  1726 | `			pGen->pIn = pTarget;` |
|       5 |  1727 | `			pGen->pEnd = pNext;` |
|       5 |  1728 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 |  1729 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 |  1730 | `			pGen->pIn = pSavedIn;` |
|       5 |  1731 | `			pGen->pEnd = pSavedEnd;` |
|       5 |  1732 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1733 | `				return SXERR_ABORT;` |
|       - |  1734 | `			}` |
|       5 |  1735 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 |  1736 | `		}else{` |
|       - |  1737 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|       - |  1738 | `			 * is already on the stack as the value; compiling the target appends` |
|       - |  1739 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|       - |  1740 | `			 * assignment does. */` |
|       - |  1741 | `			VmInstr *pInstr;` |
|      34 |  1742 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|      34 |  1743 | `			sxi32 iP1 = 0, iP2 = 0;` |
|      34 |  1744 | `			void *p3 = 0;` |
|      34 |  1745 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|       - |  1746 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      34 |  1747 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  1748 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1749 | `			}` |
|      34 |  1750 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      34 |  1751 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       3 |  1752 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|      33 |  1753 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 |  1754 | `					iVmOp = PH7_OP_STORE_IDX;` |
|       3 |  1755 | `					iP1 = pInstr->iP1;` |
|       3 |  1756 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 |  1757 | `				}else{` |
|      30 |  1758 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|      30 |  1759 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  1760 | `				}` |
|      16 |  1761 | `			}` |
|      34 |  1762 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - |  1763 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|       - |  1764 | `			 * source array is back on top for the next entry. */` |
|      34 |  1765 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       - |  1766 | `		}` |
|      38 |  1767 | `		pGen->pIn = &pNext[1];` |
|       2 |  1768 | `	}` |
|      30 |  1769 | `	return SXRET_OK;` |
|      16 |  1770 | `}` |
|       - |  1771 | `/*` |
|       - |  1772 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1773 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1774 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1775 | ` */` |
|     110 |  1776 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       4 |  1777 | `{` |
|       - |  1778 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1779 | `	SyToken *pNext;` |
|       - |  1780 | `	SyToken *pClassifyIn;` |
|     114 |  1781 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - |  1782 | `	sxi32 nExpr;` |
|       - |  1783 | `	sxi32 rc;` |
|       - |  1784 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - |  1785 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - |  1786 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - |  1787 | `	 * list. */` |
|     114 |  1788 | `	pClassifyIn = pGen->pIn;` |
|     322 |  1789 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     212 |  1790 | `		if( pGen->pIn >= pNext ){` |
|      13 |  1791 | `			nEmpty++;` |
|     206 |  1792 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      38 |  1793 | `			nKeyed++;` |
|      20 |  1794 | `		}else{` |
|     164 |  1795 | `			nPositional++;` |
|       - |  1796 | `		}` |
|     212 |  1797 | `		pGen->pIn = &pNext[1];` |
|       4 |  1798 | `	}` |
|     114 |  1799 | `	pGen->pIn = pClassifyIn;` |
|     114 |  1800 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 |  1801 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1802 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1803 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1804 | `	}` |
|     114 |  1805 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 |  1806 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1807 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 |  1808 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1809 | `	}` |
|     114 |  1810 | `	if( nKeyed > 0 ){` |
|      30 |  1811 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - |  1812 | `	}` |
|      86 |  1813 | `	nExpr = 0;` |
|      86 |  1814 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     258 |  1815 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     176 |  1816 | `		if( pGen->pIn < pNext ){` |
|       - |  1817 | `			/* Check for nested list() */` |
|     164 |  1818 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1819 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1820 | `				/* Record this nested list for post-processing */` |
|       3 |  1821 | `				SyToken *pListEnd = 0;` |
|       3 |  1822 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1823 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1824 | `				}` |
|       3 |  1825 | `				if( pListEnd ){` |
|       - |  1826 | `					struct NestedListEntry sEntry;` |
|       3 |  1827 | `					sEntry.nIndex = nExpr;` |
|       3 |  1828 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1829 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1830 | `					sEntry.isShort = 0;` |
|       3 |  1831 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1832 | `				}` |
|       - |  1833 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1834 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     163 |  1835 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1836 | `				/* Nested short destructuring [...] */` |
|      13 |  1837 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1838 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1839 | `				if( pBracketEnd ){` |
|       - |  1840 | `					struct NestedListEntry sEntry;` |
|      13 |  1841 | `					sEntry.nIndex = nExpr;` |
|      13 |  1842 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1843 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1844 | `					sEntry.isShort = 1;` |
|      13 |  1845 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1846 | `				}` |
|       - |  1847 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1848 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1849 | `			}else{` |
|       - |  1850 | `				/* Compile the expression holding the variable */` |
|     150 |  1851 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     150 |  1852 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1853 | `					SySetRelease(&sNested);` |
|     ! 0 |  1854 | `					return SXRET_OK;` |
|       - |  1855 | `				}` |
|       - |  1856 | `			}` |
|      84 |  1857 | `		}else{` |
|       - |  1858 | `			/* Empty entry,load NULL */` |
|      13 |  1859 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1860 | `		}` |
|     176 |  1861 | `		nExpr++;` |
|       - |  1862 | `		/* Advance the stream cursor */` |
|     176 |  1863 | `		pGen->pIn = &pNext[1];` |
|       4 |  1864 | `	}` |
|       - |  1865 | `	/* Emit the LOAD_LIST instruction */` |
|      86 |  1866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1867 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1868 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1869 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1870 | `	 */` |
|      86 |  1871 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1872 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1873 | `		sxu32 i;` |
|      27 |  1874 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1875 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1876 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1877 | `			ph7_value *pIdx;` |
|       - |  1878 | `			sxu32 nConstIdx;` |
|       - |  1879 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1880 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1881 | `			/* Push the integer index for this nested entry */` |
|      15 |  1882 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1883 | `			if( pIdx == 0 ){` |
|     ! 0 |  1884 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1885 | `				SySetRelease(&sNested);` |
|     ! 0 |  1886 | `				return SXERR_ABORT;` |
|       - |  1887 | `			}` |
|      15 |  1888 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1889 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1890 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1891 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1892 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1893 | `			 */` |
|      15 |  1894 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1895 | `			/* Recursively compile the inner list */` |
|      15 |  1896 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1897 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1898 | `			if( apNested[i].isShort ){` |
|      13 |  1899 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1900 | `			}else{` |
|       3 |  1901 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1902 | `			}` |
|      15 |  1903 | `			pGen->pIn = pSavedIn;` |
|      15 |  1904 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1905 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1906 | `				SySetRelease(&sNested);` |
|     ! 0 |  1907 | `				return SXERR_ABORT;` |
|       - |  1908 | `			}` |
|       - |  1909 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1910 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1911 | `		}` |
|       6 |  1912 | `	}` |
|      86 |  1913 | `	SySetRelease(&sNested);` |
|       - |  1914 | `	/* Node successfully compiled */` |
|      86 |  1915 | `	return SXRET_OK;` |
|      59 |  1916 | `}` |
|      34 |  1917 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1918 | `{` |
|       - |  1919 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      36 |  1920 | `	pGen->pIn += 2;` |
|      36 |  1921 | `	pGen->pEnd--;` |
|      17 |  1922 | `	SXUNUSED(iCompileFlag);` |
|      36 |  1923 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1924 | `}` |
|      76 |  1925 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  1926 | `{` |
|       - |  1927 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      80 |  1928 | `	pGen->pIn++;` |
|      80 |  1929 | `	pGen->pEnd--;` |
|      38 |  1930 | `	SXUNUSED(iCompileFlag);` |
|      80 |  1931 | `	return GenStateCompileListBody(pGen);` |
|       4 |  1932 | `}` |
|       - |  1933 | `/* Forward declarations */` |
|       - |  1934 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  1935 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  1936 | `static int GenStateIsReadonly(SyToken *pTok);` |
|       - |  1937 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|       - |  1938 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|       - |  1939 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1940 | `/*` |
|       - |  1941 | ` * Compile an annoynmous function or a closure.` |
|       - |  1942 | ` * According to the PHP language reference` |
|       - |  1943 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1944 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1945 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1946 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1947 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1948 | ` *  Example Anonymous function variable assignment example` |
|       - |  1949 | ` * <?php` |
|       - |  1950 | ` * $greet = function($name)` |
|       - |  1951 | ` * {` |
|       - |  1952 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1953 | ` * };` |
|       - |  1954 | ` * $greet('World');` |
|       - |  1955 | ` * $greet('PHP');` |
|       - |  1956 | ` * ?>` |
|       - |  1957 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1958 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1959 | ` */` |
|     294 |  1960 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1961 | `{` |
|       - |  1962 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1963 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1964 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1965 | `							  * one thread is allowed to compile the script.` |
|       - |  1966 | `						      */` |
|       - |  1967 | `	SyString sName;` |
|       - |  1968 | `	sxu32 nLen;` |
|       - |  1969 | `	sxi32 rc;` |
|     147 |  1970 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1971 |  |
|     299 |  1972 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     299 |  1973 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1974 | `		pGen->pIn++;` |
|     ! 0 |  1975 | `	}` |
|       - |  1976 | `	/* Generate a unique name */` |
|     299 |  1977 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1978 | `	/* Make sure the generated name is unique */` |
|     299 |  1979 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1980 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1981 | `	}` |
|     299 |  1982 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  1983 | `	/* Compile the lambda body */` |
|     299 |  1984 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     299 |  1985 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1986 | `		return SXERR_ABORT;` |
|       - |  1987 | `	}` |
|       - |  1988 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  1989 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  1990 | `	 * the handler wraps either in a Closure instance. */` |
|     299 |  1991 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  1992 | `	/* Node successfully compiled */` |
|     299 |  1993 | `	return SXRET_OK;` |
|     152 |  1994 | `}` |
|       - |  1995 | `/*` |
|       - |  1996 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1997 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1998 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1999 | ` */` |
|     184 |  2000 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  2001 | `	ph7_gen_state *pGen,` |
|       - |  2002 | `	ph7_vm_func *pFunc,` |
|       - |  2003 | `	const char *zName,` |
|       - |  2004 | `	sxu32 nByte,` |
|       - |  2005 | `	SyString *aShadow,` |
|       - |  2006 | `	sxu32 nShadow)` |
|       2 |  2007 | `{` |
|       - |  2008 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2009 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2010 | `	sxu32 n, nEnv;` |
|       - |  2011 | `	char *zDup;` |
|     186 |  2012 | `	if( nByte == 0 ){` |
|     ! 0 |  2013 | `		return SXRET_OK;` |
|       - |  2014 | `	}` |
|     184 |  2015 | `	if( nByte == sizeof("this")-1` |
|     100 |  2016 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2017 | `		return SXRET_OK;` |
|       - |  2018 | `	}` |
|     232 |  2019 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     172 |  2020 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     165 |  2021 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     126 |  2022 | `			return SXRET_OK;` |
|       - |  2023 | `		}` |
|      26 |  2024 | `	}` |
|      59 |  2025 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      59 |  2026 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      87 |  2027 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  2028 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  2029 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  2030 | `			return SXRET_OK;` |
|       - |  2031 | `		}` |
|      15 |  2032 | `	}` |
|      59 |  2033 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      59 |  2034 | `	if( zDup == 0 ){` |
|     ! 0 |  2035 | `		return SXERR_ABORT;` |
|       - |  2036 | `	}` |
|      59 |  2037 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      59 |  2038 | `	sEnv.iFlags = 0;` |
|      59 |  2039 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      59 |  2040 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      59 |  2041 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      59 |  2042 | `	return SXRET_OK;` |
|      94 |  2043 | `}` |
|       - |  2044 | `/*` |
|       - |  2045 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2046 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2047 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2048 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2049 | ` */` |
|      36 |  2050 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2051 | `	ph7_gen_state *pGen,` |
|       - |  2052 | `	ph7_vm_func *pFunc,` |
|       - |  2053 | `	const char *zIn,` |
|       - |  2054 | `	const char *zEnd,` |
|       - |  2055 | `	SyString *aShadow,` |
|       - |  2056 | `	sxu32 nShadow)` |
|       2 |  2057 | `{` |
|       - |  2058 | `	sxi32 rc;` |
|     302 |  2059 | `	while( zIn < zEnd ){` |
|     266 |  2060 | `		if( zIn[0] == '\\' ){` |
|       5 |  2061 | `			zIn++;` |
|       5 |  2062 | `			if( zIn < zEnd ){` |
|       5 |  2063 | `				zIn++;` |
|       2 |  2064 | `			}` |
|       5 |  2065 | `			continue;` |
|       - |  2066 | `		}` |
|     260 |  2067 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      22 |  2068 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      20 |  2069 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2070 | `			const char *zName;` |
|      22 |  2071 | `			zIn++; /* skip '$' */` |
|      22 |  2072 | `			zName = zIn;` |
|      74 |  2073 | `			while( zIn < zEnd ){` |
|      70 |  2074 | `				unsigned char c = (unsigned char)zIn[0];` |
|      70 |  2075 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2076 | `					zIn++;` |
|     ! 0 |  2077 | `					while( zIn < zEnd` |
|     ! 0 |  2078 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2079 | `						zIn++;` |
|     ! 0 |  2080 | `					}` |
|     ! 0 |  2081 | `					continue;` |
|       - |  2082 | `				}` |
|      70 |  2083 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      18 |  2084 | `					break;` |
|       - |  2085 | `				}` |
|      54 |  2086 | `				zIn++;` |
|       2 |  2087 | `			}` |
|      22 |  2088 | `			if( zIn > zName ){` |
|      32 |  2089 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      20 |  2090 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      22 |  2091 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2092 | `					return SXERR_ABORT;` |
|       - |  2093 | `				}` |
|      10 |  2094 | `			}` |
|      22 |  2095 | `			continue;` |
|       - |  2096 | `		}` |
|     242 |  2097 | `		zIn++;` |
|       2 |  2098 | `	}` |
|      38 |  2099 | `	return SXRET_OK;` |
|      20 |  2100 | `}` |
|       - |  2101 | `/*` |
|       - |  2102 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  2103 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  2104 | ` *   - plain $<id> pairs` |
|       - |  2105 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  2106 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  2107 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  2108 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  2109 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  2110 | ` *     are never mistakenly captured.` |
|       - |  2111 | ` */` |
|     192 |  2112 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2113 | `	ph7_gen_state *pGen,` |
|       - |  2114 | `	ph7_vm_func *pFunc,` |
|       - |  2115 | `	SyToken *pStart,` |
|       - |  2116 | `	SyToken *pEnd,` |
|       - |  2117 | `	SyString *aShadow,` |
|       - |  2118 | `	sxu32 nShadow)` |
|       3 |  2119 | `{` |
|     195 |  2120 | `	SyToken *pScan = pStart;` |
|       - |  2121 | `	sxi32 rc;` |
|     805 |  2122 | `	while( pScan < pEnd ){` |
|     613 |  2123 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      56 |  2124 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|      18 |  2125 | `				pScan->sData.zString,` |
|      36 |  2126 | `				pScan->sData.zString + pScan->sData.nByte,` |
|      18 |  2127 | `				aShadow,nShadow);` |
|      38 |  2128 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2129 | `				return SXERR_ABORT;` |
|       - |  2130 | `			}` |
|      38 |  2131 | `			pScan++;` |
|      38 |  2132 | `			continue;` |
|       - |  2133 | `		}` |
|     577 |  2134 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      24 |  2135 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      24 |  2136 | `			SyToken *pFnKw = pScan;` |
|      22 |  2137 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2138 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       2 |  2139 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2140 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2141 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2142 | `			}` |
|      24 |  2143 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2144 | `				SyToken *pInnerSigStart;` |
|       - |  2145 | `				SyToken *pInnerSigEnd;` |
|       - |  2146 | `				SyToken *pInnerBodyEnd;` |
|       - |  2147 | `				SyString *aInnerShadow;` |
|       - |  2148 | `				sxu32 nInnerShadow;` |
|       - |  2149 | `				sxu32 nInnerParamMax;` |
|       - |  2150 | `				SyToken *p;` |
|       - |  2151 | `				int iNestInner;` |
|      19 |  2152 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2153 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2154 | `					pScan++;` |
|     ! 0 |  2155 | `				}` |
|      19 |  2156 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2157 | `					pScan++;` |
|     ! 0 |  2158 | `					continue;` |
|       - |  2159 | `				}` |
|      19 |  2160 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2161 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2162 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2163 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2164 | `					pScan = pEnd;` |
|     ! 0 |  2165 | `					continue;` |
|       - |  2166 | `				}` |
|       - |  2167 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2168 | `				nInnerParamMax = 0;` |
|      57 |  2169 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2170 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2171 | `						nInnerParamMax++;` |
|       6 |  2172 | `					}` |
|      20 |  2173 | `				}` |
|      19 |  2174 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2175 | `					&pGen->pVm->sAllocator,` |
|      18 |  2176 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2177 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2178 | `					return SXERR_ABORT;` |
|       - |  2179 | `				}` |
|      19 |  2180 | `				nInnerShadow = 0;` |
|      25 |  2181 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2182 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2183 | `				}` |
|      57 |  2184 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2185 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2186 | `						continue;` |
|       - |  2187 | `					}` |
|      13 |  2188 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2189 | `						break;` |
|       - |  2190 | `					}` |
|      13 |  2191 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2192 | `						continue;` |
|       - |  2193 | `					}` |
|      13 |  2194 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2195 | `				}` |
|      19 |  2196 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2197 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2198 | `					pScan++;` |
|     ! 0 |  2199 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2200 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2201 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2202 | `						pScan++;` |
|     ! 0 |  2203 | `					}` |
|     ! 0 |  2204 | `					if( pScan < pEnd` |
|     ! 0 |  2205 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2206 | `						pScan++;` |
|     ! 0 |  2207 | `					}` |
|     ! 0 |  2208 | `				}` |
|      19 |  2209 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2210 | `					pScan++; /* past '=>' */` |
|       9 |  2211 | `				}` |
|      19 |  2212 | `				pInnerBodyEnd = pScan;` |
|      19 |  2213 | `				iNestInner = 0;` |
|     131 |  2214 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2215 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2216 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2217 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2218 | `						break;` |
|       - |  2219 | `					}` |
|     113 |  2220 | `					if( pInnerBodyEnd->nType &` |
|       - |  2221 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2222 | `						iNestInner++;` |
|     112 |  2223 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2224 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2225 | `						iNestInner--;` |
|       1 |  2226 | `					}` |
|     113 |  2227 | `					pInnerBodyEnd++;` |
|       1 |  2228 | `				}` |
|       - |  2229 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2230 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2231 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2232 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2233 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2234 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2235 | `				 *` |
|       - |  2236 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2237 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2238 | `				 * range after the '=' sign. */` |
|       - |  2239 | `				{` |
|      19 |  2240 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2241 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2242 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2243 | `						SyToken *pEq = 0;` |
|      13 |  2244 | `						int iNestArg = 0;` |
|      49 |  2245 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2246 | `							if( iNestArg == 0` |
|      39 |  2247 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2248 | `								break;` |
|       - |  2249 | `							}` |
|      37 |  2250 | `							if( pArgEnd->nType &` |
|       - |  2251 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2252 | `								iNestArg++;` |
|      37 |  2253 | `							}else if( pArgEnd->nType &` |
|       - |  2254 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2255 | `								iNestArg--;` |
|     ! 0 |  2256 | `							}` |
|      36 |  2257 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2258 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2259 | `								pEq = pArgEnd;` |
|       3 |  2260 | `							}` |
|      37 |  2261 | `							pArgEnd++;` |
|       1 |  2262 | `						}` |
|      13 |  2263 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2264 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2265 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2266 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2267 | `								return SXERR_ABORT;` |
|       - |  2268 | `							}` |
|       3 |  2269 | `						}` |
|      13 |  2270 | `						pArgStart = pArgEnd;` |
|      12 |  2271 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2272 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2273 | `							pArgStart++;` |
|       1 |  2274 | `						}` |
|       1 |  2275 | `					}` |
|       - |  2276 | `				}` |
|      28 |  2277 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2278 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2279 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2280 | `					return SXERR_ABORT;` |
|       - |  2281 | `				}` |
|      19 |  2282 | `				pScan = pInnerBodyEnd;` |
|      19 |  2283 | `				continue;` |
|       - |  2284 | `			}` |
|       2 |  2285 | `		}` |
|     559 |  2286 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     395 |  2287 | `			pScan++;` |
|     395 |  2288 | `			continue;` |
|       - |  2289 | `		}` |
|       - |  2290 | `		{` |
|       - |  2291 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     166 |  2292 | `			SyToken *pDollar = pScan;` |
|     246 |  2293 | `			while( &pDollar[1] < pEnd` |
|     166 |  2294 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2295 | `				pDollar++;` |
|     ! 0 |  2296 | `			}` |
|     166 |  2297 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2298 | `				break;` |
|       - |  2299 | `			}` |
|     166 |  2300 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2301 | `				pScan = pDollar + 1;` |
|     ! 0 |  2302 | `				continue;` |
|       - |  2303 | `			}` |
|     248 |  2304 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     164 |  2305 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      82 |  2306 | `				aShadow,nShadow);` |
|     166 |  2307 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2308 | `				return SXERR_ABORT;` |
|       - |  2309 | `			}` |
|     166 |  2310 | `			pScan = pDollar + 2;` |
|       - |  2311 | `		}` |
|       2 |  2312 | `	}` |
|     195 |  2313 | `	return SXRET_OK;` |
|      99 |  2314 | `}` |
|       - |  2315 | `/*` |
|       - |  2316 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2317 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2318 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2319 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2320 | ` * $this is also made available.` |
|       - |  2321 | ` */` |
|     174 |  2322 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2323 | `{` |
|       - |  2324 | `	ph7_vm_func *pFunc;` |
|       - |  2325 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2326 | `	GenBlock *pBlock;` |
|       - |  2327 | `	SySet *pInstrContainer;` |
|       - |  2328 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2329 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2330 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2331 | `	SyToken *pSavedEnd;` |
|       - |  2332 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2333 | `	char zName[512];` |
|       - |  2334 | `	static int iCnt = 1;` |
|       - |  2335 | `	char *zDup;` |
|       - |  2336 | `	sxu32 nLen;` |
|       - |  2337 | `	sxu32 nLine;` |
|     179 |  2338 | `	sxi32 iFlags = 0;` |
|     179 |  2339 | `	int bStatic = 0;` |
|       - |  2340 | `	sxi32 rc;` |
|       - |  2341 | `	sxu32 n;` |
|      87 |  2342 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2343 |  |
|     179 |  2344 | `	nLine = pGen->pIn->nLine;` |
|       - |  2345 | `	/* Optional 'static' prefix */` |
|     174 |  2346 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     179 |  2347 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2348 | `		bStatic = 1;` |
|       3 |  2349 | `		pGen->pIn++;` |
|       1 |  2350 | `	}` |
|       - |  2351 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     174 |  2352 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     179 |  2353 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2354 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2355 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2356 | `		return SXERR_SYNTAX;` |
|       - |  2357 | `	}` |
|     179 |  2358 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2359 | `	/* Optional '&' — return by reference */` |
|     179 |  2360 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2361 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2362 | `		pGen->pIn++;` |
|     ! 0 |  2363 | `	}` |
|       - |  2364 | `	/* Expect '(' */` |
|     179 |  2365 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2366 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2367 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2368 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2369 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2370 | `		}else{` |
|     ! 0 |  2371 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2372 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2373 | `		}` |
|       3 |  2374 | `		return SXERR_SYNTAX;` |
|       - |  2375 | `	}` |
|     177 |  2376 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2377 | `	/* Delimit the parameter list */` |
|     177 |  2378 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     177 |  2379 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2380 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2381 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2382 | `		return SXERR_SYNTAX;` |
|       - |  2383 | `	}` |
|       - |  2384 | `	/* Allocate the function state */` |
|     174 |  2385 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     174 |  2386 | `	if( pFunc == 0 ){` |
|     ! 0 |  2387 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2388 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2389 | `		return SXERR_ABORT;` |
|       - |  2390 | `	}` |
|       - |  2391 | `	/* Generate a unique lambda name */` |
|     174 |  2392 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     268 |  2393 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      96 |  2394 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2395 | `	}` |
|     174 |  2396 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     174 |  2397 | `	if( zDup == 0 ){` |
|     ! 0 |  2398 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2399 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2400 | `		return SXERR_ABORT;` |
|       - |  2401 | `	}` |
|     174 |  2402 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2403 | `	/* Collect function arguments */` |
|     174 |  2404 | `	if( pGen->pIn < pSigEnd ){` |
|     103 |  2405 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     103 |  2406 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2407 | `			return SXERR_ABORT;` |
|       - |  2408 | `		}` |
|      50 |  2409 | `	}` |
|       - |  2410 | `	/* Point past ')' and parse optional return type */` |
|     174 |  2411 | `	pGen->pIn = &pSigEnd[1];` |
|     174 |  2412 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     174 |  2413 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2414 | `		return SXERR_ABORT;` |
|     174 |  2415 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2416 | `		return SXERR_SYNTAX;` |
|       - |  2417 | `	}` |
|       - |  2418 | `	/* Expect '=>' */` |
|     174 |  2419 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2420 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2421 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2422 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2423 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2424 | `		}else{` |
|     ! 0 |  2425 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2426 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2427 | `		}` |
|       3 |  2428 | `		return SXERR_SYNTAX;` |
|       - |  2429 | `	}` |
|     171 |  2430 | `	pGen->pIn++; /* Jump '=>' */` |
|     171 |  2431 | `	pBodyStart = pGen->pIn;` |
|     171 |  2432 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2433 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2434 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2435 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2436 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     171 |  2437 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2438 | `	{` |
|     171 |  2439 | `		SyString *aShadow = 0;` |
|     171 |  2440 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     171 |  2441 | `		if( nShadow > 0 ){` |
|     100 |  2442 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      98 |  2443 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|     100 |  2444 | `			if( aShadow == 0 ){` |
|     ! 0 |  2445 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2446 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2447 | `				return SXERR_ABORT;` |
|       - |  2448 | `			}` |
|     224 |  2449 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     126 |  2450 | `				aShadow[n] = aArgs[n].sName;` |
|      64 |  2451 | `			}` |
|      49 |  2452 | `		}` |
|     255 |  2453 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      84 |  2454 | `			aShadow,nShadow);` |
|     171 |  2455 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2456 | `			return SXERR_ABORT;` |
|       - |  2457 | `		}` |
|       - |  2458 | `	}` |
|       - |  2459 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2460 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2461 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2462 | `	 * $this. */` |
|     171 |  2463 | `	if( !bStatic ){` |
|       - |  2464 | `		char *zThisDup;` |
|     169 |  2465 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     169 |  2466 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2467 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2468 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2469 | `			return SXERR_ABORT;` |
|       - |  2470 | `		}` |
|     169 |  2471 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     169 |  2472 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     169 |  2473 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     169 |  2474 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     169 |  2475 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      83 |  2476 | `	}` |
|       - |  2477 | `	/* Arrow functions are always closures */` |
|     171 |  2478 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2479 | `	/* Compile the body expression as an implicit return */` |
|     255 |  2480 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      84 |  2481 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     171 |  2482 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2483 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2484 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2485 | `		return SXERR_ABORT;` |
|       - |  2486 | `	}` |
|     171 |  2487 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     171 |  2488 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     171 |  2489 | `	pSavedEnd = pGen->pEnd;` |
|     171 |  2490 | `	pGen->pIn = pBodyStart;` |
|     171 |  2491 | `	pGen->pEnd = pBodyEnd;` |
|     171 |  2492 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     171 |  2493 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2494 | `		return SXERR_ABORT;` |
|       - |  2495 | `	}` |
|       - |  2496 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2497 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2498 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2499 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     171 |  2500 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     171 |  2501 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     171 |  2502 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     171 |  2503 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     171 |  2504 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2505 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     171 |  2506 | `	pGen->pIn = pBodyEnd;` |
|     171 |  2507 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2508 | `	/* Emit the load-closure instruction */` |
|     171 |  2509 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     171 |  2510 | `	return SXRET_OK;` |
|      92 |  2511 | `}` |
|       - |  2512 | `/*` |
|       - |  2513 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2514 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2515 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2516 | ` * expression's value.` |
|       - |  2517 | ` */` |
|     346 |  2518 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2519 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2520 | `{` |
|       - |  2521 | `	SySet *pInstrContainer;` |
|       - |  2522 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2523 | `	GenBlock *pArmBlock;` |
|       - |  2524 | `	sxi32 rc;` |
|     349 |  2525 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2526 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2527 | `	pGen->pIn  = pStart;` |
|     349 |  2528 | `	pGen->pEnd = pStop;` |
|     349 |  2529 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2530 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2531 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2532 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2533 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2534 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2535 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2536 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2537 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2538 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2539 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2540 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2541 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2542 | `		return SXERR_ABORT;` |
|       - |  2543 | `	}` |
|     349 |  2544 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2545 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2546 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2547 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2548 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2549 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2550 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2551 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2552 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2553 | `		return SXERR_ABORT;` |
|       - |  2554 | `	}` |
|     349 |  2555 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2556 | `		return SXERR_EMPTY;` |
|       - |  2557 | `	}` |
|     349 |  2558 | `	return SXRET_OK;` |
|     176 |  2559 | `}` |
|       - |  2560 | `/*` |
|       - |  2561 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2562 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2563 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2564 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2565 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2566 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2567 | ` */` |
|       - |  2568 | `/*` |
|       - |  2569 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2570 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2571 | ` * caller can bail out of the current expression.` |
|       - |  2572 | ` */` |
|       2 |  2573 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2574 | `{` |
|       - |  2575 | `	va_list ap;` |
|       - |  2576 | `	sxi32 rc;` |
|       - |  2577 | `	SyBlob sMsg;` |
|       3 |  2578 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2579 | `	va_start(ap,zFmt);` |
|       3 |  2580 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2581 | `	va_end(ap);` |
|       3 |  2582 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2583 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2584 | `	SyBlobRelease(&sMsg);` |
|       3 |  2585 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2586 | `		return SXERR_ABORT;` |
|       - |  2587 | `	}` |
|       3 |  2588 | `	return SXERR_SYNTAX;` |
|       2 |  2589 | `}` |
|       - |  2590 | `/*` |
|       - |  2591 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2592 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2593 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2594 | ` */` |
|     348 |  2595 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2596 | `{` |
|     352 |  2597 | `	SyToken *pCur = pStart;` |
|     352 |  2598 | `	int iNest = 0;` |
|     814 |  2599 | `	while( pCur < pEnd ){` |
|     780 |  2600 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2601 | `			iNest++;` |
|     774 |  2602 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2603 | `			iNest--;` |
|     762 |  2604 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2605 | `			return pCur;` |
|       - |  2606 | `		}` |
|     466 |  2607 | `		pCur++;` |
|       4 |  2608 | `	}` |
|      37 |  2609 | `	return pEnd;` |
|     178 |  2610 | `}` |
|      70 |  2611 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2612 | `{` |
|       - |  2613 | `	ph7_match *pMatch;` |
|       - |  2614 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2615 | `	int bHasDefault = 0;` |
|       - |  2616 | `	sxu32 nLine;` |
|       - |  2617 | `	sxi32 rc;` |
|      35 |  2618 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2619 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2620 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2621 | `	/* Expect '(' */` |
|      75 |  2622 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2623 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2624 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2625 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2626 | `	}` |
|      75 |  2627 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2628 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2629 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2630 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2631 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2632 | `	}` |
|      75 |  2633 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2634 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2635 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2636 | `	}` |
|       - |  2637 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2638 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2639 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2640 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2641 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2642 | `		return SXERR_ABORT;` |
|       - |  2643 | `	}` |
|      75 |  2644 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2645 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2646 | `	/* Expect '{' */` |
|      75 |  2647 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2648 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2649 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2650 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2651 | `	}` |
|      75 |  2652 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2653 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2654 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2655 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2656 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2657 | `	}` |
|       - |  2658 | `	/* Allocate ph7_match container */` |
|      75 |  2659 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2660 | `	if( pMatch == 0 ){` |
|     ! 0 |  2661 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2662 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2663 | `		return SXERR_ABORT;` |
|       - |  2664 | `	}` |
|      75 |  2665 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2666 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2667 | `	/* Iterate arms */` |
|     253 |  2668 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2669 | `		ph7_match_arm sArm;` |
|       - |  2670 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2671 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2672 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2673 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2674 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2675 | `		/* 'default' arm? */` |
|     182 |  2676 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2677 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2678 | `			if( bHasDefault ){` |
|       3 |  2679 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2680 | `					"Match expressions may only contain one default arm");` |
|       4 |  2681 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2682 | `			}` |
|      20 |  2683 | `			sArm.bDefault = 1;` |
|      20 |  2684 | `			bHasDefault = 1;` |
|      20 |  2685 | `			pGen->pIn++;` |
|      20 |  2686 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2687 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2688 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2689 | `			}` |
|      20 |  2690 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2691 | `		}else{` |
|       - |  2692 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2693 | `			pCondStart = pGen->pIn;` |
|     166 |  2694 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2695 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2696 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2697 | `				SySet sCondBc;` |
|       9 |  2698 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2699 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2700 | `						"syntax error, empty match condition expression");` |
|       - |  2701 | `				}` |
|       9 |  2702 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2703 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2704 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2705 | `					return SXERR_ABORT;` |
|       - |  2706 | `				}` |
|       9 |  2707 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2708 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2709 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2710 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2711 | `			}` |
|     166 |  2712 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2713 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2714 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2715 | `			}` |
|     163 |  2716 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2717 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2718 | `					"syntax error, empty match condition expression");` |
|       - |  2719 | `			}` |
|       - |  2720 | `			{` |
|       - |  2721 | `				SySet sCondBc;` |
|     163 |  2722 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2723 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2724 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2725 | `					return SXERR_ABORT;` |
|       - |  2726 | `				}` |
|     163 |  2727 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2728 | `			}` |
|     163 |  2729 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2730 | `		}` |
|       - |  2731 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2732 | `		pResStart = pGen->pIn;` |
|     181 |  2733 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2734 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2735 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2736 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2737 | `		}` |
|     181 |  2738 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2739 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2740 | `			return SXERR_ABORT;` |
|       - |  2741 | `		}` |
|     181 |  2742 | `		pGen->pIn = pResEnd;` |
|     181 |  2743 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2744 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2745 | `		}` |
|     181 |  2746 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2747 | `	}` |
|      69 |  2748 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2750 | `	return SXRET_OK;` |
|      40 |  2751 | `}` |
|       - |  2752 | `/*` |
|       - |  2753 | ` * Compile a backtick quoted string.` |
|       - |  2754 | ` */` |
|       4 |  2755 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2756 | `{` |
|       - |  2757 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2758 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2759 | `	 */` |
|       8 |  2760 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2761 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2762 | `		ph7_lib_version()` |
|       - |  2763 | `		);` |
|       - |  2764 | `	/* Load NULL */` |
|       6 |  2765 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2766 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2767 | `	/* Node successfully compiled */` |
|       6 |  2768 | `	return SXRET_OK;` |
|       2 |  2769 | `}` |
|       - |  2770 | `/*` |
|       - |  2771 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2772 | ` * construct.` |
|       - |  2773 | ` */` |
|      82 |  2774 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2775 | `{` |
|       - |  2776 | `	SyString *pName;` |
|       - |  2777 | `	sxu32 nKeyID;` |
|       - |  2778 | `	sxi32 rc;` |
|       - |  2779 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      87 |  2780 | `	pName = &pGen->pIn->sData;` |
|      87 |  2781 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      87 |  2782 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      87 |  2783 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2784 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2785 | `		/* Compile arguments one after one */` |
|       9 |  2786 | `		pTmp = pGen->pEnd;` |
|       - |  2787 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2788 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2789 | `		 *  mean that the following expression is valid:` |
|       - |  2790 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2791 | `		 */` |
|       9 |  2792 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2793 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2794 | `			if( pGen->pIn < pNext ){` |
|       9 |  2795 | `				pGen->pEnd = pNext;` |
|       9 |  2796 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2797 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2798 | `					return SXERR_ABORT;` |
|       - |  2799 | `				}` |
|       9 |  2800 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2801 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2802 | `					 * without the overhead of a function call.` |
|       - |  2803 | `					 * This is a very powerful optimization that improve` |
|       - |  2804 | `					 * performance greatly.` |
|       - |  2805 | `					 */` |
|       9 |  2806 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2807 | `				}` |
|       4 |  2808 | `			}` |
|       - |  2809 | `			/* Jump trailing commas */` |
|       9 |  2810 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2811 | `				pNext++;` |
|     ! 0 |  2812 | `			}` |
|       9 |  2813 | `			pGen->pIn = pNext;` |
|       1 |  2814 | `		}` |
|       - |  2815 | `		/* Restore token stream */` |
|       9 |  2816 | `		pGen->pEnd = pTmp;` |
|       5 |  2817 | `	}else{` |
|      79 |  2818 | `		sxi32 nArg = 0;` |
|      79 |  2819 | `		sxu32 nIdx = 0;` |
|      79 |  2820 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      79 |  2821 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2822 | `			return SXERR_ABORT;` |
|      79 |  2823 | `		}else if(rc != SXERR_EMPTY ){` |
|      79 |  2824 | `			nArg = 1;` |
|      37 |  2825 | `		}` |
|      79 |  2826 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2827 | `			ph7_value *pObj;` |
|       - |  2828 | `			/* Emit the call instruction */` |
|      31 |  2829 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      31 |  2830 | `			if( pObj == 0 ){` |
|     ! 0 |  2831 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2832 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2833 | `				return SXERR_ABORT;` |
|       - |  2834 | `			}` |
|      31 |  2835 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2836 | `			/* Install in the literal table */` |
|      31 |  2837 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      13 |  2838 | `		}` |
|       - |  2839 | `		/* Emit the call instruction */` |
|      79 |  2840 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      79 |  2841 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2842 | `	}` |
|       - |  2843 | `	/* Node successfully compiled */` |
|      87 |  2844 | `	return SXRET_OK;` |
|      46 |  2845 | `}` |
|       - |  2846 | `/*` |
|       - |  2847 | ` * Compile a node holding a variable declaration.` |
|       - |  2848 | ` * According to the PHP language reference` |
|       - |  2849 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2850 | ` *  The variable name is case-sensitive.` |
|       - |  2851 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2852 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2853 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2854 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2855 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2856 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2857 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2858 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2859 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2860 | ` *  the chapter on Expressions.` |
|       - |  2861 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2862 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2863 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2864 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2865 | ` *  is being assigned (the source variable).` |
|       - |  2866 | ` */` |
| 1173472 |  2867 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2868 | `{` |
| 1173477 |  2869 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2870 | `	sxi32 iVv;` |
|       - |  2871 | `	sxi32 iP1;` |
|       - |  2872 | `	void *p3;` |
|       - |  2873 | `	sxi32 rc;` |
| 1173477 |  2874 | `	iVv = -1; /* Variable variable counter */` |
| 2346961 |  2875 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1173489 |  2876 | `		pGen->pIn++;` |
| 1173489 |  2877 | `		iVv++;` |
|       5 |  2878 | `	}` |
| 1173477 |  2879 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2880 | `		/* Invalid variable name */` |
|     ! 0 |  2881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2882 | `		if( rc == SXERR_ABORT ){` |
|       - |  2883 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2884 | `			return SXERR_ABORT;` |
|       - |  2885 | `		}` |
|     ! 0 |  2886 | `		return SXRET_OK;` |
|       - |  2887 | `	}` |
| 1173477 |  2888 | `	p3  = 0;` |
| 1173477 |  2889 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2890 | `		/* Dynamic variable creation */` |
|      19 |  2891 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  2892 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  2893 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2894 | `			/* Empty expression */` |
|       3 |  2895 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2896 | `			return SXRET_OK;` |
|       - |  2897 | `		}` |
|       - |  2898 | `		/* Compile the expression holding the variable name */` |
|      16 |  2899 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2900 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2901 | `			return SXERR_ABORT;` |
|      16 |  2902 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2903 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2904 | `			return SXRET_OK;` |
|       - |  2905 | `		}` |
|       7 |  2906 | `	}else{` |
|       - |  2907 | `		SyHashEntry *pEntry;` |
|       - |  2908 | `		SyString *pName;` |
| 1173461 |  2909 | `		char *zName = 0;` |
|       - |  2910 | `		/* Extract variable name */` |
| 1173461 |  2911 | `		pName = &pGen->pIn->sData;` |
|       - |  2912 | `		/* Advance the stream cursor */` |
| 1173461 |  2913 | `		pGen->pIn++;` |
| 1173461 |  2914 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1173461 |  2915 | `		if( pEntry == 0 ){` |
|       - |  2916 | `			/* Duplicate name */` |
|  168931 |  2917 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  168931 |  2918 | `			if( zName == 0 ){` |
|     ! 0 |  2919 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2920 | `				return SXERR_ABORT;` |
|       - |  2921 | `			}` |
|       - |  2922 | `			/* Install in the hashtable */` |
|  168931 |  2923 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   84468 |  2924 | `		}else{` |
|       - |  2925 | `			/* Name already available */` |
| 1004535 |  2926 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2927 | `		}` |
| 1173461 |  2928 | `		p3 = (void *)zName;` |
|       - |  2929 | `	}` |
| 1173473 |  2930 | `	iP1 = 0;` |
| 1173473 |  2931 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  457753 |  2932 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2933 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  457735 |  2934 | `			iP1 = 1;` |
|  228865 |  2935 | `		}` |
|  228874 |  2936 | `	}` |
|       - |  2937 | `	/* Emit the load instruction */` |
| 1173473 |  2938 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1173485 |  2939 | `	while( iVv > 0 ){` |
|      13 |  2940 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2941 | `		iVv--;` |
|       1 |  2942 | `	}` |
|       - |  2943 | `	/* Node successfully compiled */` |
| 1173473 |  2944 | `	return SXRET_OK;` |
|  586741 |  2945 | `}` |
|       - |  2946 | `/*` |
|       - |  2947 | ` * Load a literal.` |
|       - |  2948 | ` */` |
|  809414 |  2949 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2950 | `{` |
|  809419 |  2951 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2952 | `	ph7_value *pObj;` |
|       - |  2953 | `	SyString *pStr;` |
|       - |  2954 | `	sxu32 nIdx;` |
|       - |  2955 | `	/* Extract token value */` |
|  809419 |  2956 | `	pStr = &pToken->sData;` |
|       - |  2957 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  809419 |  2958 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  171575 |  2959 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2960 | `			/* NULL constant are always indexed at 0 */` |
|   63111 |  2961 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   63111 |  2962 | `			return SXRET_OK;` |
|  108469 |  2963 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2964 | `			/* TRUE constant are always indexed at 1 */` |
|     833 |  2965 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     833 |  2966 | `			return SXRET_OK;` |
|       5 |  2967 | `		}` |
|  746482 |  2968 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  109630 |  2969 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2970 | `			/* FALSE constant are always indexed at 2 */` |
|   48379 |  2971 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   48379 |  2972 | `			return SXRET_OK;` |
|  646922 |  2973 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  114894 |  2974 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2975 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   11015 |  2976 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   11015 |  2977 | `			if( pObj == 0 ){` |
|     ! 0 |  2978 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2979 | `				return SXERR_ABORT;` |
|       - |  2980 | `			}` |
|   11015 |  2981 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2982 | `			/* Emit the load constant instruction */` |
|   11015 |  2983 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   11015 |  2984 | `			return SXRET_OK;` |
|  597023 |  2985 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   37116 |  2986 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2987 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       8 |  2988 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       8 |  2989 | `			if( pObj == 0 ){` |
|     ! 0 |  2990 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2991 | `				return SXERR_ABORT;` |
|       - |  2992 | `			}` |
|       8 |  2993 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2994 | `				SyString sNs;` |
|       8 |  2995 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  2996 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       5 |  2997 | `			}else{` |
|     ! 0 |  2998 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2999 | `			}` |
|       8 |  3000 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       8 |  3001 | `			return SXRET_OK;` |
|  596191 |  3002 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   15531 |  3003 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  588421 |  3004 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   19948 |  3005 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  3006 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  3007 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  3008 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  3009 | `				/* Point to the upper block */` |
|      11 |  3010 | `				pBlock = pBlock->pParent;` |
|       1 |  3011 | `			}` |
|      11 |  3012 | `			if( pBlock == 0 ){` |
|       - |  3013 | `				/* Called in the global scope,load NULL */` |
|       5 |  3014 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  3015 | `			}else{` |
|       - |  3016 | `				/* Extract the target function/method */` |
|       7 |  3017 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  3018 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  3019 | `					/* Not a class method,Load null */` |
|       3 |  3020 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3021 | `				}else{` |
|       5 |  3022 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  3023 | `					if( pObj == 0 ){` |
|     ! 0 |  3024 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3025 | `						return SXERR_ABORT;` |
|       - |  3026 | `					}` |
|       5 |  3027 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  3028 | `					/* Emit the load constant instruction */` |
|       5 |  3029 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  3030 | `				}` |
|       - |  3031 | `			}` |
|      11 |  3032 | `			return SXRET_OK;` |
|       - |  3033 | `	}` |
|       - |  3034 | `	/* Query literal table */` |
|  686085 |  3035 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3036 | `		ph7_value *pLitObj;` |
|       - |  3037 | `		/* Unknown literal,install it in the literal table */` |
|  292347 |  3038 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  292347 |  3039 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3040 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3041 | `			return SXERR_ABORT;` |
|       - |  3042 | `		}` |
|  292347 |  3043 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  292347 |  3044 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  146171 |  3045 | `	}` |
|       - |  3046 | `	/* Emit the load constant instruction */` |
|  686085 |  3047 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  686085 |  3048 | `	return SXRET_OK;` |
|  404712 |  3049 | `}` |
|       - |  3050 | `/*` |
|       - |  3051 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3052 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3053 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3054 | ` * Otherwise, load the simple literal directly.` |
|       - |  3055 | ` */` |
|  813122 |  3056 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3057 | `{` |
|       - |  3058 | `	sxi32 rc;` |
|  813127 |  3059 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3060 | `		return SXRET_OK;` |
|       - |  3061 | `	}` |
|       - |  3062 | `	/* Check if this is a multi-token namespace path */` |
|  813127 |  3063 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3064 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3713 |  3065 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3713 |  3066 | `		int isAbsolute = 0;` |
|    3713 |  3067 | `		SyBlobReset(pWorker);` |
|       - |  3068 | `		/* Check for leading backslash (absolute path) */` |
|    3713 |  3069 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3711 |  3070 | `			isAbsolute = 1;` |
|    3711 |  3071 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1853 |  3072 | `		}` |
|       - |  3073 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3713 |  3074 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3075 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3076 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3077 | `		}` |
|       - |  3078 | `		/* Collect all path components */` |
|    3809 |  3079 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    3809 |  3080 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      52 |  3081 | `				SyBlobAppend(pWorker,"\\",1);` |
|      28 |  3082 | `			}else{` |
|    3761 |  3083 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3084 | `			}` |
|    3809 |  3085 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3713 |  3086 | `				pGen->pIn++;` |
|    3713 |  3087 | `				break;` |
|       - |  3088 | `			}` |
|     100 |  3089 | `			pGen->pIn++;` |
|       4 |  3090 | `		}` |
|    3713 |  3091 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3092 | `			ph7_value *pObj;` |
|       - |  3093 | `			SyString sPath;` |
|       - |  3094 | `			sxu32 nIdx;` |
|    3713 |  3095 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3096 | `			/* Install in the literal table */` |
|    3713 |  3097 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3689 |  3098 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3689 |  3099 | `				if( pObj == 0 ){` |
|     ! 0 |  3100 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3101 | `					return SXERR_ABORT;` |
|       - |  3102 | `				}` |
|    3689 |  3103 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3689 |  3104 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1842 |  3105 | `			}` |
|       - |  3106 | `			/* Emit the load constant instruction.` |
|       - |  3107 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3108 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5567 |  3109 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1854 |  3110 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1854 |  3111 | `				nIdx,0,0);` |
|    3713 |  3112 | `			return SXRET_OK;` |
|       - |  3113 | `		}` |
|     ! 0 |  3114 | `	}` |
|       - |  3115 | `	/* Single-token literal: load directly */` |
|  809419 |  3116 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  809419 |  3117 | `	return rc;` |
|  406566 |  3118 | `}` |
|       - |  3119 | `/*` |
|       - |  3120 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3121 | ` */` |
|       - |  3122 | `/*` |
|       - |  3123 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - |  3124 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - |  3125 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - |  3126 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - |  3127 | ` */` |
|     ! 0 |  3128 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 |  3129 | `{` |
|     ! 0 |  3130 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 |  3131 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - |  3132 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 |  3133 | `	return SXERR_SYNTAX;` |
|     ! 0 |  3134 | `}` |
|  813122 |  3135 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3136 | `{` |
|       - |  3137 | `	sxi32 rc;` |
|  813127 |  3138 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  813127 |  3139 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3140 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3141 | `		return rc;` |
|       - |  3142 | `	}` |
|       - |  3143 | `	/* Node successfully compiled */` |
|  813127 |  3144 | `	return SXRET_OK;` |
|  406566 |  3145 | `}` |
|       - |  3146 | `/*` |
|       - |  3147 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3148 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3149 | ` */` |
|       8 |  3150 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3151 | `{` |
|       - |  3152 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3153 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3154 | `		pGen->pIn++;` |
|       1 |  3155 | `	}` |
|       9 |  3156 | `	return SXRET_OK;` |
|       1 |  3157 | `}` |
|       - |  3158 | `/*` |
|       - |  3159 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3160 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3161 | ` */` |
|     122 |  3162 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3163 | `{` |
|     127 |  3164 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      30 |  3165 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3166 | `			return TRUE;` |
|      28 |  3167 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3168 | `			return TRUE;` |
|       2 |  3169 | `		}` |
|     111 |  3170 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3171 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3172 | `			return TRUE;` |
|       - |  3173 | `		}` |
|     ! 0 |  3174 | `	}` |
|       - |  3175 | `	/* Not a reserved constant */` |
|     119 |  3176 | `	return FALSE;` |
|      66 |  3177 | `}` |
|       - |  3178 | `/*` |
|       - |  3179 | ` * Compile the 'const' statement.` |
|       - |  3180 | ` * According to the PHP language reference` |
|       - |  3181 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3182 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3183 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3184 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3185 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3186 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3187 | ` *  Syntax` |
|       - |  3188 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3189 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3190 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3191 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3192 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3193 | ` *  to get a list of all defined constants.` |
|       - |  3194 | ` *` |
|       - |  3195 | ` * Symisc eXtension.` |
|       - |  3196 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3197 | ` *  would allow only simple scalar value.` |
|       - |  3198 | ` *  Example` |
|       - |  3199 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3200 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3201 | ` */` |
|      34 |  3202 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3203 | `{` |
|       - |  3204 | `	SySet *pConsCode,*pInstrContainer;` |
|      39 |  3205 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3206 | `	SyString *pName;` |
|       - |  3207 | `	sxi32 rc;` |
|      39 |  3208 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      39 |  3209 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3210 | `		/* Invalid constant name */` |
|       8 |  3211 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       8 |  3212 | `		if( rc == SXERR_ABORT ){` |
|       - |  3213 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3214 | `			return SXERR_ABORT;` |
|       - |  3215 | `		}` |
|       8 |  3216 | `		goto Synchronize;` |
|       - |  3217 | `	}` |
|       - |  3218 | `	/* Peek constant name */` |
|      33 |  3219 | `	pName = &pGen->pIn->sData;` |
|       - |  3220 | `	/* Make sure the constant name isn't reserved */` |
|      33 |  3221 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3222 | `		/* Reserved constant */` |
|      10 |  3223 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3224 | `		if( rc == SXERR_ABORT ){` |
|       - |  3225 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3226 | `			return SXERR_ABORT;` |
|       - |  3227 | `		}` |
|      10 |  3228 | `		goto Synchronize;` |
|       - |  3229 | `	}` |
|      24 |  3230 | `	pGen->pIn++;` |
|      24 |  3231 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3232 | `		/* Invalid statement*/` |
|       6 |  3233 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3234 | `		if( rc == SXERR_ABORT ){` |
|       - |  3235 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3236 | `			return SXERR_ABORT;` |
|       - |  3237 | `		}` |
|       6 |  3238 | `		goto Synchronize;` |
|       - |  3239 | `	}` |
|      18 |  3240 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3241 | `	/* Allocate a new constant value container */` |
|      18 |  3242 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      18 |  3243 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3244 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3245 | `		return SXERR_ABORT;` |
|       - |  3246 | `	}` |
|      18 |  3247 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3248 | `	/* Swap bytecode container */` |
|      18 |  3249 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      18 |  3250 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3251 | `	/* Compile constant value */` |
|      18 |  3252 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3253 | `	/* Emit the done instruction */` |
|      18 |  3254 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      18 |  3255 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      18 |  3256 | `	if( rc == SXERR_ABORT ){` |
|       - |  3257 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3258 | `		return SXERR_ABORT;` |
|       - |  3259 | `	}` |
|      18 |  3260 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3261 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3262 | `	{` |
|       - |  3263 | `		SyBlob sFQN;` |
|       - |  3264 | `		SyString sFQNStr;` |
|      18 |  3265 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      18 |  3266 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      18 |  3267 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      18 |  3268 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      18 |  3269 | `		SyBlobRelease(&sFQN);` |
|       - |  3270 | `	}` |
|      18 |  3271 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3272 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3273 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3274 | `	}` |
|      18 |  3275 | `	return SXRET_OK;` |
|       9 |  3276 | `Synchronize:` |
|       - |  3277 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3278 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      41 |  3279 | `		pGen->pIn++;` |
|       3 |  3280 | `	}` |
|      22 |  3281 | `	return SXRET_OK;` |
|      22 |  3282 | `}` |
|       - |  3283 | `/*` |
|       - |  3284 | ` * Compile the 'continue' statement.` |
|       - |  3285 | ` * According to the PHP language reference` |
|       - |  3286 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3287 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3288 | ` *  iteration.` |
|       - |  3289 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3290 | ` *  the purposes of continue.` |
|       - |  3291 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3292 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3293 | ` *  Note:` |
|       - |  3294 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3295 | ` */` |
|       - |  3296 | `/*` |
|       - |  3297 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3298 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3299 | ` * break/continue crosses a try boundary.` |
|       - |  3300 | ` *` |
|       - |  3301 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3302 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3303 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3304 | ` */` |
|    3810 |  3305 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3306 | `{` |
|    3815 |  3307 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    3815 |  3308 | `	int nInlineTry = 0;` |
|   22371 |  3309 | `	while( pBlock && pBlock != pTarget ){` |
|   18561 |  3310 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       6 |  3311 | `			if( pBlock->pUserData ){` |
|       - |  3312 | `				/* A try block with an exception context. In a generator its catch/finally` |
|       - |  3313 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|       - |  3314 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|       - |  3315 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|       6 |  3316 | `				if( pGen->bInGenerator ){` |
|       3 |  3317 | `					nInlineTry++;` |
|       2 |  3318 | `				}else{` |
|       3 |  3319 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       - |  3320 | `				}` |
|       4 |  3321 | `			}else{` |
|       - |  3322 | `				/* A catch/finally block compiled into a separate bytecode container` |
|       - |  3323 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|     ! 0 |  3324 | `				break;` |
|       - |  3325 | `			}` |
|       2 |  3326 | `		}` |
|   18561 |  3327 | `		pBlock = pBlock->pParent;` |
|       5 |  3328 | `	}` |
|    3815 |  3329 | `	return nInlineTry;` |
|       5 |  3330 | `}` |
|    3712 |  3331 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3332 | `{` |
|       - |  3333 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3334 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3335 | `	sxu32 nLineLocal;` |
|       - |  3336 | `	sxi32 rc;` |
|    3717 |  3337 | `	nLineLocal = pGen->pIn->nLine;` |
|    3717 |  3338 | `	iLevel = 0;` |
|       - |  3339 | `	/* Jump the 'continue' keyword */` |
|    3717 |  3340 | `	pGen->pIn++;` |
|    3717 |  3341 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3342 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3343 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3344 | `		 */` |
|       - |  3345 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3346 | `		char *zAlloc = 0;` |
|       - |  3347 | `		SyString sNum;` |
|      17 |  3348 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3349 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3350 | `			return SXERR_ABORT;` |
|       - |  3351 | `		}` |
|      17 |  3352 | `		if( rc == SXRET_OK ){` |
|      20 |  3353 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3354 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3355 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3356 | `				return SXERR_ABORT;` |
|       - |  3357 | `			}` |
|      14 |  3358 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3359 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3360 | `		}` |
|      17 |  3361 | `		if( iLevel < 2 ){` |
|       3 |  3362 | `			iLevel = 0;` |
|       1 |  3363 | `		}` |
|      17 |  3364 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3365 | `	}` |
|       - |  3366 | `	/* Point to the target loop */` |
|    3717 |  3367 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3717 |  3368 | `	if( pLoop == 0 ){` |
|       - |  3369 | `		/* Illegal continue */` |
|      12 |  3370 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      12 |  3371 | `		if( rc == SXERR_ABORT ){` |
|       - |  3372 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3373 | `			return SXERR_ABORT;` |
|       - |  3374 | `		}` |
|       7 |  3375 | `	}else{` |
|    3707 |  3376 | `		sxu32 nInstrIdx = 0;` |
|       - |  3377 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    3707 |  3378 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|       - |  3379 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|       - |  3380 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    3707 |  3381 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    3707 |  3382 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3383 | `			/* According to the PHP language reference manual` |
|       - |  3384 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3385 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3386 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3387 | `			 */` |
|       5 |  3388 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|       5 |  3389 | `			if( rc == SXRET_OK ){` |
|       5 |  3390 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3391 | `			}` |
|       3 |  3392 | `		}else{` |
|       - |  3393 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3703 |  3394 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3703 |  3395 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3396 | `				JumpFixup sJumpFix;` |
|       - |  3397 | `				/* Post-continue */` |
|      14 |  3398 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3399 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3400 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3401 | `			}` |
|       - |  3402 | `		}` |
|       - |  3403 | `	}` |
|    3717 |  3404 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3405 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3406 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3407 | `	}` |
|       - |  3408 | `	/* Statement successfully compiled */` |
|    3717 |  3409 | `	return SXRET_OK;` |
|    1861 |  3410 | `}` |
|       - |  3411 | `/*` |
|       - |  3412 | ` * Compile the 'break' statement.` |
|       - |  3413 | ` * According to the PHP language reference` |
|       - |  3414 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3415 | ` *  structure.` |
|       - |  3416 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3417 | ` *  enclosing structures are to be broken out of.` |
|       - |  3418 | ` */` |
|     124 |  3419 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3420 | `{` |
|       - |  3421 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3422 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3423 | `	sxi32 rc;` |
|     129 |  3424 | `	iLevel = 0;` |
|       - |  3425 | `	/* Jump the 'break' keyword */` |
|     129 |  3426 | `	pGen->pIn++;` |
|     129 |  3427 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3428 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3429 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3430 | `		 */` |
|       - |  3431 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      18 |  3432 | `		char *zAlloc = 0;` |
|       - |  3433 | `		SyString sNum;` |
|      18 |  3434 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      18 |  3435 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3436 | `			return SXERR_ABORT;` |
|       - |  3437 | `		}` |
|      18 |  3438 | `		if( rc == SXRET_OK ){` |
|      21 |  3439 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3440 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3441 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3442 | `				return SXERR_ABORT;` |
|       - |  3443 | `			}` |
|      15 |  3444 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3445 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3446 | `		}` |
|      18 |  3447 | `		if( iLevel < 2 ){` |
|       3 |  3448 | `			iLevel = 0;` |
|       1 |  3449 | `		}` |
|      18 |  3450 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3451 | `	}` |
|       - |  3452 | `	/* Extract the target loop */` |
|     129 |  3453 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     129 |  3454 | `	if( pLoop == 0 ){` |
|       - |  3455 | `		/* Illegal break */` |
|      18 |  3456 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      18 |  3457 | `		if( rc == SXERR_ABORT ){` |
|       - |  3458 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3459 | `			return SXERR_ABORT;` |
|       - |  3460 | `		}` |
|      10 |  3461 | `	}else{` |
|       - |  3462 | `		sxu32 nInstrIdx;` |
|       - |  3463 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     113 |  3464 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|       - |  3465 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     113 |  3466 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     113 |  3467 | `		if( rc == SXRET_OK ){` |
|       - |  3468 | `			/* Fix the jump later when the jump destination is resolved */` |
|     113 |  3469 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      54 |  3470 | `		}` |
|       - |  3471 | `	}` |
|     129 |  3472 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3473 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3474 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3475 | `	}` |
|       - |  3476 | `	/* Statement successfully compiled */` |
|     129 |  3477 | `	return SXRET_OK;` |
|      67 |  3478 | `}` |
|       - |  3479 | `/*` |
|       - |  3480 | ` * Compile or record a label.` |
|       - |  3481 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3482 | ` * Example` |
|       - |  3483 | ` *  goto LABEL;` |
|       - |  3484 | ` *   echo 'Foo';` |
|       - |  3485 | ` *  LABEL:` |
|       - |  3486 | ` *   echo 'Bar';` |
|       - |  3487 | ` */` |
|     112 |  3488 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3489 | `{` |
|       - |  3490 | `	GenBlock *pBlock;` |
|       - |  3491 | `	Label sLabel;` |
|       - |  3492 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3493 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3494 | `	if( pBlock ){` |
|       - |  3495 | `		sxi32 rc;` |
|       8 |  3496 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3497 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3498 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3499 | `			return SXERR_ABORT;` |
|       - |  3500 | `		}` |
|       4 |  3501 | `	}else{` |
|     113 |  3502 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3503 | `		char *zDup;` |
|       - |  3504 | `		/* Initialize label fields */` |
|     113 |  3505 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3506 | `		/* Duplicate label name */` |
|     113 |  3507 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3508 | `		if( zDup == 0 ){` |
|     ! 0 |  3509 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3510 | `			return SXERR_ABORT;` |
|       - |  3511 | `		}` |
|     113 |  3512 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3513 | `		sLabel.bRef  = FALSE;` |
|     113 |  3514 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3515 | `		pBlock = pGen->pCurrent;` |
|     221 |  3516 | `		while( pBlock ){` |
|     133 |  3517 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      23 |  3518 | `				break;` |
|       - |  3519 | `			}` |
|       - |  3520 | `			/* Point to the upper block */` |
|     113 |  3521 | `			pBlock = pBlock->pParent;` |
|       5 |  3522 | `		}` |
|     113 |  3523 | `		if( pBlock ){` |
|      23 |  3524 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      13 |  3525 | `		}else{` |
|      93 |  3526 | `			sLabel.pFunc = 0;` |
|       - |  3527 | `		}` |
|       - |  3528 | `		/* Insert in label set */` |
|     113 |  3529 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3530 | `	}` |
|     117 |  3531 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3532 | `	return SXRET_OK;` |
|      61 |  3533 | `}` |
|       - |  3534 | `/*` |
|       - |  3535 | ` * Compile the so hated 'goto' statement.` |
|       - |  3536 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3537 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3538 | ` * a compiler it has to do this.` |
|       - |  3539 | ` * According to the PHP language reference manual` |
|       - |  3540 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3541 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3542 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3543 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3544 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3545 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3546 | ` *   of a multi-level break` |
|       - |  3547 | ` */` |
|     152 |  3548 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3549 | `{` |
|       - |  3550 | `	JumpFixup sJump;` |
|       - |  3551 | `	sxi32 rc;` |
|     157 |  3552 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3553 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3554 | `		/* Missing label */` |
|     ! 0 |  3555 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3556 | `		if( rc == SXERR_ABORT ){` |
|       - |  3557 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3558 | `			return SXERR_ABORT;` |
|       - |  3559 | `		}` |
|     ! 0 |  3560 | `		return SXRET_OK;` |
|       - |  3561 | `	}` |
|     157 |  3562 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  3563 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3564 | `		if( rc == SXERR_ABORT ){` |
|       - |  3565 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3566 | `			return SXERR_ABORT;` |
|       - |  3567 | `		}` |
|       3 |  3568 | `	}else{` |
|     153 |  3569 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3570 | `		GenBlock *pBlock;` |
|       - |  3571 | `		char *zDup;` |
|       - |  3572 | `		/* Prepare the jump destination */` |
|     153 |  3573 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3574 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3575 | `		/* Duplicate label name */` |
|     153 |  3576 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3577 | `		if( zDup == 0 ){` |
|     ! 0 |  3578 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3579 | `			return SXERR_ABORT;` |
|       - |  3580 | `		}` |
|     153 |  3581 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3582 | `		pBlock = pGen->pCurrent;` |
|     315 |  3583 | `		while( pBlock ){` |
|     199 |  3584 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      36 |  3585 | `				break;` |
|       - |  3586 | `			}` |
|       - |  3587 | `			/* Point to the upper block */` |
|     167 |  3588 | `			pBlock = pBlock->pParent;` |
|       5 |  3589 | `		}` |
|     153 |  3590 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       9 |  3591 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       9 |  3592 | `			if( rc == SXERR_ABORT ){` |
|       - |  3593 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3594 | `				return SXERR_ABORT;` |
|       - |  3595 | `			}` |
|       3 |  3596 | `		}` |
|     153 |  3597 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      29 |  3598 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      16 |  3599 | `		}else{` |
|     127 |  3600 | `			sJump.pFunc = 0;` |
|       - |  3601 | `		}` |
|       - |  3602 | `		/* Emit the unconditional jump */` |
|     153 |  3603 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3604 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3605 | `		}` |
|       - |  3606 | `	}` |
|     157 |  3607 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3608 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3609 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3610 | `	}` |
|       - |  3611 | `	/* Statement successfully compiled */` |
|     157 |  3612 | `	return SXRET_OK;` |
|      81 |  3613 | `}` |
|       - |  3614 | `/*` |
|       - |  3615 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3616 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3617 | ` * failure.` |
|       - |  3618 | ` */` |
|      20 |  3619 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  3620 | `{` |
|       - |  3621 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3622 | `	sxu32 nRawObj;` |
|      10 |  3623 | `	sxu32 nObjIdx;` |
|       - |  3624 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3625 | `	 * a PHP block.` |
|       - |  3626 | `	 */` |
|      10 |  3627 | `Consume:` |
|      22 |  3628 | `	nRawObj = nObjIdx = 0;` |
|      22 |  3629 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3630 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3631 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3632 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3633 | `			return SXERR_ABORT;` |
|       - |  3634 | `		}` |
|       - |  3635 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3636 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3637 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3638 | `		++nRawObj;` |
|     ! 0 |  3639 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3640 | `	}` |
|      22 |  3641 | `	if( nRawObj > 0 ){` |
|       - |  3642 | `		/* Emit the consume instruction */` |
|     ! 0 |  3643 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3644 | `	}` |
|      22 |  3645 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3646 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3647 | `		/* Reset the token set */` |
|     ! 0 |  3648 | `		SySetReset(pTokenSet);` |
|       - |  3649 | `		/* Tokenize input */` |
|     ! 0 |  3650 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3651 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3652 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3653 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3654 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3655 | `		/* Advance the stream cursor */` |
|     ! 0 |  3656 | `		pGen->pRawIn++;` |
|       - |  3657 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3658 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3659 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3660 | `			sxi32 rc;` |
|       - |  3661 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3662 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3663 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3664 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3665 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3666 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3667 | `				return SXERR_ABORT;` |
|     ! 0 |  3668 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3669 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3670 | `			}` |
|     ! 0 |  3671 | `			goto Consume;` |
|       - |  3672 | `		}` |
|     ! 0 |  3673 | `	}else{` |
|       - |  3674 | `		/* No more chunks to process */` |
|      22 |  3675 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  3676 | `		return SXERR_EOF;` |
|       - |  3677 | `	}` |
|     ! 0 |  3678 | `	return SXRET_OK;` |
|      12 |  3679 | `}` |
|       - |  3680 | `/*` |
|       - |  3681 | ` * Compile a PHP block.` |
|       - |  3682 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3683 | ` * optionally delimited by braces {}.` |
|       - |  3684 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3685 | ` * and this function takes care of generating the appropriate error` |
|       - |  3686 | ` * message.` |
|       - |  3687 | ` */` |
|  445602 |  3688 | `static sxi32 PH7_CompileBlock(` |
|       - |  3689 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3690 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3691 | `	)` |
|       5 |  3692 | `{` |
|       - |  3693 | `	sxi32 rc;` |
|       - |  3694 | `	sxu32 nLine;` |
|  445607 |  3695 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  443911 |  3696 | `		nLine = pGen->pIn->nLine;` |
|  443911 |  3697 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  443911 |  3698 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3699 | `			return SXERR_ABORT;` |
|       - |  3700 | `		}` |
|  443911 |  3701 | `		pGen->pIn++;` |
|       - |  3702 | `		/* Compile until we hit the closing braces '}' */` |
|  607933 |  3703 | `		for(;;){` |
| 1215871 |  3704 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  3705 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  3706 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3707 | `			 	   return SXERR_ABORT;` |
|       - |  3708 | `				}` |
|      22 |  3709 | `				if( rc == SXERR_EOF ){` |
|       - |  3710 | `					/* No more token to process. Missing closing braces */` |
|      22 |  3711 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      22 |  3712 | `					break;` |
|       - |  3713 | `				}` |
|     ! 0 |  3714 | `			}` |
| 1215851 |  3715 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3716 | `				/* Closing braces found,break immediately*/` |
|  443891 |  3717 | `				pGen->pIn++;` |
|  443891 |  3718 | `				break;` |
|       - |  3719 | `			}` |
|       - |  3720 | `			/* Compile a single statement */` |
|  771965 |  3721 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  771965 |  3722 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3723 | `				return SXERR_ABORT;` |
|       - |  3724 | `			}` |
|       5 |  3725 | `		}` |
|  443911 |  3726 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  223654 |  3727 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3728 | `		pGen->pIn++;` |
|     ! 0 |  3729 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3730 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3731 | `			return SXERR_ABORT;` |
|       - |  3732 | `		}` |
|       - |  3733 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3734 | `		for(;;){` |
|     ! 0 |  3735 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3736 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3737 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3738 | `			 	   return SXERR_ABORT;` |
|       - |  3739 | `				}` |
|     ! 0 |  3740 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3741 | `					/* No more token to process */` |
|     ! 0 |  3742 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3743 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3744 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3745 | `					}` |
|     ! 0 |  3746 | `					break;` |
|       - |  3747 | `				}` |
|     ! 0 |  3748 | `			}` |
|     ! 0 |  3749 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3750 | `				sxi32 nKwrd;` |
|       - |  3751 | `				/* Keyword found */` |
|     ! 0 |  3752 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3753 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3754 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3755 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3756 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3757 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3758 | `						}` |
|     ! 0 |  3759 | `						break;` |
|       - |  3760 | `				}` |
|     ! 0 |  3761 | `			}` |
|       - |  3762 | `			/* Compile a single statement */` |
|     ! 0 |  3763 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3764 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3765 | `				return SXERR_ABORT;` |
|       - |  3766 | `			}` |
|     ! 0 |  3767 | `		}` |
|     ! 0 |  3768 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3769 | `	}else{` |
|       - |  3770 | `		/* Compile a single statement */` |
|    1701 |  3771 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1701 |  3772 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3773 | `			return SXERR_ABORT;` |
|       - |  3774 | `		}` |
|       - |  3775 | `	}` |
|       - |  3776 | `	/* Jump trailing semi-colons ';' */` |
|  445607 |  3777 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3778 | `		pGen->pIn++;` |
|     ! 0 |  3779 | `	}` |
|  445607 |  3780 | `	return SXRET_OK;` |
|  222806 |  3781 | `}` |
|       - |  3782 | `/*` |
|       - |  3783 | ` * Compile the gentle 'while' statement.` |
|       - |  3784 | ` * According to the PHP language reference` |
|       - |  3785 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3786 | ` *  The basic form of a while statement is:` |
|       - |  3787 | ` *  while (expr)` |
|       - |  3788 | ` *   statement` |
|       - |  3789 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3790 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3791 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3792 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3793 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3794 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3795 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3796 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3797 | ` *  while (expr):` |
|       - |  3798 | ` *    statement` |
|       - |  3799 | ` *   endwhile;` |
|       - |  3800 | ` */` |
|   14796 |  3801 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3802 | `{` |
|   14801 |  3803 | `	GenBlock *pWhileBlock = 0;` |
|   14801 |  3804 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3805 | `	sxu32 nFalseJump;` |
|       - |  3806 | `	sxu32 nLine;` |
|       - |  3807 | `	sxi32 rc;` |
|   14801 |  3808 | `	nLine = pGen->pIn->nLine;` |
|       - |  3809 | `	/* Jump the 'while' keyword */` |
|   14801 |  3810 | `	pGen->pIn++;` |
|   14801 |  3811 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3812 | `		/* Syntax error */` |
|     ! 0 |  3813 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3814 | `		if( rc == SXERR_ABORT ){` |
|       - |  3815 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3816 | `			return SXERR_ABORT;` |
|       - |  3817 | `		}` |
|     ! 0 |  3818 | `		goto Synchronize;` |
|       - |  3819 | `	}` |
|       - |  3820 | `	/* Jump the left parenthesis '(' */` |
|   14801 |  3821 | `	pGen->pIn++;` |
|       - |  3822 | `	/* Create the loop block */` |
|   14801 |  3823 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   14801 |  3824 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3825 | `		return SXERR_ABORT;` |
|       - |  3826 | `	}` |
|       - |  3827 | `	/* Delimit the condition */` |
|   14801 |  3828 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14801 |  3829 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3830 | `		/* Empty expression */` |
|       3 |  3831 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3832 | `		if( rc == SXERR_ABORT ){` |
|       - |  3833 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3834 | `			return SXERR_ABORT;` |
|       - |  3835 | `		}` |
|       1 |  3836 | `	}` |
|       - |  3837 | `	/* Swap token streams */` |
|   14801 |  3838 | `	pTmp = pGen->pEnd;` |
|   14801 |  3839 | `	pGen->pEnd = pEnd;` |
|       - |  3840 | `	/* Compile the expression */` |
|   14801 |  3841 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14801 |  3842 | `	if( rc == SXERR_ABORT ){` |
|       - |  3843 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3844 | `		return SXERR_ABORT;` |
|       - |  3845 | `	}` |
|       - |  3846 | `	/* Update token stream */` |
|   14801 |  3847 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3848 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3849 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3850 | `			return SXERR_ABORT;` |
|       - |  3851 | `		}` |
|     ! 0 |  3852 | `		pGen->pIn++;` |
|     ! 0 |  3853 | `	}` |
|       - |  3854 | `	/* Synchronize pointers */` |
|   14801 |  3855 | `	pGen->pIn  = &pEnd[1];` |
|   14801 |  3856 | `	pGen->pEnd = pTmp;` |
|       - |  3857 | `	/* Emit the false jump */` |
|   14801 |  3858 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3859 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14801 |  3860 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3861 | `	/* Compile the loop body */` |
|   14801 |  3862 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   14801 |  3863 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3864 | `		return SXERR_ABORT;` |
|       - |  3865 | `	}` |
|       - |  3866 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14801 |  3867 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3868 | `	/* Fix all jumps now the destination is resolved */` |
|   14801 |  3869 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3870 | `	/* Release the loop block */` |
|   14801 |  3871 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3872 | `	/* Statement successfully compiled */` |
|   14801 |  3873 | `	return SXRET_OK;` |
|     ! 0 |  3874 | `Synchronize:` |
|       - |  3875 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3876 | `	 * compiling this erroneous block.` |
|       - |  3877 | `	 */` |
|     ! 0 |  3878 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3879 | `		pGen->pIn++;` |
|     ! 0 |  3880 | `	}` |
|     ! 0 |  3881 | `	return SXRET_OK;` |
|    7403 |  3882 | `}` |
|       - |  3883 | `/*` |
|       - |  3884 | ` * Compile the ugly do..while() statement.` |
|       - |  3885 | ` * According to the PHP language reference` |
|       - |  3886 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3887 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3888 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3889 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3890 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3891 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3892 | ` *  would end immediately).` |
|       - |  3893 | ` *  There is just one syntax for do-while loops:` |
|       - |  3894 | ` *  <?php` |
|       - |  3895 | ` *  $i = 0;` |
|       - |  3896 | ` *  do {` |
|       - |  3897 | ` *   echo $i;` |
|       - |  3898 | ` *  } while ($i > 0);` |
|       - |  3899 | ` * ?>` |
|       - |  3900 | ` */` |
|       2 |  3901 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3902 | `{` |
|       3 |  3903 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3904 | `	GenBlock *pDoBlock = 0;` |
|       - |  3905 | `	sxu32 nLine;` |
|       - |  3906 | `	sxi32 rc;` |
|       3 |  3907 | `	nLine = pGen->pIn->nLine;` |
|       - |  3908 | `	/* Jump the 'do' keyword */` |
|       3 |  3909 | `	pGen->pIn++;` |
|       - |  3910 | `	/* Create the loop block */` |
|       3 |  3911 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3912 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3913 | `		return SXERR_ABORT;` |
|       - |  3914 | `	}` |
|       - |  3915 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3916 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3917 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3918 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3919 | `		return SXERR_ABORT;` |
|       - |  3920 | `	}` |
|       3 |  3921 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3922 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3923 | `	}` |
|       3 |  3924 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3925 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3926 | `			/* Missing 'while' statement */` |
|       3 |  3927 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3928 | `			if( rc == SXERR_ABORT ){` |
|       - |  3929 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3930 | `				return SXERR_ABORT;` |
|       - |  3931 | `			}` |
|       3 |  3932 | `			goto Synchronize;` |
|       - |  3933 | `	}` |
|       - |  3934 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3935 | `	pGen->pIn++;` |
|     ! 0 |  3936 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3937 | `		/* Syntax error */` |
|     ! 0 |  3938 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3939 | `		if( rc == SXERR_ABORT ){` |
|       - |  3940 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3941 | `			return SXERR_ABORT;` |
|       - |  3942 | `		}` |
|     ! 0 |  3943 | `		goto Synchronize;` |
|       - |  3944 | `	}` |
|       - |  3945 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3946 | `	pGen->pIn++;` |
|       - |  3947 | `	/* Delimit the condition */` |
|     ! 0 |  3948 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3949 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3950 | `		/* Empty expression */` |
|     ! 0 |  3951 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3952 | `		if( rc == SXERR_ABORT ){` |
|       - |  3953 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3954 | `			return SXERR_ABORT;` |
|       - |  3955 | `		}` |
|     ! 0 |  3956 | `		goto Synchronize;` |
|       - |  3957 | `	}` |
|       - |  3958 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3959 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3960 | `		JumpFixup *aPost;` |
|       - |  3961 | `		VmInstr *pInstr;` |
|       - |  3962 | `		sxu32 nJumpDest;` |
|       - |  3963 | `		sxu32 n;` |
|     ! 0 |  3964 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3965 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3966 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3967 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3968 | `			if( pInstr ){` |
|       - |  3969 | `				/* Fix */` |
|     ! 0 |  3970 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3971 | `			}` |
|     ! 0 |  3972 | `		}` |
|     ! 0 |  3973 | `	}` |
|       - |  3974 | `	/* Swap token streams */` |
|     ! 0 |  3975 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3976 | `	pGen->pEnd = pEnd;` |
|       - |  3977 | `	/* Compile the expression */` |
|     ! 0 |  3978 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3979 | `	if( rc == SXERR_ABORT ){` |
|       - |  3980 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3981 | `		return SXERR_ABORT;` |
|       - |  3982 | `	}` |
|       - |  3983 | `	/* Update token stream */` |
|     ! 0 |  3984 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3985 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3986 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3987 | `			return SXERR_ABORT;` |
|       - |  3988 | `		}` |
|     ! 0 |  3989 | `		pGen->pIn++;` |
|     ! 0 |  3990 | `	}` |
|     ! 0 |  3991 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3992 | `	pGen->pEnd = pTmp;` |
|       - |  3993 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3994 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3995 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3996 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3997 | `	/* Release the loop block */` |
|     ! 0 |  3998 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3999 | `	/* Statement successfully compiled */` |
|     ! 0 |  4000 | `	return SXRET_OK;` |
|       1 |  4001 | `Synchronize:` |
|       - |  4002 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4003 | `	 * compiling this erroneous block.` |
|       - |  4004 | `	 */` |
|       3 |  4005 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4006 | `		pGen->pIn++;` |
|     ! 0 |  4007 | `	}` |
|       3 |  4008 | `	return SXRET_OK;` |
|       2 |  4009 | `}` |
|       - |  4010 | `/*` |
|       - |  4011 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  4012 | ` * According to the PHP language reference` |
|       - |  4013 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  4014 | ` *  The syntax of a for loop is:` |
|       - |  4015 | ` *  for (expr1; expr2; expr3)` |
|       - |  4016 | ` *   statement` |
|       - |  4017 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  4018 | ` *  the beginning of the loop.` |
|       - |  4019 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  4020 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  4021 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  4022 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  4023 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  4024 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  4025 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  4026 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  4027 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  4028 | ` *  of using the for truth expression.` |
|       - |  4029 | ` */` |
|   14796 |  4030 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4031 | `{` |
|   14801 |  4032 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   14801 |  4033 | `	GenBlock *pForBlock = 0;` |
|       - |  4034 | `	sxu32 nFalseJump;` |
|       - |  4035 | `	sxu32 nLine;` |
|       - |  4036 | `	sxi32 rc;` |
|   14801 |  4037 | `	nLine = pGen->pIn->nLine;` |
|       - |  4038 | `	/* Jump the 'for' keyword */` |
|   14801 |  4039 | `	pGen->pIn++;` |
|   14801 |  4040 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4041 | `		/* Syntax error */` |
|     ! 0 |  4042 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4043 | `		if( rc == SXERR_ABORT ){` |
|       - |  4044 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4045 | `			return SXERR_ABORT;` |
|       - |  4046 | `		}` |
|     ! 0 |  4047 | `		return SXRET_OK;` |
|       - |  4048 | `	}` |
|       - |  4049 | `	/* Jump the left parenthesis '(' */` |
|   14801 |  4050 | `	pGen->pIn++;` |
|       - |  4051 | `	/* Delimit the init-expr;condition;post-expr */` |
|   14801 |  4052 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14801 |  4053 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4054 | `		/* Empty expression */` |
|     ! 0 |  4055 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4056 | `		if( rc == SXERR_ABORT ){` |
|       - |  4057 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4058 | `			return SXERR_ABORT;` |
|       - |  4059 | `		}` |
|       - |  4060 | `		/* Synchronize */` |
|     ! 0 |  4061 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4062 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4063 | `			pGen->pIn++;` |
|     ! 0 |  4064 | `		}` |
|     ! 0 |  4065 | `		return SXRET_OK;` |
|       - |  4066 | `	}` |
|       - |  4067 | `	/* Swap token streams */` |
|   14801 |  4068 | `	pTmp = pGen->pEnd;` |
|   14801 |  4069 | `	pGen->pEnd = pEnd;` |
|       - |  4070 | `	/* Compile initialization expressions if available */` |
|   14801 |  4071 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4072 | `	/* Pop operand lvalues */` |
|   14801 |  4073 | `	if( rc == SXERR_ABORT ){` |
|       - |  4074 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4075 | `		return SXERR_ABORT;` |
|   14801 |  4076 | `	}else if( rc != SXERR_EMPTY ){` |
|   14799 |  4077 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7397 |  4078 | `	}` |
|   14801 |  4079 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4080 | `		/* Syntax error */` |
|     ! 0 |  4081 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4082 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4083 | `		if( rc == SXERR_ABORT ){` |
|       - |  4084 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4085 | `			return SXERR_ABORT;` |
|       - |  4086 | `		}` |
|     ! 0 |  4087 | `		return SXRET_OK;` |
|       - |  4088 | `	}` |
|       - |  4089 | `	/* Jump the trailing ';' */` |
|   14801 |  4090 | `	pGen->pIn++;` |
|       - |  4091 | `	/* Create the loop block */` |
|   14801 |  4092 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   14801 |  4093 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4094 | `		return SXERR_ABORT;` |
|       - |  4095 | `	}` |
|       - |  4096 | `	/* Deffer continue jumps */` |
|   14801 |  4097 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4098 | `	/* Compile the condition */` |
|   14801 |  4099 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14801 |  4100 | `	if( rc == SXERR_ABORT ){` |
|       - |  4101 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4102 | `		return SXERR_ABORT;` |
|   14801 |  4103 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4104 | `		/* Emit the false jump */` |
|   14799 |  4105 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4106 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14799 |  4107 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7397 |  4108 | `	}` |
|   14801 |  4109 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4110 | `		/* Syntax error */` |
|       6 |  4111 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4112 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4113 | `		if( rc == SXERR_ABORT ){` |
|       - |  4114 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4115 | `			return SXERR_ABORT;` |
|       - |  4116 | `		}` |
|       6 |  4117 | `		return SXRET_OK;` |
|       - |  4118 | `	}` |
|       - |  4119 | `	/* Jump the trailing ';' */` |
|   14797 |  4120 | `	pGen->pIn++;` |
|       - |  4121 | `	/* Save the post condition stream */` |
|   14797 |  4122 | `	pPostStart = pGen->pIn;` |
|       - |  4123 | `	/* Compile the loop body */` |
|   14797 |  4124 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   14797 |  4125 | `	pGen->pEnd = pTmp;` |
|   14797 |  4126 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   14797 |  4127 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4128 | `		return SXERR_ABORT;` |
|       - |  4129 | `	}` |
|       - |  4130 | `	/* Fix post-continue jumps */` |
|   14797 |  4131 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4132 | `		JumpFixup *aPost;` |
|       - |  4133 | `		VmInstr *pInstr;` |
|       - |  4134 | `		sxu32 nJumpDest;` |
|       - |  4135 | `		sxu32 n;` |
|      14 |  4136 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4137 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4138 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4139 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4140 | `			if( pInstr ){` |
|       - |  4141 | `				/* Fix jump */` |
|      14 |  4142 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4143 | `			}` |
|       8 |  4144 | `		}` |
|       6 |  4145 | `	}` |
|       - |  4146 | `	/* compile the post-expressions if available */` |
|   14797 |  4147 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4148 | `		pPostStart++;` |
|     ! 0 |  4149 | `	}` |
|   14797 |  4150 | `	if( pPostStart < pEnd ){` |
|       - |  4151 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   14797 |  4152 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   14797 |  4153 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14797 |  4154 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4155 | `			/* Syntax error */` |
|     ! 0 |  4156 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4157 | `			if( rc == SXERR_ABORT ){` |
|       - |  4158 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4159 | `				return SXERR_ABORT;` |
|       - |  4160 | `			}` |
|     ! 0 |  4161 | `			return SXRET_OK;` |
|       - |  4162 | `		}` |
|   14797 |  4163 | `		RE_SWAP_DELIMITER(pGen);` |
|   14797 |  4164 | `		if( rc == SXERR_ABORT ){` |
|       - |  4165 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4166 | `			return SXERR_ABORT;` |
|   14797 |  4167 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4168 | `			/* Pop operand lvalue */` |
|   14797 |  4169 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7396 |  4170 | `		}` |
|    7396 |  4171 | `	}` |
|       - |  4172 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14797 |  4173 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4174 | `	/* Fix all jumps now the destination is resolved */` |
|   14797 |  4175 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4176 | `	/* Release the loop block */` |
|   14797 |  4177 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4178 | `	/* Statement successfully compiled */` |
|   14797 |  4179 | `	return SXRET_OK;` |
|    7403 |  4180 | `}` |
|       - |  4181 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4182 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4183 | ` * are allowed.` |
|       - |  4184 | ` */` |
|    7936 |  4185 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4186 | `{` |
|    7941 |  4187 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7941 |  4188 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4189 | `		/* Unexpected expression */` |
|     ! 0 |  4190 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4191 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4192 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4193 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4194 | `		}` |
|     ! 0 |  4195 | `	}` |
|    7941 |  4196 | `	return rc;` |
|       5 |  4197 | `}` |
|       - |  4198 | `/*` |
|       - |  4199 | ` * Compile the 'foreach' statement.` |
|       - |  4200 | ` * According to the PHP language reference` |
|       - |  4201 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4202 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4203 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4204 | ` *  is a minor but useful extension of the first:` |
|       - |  4205 | ` *  foreach (array_expression as $value)` |
|       - |  4206 | ` *    statement` |
|       - |  4207 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4208 | ` *   statement` |
|       - |  4209 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4210 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4211 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4212 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4213 | ` *  to the variable $key on each loop.` |
|       - |  4214 | ` *  Note:` |
|       - |  4215 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4216 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4217 | ` *  Note:` |
|       - |  4218 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4219 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4220 | ` *  or after the foreach without resetting it.` |
|       - |  4221 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4222 | ` *  of copying the value.` |
|       - |  4223 | ` */` |
|    4078 |  4224 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4225 | `{` |
|    4083 |  4226 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    4083 |  4227 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    4083 |  4228 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4229 | `	ph7_foreach_info *pInfo;` |
|       - |  4230 | `	sxu32 nFalseJump;` |
|       - |  4231 | `	VmInstr *pInstr;` |
|       - |  4232 | `	sxu32 nLine;` |
|       - |  4233 | `	sxi32 rc;` |
|    4083 |  4234 | `	nLine = pGen->pIn->nLine;` |
|       - |  4235 | `	/* Jump the 'foreach' keyword */` |
|    4083 |  4236 | `	pGen->pIn++;` |
|    4083 |  4237 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4238 | `		/* Syntax error */` |
|     ! 0 |  4239 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4240 | `		if( rc == SXERR_ABORT ){` |
|       - |  4241 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4242 | `			return SXERR_ABORT;` |
|       - |  4243 | `		}` |
|     ! 0 |  4244 | `		goto Synchronize;` |
|       - |  4245 | `	}` |
|       - |  4246 | `	/* Jump the left parenthesis '(' */` |
|    4083 |  4247 | `	pGen->pIn++;` |
|       - |  4248 | `	/* Create the loop block */` |
|    4083 |  4249 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    4083 |  4250 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4251 | `		return SXERR_ABORT;` |
|       - |  4252 | `	}` |
|       - |  4253 | `	/* Delimit the expression */` |
|    4083 |  4254 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    4083 |  4255 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4256 | `		/* Empty expression */` |
|     ! 0 |  4257 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4258 | `		if( rc == SXERR_ABORT ){` |
|       - |  4259 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4260 | `			return SXERR_ABORT;` |
|       - |  4261 | `		}` |
|       - |  4262 | `		/* Synchronize */` |
|     ! 0 |  4263 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4264 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4265 | `			pGen->pIn++;` |
|     ! 0 |  4266 | `		}` |
|     ! 0 |  4267 | `		return SXRET_OK;` |
|       - |  4268 | `	}` |
|       - |  4269 | `	/* Compile the array expression */` |
|    4083 |  4270 | `	pCur = pGen->pIn;` |
|   27993 |  4271 | `	while( pCur < pEnd ){` |
|   27993 |  4272 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    4097 |  4273 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    4097 |  4274 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4275 | `				/* Break with the first 'as' found */` |
|    4083 |  4276 | `				break;` |
|       - |  4277 | `			}` |
|       7 |  4278 | `		}` |
|       - |  4279 | `		/* Advance the stream cursor */` |
|   23915 |  4280 | `		pCur++;` |
|       5 |  4281 | `	}` |
|    4083 |  4282 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4283 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4284 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4285 | `		if( rc == SXERR_ABORT ){` |
|       - |  4286 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4287 | `			return SXERR_ABORT;` |
|       - |  4288 | `		}` |
|     ! 0 |  4289 | `		goto Synchronize;` |
|       - |  4290 | `	}` |
|       - |  4291 | `	/* Swap token streams */` |
|    4083 |  4292 | `	pTmp = pGen->pEnd;` |
|    4083 |  4293 | `	pGen->pEnd = pCur;` |
|    4083 |  4294 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    4083 |  4295 | `	if( rc == SXERR_ABORT ){` |
|       - |  4296 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4297 | `		return SXERR_ABORT;` |
|       - |  4298 | `	}` |
|       - |  4299 | `	/* Update token stream */` |
|    4083 |  4300 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4301 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4302 | `		if( rc == SXERR_ABORT ){` |
|       - |  4303 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4304 | `			return SXERR_ABORT;` |
|       - |  4305 | `		}` |
|     ! 0 |  4306 | `		pGen->pIn++;` |
|     ! 0 |  4307 | `	}` |
|    4083 |  4308 | `	pCur++; /* Jump the 'as' keyword */` |
|    4083 |  4309 | `	pGen->pIn = pCur;` |
|    4083 |  4310 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4311 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4312 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4313 | `			return SXERR_ABORT;` |
|       - |  4314 | `		}` |
|     ! 0 |  4315 | `	}` |
|       - |  4316 | `	/* Create the foreach context */` |
|    4083 |  4317 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    4083 |  4318 | `	if( pInfo == 0 ){` |
|     ! 0 |  4319 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4320 | `		return SXERR_ABORT;` |
|       - |  4321 | `	}` |
|       - |  4322 | `	/* Zero the structure */` |
|    4083 |  4323 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4324 | `	/* Initialize structure fields */` |
|    4083 |  4325 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4326 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4327 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4328 | `	 * '=>'. */` |
|    4083 |  4329 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    4083 |  4330 | `	if( pCur < pEnd ){` |
|       - |  4331 | `		/* Compile the expression holding the key name */` |
|    3881 |  4332 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4333 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4334 | `			if( rc == SXERR_ABORT ){` |
|       - |  4335 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4336 | `				return SXERR_ABORT;` |
|       - |  4337 | `			}` |
|     ! 0 |  4338 | `		}else{` |
|    3881 |  4339 | `			pGen->pEnd = pCur;` |
|    3881 |  4340 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3881 |  4341 | `			if( rc == SXERR_ABORT ){` |
|       - |  4342 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4343 | `				return SXERR_ABORT;` |
|       - |  4344 | `			}` |
|    3881 |  4345 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3881 |  4346 | `			if( pInstr->p3 ){` |
|       - |  4347 | `				/* Record key name */` |
|    3881 |  4348 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1938 |  4349 | `			}` |
|    3881 |  4350 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4351 | `		}` |
|    3881 |  4352 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1938 |  4353 | `	}` |
|    4083 |  4354 | `	pGen->pEnd = pEnd;` |
|    4083 |  4355 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4356 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4357 | `		if( rc == SXERR_ABORT ){` |
|       - |  4358 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4359 | `			return SXERR_ABORT;` |
|       - |  4360 | `		}` |
|     ! 0 |  4361 | `		goto Synchronize;` |
|       - |  4362 | `	}` |
|    4083 |  4363 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4364 | `		pGen->pIn++;` |
|       - |  4365 | `		/* Pass by reference  */` |
|      11 |  4366 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4367 | `	}` |
|       - |  4368 | `	/* Check if the value target is list() */` |
|    4083 |  4369 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4370 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4371 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4372 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4373 | `		 */` |
|       - |  4374 | `		static int iForeachListCnt = 0;` |
|       - |  4375 | `		char zTmp[128];` |
|       - |  4376 | `		sxu32 nLen;` |
|       - |  4377 | `		char *zDup;` |
|      10 |  4378 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4379 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4380 | `		if( zDup == 0 ){` |
|     ! 0 |  4381 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4382 | `			return SXERR_ABORT;` |
|       - |  4383 | `		}` |
|      10 |  4384 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4385 | `		/* Save list() token boundaries */` |
|      10 |  4386 | `		pListStart = pGen->pIn;` |
|       - |  4387 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4388 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4389 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4390 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4391 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4392 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4393 | `				return SXERR_ABORT;` |
|       - |  4394 | `			}` |
|       3 |  4395 | `			goto Synchronize;` |
|       - |  4396 | `		}` |
|       7 |  4397 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4398 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4399 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4400 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4401 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4402 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4403 | `				return SXERR_ABORT;` |
|       - |  4404 | `			}` |
|     ! 0 |  4405 | `			goto Synchronize;` |
|       - |  4406 | `		}` |
|       7 |  4407 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4408 | `		pListEnd = pGen->pIn;` |
|       7 |  4409 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    4078 |  4410 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4411 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4412 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4413 | `		 */` |
|       - |  4414 | `		static int iForeachShortListCnt = 0;` |
|       - |  4415 | `		char zTmp[128];` |
|       - |  4416 | `		sxu32 nLen;` |
|       - |  4417 | `		char *zDup;` |
|      11 |  4418 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|      11 |  4419 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      11 |  4420 | `		if( zDup == 0 ){` |
|     ! 0 |  4421 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4422 | `			return SXERR_ABORT;` |
|       - |  4423 | `		}` |
|      11 |  4424 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4425 | `		/* Save [...] token boundaries */` |
|      11 |  4426 | `		pListStart = pGen->pIn;` |
|       - |  4427 | `		/* Advance past [...] */` |
|      11 |  4428 | `		pGen->pIn++; /* Jump '[' */` |
|      11 |  4429 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|      11 |  4430 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4431 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4432 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4433 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4434 | `				return SXERR_ABORT;` |
|       - |  4435 | `			}` |
|     ! 0 |  4436 | `			goto Synchronize;` |
|       - |  4437 | `		}` |
|      11 |  4438 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|      11 |  4439 | `		pListEnd = pGen->pIn;` |
|      11 |  4440 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       6 |  4441 | `	}else{` |
|       - |  4442 | `		/* Compile the expression holding the value name */` |
|    4065 |  4443 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    4065 |  4444 | `		if( rc == SXERR_ABORT ){` |
|       - |  4445 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4446 | `			return SXERR_ABORT;` |
|       - |  4447 | `		}` |
|    4065 |  4448 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    4065 |  4449 | `		if( pInstr->p3 ){` |
|       - |  4450 | `			/* Record value name */` |
|    4065 |  4451 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    2030 |  4452 | `		}` |
|       - |  4453 | `	}` |
|       - |  4454 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    4081 |  4455 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4456 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4081 |  4457 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4458 | `	/* Record the first instruction to execute */` |
|    4081 |  4459 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4460 | `	/* Emit the FOREACH_STEP instruction */` |
|    4081 |  4461 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4462 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4081 |  4463 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4464 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    4081 |  4465 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4466 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4467 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4468 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4469 | `		 */` |
|      17 |  4470 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4471 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4472 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4473 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4474 | `		 */` |
|      17 |  4475 | `		pSavedIn = pGen->pIn;` |
|      17 |  4476 | `		pSavedEnd = pGen->pEnd;` |
|      17 |  4477 | `		pGen->pIn = pListStart;` |
|      17 |  4478 | `		pGen->pEnd = pListEnd;` |
|      17 |  4479 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|      11 |  4480 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       6 |  4481 | `		}else{` |
|       7 |  4482 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4483 | `		}` |
|      17 |  4484 | `		pGen->pIn = pSavedIn;` |
|      17 |  4485 | `		pGen->pEnd = pSavedEnd;` |
|      17 |  4486 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4487 | `			return SXERR_ABORT;` |
|       - |  4488 | `		}` |
|       - |  4489 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      17 |  4490 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  4491 | `	}` |
|       - |  4492 | `	/* Compile the loop body */` |
|    4081 |  4493 | `	pGen->pIn = &pEnd[1];` |
|    4081 |  4494 | `	pGen->pEnd = pTmp;` |
|    4081 |  4495 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    4081 |  4496 | `	if( rc == SXERR_ABORT ){` |
|       - |  4497 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4498 | `		return SXERR_ABORT;` |
|       - |  4499 | `	}` |
|       - |  4500 | `	/* Emit the unconditional jump to the start of the loop */` |
|    4081 |  4501 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4502 | `	/* Fix all jumps now the destination is resolved */` |
|    4081 |  4503 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4504 | `	/* Release the loop block */` |
|    4081 |  4505 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4506 | `	/* Statement successfully compiled */` |
|    4081 |  4507 | `	return SXRET_OK;` |
|       1 |  4508 | `Synchronize:` |
|       - |  4509 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4510 | `	 * compiling this erroneous block.` |
|       - |  4511 | `	 */` |
|       3 |  4512 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4513 | `		pGen->pIn++;` |
|     ! 0 |  4514 | `	}` |
|       3 |  4515 | `	return SXRET_OK;` |
|    2044 |  4516 | `}` |
|       - |  4517 | `/*` |
|       - |  4518 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4519 | ` * According to the PHP language reference` |
|       - |  4520 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4521 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4522 | ` *  that is similar to that of C:` |
|       - |  4523 | ` *  if (expr)` |
|       - |  4524 | ` *   statement` |
|       - |  4525 | ` *  else construct:` |
|       - |  4526 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4527 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4528 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4529 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4530 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4531 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4532 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4533 | ` *  elseif` |
|       - |  4534 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4535 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4536 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4537 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4538 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4539 | ` *   <?php` |
|       - |  4540 | ` *    if ($a > $b) {` |
|       - |  4541 | ` *     echo "a is bigger than b";` |
|       - |  4542 | ` *    } elseif ($a == $b) {` |
|       - |  4543 | ` *     echo "a is equal to b";` |
|       - |  4544 | ` *    } else {` |
|       - |  4545 | ` *     echo "a is smaller than b";` |
|       - |  4546 | ` *    }` |
|       - |  4547 | ` *    ?>` |
|       - |  4548 | ` */` |
|  153684 |  4549 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4550 | `{` |
|  153689 |  4551 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  153689 |  4552 | `	GenBlock *pCondBlock = 0;` |
|       - |  4553 | `	sxu32 nJumpIdx;` |
|       - |  4554 | `	sxu32 nKeyID;` |
|       - |  4555 | `	sxi32 rc;` |
|       - |  4556 | `	/* Jump the 'if' keyword */` |
|  153689 |  4557 | `	pGen->pIn++;` |
|  153689 |  4558 | `	pToken = pGen->pIn;` |
|       - |  4559 | `	/* Create the conditional block */` |
|  153689 |  4560 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  153689 |  4561 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4562 | `		return SXERR_ABORT;` |
|       - |  4563 | `	}` |
|       - |  4564 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   84235 |  4565 | `	for(;;){` |
|  168475 |  4566 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4567 | `			/* Syntax error */` |
|     ! 0 |  4568 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4569 | `				pToken--;` |
|     ! 0 |  4570 | `			}` |
|     ! 0 |  4571 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4572 | `			if( rc == SXERR_ABORT ){` |
|       - |  4573 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4574 | `				return SXERR_ABORT;` |
|       - |  4575 | `			}` |
|     ! 0 |  4576 | `			goto Synchronize;` |
|       - |  4577 | `		}` |
|       - |  4578 | `		/* Jump the left parenthesis '(' */` |
|  168475 |  4579 | `		pToken++;` |
|       - |  4580 | `		/* Delimit the condition */` |
|  168475 |  4581 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  168475 |  4582 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4583 | `			/* Syntax error */` |
|     ! 0 |  4584 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4585 | `				pToken--;` |
|     ! 0 |  4586 | `			}` |
|     ! 0 |  4587 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4588 | `			if( rc == SXERR_ABORT ){` |
|       - |  4589 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4590 | `				return SXERR_ABORT;` |
|       - |  4591 | `			}` |
|     ! 0 |  4592 | `			goto Synchronize;` |
|       - |  4593 | `		}` |
|       - |  4594 | `		/* Swap token streams */` |
|  168475 |  4595 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4596 | `		/* Compile the condition */` |
|  168475 |  4597 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4598 | `		/* Update token stream */` |
|  168475 |  4599 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4600 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4601 | `			pGen->pIn++;` |
|     ! 0 |  4602 | `		}` |
|  168475 |  4603 | `		pGen->pIn  = &pEnd[1];` |
|  168475 |  4604 | `		pGen->pEnd = pTmp;` |
|  168475 |  4605 | `		if( rc == SXERR_ABORT ){` |
|       - |  4606 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4607 | `			return SXERR_ABORT;` |
|       - |  4608 | `		}` |
|       - |  4609 | `		/* Emit the false jump */` |
|  168475 |  4610 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4611 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  168475 |  4612 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4613 | `		/* Compile the body */` |
|  168475 |  4614 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  168475 |  4615 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4616 | `			return SXERR_ABORT;` |
|       - |  4617 | `		}` |
|  168475 |  4618 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   46959 |  4619 | `			break;` |
|       - |  4620 | `		}` |
|       - |  4621 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   74567 |  4622 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   74567 |  4623 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   48011 |  4624 | `			break;` |
|       - |  4625 | `		}` |
|       - |  4626 | `		/* Emit the unconditional jump */` |
|   26561 |  4627 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4628 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   26561 |  4629 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   26561 |  4630 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   19111 |  4631 | `			pToken = &pGen->pIn[1];` |
|   19111 |  4632 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7388 |  4633 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5890 |  4634 | `					break;` |
|       - |  4635 | `			}` |
|    7341 |  4636 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3668 |  4637 | `		}` |
|   14791 |  4638 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4639 | `		/* Synchronize cursors */` |
|   14791 |  4640 | `		pToken = pGen->pIn;` |
|       - |  4641 | `		/* Fix the false jump */` |
|   14791 |  4642 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4643 | `	} /* For(;;) */` |
|       - |  4644 | `	/* Fix the false jump */` |
|  153689 |  4645 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  153689 |  4646 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   59776 |  4647 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4648 | `			/* Compile the else block */` |
|   11775 |  4649 | `			pGen->pIn++;` |
|   11775 |  4650 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11775 |  4651 | `			if( rc == SXERR_ABORT ){` |
|       - |  4652 |  |
|     ! 0 |  4653 | `				return SXERR_ABORT;` |
|       - |  4654 | `			}` |
|    5885 |  4655 | `	}` |
|  153689 |  4656 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4657 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  153689 |  4658 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4659 | `	/* Release the conditional block */` |
|  153689 |  4660 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4661 | `	/* Statement successfully compiled */` |
|  153689 |  4662 | `	return SXRET_OK;` |
|     ! 0 |  4663 | `Synchronize:` |
|       - |  4664 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4665 | `	 */` |
|     ! 0 |  4666 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4667 | `		pGen->pIn++;` |
|     ! 0 |  4668 | `	}` |
|     ! 0 |  4669 | `	return SXRET_OK;` |
|   76847 |  4670 | `}` |
|       - |  4671 | `/*` |
|       - |  4672 | ` * Compile the global construct.` |
|       - |  4673 | ` * According to the PHP language reference` |
|       - |  4674 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4675 | ` *  to be used in that function.` |
|       - |  4676 | ` *  Example #1 Using global` |
|       - |  4677 | ` *  <?php` |
|       - |  4678 | ` *   $a = 1;` |
|       - |  4679 | ` *   $b = 2;` |
|       - |  4680 | ` *   function Sum()` |
|       - |  4681 | ` *   {` |
|       - |  4682 | ` *    global $a, $b;` |
|       - |  4683 | ` *    $b = $a + $b;` |
|       - |  4684 | ` *   }` |
|       - |  4685 | ` *   Sum();` |
|       - |  4686 | ` *   echo $b;` |
|       - |  4687 | ` *  ?>` |
|       - |  4688 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4689 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4690 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4691 | ` */` |
|      36 |  4692 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4693 | `{` |
|      41 |  4694 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4695 | `	sxi32 nExpr;` |
|       - |  4696 | `	sxi32 rc;` |
|       - |  4697 | `	/* Jump the 'global' keyword */` |
|      41 |  4698 | `	pGen->pIn++;` |
|      41 |  4699 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4700 | `		/* Nothing to process */` |
|     ! 0 |  4701 | `		return SXRET_OK;` |
|       - |  4702 | `	}` |
|      41 |  4703 | `	pTmp = pGen->pEnd;` |
|      41 |  4704 | `	nExpr = 0;` |
|      87 |  4705 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4706 | `		if( pGen->pIn < pNext ){` |
|      51 |  4707 | `			pGen->pEnd = pNext;` |
|      51 |  4708 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4709 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4710 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4711 | `					return SXERR_ABORT;` |
|       - |  4712 | `				}` |
|     ! 0 |  4713 | `			}else{` |
|      51 |  4714 | `				pGen->pIn++;` |
|      51 |  4715 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4716 | `					/* Emit a warning */` |
|     ! 0 |  4717 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4718 | `				}else{` |
|      51 |  4719 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4720 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4721 | `						return SXERR_ABORT;` |
|      51 |  4722 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4723 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4724 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4725 | `							/* Variable name, not a constant */` |
|      51 |  4726 | `							pLast->iP1 = 0;` |
|      23 |  4727 | `						}` |
|      51 |  4728 | `						nExpr++;` |
|      23 |  4729 | `					}` |
|       - |  4730 | `				}` |
|       - |  4731 | `			}` |
|      23 |  4732 | `		}` |
|       - |  4733 | `		/* Next expression in the stream */` |
|      51 |  4734 | `		pGen->pIn = pNext;` |
|       - |  4735 | `		/* Jump trailing commas */` |
|      61 |  4736 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4737 | `			pGen->pIn++;` |
|       5 |  4738 | `		}` |
|       5 |  4739 | `	}` |
|       - |  4740 | `	/* Restore token stream */` |
|      41 |  4741 | `	pGen->pEnd = pTmp;` |
|      41 |  4742 | `	if( nExpr > 0 ){` |
|       - |  4743 | `		/* Emit the uplink instruction */` |
|      41 |  4744 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4745 | `	}` |
|      41 |  4746 | `	return SXRET_OK;` |
|      23 |  4747 | `}` |
|       - |  4748 | `/*` |
|       - |  4749 | ` * Compile the return statement.` |
|       - |  4750 | ` * According to the PHP language reference` |
|       - |  4751 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4752 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4753 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4754 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4755 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4756 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4757 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4758 | ` *  from within the main script file, then script execution end.` |
|       - |  4759 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4760 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4761 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4762 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4763 | ` */` |
|  243548 |  4764 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4765 | `{` |
|  243553 |  4766 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4767 | `	sxi32 rc;` |
|  243553 |  4768 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  243553 |  4769 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - |  4770 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - |  4771 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - |  4772 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - |  4773 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - |  4774 | `	 * normally below so token processing stays consistent. */` |
|  627195 |  4775 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  383647 |  4776 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 |  4777 | `	}` |
|  243548 |  4778 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  243521 |  4779 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|       3 |  4780 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  4781 | `			"A never-returning function must not return");` |
|       3 |  4782 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4783 | `			return SXERR_ABORT;` |
|       - |  4784 | `		}` |
|       1 |  4785 | `	}` |
|       - |  4786 | `	/* Jump the 'return' keyword */` |
|  243553 |  4787 | `	pGen->pIn++;` |
|  243553 |  4788 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4789 | `		/* Compile the expression */` |
|  243523 |  4790 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  243523 |  4791 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4792 | `			return SXERR_ABORT;` |
|  243523 |  4793 | `		}else if(rc != SXERR_EMPTY ){` |
|  243523 |  4794 | `			nRet = 1;` |
|  121759 |  4795 | `		}` |
|  121759 |  4796 | `	}` |
|       - |  4797 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|       - |  4798 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|       - |  4799 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|       - |  4800 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  243553 |  4801 | `	if( pGen->bInGenerator ){` |
|      24 |  4802 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      24 |  4803 | `		return SXRET_OK;` |
|       - |  4804 | `	}` |
|       - |  4805 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4806 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4807 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4808 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4809 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  243533 |  4810 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  243533 |  4811 | `	return SXRET_OK;` |
|  121779 |  4812 | `}` |
|       - |  4813 | `/*` |
|       - |  4814 | ` * Compile a yield expression.` |
|       - |  4815 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4816 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4817 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4818 | ` */` |
|     232 |  4819 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4820 | `{` |
|       - |  4821 | `	SyToken *pTmp, *pSplit;` |
|     237 |  4822 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     237 |  4823 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4824 | `	sxi32 rc;` |
|     116 |  4825 | `	(void)iCompileFlag;` |
|       - |  4826 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     237 |  4827 | `	pGen->pIn++;` |
|       - |  4828 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4829 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4830 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4831 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4832 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     251 |  4833 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     134 |  4834 | `		&& pGen->pIn->sData.nByte == 4` |
|      43 |  4835 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      42 |  4836 | `		pGen->pIn++; /* Skip 'from' */` |
|      42 |  4837 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      42 |  4838 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4839 | `			return SXERR_ABORT;` |
|       - |  4840 | `		}` |
|      42 |  4841 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  4842 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  4843 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  4844 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  4845 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4846 | `				return SXERR_ABORT;` |
|       - |  4847 | `			}` |
|     ! 0 |  4848 | `		}` |
|      42 |  4849 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      42 |  4850 | `		return SXRET_OK;` |
|       - |  4851 | `	}` |
|     199 |  4852 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4853 | `		/* Bare yield — no value */` |
|       3 |  4854 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  4855 | `		return SXRET_OK;` |
|       - |  4856 | `	}` |
|       - |  4857 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     197 |  4858 | `	pSplit = 0;` |
|       - |  4859 | `	{` |
|     197 |  4860 | `		SyToken *pCur = pGen->pIn;` |
|     197 |  4861 | `		sxi32 nNest = 0;` |
|     413 |  4862 | `		while( pCur < pGen->pEnd ){` |
|     235 |  4863 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4864 | `				nNest++;` |
|     235 |  4865 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4866 | `				nNest--;` |
|     235 |  4867 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4868 | `				pSplit = pCur;` |
|      16 |  4869 | `				break;` |
|       - |  4870 | `			}` |
|     221 |  4871 | `			pCur++;` |
|       5 |  4872 | `		}` |
|       - |  4873 | `	}` |
|     197 |  4874 | `	pTmp = pGen->pEnd;` |
|     197 |  4875 | `	if( pSplit ){` |
|       - |  4876 | `		/* yield $key => $value */` |
|      16 |  4877 | `		pGen->pEnd = pSplit;` |
|      16 |  4878 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4879 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4880 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4881 | `		pGen->pEnd = pTmp;` |
|      16 |  4882 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4883 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4884 | `		iP1 = 1;` |
|      16 |  4885 | `		iP2 = 1;` |
|       9 |  4886 | `	}else{` |
|       - |  4887 | `		/* yield $value */` |
|     183 |  4888 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     183 |  4889 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     183 |  4890 | `		if( rc != SXERR_EMPTY ){` |
|     183 |  4891 | `			iP1 = 1;` |
|      89 |  4892 | `		}` |
|       - |  4893 | `	}` |
|     197 |  4894 | `	pGen->pEnd = pTmp;` |
|     197 |  4895 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     197 |  4896 | `	return SXRET_OK;` |
|     121 |  4897 | `}` |
|       - |  4898 | `/*` |
|       - |  4899 | ` * Compile the die/exit language construct.` |
|       - |  4900 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4901 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4902 | ` */` |
|     120 |  4903 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4904 | `{` |
|     125 |  4905 | `	sxi32 nExpr = 0;` |
|       - |  4906 | `	sxi32 rc;` |
|       - |  4907 | `	/* Jump the die/exit keyword */` |
|     125 |  4908 | `	pGen->pIn++;` |
|     125 |  4909 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4910 | `		/* Compile the expression */` |
|     125 |  4911 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4912 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4913 | `			return SXERR_ABORT;` |
|     125 |  4914 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4915 | `			nExpr = 1;` |
|      60 |  4916 | `		}` |
|      60 |  4917 | `	}` |
|       - |  4918 | `	/* Emit the HALT instruction */` |
|     125 |  4919 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4920 | `	return SXRET_OK;` |
|      65 |  4921 | `}` |
|       - |  4922 | `/*` |
|       - |  4923 | ` * Compile the 'echo' language construct.` |
|       - |  4924 | ` */` |
|   14948 |  4925 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4926 | `{` |
|   14953 |  4927 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4928 | `	sxi32 rc;` |
|       - |  4929 | `	/* Jump the 'echo' keyword */` |
|   14953 |  4930 | `	pGen->pIn++;` |
|       - |  4931 | `	/* Compile arguments one after one */` |
|   14953 |  4932 | `	pTmp = pGen->pEnd;` |
|   33259 |  4933 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   18311 |  4934 | `		if( pGen->pIn < pNext ){` |
|   18311 |  4935 | `			pGen->pEnd = pNext;` |
|   18311 |  4936 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   18311 |  4937 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4938 | `				return SXERR_ABORT;` |
|   18311 |  4939 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4940 | `				/* Emit the consume instruction */` |
|   18287 |  4941 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    9141 |  4942 | `			}` |
|    9153 |  4943 | `		}` |
|       - |  4944 | `		/* Jump trailing commas */` |
|   21669 |  4945 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    3363 |  4946 | `			pNext++;` |
|       5 |  4947 | `		}` |
|   18311 |  4948 | `		pGen->pIn = pNext;` |
|       5 |  4949 | `	}` |
|       - |  4950 | `	/* Restore token stream */` |
|   14953 |  4951 | `	pGen->pEnd = pTmp;` |
|   14953 |  4952 | `	return SXRET_OK;` |
|    7479 |  4953 | `}` |
|       - |  4954 | `/*` |
|       - |  4955 | ` * Compile the static statement.` |
|       - |  4956 | ` * According to the PHP language reference` |
|       - |  4957 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4958 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4959 | ` *  when program execution leaves this scope.` |
|       - |  4960 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4961 | ` * Symisc eXtension.` |
|       - |  4962 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4963 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4964 | ` *  Example` |
|       - |  4965 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4966 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4967 | ` */` |
|       8 |  4968 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       3 |  4969 | `{` |
|       - |  4970 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4971 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4972 | `	GenBlock *pBlock;` |
|       - |  4973 | `	SyString *pName;` |
|       - |  4974 | `	char *zDup;` |
|       - |  4975 | `	sxu32 nLine;` |
|       - |  4976 | `	sxi32 rc;` |
|       - |  4977 | `	/* Jump the static keyword */` |
|      11 |  4978 | `	nLine = pGen->pIn->nLine;` |
|      11 |  4979 | `	pGen->pIn++;` |
|       - |  4980 | `	/* Extract the enclosing function if any */` |
|      11 |  4981 | `	pBlock = pGen->pCurrent;` |
|      19 |  4982 | `	while( pBlock ){` |
|      19 |  4983 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      11 |  4984 | `			break;` |
|       - |  4985 | `		}` |
|       - |  4986 | `		/* Point to the upper block */` |
|      11 |  4987 | `		pBlock = pBlock->pParent;` |
|       3 |  4988 | `	}` |
|      11 |  4989 | `	if( pBlock == 0 ){` |
|       - |  4990 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4991 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4992 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4993 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4994 | `				return SXERR_ABORT;` |
|       - |  4995 | `			}` |
|     ! 0 |  4996 | `			goto Synchronize;` |
|       - |  4997 | `		}` |
|       - |  4998 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4999 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  5000 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5001 | `			return SXERR_ABORT;` |
|     ! 0 |  5002 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  5003 | `			/* Emit the POP instruction */` |
|     ! 0 |  5004 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  5005 | `		}` |
|     ! 0 |  5006 | `		return SXRET_OK;` |
|       - |  5007 | `	}` |
|      11 |  5008 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  5009 | `	/* Make sure we are dealing with a valid statement */` |
|      11 |  5010 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       6 |  5011 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  5012 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  5013 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5014 | `				return SXERR_ABORT;` |
|       - |  5015 | `			}` |
|       3 |  5016 | `			goto Synchronize;` |
|       - |  5017 | `	}` |
|       8 |  5018 | `	pGen->pIn++;` |
|       - |  5019 | `	/* Extract variable name */` |
|       8 |  5020 | `	pName = &pGen->pIn->sData;` |
|       8 |  5021 | `	pGen->pIn++; /* Jump the var name */` |
|       8 |  5022 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  5023 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  5024 | `		goto Synchronize;` |
|       - |  5025 | `	}` |
|       - |  5026 | `	/* Initialize the structure describing the static variable */` |
|       8 |  5027 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       8 |  5028 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  5029 | `	/* Duplicate variable name */` |
|       8 |  5030 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       8 |  5031 | `	if( zDup == 0 ){` |
|     ! 0 |  5032 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  5033 | `		return SXERR_ABORT;` |
|       - |  5034 | `	}` |
|       8 |  5035 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  5036 | `	/* Check if we have an expression to compile */` |
|       8 |  5037 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  5038 | `		SySet *pInstrContainer;` |
|       - |  5039 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  5040 | `		 * Static variable can take any complex expression including function` |
|       - |  5041 | `		 * call as their initialization value.` |
|       - |  5042 | `		 * Example:` |
|       - |  5043 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  5044 | `		 */` |
|       8 |  5045 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5046 | `		/* Swap bytecode container */` |
|       8 |  5047 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       8 |  5048 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5049 | `		/* Compile the expression */` |
|       8 |  5050 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5051 | `		/* Emit the done instruction */` |
|       8 |  5052 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5053 | `		/* Restore default bytecode container */` |
|       8 |  5054 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       3 |  5055 | `	}` |
|       - |  5056 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       8 |  5057 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       8 |  5058 | `	return SXRET_OK;` |
|       1 |  5059 | `Synchronize:` |
|       - |  5060 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5061 | `	 * statement.` |
|       - |  5062 | `	 */` |
|       5 |  5063 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5064 | `		pGen->pIn++;` |
|       1 |  5065 | `	}` |
|       3 |  5066 | `	return SXRET_OK;` |
|       7 |  5067 | `}` |
|       - |  5068 | `/*` |
|       - |  5069 | ` * Compile the var statement.` |
|       - |  5070 | ` * Symisc Extension:` |
|       - |  5071 | ` *      var statement can be used outside of a class definition.` |
|       - |  5072 | ` */` |
|       4 |  5073 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5074 | `{` |
|       - |  5075 | `	sxu32 nLine;` |
|       - |  5076 | `	sxi32 rc;` |
|       5 |  5077 | `	nLine = pGen->pIn->nLine;` |
|       - |  5078 | `	/* Jump the 'var' keyword */` |
|       5 |  5079 | `	pGen->pIn++;` |
|       5 |  5080 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5081 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5082 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5083 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5084 | `			pGen->pIn++;` |
|     ! 0 |  5085 | `		}` |
|     ! 0 |  5086 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5087 | `			return SXERR_ABORT;` |
|       - |  5088 | `		}` |
|     ! 0 |  5089 | `	}else{` |
|       - |  5090 | `		/* Compile the expression */` |
|       5 |  5091 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5092 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5093 | `			return SXERR_ABORT;` |
|       5 |  5094 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5095 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5096 | `		}` |
|       - |  5097 | `	}` |
|       5 |  5098 | `	return SXRET_OK;` |
|       3 |  5099 | `}` |
|       - |  5100 | `/*` |
|       - |  5101 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5102 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5103 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5104 | ` */` |
|       - |  5105 | `/*` |
|       - |  5106 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5107 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5108 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5109 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5110 | ` *` |
|       - |  5111 | ` * Resolution order:` |
|       - |  5112 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5113 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5114 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5115 | ` *` |
|       - |  5116 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5117 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5118 | ` * Returns the (possibly new) literal index.` |
|       - |  5119 | ` */` |
|  473126 |  5120 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5121 | `{` |
|       - |  5122 | `	ph7_value *pLit;` |
|       - |  5123 | `	const char *zLit;` |
|       - |  5124 | `	SyString sQualified;` |
|       - |  5125 | `	sxu32 nLit;` |
|       - |  5126 | `	sxu32 k;` |
|       - |  5127 | `	sxu32 nNewIdx;` |
|       - |  5128 | `	int hasNsSep;` |
|       - |  5129 | `	SyHashEntry *pImport;` |
|       - |  5130 | `	ph7_value *pNew;` |
|  473131 |  5131 | `	if( pFromImport ){` |
|  452783 |  5132 | `		*pFromImport = 0;` |
|  226389 |  5133 | `	}` |
|  473131 |  5134 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  473131 |  5135 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5136 | `		return nOrigIdx;` |
|       - |  5137 | `	}` |
|  473131 |  5138 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  473131 |  5139 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5140 | `	/* Skip if already qualified (contains backslash) */` |
|  473131 |  5141 | `	hasNsSep = 0;` |
| 5224665 |  5142 | `	for( k = 0; k < nLit; k++ ){` |
| 4751547 |  5143 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2375772 |  5144 | `	}` |
|  473131 |  5145 | `	if( hasNsSep ){` |
|      10 |  5146 | `		return nOrigIdx;` |
|       - |  5147 | `	}` |
|       - |  5148 | `	/* Check use imports first (works even outside namespaces) */` |
|  473123 |  5149 | `	SyBlobReset(&pGen->sWorker);` |
|  473123 |  5150 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  473123 |  5151 | `	if( pImport ){` |
|      41 |  5152 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5153 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5154 | `		if( pFromImport ){` |
|      18 |  5155 | `			*pFromImport = 1;` |
|       8 |  5156 | `		}` |
|      23 |  5157 | `	}else{` |
|  473087 |  5158 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  472997 |  5159 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5160 | `		}` |
|       - |  5161 | `		/* Prepend current namespace */` |
|      95 |  5162 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5163 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5164 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5165 | `	}` |
|       - |  5166 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5167 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5168 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5169 | `		return nNewIdx; /* Already interned */` |
|       - |  5170 | `	}` |
|      79 |  5171 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5172 | `	if( pNew == 0 ){` |
|     ! 0 |  5173 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5174 | `	}` |
|      79 |  5175 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5176 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5177 | `	return nNewIdx;` |
|  236568 |  5178 | `}` |
|       - |  5179 | `/*` |
|       - |  5180 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5181 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5182 | ` */` |
|  100038 |  5183 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5184 | `{` |
|       - |  5185 | `	SyHashEntry *pImport;` |
|       - |  5186 | `	/* Check use imports first */` |
|  100043 |  5187 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|  100043 |  5188 | `	if( pImport ){` |
|      15 |  5189 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 |  5190 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      15 |  5191 | `		return;` |
|       - |  5192 | `	}` |
|       - |  5193 | `	/* Prepend current namespace if active */` |
|  100031 |  5194 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5195 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5196 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5197 | `	}` |
|  100031 |  5198 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   50024 |  5199 | `}` |
|       - |  5200 | `/*` |
|       - |  5201 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5202 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5203 | ` * The caller must release pOut when done.` |
|       - |  5204 | ` */` |
|  144490 |  5205 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5206 | `{` |
|  144495 |  5207 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5208 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5209 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5210 | `	}` |
|  144495 |  5211 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  144495 |  5212 | `}` |
|       - |  5213 | `/*` |
|       - |  5214 | ` * Compile a namespace statement` |
|       - |  5215 | ` * According to the PHP language reference manual` |
|       - |  5216 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5217 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5218 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5219 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5220 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5221 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5222 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5223 | ` *  programming world.` |
|       - |  5224 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5225 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5226 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5227 | ` *  classes/functions/constants.` |
|       - |  5228 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5229 | ` *  readability of source code.` |
|       - |  5230 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5231 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5232 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5233 | ` *       class MyClass {}` |
|       - |  5234 | ` *       function myfunction() {}` |
|       - |  5235 | ` *       const MYCONST = 1;` |
|       - |  5236 | ` *       $a = new MyClass;` |
|       - |  5237 | ` *       $c = new \my\name\MyClass;` |
|       - |  5238 | ` *       $a = strlen('hi');` |
|       - |  5239 | ` *       $d = namespace\MYCONST;` |
|       - |  5240 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5241 | ` *       echo constant($d);` |
|       - |  5242 | ` * NOTE` |
|       - |  5243 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5244 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5245 | ` */` |
|       - |  5246 | `/*` |
|       - |  5247 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5248 | ` */` |
|      14 |  5249 | `static const char * TokenTypeName(sxu32 nType)` |
|       3 |  5250 | `{` |
|      17 |  5251 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 |  5252 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 |  5253 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 |  5254 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 |  5255 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 |  5256 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5257 | `	return "token";` |
|      10 |  5258 | `}` |
|     106 |  5259 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5260 | `{` |
|       - |  5261 | `	sxu32 nLine;` |
|       - |  5262 | `	sxi32 rc;` |
|     111 |  5263 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5264 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5265 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5266 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5267 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5268 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5269 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5270 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5271 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5272 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5273 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5274 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5275 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5276 | `		return SXRET_OK;` |
|       - |  5277 | `	}` |
|     111 |  5278 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5279 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5280 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5281 | `		return SXRET_OK;` |
|       - |  5282 | `	}` |
|     111 |  5283 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5284 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5285 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5286 | `		return SXRET_OK;` |
|       - |  5287 | `	}` |
|       - |  5288 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5289 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5290 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5291 | `			/* Append backslash separator */` |
|      26 |  5292 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      26 |  5293 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5294 | `			}` |
|      15 |  5295 | `		}else{` |
|       - |  5296 | `			/* Append identifier */` |
|     131 |  5297 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5298 | `		}` |
|     153 |  5299 | `		pGen->pIn++;` |
|       5 |  5300 | `	}` |
|       - |  5301 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5302 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5303 | `	{` |
|     111 |  5304 | `		char *zNsDup = 0;` |
|     111 |  5305 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5306 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5307 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5308 | `		}` |
|     111 |  5309 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5310 | `	}` |
|     111 |  5311 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5312 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5313 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5314 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5315 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5316 | `			return SXERR_ABORT;` |
|       - |  5317 | `		}` |
|       2 |  5318 | `	}` |
|     111 |  5319 | `	return SXRET_OK;` |
|      58 |  5320 | `}` |
|       - |  5321 | `/*` |
|       - |  5322 | ` * Compile the 'use' statement` |
|       - |  5323 | ` * According to the PHP language reference manual` |
|       - |  5324 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5325 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5326 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5327 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5328 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5329 | ` *  a function or constant is not supported.` |
|       - |  5330 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5331 | ` * NOTE` |
|       - |  5332 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5333 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5334 | ` */` |
|      68 |  5335 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5336 | `{` |
|       - |  5337 | `	sxu32 nLine;` |
|       - |  5338 | `	sxi32 rc;` |
|       - |  5339 | `	SyBlob sPath;` |
|       - |  5340 | `	SyString sAlias;` |
|       - |  5341 | `	SyToken *pLast;` |
|       - |  5342 | `	char *zDup;` |
|       - |  5343 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5344 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5345 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5346 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5347 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5348 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5349 | `	iUseType = 0;` |
|      73 |  5350 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5351 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5352 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5353 | `			iUseType = 1;` |
|      16 |  5354 | `			pGen->pIn++;` |
|      23 |  5355 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5356 | `			iUseType = 2;` |
|      16 |  5357 | `			pGen->pIn++;` |
|       7 |  5358 | `		}` |
|      14 |  5359 | `	}` |
|       - |  5360 | `	/* Select target hash tables based on import type */` |
|      73 |  5361 | `	switch( iUseType ){` |
|       7 |  5362 | `		case 1:` |
|      16 |  5363 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5364 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5365 | `			break;` |
|       7 |  5366 | `		case 2:` |
|      16 |  5367 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5368 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5369 | `			break;` |
|      20 |  5370 | `		default:` |
|      45 |  5371 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5372 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5373 | `			break;` |
|       - |  5374 | `	}` |
|      73 |  5375 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5376 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5377 | `	for(;;){` |
|      75 |  5378 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5379 | `			break;` |
|       - |  5380 | `		}` |
|      75 |  5381 | `		SyBlobReset(&sPath);` |
|      75 |  5382 | `		pLast = 0;` |
|       - |  5383 | `		/* Collect the full namespace path */` |
|     261 |  5384 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5385 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5386 | `				pLast = pGen->pIn;` |
|     131 |  5387 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5388 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5389 | `				}` |
|     131 |  5390 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5391 | `			}` |
|     191 |  5392 | `			pGen->pIn++;` |
|       5 |  5393 | `		}` |
|      75 |  5394 | `		if( pLast == 0 ){` |
|       - |  5395 | `			/* Empty path */` |
|       5 |  5396 | `			break;` |
|       - |  5397 | `		}` |
|       - |  5398 | `		/* Default alias is the last component of the path */` |
|      71 |  5399 | `		sAlias = pLast->sData;` |
|       - |  5400 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5401 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5402 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      19 |  5403 | `			pGen->pIn++; /* Jump 'as' */` |
|      19 |  5404 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      19 |  5405 | `				sAlias = pGen->pIn->sData;` |
|      19 |  5406 | `				pGen->pIn++;` |
|       8 |  5407 | `			}` |
|       8 |  5408 | `		}` |
|       - |  5409 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5410 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5411 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5412 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5413 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5414 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5415 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5416 | `				return SXERR_ABORT;` |
|       - |  5417 | `			}` |
|       2 |  5418 | `		}` |
|       - |  5419 | `		/* Register the import: alias -> FQN.` |
|       - |  5420 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5421 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5422 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5423 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5424 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5425 | `		if( zDup ){` |
|      71 |  5426 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5427 | `			if( pVmHash ){` |
|       - |  5428 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5429 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5430 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5431 | `				if( zAliasDup ){` |
|      43 |  5432 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5433 | `				}` |
|      19 |  5434 | `			}` |
|      71 |  5435 | `			if( iUseType == 2 ){` |
|       - |  5436 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5437 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5438 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5439 | `				if( zAliasDup ){` |
|       - |  5440 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5441 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5442 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5443 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5444 | `					if( azPair ){` |
|      16 |  5445 | `						azPair[0] = zAliasDup;` |
|      16 |  5446 | `						azPair[1] = zDup;` |
|      16 |  5447 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5448 | `					}` |
|       7 |  5449 | `				}` |
|       7 |  5450 | `			}` |
|      33 |  5451 | `		}` |
|       - |  5452 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5453 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5454 | `			pGen->pIn++;` |
|       2 |  5455 | `		}else{` |
|      37 |  5456 | `			break;` |
|       - |  5457 | `		}` |
|       1 |  5458 | `	}` |
|      73 |  5459 | `	SyBlobRelease(&sPath);` |
|      73 |  5460 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5461 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5462 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5463 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5464 | `			return SXERR_ABORT;` |
|       - |  5465 | `		}` |
|       1 |  5466 | `	}` |
|      73 |  5467 | `	return SXRET_OK;` |
|      39 |  5468 | `}` |
|       - |  5469 | `/*` |
|       - |  5470 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5471 | ` *` |
|       - |  5472 | ` * According to the PHP language reference manual.` |
|       - |  5473 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5474 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5475 | ` *  declare (directive)` |
|       - |  5476 | ` *   statement` |
|       - |  5477 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5478 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5479 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5480 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5481 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5482 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5483 | ` * <?php` |
|       - |  5484 | ` * // these are the same:` |
|       - |  5485 | ` * // you can use this:` |
|       - |  5486 | ` * declare(ticks=1) {` |
|       - |  5487 | ` *   // entire script here` |
|       - |  5488 | ` * }` |
|       - |  5489 | ` * // or you can use this:` |
|       - |  5490 | ` * declare(ticks=1);` |
|       - |  5491 | ` * // entire script here` |
|       - |  5492 | ` * ?>` |
|       - |  5493 | ` *` |
|       - |  5494 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5495 | ` */` |
|       - |  5496 | `/*` |
|       - |  5497 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5498 | ` */` |
|      68 |  5499 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5500 | `{` |
|     103 |  5501 | `	return SyStringLength(pName) == nWant` |
|      68 |  5502 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5503 | `}` |
|       - |  5504 |  |
|      40 |  5505 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5506 | `{` |
|      45 |  5507 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5508 | `	SyToken *pBodyEnd = 0;` |
|       - |  5509 | `	SyToken *pBodyStart;` |
|       - |  5510 | `	SyToken *pCursor;` |
|       - |  5511 | `	int bHasStrictTypes;` |
|       - |  5512 | `	int bBlockForm;` |
|       - |  5513 | `	int bPlacementOk;` |
|       - |  5514 | `	sxi32 rc;` |
|      45 |  5515 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5516 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5517 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5518 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5519 | `			return SXERR_ABORT;` |
|       - |  5520 | `		}` |
|       6 |  5521 | `		goto Synchro;` |
|       - |  5522 | `	}` |
|      41 |  5523 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5524 | `	pBodyStart = pGen->pIn;` |
|       - |  5525 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5526 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5527 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5528 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5529 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5530 | `			return SXERR_ABORT;` |
|       - |  5531 | `		}` |
|     ! 0 |  5532 | `		return SXRET_OK;` |
|       - |  5533 | `	}` |
|       - |  5534 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5535 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5536 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5537 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5538 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5539 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5540 | `			return SXERR_ABORT;` |
|       - |  5541 | `		}` |
|     ! 0 |  5542 | `	}` |
|      41 |  5543 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5544 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5545 | `	bHasStrictTypes = 0;` |
|       - |  5546 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5547 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5548 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5549 | `	pCursor = pBodyStart;` |
|      53 |  5550 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5551 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5552 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5553 | `				bHasStrictTypes = 1;` |
|      37 |  5554 | `				break;` |
|       - |  5555 | `			}` |
|       2 |  5556 | `		}` |
|      14 |  5557 | `		pCursor++;` |
|       2 |  5558 | `	}` |
|      41 |  5559 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5560 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5561 | `			"strict_types declaration must not use block mode");` |
|       3 |  5562 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5563 | `		return SXRET_OK;` |
|       - |  5564 | `	}` |
|      39 |  5565 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5566 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5567 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5568 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5569 | `		return SXRET_OK;` |
|       - |  5570 | `	}` |
|       - |  5571 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5572 | `	pCursor = pBodyStart;` |
|      65 |  5573 | `	while( pCursor < pBodyEnd ){` |
|       - |  5574 | `		SyToken *pNameTok;` |
|       - |  5575 | `		SyToken *pEqTok;` |
|       - |  5576 | `		SyToken *pValTok;` |
|       - |  5577 | `		SyString *pDirName;` |
|       - |  5578 | `		int bIsStrict;` |
|       - |  5579 | `		int iStrictValue;` |
|      37 |  5580 | `		pNameTok = pCursor;` |
|      37 |  5581 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5582 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5583 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5584 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5585 | `			return SXRET_OK;` |
|       - |  5586 | `		}` |
|      37 |  5587 | `		pEqTok = pNameTok + 1;` |
|      37 |  5588 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5589 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5590 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5591 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5592 | `			return SXRET_OK;` |
|       - |  5593 | `		}` |
|      37 |  5594 | `		pValTok = pEqTok + 1;` |
|      37 |  5595 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5596 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5597 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5598 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5599 | `			return SXRET_OK;` |
|       - |  5600 | `		}` |
|      37 |  5601 | `		pDirName = &pNameTok->sData;` |
|      37 |  5602 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5603 | `		if( bIsStrict ){` |
|       - |  5604 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5605 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5606 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5607 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5608 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5609 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5610 | `				return SXRET_OK;` |
|       - |  5611 | `			}` |
|      33 |  5612 | `			iStrictValue = -1;` |
|      33 |  5613 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5614 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5615 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5616 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5617 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5618 | `			}` |
|      33 |  5619 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5620 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5621 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5622 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5623 | `				return SXRET_OK;` |
|       - |  5624 | `			}` |
|      30 |  5625 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5626 | `		}else{` |
|       - |  5627 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5628 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5629 | `			 * behavior don't regress. */` |
|       8 |  5630 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5631 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5632 | `				ph7_lib_version()` |
|       - |  5633 | `				);` |
|       - |  5634 | `		}` |
|      35 |  5635 | `		pCursor = pValTok + 1;` |
|       - |  5636 | `		/* Consume separating comma (or end). */` |
|      35 |  5637 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5638 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5639 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5640 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5641 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5642 | `				return SXRET_OK;` |
|       - |  5643 | `			}` |
|       3 |  5644 | `			pCursor++;` |
|       1 |  5645 | `		}` |
|       5 |  5646 | `	}` |
|       - |  5647 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5648 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5649 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5650 | `	return SXRET_OK;` |
|       2 |  5651 | `Synchro:` |
|       - |  5652 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5653 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5654 | `		pGen->pIn++;` |
|       2 |  5655 | `	}` |
|       6 |  5656 | `	return SXRET_OK;` |
|      25 |  5657 | `}` |
|       - |  5658 | `/*` |
|       - |  5659 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5660 | ` * as follows:` |
|       - |  5661 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5662 | ` * {` |
|       - |  5663 | ` *   return "Making a cup of $type.\n";` |
|       - |  5664 | ` * }` |
|       - |  5665 | ` * Symisc eXtension.` |
|       - |  5666 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5667 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5668 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5669 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5670 | ` *      {` |
|       - |  5671 | ` *       var_dump($a);` |
|       - |  5672 | ` *      }` |
|       - |  5673 | ` *     //call test without args` |
|       - |  5674 | ` *      test();` |
|       - |  5675 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5676 | ` *      Example:` |
|       - |  5677 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5678 | ` * 3 -) Function overloading!!` |
|       - |  5679 | ` *      Example:` |
|       - |  5680 | ` *      function foo($a) {` |
|       - |  5681 | ` *   	  return $a.PHP_EOL;` |
|       - |  5682 | ` *	    }` |
|       - |  5683 | ` *	    function foo($a, $b) {` |
|       - |  5684 | ` *   	  return $a + $b;` |
|       - |  5685 | ` *	    }` |
|       - |  5686 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5687 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5688 | ` *      // Same arg` |
|       - |  5689 | ` *	   function foo(string $a)` |
|       - |  5690 | ` *	   {` |
|       - |  5691 | ` *	     echo "a is a string\n";` |
|       - |  5692 | ` *	     var_dump($a);` |
|       - |  5693 | ` *	   }` |
|       - |  5694 | ` *	  function foo(int $a)` |
|       - |  5695 | ` *	  {` |
|       - |  5696 | ` *	    echo "a is integer\n";` |
|       - |  5697 | ` *	    var_dump($a);` |
|       - |  5698 | ` *	  }` |
|       - |  5699 | ` *	  function foo(array $a)` |
|       - |  5700 | ` *	  {` |
|       - |  5701 | ` * 	    echo "a is an array\n";` |
|       - |  5702 | ` * 	    var_dump($a);` |
|       - |  5703 | ` *	  }` |
|       - |  5704 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5705 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5706 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5707 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5708 | ` * introduced by the PH7 engine.` |
|       - |  5709 | ` */` |
|   77070 |  5710 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5711 | `{` |
|       - |  5712 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5713 | `	SySet *pInstrContainer;` |
|       - |  5714 | `	sxi32 rc;` |
|       - |  5715 | `	/* Swap token stream */` |
|   77075 |  5716 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   77075 |  5717 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   77075 |  5718 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5719 | `	/* Compile the expression holding the argument value */` |
|   77075 |  5720 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5721 | `	/* Emit the done instruction */` |
|   77075 |  5722 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   77075 |  5723 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   77075 |  5724 | `	RE_SWAP_DELIMITER(pGen);` |
|   77075 |  5725 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5726 | `		return SXERR_ABORT;` |
|       - |  5727 | `	}` |
|   77075 |  5728 | `	return SXRET_OK;` |
|   38540 |  5729 | `}` |
|       - |  5730 | `/*` |
|       - |  5731 | ` * Collect function arguments one after one.` |
|       - |  5732 | ` * According to the PHP language reference manual.` |
|       - |  5733 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5734 | ` * list of expressions.` |
|       - |  5735 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5736 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5737 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5738 | ` * for more information.` |
|       - |  5739 | ` * Example #1 Passing arrays to functions` |
|       - |  5740 | ` * <?php` |
|       - |  5741 | ` * function takes_array($input)` |
|       - |  5742 | ` * {` |
|       - |  5743 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5744 | ` * }` |
|       - |  5745 | ` * ?>` |
|       - |  5746 | ` * Making arguments be passed by reference` |
|       - |  5747 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5748 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5749 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5750 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5751 | ` * to the argument name in the function definition:` |
|       - |  5752 | ` * Example #2 Passing function parameters by reference` |
|       - |  5753 | ` * <?php` |
|       - |  5754 | ` * function add_some_extra(&$string)` |
|       - |  5755 | ` * {` |
|       - |  5756 | ` *   $string .= 'and something extra.';` |
|       - |  5757 | ` * }` |
|       - |  5758 | ` * $str = 'This is a string, ';` |
|       - |  5759 | ` * add_some_extra($str);` |
|       - |  5760 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5761 | ` * ?>` |
|       - |  5762 | ` *` |
|       - |  5763 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5764 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5765 | ` * on these extension.` |
|       - |  5766 | ` */` |
|  107772 |  5767 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5768 | `{` |
|       - |  5769 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5770 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5771 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5772 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5773 | `	sxi32 rc;` |
|       - |  5774 |  |
|  107777 |  5775 | `	pIn = pGen->pIn;` |
|  107777 |  5776 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5777 | `	/* Process arguments one after one */` |
|  139320 |  5778 | `	for(;;){` |
|  278645 |  5779 | `		if( pIn >= pEnd ){` |
|       - |  5780 | `			/* No more arguments to process */` |
|  107761 |  5781 | `			break;` |
|       - |  5782 | `		}` |
|  170889 |  5783 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  170889 |  5784 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  170889 |  5785 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  170889 |  5786 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5787 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5788 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5789 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5790 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5791 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5792 | `		{` |
|  170889 |  5793 | `			int bReadonly = 0, bVisSeen = 0;` |
|  170889 |  5794 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  170889 |  5795 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5796 | `				bReadonly = 1;` |
|       3 |  5797 | `				pIn++;` |
|       1 |  5798 | `			}` |
|  170889 |  5799 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   66263 |  5800 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   66263 |  5801 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      71 |  5802 | `					bVisSeen = 1;` |
|      71 |  5803 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      95 |  5804 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      31 |  5805 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      71 |  5806 | `					pIn++;` |
|      71 |  5807 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5808 | `						bReadonly = 1;` |
|      16 |  5809 | `						pIn++;` |
|       6 |  5810 | `					}` |
|      33 |  5811 | `				}` |
|   33129 |  5812 | `			}` |
|  170889 |  5813 | `			if( bVisSeen \|\| bReadonly ){` |
|      73 |  5814 | `				if( !bCtorCtx ){` |
|       6 |  5815 | `					if( bAbstractCtx ){` |
|       3 |  5816 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5817 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5818 | `					}else{` |
|       3 |  5819 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5820 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5821 | `					}` |
|       6 |  5822 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5823 | `						return SXERR_ABORT;` |
|       - |  5824 | `					}` |
|       6 |  5825 | `					return SXERR_SYNTAX;` |
|       - |  5826 | `				}` |
|      69 |  5827 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      69 |  5828 | `				sArg.iPromoteVis = iVis;` |
|      69 |  5829 | `				if( bReadonly ){` |
|      18 |  5830 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5831 | `				}` |
|      32 |  5832 | `			}` |
|       - |  5833 | `		}` |
|       - |  5834 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  213230 |  5835 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  129637 |  5836 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   86552 |  5837 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   81013 |  5838 | `			sxu32 nLineLocal = pIn->nLine;` |
|   81013 |  5839 | `			sxi32 iTFlags = 0;` |
|   81013 |  5840 | `			pGen->pIn = pIn;` |
|   81013 |  5841 | `			rc = GenStateParseUnionTypeDecl(` |
|   40504 |  5842 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   40504 |  5843 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5844 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5845 | `				/* bAllowVoid */ 0,` |
|   40504 |  5846 | `						nLineLocal);` |
|   81013 |  5847 | `			pIn = pGen->pIn;` |
|   81013 |  5848 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5849 | `				return SXERR_ABORT;` |
|   81013 |  5850 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5851 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5852 | `				return SXERR_SYNTAX;` |
|   81011 |  5853 | `			}else if( rc == SXERR_SYNTAX ){` |
|      12 |  5854 | `				if( pIn < pEnd ){` |
|      16 |  5855 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5856 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       4 |  5857 | `						&pIn->sData);` |
|       8 |  5858 | `				}else{` |
|     ! 0 |  5859 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5860 | `						"syntax error, unexpected end of file");` |
|       - |  5861 | `				}` |
|      12 |  5862 | `				return SXERR_SYNTAX;` |
|       - |  5863 | `			}` |
|   81003 |  5864 | `			sArg.iFlags \|= iTFlags;` |
|   40499 |  5865 | `		}` |
|  170875 |  5866 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5867 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5868 | `			return rc;` |
|       - |  5869 | `		}` |
|  170875 |  5870 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5871 | `			/* Pass by reference,record that */` |
|    3701 |  5872 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3701 |  5873 | `			pIn++;` |
|    1848 |  5874 | `		}` |
|  170875 |  5875 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5876 | `			/* Variadic parameter: ...$args */` |
|    3717 |  5877 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    3717 |  5878 | `			pIn++;` |
|    1856 |  5879 | `		}` |
|  170875 |  5880 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5881 | `			/* Invalid argument */` |
|     ! 0 |  5882 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5883 | `			return rc;` |
|       - |  5884 | `		}` |
|  170875 |  5885 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5886 | `		/* Copy argument name */` |
|  170875 |  5887 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  170875 |  5888 | `		if( zDup == 0 ){` |
|     ! 0 |  5889 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5890 | `			return SXERR_ABORT;` |
|       - |  5891 | `		}` |
|  170875 |  5892 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  170875 |  5893 | `		pIn++;` |
|  170875 |  5894 | `		if( pIn < pEnd ){` |
|  103505 |  5895 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5896 | `				SyToken *pDefend;` |
|   77077 |  5897 | `				sxi32 iNest = 0;` |
|   77077 |  5898 | `				pIn++; /* Jump the equal sign */` |
|   77077 |  5899 | `				pDefend = pIn;` |
|       - |  5900 | `				/* Process the default value associated with this argument */` |
|  161489 |  5901 | `				while( pDefend < pEnd ){` |
|  121107 |  5902 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   36695 |  5903 | `						break;` |
|       - |  5904 | `					}` |
|   84417 |  5905 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5906 | `						/* Increment nesting level */` |
|    3675 |  5907 | `						iNest++;` |
|   82582 |  5908 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5909 | `						/* Decrement nesting level */` |
|    3675 |  5910 | `						iNest--;` |
|    1835 |  5911 | `					}` |
|   84417 |  5912 | `					pDefend++;` |
|       5 |  5913 | `				}` |
|   77077 |  5914 | `				if( pIn >= pDefend ){` |
|       3 |  5915 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5916 | `					return rc;` |
|       - |  5917 | `				}` |
|       - |  5918 | `				/* Process default value */` |
|   77075 |  5919 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   77075 |  5920 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5921 | `					return rc;` |
|       - |  5922 | `				}` |
|       - |  5923 | `				/* Point beyond the default value */` |
|   77075 |  5924 | `				pIn = pDefend;` |
|   38535 |  5925 | `			}` |
|  103503 |  5926 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5927 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5928 | `				return rc;` |
|       - |  5929 | `			}` |
|  103503 |  5930 | `			pIn++; /* Jump the trailing comma */` |
|   51749 |  5931 | `		}` |
|       - |  5932 | `		/* Append argument signature */` |
|  170873 |  5933 | `		if( sArg.nType > 0 ){` |
|   80949 |  5934 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5935 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   14733 |  5936 | `				int marker = 'o';` |
|   14733 |  5937 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   14733 |  5938 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7369 |  5939 | `			}else{` |
|       - |  5940 | `				int c;` |
|   66221 |  5941 | `				c = 'n'; /* cc warning */` |
|       - |  5942 | `				/* Type leading character */` |
|   66221 |  5943 | `				switch(sArg.nType){` |
|       3 |  5944 | `				case MEMOBJ_HASHMAP:` |
|       - |  5945 | `					/* Hashmap aka 'array' */` |
|       7 |  5946 | `					c = 'h';` |
|       7 |  5947 | `					break;` |
|    9226 |  5948 | `				case MEMOBJ_INT:` |
|       - |  5949 | `					/* Integer */` |
|   18457 |  5950 | `					c = 'i';` |
|   18457 |  5951 | `					break;` |
|       2 |  5952 | `				case MEMOBJ_BOOL:` |
|       - |  5953 | `					/* Bool */` |
|       5 |  5954 | `					c = 'b';` |
|       5 |  5955 | `					break;` |
|       2 |  5956 | `				case MEMOBJ_REAL:` |
|       - |  5957 | `					/* Float */` |
|       5 |  5958 | `					c = 'f';` |
|       5 |  5959 | `					break;` |
|   23867 |  5960 | `				case MEMOBJ_STRING:` |
|       - |  5961 | `					/* String */` |
|   47739 |  5962 | `					c = 's';` |
|   47739 |  5963 | `					break;` |
|       7 |  5964 | `				case MEMOBJ_OBJ:` |
|       - |  5965 | `					/* Object */` |
|      16 |  5966 | `					c = 'o';` |
|      14 |  5967 | `					break;` |
|       1 |  5968 | `				default:` |
|       2 |  5969 | `					break;` |
|       - |  5970 | `				}` |
|   66221 |  5971 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5972 | `			}` |
|   40477 |  5973 | `		}else{` |
|       - |  5974 | `			/* No type is associated with this parameter which mean` |
|       - |  5975 | `			 * that this function is not condidate for overloading.` |
|       - |  5976 | `			 */` |
|   89929 |  5977 | `			SyBlobRelease(&sSig);` |
|       - |  5978 | `		}` |
|       - |  5979 | `		/* Save in the argument set */` |
|  170873 |  5980 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5981 | `	}` |
|  107761 |  5982 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5983 | `		/* Save function signature */` |
|   51571 |  5984 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   25783 |  5985 | `	}` |
|  107761 |  5986 | `	return SXRET_OK;` |
|   53891 |  5987 | `}` |
|       - |  5988 | `/*` |
|       - |  5989 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|       - |  5990 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|       - |  5991 | ` * the enclosing function. Returns the token just past the nested construct.` |
|       - |  5992 | ` */` |
|      14 |  5993 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|       2 |  5994 | `{` |
|      16 |  5995 | `	sxi32 iParen = 0;` |
|      16 |  5996 | `	pIn++; /* past 'function'/'fn' */` |
|       - |  5997 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|       - |  5998 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|       - |  5999 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|      54 |  6000 | `	while( pIn < pEnd ){` |
|      54 |  6001 | `		sxu32 t = pIn->nType;` |
|      54 |  6002 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|      40 |  6003 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|      26 |  6004 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|      12 |  6005 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|      40 |  6006 | `		pIn++;` |
|       2 |  6007 | `	}` |
|      16 |  6008 | `	if( pIn >= pEnd ){ return pIn; }` |
|       - |  6009 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|       - |  6010 | `	{` |
|      16 |  6011 | `		sxi32 d = 0;` |
|     108 |  6012 | `		while( pIn < pEnd ){` |
|     108 |  6013 | `			sxu32 t = pIn->nType;` |
|     108 |  6014 | `			if( t & PH7_TK_OCB ){ d++; }` |
|      94 |  6015 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|      94 |  6016 | `			pIn++;` |
|       2 |  6017 | `		}` |
|       - |  6018 | `	}` |
|      16 |  6019 | `	return pIn;` |
|       9 |  6020 | `}` |
|       - |  6021 | `/*` |
|       - |  6022 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|       - |  6023 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|       - |  6024 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|       - |  6025 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|       - |  6026 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|       - |  6027 | ` * detached-mini-program path untouched.` |
|       - |  6028 | ` */` |
|  229892 |  6029 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|       5 |  6030 | `{` |
|  229897 |  6031 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  229897 |  6032 | `	SyToken *pEnd = pGen->pEnd;` |
|  229897 |  6033 | `	sxi32 iDepth = 0;` |
|  229897 |  6034 | `	int bStarted = 0;` |
| 7635965 |  6035 | `	while( pIn < pEnd ){` |
| 7635965 |  6036 | `		sxu32 t = pIn->nType;` |
| 7635965 |  6037 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 7196213 |  6038 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 6756677 |  6039 | `		if( t & PH7_TK_KEYWORD ){` |
|  535859 |  6040 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  535859 |  6041 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  535715 |  6042 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|       - |  6043 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  267848 |  6044 | `		}` |
| 6756519 |  6045 | `		pIn++;` |
|       5 |  6046 | `	}` |
|  229753 |  6047 | `	return FALSE;` |
|  114951 |  6048 | `}` |
|       - |  6049 | `/*` |
|       - |  6050 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  6051 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  6052 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  6053 | ` */` |
|  229892 |  6054 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  6055 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  6056 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  6057 | `	)` |
|       5 |  6058 | `{` |
|       - |  6059 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  6060 | `	GenBlock *pBlock;` |
|       - |  6061 | `	sxu32 nGotoOfft;` |
|       - |  6062 | `	sxi32 rc;` |
|       - |  6063 | `	/* Attach the new function */` |
|  229897 |  6064 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  229897 |  6065 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6066 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  6067 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6068 | `		return SXERR_ABORT;` |
|       - |  6069 | `	}` |
|  229897 |  6070 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  6071 | `	/* Swap bytecode containers */` |
|  229897 |  6072 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  229897 |  6073 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  6074 | `	/* Emit constructor property promotion prologue:` |
|       - |  6075 | `	 *   $this->NAME = $NAME;` |
|       - |  6076 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  6077 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  6078 | `	{` |
|  229897 |  6079 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  6080 | `		sxu32 i;` |
|  371273 |  6081 | `		for( i = 0; i < nArg; i++ ){` |
|  141381 |  6082 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  6083 | `			char *zSrc;` |
|       - |  6084 | `			sxu32 nSrc,nName;` |
|       - |  6085 | `			SySet sToken;` |
|       - |  6086 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  6087 | `			sxi32 rcPromote;` |
|  141381 |  6088 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  141327 |  6089 | `				continue;` |
|       - |  6090 | `			}` |
|       - |  6091 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  6092 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  6093 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  6094 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  6095 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      59 |  6096 | `			nName = SyStringLength(&pArg->sName);` |
|      59 |  6097 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      59 |  6098 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      59 |  6099 | `			if( zSrc == 0 ){` |
|     ! 0 |  6100 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6101 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6102 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  6103 | `				return SXERR_ABORT;` |
|       - |  6104 | `			}` |
|       - |  6105 | `			{` |
|      59 |  6106 | `				char *z = zSrc;` |
|      59 |  6107 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      59 |  6108 | `				z += sizeof("$this->")-1;` |
|      59 |  6109 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6110 | `				z += nName;` |
|      59 |  6111 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      59 |  6112 | `				z += sizeof(" = $")-1;` |
|      59 |  6113 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6114 | `				z += nName;` |
|      59 |  6115 | `				*z = 0;` |
|       - |  6116 | `			}` |
|      59 |  6117 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      59 |  6118 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      59 |  6119 | `			pTmpIn = pGen->pIn;` |
|      59 |  6120 | `			pTmpEnd = pGen->pEnd;` |
|      59 |  6121 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      59 |  6122 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      59 |  6123 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      59 |  6124 | `			pGen->pIn = pTmpIn;` |
|      59 |  6125 | `			pGen->pEnd = pTmpEnd;` |
|      59 |  6126 | `			SySetRelease(&sToken);` |
|      59 |  6127 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6128 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6129 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6130 | `				return SXERR_ABORT;` |
|       - |  6131 | `			}` |
|       - |  6132 | `			/* Discard the assignment result — this is a statement expression. */` |
|      59 |  6133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      32 |  6134 | `		}` |
|       - |  6135 | `	}` |
|       - |  6136 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|       - |  6137 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|       - |  6138 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|       - |  6139 | `	 * generator — and vice versa — is classified independently. */` |
|       - |  6140 | `	{` |
|  229897 |  6141 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  229897 |  6142 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|       - |  6143 | `		/* Compile the body */` |
|  229897 |  6144 | `		PH7_CompileBlock(&(*pGen),0);` |
|  229897 |  6145 | `		pGen->bInGenerator = bSavedGen;` |
|       - |  6146 | `	}` |
|       - |  6147 | `	/* Fix exception jumps now the destination is resolved */` |
|  229897 |  6148 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6149 | `	/* Emit the final return if not yet done */` |
|  229897 |  6150 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6151 | `	/* Fix gotos jumps now the destination is resolved */` |
|  229897 |  6152 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6153 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6154 | `	}` |
|  229897 |  6155 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6156 | `	/* Restore the default container */` |
|  229897 |  6157 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6158 | `	/* Leave function block */` |
|  229897 |  6159 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  229897 |  6160 | `	if( rc == SXERR_ABORT ){` |
|       - |  6161 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6162 | `		return SXERR_ABORT;` |
|       - |  6163 | `	}` |
|       - |  6164 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6165 | `	{` |
|  229897 |  6166 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6167 | `		sxu32 i;` |
| 4515361 |  6168 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4285613 |  6169 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     149 |  6170 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     149 |  6171 | `				break;` |
|       - |  6172 | `			}` |
| 2142737 |  6173 | `		}` |
|       - |  6174 | `	}` |
|       - |  6175 | `	/* All done, function body compiled */` |
|  229897 |  6176 | `	return SXRET_OK;` |
|  114951 |  6177 | `}` |
|       - |  6178 | `/*` |
|       - |  6179 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6180 | ` * According to the PHP language reference manual.` |
|       - |  6181 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6182 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6183 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6184 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6185 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6186 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6187 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6188 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6189 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6190 | ` *` |
|       - |  6191 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6192 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6193 | ` * on these extension.` |
|       - |  6194 | ` */` |
|       - |  6195 | `/*` |
|       - |  6196 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6197 | ` */` |
|     510 |  6198 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6199 | `{` |
|       - |  6200 | `	sxu32 i;` |
|    1453 |  6201 | `	for( i = 0; i < n; i++ ){` |
|    1247 |  6202 | `		int a = zA[i], b = zB[i];` |
|    1247 |  6203 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1247 |  6204 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1247 |  6205 | `		if( a != b ) return a - b;` |
|     474 |  6206 | `	}` |
|     211 |  6207 | `	return 0;` |
|     260 |  6208 | `}` |
|       - |  6209 | `/*` |
|       - |  6210 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6211 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6212 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6213 | ` */` |
|       - |  6214 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6215 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6216 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6217 |  |
|       - |  6218 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6219 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6220 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6221 |  |
|       - |  6222 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6223 | `struct PhlTypeAtom {` |
|       - |  6224 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6225 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6226 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6227 | `	sxu32 nCanon;` |
|       - |  6228 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6229 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6230 | `};` |
|       - |  6231 |  |
|       - |  6232 | `/*` |
|       - |  6233 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6234 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6235 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6236 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6237 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6238 | ` * already be consumed by the caller.` |
|       - |  6239 | ` */` |
|   81874 |  6240 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6241 | `{` |
|   81879 |  6242 | `	SyToken *pIn = pGen->pIn;` |
|   81879 |  6243 | `	SyZero(pOut, sizeof(*pOut));` |
|   81879 |  6244 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   81879 |  6245 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6246 | `		return SXERR_SYNTAX;` |
|       - |  6247 | `	}` |
|       - |  6248 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   81879 |  6249 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6250 | `		pIn++;` |
|       8 |  6251 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6252 | `			return SXERR_SYNTAX;` |
|       - |  6253 | `		}` |
|       3 |  6254 | `	}` |
|   81879 |  6255 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6256 | `		return SXERR_SYNTAX;` |
|       - |  6257 | `	}` |
|   81879 |  6258 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   66775 |  6259 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   66775 |  6260 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      32 |  6261 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   66761 |  6262 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      71 |  6263 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   66714 |  6264 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   18717 |  6265 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   57325 |  6266 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   47899 |  6267 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   24022 |  6268 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      33 |  6269 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      61 |  6270 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6271 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      33 |  6272 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       9 |  6273 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      19 |  6274 | `			pOut->nType = SXU32_HIGH;` |
|      19 |  6275 | `			pOut->sClass = pIn->sData;` |
|      11 |  6276 | `		}else{` |
|       3 |  6277 | `			return SXERR_SYNTAX;` |
|       - |  6278 | `		}` |
|   66773 |  6279 | `		pIn++;` |
|   33389 |  6280 | `	}else{` |
|       - |  6281 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6282 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   15109 |  6283 | `		SyString *pT = &pIn->sData;` |
|   15109 |  6284 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6285 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6286 | `			pIn++;` |
|   15095 |  6287 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     157 |  6288 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     157 |  6289 | `			pIn++;` |
|   15005 |  6290 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|      24 |  6291 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|      24 |  6292 | `			pIn++;` |
|      14 |  6293 | `		}else{` |
|       - |  6294 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   14909 |  6295 | `			SyToken *pFirst = pIn;` |
|   14909 |  6296 | `			SyToken *pLast = pIn;` |
|   14909 |  6297 | `			pOut->nType = SXU32_HIGH;` |
|   14909 |  6298 | `			pOut->sClass = pIn->sData;` |
|   14909 |  6299 | `			pIn++;` |
|   22359 |  6300 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   14912 |  6301 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6302 | `				pLast = &pIn[1];` |
|       3 |  6303 | `				pIn += 2;` |
|       1 |  6304 | `			}` |
|   14909 |  6305 | `			if( pLast != pFirst ){` |
|       3 |  6306 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6307 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6308 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6309 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6310 | `			}` |
|       - |  6311 | `		}` |
|       - |  6312 | `	}` |
|   81877 |  6313 | `	pGen->pIn = pIn;` |
|   81877 |  6314 | `	return SXRET_OK;` |
|   40942 |  6315 | `}` |
|       - |  6316 |  |
|       - |  6317 | `/*` |
|       - |  6318 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6319 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6320 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6321 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6322 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6323 | ` */` |
|   81714 |  6324 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6325 | `{` |
|       - |  6326 | `	int i;` |
|   81719 |  6327 | `	int nNonNull = 0;` |
|   81719 |  6328 | `	int bAnyIntersection = 0;` |
|       - |  6329 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   81719 |  6330 | `	sxu32 nMaxGroup = 0;` |
| 2696567 |  6331 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  163567 |  6332 | `	for( i = 0; i < nAtoms; i++ ){` |
|   81853 |  6333 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   81825 |  6334 | `			nNonNull++;` |
|   81825 |  6335 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   81825 |  6336 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   81825 |  6337 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   40910 |  6338 | `			}` |
|   40910 |  6339 | `		}` |
|   40929 |  6340 | `	}` |
|  163533 |  6341 | `	for( i = 0; i < nAtoms; i++ ){` |
|   81835 |  6342 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      20 |  6343 | `			bAnyIntersection = 1;` |
|      20 |  6344 | `			break;` |
|       - |  6345 | `		}` |
|   40912 |  6346 | `	}` |
|   81719 |  6347 | `	if( bAnyIntersection ){` |
|       - |  6348 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6349 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6350 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      20 |  6351 | `		sxu32 g, nGroups = 0;` |
|      20 |  6352 | `		int bFirstGroup = 1;` |
|      40 |  6353 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      40 |  6354 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      24 |  6355 | `			int bFirstMember = 1;` |
|       - |  6356 | `			int bWrap;` |
|      24 |  6357 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6358 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6359 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6360 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6361 | `			 * parens, matching PHP's canonical text. */` |
|      32 |  6362 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      24 |  6363 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      24 |  6364 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      72 |  6365 | `			for( i = 0; i < nAtoms; i++ ){` |
|      52 |  6366 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      40 |  6367 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      40 |  6368 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      38 |  6369 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      21 |  6370 | `				}else{` |
|       3 |  6371 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6372 | `				}` |
|      40 |  6373 | `				bFirstMember = 0;` |
|      22 |  6374 | `			}` |
|      24 |  6375 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      24 |  6376 | `			bFirstGroup = 0;` |
|      14 |  6377 | `		}` |
|      20 |  6378 | `		if( bNullable ){` |
|     ! 0 |  6379 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6380 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6381 | `		}` |
|      58 |  6382 | `		return;` |
|       - |  6383 | `	}` |
|   81703 |  6384 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6385 | `		/* Shorthand: ?T */` |
|      81 |  6386 | `		for( i = 0; i < nAtoms; i++ ){` |
|      81 |  6387 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      81 |  6388 | `			SyBlobAppend(pBlob, "?", 1);` |
|      81 |  6389 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      21 |  6390 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      12 |  6391 | `			}else{` |
|      62 |  6392 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6393 | `			}` |
|      81 |  6394 | `			return;` |
|     ! 0 |  6395 | `		}` |
|     ! 0 |  6396 | `	}` |
|       - |  6397 | `	{` |
|   81627 |  6398 | `		int bFirst = 1;` |
|       - |  6399 | `		/* 1) Classes in declaration order */` |
|  163351 |  6400 | `		for( i = 0; i < nAtoms; i++ ){` |
|   81729 |  6401 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   14873 |  6402 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   14873 |  6403 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   14873 |  6404 | `				bFirst = 0;` |
|    7434 |  6405 | `			}` |
|   40867 |  6406 | `		}` |
|       - |  6407 | `		/* 2) Built-ins in canonical order */` |
|       - |  6408 | `		{` |
|       - |  6409 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6410 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6411 | `			int k;` |
|  571359 |  6412 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  913297 |  6413 | `				for( i = 0; i < nAtoms; i++ ){` |
|  490241 |  6414 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   66681 |  6415 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   66681 |  6416 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   66681 |  6417 | `						bFirst = 0;` |
|   66681 |  6418 | `						break;` |
|       - |  6419 | `					}` |
|  211785 |  6420 | `				}` |
|  244871 |  6421 | `			}` |
|       - |  6422 | `		}` |
|       - |  6423 | `		/* 3) null suffix */` |
|   81627 |  6424 | `		if( bNullable ){` |
|      20 |  6425 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      20 |  6426 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6427 | `		}` |
|       - |  6428 | `	}` |
|   40862 |  6429 | `}` |
|       - |  6430 |  |
|       - |  6431 | `/*` |
|       - |  6432 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6433 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6434 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6435 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6436 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6437 | ` * whether it was parenthesized.` |
|       - |  6438 | ` *` |
|       - |  6439 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6440 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6441 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6442 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6443 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6444 | ` */` |
|   81856 |  6445 | `static sxi32 GenStateParsePart(` |
|       - |  6446 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6447 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6448 | `{` |
|       - |  6449 | `	sxi32 rc;` |
|   81861 |  6450 | `	int nMembers = 0;` |
|   81861 |  6451 | `	int bParen = 0;` |
|   81861 |  6452 | `	*pnMembers = 0;` |
|   81861 |  6453 | `	*pbParen = 0;` |
|   81861 |  6454 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6455 | `		bParen = 1;` |
|       6 |  6456 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6457 | `	}` |
|   40928 |  6458 | `	for(;;){` |
|   81879 |  6459 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6460 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6461 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6462 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6463 | `		}` |
|   81879 |  6464 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   81879 |  6465 | `		if( rc != SXRET_OK ){` |
|       3 |  6466 | `			return rc;` |
|       - |  6467 | `		}` |
|   81877 |  6468 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   81877 |  6469 | `		(*pnAtoms)++;` |
|   81877 |  6470 | `		nMembers++;` |
|       - |  6471 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   81877 |  6472 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      24 |  6473 | `			SyToken *pNext = &pGen->pIn[1];` |
|      20 |  6474 | `			if( pNext < pGen->pEnd` |
|      24 |  6475 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      22 |  6476 | `				pGen->pIn++; /* skip '&' */` |
|      22 |  6477 | `				continue;` |
|       - |  6478 | `			}` |
|       1 |  6479 | `		}` |
|   81859 |  6480 | `		break;` |
|     ! 0 |  6481 | `	}` |
|   81859 |  6482 | `	if( bParen ){` |
|       6 |  6483 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6484 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6485 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6486 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6487 | `		}` |
|       6 |  6488 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6489 | `		if( nMembers < 2 ){` |
|     ! 0 |  6490 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6491 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6492 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6493 | `		}` |
|       2 |  6494 | `	}` |
|   81859 |  6495 | `	*pnMembers = nMembers;` |
|   81859 |  6496 | `	*pbParen = bParen;` |
|   81859 |  6497 | `	return SXRET_OK;` |
|   40933 |  6498 | `}` |
|       - |  6499 |  |
|       - |  6500 | `/*` |
|       - |  6501 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6502 | ` *` |
|       - |  6503 | ` * Outputs:` |
|       - |  6504 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6505 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6506 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6507 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6508 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6509 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6510 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6511 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6512 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6513 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6514 | ` *` |
|       - |  6515 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6516 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6517 | ` */` |
|   81730 |  6518 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6519 | `	ph7_gen_state *pGen,` |
|       - |  6520 | `	sxu32 *pnType,` |
|       - |  6521 | `	SyString *pClass,` |
|       - |  6522 | `	SySet *pAlts,` |
|       - |  6523 | `	sxi32 *piTypeFlags,` |
|       - |  6524 | `	SyString *pTypeText,` |
|       - |  6525 | `	int iNullableFlag,` |
|       - |  6526 | `	int iUnionFlag,` |
|       - |  6527 | `	int bAllowVoid,` |
|       - |  6528 | `	sxu32 nLine` |
|       5 |  6529 | `){` |
|       - |  6530 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   81735 |  6531 | `	int nAtoms = 0;` |
|   81735 |  6532 | `	int bShortNullable = 0;` |
|   81735 |  6533 | `	int bExplicitNull = 0;` |
|       - |  6534 | `	sxi32 rc;` |
|   81735 |  6535 | `	*pnType = 0;` |
|   81735 |  6536 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   81735 |  6537 | `	*piTypeFlags = 0;` |
|   81735 |  6538 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6539 |  |
|   81735 |  6540 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6541 | `		return SXRET_OK;` |
|       - |  6542 | `	}` |
|       - |  6543 | ``	/* Optional `?` shorthand prefix */`` |
|   81730 |  6544 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      71 |  6545 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      71 |  6546 | `		bShortNullable = 1;` |
|      71 |  6547 | `		pGen->pIn++;` |
|      71 |  6548 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6549 | `			return SXERR_SYNTAX;` |
|       - |  6550 | `		}` |
|      33 |  6551 | `	}` |
|       - |  6552 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6553 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6554 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6555 | `	{` |
|       - |  6556 | `		int nMembers, bParen;` |
|   81735 |  6557 | `		sxu32 iGroup = 0;` |
|   81735 |  6558 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   81735 |  6559 | `		if( rc != SXRET_OK ){` |
|       4 |  6560 | `			return rc;` |
|       - |  6561 | `		}` |
|       - |  6562 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6563 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6564 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6565 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6566 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  122783 |  6567 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   81924 |  6568 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     133 |  6569 | `			if( bShortNullable ){` |
|       - |  6570 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6571 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6572 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6573 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6574 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6575 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6576 | `			}` |
|     131 |  6577 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6578 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6579 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6580 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6581 | `			}` |
|     131 |  6582 | ``			pGen->pIn++; /* skip `\|` */`` |
|     131 |  6583 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     131 |  6584 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6585 | `				return rc;` |
|       - |  6586 | `			}` |
|       5 |  6587 | `		}` |
|   81731 |  6588 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6589 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6590 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6591 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6592 | `		}` |
|       - |  6593 | `	}` |
|       - |  6594 | `	/* Validation pass.` |
|       - |  6595 | `	 *` |
|       - |  6596 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6597 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6598 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6599 | `	 */` |
|       - |  6600 | `	{` |
|       - |  6601 | `		int i, j;` |
|   81731 |  6602 | `		int bHasNonNull = 0;` |
|   81731 |  6603 | `		int bAnyIntersection = 0;` |
|       - |  6604 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6605 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6606 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2696963 |  6607 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  163601 |  6608 | `		for( i = 0; i < nAtoms; i++ ){` |
|   81875 |  6609 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   40940 |  6610 | `		}` |
|  163563 |  6611 | `		for( i = 0; i < nAtoms; i++ ){` |
|   81855 |  6612 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   40921 |  6613 | `		}` |
|       - |  6614 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6615 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   81731 |  6616 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6617 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6618 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6619 | `			return SXERR_SYNTAX;` |
|       - |  6620 | `		}` |
|  163587 |  6621 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6622 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6623 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6624 | ``			 * `true`/`false` in an intersection). */`` |
|   81873 |  6625 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      38 |  6626 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      38 |  6627 | `				if( bClassLike ){` |
|      36 |  6628 | `					SyString *pC = &aAtoms[i].sClass;` |
|      32 |  6629 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      32 |  6630 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      32 |  6631 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      36 |  6632 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6633 | `						bClassLike = 0;` |
|     ! 0 |  6634 | `					}` |
|      16 |  6635 | `				}` |
|      38 |  6636 | `				if( !bClassLike ){` |
|       - |  6637 | `					const char *zName; sxu32 nName;` |
|       3 |  6638 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6639 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6640 | `					}else{` |
|       3 |  6641 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6642 | `					}` |
|       4 |  6643 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6644 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6645 | `						(int)nName, zName);` |
|       3 |  6646 | `					return SXERR_SYNTAX;` |
|       - |  6647 | `				}` |
|      16 |  6648 | `			}` |
|   81871 |  6649 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     157 |  6650 | `				if( nAtoms > 1 ){` |
|       3 |  6651 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6652 | `						"Void can only be used as a standalone type");` |
|       3 |  6653 | `					return SXERR_SYNTAX;` |
|       - |  6654 | `				}` |
|     155 |  6655 | `				if( !bAllowVoid ){` |
|     ! 0 |  6656 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6657 | `						"void cannot be used here");` |
|     ! 0 |  6658 | `					return SXERR_SYNTAX;` |
|       - |  6659 | `				}` |
|     155 |  6660 | `				if( bShortNullable ){` |
|     ! 0 |  6661 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6662 | `						"Void type cannot be nullable");` |
|     ! 0 |  6663 | `					return SXERR_SYNTAX;` |
|       - |  6664 | `				}` |
|      75 |  6665 | `			}` |
|   81869 |  6666 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6667 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|       - |  6668 | `				 * type (never = the function does not return). Mirrors the void` |
|       - |  6669 | `				 * validation above; accepted here and enforced at compile time` |
|       - |  6670 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|      24 |  6671 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|       - |  6672 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|       - |  6673 | `					 * same as any other non-standalone use. */` |
|       5 |  6674 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6675 | `						"never can only be used as a standalone type");` |
|       5 |  6676 | `					return SXERR_SYNTAX;` |
|       - |  6677 | `				}` |
|      19 |  6678 | `				if( !bAllowVoid ){` |
|       - |  6679 | `					/* Return-only: params call with bAllowVoid=0. */` |
|       3 |  6680 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6681 | `						"never cannot be used as a parameter type");` |
|       3 |  6682 | `					return SXERR_SYNTAX;` |
|       - |  6683 | `				}` |
|       7 |  6684 | `			}` |
|   81863 |  6685 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  6686 | `				bExplicitNull = 1;` |
|      18 |  6687 | `			}else{` |
|   81835 |  6688 | `				bHasNonNull = 1;` |
|       - |  6689 | `			}` |
|       - |  6690 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  6691 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  6692 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  6693 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  6694 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   82043 |  6695 | `			for( j = 0; j < i; j++ ){` |
|     187 |  6696 | `				int bDup = 0;` |
|     187 |  6697 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     359 |  6698 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     182 |  6699 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     187 |  6700 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     179 |  6701 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      41 |  6702 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      34 |  6703 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      38 |  6704 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      16 |  6705 | `								aAtoms[j].sClass.zString,` |
|      32 |  6706 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6707 | `							bDup = 1;` |
|     ! 0 |  6708 | `						}` |
|      22 |  6709 | `					}else{` |
|       3 |  6710 | `						bDup = 1;` |
|       - |  6711 | `					}` |
|      18 |  6712 | `				}` |
|     179 |  6713 | `				if( bDup ){` |
|       - |  6714 | `					const char *zName;` |
|       - |  6715 | `					sxu32 nName;` |
|       3 |  6716 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6717 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6718 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6719 | `					}else{` |
|       3 |  6720 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6721 | `						nName = aAtoms[i].nCanon;` |
|       - |  6722 | `					}` |
|       4 |  6723 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6724 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6725 | `					return SXERR_SYNTAX;` |
|       - |  6726 | `				}` |
|      91 |  6727 | `			}` |
|   40933 |  6728 | `		}` |
|   81719 |  6729 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6730 | `			if( bShortNullable ){` |
|       - |  6731 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6732 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6733 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6734 | `				return SXERR_SYNTAX;` |
|       - |  6735 | `			}` |
|       - |  6736 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6737 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6738 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6739 | `			 * atom, so set it here. */` |
|       7 |  6740 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6741 | `		}` |
|       - |  6742 | `	}` |
|       - |  6743 | `	/* Compute nullability flag */` |
|   81719 |  6744 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      97 |  6745 | `		*piTypeFlags \|= iNullableFlag;` |
|      46 |  6746 | `	}` |
|       - |  6747 | `	/* Build canonical type text */` |
|   81719 |  6748 | `	if( pTypeText ){` |
|       - |  6749 | `		SyBlob sBlob;` |
|   81719 |  6750 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  122544 |  6751 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   40857 |  6752 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   81719 |  6753 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  122330 |  6754 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   81550 |  6755 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   81555 |  6756 | `			if( zDup ){` |
|   81555 |  6757 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   40775 |  6758 | `			}` |
|   40775 |  6759 | `		}` |
|   81719 |  6760 | `		SyBlobRelease(&sBlob);` |
|   40857 |  6761 | `	}` |
|       - |  6762 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6763 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6764 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6765 | `	{` |
|   81719 |  6766 | `		int nNonNull = 0;` |
|   81719 |  6767 | `		int iNonNullIdx = -1;` |
|       - |  6768 | `		int i;` |
|  163567 |  6769 | `		for( i = 0; i < nAtoms; i++ ){` |
|   81853 |  6770 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   81825 |  6771 | `				nNonNull++;` |
|   81825 |  6772 | `				iNonNullIdx = i;` |
|   40910 |  6773 | `			}` |
|   40929 |  6774 | `		}` |
|   81719 |  6775 | `		if( nNonNull <= 1 ){` |
|       - |  6776 | `			/* Fast path: store as single type. */` |
|   81627 |  6777 | `			if( iNonNullIdx >= 0 ){` |
|   81621 |  6778 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   81621 |  6779 | `				if( pA->nType == SXU32_HIGH ){` |
|   22274 |  6780 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7423 |  6781 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   14851 |  6782 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   14851 |  6783 | `					*pnType = SXU32_HIGH;` |
|   14851 |  6784 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   74198 |  6785 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     155 |  6786 | `					*pnType = MEMOBJ_VOID;` |
|   66700 |  6787 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|      16 |  6788 | `					*pnType = MEMOBJ_NEVER;` |
|       9 |  6789 | `				}else{` |
|   66611 |  6790 | `					*pnType = pA->nType;` |
|       - |  6791 | `				}` |
|   40808 |  6792 | `			}` |
|   40816 |  6793 | `		}else{` |
|       - |  6794 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      97 |  6795 | `			*piTypeFlags \|= iUnionFlag;` |
|     311 |  6796 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6797 | `				ph7_type_alt sAlt;` |
|     219 |  6798 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     209 |  6799 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     209 |  6800 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     209 |  6801 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     116 |  6802 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      37 |  6803 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      79 |  6804 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      79 |  6805 | `					sAlt.nType = SXU32_HIGH;` |
|      79 |  6806 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      42 |  6807 | `				}else{` |
|     135 |  6808 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  6809 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6810 | `				}` |
|     209 |  6811 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     107 |  6812 | `			}` |
|       - |  6813 | `		}` |
|       - |  6814 | `	}` |
|   81719 |  6815 | `	return SXRET_OK;` |
|   40870 |  6816 | `}` |
|       - |  6817 |  |
|       - |  6818 | `/*` |
|       - |  6819 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6820 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6821 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6822 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6823 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6824 | `` *          and union types `: T\|U`.`` |
|       - |  6825 | ` */` |
|  325504 |  6826 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6827 | `{` |
|  325509 |  6828 | `	sxi32 iFlags = 0;` |
|       - |  6829 | `	sxi32 rc;` |
|       - |  6830 | `	sxu32 nLine;` |
|  325509 |  6831 | `	pFunc->nReturnType = 0;` |
|  325509 |  6832 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  325509 |  6833 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  325509 |  6834 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  325009 |  6835 | `		return SXRET_OK;` |
|       - |  6836 | `	}` |
|     505 |  6837 | `	pGen->pIn++; /* Skip ':' */` |
|     505 |  6838 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6839 | `		return SXRET_OK;` |
|       - |  6840 | `	}` |
|     505 |  6841 | `	nLine = pGen->pIn->nLine;` |
|     505 |  6842 | `	rc = GenStateParseUnionTypeDecl(` |
|     250 |  6843 | `		pGen,` |
|     250 |  6844 | `		&pFunc->nReturnType,` |
|     250 |  6845 | `		&pFunc->sReturnClass,` |
|     250 |  6846 | `		&pFunc->aReturnUnion,` |
|       - |  6847 | `		&iFlags,` |
|     250 |  6848 | `		&pFunc->sReturnTypeName,` |
|       - |  6849 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  6850 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  6851 | `		/* iUnionFlag */ 0,` |
|       - |  6852 | `		/* bAllowVoid */ 1,` |
|     250 |  6853 | `		nLine);` |
|     505 |  6854 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6855 | `		return SXERR_ABORT;` |
|       - |  6856 | `	}` |
|     505 |  6857 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6858 | `		/* Error already reported */` |
|     ! 0 |  6859 | `		return SXERR_SYNTAX;` |
|       - |  6860 | `	}` |
|     505 |  6861 | `	if( rc == SXERR_SYNTAX ){` |
|       8 |  6862 | `		if( pGen->pIn < pGen->pEnd ){` |
|      11 |  6863 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6864 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       6 |  6865 | `				&pGen->pIn->sData);` |
|       5 |  6866 | `		}else{` |
|     ! 0 |  6867 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6868 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6869 | `		}` |
|       8 |  6870 | `		return SXERR_SYNTAX;` |
|       - |  6871 | `	}` |
|     499 |  6872 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     499 |  6873 | `	return SXRET_OK;` |
|  162757 |  6874 | `}` |
|       - |  6875 |  |
|   49100 |  6876 | `static sxi32 GenStateCompileFunc(` |
|       - |  6877 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6878 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6879 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6880 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6881 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6882 | `	)` |
|       5 |  6883 | `{` |
|       - |  6884 | `	ph7_vm_func *pFunc;` |
|       - |  6885 | `	SyToken *pEnd;` |
|       - |  6886 | `	sxu32 nLine;` |
|       - |  6887 | `	char *zName;` |
|       - |  6888 | `	sxi32 rc;` |
|       - |  6889 | `	/* Extract line number */` |
|   49105 |  6890 | `	nLine = pGen->pIn->nLine;` |
|       - |  6891 | `	/* Jump the left parenthesis '(' */` |
|   49105 |  6892 | `	pGen->pIn++;` |
|       - |  6893 | `	/* Delimit the function signature */` |
|   49105 |  6894 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   49105 |  6895 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6896 | `		/* Syntax error */` |
|       8 |  6897 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       8 |  6898 | `		if( rc == SXERR_ABORT ){` |
|       - |  6899 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6900 | `			return SXERR_ABORT;` |
|       - |  6901 | `		}` |
|       8 |  6902 | `		pGen->pIn = pGen->pEnd;` |
|       8 |  6903 | `		return SXRET_OK;` |
|       - |  6904 | `	}` |
|       - |  6905 | `	/* Create the function state */` |
|   49099 |  6906 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   49099 |  6907 | `	if( pFunc == 0 ){` |
|     ! 0 |  6908 | `		goto OutOfMem;` |
|       - |  6909 | `	}` |
|       - |  6910 | `	/* Build the function name, prepending namespace if active */` |
|   49106 |  6911 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6912 | `		SyBlob sFQN;` |
|       - |  6913 | `		sxu32 nLen;` |
|      16 |  6914 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6915 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6916 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6917 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6918 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6919 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6920 | `		SyBlobRelease(&sFQN);` |
|      16 |  6921 | `		if( zName == 0 ){` |
|     ! 0 |  6922 | `			goto OutOfMem;` |
|       - |  6923 | `		}` |
|      16 |  6924 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6925 | `	}else{` |
|   49085 |  6926 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   49085 |  6927 | `		if( zName == 0 ){` |
|     ! 0 |  6928 | `			goto OutOfMem;` |
|       - |  6929 | `		}` |
|   49085 |  6930 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6931 | `	}` |
|   49099 |  6932 | `	if( pGen->pIn < pEnd ){` |
|       - |  6933 | `		/* Collect function arguments */` |
|   33847 |  6934 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   33847 |  6935 | `		if( rc == SXERR_ABORT ){` |
|       - |  6936 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6937 | `			return SXERR_ABORT;` |
|       - |  6938 | `		}` |
|   16921 |  6939 | `	}` |
|       - |  6940 | `	/* Point past ')' and parse optional return type ': type' */` |
|   49099 |  6941 | `	pGen->pIn = &pEnd[1];` |
|       - |  6942 | `	{` |
|   49099 |  6943 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   49099 |  6944 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6945 | `			return SXERR_ABORT;` |
|   49099 |  6946 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       8 |  6947 | `			return SXERR_SYNTAX;` |
|       - |  6948 | `		}` |
|       - |  6949 | `	}` |
|   49093 |  6950 | `	if( bHandleClosure ){` |
|       - |  6951 | `		ph7_vm_func_closure_env sEnv;` |
|     299 |  6952 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     294 |  6953 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     161 |  6954 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      23 |  6955 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6956 | `				/* Closure,record environment variable */` |
|      23 |  6957 | `				pGen->pIn++;` |
|      23 |  6958 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6959 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6960 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6961 | `						return SXERR_ABORT;` |
|       - |  6962 | `					}` |
|     ! 0 |  6963 | `				}` |
|      23 |  6964 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6965 | `				/* Compile until we hit the first closing parenthesis */` |
|      45 |  6966 | `				while( pGen->pIn < pGen->pEnd ){` |
|      45 |  6967 | `					int iFlagsLocal = 0;` |
|      45 |  6968 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      23 |  6969 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      23 |  6970 | `						break;` |
|       - |  6971 | `					}` |
|      27 |  6972 | `					nLineLocal = pGen->pIn->nLine;` |
|      27 |  6973 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6974 | `						/* Pass by reference,record that */` |
|     ! 0 |  6975 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6976 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6977 | `							);` |
|     ! 0 |  6978 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6979 | `						pGen->pIn++;` |
|     ! 0 |  6980 | `					}` |
|      22 |  6981 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      27 |  6982 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6983 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6984 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6985 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6986 | `								return SXERR_ABORT;` |
|       - |  6987 | `							}` |
|       - |  6988 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6989 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6990 | `								pGen->pIn++;` |
|     ! 0 |  6991 | `							}` |
|     ! 0 |  6992 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6993 | `								pGen->pIn++;` |
|     ! 0 |  6994 | `							}` |
|     ! 0 |  6995 | `							break;` |
|       - |  6996 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6997 | `					}else{` |
|       - |  6998 | `						SyString *pNameLocal;` |
|       - |  6999 | `						char *zDup;` |
|       - |  7000 | `						/* Duplicate variable name */` |
|      27 |  7001 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      27 |  7002 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      27 |  7003 | `						if( zDup ){` |
|       - |  7004 | `							/* Zero the structure */` |
|      27 |  7005 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      27 |  7006 | `							sEnv.iFlags = iFlagsLocal;` |
|      27 |  7007 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      27 |  7008 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      27 |  7009 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  7010 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  7011 | `									got_this = 1;` |
|     ! 0 |  7012 | `							}` |
|       - |  7013 | `							/* Save imported variable */` |
|      27 |  7014 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      16 |  7015 | `						}else{` |
|     ! 0 |  7016 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7017 | `							 return SXERR_ABORT;` |
|       - |  7018 | `						}` |
|       - |  7019 | `					}` |
|      27 |  7020 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      33 |  7021 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7022 | `						/* Ignore trailing commas */` |
|       7 |  7023 | `						pGen->pIn++;` |
|       1 |  7024 | `					}` |
|       5 |  7025 | `				}` |
|      23 |  7026 | `				if( !got_this ){` |
|       - |  7027 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  7028 | `					 * available to the closure environment.` |
|       - |  7029 | `					 */` |
|      23 |  7030 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      23 |  7031 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      23 |  7032 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      23 |  7033 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      23 |  7034 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 |  7035 | `				}` |
|      23 |  7036 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  7037 | `					/* Mark as closure */` |
|      23 |  7038 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       9 |  7039 | `				}` |
|       9 |  7040 | `		}` |
|     147 |  7041 | `	}` |
|       - |  7042 | `	/* Compile the body */` |
|   49093 |  7043 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   49093 |  7044 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7045 | `		return SXERR_ABORT;` |
|       - |  7046 | `	}` |
|   49093 |  7047 | `	if( ppFunc ){` |
|     299 |  7048 | `		*ppFunc = pFunc;` |
|     147 |  7049 | `	}` |
|   49093 |  7050 | `	rc = SXRET_OK;` |
|   49093 |  7051 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  7052 | `		/* Finally register the function */` |
|   49075 |  7053 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   24535 |  7054 | `	}` |
|   49093 |  7055 | `	if( rc == SXRET_OK ){` |
|   49093 |  7056 | `		return SXRET_OK;` |
|       - |  7057 | `	}` |
|       - |  7058 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  7059 | `OutOfMem:` |
|       - |  7060 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  7061 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  7062 | `	 */` |
|     ! 0 |  7063 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  7064 | `	return SXERR_ABORT;` |
|   24555 |  7065 | `}` |
|       - |  7066 | `/*` |
|       - |  7067 | ` * Compile a standard PHP function.` |
|       - |  7068 | ` *  Refer to the block-comment above for more information.` |
|       - |  7069 | ` */` |
|   48814 |  7070 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  7071 | `{` |
|       - |  7072 | `	SyString *pName;` |
|       - |  7073 | `	sxi32 iFlags;` |
|       - |  7074 | `	sxu32 nLine;` |
|       - |  7075 | `	sxi32 rc;` |
|       - |  7076 |  |
|   48819 |  7077 | `	nLine = pGen->pIn->nLine;` |
|   48819 |  7078 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   48819 |  7079 | `	iFlags = 0;` |
|   48819 |  7080 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7081 | `		/* Return by reference,remember that */` |
|       7 |  7082 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7083 | `		/* Jump the '&' token */` |
|       7 |  7084 | `		pGen->pIn++;` |
|       3 |  7085 | `	}` |
|   48819 |  7086 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7087 | `		/* Invalid function name */` |
|       8 |  7088 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       8 |  7089 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7090 | `			return SXERR_ABORT;` |
|       - |  7091 | `		}` |
|       - |  7092 | `		/* Sychronize with the next semi-colon or braces*/` |
|      22 |  7093 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      16 |  7094 | `			pGen->pIn++;` |
|       2 |  7095 | `		}` |
|       8 |  7096 | `		return SXRET_OK;` |
|       - |  7097 | `	}` |
|   48813 |  7098 | `	pName = &pGen->pIn->sData;` |
|   48813 |  7099 | `	nLine = pGen->pIn->nLine;` |
|       - |  7100 | `	/* Jump the function name */` |
|   48813 |  7101 | `	pGen->pIn++;` |
|   48813 |  7102 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7103 | `		/* Syntax error */` |
|       3 |  7104 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  7105 | `		if( rc == SXERR_ABORT ){` |
|       - |  7106 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7107 | `			return SXERR_ABORT;` |
|       - |  7108 | `		}` |
|       - |  7109 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  7110 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  7111 | `			pGen->pIn++;` |
|     ! 0 |  7112 | `		}` |
|       3 |  7113 | `		return SXRET_OK;` |
|       - |  7114 | `	}` |
|       - |  7115 | `	/* Compile function body */` |
|   48811 |  7116 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   48811 |  7117 | `	return rc;` |
|   24412 |  7118 | `}` |
|       - |  7119 | `/*` |
|       - |  7120 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7121 | ` * According to the PHP language reference manual` |
|       - |  7122 | ` *  Visibility:` |
|       - |  7123 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7124 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7125 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7126 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7127 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7128 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7129 | ` */` |
|  354060 |  7130 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7131 | `{` |
|  354065 |  7132 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   22123 |  7133 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  331947 |  7134 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   47743 |  7135 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7136 | `	}` |
|       - |  7137 | `	/* Assume public by default */` |
|  284209 |  7138 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  177035 |  7139 | `}` |
|       - |  7140 | `/*` |
|       - |  7141 | ` * Compile a class constant.` |
|       - |  7142 | ` * According to the PHP language reference manual` |
|       - |  7143 | ` *  Class Constants` |
|       - |  7144 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7145 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7146 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7147 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7148 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7149 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7150 | ` * Symisc eXtension.` |
|       - |  7151 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7152 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7153 | ` *  Example:` |
|       - |  7154 | ` *   class Test{` |
|       - |  7155 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7156 | ` *   };` |
|       - |  7157 | ` *   var_dump(TEST::MyConst);` |
|       - |  7158 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7159 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7160 | ` */` |
|       - |  7161 | `/*` |
|       - |  7162 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7163 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7164 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7165 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7166 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7167 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7168 | ` */` |
|      92 |  7169 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7170 | `{` |
|       - |  7171 | `	SyToken *p0, *p1;` |
|      97 |  7172 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7173 | `		return 0;` |
|       - |  7174 | `	}` |
|      97 |  7175 | `	p0 = pGen->pIn;` |
|       - |  7176 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      97 |  7177 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7178 | `		return 1;` |
|       - |  7179 | `	}` |
|      97 |  7180 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7181 | `		return 1;` |
|       - |  7182 | `	}` |
|       - |  7183 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7184 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7185 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      93 |  7186 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      93 |  7187 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      93 |  7188 | `		if( p1 ){` |
|      93 |  7189 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      34 |  7190 | `				return 1;` |
|       - |  7191 | `			}` |
|      62 |  7192 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7193 | `				return 1;` |
|       - |  7194 | `			}` |
|      27 |  7195 | `		}` |
|      27 |  7196 | `	}` |
|      58 |  7197 | `	return 0;` |
|      51 |  7198 | `}` |
|       - |  7199 | `/*` |
|       - |  7200 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|       - |  7201 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|       - |  7202 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|       - |  7203 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|       - |  7204 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|       - |  7205 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|       - |  7206 | ` * Peek only; never consumes tokens.` |
|       - |  7207 | ` */` |
|      24 |  7208 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|       4 |  7209 | `{` |
|      28 |  7210 | `	SyToken *p = pGen->pIn;` |
|      39 |  7211 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      20 |  7212 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|       3 |  7213 | `		p++; /* skip leading unary sign(s) */` |
|       1 |  7214 | `	}` |
|      28 |  7215 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|      23 |  7216 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|       - |  7217 | `	}` |
|       6 |  7218 | `	p++;` |
|       - |  7219 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|       6 |  7220 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|      16 |  7221 | `}` |
|       - |  7222 | `/*` |
|       - |  7223 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|       - |  7224 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|       - |  7225 | `` * `$o->new`), not a `new` expression.`` |
|       - |  7226 | ` */` |
|       6 |  7227 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|       3 |  7228 | `{` |
|       - |  7229 | `	sxi32 iOp;` |
|       9 |  7230 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|     ! 0 |  7231 | `		return 0;` |
|       - |  7232 | `	}` |
|       9 |  7233 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       9 |  7234 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|       6 |  7235 | `}` |
|       - |  7236 | `/*` |
|       - |  7237 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|       - |  7238 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|       - |  7239 | ` * interface-constant and (instance/static) property-default initializers` |
|       - |  7240 | ` * ("New expressions are not supported in this context") while still allowing it` |
|       - |  7241 | ` * in global constants, parameter defaults and static-local initializers (which` |
|       - |  7242 | ` * are compiled by different functions and left untouched). The scan is` |
|       - |  7243 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|       - |  7244 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|       - |  7245 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|       - |  7246 | ` *` |
|       - |  7247 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|       - |  7248 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|       - |  7249 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|       - |  7250 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|       - |  7251 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|       - |  7252 | ` */` |
|   22596 |  7253 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|       5 |  7254 | `{` |
|   22601 |  7255 | `	SyToken *p = pGen->pIn;` |
|   22601 |  7256 | `	int iDepth = 0;` |
|   68003 |  7257 | `	while( p < pGen->pEnd ){` |
|   68003 |  7258 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   22593 |  7259 | `			break; /* end of this initializer */` |
|       - |  7260 | `		}` |
|   45415 |  7261 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   22715 |  7262 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      10 |  7263 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  7264 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|       - |  7265 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|       - |  7266 | `			 * expression. */` |
|       3 |  7267 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|       3 |  7268 | `			p++;` |
|       3 |  7269 | `			if( bArrow ){` |
|       - |  7270 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|       - |  7271 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|       3 |  7272 | `				int iBase = iDepth;` |
|      17 |  7273 | `				while( p < pGen->pEnd ){` |
|      17 |  7274 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       5 |  7275 | `						iDepth++;` |
|      15 |  7276 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       5 |  7277 | `						if( iDepth <= iBase ){` |
|     ! 0 |  7278 | `							break; /* closes an enclosing group, not the fn's own */` |
|       - |  7279 | `						}` |
|       5 |  7280 | `						iDepth--;` |
|      11 |  7281 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|       3 |  7282 | `						break;` |
|       - |  7283 | `					}` |
|      15 |  7284 | `					p++;` |
|       1 |  7285 | `				}` |
|       2 |  7286 | `			}else{` |
|       - |  7287 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|       - |  7288 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|       - |  7289 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|       - |  7290 | `				 * then skip the balanced brace block. */` |
|     ! 0 |  7291 | `				int iLocal = 0;` |
|     ! 0 |  7292 | `				while( p < pGen->pEnd ){` |
|     ! 0 |  7293 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|     ! 0 |  7294 | `						break; /* body brace */` |
|       - |  7295 | `					}` |
|     ! 0 |  7296 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  7297 | `						iLocal++;` |
|     ! 0 |  7298 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  7299 | `						if( iLocal > 0 ){` |
|     ! 0 |  7300 | `							iLocal--;` |
|     ! 0 |  7301 | `						}` |
|     ! 0 |  7302 | `					}` |
|     ! 0 |  7303 | `					p++;` |
|     ! 0 |  7304 | `				}` |
|     ! 0 |  7305 | `				if( p < pGen->pEnd ){` |
|     ! 0 |  7306 | `					int iBrace = 0; /* p is on the body '{' */` |
|     ! 0 |  7307 | `					while( p < pGen->pEnd ){` |
|     ! 0 |  7308 | `						if( p->nType & PH7_TK_OCB ){` |
|     ! 0 |  7309 | `							iBrace++;` |
|     ! 0 |  7310 | `						}else if( p->nType & PH7_TK_CCB ){` |
|     ! 0 |  7311 | `							iBrace--;` |
|     ! 0 |  7312 | `							if( iBrace == 0 ){` |
|     ! 0 |  7313 | `								p++;` |
|     ! 0 |  7314 | `								break;` |
|       - |  7315 | `							}` |
|     ! 0 |  7316 | `						}` |
|     ! 0 |  7317 | `						p++;` |
|     ! 0 |  7318 | `					}` |
|     ! 0 |  7319 | `				}` |
|       - |  7320 | `			}` |
|       3 |  7321 | `			continue;` |
|       - |  7322 | `		}` |
|   45413 |  7323 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      69 |  7324 | `			iDepth++;` |
|   45381 |  7325 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      67 |  7326 | `			if( iDepth > 0 ){` |
|      67 |  7327 | `				iDepth--;` |
|      31 |  7328 | `			}` |
|   45318 |  7329 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   22579 |  7330 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|       - |  7331 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|       - |  7332 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|       - |  7333 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|      11 |  7334 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|      11 |  7335 | `				return 1;` |
|       - |  7336 | `			}` |
|     ! 0 |  7337 | `		}` |
|   45405 |  7338 | `		p++;` |
|       5 |  7339 | `	}` |
|   22593 |  7340 | `	return 0;` |
|   11303 |  7341 | `}` |
|       - |  7342 | `/*` |
|       - |  7343 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7344 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7345 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7346 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7347 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7348 | ` * share the same backing.` |
|       - |  7349 | ` */` |
|     212 |  7350 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7351 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7352 | `{` |
|     217 |  7353 | `	pAttr->nType = nType;` |
|     217 |  7354 | `	pAttr->sClass = *pClass;` |
|     217 |  7355 | `	pAttr->sTypeName = *pTypeName;` |
|     217 |  7356 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7357 | `		sxu32 i;` |
|      66 |  7358 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      46 |  7359 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      46 |  7360 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      25 |  7361 | `		}` |
|      10 |  7362 | `	}` |
|     217 |  7363 | `}` |
|      92 |  7364 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7365 | `{` |
|      97 |  7366 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7367 | `	SySet *pInstrContainer;` |
|       - |  7368 | `	ph7_class_attr *pCons;` |
|       - |  7369 | `	SyString *pName;` |
|       - |  7370 | `	sxi32 rc;` |
|      97 |  7371 | `	sxu32 nType = 0;` |
|       - |  7372 | `	SyString sTypeClass;` |
|       - |  7373 | `	SyString sTypeText;` |
|       - |  7374 | `	SySet aUnionAlts;` |
|      97 |  7375 | `	sxi32 iTypeFlags = 0;` |
|      97 |  7376 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      97 |  7377 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      97 |  7378 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7379 | `	/* Extract visibility level */` |
|      97 |  7380 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7381 | `	/* Mark as constant */` |
|      97 |  7382 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      97 |  7383 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7384 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7385 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|     116 |  7386 | `	if( GenStateClassConstHasType(pGen) ){` |
|      61 |  7387 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      38 |  7388 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7389 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7390 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7391 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7392 | `		 * and success paths release. */` |
|      42 |  7393 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7394 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7395 | `			goto Synchronize;` |
|      42 |  7396 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7397 | `			return SXERR_ABORT;` |
|      42 |  7398 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7399 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7400 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7401 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7402 | `				return SXERR_ABORT;` |
|       - |  7403 | `			}` |
|     ! 0 |  7404 | `			goto Synchronize;` |
|       - |  7405 | `		}` |
|      42 |  7406 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      19 |  7407 | `	}` |
|      46 |  7408 | `loop:` |
|      99 |  7409 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7410 | `		/* Invalid constant name */` |
|     ! 0 |  7411 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7412 | `		if( rc == SXERR_ABORT ){` |
|       - |  7413 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7414 | `			return SXERR_ABORT;` |
|       - |  7415 | `		}` |
|     ! 0 |  7416 | `		goto Synchronize;` |
|       - |  7417 | `	}` |
|       - |  7418 | `	/* Peek constant name */` |
|      99 |  7419 | `	pName = &pGen->pIn->sData;` |
|       - |  7420 | `	/* Make sure the constant name isn't reserved */` |
|      99 |  7421 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7422 | `		/* Reserved constant name */` |
|     ! 0 |  7423 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7424 | `		if( rc == SXERR_ABORT ){` |
|       - |  7425 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7426 | `			return SXERR_ABORT;` |
|       - |  7427 | `		}` |
|     ! 0 |  7428 | `		goto Synchronize;` |
|       - |  7429 | `	}` |
|       - |  7430 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      99 |  7431 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      61 |  7432 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      38 |  7433 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      19 |  7434 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      42 |  7435 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7436 | `			return SXERR_ABORT;` |
|      42 |  7437 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7438 | `			goto Synchronize;` |
|       - |  7439 | `		}` |
|      18 |  7440 | `	}` |
|       - |  7441 | `	/* Advance the stream cursor */` |
|      97 |  7442 | `	pGen->pIn++;` |
|      97 |  7443 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7444 | `		/* Invalid declaration */` |
|     ! 0 |  7445 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7446 | `		if( rc == SXERR_ABORT ){` |
|       - |  7447 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7448 | `			return SXERR_ABORT;` |
|       - |  7449 | `		}` |
|     ! 0 |  7450 | `		goto Synchronize;` |
|       - |  7451 | `	}` |
|      97 |  7452 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7453 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|       - |  7454 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|       - |  7455 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|       - |  7456 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|     104 |  7457 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|      39 |  7458 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|       8 |  7459 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7460 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|       2 |  7461 | `			&pClass->sName,pName,&sTypeText);` |
|       6 |  7462 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7463 | `			return SXERR_ABORT;` |
|       - |  7464 | `		}` |
|       6 |  7465 | `		goto Synchronize;` |
|       - |  7466 | `	}` |
|       - |  7467 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|       - |  7468 | `	 * constant initializer ("New expressions are not supported in this context").` |
|       - |  7469 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|      93 |  7470 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|       5 |  7471 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7472 | `			"New expressions are not supported in this context");` |
|       5 |  7473 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7474 | `			return SXERR_ABORT;` |
|       - |  7475 | `		}` |
|       5 |  7476 | `		goto Synchronize;` |
|       - |  7477 | `	}` |
|       - |  7478 | `	/* Allocate a new class attribute */` |
|      89 |  7479 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      89 |  7480 | `	if( pCons == 0 ){` |
|     ! 0 |  7481 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7482 | `		return SXERR_ABORT;` |
|       - |  7483 | `	}` |
|      89 |  7484 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      35 |  7485 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      16 |  7486 | `	}` |
|       - |  7487 | `	/* Swap bytecode container */` |
|      89 |  7488 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      89 |  7489 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7490 | `	/* Compile constant value.` |
|       - |  7491 | `	 */` |
|      89 |  7492 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      89 |  7493 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7494 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7495 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7496 | `			return SXERR_ABORT;` |
|       - |  7497 | `		}` |
|       1 |  7498 | `	}` |
|       - |  7499 | `	/* Emit the done instruction */` |
|      89 |  7500 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      89 |  7501 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      89 |  7502 | `	if( rc == SXERR_ABORT ){` |
|       - |  7503 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7504 | `		return SXERR_ABORT;` |
|       - |  7505 | `	}` |
|       - |  7506 | `	/* All done,install the constant */` |
|      89 |  7507 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      89 |  7508 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7509 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7510 | `		return SXERR_ABORT;` |
|       - |  7511 | `	}` |
|      89 |  7512 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7513 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7514 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7515 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7516 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7517 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7518 | `				pTok--;` |
|     ! 0 |  7519 | `			}` |
|     ! 0 |  7520 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7521 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7522 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7523 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7524 | `				return SXERR_ABORT;` |
|       - |  7525 | `			}` |
|     ! 0 |  7526 | `		}else{` |
|       3 |  7527 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7528 | `				goto loop;` |
|       - |  7529 | `			}` |
|       - |  7530 | `		}` |
|     ! 0 |  7531 | `	}` |
|      87 |  7532 | `	SySetRelease(&aUnionAlts);` |
|      87 |  7533 | `	return SXRET_OK;` |
|       5 |  7534 | `Synchronize:` |
|      13 |  7535 | `	SySetRelease(&aUnionAlts);` |
|       - |  7536 | `	/* Synchronize with the first semi-colon */` |
|      45 |  7537 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      35 |  7538 | `		pGen->pIn++;` |
|       3 |  7539 | `	}` |
|      13 |  7540 | `	return SXERR_CORRUPT;` |
|      51 |  7541 | `}` |
|       - |  7542 | `/*` |
|       - |  7543 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7544 | ` * According to the PHP language reference manual` |
|       - |  7545 | ` *  Properties` |
|       - |  7546 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7547 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7548 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7549 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7550 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7551 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7552 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7553 | ` * Symisc eXtension.` |
|       - |  7554 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7555 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7556 | ` *  Example:` |
|       - |  7557 | ` *   class Test{` |
|       - |  7558 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7559 | ` *   };` |
|       - |  7560 | ` *   var_dump(TEST::myVar);` |
|       - |  7561 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7562 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7563 | ` */` |
|       - |  7564 | `/*` |
|       - |  7565 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7566 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7567 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7568 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7569 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7570 | ` */` |
|  191858 |  7571 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7572 | `{` |
|  191863 |  7573 | `	SyToken *p = pStart;` |
|  191863 |  7574 | `	int bFirst = 1;` |
|  191863 |  7575 | `	if( p >= pEnd ) return 0;` |
|       - |  7576 | ``	/* Optional nullable `?` shorthand. */`` |
|  191863 |  7577 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      19 |  7578 | `		p++;` |
|      19 |  7579 | `		if( p >= pEnd ) return 0;` |
|       8 |  7580 | `	}` |
|       - |  7581 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7582 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7583 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7584 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   95929 |  7585 | `	for(;;){` |
|  191881 |  7586 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7587 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7588 | `			p++;` |
|       9 |  7589 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7590 | `			if( p >= pEnd ) return 0;` |
|       3 |  7591 | `			p++; /* skip ')' */` |
|       2 |  7592 | `		}else{` |
|       - |  7593 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7594 | ``			 * then any `&`-joined intersection members. */`` |
|  191879 |  7595 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  191879 |  7596 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7597 | `				return 0;` |
|       - |  7598 | `			}` |
|       - |  7599 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7600 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7601 | `			 * may still appear at the initial dispatch site). */` |
|  191879 |  7602 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  191833 |  7603 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  191905 |  7604 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   11226 |  7605 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  191679 |  7606 | `					return 0;` |
|       - |  7607 | `				}` |
|      77 |  7608 | `			}` |
|     205 |  7609 | `			p++;` |
|     207 |  7610 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7611 | `				p += 2;` |
|       1 |  7612 | `			}` |
|     303 |  7613 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     208 |  7614 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7615 | `				p++; /* skip '&' */` |
|       3 |  7616 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7617 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7618 | `				p++;` |
|       3 |  7619 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7620 | `					p += 2;` |
|     ! 0 |  7621 | `				}` |
|       1 |  7622 | `			}` |
|       - |  7623 | `		}` |
|     207 |  7624 | `		bFirst = 0;` |
|     202 |  7625 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7626 | `			&& p->sData.zString[0] == '\|' ){` |
|      22 |  7627 | ``			p++; /* next `\|`-separated part */`` |
|      22 |  7628 | `			continue;` |
|       - |  7629 | `		}` |
|     189 |  7630 | `		break;` |
|     ! 0 |  7631 | `	}` |
|     189 |  7632 | `	if( p >= pEnd ) return 0;` |
|     189 |  7633 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   95934 |  7634 | `}` |
|       - |  7635 |  |
|       - |  7636 | `/*` |
|       - |  7637 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7638 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7639 | ` * if not). Recognized forms:` |
|       - |  7640 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7641 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7642 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7643 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7644 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7645 | ` * on unrecoverable error.` |
|       - |  7646 | ` *` |
|       - |  7647 | ` * When a type is parsed:` |
|       - |  7648 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7649 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7650 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7651 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7652 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7653 | ` */` |
|     184 |  7654 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7655 | `	ph7_gen_state *pGen,` |
|       - |  7656 | `	sxu32 *pnType,` |
|       - |  7657 | `	SyString *pClass,` |
|       - |  7658 | `	sxi32 *piTypeFlags,` |
|       - |  7659 | `	SyString *pTypeText,` |
|       - |  7660 | `	SySet *pAlts` |
|       5 |  7661 | `){` |
|     189 |  7662 | `	sxi32 iFlags = 0;` |
|       - |  7663 | `	sxi32 rc;` |
|     189 |  7664 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7665 | `		return SXRET_OK;` |
|       - |  7666 | `	}` |
|       - |  7667 | `	/* If the first token is '$', there's no type */` |
|     189 |  7668 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7669 | `		return SXRET_OK;` |
|       - |  7670 | `	}` |
|     189 |  7671 | `	rc = GenStateParseUnionTypeDecl(` |
|      92 |  7672 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7673 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7674 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7675 | `		/* bAllowVoid */ 0,` |
|     184 |  7676 | `		pGen->pIn->nLine);` |
|     189 |  7677 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7678 | `		return rc;` |
|       - |  7679 | `	}` |
|       - |  7680 | `	/* Verify next token is '$' (start of property name) */` |
|     189 |  7681 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7682 | `		return SXERR_SYNTAX;` |
|       - |  7683 | `	}` |
|     189 |  7684 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     189 |  7685 | `	return SXRET_OK;` |
|      97 |  7686 | `}` |
|       - |  7687 |  |
|       - |  7688 | `/*` |
|       - |  7689 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7690 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7691 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7692 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7693 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7694 | ` * by the type parser itself before reaching here.` |
|       - |  7695 | ` *` |
|       - |  7696 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7697 | ` * use in the error message.` |
|       - |  7698 | ` */` |
|     336 |  7699 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7700 | `	sxu32 nType,` |
|       - |  7701 | `	const SyString *pClass,` |
|       - |  7702 | `	const char **pzName,` |
|       - |  7703 | `	sxu32 *pnName)` |
|       5 |  7704 | `{` |
|       - |  7705 | `	const char *z;` |
|       - |  7706 | `	sxu32 n;` |
|     341 |  7707 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     287 |  7708 | `		return 0;` |
|       - |  7709 | `	}` |
|      59 |  7710 | `	z = pClass->zString;` |
|      59 |  7711 | `	n = pClass->nByte;` |
|      59 |  7712 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7713 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7714 | `	}` |
|       - |  7715 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7716 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7717 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      53 |  7718 | `	return 0;` |
|     173 |  7719 | `}` |
|       - |  7720 |  |
|       - |  7721 | `/*` |
|       - |  7722 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7723 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7724 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7725 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7726 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7727 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7728 | ` *` |
|       - |  7729 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7730 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7731 | ` */` |
|     278 |  7732 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7733 | `	ph7_gen_state *pGen,` |
|       - |  7734 | `	ph7_class *pClass,` |
|       - |  7735 | `	const SyString *pMemberName,` |
|       - |  7736 | `	sxu32 nType,` |
|       - |  7737 | `	const SyString *pTypeClass,` |
|       - |  7738 | `	const SyString *pTypeText,` |
|       - |  7739 | `	SySet *pUnionAlts,` |
|       - |  7740 | `	const char *zErrFmt,` |
|       - |  7741 | `	sxu32 nLine)` |
|       5 |  7742 | `{` |
|     283 |  7743 | `	const char *zBad = 0;` |
|     283 |  7744 | `	sxu32 nBad = 0;` |
|       - |  7745 | `	SyString sFallback;` |
|       - |  7746 | `	const SyString *pBad;` |
|       - |  7747 | `	sxi32 rc;` |
|     283 |  7748 | `	int bDisallowed = 0;` |
|     283 |  7749 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7750 | `		bDisallowed = 1;` |
|     281 |  7751 | `	}else if( pUnionAlts ){` |
|       - |  7752 | `		sxu32 i;` |
|      88 |  7753 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      62 |  7754 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      62 |  7755 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7756 | `				bDisallowed = 1;` |
|       3 |  7757 | `				break;` |
|       - |  7758 | `			}` |
|      32 |  7759 | `		}` |
|      14 |  7760 | `	}` |
|     283 |  7761 | `	if( !bDisallowed ){` |
|     277 |  7762 | `		return SXRET_OK;` |
|       - |  7763 | `	}` |
|       - |  7764 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7765 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7766 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7767 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7768 | `		pBad = pTypeText;` |
|       5 |  7769 | `	}else{` |
|     ! 0 |  7770 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7771 | `		pBad = &sFallback;` |
|       - |  7772 | `	}` |
|      11 |  7773 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7774 | `		zErrFmt,` |
|       3 |  7775 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7776 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7777 | `		return SXERR_ABORT;` |
|       - |  7778 | `	}` |
|       8 |  7779 | `	return SXERR_SYNTAX;` |
|     144 |  7780 | `}` |
|       - |  7781 | `/*` |
|       - |  7782 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7783 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7784 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7785 | ` * than promoted to a lexer keyword.` |
|       - |  7786 | ` */` |
| 1698294 |  7787 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7788 | `{` |
| 1733013 |  7789 | `	return (pTok->nType & PH7_TK_ID)` |
|  883861 |  7790 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1733008 |  7791 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7792 | `}` |
|   77726 |  7793 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7794 | `{` |
|   77731 |  7795 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7796 | `	ph7_class_attr *pAttr;` |
|       - |  7797 | `	SyString *pName;` |
|       - |  7798 | `	sxi32 rc;` |
|   77731 |  7799 | `	sxu32 nType = 0;` |
|       - |  7800 | `	SyString sTypeClass;` |
|       - |  7801 | `	SyString sTypeText;` |
|       - |  7802 | `	SySet aUnionAlts;` |
|   77731 |  7803 | `	sxi32 iTypeFlags = 0;` |
|   77731 |  7804 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   77731 |  7805 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   77731 |  7806 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7807 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7808 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7809 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   77731 |  7810 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7811 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7812 | `	}` |
|       - |  7813 | `	/* Extract visibility level */` |
|   77731 |  7814 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7815 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   77823 |  7816 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     189 |  7817 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     189 |  7818 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7819 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7820 | `			goto Synchronize;` |
|     189 |  7821 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7822 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7823 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7824 | `				&pGen->pIn->sData);` |
|     ! 0 |  7825 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7826 | `				return SXERR_ABORT;` |
|       - |  7827 | `			}` |
|     ! 0 |  7828 | `			goto Synchronize;` |
|     189 |  7829 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7830 | `			return SXERR_ABORT;` |
|       - |  7831 | `		}` |
|      92 |  7832 | `	}` |
|     ! 0 |  7833 | `loop:` |
|   77735 |  7834 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7835 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7836 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7837 | `			return SXERR_ABORT;` |
|       - |  7838 | `		}` |
|     ! 0 |  7839 | `		goto Synchronize;` |
|       - |  7840 | `	}` |
|   77735 |  7841 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   77735 |  7842 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7843 | `		/* Invalid attribute name */` |
|     ! 0 |  7844 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7845 | `		if( rc == SXERR_ABORT ){` |
|       - |  7846 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7847 | `			return SXERR_ABORT;` |
|       - |  7848 | `		}` |
|     ! 0 |  7849 | `		goto Synchronize;` |
|       - |  7850 | `	}` |
|       - |  7851 | `	/* Peek attribute name */` |
|   77735 |  7852 | `	pName = &pGen->pIn->sData;` |
|       - |  7853 | `	/* Advance the stream cursor */` |
|   77735 |  7854 | `	pGen->pIn++;` |
|   77735 |  7855 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7856 | `		/* Invalid declaration */` |
|       3 |  7857 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7858 | `		if( rc == SXERR_ABORT ){` |
|       - |  7859 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7860 | `			return SXERR_ABORT;` |
|       - |  7861 | `		}` |
|       3 |  7862 | `		goto Synchronize;` |
|       - |  7863 | `	}` |
|       - |  7864 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7865 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   77733 |  7866 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7867 | `		const char *zRoErr = 0;` |
|      39 |  7868 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7869 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7870 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7871 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7872 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7873 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7874 | `		}` |
|      39 |  7875 | `		if( zRoErr ){` |
|      13 |  7876 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7877 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7878 | `				return SXERR_ABORT;` |
|       - |  7879 | `			}` |
|      13 |  7880 | `			goto Synchronize;` |
|       - |  7881 | `		}` |
|      12 |  7882 | `	}` |
|       - |  7883 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7884 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7885 | `	 * by the type parser. */` |
|   77723 |  7886 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     278 |  7887 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7888 | `			&sTypeText,` |
|     182 |  7889 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      91 |  7890 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     187 |  7891 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7892 | `			return SXERR_ABORT;` |
|     187 |  7893 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7894 | `			goto Synchronize;` |
|       - |  7895 | `		}` |
|      91 |  7896 | `	}` |
|       - |  7897 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   77723 |  7898 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7899 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7900 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7901 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7902 | `			return SXERR_ABORT;` |
|       - |  7903 | `		}` |
|       3 |  7904 | `		goto Synchronize;` |
|       - |  7905 | `	}` |
|       - |  7906 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|       - |  7907 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|       - |  7908 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|       - |  7909 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|       - |  7910 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|       - |  7911 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   77721 |  7912 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|       6 |  7913 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7914 | `			"New expressions are not supported in this context");` |
|       6 |  7915 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7916 | `			return SXERR_ABORT;` |
|       - |  7917 | `		}` |
|       6 |  7918 | `		goto Synchronize;` |
|       - |  7919 | `	}` |
|       - |  7920 | `	/* Allocate a new class attribute */` |
|   77717 |  7921 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   77717 |  7922 | `	if( pAttr == 0 ){` |
|     ! 0 |  7923 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7924 | `		return SXERR_ABORT;` |
|       - |  7925 | `	}` |
|   77717 |  7926 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     185 |  7927 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      90 |  7928 | `	}` |
|   77717 |  7929 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7930 | `		SySet *pInstrContainer;` |
|   22509 |  7931 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7932 | `		/* Swap bytecode container */` |
|   22509 |  7933 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   22509 |  7934 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7935 | `		/* Compile attribute value.` |
|       - |  7936 | `		 */` |
|   22509 |  7937 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   22509 |  7938 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7939 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7940 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7941 | `				return SXERR_ABORT;` |
|       - |  7942 | `			}` |
|     ! 0 |  7943 | `		}` |
|       - |  7944 | `		/* Emit the done instruction */` |
|   22509 |  7945 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   22509 |  7946 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   11252 |  7947 | `	}` |
|       - |  7948 | `	/* All done,install the attribute */` |
|   77717 |  7949 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   77717 |  7950 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7951 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7952 | `		return SXERR_ABORT;` |
|       - |  7953 | `	}` |
|   77717 |  7954 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7955 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7956 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7957 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7958 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7959 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7960 | `				pTok--;` |
|     ! 0 |  7961 | `			}` |
|     ! 0 |  7962 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7963 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7964 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7965 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7966 | `				return SXERR_ABORT;` |
|       - |  7967 | `			}` |
|     ! 0 |  7968 | `		}else{` |
|       5 |  7969 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7970 | `				goto loop;` |
|       - |  7971 | `			}` |
|       - |  7972 | `		}` |
|     ! 0 |  7973 | `	}` |
|   77713 |  7974 | `	SySetRelease(&aUnionAlts);` |
|   77713 |  7975 | `	return SXRET_OK;` |
|       9 |  7976 | `Synchronize:` |
|       - |  7977 | `	/* Synchronize with the first semi-colon */` |
|      56 |  7978 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      37 |  7979 | `		pGen->pIn++;` |
|       3 |  7980 | `	}` |
|      22 |  7981 | `	SySetRelease(&aUnionAlts);` |
|      22 |  7982 | `	return SXERR_CORRUPT;` |
|   38868 |  7983 | `}` |
|       - |  7984 | `/*` |
|       - |  7985 | ` * Compile a class method.` |
|       - |  7986 | ` *` |
|       - |  7987 | ` * Refer to the official documentation for more information` |
|       - |  7988 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7989 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7990 | ` * overloading and many more.` |
|       - |  7991 | ` */` |
|  276242 |  7992 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7993 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7994 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7995 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7996 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7997 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7998 | `	)` |
|       5 |  7999 | `{` |
|  276247 |  8000 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8001 | `	ph7_class_method *pMeth;` |
|       - |  8002 | `	sxi32 iFuncFlags;` |
|       - |  8003 | `	SyString *pName;` |
|       - |  8004 | `	SyToken *pEnd;` |
|       - |  8005 | `	sxi32 rc;` |
|       - |  8006 | `	/* Extract visibility level */` |
|  276247 |  8007 | `	iProtection = GetProtectionLevel(iProtection);` |
|  276247 |  8008 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  276247 |  8009 | `	iFuncFlags = 0;` |
|  276247 |  8010 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8011 | `		/* Invalid method name */` |
|     ! 0 |  8012 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8013 | `		if( rc == SXERR_ABORT ){` |
|       - |  8014 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8015 | `			return SXERR_ABORT;` |
|       - |  8016 | `		}` |
|     ! 0 |  8017 | `		goto Synchronize;` |
|       - |  8018 | `	}` |
|  276247 |  8019 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  8020 | `		/* Return by reference,remember that */` |
|     ! 0 |  8021 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  8022 | `		/* Jump the '&' token */` |
|     ! 0 |  8023 | `		pGen->pIn++;` |
|     ! 0 |  8024 | `	}` |
|  276247 |  8025 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  8026 | `		/* Invalid method name */` |
|     ! 0 |  8027 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8028 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8029 | `			return SXERR_ABORT;` |
|       - |  8030 | `		}` |
|     ! 0 |  8031 | `		goto Synchronize;` |
|       - |  8032 | `	}` |
|       - |  8033 | `	/* Peek method name */` |
|  276247 |  8034 | `	pName = &pGen->pIn->sData;` |
|  276247 |  8035 | `	nLine = pGen->pIn->nLine;` |
|       - |  8036 | `	/* Jump the method name */` |
|  276247 |  8037 | `	pGen->pIn++;` |
|  276247 |  8038 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  8039 | `		/* Abstract method */` |
|   95431 |  8040 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  8041 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8042 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  8043 | `				&pClass->sName,pName);` |
|     ! 0 |  8044 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8045 | `				return SXERR_ABORT;` |
|       - |  8046 | `			}` |
|     ! 0 |  8047 | `		}` |
|       - |  8048 | `		/* Assemble method signature only */` |
|   95431 |  8049 | `		doBody = FALSE;` |
|   47713 |  8050 | `	}` |
|  276247 |  8051 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  8052 | `		/* Syntax error */` |
|     ! 0 |  8053 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  8054 | `		if( rc == SXERR_ABORT ){` |
|       - |  8055 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8056 | `			return SXERR_ABORT;` |
|       - |  8057 | `		}` |
|     ! 0 |  8058 | `		goto Synchronize;` |
|       - |  8059 | `	}` |
|       - |  8060 | `	/* Allocate a new class_method instance */` |
|  276247 |  8061 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  276247 |  8062 | `	if( pMeth == 0 ){` |
|     ! 0 |  8063 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8064 | `		return SXERR_ABORT;` |
|       - |  8065 | `	}` |
|       - |  8066 | `	/* Jump the left parenthesis '(' */` |
|  276247 |  8067 | `	pGen->pIn++;` |
|  276247 |  8068 | `	pEnd = 0; /* cc warning */` |
|       - |  8069 | `	/* Delimit the method signature */` |
|  276247 |  8070 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  276247 |  8071 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8072 | `		/* Syntax error */` |
|       3 |  8073 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  8074 | `		if( rc == SXERR_ABORT ){` |
|       - |  8075 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8076 | `			return SXERR_ABORT;` |
|       - |  8077 | `		}` |
|       3 |  8078 | `		goto Synchronize;` |
|       - |  8079 | `	}` |
|       - |  8080 | `	{` |
|  276245 |  8081 | `		int bIsCtor = 0;` |
|  276245 |  8082 | `		int bAbstractCtor = 0;` |
|  403274 |  8083 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  163922 |  8084 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  265159 |  8085 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   22177 |  8086 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  8087 | `				bAbstractCtor = 1;` |
|       2 |  8088 | `			}else{` |
|   22175 |  8089 | `				bIsCtor = 1;` |
|       - |  8090 | `			}` |
|   11086 |  8091 | `		}` |
|  276245 |  8092 | `		if( pGen->pIn < pEnd ){` |
|       - |  8093 | `			/* Collect method arguments */` |
|   73835 |  8094 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   73835 |  8095 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8096 | `				return SXERR_ABORT;` |
|       - |  8097 | `			}` |
|   36915 |  8098 | `		}` |
|       - |  8099 | `	}` |
|       - |  8100 | `	/* Point past ')' and parse optional return type ': type' */` |
|  276245 |  8101 | `	pGen->pIn = &pEnd[1];` |
|       - |  8102 | `	{` |
|  276245 |  8103 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  276245 |  8104 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  8105 | `			return SXERR_ABORT;` |
|  276245 |  8106 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  8107 | `			goto Synchronize;` |
|       - |  8108 | `		}` |
|       - |  8109 | `	}` |
|       - |  8110 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  8111 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  8112 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  8113 | `	{` |
|  276245 |  8114 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  8115 | `		sxu32 i;` |
|  401537 |  8116 | `		for( i = 0; i < nArg; i++ ){` |
|  125307 |  8117 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  8118 | `			ph7_class_attr *pAttr;` |
|  125307 |  8119 | `			sxi32 iAttrFlags = 0;` |
|       - |  8120 | `			int bArgTyped;` |
|  125307 |  8121 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  125243 |  8122 | `				continue;` |
|       - |  8123 | `			}` |
|       - |  8124 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  8125 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  8126 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      49 |  8127 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      70 |  8128 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      69 |  8129 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  8130 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8131 | `					"Cannot declare variadic promoted property");` |
|       3 |  8132 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8133 | `					return SXERR_ABORT;` |
|       - |  8134 | `				}` |
|       3 |  8135 | `				goto Synchronize;` |
|       - |  8136 | `			}` |
|       - |  8137 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  8138 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  8139 | `			 * appear as an alternative of a union type. */` |
|      67 |  8140 | `			if( bArgTyped ){` |
|      92 |  8141 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      58 |  8142 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      58 |  8143 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      29 |  8144 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      63 |  8145 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8146 | `					return SXERR_ABORT;` |
|      63 |  8147 | `				}else if( rc != SXRET_OK ){` |
|       6 |  8148 | `					goto Synchronize;` |
|       - |  8149 | `				}` |
|      27 |  8150 | `			}` |
|       - |  8151 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      63 |  8152 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  8153 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8154 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  8155 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8156 | `					return SXERR_ABORT;` |
|       - |  8157 | `				}` |
|       3 |  8158 | `				goto Synchronize;` |
|       - |  8159 | `			}` |
|      61 |  8160 | `			if( bArgTyped ){` |
|      57 |  8161 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      26 |  8162 | `			}` |
|      61 |  8163 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  8164 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  8165 | `			}` |
|      61 |  8166 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  8167 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  8168 | `			}` |
|      61 |  8169 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  8170 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  8171 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  8172 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  8173 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8174 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  8175 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8176 | `						return SXERR_ABORT;` |
|       - |  8177 | `					}` |
|       3 |  8178 | `					goto Synchronize;` |
|       - |  8179 | `				}` |
|      22 |  8180 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  8181 | `			}` |
|      59 |  8182 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      59 |  8183 | `			if( pAttr == 0 ){` |
|     ! 0 |  8184 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8185 | `				return SXERR_ABORT;` |
|       - |  8186 | `			}` |
|      59 |  8187 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      57 |  8188 | `				pAttr->nType = pArg->nType;` |
|      57 |  8189 | `				pAttr->sClass = pArg->sClass;` |
|      57 |  8190 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      57 |  8191 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  8192 | `					sxu32 k;` |
|      20 |  8193 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  8194 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  8195 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  8196 | `					}` |
|       3 |  8197 | `				}` |
|      26 |  8198 | `			}` |
|      59 |  8199 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      59 |  8200 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8201 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8202 | `				return SXERR_ABORT;` |
|       - |  8203 | `			}` |
|      32 |  8204 | `		}` |
|       - |  8205 | `	}` |
|  276235 |  8206 | `	if( doBody ){` |
|       - |  8207 | `		/* Compile method body */` |
|  180809 |  8208 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  180809 |  8209 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8210 | `			return SXERR_ABORT;` |
|       - |  8211 | `		}` |
|   90407 |  8212 | `	}else{` |
|       - |  8213 | `		/* Only method signature is allowed */` |
|   95431 |  8214 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  8215 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8216 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  8217 | `				if( rc == SXERR_ABORT ){` |
|       - |  8218 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8219 | `					return SXERR_ABORT;` |
|       - |  8220 | `				}` |
|     ! 0 |  8221 | `				return SXERR_CORRUPT;` |
|       - |  8222 | `			}` |
|       - |  8223 | `	}` |
|       - |  8224 | `	/* All done,install the method */` |
|  276235 |  8225 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  276235 |  8226 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8227 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8228 | `		return SXERR_ABORT;` |
|       - |  8229 | `	}` |
|  276235 |  8230 | `	return SXRET_OK;` |
|       6 |  8231 | `Synchronize:` |
|       - |  8232 | `	/* Synchronize with the first semi-colon */` |
|      40 |  8233 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  8234 | `		pGen->pIn++;` |
|       4 |  8235 | `	}` |
|      16 |  8236 | `	return SXERR_CORRUPT;` |
|  138126 |  8237 | `}` |
|       - |  8238 | `/*` |
|       - |  8239 | ` * Compile an object interface.` |
|       - |  8240 | ` *  According to the PHP language reference manual` |
|       - |  8241 | ` *   Object Interfaces:` |
|       - |  8242 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  8243 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  8244 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  8245 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  8246 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  8247 | ` */` |
|   40432 |  8248 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  8249 | `{` |
|   40437 |  8250 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8251 | `	ph7_class *pClass,*pBase;` |
|       - |  8252 | `	SyToken *pEnd,*pTmp;` |
|       - |  8253 | `	SyString *pName;` |
|       - |  8254 | `	sxi32 nKwrd;` |
|       - |  8255 | `	sxi32 rc;` |
|       - |  8256 | `	/* Jump the 'interface' keyword */` |
|   40437 |  8257 | `	pGen->pIn++;` |
|       - |  8258 | `	/* Extract interface name */` |
|   40437 |  8259 | `	pName = &pGen->pIn->sData;` |
|       - |  8260 | `	/* Advance the stream cursor */` |
|   40437 |  8261 | `	pGen->pIn++;` |
|       - |  8262 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8263 | `		SyBlob sFQN;` |
|       - |  8264 | `		SyString sFQNStr;` |
|   40437 |  8265 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   40437 |  8266 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   40437 |  8267 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   40437 |  8268 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   40437 |  8269 | `		SyBlobRelease(&sFQN);` |
|       - |  8270 | `	}` |
|   40437 |  8271 | `	if( pClass == 0 ){` |
|     ! 0 |  8272 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8273 | `		return SXERR_ABORT;` |
|       - |  8274 | `	}` |
|       - |  8275 | `	/* Mark as an interface */` |
|   40437 |  8276 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  8277 | `	/* Assume no base class is given */` |
|   40437 |  8278 | `	pBase = 0;` |
|   40437 |  8279 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   11017 |  8280 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   11017 |  8281 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  8282 | `			SyBlob sResolved;` |
|       - |  8283 | `			SyString sBaseName;` |
|       - |  8284 | `			sxu32 nRefLine;` |
|       - |  8285 | `			/* Extract base interface */` |
|   11017 |  8286 | `			pGen->pIn++;` |
|   11017 |  8287 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11017 |  8288 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11017 |  8289 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8290 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  8291 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8292 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  8293 | `					pName);` |
|     ! 0 |  8294 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8295 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8296 | `					return SXERR_ABORT;` |
|       - |  8297 | `				}` |
|     ! 0 |  8298 | `				return SXRET_OK;` |
|       - |  8299 | `			}` |
|   16523 |  8300 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   11012 |  8301 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11017 |  8302 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8303 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8304 | `			/* Only interfaces is allowed */` |
|   11017 |  8305 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8306 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8307 | `			}` |
|   11017 |  8308 | `			if( pBase == 0 ){` |
|     ! 0 |  8309 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8310 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8311 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8312 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8313 | `					return SXERR_ABORT;` |
|       - |  8314 | `				}` |
|     ! 0 |  8315 | `			}` |
|   11017 |  8316 | `			SyBlobRelease(&sResolved);` |
|    5506 |  8317 | `		}` |
|    5506 |  8318 | `	}` |
|   40437 |  8319 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8320 | `		/* Syntax error */` |
|     ! 0 |  8321 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8322 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8323 | `		if( rc == SXERR_ABORT ){` |
|       - |  8324 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8325 | `			return SXERR_ABORT;` |
|       - |  8326 | `		}` |
|     ! 0 |  8327 | `		return SXRET_OK;` |
|       - |  8328 | `	}` |
|   40437 |  8329 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   40437 |  8330 | `	pEnd = 0; /* cc warning */` |
|       - |  8331 | `	/* Delimit the interface body */` |
|   40437 |  8332 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   40437 |  8333 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8334 | `		/* Syntax error */` |
|     ! 0 |  8335 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8336 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8337 | `		if( rc == SXERR_ABORT ){` |
|       - |  8338 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8339 | `			return SXERR_ABORT;` |
|       - |  8340 | `		}` |
|     ! 0 |  8341 | `		return SXRET_OK;` |
|       - |  8342 | `	}` |
|       - |  8343 | `	/* Swap token stream */` |
|   40437 |  8344 | `	pTmp = pGen->pEnd;` |
|   40437 |  8345 | `	pGen->pEnd = pEnd;` |
|       - |  8346 | `	/* Start the parse process` |
|       - |  8347 | `	 * Note (According to the PHP reference manual):` |
|       - |  8348 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8349 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8350 | `	 */` |
|   67924 |  8351 | `	for(;;){` |
|       - |  8352 | `		/* Jump leading/trailing semi-colons */` |
|  231269 |  8353 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   95421 |  8354 | `			pGen->pIn++;` |
|       5 |  8355 | `		}` |
|  135853 |  8356 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8357 | `			/* End of interface body */` |
|   40433 |  8358 | `			break;` |
|       - |  8359 | `		}` |
|   95425 |  8360 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8361 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8362 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8363 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8364 | `			if( rc == SXERR_ABORT ){` |
|       - |  8365 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8366 | `				return SXERR_ABORT;` |
|       - |  8367 | `			}` |
|     ! 0 |  8368 | `			goto done;` |
|       - |  8369 | `		}` |
|       - |  8370 | `		/* Extract the current keyword */` |
|   95425 |  8371 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   95425 |  8372 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8373 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8374 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8375 | `			const char *zKind = "member";` |
|       3 |  8376 | `			SyString *pMemberName = 0;` |
|       3 |  8377 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8378 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8379 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8380 | `					zKind = "constant";` |
|       3 |  8381 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8382 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8383 | `					}` |
|       1 |  8384 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8385 | `					zKind = "method";` |
|     ! 0 |  8386 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8387 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8388 | `					}` |
|     ! 0 |  8389 | `				}` |
|       1 |  8390 | `			}` |
|       3 |  8391 | `			if( pMemberName ){` |
|       4 |  8392 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8393 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8394 | `			}else{` |
|     ! 0 |  8395 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8396 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8397 | `			}` |
|       3 |  8398 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8399 | `				return SXERR_ABORT;` |
|       - |  8400 | `			}` |
|       3 |  8401 | `			goto done;` |
|       - |  8402 | `		}` |
|   95423 |  8403 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8404 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8405 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8406 | `			if( rc == SXERR_ABORT ){` |
|       - |  8407 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8408 | `				return SXERR_ABORT;` |
|       - |  8409 | `			}` |
|     ! 0 |  8410 | `			goto done;` |
|       - |  8411 | `		}` |
|   95423 |  8412 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8413 | `			/* Advance the stream cursor */` |
|   95411 |  8414 | `			pGen->pIn++;` |
|   95411 |  8415 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8416 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8417 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8418 | `				if( rc == SXERR_ABORT ){` |
|       - |  8419 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8420 | `					return SXERR_ABORT;` |
|       - |  8421 | `				}` |
|     ! 0 |  8422 | `				goto done;` |
|       - |  8423 | `			}` |
|   95411 |  8424 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   95411 |  8425 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8426 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8427 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8428 | `				if( rc == SXERR_ABORT ){` |
|       - |  8429 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8430 | `					return SXERR_ABORT;` |
|       - |  8431 | `				}` |
|     ! 0 |  8432 | `				goto done;` |
|       - |  8433 | `			}` |
|   47703 |  8434 | `		}` |
|   95423 |  8435 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8436 | `			/* Parse constant */` |
|      10 |  8437 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|      10 |  8438 | `			if( rc != SXRET_OK ){` |
|       3 |  8439 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8440 | `					return SXERR_ABORT;` |
|       - |  8441 | `				}` |
|       3 |  8442 | `				goto done;` |
|       - |  8443 | `			}` |
|       4 |  8444 | `		}else{` |
|   95415 |  8445 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   95415 |  8446 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8447 | `				/* Static method,record that */` |
|   11009 |  8448 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8449 | `				/* Advance the stream cursor */` |
|   11009 |  8450 | `				pGen->pIn++;` |
|   11004 |  8451 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   11009 |  8452 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8453 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8454 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8455 | `						if( rc == SXERR_ABORT ){` |
|       - |  8456 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8457 | `							return SXERR_ABORT;` |
|       - |  8458 | `						}` |
|     ! 0 |  8459 | `						goto done;` |
|       - |  8460 | `				}` |
|    5502 |  8461 | `			}` |
|       - |  8462 | `			/* Process method signature (no body for interface methods) */` |
|   95415 |  8463 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   95415 |  8464 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8465 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8466 | `					return SXERR_ABORT;` |
|       - |  8467 | `				}` |
|     ! 0 |  8468 | `				goto done;` |
|       - |  8469 | `			}` |
|       - |  8470 | `		}` |
|       5 |  8471 | `	}` |
|       - |  8472 | `	/* Install the interface */` |
|   40433 |  8473 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   40433 |  8474 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8475 | `		/* Inherit from the base interface */` |
|   11017 |  8476 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5506 |  8477 | `	}` |
|   40433 |  8478 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8479 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8480 | `		return SXERR_ABORT;` |
|       - |  8481 | `	}` |
|   20214 |  8482 | `done:` |
|       - |  8483 | `	/* Point beyond the interface body */` |
|   40437 |  8484 | `	pGen->pIn  = &pEnd[1];` |
|   40437 |  8485 | `	pGen->pEnd = pTmp;` |
|   40437 |  8486 | `	return PH7_OK;` |
|   20221 |  8487 | `}` |
|       - |  8488 | `/*` |
|       - |  8489 | ` * Compile a user-defined class.` |
|       - |  8490 | ` * According to the PHP language reference manual` |
|       - |  8491 | ` *  class` |
|       - |  8492 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8493 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8494 | ` *  of the properties and methods belonging to the class.` |
|       - |  8495 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8496 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8497 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8498 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8499 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8500 | ` *  (called "methods").` |
|       - |  8501 | ` */` |
|       - |  8502 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8503 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8504 | `struct TraitUseEntry {` |
|       - |  8505 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8506 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8507 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8508 | `};` |
|       - |  8509 | `/*` |
|       - |  8510 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8511 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8512 | ` */` |
|  103964 |  8513 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8514 | `{` |
|       - |  8515 | `	ph7_class **apIface;` |
|       - |  8516 | `	sxu32 nIface,i;` |
|       - |  8517 | `	sxi32 rc;` |
|  103969 |  8518 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8519 | `		return SXRET_OK;` |
|       - |  8520 | `	}` |
|  103969 |  8521 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  103969 |  8522 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  199591 |  8523 | `	for(i = 0; i < nIface; i++){` |
|   95627 |  8524 | `		ph7_class *pIface = apIface[i];` |
|       - |  8525 | `		SyHashEntry *pEntry;` |
|   95627 |  8526 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  257473 |  8527 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  161851 |  8528 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8529 | `			ph7_class_method *pImplMeth;` |
|  161851 |  8530 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8531 | `			/* Find the implementing method in the class */` |
|  161851 |  8532 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  161851 |  8533 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8534 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8535 | `			}` |
|       - |  8536 | `			/* Check visibility: interface methods must be implemented as public */` |
|  161837 |  8537 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8538 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8539 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8540 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8541 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8542 | `					return SXERR_ABORT;` |
|       - |  8543 | `				}` |
|       1 |  8544 | `			}` |
|       - |  8545 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8546 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8547 | `			 */` |
|       - |  8548 | `			{` |
|  161837 |  8549 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  161837 |  8550 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  161837 |  8551 | `				int sigError = 0;` |
|  161837 |  8552 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8553 | `					sigError = 1;` |
|  161836 |  8554 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8555 | `					/* Extra parameters must all have default values */` |
|       6 |  8556 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8557 | `					sxu32 k;` |
|       8 |  8558 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8559 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8560 | `							sigError = 1;` |
|       3 |  8561 | `							break;` |
|       - |  8562 | `						}` |
|       2 |  8563 | `					}` |
|       2 |  8564 | `				}` |
|  161837 |  8565 | `				if( sigError ){` |
|       - |  8566 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8567 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8568 | `					sxu32 j;` |
|       6 |  8569 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8570 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8571 | `					/* Build implementing method signature */` |
|       6 |  8572 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8573 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8574 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8575 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8576 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8577 | `					}` |
|       - |  8578 | `					/* Build interface method signature */` |
|       6 |  8579 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8580 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8581 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8582 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8583 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8584 | `					}` |
|       8 |  8585 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8586 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8587 | `						&pClass->sName,pMName,` |
|       4 |  8588 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8589 | `						&pIface->sName,pMName,` |
|       4 |  8590 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8591 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8592 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8593 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8594 | `						return SXERR_ABORT;` |
|       - |  8595 | `					}` |
|       2 |  8596 | `				}` |
|       - |  8597 | `			}` |
|       5 |  8598 | `		}` |
|   47816 |  8599 | `	}` |
|  103969 |  8600 | `	return SXRET_OK;` |
|   51987 |  8601 | `}` |
|       - |  8602 | `/*` |
|       - |  8603 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8604 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8605 | ` */` |
|  103964 |  8606 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8607 | `{` |
|       - |  8608 | `	ph7_class_method *pMeth;` |
|       - |  8609 | `	SyHashEntry *pEntry;` |
|       - |  8610 | `	sxu32 nAbstract;` |
|       - |  8611 | `	SyBlob sMsg;` |
|       - |  8612 | `	sxi32 rc;` |
|       - |  8613 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  103969 |  8614 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      37 |  8615 | `		return SXRET_OK;` |
|       - |  8616 | `	}` |
|       - |  8617 | `	/* Count abstract methods */` |
|  103937 |  8618 | `	nAbstract = 0;` |
|  103937 |  8619 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  974985 |  8620 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  871053 |  8621 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  871053 |  8622 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8623 | `			nAbstract++;` |
|       8 |  8624 | `		}` |
|       5 |  8625 | `	}` |
|  103937 |  8626 | `	if( nAbstract == 0 ){` |
|  103923 |  8627 | `		return SXRET_OK;` |
|       - |  8628 | `	}` |
|       - |  8629 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8630 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8631 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8632 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8633 | `		&pClass->sName,nAbstract,` |
|       7 |  8634 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8635 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8636 | `	/* Second pass: list methods with origins */` |
|       - |  8637 | `	{` |
|      18 |  8638 | `		sxu32 nListed = 0;` |
|      18 |  8639 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8640 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8641 | `			ph7_class *pOrigin = 0;` |
|       - |  8642 | `			SyString *pMName;` |
|      22 |  8643 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8644 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8645 | `				continue;` |
|       - |  8646 | `			}` |
|      20 |  8647 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8648 | `			if( nListed > 0 ){` |
|       3 |  8649 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8650 | `			}` |
|       - |  8651 | `			/* Find the origin of this abstract method.` |
|       - |  8652 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8653 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8654 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8655 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8656 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8657 | `			 * class's namespace.` |
|       - |  8658 | `			 */` |
|       - |  8659 | `			{` |
|       - |  8660 | `				ph7_class **apIface;` |
|       - |  8661 | `				ph7_class **apTrait;` |
|       - |  8662 | `				ph7_class *pWalk;` |
|       - |  8663 | `				sxu32 i;` |
|       - |  8664 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8665 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8666 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8667 | `				 */` |
|      20 |  8668 | `				if( pClass->pBase ){` |
|      11 |  8669 | `					pWalk = pClass->pBase;` |
|      19 |  8670 | `					while( pWalk ){` |
|       - |  8671 | `						ph7_class_method *pParentMeth;` |
|      13 |  8672 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8673 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8674 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8675 | `							 * in this class's ancestor chain.` |
|       - |  8676 | `							 */` |
|      13 |  8677 | `							int fromIface = 0;` |
|      13 |  8678 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8679 | `							while( pAnc ){` |
|       - |  8680 | `								ph7_class **apPI;` |
|       - |  8681 | `								sxu32 j;` |
|      15 |  8682 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8683 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8684 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8685 | `										fromIface = 1;` |
|      10 |  8686 | `										break;` |
|       - |  8687 | `									}` |
|     ! 0 |  8688 | `								}` |
|      15 |  8689 | `								if( fromIface ) break;` |
|       6 |  8690 | `								pAnc = pAnc->pBase;` |
|       2 |  8691 | `							}` |
|      13 |  8692 | `							if( !fromIface ){` |
|       3 |  8693 | `								pOrigin = pWalk;` |
|       3 |  8694 | `								break;` |
|       - |  8695 | `							}` |
|       4 |  8696 | `						}` |
|      10 |  8697 | `						pWalk = pWalk->pBase;` |
|       2 |  8698 | `					}` |
|       4 |  8699 | `				}` |
|       - |  8700 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8701 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8702 | `				 */` |
|      20 |  8703 | `				if( !pOrigin ){` |
|      18 |  8704 | `					pWalk = pClass;` |
|      40 |  8705 | `					while( pWalk && !pOrigin ){` |
|      26 |  8706 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8707 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8708 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8709 | `							ph7_class *pDeepest = 0;` |
|      28 |  8710 | `							while( pIface ){` |
|      16 |  8711 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8712 | `									pDeepest = pIface;` |
|       6 |  8713 | `								}` |
|      16 |  8714 | `								pIface = pIface->pBase;` |
|       4 |  8715 | `							}` |
|      16 |  8716 | `							if( pDeepest ){` |
|      16 |  8717 | `								pOrigin = pDeepest;` |
|      16 |  8718 | `								break;` |
|       - |  8719 | `							}` |
|     ! 0 |  8720 | `						}` |
|      26 |  8721 | `						pWalk = pWalk->pBase;` |
|       4 |  8722 | `					}` |
|       7 |  8723 | `				}` |
|       - |  8724 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8725 | `				if( !pOrigin ){` |
|       3 |  8726 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8727 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8728 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8729 | `							pOrigin = pClass;` |
|       3 |  8730 | `							break;` |
|       - |  8731 | `						}` |
|     ! 0 |  8732 | `					}` |
|       1 |  8733 | `				}` |
|       - |  8734 | `			}` |
|      20 |  8735 | `			if( pOrigin ){` |
|      20 |  8736 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8737 | `			}else{` |
|       - |  8738 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8739 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8740 | `			}` |
|      20 |  8741 | `			nListed++;` |
|       4 |  8742 | `		}` |
|       - |  8743 | `	}` |
|      18 |  8744 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8745 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8746 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8747 | `	SyBlobRelease(&sMsg);` |
|      18 |  8748 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8749 | `		return SXERR_ABORT;` |
|       - |  8750 | `	}` |
|      18 |  8751 | `	return SXRET_OK;` |
|   51987 |  8752 | `}` |
|       - |  8753 | `/*` |
|       - |  8754 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8755 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8756 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8757 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8758 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8759 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8760 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8761 | ` */` |
|  100080 |  8762 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8763 | `{` |
|  100085 |  8764 | `	int isAbsolute = 0;` |
|  100085 |  8765 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8766 | `	SyBlob sName;` |
|  100085 |  8767 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     107 |  8768 | `		isAbsolute = 1;` |
|     107 |  8769 | `		pGen->pIn++;` |
|      51 |  8770 | `	}` |
|  100085 |  8771 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       9 |  8772 | `		pGen->pIn = pStart;` |
|       9 |  8773 | `		return SXERR_INVALID;` |
|       - |  8774 | `	}` |
|  100079 |  8775 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|  100079 |  8776 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|  100079 |  8777 | `	pGen->pIn++;` |
|  150129 |  8778 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   50060 |  8779 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8780 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8781 | `		pGen->pIn++;` |
|      13 |  8782 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8783 | `		pGen->pIn++;` |
|       1 |  8784 | `	}` |
|  100079 |  8785 | `	if( isAbsolute ){` |
|     105 |  8786 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      55 |  8787 | `	}else{` |
|       - |  8788 | `		SyString sRaw;` |
|   99979 |  8789 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   99979 |  8790 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8791 | `	}` |
|  100079 |  8792 | `	SyBlobRelease(&sName);` |
|  100079 |  8793 | `	return SXRET_OK;` |
|   50045 |  8794 | `}` |
|       - |  8795 | `/*` |
|       - |  8796 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8797 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8798 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8799 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8800 | ` * either direction cannot run unbounded.` |
|       - |  8801 | ` */` |
|       - |  8802 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   11176 |  8803 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8804 | `{` |
|       - |  8805 | `	ph7_class **apParent;` |
|       - |  8806 | `	sxu32 n;` |
|   18721 |  8807 | `	while( pInterface ){` |
|   14891 |  8808 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8809 | `			return FALSE;` |
|       - |  8810 | `		}` |
|   18573 |  8811 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7364 |  8812 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7351 |  8813 | `			return TRUE;` |
|       - |  8814 | `		}` |
|    7545 |  8815 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7545 |  8816 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8817 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8818 | `				return TRUE;` |
|       - |  8819 | `			}` |
|     ! 0 |  8820 | `		}` |
|    7545 |  8821 | `		pInterface = pInterface->pBase;` |
|    7545 |  8822 | `		iDepth++;` |
|       5 |  8823 | `	}` |
|    3835 |  8824 | `	return FALSE;` |
|    5593 |  8825 | `}` |
|   11176 |  8826 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8827 | `{` |
|   11181 |  8828 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8829 | `}` |
|       - |  8830 | `/*` |
|       - |  8831 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8832 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8833 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8834 | ` */` |
|    7346 |  8835 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8836 | `{` |
|    7355 |  8837 | `	while( pBase ){` |
|      10 |  8838 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8839 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8840 | `			return TRUE;` |
|       - |  8841 | `		}` |
|      10 |  8842 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8843 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8844 | `			return TRUE;` |
|       - |  8845 | `		}` |
|       5 |  8846 | `		pBase = pBase->pBase;` |
|       1 |  8847 | `	}` |
|    7347 |  8848 | `	return FALSE;` |
|    3678 |  8849 | `}` |
|       - |  8850 | `/*` |
|       - |  8851 | ` * Compile a class declaration, named or anonymous.` |
|       - |  8852 | ` *` |
|       - |  8853 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  8854 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  8855 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  8856 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  8857 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  8858 | ` * implements, body, install) is shared by both paths.` |
|       - |  8859 | ` */` |
|  104004 |  8860 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  8861 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  8862 | `{` |
|  104009 |  8863 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8864 | `	ph7_class *pClass,*pBase;` |
|       - |  8865 | `	SyToken *pEnd,*pTmp;` |
|       - |  8866 | `	sxi32 iProtection;` |
|       - |  8867 | `	SySet aInterfaces;` |
|       - |  8868 | `	SySet aUseEntries;` |
|       - |  8869 | `	sxi32 iAttrflags;` |
|       - |  8870 | `	SyString *pName;` |
|       - |  8871 | `	sxi32 nKwrd;` |
|       - |  8872 | `	sxi32 rc;` |
|       - |  8873 | `	/* Jump the 'class' keyword */` |
|  104009 |  8874 | `	pGen->pIn++;` |
|  104009 |  8875 | `	if( pAnonName ){` |
|       - |  8876 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  8877 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  8878 | `		 * then use the synthesized name. */` |
|      30 |  8879 | `		*ppArgStart = *ppArgEnd = 0;` |
|      30 |  8880 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  8881 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  8882 | `			*ppArgStart = pGen->pIn;` |
|      10 |  8883 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  8884 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  8885 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  8886 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  8887 | `		}` |
|      30 |  8888 | `		pName = pAnonName;` |
|      30 |  8889 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      17 |  8890 | `	}else{` |
|  103983 |  8891 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8892 | `			/* Syntax error */` |
|     ! 0 |  8893 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8894 | `			if( rc == SXERR_ABORT ){` |
|       - |  8895 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8896 | `				return SXERR_ABORT;` |
|       - |  8897 | `			}` |
|       - |  8898 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8899 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8900 | `				pGen->pIn++;` |
|     ! 0 |  8901 | `			}` |
|     ! 0 |  8902 | `			return SXRET_OK;` |
|       - |  8903 | `		}` |
|       - |  8904 | `		/* Extract class name */` |
|  103983 |  8905 | `		pName = &pGen->pIn->sData;` |
|       - |  8906 | `		/* Advance the stream cursor */` |
|  103983 |  8907 | `		pGen->pIn++;` |
|       - |  8908 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  8909 | `			SyBlob sFQN;` |
|       - |  8910 | `			SyString sFQNStr;` |
|  103983 |  8911 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  103983 |  8912 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  103983 |  8913 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  103983 |  8914 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  103983 |  8915 | `			SyBlobRelease(&sFQN);` |
|       - |  8916 | `		}` |
|       - |  8917 | `	}` |
|  104009 |  8918 | `	if( pClass == 0 ){` |
|     ! 0 |  8919 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8920 | `		return SXERR_ABORT;` |
|       - |  8921 | `	}` |
|       - |  8922 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  104009 |  8923 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  104009 |  8924 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8925 | `	/* Assume a standalone class */` |
|  104009 |  8926 | `	pBase = 0;` |
|  104009 |  8927 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   88381 |  8928 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   88381 |  8929 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8930 | `			SyBlob sResolved;` |
|       - |  8931 | `			SyString sBaseName;` |
|       - |  8932 | `			sxu32 nRefLine;` |
|   77223 |  8933 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   77223 |  8934 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   77223 |  8935 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   77223 |  8936 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8937 | `				SyBlobRelease(&sResolved);` |
|       4 |  8938 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8939 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8940 | `					pName);` |
|       3 |  8941 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8942 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8943 | `					return SXERR_ABORT;` |
|       - |  8944 | `				}` |
|       3 |  8945 | `				return SXRET_OK;` |
|       - |  8946 | `			}` |
|  115829 |  8947 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   77216 |  8948 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   77221 |  8949 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8950 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8951 | `			/* Interfaces are not allowed */` |
|   77221 |  8952 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8953 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8954 | `			}` |
|   77221 |  8955 | `			if( pBase == 0 ){` |
|     ! 0 |  8956 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8957 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8958 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8959 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8960 | `					return SXERR_ABORT;` |
|       - |  8961 | `				}` |
|     ! 0 |  8962 | `			}else{` |
|   77221 |  8963 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8964 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8965 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8966 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8967 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8968 | `						return SXERR_ABORT;` |
|       - |  8969 | `					}` |
|     ! 0 |  8970 | `				}` |
|       - |  8971 | `			}` |
|   77221 |  8972 | `			SyBlobRelease(&sResolved);` |
|   38608 |  8973 | `		}` |
|   88379 |  8974 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8975 | `			ph7_class *pInterface;` |
|       - |  8976 | `			/* Interface implementation */` |
|   11171 |  8977 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5593 |  8978 | `			for(;;){` |
|       - |  8979 | `				SyBlob sResolved;` |
|       - |  8980 | `				SyString sIntName;` |
|       - |  8981 | `				sxu32 nRefLine;` |
|   11181 |  8982 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11181 |  8983 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11181 |  8984 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8985 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8986 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8987 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8988 | `						pName);` |
|     ! 0 |  8989 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8990 | `						return SXERR_ABORT;` |
|       - |  8991 | `					}` |
|     ! 0 |  8992 | `					break;` |
|       - |  8993 | `				}` |
|   22357 |  8994 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   11176 |  8995 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11181 |  8996 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8997 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8998 | `				/* Only interfaces are allowed */` |
|   11181 |  8999 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  9000 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  9001 | `				}` |
|   11181 |  9002 | `				if( pInterface == 0 ){` |
|     ! 0 |  9003 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  9004 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  9005 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9006 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  9007 | `						return SXERR_ABORT;` |
|       - |  9008 | `					}` |
|     ! 0 |  9009 | `				}else{` |
|       - |  9010 | `					/* Reject user classes that try to implement Throwable` |
|       - |  9011 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  9012 | `					 * unless they already extend Exception or Error.` |
|       - |  9013 | `					 * Exception and Error themselves are compiled from the` |
|       - |  9014 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  9015 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   11181 |  9016 | `					SyString *pFqn = &pClass->sName;` |
|   11181 |  9017 | `					int bIsExceptionOrError =` |
|    9260 |  9018 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   18602 |  9019 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9349 |  9020 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3682 |  9021 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   18520 |  9022 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   11022 |  9023 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3671 |  9024 | `						!bIsExceptionOrError ){` |
|      12 |  9025 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9026 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  9027 | `							&pClass->sName);` |
|       9 |  9028 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9029 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  9030 | `							return SXERR_ABORT;` |
|       - |  9031 | `						}` |
|       - |  9032 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  9033 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  9034 | `					}else{` |
|   11175 |  9035 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  9036 | `					}` |
|       - |  9037 | `				}` |
|   11181 |  9038 | `				SyBlobRelease(&sResolved);` |
|   11181 |  9039 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5588 |  9040 | `					break;` |
|       - |  9041 | `				}` |
|      14 |  9042 | `				pGen->pIn++;/* Jump the comma */` |
|       4 |  9043 | `			}` |
|    5583 |  9044 | `		}` |
|   44187 |  9045 | `	}` |
|  104007 |  9046 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  9047 | `		/* Syntax error */` |
|     ! 0 |  9048 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  9049 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9050 | `		if( rc == SXERR_ABORT ){` |
|       - |  9051 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9052 | `			return SXERR_ABORT;` |
|       - |  9053 | `		}` |
|     ! 0 |  9054 | `		return SXRET_OK;` |
|       - |  9055 | `	}` |
|  104007 |  9056 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  104007 |  9057 | `	pEnd = 0; /* cc warning */` |
|       - |  9058 | `	/* Delimit the class body */` |
|  104007 |  9059 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  104007 |  9060 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  9061 | `		/* Syntax error */` |
|     ! 0 |  9062 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  9063 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9064 | `		if( rc == SXERR_ABORT ){` |
|       - |  9065 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9066 | `			return SXERR_ABORT;` |
|       - |  9067 | `		}` |
|     ! 0 |  9068 | `		return SXRET_OK;` |
|       - |  9069 | `	}` |
|       - |  9070 | `	/* Swap token stream */` |
|  104007 |  9071 | `	pTmp = pGen->pEnd;` |
|  104007 |  9072 | `	pGen->pEnd = pEnd;` |
|       - |  9073 | `	/* Set the inherited flags */` |
|  104007 |  9074 | `	pClass->iFlags = iFlags;` |
|       - |  9075 | `	/* Start the parse process */` |
|  142415 |  9076 | `	for(;;){` |
|       - |  9077 | `		/* Jump leading/trailing semi-colons */` |
|  440405 |  9078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   77827 |  9079 | `			pGen->pIn++;` |
|       5 |  9080 | `		}` |
|  362583 |  9081 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9082 | `			/* End of class body */` |
|  103969 |  9083 | `			break;` |
|       - |  9084 | `		}` |
|  258614 |  9085 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  129312 |  9086 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  9087 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9088 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9089 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9090 | `			if( rc == SXERR_ABORT ){` |
|       - |  9091 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  9092 | `				return SXERR_ABORT;` |
|       - |  9093 | `			}` |
|     ! 0 |  9094 | `			goto done;` |
|       - |  9095 | `		}` |
|       - |  9096 | `		/* Assume public visibility */` |
|  258619 |  9097 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  258619 |  9098 | `		iAttrflags = 0;` |
|       - |  9099 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  9100 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  9101 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  9102 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  258619 |  9103 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9104 | `			int bMod = 0;` |
|     ! 0 |  9105 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9106 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  9107 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  9108 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  9109 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  9110 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  9111 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  9112 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  9113 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  9114 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  9115 | `			}` |
|     ! 0 |  9116 | `			if( !bMod ){` |
|     ! 0 |  9117 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9118 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9119 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9120 | `						return SXERR_ABORT;` |
|       - |  9121 | `					}` |
|     ! 0 |  9122 | `					goto done;` |
|       - |  9123 | `				}` |
|     ! 0 |  9124 | `				continue;` |
|       - |  9125 | `			}` |
|     ! 0 |  9126 | `		}` |
|  258619 |  9127 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9128 | `			/* Extract the current keyword */` |
|  258619 |  9129 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  258619 |  9130 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9131 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  9132 | `				TraitUseEntry sUse;` |
|      57 |  9133 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      57 |  9134 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      57 |  9135 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      34 |  9136 | `				for(;;){` |
|       - |  9137 | `					ph7_class *pTrait;` |
|       - |  9138 | `					SyString *pTraitName;` |
|      65 |  9139 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9140 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9141 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  9142 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9143 | `							return SXERR_ABORT;` |
|       - |  9144 | `						}` |
|     ! 0 |  9145 | `						break;` |
|       - |  9146 | `					}` |
|      65 |  9147 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  9148 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  9149 | `						SyBlob sResolved;` |
|      65 |  9150 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      65 |  9151 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     125 |  9152 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      60 |  9153 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      65 |  9154 | `						SyBlobRelease(&sResolved);` |
|       - |  9155 | `					}` |
|       - |  9156 | `					/* Only traits are allowed */` |
|      65 |  9157 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9158 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  9159 | `					}` |
|      65 |  9160 | `					if( pTrait == 0 ){` |
|     ! 0 |  9161 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9162 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  9163 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9164 | `							return SXERR_ABORT;` |
|       - |  9165 | `						}` |
|     ! 0 |  9166 | `					}else{` |
|      65 |  9167 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  9168 | `					}` |
|      65 |  9169 | `					pGen->pIn++; /* Advance past trait name */` |
|      65 |  9170 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      31 |  9171 | `						break;` |
|       - |  9172 | `					}` |
|      10 |  9173 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  9174 | `				}` |
|       - |  9175 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      57 |  9176 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  9177 | `					SyToken *pBlock;` |
|      13 |  9178 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  9179 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  9180 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  9181 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  9182 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  9183 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  9184 | `					}else{` |
|     ! 0 |  9185 | `						pGen->pIn = pGen->pEnd;` |
|       - |  9186 | `					}` |
|       5 |  9187 | `				}` |
|      57 |  9188 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  9189 | `				/* The semicolon will be consumed by the outer loop */` |
|      57 |  9190 | `				continue;` |
|       - |  9191 | `			}` |
|  258567 |  9192 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  258261 |  9193 | `				iProtection = nKwrd;` |
|  258261 |  9194 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  9195 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  258261 |  9196 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  9197 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  9198 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  9199 | `				}` |
|  258256 |  9200 | `				if( pGen->pIn >= pGen->pEnd` |
|  258261 |  9201 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9202 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9203 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9204 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9205 | `					if( rc == SXERR_ABORT ){` |
|       - |  9206 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  9207 | `						return SXERR_ABORT;` |
|       - |  9208 | `					}` |
|     ! 0 |  9209 | `					goto done;` |
|       - |  9210 | `				}` |
|  258261 |  9211 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9212 | `					/* Attribute declaration (untyped) */` |
|   77521 |  9213 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   77521 |  9214 | `					if( rc != SXRET_OK ){` |
|      11 |  9215 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9216 | `							return SXERR_ABORT;` |
|       - |  9217 | `						}` |
|      11 |  9218 | `						goto done;` |
|       - |  9219 | `					}` |
|   77513 |  9220 | `					continue;` |
|       - |  9221 | `				}` |
|  180745 |  9222 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9223 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     173 |  9224 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     173 |  9225 | `					if( rc != SXRET_OK ){` |
|       8 |  9226 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9227 | `							return SXERR_ABORT;` |
|       - |  9228 | `						}` |
|       8 |  9229 | `						goto done;` |
|       - |  9230 | `					}` |
|     167 |  9231 | `					continue;` |
|       - |  9232 | `				}` |
|       - |  9233 | `				/* Extract the keyword */` |
|  180577 |  9234 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   90286 |  9235 | `			}` |
|  180883 |  9236 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  9237 | `				/* Process constant declaration */` |
|      79 |  9238 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      79 |  9239 | `				if( rc != SXRET_OK ){` |
|      11 |  9240 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9241 | `						return SXERR_ABORT;` |
|       - |  9242 | `					}` |
|      11 |  9243 | `					goto done;` |
|       - |  9244 | `				}` |
|      38 |  9245 | `			}else{` |
|  180809 |  9246 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  9247 | `					/* Static method or attribute,record that */` |
|   11071 |  9248 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   11071 |  9249 | `					pGen->pIn++; /* Jump the static keyword */` |
|   11071 |  9250 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9251 | `						/* Extract the keyword */` |
|   11061 |  9252 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   11061 |  9253 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9254 | `							iProtection = nKwrd;` |
|     ! 0 |  9255 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  9256 | `						}` |
|    5528 |  9257 | `					}` |
|       - |  9258 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  9259 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  9260 | `					 * than a generic "expecting method" parse error. */` |
|   11071 |  9261 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9262 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9263 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  9264 | `					}` |
|   11066 |  9265 | `					if( pGen->pIn >= pGen->pEnd` |
|   11071 |  9266 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9267 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9268 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  9269 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9270 | `						if( rc == SXERR_ABORT ){` |
|       - |  9271 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9272 | `							return SXERR_ABORT;` |
|       - |  9273 | `						}` |
|     ! 0 |  9274 | `						goto done;` |
|       - |  9275 | `					}` |
|   11071 |  9276 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9277 | `						/* Attribute declaration */` |
|      11 |  9278 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  9279 | `						if( rc != SXRET_OK ){` |
|       3 |  9280 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9281 | `								return SXERR_ABORT;` |
|       - |  9282 | `							}` |
|       3 |  9283 | `							goto done;` |
|       - |  9284 | `						}` |
|       8 |  9285 | `						continue;` |
|       - |  9286 | `					}` |
|   11063 |  9287 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9288 | `						/* Typed static attribute declaration */` |
|      15 |  9289 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  9290 | `						if( rc != SXRET_OK ){` |
|       3 |  9291 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9292 | `								return SXERR_ABORT;` |
|       - |  9293 | `							}` |
|       3 |  9294 | `							goto done;` |
|       - |  9295 | `						}` |
|      13 |  9296 | `						continue;` |
|       - |  9297 | `					}` |
|       - |  9298 | `					/* Extract the keyword */` |
|   11051 |  9299 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  175266 |  9300 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  9301 | `					/* Abstract method,record that */` |
|      15 |  9302 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9303 | `					/* Mark the whole class as abstract */` |
|      15 |  9304 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9305 | `					/* Advance the stream cursor */` |
|      15 |  9306 | `					pGen->pIn++;` |
|      15 |  9307 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      15 |  9308 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      15 |  9309 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      13 |  9310 | `							iProtection = nKwrd;` |
|      13 |  9311 | `							pGen->pIn++; /* Jump the visibility token */` |
|       5 |  9312 | `						}` |
|       6 |  9313 | `					}` |
|      15 |  9314 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      12 |  9315 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9316 | `							/* Static method */` |
|     ! 0 |  9317 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9318 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9319 | `					}` |
|      15 |  9320 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      12 |  9321 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9322 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9323 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9324 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9325 | `							if( rc == SXERR_ABORT ){` |
|       - |  9326 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9327 | `								return SXERR_ABORT;` |
|       - |  9328 | `							}` |
|     ! 0 |  9329 | `							goto done;` |
|       - |  9330 | `					}` |
|      15 |  9331 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  169737 |  9332 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9333 | `					/* final method ,record that */` |
|      16 |  9334 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      16 |  9335 | `					pGen->pIn++; /* Jump the final keyword */` |
|      16 |  9336 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9337 | `						/* Extract the keyword */` |
|      16 |  9338 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      16 |  9339 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 |  9340 | `							iProtection = nKwrd;` |
|       8 |  9341 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9342 | `						}` |
|       7 |  9343 | `					}` |
|      16 |  9344 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9345 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9346 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9347 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9348 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9349 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9350 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9351 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9352 | `									return SXERR_ABORT;` |
|       - |  9353 | `								}` |
|     ! 0 |  9354 | `								goto done;` |
|       - |  9355 | `							}` |
|      12 |  9356 | `							continue;` |
|       - |  9357 | `					}` |
|       5 |  9358 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9359 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9360 | `							/* Static method */` |
|     ! 0 |  9361 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9362 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9363 | `					}` |
|       5 |  9364 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9365 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9366 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9367 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9368 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9369 | `							if( rc == SXERR_ABORT ){` |
|       - |  9370 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9371 | `								return SXERR_ABORT;` |
|       - |  9372 | `							}` |
|     ! 0 |  9373 | `							goto done;` |
|       - |  9374 | `					}` |
|       5 |  9375 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9376 | `				}` |
|  180779 |  9377 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9378 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9379 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9380 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9381 | `						if( rc == SXERR_ABORT ){` |
|       - |  9382 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9383 | `							return SXERR_ABORT;` |
|       - |  9384 | `						}` |
|     ! 0 |  9385 | `						goto done;` |
|       - |  9386 | `				}` |
|  180779 |  9387 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9388 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9389 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9390 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9391 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9392 | `						if( rc == SXERR_ABORT ){` |
|       - |  9393 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9394 | `							return SXERR_ABORT;` |
|       - |  9395 | `						}` |
|     ! 0 |  9396 | `						goto done;` |
|       - |  9397 | `					}` |
|       - |  9398 | `					/* Attribute declaration */` |
|       7 |  9399 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9400 | `				}else{` |
|       - |  9401 | `					/* Process method declaration */` |
|  180773 |  9402 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9403 | `				}` |
|  180779 |  9404 | `				if( rc != SXRET_OK ){` |
|      16 |  9405 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9406 | `						return SXERR_ABORT;` |
|       - |  9407 | `					}` |
|      16 |  9408 | `					goto done;` |
|       - |  9409 | `				}` |
|       - |  9410 | `			}` |
|   90419 |  9411 | `		}else{` |
|       - |  9412 | `			/* Attribute declaration */` |
|     ! 0 |  9413 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9414 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9415 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9416 | `					return SXERR_ABORT;` |
|       - |  9417 | `				}` |
|     ! 0 |  9418 | `				goto done;` |
|       - |  9419 | `			}` |
|       - |  9420 | `		}` |
|       5 |  9421 | `	}` |
|       - |  9422 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9423 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9424 | `	 */` |
|       - |  9425 | `	{` |
|       - |  9426 | `		TraitUseEntry *apUse;` |
|       - |  9427 | `		sxu32 nU;` |
|  103969 |  9428 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  104021 |  9429 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      57 |  9430 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      57 |  9431 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      57 |  9432 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      57 |  9433 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9434 | `			sxu32 nT;` |
|      57 |  9435 | `			if( !hasResolution ){` |
|       - |  9436 | `				/* No conflict resolution block: use standard trait application */` |
|      95 |  9437 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      53 |  9438 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      53 |  9439 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9440 | `						break;` |
|       - |  9441 | `					}` |
|      29 |  9442 | `				}` |
|      26 |  9443 | `			}else{` |
|       - |  9444 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9445 | `				 * then use the block to resolve method conflicts.` |
|       - |  9446 | `				 */` |
|       - |  9447 | `				SyToken *pR;` |
|      25 |  9448 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9449 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9450 | `					ph7_class_attr *pAR;` |
|       - |  9451 | `					SyHashEntry *pER;` |
|       - |  9452 | `					SyString *pNR;` |
|      15 |  9453 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9454 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9455 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9456 | `						pNR = &pAR->sName;` |
|     ! 0 |  9457 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9458 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9459 | `						}` |
|     ! 0 |  9460 | `					}` |
|      15 |  9461 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9462 | `				}` |
|       - |  9463 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9464 | `				pR = pUse->pResolvStart;` |
|      27 |  9465 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9466 | `					SyString sTrait,sMethod;` |
|       - |  9467 | `					ph7_class *pSrcTrait;` |
|       - |  9468 | `					ph7_class_method *pMeth;` |
|       - |  9469 | `					sxi32 nRKwrd;` |
|      41 |  9470 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9471 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9472 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9473 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9474 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9475 | `					sMethod = pR->sData;` |
|      17 |  9476 | `					pR++;` |
|      17 |  9477 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9478 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9479 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9480 | `							sTrait = sMethod;` |
|       7 |  9481 | `							pR++;` |
|       7 |  9482 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9483 | `							sMethod = pR->sData;` |
|       7 |  9484 | `							pR++;` |
|       3 |  9485 | `						}` |
|       3 |  9486 | `					}` |
|      17 |  9487 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9488 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9489 | `						continue;` |
|       - |  9490 | `					}` |
|      17 |  9491 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9492 | `					pR++;` |
|      17 |  9493 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9494 | `						pSrcTrait = 0;` |
|       7 |  9495 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9496 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9497 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9498 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9499 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9500 | `								break;` |
|       - |  9501 | `							}` |
|       2 |  9502 | `						}` |
|       5 |  9503 | `						if( pSrcTrait ){` |
|       5 |  9504 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9505 | `							if( pMeth ){` |
|       5 |  9506 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9507 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9508 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9509 | `								}` |
|       2 |  9510 | `							}` |
|       2 |  9511 | `						}` |
|       2 |  9512 | `					}` |
|      35 |  9513 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9514 | `				}` |
|       - |  9515 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9516 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9517 | `					ph7_class_method *pMR;` |
|       - |  9518 | `					SyHashEntry *pER;` |
|       - |  9519 | `					SyString *pNR;` |
|      15 |  9520 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9521 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9522 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9523 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9524 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9525 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9526 | `						}` |
|       3 |  9527 | `					}` |
|       9 |  9528 | `				}` |
|       - |  9529 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9530 | `				pR = pUse->pResolvStart;` |
|      27 |  9531 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9532 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9533 | `					ph7_class *pSrcTrait;` |
|       - |  9534 | `					ph7_class_method *pMeth;` |
|      27 |  9535 | `					int hasQual = 0;` |
|       - |  9536 | `					sxi32 nRKwrd;` |
|      41 |  9537 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9538 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9539 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9540 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9541 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9542 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9543 | `					sMethod = pR->sData;` |
|      17 |  9544 | `					pR++;` |
|      17 |  9545 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9546 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9547 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9548 | `							sTrait = sMethod;` |
|       7 |  9549 | `							hasQual = 1;` |
|       7 |  9550 | `							pR++;` |
|       7 |  9551 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9552 | `							sMethod = pR->sData;` |
|       7 |  9553 | `							pR++;` |
|       3 |  9554 | `						}` |
|       3 |  9555 | `					}` |
|      17 |  9556 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9557 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9558 | `						continue;` |
|       - |  9559 | `					}` |
|      17 |  9560 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9561 | `					pR++;` |
|      17 |  9562 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9563 | `						sxi32 iNewVis = -1;` |
|      13 |  9564 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9565 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9566 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9567 | `								iNewVis = nAK;` |
|       7 |  9568 | `								pR++;` |
|       3 |  9569 | `							}` |
|       3 |  9570 | `						}` |
|      13 |  9571 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9572 | `							sAlias = pR->sData;` |
|      11 |  9573 | `							pR++;` |
|       4 |  9574 | `						}` |
|      13 |  9575 | `						pMeth = 0;` |
|      13 |  9576 | `						if( hasQual ){` |
|       3 |  9577 | `							pSrcTrait = 0;` |
|       5 |  9578 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9579 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9580 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9581 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9582 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9583 | `									break;` |
|       - |  9584 | `								}` |
|       2 |  9585 | `							}` |
|       3 |  9586 | `							if( pSrcTrait ){` |
|       3 |  9587 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9588 | `							}` |
|       2 |  9589 | `						}else{` |
|      10 |  9590 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9591 | `						}` |
|      13 |  9592 | `						if( pMeth ){` |
|      13 |  9593 | `							if( sAlias.nByte > 0 ){` |
|       - |  9594 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9595 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9596 | `								 */` |
|       - |  9597 | `								ph7_class_method *pAlias;` |
|       - |  9598 | `								char *zAliasDup;` |
|      11 |  9599 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9600 | `								if( pAlias ){` |
|      11 |  9601 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9602 | `									if( iNewVis >= 0 ){` |
|       5 |  9603 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9604 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9605 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9606 | `									}` |
|      11 |  9607 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9608 | `									if( zAliasDup ){` |
|      11 |  9609 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9610 | `									}` |
|       7 |  9611 | `								}` |
|       7 |  9612 | `							}else if( iNewVis >= 0 ){` |
|       - |  9613 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9614 | `								ph7_class_method *pCopy;` |
|       3 |  9615 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9616 | `								if( pCopy ){` |
|       3 |  9617 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9618 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9619 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9620 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9621 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9622 | `									/* Replace the method in the class hash */` |
|       3 |  9623 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9624 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9625 | `								}` |
|       1 |  9626 | `							}` |
|       5 |  9627 | `						}` |
|       5 |  9628 | `						SXUNUSED(hasQual);` |
|       5 |  9629 | `					}` |
|      21 |  9630 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9631 | `				}` |
|       - |  9632 | `			}` |
|      57 |  9633 | `			SySetRelease(&pUse->aTraits);` |
|      31 |  9634 | `		}` |
|       - |  9635 | `	}` |
|       - |  9636 | `	/* Install the class */` |
|  103969 |  9637 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  103969 |  9638 | `	if( rc == SXRET_OK ){` |
|       - |  9639 | `		ph7_class **apInterface;` |
|       - |  9640 | `		sxu32 n;` |
|  103969 |  9641 | `		if( pBase ){` |
|       - |  9642 | `			/* Inherit from base class and mark as a subclass */` |
|   77221 |  9643 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   38608 |  9644 | `		}` |
|  103969 |  9645 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  115139 |  9646 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9647 | `			/* Implements one or more interface */` |
|   11175 |  9648 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   11175 |  9649 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9650 | `				break;` |
|       - |  9651 | `			}` |
|    5590 |  9652 | `		}` |
|       - |  9653 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9654 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  155946 |  9655 | `		if( rc == SXRET_OK` |
|  103964 |  9656 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  103969 |  9657 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   84459 |  9658 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9659 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   84459 |  9660 | `			if( pStringable ){` |
|   84459 |  9661 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   84459 |  9662 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9663 | `				sxu32 i;` |
|   84459 |  9664 | `				int bAlready = 0;` |
|   91799 |  9665 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7347 |  9666 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9667 | `						bAlready = 1;` |
|       3 |  9668 | `						break;` |
|       - |  9669 | `					}` |
|    3675 |  9670 | `				}` |
|   84459 |  9671 | `				if( !bAlready ){` |
|   84457 |  9672 | `					PH7_ClassImplement(pClass,pStringable);` |
|   42226 |  9673 | `				}` |
|   42227 |  9674 | `			}` |
|   42227 |  9675 | `		}` |
|       - |  9676 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  103969 |  9677 | `		if( rc == SXRET_OK ){` |
|  103969 |  9678 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  103969 |  9679 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9680 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9681 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9682 | `				return SXERR_ABORT;` |
|       - |  9683 | `			}` |
|   51982 |  9684 | `		}` |
|       - |  9685 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  103969 |  9686 | `		if( rc == SXRET_OK ){` |
|  103969 |  9687 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  103969 |  9688 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9689 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9690 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9691 | `				return SXERR_ABORT;` |
|       - |  9692 | `			}` |
|   51982 |  9693 | `		}` |
|   51982 |  9694 | `	}` |
|  103969 |  9695 | `	SySetRelease(&aUseEntries);` |
|  103969 |  9696 | `	SySetRelease(&aInterfaces);` |
|  103969 |  9697 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9698 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9699 | `		return SXERR_ABORT;` |
|       - |  9700 | `	}` |
|   51982 |  9701 | `done:` |
|       - |  9702 | `	/* Point beyond the class body */` |
|  104007 |  9703 | `	pGen->pIn = &pEnd[1];` |
|  104007 |  9704 | `	pGen->pEnd = pTmp;` |
|  104007 |  9705 | `	return PH7_OK;` |
|   52007 |  9706 | `}` |
|       - |  9707 | `/* Compile a named class declaration (the common case). */` |
|  103978 |  9708 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9709 | `{` |
|  103983 |  9710 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9711 | `}` |
|       - |  9712 | `/*` |
|       - |  9713 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9714 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9715 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9716 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9717 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9718 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9719 | ` */` |
|      26 |  9720 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  9721 | `{` |
|       - |  9722 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9723 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9724 | `	SyString sName;` |
|       - |  9725 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9726 | `	ph7_value *pObj;` |
|      30 |  9727 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9728 | `	sxu32 nIdx,nLen;` |
|       - |  9729 | `	sxi32 nArg,rc;` |
|      13 |  9730 | `	SXUNUSED(iCompileFlag);` |
|       - |  9731 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      30 |  9732 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      30 |  9733 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9734 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9735 | `	}` |
|      30 |  9736 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9737 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9738 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9739 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      30 |  9740 | `	pArgStart = pArgEnd = 0;` |
|      30 |  9741 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      30 |  9742 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9743 | `		return rc;` |
|       - |  9744 | `	}` |
|       - |  9745 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9746 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      30 |  9747 | `	nArg = 0;` |
|      30 |  9748 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9749 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9750 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9751 | `		SyToken *pArgNext;` |
|       7 |  9752 | `		pGen->pIn = pArgStart;` |
|       7 |  9753 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9754 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9755 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9756 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 |  9757 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9758 | `					pGen->pIn = pSavedIn;` |
|     ! 0 |  9759 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 |  9760 | `					return SXERR_ABORT;` |
|       - |  9761 | `				}` |
|       7 |  9762 | `				nArg++;` |
|       3 |  9763 | `			}` |
|       7 |  9764 | `			pGen->pIn = &pArgNext[1];` |
|       1 |  9765 | `		}` |
|       7 |  9766 | `		pGen->pIn = pSavedIn;` |
|       7 |  9767 | `		pGen->pEnd = pSavedEnd;` |
|       3 |  9768 | `	}` |
|       - |  9769 | `	/* Load the synthesized class name */` |
|      30 |  9770 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      30 |  9771 | `	if( pObj == 0 ){` |
|     ! 0 |  9772 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9773 | `		return SXERR_ABORT;` |
|       - |  9774 | `	}` |
|      30 |  9775 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      30 |  9776 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  9777 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      30 |  9778 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      30 |  9779 | `	return SXRET_OK;` |
|      17 |  9780 | `}` |
|       - |  9781 | `/*` |
|       - |  9782 | ` * Compile a user-defined abstract class.` |
|       - |  9783 | ` *  According to the PHP language reference manual` |
|       - |  9784 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9785 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9786 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9787 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9788 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9789 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9790 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9791 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9792 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9793 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9794 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9795 | ` *   could differ.` |
|       - |  9796 | ` */` |
|       - |  9797 | `/*` |
|       - |  9798 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9799 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9800 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9801 | ` */` |
| 1006824 |  9802 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9803 | `{` |
| 1006829 |  9804 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  673725 |  9805 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  673725 |  9806 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  666371 |  9807 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  333152 |  9808 | `	}` |
|  999413 |  9809 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  999353 |  9810 | `	return FALSE;` |
|  503417 |  9811 | `}` |
|       - |  9812 | `/*` |
|       - |  9813 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9814 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9815 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9816 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9817 | ` */` |
|  999348 |  9818 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9819 | `{` |
|  999353 |  9820 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  999353 |  9821 | `	sxi32 iFlags = 0,iFlag;` |
| 1006829 |  9822 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7481 |  9823 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9824 | `			pDup = pIn;` |
|       2 |  9825 | `		}` |
|    7481 |  9826 | `		iFlags \|= iFlag;` |
|    7481 |  9827 | `		pIn++;` |
|       5 |  9828 | `	}` |
|  999353 |  9829 | `	*ppIn = pIn;` |
|  999353 |  9830 | `	if( ppDup ){ *ppDup = pDup; }` |
|  999353 |  9831 | `	return iFlags;` |
|       5 |  9832 | `}` |
|       - |  9833 | `/*` |
|       - |  9834 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9835 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9836 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9837 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9838 | `` * `readonly`) to their existing handlers.`` |
|       - |  9839 | ` */` |
|  995620 |  9840 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9841 | `{` |
|  995625 |  9842 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  501545 |  9843 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  997486 |  9844 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9845 | `}` |
|       - |  9846 | `/*` |
|       - |  9847 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9848 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9849 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9850 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9851 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9852 | ` */` |
|    3728 |  9853 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9854 | `{` |
|       - |  9855 | `	SyToken *pDup;` |
|    3733 |  9856 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9857 | `	sxi32 rc;` |
|    3733 |  9858 | `	if( pDup ){` |
|       4 |  9859 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9860 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9861 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9862 | `			return SXERR_ABORT;` |
|       - |  9863 | `		}` |
|       1 |  9864 | `	}` |
|    5592 |  9865 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1869 |  9866 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9867 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9868 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9869 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9870 | `			return SXERR_ABORT;` |
|       - |  9871 | `		}` |
|       1 |  9872 | `	}` |
|    3733 |  9873 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1869 |  9874 | `}` |
|       - |  9875 | `/*` |
|       - |  9876 | ` * Compile a user-defined trait.` |
|       - |  9877 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9878 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9879 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9880 | ` */` |
|      64 |  9881 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9882 | `{` |
|      69 |  9883 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9884 | `	ph7_class *pClass;` |
|       - |  9885 | `	SyToken *pEnd,*pTmp;` |
|       - |  9886 | `	sxi32 iProtection;` |
|       - |  9887 | `	sxi32 iAttrflags;` |
|       - |  9888 | `	SyString *pName;` |
|       - |  9889 | `	sxi32 nKwrd;` |
|       - |  9890 | `	sxi32 rc;` |
|       - |  9891 | `	/* Jump the 'trait' keyword */` |
|      69 |  9892 | `	pGen->pIn++;` |
|      69 |  9893 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9894 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9895 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9896 | `			return SXERR_ABORT;` |
|       - |  9897 | `		}` |
|     ! 0 |  9898 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9899 | `			pGen->pIn++;` |
|     ! 0 |  9900 | `		}` |
|     ! 0 |  9901 | `		return SXRET_OK;` |
|       - |  9902 | `	}` |
|       - |  9903 | `	/* Extract trait name */` |
|      69 |  9904 | `	pName = &pGen->pIn->sData;` |
|      69 |  9905 | `	pGen->pIn++;` |
|       - |  9906 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9907 | `		SyBlob sFQN;` |
|       - |  9908 | `		SyString sFQNStr;` |
|      69 |  9909 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      69 |  9910 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      69 |  9911 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      69 |  9912 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      69 |  9913 | `		SyBlobRelease(&sFQN);` |
|       - |  9914 | `	}` |
|      69 |  9915 | `	if( pClass == 0 ){` |
|     ! 0 |  9916 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9917 | `		return SXERR_ABORT;` |
|       - |  9918 | `	}` |
|       - |  9919 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      69 |  9920 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9921 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9922 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9923 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9924 | `			return SXERR_ABORT;` |
|       - |  9925 | `		}` |
|     ! 0 |  9926 | `		return SXRET_OK;` |
|       - |  9927 | `	}` |
|      69 |  9928 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      69 |  9929 | `	pEnd = 0;` |
|      69 |  9930 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      69 |  9931 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  9932 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  9933 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9934 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9935 | `			return SXERR_ABORT;` |
|       - |  9936 | `		}` |
|     ! 0 |  9937 | `		return SXRET_OK;` |
|       - |  9938 | `	}` |
|       - |  9939 | `	/* Swap token stream */` |
|      69 |  9940 | `	pTmp = pGen->pEnd;` |
|      69 |  9941 | `	pGen->pEnd = pEnd;` |
|       - |  9942 | `	/* Mark as trait */` |
|      69 |  9943 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9944 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      64 |  9945 | `	for(;;){` |
|     177 |  9946 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9947 | `			pGen->pIn++;` |
|       4 |  9948 | `		}` |
|     153 |  9949 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      69 |  9950 | `			break;` |
|       - |  9951 | `		}` |
|      89 |  9952 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9953 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9954 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9955 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9956 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9957 | `				return SXERR_ABORT;` |
|       - |  9958 | `			}` |
|     ! 0 |  9959 | `			goto done;` |
|       - |  9960 | `		}` |
|      89 |  9961 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      89 |  9962 | `		iAttrflags = 0;` |
|      89 |  9963 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      89 |  9964 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      89 |  9965 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9966 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9967 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9968 | `				for(;;){` |
|       - |  9969 | `					ph7_class *pUsedTrait;` |
|       - |  9970 | `					SyString *pUsedName;` |
|       5 |  9971 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9972 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9973 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9974 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9975 | `							return SXERR_ABORT;` |
|       - |  9976 | `						}` |
|     ! 0 |  9977 | `						break;` |
|       - |  9978 | `					}` |
|       5 |  9979 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9980 | `					{` |
|       - |  9981 | `						SyBlob sResolved;` |
|       5 |  9982 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9983 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9984 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9985 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9986 | `						SyBlobRelease(&sResolved);` |
|       - |  9987 | `					}` |
|       5 |  9988 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9989 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9990 | `					}` |
|       5 |  9991 | `					if( pUsedTrait == 0 ){` |
|       4 |  9992 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9993 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9994 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9995 | `							return SXERR_ABORT;` |
|       - |  9996 | `						}` |
|       2 |  9997 | `					}else{` |
|       3 |  9998 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9999 | `					}` |
|       5 | 10000 | `					pGen->pIn++;` |
|       5 | 10001 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 10002 | `						break;` |
|       - | 10003 | `					}` |
|     ! 0 | 10004 | `					pGen->pIn++;` |
|     ! 0 | 10005 | `				}` |
|       5 | 10006 | `				continue;` |
|       - | 10007 | `			}` |
|      85 | 10008 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 | 10009 | `				iProtection = nKwrd;` |
|      73 | 10010 | `				pGen->pIn++;` |
|      68 | 10011 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 | 10012 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10013 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10014 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 10015 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10016 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10017 | `						return SXERR_ABORT;` |
|       - | 10018 | `					}` |
|     ! 0 | 10019 | `					goto done;` |
|       - | 10020 | `				}` |
|      73 | 10021 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 | 10022 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 | 10023 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10024 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10025 | `							return SXERR_ABORT;` |
|       - | 10026 | `						}` |
|     ! 0 | 10027 | `						goto done;` |
|       - | 10028 | `					}` |
|      12 | 10029 | `					continue;` |
|       - | 10030 | `				}` |
|      63 | 10031 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 | 10032 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 10033 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10034 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10035 | `							return SXERR_ABORT;` |
|       - | 10036 | `						}` |
|     ! 0 | 10037 | `						goto done;` |
|       - | 10038 | `					}` |
|       5 | 10039 | `					continue;` |
|       - | 10040 | `				}` |
|      58 | 10041 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 | 10042 | `			}` |
|      71 | 10043 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 10044 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10045 | `					"Traits cannot have constants");` |
|     ! 0 | 10046 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10047 | `					return SXERR_ABORT;` |
|       - | 10048 | `				}` |
|     ! 0 | 10049 | `				goto done;` |
|     ! 0 | 10050 | `			}else{` |
|      71 | 10051 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 10052 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 10053 | `					pGen->pIn++;` |
|       5 | 10054 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 10055 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 10056 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 10057 | `							iProtection = nKwrd;` |
|     ! 0 | 10058 | `							pGen->pIn++;` |
|     ! 0 | 10059 | `						}` |
|       1 | 10060 | `					}` |
|       4 | 10061 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 | 10062 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10063 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10064 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 10065 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10066 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10067 | `							return SXERR_ABORT;` |
|       - | 10068 | `						}` |
|     ! 0 | 10069 | `						goto done;` |
|       - | 10070 | `					}` |
|       5 | 10071 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 10072 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 10073 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10074 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10075 | `								return SXERR_ABORT;` |
|       - | 10076 | `							}` |
|     ! 0 | 10077 | `							goto done;` |
|       - | 10078 | `						}` |
|       3 | 10079 | `						continue;` |
|       - | 10080 | `					}` |
|       3 | 10081 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 | 10082 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10083 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10084 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10085 | `								return SXERR_ABORT;` |
|       - | 10086 | `							}` |
|     ! 0 | 10087 | `							goto done;` |
|       - | 10088 | `						}` |
|     ! 0 | 10089 | `						continue;` |
|       - | 10090 | `					}` |
|       3 | 10091 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      68 | 10092 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 | 10093 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 | 10094 | `					pGen->pIn++;` |
|       6 | 10095 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 | 10096 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 | 10097 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 10098 | `							iProtection = nKwrd;` |
|       6 | 10099 | `							pGen->pIn++;` |
|       2 | 10100 | `						}` |
|       2 | 10101 | `					}` |
|       6 | 10102 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 10103 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 10104 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10105 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 10106 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10107 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10108 | `							return SXERR_ABORT;` |
|       - | 10109 | `						}` |
|     ! 0 | 10110 | `						goto done;` |
|       - | 10111 | `					}` |
|       6 | 10112 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 10113 | `				}` |
|      69 | 10114 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 10115 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10116 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 10117 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10118 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10119 | `						return SXERR_ABORT;` |
|       - | 10120 | `					}` |
|     ! 0 | 10121 | `					goto done;` |
|       - | 10122 | `				}` |
|      69 | 10123 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 10124 | `					pGen->pIn++;` |
|     ! 0 | 10125 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 10126 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10127 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 10128 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10129 | `							return SXERR_ABORT;` |
|       - | 10130 | `						}` |
|     ! 0 | 10131 | `						goto done;` |
|       - | 10132 | `					}` |
|     ! 0 | 10133 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10134 | `				}else{` |
|      69 | 10135 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 10136 | `				}` |
|      69 | 10137 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10138 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10139 | `						return SXERR_ABORT;` |
|       - | 10140 | `					}` |
|     ! 0 | 10141 | `					goto done;` |
|       - | 10142 | `				}` |
|       - | 10143 | `			}` |
|      37 | 10144 | `		}else{` |
|     ! 0 | 10145 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10146 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10147 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10148 | `					return SXERR_ABORT;` |
|       - | 10149 | `				}` |
|     ! 0 | 10150 | `				goto done;` |
|       - | 10151 | `			}` |
|       - | 10152 | `		}` |
|       5 | 10153 | `	}` |
|       - | 10154 | `	/* Install the trait */` |
|      69 | 10155 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      69 | 10156 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10157 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10158 | `		return SXERR_ABORT;` |
|       - | 10159 | `	}` |
|      32 | 10160 | `done:` |
|       - | 10161 | `	/* Point beyond the trait body */` |
|      69 | 10162 | `	pGen->pIn = &pEnd[1];` |
|      69 | 10163 | `	pGen->pEnd = pTmp;` |
|      69 | 10164 | `	return PH7_OK;` |
|      37 | 10165 | `}` |
|       - | 10166 | `/*` |
|       - | 10167 | ` * Compile a user-defined class.` |
|       - | 10168 | ` *  According to the PHP language reference manual` |
|       - | 10169 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 10170 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 10171 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 10172 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 10173 | ` *   and functions (called "methods").` |
|       - | 10174 | ` */` |
|  100250 | 10175 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 | 10176 | `{` |
|       - | 10177 | `	sxi32 rc;` |
|  100255 | 10178 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|  100255 | 10179 | `	return rc;` |
|       5 | 10180 | `}` |
|       - | 10181 | `/*` |
|       - | 10182 | ` * Exception handling.` |
|       - | 10183 | ` *  According to the PHP language reference manual` |
|       - | 10184 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 10185 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 10186 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 10187 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 10188 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 10189 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 10190 | ` *    (or re-thrown) within a catch block.` |
|       - | 10191 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 10192 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 10193 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 10194 | ` *    been defined with set_exception_handler().` |
|       - | 10195 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 10196 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 10197 | ` */` |
|       - | 10198 | `/*` |
|       - | 10199 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 10200 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 10201 | ` * indicates failure.` |
|       - | 10202 | ` */` |
|   15020 | 10203 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 10204 | `{` |
|   15025 | 10205 | `	sxi32 rc = SXRET_OK;` |
|   15025 | 10206 | `	if( pRoot->pOp ){` |
|   15015 | 10207 | `		switch( pRoot->pOp->iOp ){` |
|    7505 | 10208 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - | 10209 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - | 10210 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - | 10211 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - | 10212 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - | 10213 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   15015 | 10214 | `			break;` |
|     ! 0 | 10215 | `		default:` |
|       - | 10216 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - | 10217 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - | 10218 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 | 10219 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10220 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 10221 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 10222 | `				rc = SXERR_INVALID;` |
|     ! 0 | 10223 | `			}` |
|     ! 0 | 10224 | `			break;` |
|       - | 10225 | `		}` |
|    7520 | 10226 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 10227 | `		/* Unexpected expression */` |
|     ! 0 | 10228 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10229 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10230 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 10231 | `			rc = SXERR_INVALID;` |
|     ! 0 | 10232 | `		}` |
|     ! 0 | 10233 | `	}` |
|   15025 | 10234 | `	return rc;` |
|       5 | 10235 | `}` |
|       - | 10236 | `/*` |
|       - | 10237 | ` * Compile a 'throw' statement.` |
|       - | 10238 | ` * throw: This is how you trigger an exception.` |
|       - | 10239 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 10240 | ` */` |
|   14984 | 10241 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 10242 | `{` |
|   14989 | 10243 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10244 | `	GenBlock *pBlock;` |
|       - | 10245 | `	sxu32 nIdx;` |
|       - | 10246 | `	sxi32 rc;` |
|   14989 | 10247 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 10248 | `	/* Compile the expression */` |
|   14989 | 10249 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   14989 | 10250 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10251 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 10252 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10253 | `			return SXERR_ABORT;` |
|       - | 10254 | `		}` |
|     ! 0 | 10255 | `		return SXRET_OK;` |
|       - | 10256 | `	}` |
|   14989 | 10257 | `	pBlock = pGen->pCurrent;` |
|       - | 10258 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   59317 | 10259 | `	while(pBlock->pParent){` |
|   59313 | 10260 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   14985 | 10261 | `			break;` |
|       - | 10262 | `		}` |
|       - | 10263 | `		/* Point to the parent block */` |
|   44333 | 10264 | `		pBlock = pBlock->pParent;` |
|       5 | 10265 | `	}` |
|       - | 10266 | `	/* Emit the throw instruction */` |
|   14989 | 10267 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 10268 | `	/* Emit the jump */` |
|   14989 | 10269 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   14989 | 10270 | `	return SXRET_OK;` |
|    7497 | 10271 | `}` |
|       - | 10272 | `/*` |
|       - | 10273 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 10274 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 10275 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 10276 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 10277 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 10278 | ` */` |
|      36 | 10279 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 10280 | `{` |
|      38 | 10281 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10282 | `	GenBlock *pBlock;` |
|       - | 10283 | `	sxu32 nIdx;` |
|       - | 10284 | `	sxi32 rc;` |
|      18 | 10285 | `	(void)iCompileFlag;` |
|      38 | 10286 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 | 10287 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 10288 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10289 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10290 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10291 | `			return SXERR_ABORT;` |
|       - | 10292 | `		}` |
|     ! 0 | 10293 | `		return SXRET_OK;` |
|       - | 10294 | `	}` |
|      38 | 10295 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 | 10296 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10297 | `		return SXERR_ABORT;` |
|       - | 10298 | `	}` |
|      38 | 10299 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10300 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10301 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10302 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10303 | `			return SXERR_ABORT;` |
|       - | 10304 | `		}` |
|     ! 0 | 10305 | `		return SXRET_OK;` |
|       - | 10306 | `	}` |
|       - | 10307 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10308 | `	pBlock = pGen->pCurrent;` |
|      60 | 10309 | `	while( pBlock->pParent ){` |
|      49 | 10310 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10311 | `			break;` |
|       - | 10312 | `		}` |
|      23 | 10313 | `		pBlock = pBlock->pParent;` |
|       1 | 10314 | `	}` |
|      38 | 10315 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10316 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10317 | `	return SXRET_OK;` |
|      20 | 10318 | `}` |
|       - | 10319 | `/*` |
|       - | 10320 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|       - | 10321 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|       - | 10322 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|       - | 10323 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|       - | 10324 | ` * compile error propagated from the parser.` |
|       - | 10325 | ` */` |
|      40 | 10326 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|       4 | 10327 | `{` |
|       - | 10328 | `	SyString sClassName;` |
|       - | 10329 | `	SyToken *pToken;` |
|       - | 10330 | `	SyString *pName;` |
|       - | 10331 | `	char *zDup;` |
|       - | 10332 | `	sxi32 rc;` |
|      44 | 10333 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|      44 | 10334 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|      44 | 10335 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|      44 | 10336 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      44 | 10337 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 10338 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10339 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10340 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10341 | `		return SXERR_INVALID;` |
|       - | 10342 | `	}` |
|      44 | 10343 | `	pGen->pIn++; /* '(' */` |
|      20 | 10344 | `	for(;;){` |
|       - | 10345 | `		SyBlob sResolved;` |
|      44 | 10346 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      44 | 10347 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 10348 | `			SyBlobRelease(&sResolved);` |
|     ! 0 | 10349 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10350 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10351 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10352 | `			return SXERR_INVALID;` |
|       - | 10353 | `		}` |
|      64 | 10354 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      40 | 10355 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      44 | 10356 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|      44 | 10357 | `		SyBlobRelease(&sResolved);` |
|      44 | 10358 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|      44 | 10359 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|      44 | 10360 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      40 | 10361 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|       4 | 10362 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     ! 0 | 10363 | `			pGen->pIn++; continue;` |
|       - | 10364 | `		}` |
|      44 | 10365 | `		break;` |
|     ! 0 | 10366 | `	}` |
|      60 | 10367 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|      44 | 10368 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 10369 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10370 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10371 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10372 | `		return SXERR_INVALID;` |
|       - | 10373 | `	}` |
|      44 | 10374 | `	pGen->pIn++; /* '$' */` |
|      44 | 10375 | `	pName = &pGen->pIn->sData;` |
|      44 | 10376 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      44 | 10377 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|      44 | 10378 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|      44 | 10379 | `	pGen->pIn++;` |
|      44 | 10380 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 10381 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10382 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10383 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10384 | `		return SXERR_INVALID;` |
|       - | 10385 | `	}` |
|      44 | 10386 | `	pGen->pIn++; /* ')' */` |
|      44 | 10387 | `	return SXRET_OK;` |
|      24 | 10388 | `}` |
|       - | 10389 | `/*` |
|       - | 10390 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|       - | 10391 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|       - | 10392 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|       - | 10393 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|       - | 10394 | ` * VmThrowException):` |
|       - | 10395 | ` *` |
|       - | 10396 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|       - | 10397 | ` *    <try body>` |
|       - | 10398 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|       - | 10399 | ` *    JMP  -> finally\|end` |
|       - | 10400 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|       - | 10401 | ` *    <catch body>` |
|       - | 10402 | ` *    JMP  -> finally\|end` |
|       - | 10403 | ` *    ... more catches ...` |
|       - | 10404 | ` *  Lfin: <finally body>` |
|       - | 10405 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|       - | 10406 | ` *  Lend:` |
|       - | 10407 | ` */` |
|      62 | 10408 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|       4 | 10409 | `{` |
|      66 | 10410 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10411 | `	GenBlock *pTry;` |
|       - | 10412 | `	VmInstr *pInstr;` |
|      66 | 10413 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|       - | 10414 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|       - | 10415 | `	sxi32 rc;` |
|      66 | 10416 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|       - | 10417 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      66 | 10418 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      66 | 10419 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      66 | 10420 | `	pTry->pUserData = pException;` |
|      66 | 10421 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      66 | 10422 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      66 | 10423 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      66 | 10424 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      66 | 10425 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      66 | 10426 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10427 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      66 | 10428 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      66 | 10429 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      66 | 10430 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      66 | 10431 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10432 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      66 | 10433 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|       - | 10434 | `	/* Catch clauses (inline) */` |
|      66 | 10435 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      62 | 10436 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|      44 | 10437 | `		sxu32 k = 0;` |
|      60 | 10438 | `		for(;;){` |
|       - | 10439 | `			ph7_exception_block sCatch;` |
|       - | 10440 | `			GenBlock *pCatchBlk;` |
|      84 | 10441 | `			sxu32 idxJmp = 0;` |
|      80 | 10442 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      77 | 10443 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      24 | 10444 | `				break;` |
|       - | 10445 | `			}` |
|      44 | 10446 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|      44 | 10447 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      44 | 10448 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|      44 | 10449 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|      44 | 10450 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|      44 | 10451 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|      44 | 10452 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       - | 10453 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|       - | 10454 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|       - | 10455 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|      44 | 10456 | `			pCatchBlk->pUserData = pException;` |
|      44 | 10457 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      44 | 10458 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      44 | 10459 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      44 | 10460 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10461 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|       - | 10462 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|      44 | 10463 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      44 | 10464 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|      44 | 10465 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|      44 | 10466 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      44 | 10467 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      44 | 10468 | `			k++;` |
|       4 | 10469 | `		}` |
|      20 | 10470 | `	}` |
|       - | 10471 | `	/* Finally (inline) */` |
|      66 | 10472 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      48 | 10473 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10474 | `		GenBlock *pFinBlk;` |
|      28 | 10475 | `		pGen->pIn++; /* Jump 'finally' */` |
|      28 | 10476 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10477 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|      28 | 10478 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      28 | 10479 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      28 | 10480 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      28 | 10481 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      28 | 10482 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      28 | 10483 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|      28 | 10484 | `		pException->iHasFinally = 1;` |
|      12 | 10485 | `	}` |
|      66 | 10486 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      66 | 10487 | `	pException->iInlined = 1;` |
|       - | 10488 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|       - | 10489 | `	{` |
|      66 | 10490 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|       - | 10491 | `		sxu32 *aJ; sxu32 n;` |
|      66 | 10492 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      66 | 10493 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      66 | 10494 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|     106 | 10495 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|      44 | 10496 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|      44 | 10497 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      24 | 10498 | `		}` |
|       - | 10499 | `	}` |
|      66 | 10500 | `	SySetRelease(&aCatchJmp);` |
|      66 | 10501 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|     ! 0 | 10502 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|     ! 0 | 10503 | `	}` |
|      66 | 10504 | `	return SXRET_OK;` |
|      35 | 10505 | `}` |
|       - | 10506 | `/*` |
|       - | 10507 | ` * Compile a 'catch' block.` |
|       - | 10508 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10509 | ` * an object containing the exception information.` |
|       - | 10510 | ` */` |
|     606 | 10511 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10512 | `{` |
|     611 | 10513 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10514 | `	ph7_exception_block sCatch;` |
|       - | 10515 | `	SySet *pInstrContainer;` |
|       - | 10516 | `	SyString sClassName;` |
|       - | 10517 | `	GenBlock *pCatch;` |
|       - | 10518 | `	SyToken *pToken;` |
|       - | 10519 | `	SyString *pName;` |
|       - | 10520 | `	char *zDup;` |
|       - | 10521 | `	sxi32 rc;` |
|     611 | 10522 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10523 | `	/* Zero the structure */` |
|     611 | 10524 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10525 | `	/* Initialize fields */` |
|     611 | 10526 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     611 | 10527 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     611 | 10528 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10529 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10530 | `			pToken = pGen->pIn;` |
|     ! 0 | 10531 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10532 | `				pToken--;` |
|     ! 0 | 10533 | `			}` |
|     ! 0 | 10534 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10535 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10536 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10537 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10538 | `				return SXERR_ABORT;` |
|       - | 10539 | `			}` |
|     ! 0 | 10540 | `			return SXERR_INVALID;` |
|       - | 10541 | `	}` |
|       - | 10542 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     611 | 10543 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     317 | 10544 | `	for(;;){` |
|       - | 10545 | `		SyBlob sResolved;` |
|     639 | 10546 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     639 | 10547 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10548 | `			SyBlobRelease(&sResolved);` |
|       6 | 10549 | `			pToken = pGen->pIn;` |
|       6 | 10550 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10551 | `				pToken--;` |
|     ! 0 | 10552 | `			}` |
|       8 | 10553 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10554 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10555 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10556 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10557 | `				return SXERR_ABORT;` |
|       - | 10558 | `			}` |
|       6 | 10559 | `			return SXERR_INVALID;` |
|       - | 10560 | `		}` |
|       - | 10561 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10562 | `		 * transient SyBlob allocation. */` |
|     950 | 10563 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     630 | 10564 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     635 | 10565 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     635 | 10566 | `		SyBlobRelease(&sResolved);` |
|     635 | 10567 | `		if( zDup == 0 ){` |
|     ! 0 | 10568 | `			goto Mem;` |
|       - | 10569 | `		}` |
|     635 | 10570 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     635 | 10571 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10572 | `			goto Mem;` |
|       - | 10573 | `		}` |
|       - | 10574 | `		/* Check for '\|' (multi-catch separator) */` |
|     644 | 10575 | `		if( pGen->pIn < pGen->pEnd &&` |
|     630 | 10576 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 10577 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 10578 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 10579 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 10580 | `			continue;` |
|       - | 10581 | `		}` |
|     607 | 10582 | `		break;` |
|     ! 0 | 10583 | `	}` |
|     903 | 10584 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     607 | 10585 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10586 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10587 | `			pToken = pGen->pIn;` |
|     ! 0 | 10588 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10589 | `				pToken--;` |
|     ! 0 | 10590 | `			}` |
|     ! 0 | 10591 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10592 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10593 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10594 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10595 | `				return SXERR_ABORT;` |
|       - | 10596 | `			}` |
|     ! 0 | 10597 | `			return SXERR_INVALID;` |
|       - | 10598 | `	}` |
|     607 | 10599 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10600 | `	/* Duplicate instance name */` |
|     607 | 10601 | `	pName = &pGen->pIn->sData;` |
|     607 | 10602 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     607 | 10603 | `	if( zDup == 0 ){` |
|     ! 0 | 10604 | `		goto Mem;` |
|       - | 10605 | `	}` |
|     607 | 10606 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     607 | 10607 | `	pGen->pIn++;` |
|     607 | 10608 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10609 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10610 | `		pToken = pGen->pIn;` |
|     ! 0 | 10611 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10612 | `			pToken--;` |
|     ! 0 | 10613 | `		}` |
|     ! 0 | 10614 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10615 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10616 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10617 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10618 | `			return SXERR_ABORT;` |
|       - | 10619 | `		}` |
|     ! 0 | 10620 | `		return SXERR_INVALID;` |
|       - | 10621 | `	}` |
|       - | 10622 | `	/* Compile the block */` |
|     607 | 10623 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10624 | `	/* Create the catch block */` |
|     607 | 10625 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     607 | 10626 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10627 | `		return SXERR_ABORT;` |
|       - | 10628 | `	}` |
|       - | 10629 | `	/* Swap bytecode container */` |
|     607 | 10630 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     607 | 10631 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10632 | `	/* Compile the block */` |
|     607 | 10633 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10634 | `	/* Fix forward jumps now the destination is resolved  */` |
|     607 | 10635 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10636 | `	/* Emit the DONE instruction */` |
|     607 | 10637 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10638 | `	/* Leave the block */` |
|     607 | 10639 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10640 | `	/* Restore the default container */` |
|     607 | 10641 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10642 | `	/* Install the catch block */` |
|     607 | 10643 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     607 | 10644 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10645 | `		goto Mem;` |
|       - | 10646 | `	}` |
|     607 | 10647 | `	return SXRET_OK;` |
|     ! 0 | 10648 | `Mem:` |
|     ! 0 | 10649 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10650 | `	return SXERR_ABORT;` |
|     308 | 10651 | `}` |
|       - | 10652 | `/*` |
|       - | 10653 | ` * Compile a 'try' block.` |
|       - | 10654 | ` * A function using an exception should be in a "try" block.` |
|       - | 10655 | ` * If the exception does not trigger, the code will continue` |
|       - | 10656 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10657 | ` * is "thrown".` |
|       - | 10658 | ` */` |
|     712 | 10659 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10660 | `{` |
|       - | 10661 | `	ph7_exception *pException;` |
|     717 | 10662 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10663 | `	GenBlock *pTry;` |
|       - | 10664 | `	sxu32 nJmpIdx;` |
|       - | 10665 | `	sxi32 rc;` |
|       - | 10666 | `	/* Create the exception container */` |
|     717 | 10667 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     717 | 10668 | `	if( pException == 0 ){` |
|     ! 0 | 10669 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 10670 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10671 | `		return SXERR_ABORT;` |
|       - | 10672 | `	}` |
|       - | 10673 | `	/* Zero the structure */` |
|     717 | 10674 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 10675 | `	/* Initialize fields */` |
|     717 | 10676 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     717 | 10677 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     717 | 10678 | `	pException->iHasFinally = 0;` |
|     717 | 10679 | `	pException->iFinallyDone = 0;` |
|     717 | 10680 | `	pException->pVm = pGen->pVm;` |
|       - | 10681 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|       - | 10682 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|       - | 10683 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|       - | 10684 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|       - | 10685 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|       - | 10686 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     717 | 10687 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      66 | 10688 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|       - | 10689 | `	}` |
|       - | 10690 | `	/* Create the try block */` |
|     655 | 10691 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     655 | 10692 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10693 | `		return SXERR_ABORT;` |
|       - | 10694 | `	}` |
|       - | 10695 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     655 | 10696 | `	pTry->pUserData = pException;` |
|       - | 10697 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     655 | 10698 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 10699 | `	/* Fix the jump later when the destination is resolved */` |
|     655 | 10700 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     655 | 10701 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 10702 | `	/* Compile the block */` |
|     655 | 10703 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     655 | 10704 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10705 | `		return SXERR_ABORT;` |
|       - | 10706 | `	}` |
|       - | 10707 | `	/* Fix forward jumps now the destination is resolved */` |
|     655 | 10708 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10709 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     655 | 10710 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 10711 | `	/* Leave the block */` |
|     655 | 10712 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10713 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     655 | 10714 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     648 | 10715 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 10716 | `		/* Compile one or more catch blocks */` |
|     602 | 10717 | `		for(;;){` |
|    1204 | 10718 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     979 | 10719 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     304 | 10720 | `					break;` |
|       - | 10721 | `			}` |
|     611 | 10722 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     611 | 10723 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10724 | `				return SXERR_ABORT;` |
|       - | 10725 | `			}` |
|       5 | 10726 | `		}` |
|     299 | 10727 | `	}` |
|       - | 10728 | `	/* Compile optional finally block */` |
|     655 | 10729 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     364 | 10730 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10731 | `		SySet *pInstrContainer;` |
|       - | 10732 | `		GenBlock *pFinBlock;` |
|     113 | 10733 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10734 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     113 | 10735 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     113 | 10736 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10737 | `			return SXERR_ABORT;` |
|       - | 10738 | `		}` |
|       - | 10739 | `		/* Swap bytecode container */` |
|     113 | 10740 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     113 | 10741 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10742 | `		/* Compile the finally body */` |
|     113 | 10743 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     113 | 10744 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10745 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10746 | `			return SXERR_ABORT;` |
|       - | 10747 | `		}` |
|       - | 10748 | `		/* Fix forward jumps now the destination is resolved */` |
|     113 | 10749 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10750 | `		/* Emit DONE to terminate the finally block */` |
|     113 | 10751 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10752 | `		/* Leave the block */` |
|     113 | 10753 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10754 | `		/* Restore the default container */` |
|     113 | 10755 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     113 | 10756 | `		pException->iHasFinally = 1;` |
|      54 | 10757 | `	}` |
|       - | 10758 | `	/* Must have at least one catch or finally */` |
|     655 | 10759 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 | 10760 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10761 | `			"Cannot use try without catch or finally");` |
|       8 | 10762 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10763 | `			return SXERR_ABORT;` |
|       - | 10764 | `		}` |
|       3 | 10765 | `	}` |
|     655 | 10766 | `	return SXRET_OK;` |
|     361 | 10767 | `}` |
|       - | 10768 | `/*` |
|       - | 10769 | ` * Compile a switch block.` |
|       - | 10770 | ` *  (See block-comment below for more information)` |
|       - | 10771 | ` */` |
|     112 | 10772 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 10773 | `{` |
|     117 | 10774 | `	sxi32 rc = SXRET_OK;` |
|     117 | 10775 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 10776 | `		/* Unexpected token */` |
|     ! 0 | 10777 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10778 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10779 | `			return SXERR_ABORT;` |
|       - | 10780 | `		}` |
|     ! 0 | 10781 | `		pGen->pIn++;` |
|     ! 0 | 10782 | `	}` |
|     117 | 10783 | `	pGen->pIn++;` |
|       - | 10784 | `	/* First instruction to execute in this block. */` |
|     117 | 10785 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10786 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 10787 | `	 * or the '}' token */` |
|     206 | 10788 | `	for(;;){` |
|     417 | 10789 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10790 | `			/* No more input to process */` |
|     ! 0 | 10791 | `			break;` |
|       - | 10792 | `		}` |
|     417 | 10793 | `		rc = SXRET_OK;` |
|     417 | 10794 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 10795 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 10796 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 10797 | `					/* Unexpected token */` |
|     ! 0 | 10798 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10799 | `						&pGen->pIn->sData);` |
|     ! 0 | 10800 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10801 | `						return SXERR_ABORT;` |
|       - | 10802 | `					}` |
|       - | 10803 | `					/* FALL THROUGH */` |
|     ! 0 | 10804 | `				}` |
|      31 | 10805 | `				rc = SXERR_EOF;` |
|      31 | 10806 | `				break;` |
|       - | 10807 | `			}` |
|      32 | 10808 | `		}else{` |
|       - | 10809 | `			sxi32 nKwrd;` |
|       - | 10810 | `			/* Extract the keyword */` |
|     337 | 10811 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 10812 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 10813 | `				break;` |
|       - | 10814 | `			}` |
|     253 | 10815 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10816 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 10817 | `					/* Unexpected token */` |
|     ! 0 | 10818 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10819 | `						&pGen->pIn->sData);` |
|     ! 0 | 10820 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10821 | `						return SXERR_ABORT;` |
|       - | 10822 | `					}` |
|       - | 10823 | `					/* FALL THROUGH */` |
|     ! 0 | 10824 | `				}` |
|       - | 10825 | `				/* Block compiled */` |
|       3 | 10826 | `				break;` |
|       - | 10827 | `			}` |
|       - | 10828 | `		}` |
|       - | 10829 | `		/* Compile block */` |
|     305 | 10830 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 10831 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10832 | `			return SXERR_ABORT;` |
|       - | 10833 | `		}` |
|       5 | 10834 | `	}` |
|     117 | 10835 | `	return rc;` |
|      61 | 10836 | `}` |
|       - | 10837 | `/*` |
|       - | 10838 | ` * Compile a case eXpression.` |
|       - | 10839 | ` *  (See block-comment below for more information)` |
|       - | 10840 | ` */` |
|      92 | 10841 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 10842 | `{` |
|       - | 10843 | `	SySet *pInstrContainer;` |
|       - | 10844 | `	SyToken *pEnd,*pTmp;` |
|      97 | 10845 | `	sxi32 iNest = 0;` |
|       - | 10846 | `	sxi32 rc;` |
|       - | 10847 | `	/* Delimit the expression */` |
|      97 | 10848 | `	pEnd = pGen->pIn;` |
|     197 | 10849 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 10850 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 10851 | `			/* Increment nesting level */` |
|       3 | 10852 | `			iNest++;` |
|     196 | 10853 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 10854 | `			/* Decrement nesting level */` |
|       3 | 10855 | `			iNest--;` |
|     194 | 10856 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 10857 | `			break;` |
|       - | 10858 | `		}` |
|     105 | 10859 | `		pEnd++;` |
|       5 | 10860 | `	}` |
|      97 | 10861 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 10862 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 10863 | `		if( rc == SXERR_ABORT ){` |
|       - | 10864 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10865 | `			return SXERR_ABORT;` |
|       - | 10866 | `		}` |
|     ! 0 | 10867 | `	}` |
|       - | 10868 | `	/* Swap token stream */` |
|      97 | 10869 | `	pTmp = pGen->pEnd;` |
|      97 | 10870 | `	pGen->pEnd = pEnd;` |
|      97 | 10871 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 10872 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 10873 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 10874 | `	/* Emit the done instruction */` |
|      97 | 10875 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 10876 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10877 | `	/* Update token stream */` |
|      97 | 10878 | `	pGen->pIn  = pEnd;` |
|      97 | 10879 | `	pGen->pEnd = pTmp;` |
|      97 | 10880 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10881 | `		return SXERR_ABORT;` |
|       - | 10882 | `	}` |
|      97 | 10883 | `	return SXRET_OK;` |
|      51 | 10884 | `}` |
|       - | 10885 | `/*` |
|       - | 10886 | ` * Compile the smart switch statement.` |
|       - | 10887 | ` * According to the PHP language reference manual` |
|       - | 10888 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 10889 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 10890 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 10891 | ` *  This is exactly what the switch statement is for.` |
|       - | 10892 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 10893 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 10894 | ` *  of the outer loop, use continue 2.` |
|       - | 10895 | ` *  Note that switch/case does loose comparision.` |
|       - | 10896 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 10897 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 10898 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 10899 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 10900 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 10901 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 10902 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 10903 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 10904 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 10905 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 10906 | ` *  list for the next case.` |
|       - | 10907 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 10908 | ` *  or floating-point numbers and strings.` |
|       - | 10909 | ` */` |
|      28 | 10910 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 10911 | `{` |
|       - | 10912 | `	GenBlock *pSwitchBlock;` |
|       - | 10913 | `	SyToken *pTmp,*pEnd;` |
|       - | 10914 | `	ph7_switch *pSwitch;` |
|       - | 10915 | `	sxu32 nToken;` |
|       - | 10916 | `	sxu32 nLine;` |
|       - | 10917 | `	sxi32 rc;` |
|      33 | 10918 | `	nLine = pGen->pIn->nLine;` |
|       - | 10919 | `	/* Jump the 'switch' keyword */` |
|      33 | 10920 | `	pGen->pIn++;` |
|      33 | 10921 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 10922 | `		/* Syntax error */` |
|     ! 0 | 10923 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 10924 | `		if( rc == SXERR_ABORT ){` |
|       - | 10925 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10926 | `			return SXERR_ABORT;` |
|       - | 10927 | `		}` |
|     ! 0 | 10928 | `		goto Synchronize;` |
|       - | 10929 | `	}` |
|       - | 10930 | `	/* Jump the left parenthesis '(' */` |
|      33 | 10931 | `	pGen->pIn++;` |
|      33 | 10932 | `	pEnd = 0; /* cc warning */` |
|       - | 10933 | `	/* Create the loop block */` |
|      47 | 10934 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 10935 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 10936 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10937 | `		return SXERR_ABORT;` |
|       - | 10938 | `	}` |
|       - | 10939 | `	/* Delimit the condition */` |
|      33 | 10940 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 10941 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 10942 | `		/* Empty expression */` |
|     ! 0 | 10943 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 10944 | `		if( rc == SXERR_ABORT ){` |
|       - | 10945 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10946 | `			return SXERR_ABORT;` |
|       - | 10947 | `		}` |
|     ! 0 | 10948 | `	}` |
|       - | 10949 | `	/* Swap token streams */` |
|      33 | 10950 | `	pTmp = pGen->pEnd;` |
|      33 | 10951 | `	pGen->pEnd = pEnd;` |
|       - | 10952 | `	/* Compile the expression */` |
|      33 | 10953 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 10954 | `	if( rc == SXERR_ABORT ){` |
|       - | 10955 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 10956 | `		return SXERR_ABORT;` |
|       - | 10957 | `	}` |
|       - | 10958 | `	/* Update token stream */` |
|      33 | 10959 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 10960 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10961 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10962 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10963 | `			return SXERR_ABORT;` |
|       - | 10964 | `		}` |
|     ! 0 | 10965 | `		pGen->pIn++;` |
|     ! 0 | 10966 | `	}` |
|      33 | 10967 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 10968 | `	pGen->pEnd = pTmp;` |
|      33 | 10969 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 10970 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 10971 | `			pTmp = pGen->pIn;` |
|     ! 0 | 10972 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 10973 | `				pTmp--;` |
|     ! 0 | 10974 | `			}` |
|       - | 10975 | `			/* Unexpected token */` |
|     ! 0 | 10976 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 10977 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10978 | `				return SXERR_ABORT;` |
|       - | 10979 | `			}` |
|     ! 0 | 10980 | `			goto Synchronize;` |
|       - | 10981 | `	}` |
|       - | 10982 | `	/* Set the delimiter token */` |
|      33 | 10983 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 10984 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 10985 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 10986 | `	}else{` |
|      31 | 10987 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 10988 | `	}` |
|      33 | 10989 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 10990 | `	/* Create the switch blocks container */` |
|      33 | 10991 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 10992 | `	if( pSwitch == 0 ){` |
|       - | 10993 | `		/* Abort compilation */` |
|     ! 0 | 10994 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10995 | `		return SXERR_ABORT;` |
|       - | 10996 | `	}` |
|       - | 10997 | `	/* Zero the structure */` |
|      33 | 10998 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 10999 | `	/* Initialize fields */` |
|      33 | 11000 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 11001 | `	/* Emit the switch instruction */` |
|      33 | 11002 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 11003 | `	/* Compile case blocks */` |
|     100 | 11004 | `	for(;;){` |
|       - | 11005 | `		sxu32 nKwrd;` |
|     119 | 11006 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11007 | `			/* No more input to process */` |
|     ! 0 | 11008 | `			break;` |
|       - | 11009 | `		}` |
|     119 | 11010 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 11011 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 11012 | `				/* Unexpected token */` |
|     ! 0 | 11013 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11014 | `					&pGen->pIn->sData);` |
|     ! 0 | 11015 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11016 | `					return SXERR_ABORT;` |
|       - | 11017 | `				}` |
|       - | 11018 | `				/* FALL THROUGH */` |
|     ! 0 | 11019 | `			}` |
|       - | 11020 | `			/* Block compiled */` |
|     ! 0 | 11021 | `			break;` |
|       - | 11022 | `		}` |
|       - | 11023 | `		/* Extract the keyword */` |
|     119 | 11024 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 11025 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 11026 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 11027 | `				/* Unexpected token */` |
|     ! 0 | 11028 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11029 | `					&pGen->pIn->sData);` |
|     ! 0 | 11030 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11031 | `					return SXERR_ABORT;` |
|       - | 11032 | `				}` |
|       - | 11033 | `				/* FALL THROUGH */` |
|     ! 0 | 11034 | `			}` |
|       - | 11035 | `			/* Block compiled */` |
|       3 | 11036 | `			break;` |
|       - | 11037 | `		}` |
|     117 | 11038 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 11039 | `			/*` |
|       - | 11040 | `			 * Accroding to the PHP language reference manual` |
|       - | 11041 | `			 *  A special case is the default case. This case matches anything` |
|       - | 11042 | `			 *  that wasn't matched by the other cases.` |
|       - | 11043 | `			 */` |
|      25 | 11044 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 11045 | `				/* Default case already compiled */` |
|     ! 0 | 11046 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 11047 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11048 | `					return SXERR_ABORT;` |
|       - | 11049 | `				}` |
|     ! 0 | 11050 | `			}` |
|      25 | 11051 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 11052 | `			/* Compile the default block */` |
|      25 | 11053 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 11054 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11055 | `				return SXERR_ABORT;` |
|      25 | 11056 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 11057 | `				break;` |
|       1 | 11058 | `			}` |
|      98 | 11059 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 11060 | `			ph7_case_expr sCase;` |
|       - | 11061 | `			/* Standard case block */` |
|      97 | 11062 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 11063 | `			/* initialize the structure */` |
|      97 | 11064 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 11065 | `			/* Compile the case expression */` |
|      97 | 11066 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 11067 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11068 | `				return SXERR_ABORT;` |
|       - | 11069 | `			}` |
|       - | 11070 | `			/* Compile the case block */` |
|      97 | 11071 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 11072 | `			/* Insert in the switch container */` |
|      97 | 11073 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 11074 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11075 | `				return SXERR_ABORT;` |
|      97 | 11076 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 11077 | `				break;` |
|       - | 11078 | `			}` |
|      47 | 11079 | `		}else{` |
|       - | 11080 | `			/* Unexpected token */` |
|     ! 0 | 11081 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11082 | `				&pGen->pIn->sData);` |
|     ! 0 | 11083 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11084 | `				return SXERR_ABORT;` |
|       - | 11085 | `			}` |
|     ! 0 | 11086 | `			break;` |
|       - | 11087 | `		}` |
|       5 | 11088 | `	}` |
|       - | 11089 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 11090 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 11091 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11092 | `	/* Release the loop block */` |
|      33 | 11093 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 11094 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 11095 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 11096 | `		pGen->pIn++;` |
|      14 | 11097 | `	}` |
|       - | 11098 | `	/* Statement successfully compiled */` |
|      33 | 11099 | `	return SXRET_OK;` |
|     ! 0 | 11100 | `Synchronize:` |
|       - | 11101 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 11102 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 11103 | `		pGen->pIn++;` |
|     ! 0 | 11104 | `	}` |
|     ! 0 | 11105 | `	return SXRET_OK;` |
|      19 | 11106 | `}` |
|       - | 11107 | `/*` |
|       - | 11108 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 11109 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 11110 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 11111 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 11112 | ` */` |
|       - | 11113 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 11114 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 11115 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 11116 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 11117 |  |
|       - | 11118 | `/*` |
|       - | 11119 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 11120 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 11121 | ` * patched entries from the pending set.` |
|       - | 11122 | ` */` |
| 2725334 | 11123 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 11124 | `{` |
| 2725339 | 11125 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 11126 | `	sxu32 nTarget;` |
|       - | 11127 | `	sxu32 *aIdx;` |
|       - | 11128 | `	sxu32 i;` |
| 2725339 | 11129 | `	if( nCur <= nBaseline ){` |
| 2725245 | 11130 | `		return;` |
|       - | 11131 | `	}` |
|      98 | 11132 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      98 | 11133 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     200 | 11134 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     106 | 11135 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     106 | 11136 | `		if( pInstr ){` |
|     106 | 11137 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      51 | 11138 | `		}` |
|      55 | 11139 | `	}` |
|      98 | 11140 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1362672 | 11141 | `}` |
|       - | 11142 |  |
|       - | 11143 | `/*` |
|       - | 11144 | ` * By-reference out-parameters of builtin functions.` |
|       - | 11145 | ` *` |
|       - | 11146 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 11147 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 11148 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 11149 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 11150 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 11151 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 11152 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 11153 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 11154 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 11155 | ` * creates it" behaviour).` |
|       - | 11156 | ` *` |
|       - | 11157 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 11158 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 11159 | ` */` |
|  458098 | 11160 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 11161 | `{` |
|       - | 11162 | `	static const struct {` |
|       - | 11163 | `		const char *zName;` |
|       - | 11164 | `		sxu32 nByte;` |
|       - | 11165 | `		sxu32 mask;` |
|       - | 11166 | `	} aByRef[] = {` |
|       - | 11167 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11168 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11169 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11170 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11171 | `	};` |
|       - | 11172 | `	sxu32 i;` |
|  458103 | 11173 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1757 | 11174 | `		return 0;` |
|       - | 11175 | `	}` |
| 2281463 | 11176 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1825206 | 11177 | `		if( pName->nByte == aByRef[i].nByte` |
|  935634 | 11178 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      99 | 11179 | `			return aByRef[i].mask;` |
|       - | 11180 | `		}` |
|  912561 | 11181 | `	}` |
|  456257 | 11182 | `	return 0;` |
|  229054 | 11183 | `}` |
|       - | 11184 | `/*` |
|       - | 11185 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 11186 | ` *` |
|       - | 11187 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 11188 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 11189 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 11190 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 11191 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 11192 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 11193 | ` */` |
|  458098 | 11194 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 11195 | `{` |
|       - | 11196 | `	SyToken *p, *pEnd;` |
|  458103 | 11197 | `	pOut->zString = 0;` |
|  458103 | 11198 | `	pOut->nByte = 0;` |
|  458103 | 11199 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 11200 | `		return;` |
|       - | 11201 | `	}` |
|  458103 | 11202 | `	p = pLeft->pStart;` |
|  458103 | 11203 | `	pEnd = pLeft->pEnd;` |
|       - | 11204 | `	/* Optional single leading namespace separator (absolute path). */` |
|  458103 | 11205 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3695 | 11206 | `		p++;` |
|    1845 | 11207 | `	}` |
|  458103 | 11208 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1729 | 11209 | `		return;` |
|       - | 11210 | `	}` |
|       - | 11211 | `	/* Must be a single component: nothing follows the name token. */` |
|  456379 | 11212 | `	if( p + 1 != pEnd ){` |
|      33 | 11213 | `		return;` |
|       - | 11214 | `	}` |
|  456351 | 11215 | `	*pOut = p->sData;` |
|  229054 | 11216 | `}` |
|       - | 11217 | `/*` |
|       - | 11218 | ` * Generate bytecode for a given expression tree.` |
|       - | 11219 | ` * If something goes wrong while generating bytecode` |
|       - | 11220 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 11221 | ` * this function takes care of generating the appropriate` |
|       - | 11222 | ` * error message.` |
|       - | 11223 | ` */` |
| 3647422 | 11224 | `static sxi32 GenStateEmitExprCode(` |
|       - | 11225 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11226 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 11227 | `	sxi32 iFlags /* Control flags */` |
|       - | 11228 | `	)` |
|       5 | 11229 | `{` |
|       - | 11230 | `	VmInstr *pInstr;` |
|       - | 11231 | `	sxu32 nJmpIdx;` |
| 3647427 | 11232 | `	sxi32 iP1 = 0;` |
| 3647427 | 11233 | `	sxu32 iP2 = 0;` |
| 3647427 | 11234 | `	void *p3  = 0;` |
|       - | 11235 | `	sxi32 iVmOp;` |
|       - | 11236 | `	sxi32 rc;` |
| 3647427 | 11237 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3647427 | 11238 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 3647427 | 11239 | `	sxu32 nRhsNsBase = 0;` |
| 3647427 | 11240 | `	if( pNode->xCode ){` |
|       - | 11241 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 11242 | `		/* Compile node */` |
| 2276849 | 11243 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2276849 | 11244 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2276849 | 11245 | `		RE_SWAP_DELIMITER(pGen);` |
| 2276849 | 11246 | `		return rc;` |
|       - | 11247 | `	}` |
| 1370583 | 11248 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 11249 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11250 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 11251 | `		return SXERR_ABORT;` |
|       - | 11252 | `	}` |
| 1370583 | 11253 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1370583 | 11254 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      65 | 11255 | `		sxu32 nJmp = 0;` |
|       - | 11256 | `		sxu32 nNcNsBase;` |
|       - | 11257 | `		VmInstr *pInstrFix;` |
|       - | 11258 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 11259 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 11260 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 11261 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 11262 | `		 * stack slot carries a writable nIdx. */` |
|      65 | 11263 | `		if( pNode->pRight ){` |
|      65 | 11264 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11265 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      65 | 11266 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11267 | `				return rc;` |
|       - | 11268 | `			}` |
|      65 | 11269 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 11270 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 11271 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 11272 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 11273 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 11274 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 11275 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 11276 | `			 * cascade for the actual write path stays correct. */` |
|      65 | 11277 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11278 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      31 | 11279 | `				pInstrFix->iP2 = 3;` |
|      14 | 11280 | `			}` |
|      31 | 11281 | `		}` |
|       - | 11282 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      65 | 11283 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 11284 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      65 | 11285 | `		if( pNode->pLeft ){` |
|      65 | 11286 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11287 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      65 | 11288 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11289 | `				return rc;` |
|       - | 11290 | `			}` |
|      65 | 11291 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      31 | 11292 | `		}` |
|       - | 11293 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      65 | 11294 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 11295 | `		/* Patch the short-circuit jump to land after the store. */` |
|      65 | 11296 | `		if( nJmp > 0 ){` |
|      65 | 11297 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      65 | 11298 | `			if( pInstrFix ){` |
|      65 | 11299 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      31 | 11300 | `			}` |
|      31 | 11301 | `		}` |
|      65 | 11302 | `		return SXRET_OK;` |
|       - | 11303 | `	}` |
| 1370521 | 11304 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 11305 | `		sxu32 nJz,nJmp;` |
|       - | 11306 | `		sxu32 nTernaryNsBase;` |
|       - | 11307 | `		/* Ternary operator require special handling */` |
|       - | 11308 | `		/* Phase#1: Compile the condition */` |
|    2675 | 11309 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2675 | 11310 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2675 | 11311 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11312 | `			return rc;` |
|       - | 11313 | `		}` |
|       - | 11314 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 11315 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 11316 | `		 * condition expression, not leak past the ternary. */` |
|    2675 | 11317 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2675 | 11318 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2675 | 11319 | `		if( pNode->pLeft ){` |
|       - | 11320 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 11321 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2607 | 11322 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11323 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2607 | 11324 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2607 | 11325 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2607 | 11326 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11327 | `				return rc;` |
|       - | 11328 | `			}` |
|    2607 | 11329 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1306 | 11330 | `		}else{` |
|       - | 11331 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 11332 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 11333 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 11334 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 11335 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11336 | `		}` |
|       - | 11337 | `		/* Phase#4: Emit the unconditional jump */` |
|    2675 | 11338 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 11339 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2675 | 11340 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2675 | 11341 | `		if( pInstr ){` |
|    2675 | 11342 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1335 | 11343 | `		}` |
|    2675 | 11344 | `		if( !pNode->pLeft ){` |
|       - | 11345 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 11346 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 11347 | `		}` |
|       - | 11348 | `		/* Phase#6: Compile the 'else' expression */` |
|    2675 | 11349 | `		if( pNode->pRight ){` |
|    2675 | 11350 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2675 | 11351 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2675 | 11352 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11353 | `				return rc;` |
|       - | 11354 | `			}` |
|    2675 | 11355 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1335 | 11356 | `		}` |
|    2675 | 11357 | `		if( nJmp > 0 ){` |
|       - | 11358 | `			/* Phase#7: Fix the unconditional jump */` |
|    2675 | 11359 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2675 | 11360 | `			if( pInstr ){` |
|    2675 | 11361 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1335 | 11362 | `			}` |
|    1335 | 11363 | `		}` |
|       - | 11364 | `		/* All done */` |
|    2675 | 11365 | `		return SXRET_OK;` |
|       - | 11366 | `	}` |
| 1367851 | 11367 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 11368 | `	/* Generate code for the left tree */` |
| 1367851 | 11369 | `	if( pNode->pLeft ){` |
| 1367811 | 11370 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1367811 | 11371 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 11372 | `			ph7_expr_node **apNode;` |
|  461913 | 11373 | `			int hasSpread = 0;` |
|  461913 | 11374 | `			int hasNamed = 0;` |
|  461913 | 11375 | `			int bAnySpread = 0;` |
|  461913 | 11376 | `			sxu32 byRefMask = 0;` |
|       - | 11377 | `			sxi32 nArgs;` |
|       - | 11378 | `			sxi32 n;` |
|       - | 11379 | `			/* Recurse and generate bytecodes for function arguments */` |
|  461913 | 11380 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  461913 | 11381 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 11382 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|       - | 11383 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|       - | 11384 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  461913 | 11385 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      65 | 11386 | `				bFcc = 1;` |
|      65 | 11387 | `				nArgs = 0;` |
|      32 | 11388 | `			}` |
|       - | 11389 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 11390 | `			{` |
|  461913 | 11391 | `				int seenNamed = 0;` |
|  937191 | 11392 | `				for( n = 0; n < nArgs; ++n ){` |
|  475285 | 11393 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     216 | 11394 | `						seenNamed = 1;` |
|     216 | 11395 | `						hasNamed = 1;` |
|  475179 | 11396 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|    3697 | 11397 | `						bAnySpread = 1;` |
|  473227 | 11398 | `					}else if( seenNamed ){` |
|       3 | 11399 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 11400 | `							"Cannot use positional argument after named argument");` |
|       3 | 11401 | `						return SXERR_SYNTAX;` |
|       - | 11402 | `					}` |
|  237644 | 11403 | `				}` |
|       - | 11404 | `			}` |
|       - | 11405 | `			/* Read-only load */` |
|  461911 | 11406 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 11407 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 11408 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 11409 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 11410 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  461911 | 11411 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  461911 | 11412 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  461906 | 11413 | `				if( pCallName->nByte == 5` |
|  252168 | 11414 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   22341 | 11415 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  450743 | 11416 | `				}else if( pCallName->nByte == 5` |
|  229832 | 11417 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      91 | 11418 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      43 | 11419 | `				}` |
|       - | 11420 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 11421 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 11422 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 11423 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 11424 | `				 * the compile-time positional index no longer maps to the` |
|       - | 11425 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  461911 | 11426 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 11427 | `					SyString sBuiltin;` |
|  458103 | 11428 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  458103 | 11429 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  229049 | 11430 | `				}` |
|  230953 | 11431 | `			}` |
|  937187 | 11432 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  475281 | 11433 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  475281 | 11434 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11435 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 11436 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 11437 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 11438 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 11439 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 11440 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  475281 | 11441 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      55 | 11442 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      55 | 11443 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      25 | 11444 | `				}` |
|  475281 | 11445 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  475281 | 11446 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11447 | `					return rc;` |
|       - | 11448 | `				}` |
|       - | 11449 | `				/* Each argument is an independent nullsafe scope. */` |
|  475281 | 11450 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  475281 | 11451 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 11452 | `					/* Emit spread opcode to unpack this array argument */` |
|    3697 | 11453 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|    3697 | 11454 | `					hasSpread = 1;` |
|    1846 | 11455 | `				}` |
|  237643 | 11456 | `			}` |
|       - | 11457 | `			/* Total number of given arguments */` |
|  461911 | 11458 | `			iP1 = nArgs;` |
|  461911 | 11459 | `			iP2 = hasSpread;` |
|       - | 11460 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 11461 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  461911 | 11462 | `			if( hasNamed ){` |
|     119 | 11463 | `				sxu32 nStrBytes = 0;` |
|       - | 11464 | `				char *zBuf;` |
|     347 | 11465 | `				for( n = 0; n < nArgs; ++n ){` |
|     231 | 11466 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     213 | 11467 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|     105 | 11468 | `					}` |
|     117 | 11469 | `				}` |
|       - | 11470 | `				{` |
|     119 | 11471 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     119 | 11472 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     116 | 11473 | `					&pGen->pVm->sAllocator, mapSize);` |
|     119 | 11474 | `				if( pMap ){` |
|     119 | 11475 | `					SyZero(pMap, mapSize);` |
|     119 | 11476 | `					pMap->bHasNamed = 1;` |
|     119 | 11477 | `					pMap->nTotal = (sxu32)nArgs;` |
|     119 | 11478 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     119 | 11479 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     347 | 11480 | `					for( n = 0; n < nArgs; ++n ){` |
|     231 | 11481 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     213 | 11482 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     213 | 11483 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     213 | 11484 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     213 | 11485 | `							zBuf += nb;` |
|     105 | 11486 | `						}` |
|       - | 11487 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     117 | 11488 | `					}` |
|     119 | 11489 | `					p3 = (void *)pMap;` |
|      58 | 11490 | `				}` |
|       - | 11491 | `				}` |
|      58 | 11492 | `			}` |
|       - | 11493 | `			/* Remove stale flags now */` |
|  461911 | 11494 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  230953 | 11495 | `		}` |
|       - | 11496 | `		{` |
|       - | 11497 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|       - | 11498 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|       - | 11499 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|       - | 11500 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|       - | 11501 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|       - | 11502 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|       - | 11503 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|       - | 11504 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 1367809 | 11505 | `			sxi32 iLeftFlags = iFlags;` |
| 1544111 | 11506 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  868367 | 11507 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  360797 | 11508 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  352644 | 11509 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   16501 | 11510 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|    8248 | 11511 | `			}` |
|       - | 11512 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|       - | 11513 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|       - | 11514 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|       - | 11515 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|       - | 11516 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|       - | 11517 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|       - | 11518 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 1960412 | 11519 | `			if( pNode->pOp` |
| 1367809 | 11520 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 1276561 | 11521 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 1185267 | 11522 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  182927 | 11523 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   91461 | 11524 | `			}` |
| 1367809 | 11525 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|       - | 11526 | `		}` |
| 1367809 | 11527 | `		if( rc != SXRET_OK ){` |
|      34 | 11528 | `			return rc;` |
|       - | 11529 | `		}` |
| 1367779 | 11530 | `		if( !bIsChainOp ){` |
|       - | 11531 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 11532 | `			 * target the end of that LHS chain, which is right here. */` |
|  628639 | 11533 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  314317 | 11534 | `		}` |
| 1367779 | 11535 | `		if( iVmOp == PH7_OP_CALL ){` |
|  461911 | 11536 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  461911 | 11537 | `			if( pInstr ){` |
|  461911 | 11538 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  456473 | 11539 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 11540 | `					sxu32 nQual;` |
|  456473 | 11541 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11542 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 11543 | `					 * so the later NEW handler (if any) can see it. */` |
|  456473 | 11544 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 11545 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 11546 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 11547 | `					 * imports — class imports must NOT affect function` |
|       - | 11548 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11549 | `					 * before NEW; we store the original literal index in the` |
|       - | 11550 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11551 | `					 * the unqualified name and re-qualify with class imports. */` |
|  456473 | 11552 | `					if( bAbsolute ){` |
|    3695 | 11553 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1850 | 11554 | `					}else{` |
|  452783 | 11555 | `						int fromImport = 0;` |
|  452783 | 11556 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  452783 | 11557 | `						pInstr->iP2 = (sxi32)nQual;` |
|  452783 | 11558 | `						if( nQual != nOrig ){` |
|       - | 11559 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11560 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11561 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11562 | `							if( !fromImport ){` |
|       - | 11563 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11564 | `								if( p3 == 0 ){` |
|      67 | 11565 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11566 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11567 | `									if( pMap ){` |
|      67 | 11568 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11569 | `										p3 = (void *)pMap;` |
|      31 | 11570 | `									}` |
|      31 | 11571 | `								}` |
|      67 | 11572 | `								if( p3 ){` |
|      67 | 11573 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11574 | `								}` |
|      31 | 11575 | `							}` |
|      36 | 11576 | `						}` |
|       5 | 11577 | `					}` |
|  233677 | 11578 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11579 | `					/* Method call,flag that */` |
|    1337 | 11580 | `					pInstr->iP2 = 1;` |
|     666 | 11581 | `				}` |
|  230958 | 11582 | `			}` |
| 1136826 | 11583 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11584 | `			ph7_expr_node **apNode;` |
|       - | 11585 | `			sxi32 n;` |
|   94317 | 11586 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11587 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11588 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11589 | `			/* Recurse and generate bytecodes for array index */` |
|   94317 | 11590 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  170189 | 11591 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   75877 | 11592 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   75877 | 11593 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   75877 | 11594 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11595 | `					return rc;` |
|       - | 11596 | `				}` |
|       - | 11597 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   75877 | 11598 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   37941 | 11599 | `			}` |
|   94317 | 11600 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   75877 | 11601 | `				iP1 = 1; /* Node have an index associated with it */` |
|   37936 | 11602 | `			}` |
|   94317 | 11603 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11604 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     243 | 11605 | `				iP2 = 4;` |
|   94198 | 11606 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11607 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11608 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      56 | 11609 | `				iP2 = 5;` |
|   94053 | 11610 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11611 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11612 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11613 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11614 | `				iP2 = 6;` |
|   94015 | 11615 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11616 | `				/* Create an empty entry when the desired index is not found */` |
|   37187 | 11617 | `				iP2 = 1;` |
|   18596 | 11618 | `			}` |
|  858717 | 11619 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11620 | `			/* POP the left node */` |
|      32 | 11621 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 11622 | `		}` |
|  683887 | 11623 | `	}` |
| 1367819 | 11624 | `	rc = SXRET_OK;` |
| 1367819 | 11625 | `	nJmpIdx = 0;` |
|       - | 11626 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 11627 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 11628 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1367819 | 11629 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     377 | 11630 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     377 | 11631 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     377 | 11632 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     377 | 11633 | `			int isSpecial = 0;` |
|     377 | 11634 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     281 | 11635 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     281 | 11636 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     293 | 11637 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     259 | 11638 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     132 | 11639 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      98 | 11640 | `					isSpecial = 1;` |
|      47 | 11641 | `				}` |
|     162 | 11642 | `			}` |
|     425 | 11643 | `			pInstr->iP1 = 0;` |
|     425 | 11644 | `			if( !isSpecial ){` |
|     235 | 11645 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     115 | 11646 | `			}` |
|       - | 11647 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 11648 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     329 | 11649 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     235 | 11650 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     235 | 11651 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      46 | 11652 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      48 | 11653 | `					return SXRET_OK;` |
|       - | 11654 | `				}` |
|      93 | 11655 | `			}` |
|     140 | 11656 | `		}` |
|     221 | 11657 | `	}` |
|       - | 11658 | `	/* Generate code for the right tree */` |
| 1367737 | 11659 | `	if( pNode->pRight ){` |
|  738111 | 11660 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 11661 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   11515 | 11662 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  732356 | 11663 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 11664 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3851 | 11665 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  724678 | 11666 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 11667 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 11668 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 11669 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  722744 | 11670 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 11671 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 11672 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 11673 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 11674 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 11675 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 11676 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     106 | 11677 | `			sxu32 nNsJmp = 0;` |
|     106 | 11678 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     106 | 11679 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  722580 | 11680 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|       - | 11681 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|       - | 11682 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|       - | 11683 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  307077 | 11684 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  153536 | 11685 | `		}` |
|  738111 | 11686 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  738111 | 11687 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  738111 | 11688 | `		if( !bIsChainOp ){` |
|       - | 11689 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 11690 | `			 * operator instruction is emitted. */` |
|  555233 | 11691 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  277614 | 11692 | `		}` |
|  738111 | 11693 | `		if( iVmOp == PH7_OP_STORE ){` |
|  303147 | 11694 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  303116 | 11695 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 11696 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 11697 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 11698 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 11699 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 11700 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 11701 | `				 */` |
|      80 | 11702 | `				iVmOp = 0;` |
|  303109 | 11703 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  303071 | 11704 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11705 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   81203 | 11706 | `					iP2 = 1;` |
|   40604 | 11707 | `				}else{` |
|  221873 | 11708 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11709 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   37111 | 11710 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   37111 | 11711 | `						iP1 = pInstr->iP1;` |
|   18558 | 11712 | `					}else{` |
|  184767 | 11713 | `						p3 = pInstr->p3;` |
|       - | 11714 | `					}` |
|       - | 11715 | `					/* POP the last dynamic load instruction */` |
|  221873 | 11716 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 11717 | `				}` |
|  151538 | 11718 | `			}` |
|  586540 | 11719 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      54 | 11720 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      54 | 11721 | `			if( pInstr ){` |
|      54 | 11722 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11723 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 11724 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 11725 | `					 */` |
|      17 | 11726 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 11727 | `					iP1 = pInstr->iP1;` |
|      17 | 11728 | `					iP2 = pInstr->iP2;` |
|      17 | 11729 | `					p3  = pInstr->p3;` |
|       9 | 11730 | `				}else{` |
|      38 | 11731 | `					p3 = pInstr->p3;` |
|       - | 11732 | `				}` |
|      26 | 11733 | `			}` |
|      26 | 11734 | `		}` |
|  369053 | 11735 | `	}` |
| 1367732 | 11736 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   11942 | 11737 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 11738 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 11739 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      30 | 11740 | `		iVmOp = 0;` |
|      13 | 11741 | `	}` |
| 1367737 | 11742 | `	if( iVmOp > 0 ){` |
| 1367481 | 11743 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   15073 | 11744 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 11745 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   11029 | 11746 | `				iP1 = 1;` |
|    5517 | 11747 | `			}` |
| 1359947 | 11748 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 11749 | `			/* Namespace-qualify the class name for NEW */ {` |
|   23635 | 11750 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   23635 | 11751 | `				VmInstr *pCallInstr = 0;` |
|   23635 | 11752 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   23443 | 11753 | `					pCallInstr = pPeek;` |
|   23443 | 11754 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   11719 | 11755 | `				}` |
|   23635 | 11756 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   23633 | 11757 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11758 | `					sxu32 nLitForClass;` |
|       - | 11759 | `					/* If the CALL handler already qualified the name using` |
|       - | 11760 | `					 * function imports, recover the original unqualified` |
|       - | 11761 | `					 * literal so we can re-qualify with class imports. */` |
|   23633 | 11762 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 11763 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 11764 | `					}else{` |
|   23601 | 11765 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 11766 | `					}` |
|   23633 | 11767 | `					pPeek->iP1 = 0;` |
|   23633 | 11768 | `					if( !bAbsolute ){` |
|   19947 | 11769 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    9976 | 11770 | `					}else{` |
|    3691 | 11771 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 11772 | `					}` |
|   11814 | 11773 | `				}` |
|       - | 11774 | `			}` |
|   23635 | 11775 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   23635 | 11776 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 11777 | `				VmInstr *pPrev;` |
|   23443 | 11778 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   23443 | 11779 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 11780 | `					/* Pop the call instruction, preserve named-arg map */` |
|   23443 | 11781 | `					iP1 = pInstr->iP1;` |
|   23443 | 11782 | `					if( pInstr->p3 ){` |
|      43 | 11783 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 11784 | `					}` |
|   23443 | 11785 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   11719 | 11786 | `				}` |
|   11724 | 11787 | `			}` |
| 1340598 | 11788 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 11789 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 11790 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     201 | 11791 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     201 | 11792 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     201 | 11793 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     201 | 11794 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     201 | 11795 | `				int isSpecialIs = 0;` |
|     201 | 11796 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     197 | 11797 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     197 | 11798 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     197 | 11799 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     192 | 11800 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      97 | 11801 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 11802 | `						isSpecialIs = 1;` |
|       5 | 11803 | `					}` |
|      97 | 11804 | `				}` |
|     203 | 11805 | `				pInstr->iP1 = 0;` |
|     203 | 11806 | `				if( !isSpecialIs && !bAbsolute ){` |
|     181 | 11807 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      88 | 11808 | `				}` |
|     102 | 11809 | `			}` |
| 1328688 | 11810 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 11811 | `			/* Prevent constant expansion for member/property names.` |
|       - | 11812 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 11813 | `			 * should not trigger constant lookup. */` |
|  182883 | 11814 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  182883 | 11815 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  182835 | 11816 | `				pInstr->iP1 = 0;` |
|   91415 | 11817 | `			}` |
|  182883 | 11818 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 11819 | `				/* Static member access,remember that */` |
|     295 | 11820 | `				iP1 = 1;` |
|     295 | 11821 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     295 | 11822 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      42 | 11823 | `					p3 = pInstr->p3;` |
|      42 | 11824 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      19 | 11825 | `				}` |
|     145 | 11826 | `			}` |
|       - | 11827 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|       - | 11828 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|       - | 11829 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|       - | 11830 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  182883 | 11831 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  182883 | 11832 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|      30 | 11833 | `					iP2 = PH7_MEMBER_UNSET;` |
|  182869 | 11834 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      77 | 11835 | `					iP2 = PH7_MEMBER_ISSET;` |
|  182819 | 11836 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|      13 | 11837 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  182777 | 11838 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|       - | 11839 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   81283 | 11840 | `					iP2 = PH7_MEMBER_WRITE;` |
|   40639 | 11841 | `				}` |
|   91439 | 11842 | `			}` |
|   91439 | 11843 | `		}` |
|       - | 11844 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|       - | 11845 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|       - | 11846 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|       - | 11847 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|       - | 11848 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 1367479 | 11849 | `		if( bFcc ){` |
|      65 | 11850 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|      65 | 11851 | `			iP2 = 0;` |
|      65 | 11852 | `			p3 = 0;` |
|      65 | 11853 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11854 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11855 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|       - | 11856 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|       - | 11857 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|       - | 11858 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      31 | 11859 | `				void *pMemberName = pInstr->p3;` |
|      31 | 11860 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      31 | 11861 | `				if( pMemberName ){` |
|       3 | 11862 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       1 | 11863 | `				}` |
|      31 | 11864 | `				iP1 = 2;` |
|      16 | 11865 | `			}else{` |
|      35 | 11866 | `				iP1 = 1;` |
|       - | 11867 | `			}` |
|      32 | 11868 | `		}` |
|       - | 11869 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 11870 | `		 * This is the primary emit path for user-visible calls. */` |
| 1367479 | 11871 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  485477 | 11872 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  242736 | 11873 | `		}` |
|       - | 11874 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1367479 | 11875 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  683737 | 11876 | `	}` |
| 1367735 | 11877 | `	if( nJmpIdx > 0 ){` |
|       - | 11878 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   15485 | 11879 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   15485 | 11880 | `		if( pInstr ){` |
|   15485 | 11881 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    7740 | 11882 | `		}` |
|    7740 | 11883 | `	}` |
| 1367735 | 11884 | `	return rc;` |
| 1823696 | 11885 | `}` |
|       - | 11886 | `/*` |
|       - | 11887 | ` * Compile a PHP expression.` |
|       - | 11888 | ` * According to the PHP language reference manual:` |
|       - | 11889 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 11890 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 11891 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 11892 | ` *  is "anything that has a value".` |
|       - | 11893 | ` * If something goes wrong while compiling the expression,this` |
|       - | 11894 | ` * function takes care of generating the appropriate error` |
|       - | 11895 | ` * message.` |
|       - | 11896 | ` */` |
|  982450 | 11897 | `static sxi32 PH7_CompileExpr(` |
|       - | 11898 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11899 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 11900 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 11901 | `	)` |
|       5 | 11902 | `{` |
|       - | 11903 | `	ph7_expr_node *pRoot;` |
|       - | 11904 | `	SySet sExprNode;` |
|       - | 11905 | `	SyToken *pEnd;` |
|       - | 11906 | `	sxi32 nExpr;` |
|       - | 11907 | `	sxi32 iNest;` |
|       - | 11908 | `	sxi32 rc;` |
|       - | 11909 | `	sxu32 nNullsafeBase;` |
|       - | 11910 | `	/* Initialize worker variables */` |
|  982455 | 11911 | `	nExpr = 0;` |
|  982455 | 11912 | `	pRoot = 0;` |
|       - | 11913 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 11914 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  982455 | 11915 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  982455 | 11916 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  982455 | 11917 | `	SySetAlloc(&sExprNode,0x10);` |
|  982455 | 11918 | `	rc = SXRET_OK;` |
|       - | 11919 | `	/* Delimit the expression */` |
|  982455 | 11920 | `	pEnd = pGen->pIn;` |
|  982455 | 11921 | `	iNest = 0;` |
| 6627989 | 11922 | `	while( pEnd < pGen->pEnd ){` |
| 6289605 | 11923 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11924 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     517 | 11925 | `			iNest++;` |
| 6289349 | 11926 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     525 | 11927 | `			iNest--;` |
| 6288833 | 11928 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  644443 | 11929 | `			if( iNest <= 0 ){` |
|  644071 | 11930 | `				break;` |
|       - | 11931 | `			}` |
|     186 | 11932 | `		}` |
| 5645539 | 11933 | `		pEnd++;` |
|       5 | 11934 | `	}` |
|  982455 | 11935 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   22593 | 11936 | `		SyToken *pEnd2 = pGen->pIn;` |
|   22593 | 11937 | `		iNest = 0;` |
|       - | 11938 | `		/* Stop at the first comma */` |
|   45499 | 11939 | `		while( pEnd2 < pEnd ){` |
|   22917 | 11940 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      71 | 11941 | `				iNest++;` |
|   22884 | 11942 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      71 | 11943 | `				iNest--;` |
|   22818 | 11944 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 11945 | `				if( iNest <= 0 ){` |
|       7 | 11946 | `					break;` |
|       - | 11947 | `				}` |
|      23 | 11948 | `			}` |
|   22911 | 11949 | `			pEnd2++;` |
|       5 | 11950 | `		}` |
|   22593 | 11951 | `		if( pEnd2 <pEnd ){` |
|       7 | 11952 | `			pEnd = pEnd2;` |
|       3 | 11953 | `		}` |
|   11294 | 11954 | `	}` |
|  982455 | 11955 | `	if( pEnd > pGen->pIn ){` |
|  982445 | 11956 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 11957 | `		/* Swap delimiter */` |
|  982445 | 11958 | `		pGen->pEnd = pEnd;` |
|       - | 11959 | `		/* Try to get an expression tree */` |
|  982445 | 11960 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  982445 | 11961 | `		if( rc == SXRET_OK && pRoot ){` |
|  982263 | 11962 | `			rc = SXRET_OK;` |
|  982263 | 11963 | `			if( xTreeValidator ){` |
|       - | 11964 | `				/* Call the upper layer validator callback */` |
|   30041 | 11965 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   15018 | 11966 | `			}` |
|  982263 | 11967 | `			if( rc != SXERR_ABORT ){` |
|       - | 11968 | `				/* Generate code for the given tree */` |
|  982263 | 11969 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 11970 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 11971 | `				 * expression so they short-circuit to its end. */` |
|  982263 | 11972 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  491129 | 11973 | `			}` |
|  982263 | 11974 | `			nExpr = 1;` |
|  491129 | 11975 | `		}` |
|       - | 11976 | `		/* Release the whole tree */` |
|  982445 | 11977 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 11978 | `		/* Synchronize token stream */` |
|  982445 | 11979 | `		pGen->pEnd = pTmp;` |
|  982445 | 11980 | `		pGen->pIn  = pEnd;` |
|  982445 | 11981 | `		if( rc == SXERR_ABORT ){` |
|      13 | 11982 | `			SySetRelease(&sExprNode);` |
|      13 | 11983 | `			return SXERR_ABORT;` |
|       - | 11984 | `		}` |
|  491215 | 11985 | `	}` |
|  982445 | 11986 | `	SySetRelease(&sExprNode);` |
|  982445 | 11987 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  491230 | 11988 | `}` |
|       - | 11989 | `/*` |
|       - | 11990 | ` * Return a pointer to the node construct handler associated` |
|       - | 11991 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 11992 | ` */` |
|  257070 | 11993 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 11994 | `{` |
|  257075 | 11995 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 11996 | `		/* Numeric literal: Either real or integer */` |
|  129487 | 11997 | `		return PH7_CompileNumLiteral;` |
|  127593 | 11998 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 11999 | `		/* Double quoted string */` |
|   24219 | 12000 | `		return PH7_CompileString;` |
|  103379 | 12001 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 12002 | `		/* Single quoted string */` |
|  103263 | 12003 | `		return PH7_CompileSimpleString;` |
|     121 | 12004 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 12005 | `		/* Heredoc */` |
|      68 | 12006 | `		return PH7_CompileHereDoc;` |
|      56 | 12007 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 12008 | `		/* Nowdoc */` |
|      50 | 12009 | `		return PH7_CompileNowDoc;` |
|       8 | 12010 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 12011 | `		/* Backtick quoted string */` |
|       6 | 12012 | `		return PH7_CompileBacktic;` |
|       - | 12013 | `	}` |
|       3 | 12014 | `	return 0;` |
|  128540 | 12015 | `}` |
|       - | 12016 | `/*` |
|       - | 12017 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 12018 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 12019 | ` * in write context" parse error.` |
|       - | 12020 | ` */` |
|    6866 | 12021 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 12022 | `{` |
|       - | 12023 | `	sxi32 rc;` |
|    6871 | 12024 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6869 | 12025 | `		return SXRET_OK;` |
|       - | 12026 | `	}` |
|       5 | 12027 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 12028 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 12029 | `		"Can't use nullsafe operator in write context");` |
|       3 | 12030 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3438 | 12031 | `}` |
|       - | 12032 | `/*` |
|       - | 12033 | ` * Compile an unset() statement.` |
|       - | 12034 | ` * unset($var, $arr[$key], ...);` |
|       - | 12035 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 12036 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 12037 | ` * parent array before extracting the element to unset.` |
|       - | 12038 | ` */` |
|    2978 | 12039 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 12040 | `{` |
|    2983 | 12041 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2983 | 12042 | `	sxu32 nIdx = 0;` |
|       - | 12043 | `	SyString sName;` |
|       - | 12044 | `	sxi32 rc;` |
|       - | 12045 | `	/* Jump the 'unset' keyword */` |
|    2983 | 12046 | `	pGen->pIn++;` |
|       - | 12047 | `	/* Save delimiter */` |
|    2983 | 12048 | `	pTmp = pGen->pEnd;` |
|       - | 12049 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2983 | 12050 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2983 | 12051 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 12052 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 12053 | `		SyToken *pClose;` |
|    2983 | 12054 | `		pGen->pIn++;   /* Skip '(' */` |
|    2983 | 12055 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2983 | 12056 | `		pEnd = pClose; /* Stop at ')' */` |
|    1489 | 12057 | `	}` |
|    2983 | 12058 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 12059 | `	/* Resolve the 'unset' builtin name once */` |
|    2983 | 12060 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     365 | 12061 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     365 | 12062 | `		if( pObj == 0 ){` |
|     ! 0 | 12063 | `			return SXERR_ABORT;` |
|       - | 12064 | `		}` |
|     365 | 12065 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     365 | 12066 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     180 | 12067 | `	}` |
|       - | 12068 | `	/* Compile each comma-separated argument */` |
|    9851 | 12069 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6873 | 12070 | `		if( pGen->pIn < pNext ){` |
|    6873 | 12071 | `			pGen->pEnd = pNext;` |
|    6873 | 12072 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 12073 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 12074 | `				GenStateUnsetValidator);` |
|    6873 | 12075 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12076 | `				return SXERR_ABORT;` |
|       - | 12077 | `			}` |
|    6873 | 12078 | `			if( rc != SXERR_EMPTY ){` |
|       - | 12079 | `				/* Emit call for this single argument */` |
|    6871 | 12080 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6871 | 12081 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6871 | 12082 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3433 | 12083 | `			}` |
|    3434 | 12084 | `		}` |
|       - | 12085 | `		/* Jump trailing commas */` |
|   10765 | 12086 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3897 | 12087 | `			pNext++;` |
|       5 | 12088 | `		}` |
|    6873 | 12089 | `		pGen->pIn = pNext;` |
|       5 | 12090 | `	}` |
|       - | 12091 | `	/* Skip past the closing ')' if present */` |
|    2983 | 12092 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2983 | 12093 | `		pGen->pIn++;` |
|    1489 | 12094 | `	}` |
|       - | 12095 | `	/* Restore token stream */` |
|    2983 | 12096 | `	pGen->pEnd = pTmp;` |
|    2983 | 12097 | `	return SXRET_OK;` |
|    1494 | 12098 | `}` |
|       - | 12099 | `/*` |
|       - | 12100 | ` * PHP Language construct table.` |
|       - | 12101 | ` */` |
|       - | 12102 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 12103 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 12104 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 12105 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 12106 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 12107 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 12108 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 12109 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 12110 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 12111 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 12112 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 12113 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 12114 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 12115 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 12116 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 12117 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 12118 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 12119 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 12120 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 12121 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 12122 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 12123 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 12124 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 12125 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 12126 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 12127 | `};` |
|       - | 12128 | `/*` |
|       - | 12129 | ` * Return a pointer to the statement handler routine associated` |
|       - | 12130 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 12131 | ` */` |
|  658852 | 12132 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 12133 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 12134 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 12135 | `	)` |
|       5 | 12136 | `{` |
|  658857 | 12137 | `	sxu32 n = 0;` |
| 3416681 | 12138 | `	for(;;){` |
| 6833367 | 12139 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  141085 | 12140 | `			break;` |
|       - | 12141 | `		}` |
| 6692287 | 12142 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  517777 | 12143 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 12144 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 12145 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 12146 | `					/* 'static' (class context),return null */` |
|     ! 0 | 12147 | `					return 0;` |
|       - | 12148 | `				}` |
|     ! 0 | 12149 | `			}` |
|  517772 | 12150 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       8 | 12151 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       9 | 12152 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 12153 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 12154 | `				return 0;` |
|       - | 12155 | `			}` |
|       - | 12156 | `			/* Return a pointer to the handler.` |
|       - | 12157 | `			*/` |
|  517777 | 12158 | `			return aLangConstruct[n].xConstruct;` |
|       - | 12159 | `		}` |
| 6174515 | 12160 | `		n++;` |
|       5 | 12161 | `	}` |
|  141085 | 12162 | `	if( pLookahed ){` |
|  141085 | 12163 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   40437 | 12164 | `			return PH7_CompileClassInterface;` |
|  100653 | 12165 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|  100255 | 12166 | `			return PH7_CompileClass;` |
|     403 | 12167 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      69 | 12168 | `			return PH7_CompileTrait;` |
|       - | 12169 | `		}` |
|       - | 12170 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 12171 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 12172 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 12173 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     167 | 12174 | `	}` |
|       - | 12175 | `	/* Not a language construct */` |
|     339 | 12176 | `	return 0;` |
|  329431 | 12177 | `}` |
|       - | 12178 | `/*` |
|       - | 12179 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 12180 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 12181 | ` */` |
|     334 | 12182 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 12183 | `{` |
|       - | 12184 | `	int rc;` |
|     339 | 12185 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     339 | 12186 | `	if( rc == FALSE ){` |
|     224 | 12187 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     223 | 12188 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 12189 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 12190 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 12191 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 12192 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 12193 | `			*/` |
|       - | 12194 | `			){` |
|     221 | 12195 | `				rc = TRUE;` |
|     108 | 12196 | `		}` |
|     112 | 12197 | `	}` |
|     339 | 12198 | `	return rc;` |
|       5 | 12199 | `}` |
|       - | 12200 | `/*` |
|       - | 12201 | ` * Compile a PHP chunk.` |
|       - | 12202 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12203 | ` * takes care of generating the appropriate error message.` |
|       - | 12204 | ` */` |
|  788136 | 12205 | `static sxi32 GenStateCompileChunk(` |
|       - | 12206 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 12207 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 12208 | `	)` |
|       5 | 12209 | `{` |
|       - | 12210 | `	ProcLangConstruct xCons;` |
|       - | 12211 | `	sxi32 rc;` |
|  788141 | 12212 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  616040 | 12213 | `	for(;;){` |
| 1010113 | 12214 | `		int bStmtIsDeclare = 0;` |
| 1010113 | 12215 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 12216 | `			/* No more input to process */` |
|   14475 | 12217 | `			break;` |
|       - | 12218 | `		}` |
|       - | 12219 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 12220 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  995643 | 12221 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  662559 | 12222 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  662559 | 12223 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 12224 | `				bStmtIsDeclare = 1;` |
|      20 | 12225 | `			}` |
|  331277 | 12226 | `		}` |
|  995643 | 12227 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 12228 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 12229 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  221947 | 12230 | `			pGen->bStrictTypesLocked = 1;` |
|  110971 | 12231 | `		}` |
|  995643 | 12232 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 12233 | `			/* Compile block */` |
|      23 | 12234 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      23 | 12235 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12236 | `				break;` |
|       - | 12237 | `			}` |
|      14 | 12238 | `		}else{` |
|  995625 | 12239 | `			xCons = 0;` |
|  995625 | 12240 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 12241 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 12242 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 12243 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3733 | 12244 | `				xCons = PH7_CompileClassModifiers;` |
|  993761 | 12245 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  658857 | 12246 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 12247 | `				/* Try to extract a language construct handler */` |
|  658857 | 12248 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  658857 | 12249 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 12250 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 12251 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 12252 | `						&pGen->pIn->sData);` |
|       9 | 12253 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 12254 | `						break;` |
|       - | 12255 | `					}` |
|       - | 12256 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 12257 | `					 * this erroneous statement.` |
|       - | 12258 | `					 */` |
|       9 | 12259 | `					xCons = PH7_ErrorRecover;` |
|       4 | 12260 | `				}` |
|  662471 | 12261 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   54589 | 12262 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 12263 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 12264 | `				xCons = PH7_CompileLabel;` |
|      56 | 12265 | `			}` |
|  995625 | 12266 | `			if( xCons == 0 ){` |
|       - | 12267 | `				/* Assume an expression an try to compile it */` |
|  333259 | 12268 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  333259 | 12269 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 12270 | `					/* Pop l-value */` |
|  333109 | 12271 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  166552 | 12272 | `				}` |
|  166632 | 12273 | `			}else{` |
|       - | 12274 | `				/* Go compile the sucker */` |
|  662371 | 12275 | `				rc = xCons(&(*pGen));` |
|       - | 12276 | `			}` |
|  995625 | 12277 | `			if( rc == SXERR_ABORT ){` |
|       - | 12278 | `				/* Request to abort compilation */` |
|      13 | 12279 | `				break;` |
|       - | 12280 | `			}` |
|       - | 12281 | `		}` |
|       - | 12282 | `		/* Ignore trailing semi-colons ';' */` |
| 1609707 | 12283 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  614079 | 12284 | `			pGen->pIn++;` |
|       5 | 12285 | `		}` |
|  995633 | 12286 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 12287 | `			/* Compile a single statement and return */` |
|  773661 | 12288 | `			break;` |
|       - | 12289 | `		}` |
|       - | 12290 | `		/* LOOP ONE */` |
|       - | 12291 | `		/* LOOP TWO */` |
|       - | 12292 | `		/* LOOP THREE */` |
|       - | 12293 | `		/* LOOP FOUR */` |
|       5 | 12294 | `	}` |
|       - | 12295 | `	/* Return compilation status */` |
|  788141 | 12296 | `	return rc;` |
|       5 | 12297 | `}` |
|       - | 12298 | `/*` |
|       - | 12299 | ` * Compile a Raw PHP chunk.` |
|       - | 12300 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12301 | ` * takes care of generating the appropriate error message.` |
|       - | 12302 | ` */` |
|   14482 | 12303 | `static sxi32 PH7_CompilePHP(` |
|       - | 12304 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 12305 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 12306 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 12307 | `	)` |
|       5 | 12308 | `{` |
|   14487 | 12309 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 12310 | `	sxi32 rc;` |
|       - | 12311 | `	/* Reset the token set */` |
|   14487 | 12312 | `	SySetReset(&(*pTokenSet));` |
|       - | 12313 | `	/* Mark as the default token set */` |
|   14487 | 12314 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 12315 | `	/* Advance the stream cursor */` |
|   14487 | 12316 | `	pGen->pRawIn++;` |
|       - | 12317 | `	/* Tokenize the PHP chunk first */` |
|   14487 | 12318 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 12319 | `	/* Point to the head and tail of the token stream. */` |
|   14487 | 12320 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   14487 | 12321 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   14487 | 12322 | `	if( is_expr ){` |
|     ! 0 | 12323 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 12324 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 12325 | `			/* A simple expression,compile it */` |
|     ! 0 | 12326 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 12327 | `		}` |
|       - | 12328 | `		/* Emit the DONE instruction */` |
|     ! 0 | 12329 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 12330 | `		return SXRET_OK;` |
|       - | 12331 | `	}` |
|   14487 | 12332 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 12333 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 12334 | `		/*` |
|       - | 12335 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 12336 | `		 * According to the PHP reference manual:` |
|       - | 12337 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 12338 | `		 *  immediately follow` |
|       - | 12339 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 12340 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 12341 | `		 * Symisc extension:` |
|       - | 12342 | `		 *   This short syntax works with all PHP opening` |
|       - | 12343 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 12344 | `		 *   only short tag.` |
|       - | 12345 | `		 */` |
|       - | 12346 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 12347 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 12348 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 12349 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 12350 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 12351 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 12352 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 12353 | `		}` |
|       3 | 12354 | `		return SXRET_OK;` |
|       - | 12355 | `	}` |
|       - | 12356 | `	/* Compile the PHP chunk */` |
|   14485 | 12357 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 12358 | `	/* Fix exceptions jumps */` |
|   14485 | 12359 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 12360 | `	/* Fix gotos now, the jump destination is resolved */` |
|   14485 | 12361 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 12362 | `		rc = SXERR_ABORT;` |
|       1 | 12363 | `	}` |
|       - | 12364 | `	/* Reset container */` |
|   14485 | 12365 | `	SySetReset(&pGen->aGoto);` |
|   14485 | 12366 | `	SySetReset(&pGen->aLabel);` |
|   14485 | 12367 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 12368 | `	/* Compilation result */` |
|   14485 | 12369 | `	return rc;` |
|    7246 | 12370 | `}` |
|       - | 12371 | `/*` |
|       - | 12372 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 12373 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 12374 | ` * This is the only compile interface exported from this file.` |
|       - | 12375 | ` */` |
|   17498 | 12376 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 12377 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 12378 | `	SyString *pScript,  /* Script to compile */` |
|       - | 12379 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 12380 | `	)` |
|       5 | 12381 | `{` |
|       - | 12382 | `	SySet aPhpToken,aRawToken;` |
|       - | 12383 | `	ph7_gen_state *pCodeGen;` |
|       - | 12384 | `	ph7_value *pRawObj;` |
|       - | 12385 | `	sxu32 nObjIdx;` |
|       - | 12386 | `	sxi32 nRawObj;` |
|       - | 12387 | `	int is_expr;` |
|       - | 12388 | `	sxi8 bSavedStrict;` |
|       - | 12389 | `	sxi8 bSavedStrictLocked;` |
|       - | 12390 | `	sxi32 rc;` |
|   17503 | 12391 | `	if( pScript->nByte < 1 ){` |
|       - | 12392 | `		/* Nothing to compile */` |
|     ! 0 | 12393 | `		return PH7_OK;` |
|       - | 12394 | `	}` |
|       - | 12395 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 12396 | `	 * file's flags so include/require restore them on return. */` |
|   17503 | 12397 | `	pCodeGen = &pVm->sCodeGen;` |
|   17503 | 12398 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   17503 | 12399 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   17503 | 12400 | `	pCodeGen->bStrictTypes = 0;` |
|   17503 | 12401 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 12402 | `	/* Initialize the tokens containers */` |
|   17503 | 12403 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17503 | 12404 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17503 | 12405 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   17503 | 12406 | `	is_expr = 0;` |
|   17503 | 12407 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 12408 | `		SyToken sTmp;` |
|       - | 12409 | `		/* PHP only: -*/` |
|    3741 | 12410 | `		sTmp.nLine = 1;` |
|    3741 | 12411 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3741 | 12412 | `		sTmp.pUserData = 0;` |
|    3741 | 12413 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3741 | 12414 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3741 | 12415 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 12416 | `			/* A simple PHP expression */` |
|     ! 0 | 12417 | `			is_expr = 1;` |
|     ! 0 | 12418 | `		}` |
|    1873 | 12419 | `	}else{` |
|       - | 12420 | `		/* Tokenize raw text */` |
|   13767 | 12421 | `		SySetAlloc(&aRawToken,32);` |
|   13767 | 12422 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 12423 | `	}` |
|       - | 12424 | `	/* Process high-level tokens */` |
|   17503 | 12425 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   17503 | 12426 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   17503 | 12427 | `	rc = PH7_OK;` |
|   17503 | 12428 | `	if( is_expr ){` |
|       - | 12429 | `		/* Compile the expression */` |
|     ! 0 | 12430 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 12431 | `		goto cleanup;` |
|       - | 12432 | `	}` |
|   17503 | 12433 | `	nObjIdx = 0;` |
|       - | 12434 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 12435 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 12436 | `	 * preventing namespace bleeding across include()d files. */` |
|   17503 | 12437 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 12438 | `	/* Start the compilation process */` |
|   15636 | 12439 | `	for(;;){` |
|   45747 | 12440 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   17491 | 12441 | `			break; /* No more tokens to process */` |
|       - | 12442 | `		}` |
|   28261 | 12443 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 12444 | `			/* Compile the PHP chunk */` |
|   14487 | 12445 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   14487 | 12446 | `			if( rc == SXERR_ABORT ){` |
|      15 | 12447 | `				break;` |
|       - | 12448 | `			}` |
|   14475 | 12449 | `			continue;` |
|       - | 12450 | `		}` |
|       - | 12451 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13779 | 12452 | `		nRawObj = 0;` |
|   27595 | 12453 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 12454 | `			/* Consume the raw chunk without any processing */` |
|   13821 | 12455 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13821 | 12456 | `			if( pRawObj == 0 ){` |
|     ! 0 | 12457 | `				rc = SXERR_MEM;` |
|     ! 0 | 12458 | `				break;` |
|       - | 12459 | `			}` |
|       - | 12460 | `			/* Mark as constant and emit the load constant instruction */` |
|   13821 | 12461 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13821 | 12462 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13821 | 12463 | `			++nRawObj;` |
|   13821 | 12464 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 12465 | `		}` |
|   13779 | 12466 | `		if( nRawObj > 0 ){` |
|       - | 12467 | `			/* Emit the consume instruction */` |
|   13779 | 12468 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6887 | 12469 | `		}` |
|    8754 | 12470 | `	}` |
|    8749 | 12471 | `cleanup:` |
|   17503 | 12472 | `	SySetRelease(&aRawToken);` |
|   17503 | 12473 | `	SySetRelease(&aPhpToken);` |
|       - | 12474 | `	/* Restore outer file's strict_types scope */` |
|   17503 | 12475 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   17503 | 12476 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   17503 | 12477 | `	return rc;` |
|    8754 | 12478 | `}` |
|       - | 12479 | `/*` |
|       - | 12480 | ` * Utility routines.Initialize the code generator.` |
|       - | 12481 | ` */` |
|    3668 | 12482 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 12483 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12484 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12485 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12486 | `	)` |
|       5 | 12487 | `{` |
|    3673 | 12488 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12489 | `	/* Zero the structure */` |
|    3673 | 12490 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 12491 | `	/* Initial state */` |
|    3673 | 12492 | `	pGen->pVm  = &(*pVm);` |
|    3673 | 12493 | `	pGen->xErr = xErr;` |
|    3673 | 12494 | `	pGen->pErrData = pErrData;` |
|    3673 | 12495 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3673 | 12496 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3673 | 12497 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3673 | 12498 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3673 | 12499 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 12500 | `	/* Error log buffer */` |
|    3673 | 12501 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 12502 | `	/* General purpose working buffer */` |
|    3673 | 12503 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 12504 | `	/* Namespace state */` |
|    3673 | 12505 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3673 | 12506 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3673 | 12507 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3673 | 12508 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12509 | `	/* Create the global scope */` |
|    3673 | 12510 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 12511 | `	/* Point to the global scope */` |
|    3673 | 12512 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3673 | 12513 | `	return SXRET_OK;` |
|       5 | 12514 | `}` |
|       - | 12515 | `/*` |
|       - | 12516 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 12517 | ` */` |
|   20802 | 12518 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 12519 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12520 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12521 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12522 | `	)` |
|       5 | 12523 | `{` |
|   20807 | 12524 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12525 | `	GenBlock *pBlock,*pParent;` |
|       - | 12526 | `	/* Reset state */` |
|   20807 | 12527 | `	SySetReset(&pGen->aLabel);` |
|   20807 | 12528 | `	SySetReset(&pGen->aGoto);` |
|   20807 | 12529 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   20807 | 12530 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   20807 | 12531 | `	SyBlobRelease(&pGen->sWorker);` |
|   20807 | 12532 | `	SyBlobRelease(&pGen->sNamespace);` |
|   20807 | 12533 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   20807 | 12534 | `	SyHashRelease(&pGen->hUseImports);` |
|   20807 | 12535 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   20807 | 12536 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   20807 | 12537 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   20807 | 12538 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   20807 | 12539 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12540 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 12541 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 12542 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 12543 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 12544 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 12545 | `	 * number of unique names, which is acceptable. */` |
|       - | 12546 | `	/* Point to the global scope */` |
|   20807 | 12547 | `	pBlock = pGen->pCurrent;` |
|   20807 | 12548 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 12549 | `		pParent = pBlock->pParent;` |
|     ! 0 | 12550 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 12551 | `		pBlock = pParent;` |
|     ! 0 | 12552 | `	}` |
|   20807 | 12553 | `	pGen->xErr = xErr;` |
|   20807 | 12554 | `	pGen->pErrData = pErrData;` |
|   20807 | 12555 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   20807 | 12556 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   20807 | 12557 | `	pGen->pIn = pGen->pEnd = 0;` |
|   20807 | 12558 | `	pGen->nErr = 0;` |
|   20807 | 12559 | `	return SXRET_OK;` |
|       5 | 12560 | `}` |
|       - | 12561 | `/*` |
|       - | 12562 | ` * Generate a compile-time error message.` |
|       - | 12563 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 12564 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 12565 | ` * abort compilation immediately.` |
|       - | 12566 | ` */` |
|     632 | 12567 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 12568 | `{` |
|     637 | 12569 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     637 | 12570 | `	const char *zErr = "Error";` |
|       - | 12571 | `	SyString *pFile;` |
|       - | 12572 | `	va_list ap;` |
|       - | 12573 | `	sxi32 rc;` |
|       - | 12574 | `	/* Reset the working buffer */` |
|     637 | 12575 | `	SyBlobReset(pWorker);` |
|       - | 12576 | `	/* Peek the processed file path if available */` |
|     637 | 12577 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     637 | 12578 | `	if( nErrType == E_ERROR ){` |
|       - | 12579 | `		/* Increment the error counter */` |
|     525 | 12580 | `		pGen->nErr++;` |
|     525 | 12581 | `		if( pGen->nErr > 15 ){` |
|       - | 12582 | `			/* Error count limit reached */` |
|       5 | 12583 | `			if( pGen->xErr ){` |
|       5 | 12584 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 12585 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 12586 | `				if( pFile ){` |
|       5 | 12587 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 12588 | `				}` |
|       5 | 12589 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 12590 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 12591 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12592 | `				}` |
|       2 | 12593 | `			}` |
|       - | 12594 | `			/* Abort immediately */` |
|       5 | 12595 | `			return SXERR_ABORT;` |
|       - | 12596 | `		}` |
|     258 | 12597 | `	}` |
|     633 | 12598 | `	if( pGen->xErr == 0 ){` |
|       - | 12599 | `		/* No available error consumer,return immediately */` |
|       3 | 12600 | `		return SXRET_OK;` |
|       - | 12601 | `	}` |
|     630 | 12602 | `	switch(nErrType){` |
|     518 | 12603 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 12604 | `	case E_WARNING: zErr = "Warning";     break;` |
|      82 | 12605 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      11 | 12606 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12607 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12608 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12609 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12610 | `	default:` |
|     ! 0 | 12611 | `		break;` |
|       - | 12612 | `	}` |
|     630 | 12613 | `	rc = SXRET_OK;` |
|       - | 12614 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     630 | 12615 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     630 | 12616 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     630 | 12617 | `	va_start(ap,zFormat);` |
|     630 | 12618 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     630 | 12619 | `	va_end(ap);` |
|     630 | 12620 | `	if( pFile ){` |
|     630 | 12621 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     313 | 12622 | `	}` |
|       - | 12623 | `	/* Append a new line */` |
|     630 | 12624 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     630 | 12625 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 12626 | `		/* Consume the generated error message */` |
|     630 | 12627 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     313 | 12628 | `	}` |
|     630 | 12629 | `	return rc;` |
|     321 | 12630 | `}` |
|       - | 12631 |  |
